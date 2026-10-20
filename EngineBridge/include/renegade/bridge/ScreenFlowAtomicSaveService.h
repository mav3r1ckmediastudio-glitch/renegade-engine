#pragma once

#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/bridge/StoryFlowAuthoringSession.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace renegade::bridge
{
    // UI-05: write the exact current Screen and StoryFlow documents in one
    // journaled project transaction, without discarding their Undo histories.
    // This is a persistence operation, not a new flow/Screen routing authority.
    class ScreenFlowAtomicSaveService final
    {
    public:
        struct Result
        {
            bool succeeded = false;
            bool noChanges = false;
            std::string message;
            ProjectDocumentTransactionResult transaction;
        };

        [[nodiscard]] static Result Save(
            ScreenAuthoringSession& screen,
            StoryFlowAuthoringSession& flow,
            const std::string& projectRoot,
            ProjectDocumentTransactionOptions options = {})
        {
            Result result;
            namespace fs = std::filesystem;
            if (!screen.loaded_ || !flow.loaded_ ||
                screen.filePath_.empty() || flow.filePath_.empty() ||
                screen.projectId_ != flow.projectId_)
            {
                result.message = "A loaded Screen and StoryFlow from the same project are required.";
                return result;
            }
            std::string error;
            if (!ValidateScreenDocument(
                    screen.Document(), screen.projectId_, error) ||
                !ValidateFlowDocument(
                    flow.Document(), flow.projectId_, error))
            {
                result.message = "Invalid authored document: " + error;
                return result;
            }
            // Undo -> alternate edit may prune a saved revision from the
            // *undo stack*. Keep the independent saved snapshot as the
            // external-edit baseline, so normal branching remains saveable.
            if (projectRoot.empty())
            {
                result.message = "The project root is required for guarded transactional Save.";
                return result;
            }

            const fs::path screenPath = fs::u8path(screen.filePath_);
            const fs::path flowPath = fs::u8path(flow.filePath_);
            if (screenPath.lexically_normal() == flowPath.lexically_normal())
            {
                result.message = "Screen and StoryFlow cannot share one file path.";
                return result;
            }

            std::error_code ec;
            const fs::path tempRoot =
                fs::temp_directory_path(ec) /
                "renegade-ui-screen-flow-save" /
                fs::u8path(GenerateStableId());
            if (ec)
            {
                result.message = "Could not locate a temporary staging directory.";
                return result;
            }
            struct ScopedTemp
            {
                fs::path path;
                ~ScopedTemp()
                {
                    std::error_code ignored;
                    fs::remove_all(path, ignored);
                }
            } temp{tempRoot};
            fs::create_directories(temp.path, ec);
            if (ec)
            {
                result.message = "Could not create a temporary serialization directory.";
                return result;
            }

            const auto serializeScreen = [&](const ScreenDocument& document,
                const std::string& filename,
                std::vector<std::uint8_t>& bytes) -> bool
            {
                const auto path = temp.path / fs::u8path(filename);
                if (!WriteScreenDocument(path.generic_u8string(), document, error))
                    return false;
                return ReadBytes(path, bytes, error);
            };
            const auto serializeFlow = [&](const FlowDocument& document,
                const std::string& filename,
                std::vector<std::uint8_t>& bytes) -> bool
            {
                const auto path = temp.path / fs::u8path(filename);
                if (!WriteFlowDocument(path.generic_u8string(), document, error))
                    return false;
                return ReadBytes(path, bytes, error);
            };
            // Compare actual loaded bytes, not a reserialization: schema-v1
            // migration and harmless legacy formatting must not look like edits.
            const auto previousScreen = screen.persistedBytes_;
            const auto previousFlow = flow.persistedBytes_;
            std::vector<std::uint8_t> newScreen, newFlow;
            if (previousScreen.empty() || previousFlow.empty())
            {
                result.message = "No original persisted byte baseline is available.";
                return result;
            }
            if (!serializeScreen(screen.Document(), "screen-after.screen", newScreen) ||
                !serializeFlow(flow.Document(), "flow-after.flow", newFlow))
            {
                result.message = "Could not serialize a validated authored snapshot: " + error;
                return result;
            }
            // Fail closed on external edits (including changes to saved files
            // made by a separate editor since these sessions were opened).
            if (!MatchesBytes(screenPath, previousScreen, error) ||
                !MatchesBytes(flowPath, previousFlow, error))
            {
                result.message =
                    "An on-disk Screen or StoryFlow file changed outside this editing session: " +
                    error;
                return result;
            }

            if (!PreserveScreenUpgradeOriginal(
                    screenPath.generic_u8string(), screen.Document(), error))
            {
                result.message = "Cannot preserve original Screen before upgrade: " + error;
                return result;
            }
            const std::string screenDestination = screenPath.generic_u8string();
            const std::string flowDestination = flowPath.generic_u8string();
            const StableId projectId = screen.projectId_;
            const StableId screenId = screen.Document().envelope.documentId;
            const StableId flowId = flow.Document().envelope.documentId;

            ProjectDocumentWrite screenWrite;
            screenWrite.destinationPath = screenDestination;
            screenWrite.content = newScreen;
            screenWrite.validator = [
                projectId, screenId, newScreen](
                    const std::string& staged, std::string& failure)
            {
                if (!MatchesBytes(fs::u8path(staged), newScreen, failure))
                    return false;
                ScreenDocument loaded;
                if (!ReadScreenDocument(staged, projectId, loaded, failure))
                    return false;
                if (loaded.envelope.documentId != screenId)
                {
                    failure = "Staged Screen changed document identity.";
                    return false;
                }
                return true;
            };
            ProjectDocumentWrite flowWrite;
            flowWrite.destinationPath = flowDestination;
            flowWrite.content = newFlow;
            flowWrite.validator = [
                projectId, flowId, newFlow](
                    const std::string& staged, std::string& failure)
            {
                if (!MatchesBytes(fs::u8path(staged), newFlow, failure))
                    return false;
                FlowDocument loaded;
                if (!ReadFlowDocument(staged, projectId, loaded, failure))
                    return false;
                if (loaded.envelope.documentId != flowId)
                {
                    failure = "Staged StoryFlow changed document identity.";
                    return false;
                }
                return true;
            };

            options.allowedRoot = projectRoot;
            if (options.journalDirectory.empty())
                options.journalDirectory =
                    (fs::u8path(projectRoot) / "Intermediate" / "Transactions").generic_u8string();
            // Recheck both baseline bytes at transaction Prepare, so an
            // intervening external file edit is rejected before staging.
            const auto originalHook = options.operationHook;
            options.operationHook = [
                originalHook, screenDestination, flowDestination,
                previousScreen, previousFlow](
                    ProjectDocumentTransactionStage stage,
                    std::size_t index,
                    const std::string& path,
                    std::string& failure)
            {
                if (stage == ProjectDocumentTransactionStage::Prepare)
                {
                    if (path == screenDestination &&
                        !MatchesBytes(
                            fs::u8path(path), previousScreen, failure))
                        return ProjectDocumentTransactionHookAction::Fail;
                    if (path == flowDestination &&
                        !MatchesBytes(
                            fs::u8path(path), previousFlow, failure))
                        return ProjectDocumentTransactionHookAction::Fail;
                }
                return originalHook ? originalHook(stage, index, path, failure)
                    : ProjectDocumentTransactionHookAction::Continue;
            };
            ProjectDocumentTransaction transaction;
            result.transaction = transaction.Execute(
                {std::move(screenWrite), std::move(flowWrite)}, std::move(options));
            if (!result.transaction.success)
            {
                result.message = "Joint Screen/StoryFlow Save failed [" +
                    result.transaction.code + "]: " + result.transaction.message;
                return result;
            }

            if (!MatchesBytes(screenPath, newScreen, error) ||
                !MatchesBytes(flowPath, newFlow, error))
            {
                result.message =
                    "The transaction reported success but committed bytes were not verified: " +
                    error;
                return result;
            }

            // Only acknowledge persistence after the entire transaction and
            // both post-commit byte verifications succeeded. Never Reload():
            // that would erase the owner's existing Undo/Redo history.
            screen.savedHistoryIndex_ = screen.historyIndex_;
            flow.savedHistoryIndex_ = flow.historyIndex_;
            screen.persistedSnapshot_ = screen.Document();
            flow.persistedSnapshot_ = flow.Document();
            screen.persistedBytes_ = newScreen;
            flow.persistedBytes_ = newFlow;
            result.succeeded = true;
            result.noChanges = result.transaction.noChanges;
            result.message = result.noChanges
                ? "Screen and StoryFlow are already saved."
                : "Screen and StoryFlow saved atomically with Undo histories preserved.";
            return result;
        }

    private:
        static bool ReadBytes(
            const std::filesystem::path& path,
            std::vector<std::uint8_t>& bytes,
            std::string& error)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                error = "Cannot read " + path.generic_u8string();
                return false;
            }
            bytes.assign(std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>());
            if (stream.bad())
            {
                error = "Read failed: " + path.generic_u8string();
                return false;
            }
            error.clear();
            return true;
        }

        static bool MatchesBytes(
            const std::filesystem::path& path,
            const std::vector<std::uint8_t>& expected,
            std::string& error)
        {
            std::vector<std::uint8_t> actual;
            if (!ReadBytes(path, actual, error)) return false;
            if (actual != expected)
            {
                error = "File content differs: " + path.generic_u8string();
                return false;
            }
            error.clear();
            return true;
        }
    };
}
