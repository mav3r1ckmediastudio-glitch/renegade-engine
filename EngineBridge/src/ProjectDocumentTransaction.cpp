#include "renegade/bridge/ProjectDocumentTransaction.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <utility>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace
{
    namespace fs = std::filesystem;
    using renegade::bridge::ProjectDocumentTransactionEvent;
    using renegade::bridge::ProjectDocumentTransactionHookAction;
    using renegade::bridge::ProjectDocumentTransactionOptions;
    using renegade::bridge::ProjectDocumentTransactionResult;
    using renegade::bridge::ProjectDocumentTransactionStage;
    using renegade::bridge::ProjectTransactionNoDocument;

    constexpr const char* JournalMagic =
        "renegade-project-document-transaction";
    constexpr std::uint32_t JournalVersion = 1;
    std::atomic<std::uint64_t> transactionSequence{0};

    enum class JournalPhase
    {
        Preparing,
        Prepared,
        Committing,
        RollingBack,
        RolledBack,
        Committed,
    };

    struct DocumentPlan
    {
        fs::path destination;
        fs::path staged;
        fs::path backup;
        fs::path restore;
        std::vector<std::uint8_t> content;
        renegade::bridge::ProjectDocumentValidator validator;
        bool existed = false;
        bool changed = true;
    };

    struct Journal
    {
        std::string transactionId;
        JournalPhase phase = JournalPhase::Preparing;
        std::size_t commitCount = 0;
        std::size_t activeIndex = ProjectTransactionNoDocument;
        std::vector<DocumentPlan> documents;
    };

    std::string GenerateTransactionId()
    {
        const auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::ostringstream token;
        token << ticks << '-' << std::setfill('0') << std::setw(10)
              << transactionSequence.fetch_add(1);
        return token.str();
    }

    bool IsSafeToken(const std::string& token)
    {
        if (token.empty() || token.size() > 96)
        {
            return false;
        }
        return std::all_of(
            token.begin(),
            token.end(),
            [](const unsigned char character)
            {
                return std::isalnum(character) != 0 ||
                    character == '-' || character == '_';
            });
    }

    fs::path NormalizedAbsolute(const fs::path& path, std::error_code& error)
    {
        const fs::path absolute = fs::absolute(path, error);
        if (error)
        {
            return {};
        }
        return absolute.lexically_normal();
    }

    std::string PathKey(const fs::path& path)
    {
        std::string key = path.generic_u8string();
#if defined(_WIN32)
        std::transform(
            key.begin(),
            key.end(),
            key.begin(),
            [](const unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
#endif
        return key;
    }


    bool IsPathWithinRoot(const fs::path& root, const fs::path& path)
    {
        std::string rootKey = PathKey(root);
        const std::string pathKey = PathKey(path);
        if (pathKey == rootKey)
        {
            return true;
        }
        if (rootKey.empty())
        {
            return false;
        }
        if (rootKey.back() != '/')
        {
            rootKey.push_back('/');
        }
        return pathKey.rfind(rootKey, 0) == 0;
    }

    bool ResolveAllowedRoot(
        const ProjectDocumentTransactionOptions& options,
        fs::path& root,
        std::string& error)
    {
        root.clear();
        error.clear();
        if (options.allowedRoot.empty())
        {
            return true;
        }

        std::error_code pathError;
        root = NormalizedAbsolute(fs::u8path(options.allowedRoot), pathError);
        if (pathError || root.empty())
        {
            error = "Could not resolve the project transaction root: " +
                pathError.message();
            return false;
        }
        if (!fs::is_directory(root, pathError) || pathError)
        {
            error = "The project transaction root is not a directory: " +
                root.generic_u8string();
            return false;
        }
        return true;
    }

    bool ReplaceFileAtomically(
        const fs::path& temporary,
        const fs::path& destination,
        std::error_code& error)
    {
        error.clear();
#if defined(_WIN32)
        const bool destinationExists = fs::exists(destination, error);
        if (error)
        {
            return false;
        }

        BOOL replaced = FALSE;
        if (destinationExists)
        {
            replaced = ReplaceFileW(
                destination.c_str(),
                temporary.c_str(),
                nullptr,
                REPLACEFILE_WRITE_THROUGH,
                nullptr,
                nullptr);
        }
        else
        {
            replaced = MoveFileExW(
                temporary.c_str(),
                destination.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        }
        if (replaced == FALSE)
        {
            error = std::error_code(
                static_cast<int>(GetLastError()),
                std::system_category());
            return false;
        }
        return true;
#else
        fs::rename(temporary, destination, error);
        return !error;
#endif
    }

    void RemoveWithoutThrow(const fs::path& path)
    {
        std::error_code ignored;
        fs::remove(path, ignored);
    }

    const char* PhaseName(const JournalPhase phase)
    {
        switch (phase)
        {
        case JournalPhase::Preparing:
            return "preparing";
        case JournalPhase::Prepared:
            return "prepared";
        case JournalPhase::Committing:
            return "committing";
        case JournalPhase::RollingBack:
            return "rolling_back";
        case JournalPhase::RolledBack:
            return "rolled_back";
        case JournalPhase::Committed:
            return "committed";
        }
        return "invalid";
    }

    bool ParsePhase(const std::string& text, JournalPhase& phase)
    {
        if (text == "preparing")
        {
            phase = JournalPhase::Preparing;
        }
        else if (text == "prepared")
        {
            phase = JournalPhase::Prepared;
        }
        else if (text == "committing")
        {
            phase = JournalPhase::Committing;
        }
        else if (text == "rolling_back")
        {
            phase = JournalPhase::RollingBack;
        }
        else if (text == "rolled_back")
        {
            phase = JournalPhase::RolledBack;
        }
        else if (text == "committed")
        {
            phase = JournalPhase::Committed;
        }
        else
        {
            return false;
        }
        return true;
    }

    void AddEvent(
        ProjectDocumentTransactionResult& result,
        const ProjectDocumentTransactionStage stage,
        const std::size_t index,
        const fs::path& path,
        std::string code)
    {
        result.events.push_back(ProjectDocumentTransactionEvent{
            stage,
            index,
            path.empty() ? std::string{} : path.generic_u8string(),
            std::move(code),
        });
    }

    ProjectDocumentTransactionHookAction InvokeHook(
        const ProjectDocumentTransactionOptions& options,
        const ProjectDocumentTransactionStage stage,
        const std::size_t index,
        const fs::path& path,
        std::string& error)
    {
        error.clear();
        if (!options.operationHook)
        {
            return ProjectDocumentTransactionHookAction::Continue;
        }
        try
        {
            return options.operationHook(
                stage,
                index,
                path.empty() ? std::string{} : path.generic_u8string(),
                error);
        }
        catch (const std::exception& exception)
        {
            error = std::string("Transaction hook threw an exception: ") +
                exception.what();
            return ProjectDocumentTransactionHookAction::Fail;
        }
        catch (...)
        {
            error = "Transaction hook threw an unknown exception.";
            return ProjectDocumentTransactionHookAction::Fail;
        }
    }

    bool WriteBytes(
        const fs::path& path,
        const std::vector<std::uint8_t>& content,
        std::string& error)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "Could not create staged document: " +
                path.generic_u8string();
            return false;
        }
        if (!content.empty())
        {
            stream.write(
                reinterpret_cast<const char*>(content.data()),
                static_cast<std::streamsize>(content.size()));
        }
        stream.flush();
        if (!stream)
        {
            error = "Could not write staged document: " +
                path.generic_u8string();
            return false;
        }
        stream.close();
        if (!stream)
        {
            error = "Could not close staged document: " +
                path.generic_u8string();
            return false;
        }
        error.clear();
        return true;
    }

    bool FileMatchesContent(
        const fs::path& path,
        const std::vector<std::uint8_t>& content,
        std::string& error)
    {
        std::error_code fileError;
        const auto size = fs::file_size(path, fileError);
        if (fileError)
        {
            error = "Could not inspect document size: " + fileError.message();
            return false;
        }
        if (size != content.size())
        {
            error.clear();
            return false;
        }

        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            error = "Could not read document: " + path.generic_u8string();
            return false;
        }

        std::vector<std::uint8_t> buffer(64 * 1024);
        std::size_t offset = 0;
        while (offset < content.size())
        {
            const std::size_t amount = std::min(
                buffer.size(),
                content.size() - offset);
            stream.read(
                reinterpret_cast<char*>(buffer.data()),
                static_cast<std::streamsize>(amount));
            if (stream.gcount() != static_cast<std::streamsize>(amount))
            {
                error = "Could not read complete document: " +
                    path.generic_u8string();
                return false;
            }
            if (!std::equal(
                    buffer.begin(),
                    buffer.begin() + static_cast<std::ptrdiff_t>(amount),
                    content.begin() + static_cast<std::ptrdiff_t>(offset)))
            {
                error.clear();
                return false;
            }
            offset += amount;
        }
        error.clear();
        return true;
    }

    bool InvokeValidator(
        const renegade::bridge::ProjectDocumentValidator& validator,
        const fs::path& path,
        std::string& error)
    {
        if (!validator)
        {
            error.clear();
            return true;
        }
        try
        {
            if (validator(path.generic_u8string(), error))
            {
                error.clear();
                return true;
            }
            if (error.empty())
            {
                error = "The project document failed validation: " +
                    path.generic_u8string();
            }
            return false;
        }
        catch (const std::exception& exception)
        {
            error = "Project document validation threw an exception: " +
                std::string(exception.what());
            return false;
        }
        catch (...)
        {
            error = "Project document validation threw an unknown exception.";
            return false;
        }
    }

    bool ValidateDocumentAtPath(
        const DocumentPlan& plan,
        const fs::path& path,
        std::string& error)
    {
        std::error_code fileError;
        if (!fs::is_regular_file(path, fileError) || fileError)
        {
            error = "The project document is not a readable regular file: " +
                path.generic_u8string();
            return false;
        }
        if (fs::file_size(path, fileError) != plan.content.size() ||
            fileError)
        {
            error = "The project document length does not match its payload: " +
                path.generic_u8string();
            return false;
        }

        std::string matchError;
        if (!FileMatchesContent(path, plan.content, matchError))
        {
            error = matchError.empty()
                ? "The project document bytes differ from the staged payload: " +
                    path.generic_u8string()
                : std::move(matchError);
            return false;
        }
        return InvokeValidator(plan.validator, path, error);
    }

    bool ValidateStaged(
        const DocumentPlan& plan,
        std::string& error)
    {
        return ValidateDocumentAtPath(plan, plan.staged, error);
    }

    bool SerializeJournal(
        const Journal& journal,
        std::string& text,
        std::string& error)
    {
        std::ostringstream output;
        output << JournalMagic << ' ' << JournalVersion << '\n';
        output << "transaction_id " << std::quoted(journal.transactionId) << '\n';
        output << "phase " << std::quoted(PhaseName(journal.phase)) << '\n';
        output << "commit_count " << journal.commitCount << '\n';
        if (journal.activeIndex == ProjectTransactionNoDocument)
        {
            output << "active_index -1\n";
        }
        else
        {
            output << "active_index " << journal.activeIndex << '\n';
        }
        output << "document_count " << journal.documents.size() << '\n';
        for (const auto& document : journal.documents)
        {
            output << "document "
                   << std::quoted(document.destination.generic_u8string()) << ' '
                   << std::quoted(document.staged.generic_u8string()) << ' '
                   << std::quoted(document.backup.generic_u8string()) << ' '
                   << std::quoted(document.restore.generic_u8string()) << ' '
                   << (document.existed ? 1 : 0) << '\n';
        }
        if (!output)
        {
            error = "Could not serialize the project transaction journal.";
            return false;
        }
        text = output.str();
        error.clear();
        return true;
    }

    bool PersistJournal(
        const Journal& journal,
        const fs::path& journalPath,
        std::string& error)
    {
        std::string text;
        if (!SerializeJournal(journal, text, error))
        {
            return false;
        }

        const fs::path temporary = journalPath.parent_path() /
            (journalPath.filename().generic_u8string() + ".writing");
        std::vector<std::uint8_t> bytes(text.begin(), text.end());
        if (!WriteBytes(temporary, bytes, error))
        {
            return false;
        }

        std::error_code replaceError;
        if (!ReplaceFileAtomically(temporary, journalPath, replaceError))
        {
            RemoveWithoutThrow(temporary);
            error = "Could not persist the project transaction journal: " +
                replaceError.message();
            return false;
        }
        error.clear();
        return true;
    }

    bool IsSafeJournalDocument(
        const DocumentPlan& document,
        const std::string& transactionId,
        std::string& error)
    {
        if (!document.destination.is_absolute() ||
            !document.staged.is_absolute() ||
            !document.backup.is_absolute() ||
            !document.restore.is_absolute())
        {
            error = "Transaction journal paths must be absolute.";
            return false;
        }
        const fs::path parent = document.destination.parent_path();
        if (document.staged.parent_path() != parent ||
            document.backup.parent_path() != parent ||
            document.restore.parent_path() != parent)
        {
            error = "Transaction staging and recovery files must remain beside "
                "their destination.";
            return false;
        }

        const std::string filename =
            document.destination.filename().generic_u8string();
        if (document.staged.filename() !=
                fs::u8path(filename + ".renegade-stage-" + transactionId) ||
            document.backup.filename() !=
                fs::u8path(filename + ".renegade-backup-" + transactionId) ||
            document.restore.filename() !=
                fs::u8path(filename + ".renegade-restore-" + transactionId))
        {
            error = "Transaction journal contains unexpected recovery paths.";
            return false;
        }
        error.clear();
        return true;
    }

    bool ReadJournal(
        const fs::path& journalPath,
        Journal& journal,
        std::string& error)
    {
        std::ifstream input(journalPath, std::ios::binary);
        if (!input)
        {
            error = "Could not open the project transaction journal: " +
                journalPath.generic_u8string();
            return false;
        }

        std::string magic;
        std::uint32_t version = 0;
        if (!(input >> magic >> version) || magic != JournalMagic ||
            version != JournalVersion)
        {
            error = "The project transaction journal header is invalid.";
            return false;
        }

        std::string label;
        std::string phaseText;
        long long activeIndex = -1;
        std::size_t documentCount = 0;
        if (!(input >> label) || label != "transaction_id" ||
            !(input >> std::quoted(journal.transactionId)) ||
            !(input >> label) || label != "phase" ||
            !(input >> std::quoted(phaseText)) ||
            !(input >> label) || label != "commit_count" ||
            !(input >> journal.commitCount) ||
            !(input >> label) || label != "active_index" ||
            !(input >> activeIndex) ||
            !(input >> label) || label != "document_count" ||
            !(input >> documentCount))
        {
            error = "The project transaction journal is incomplete.";
            return false;
        }
        if (!IsSafeToken(journal.transactionId) ||
            !ParsePhase(phaseText, journal.phase) ||
            documentCount == 0 || documentCount > 4096 ||
            journal.commitCount > documentCount ||
            activeIndex < -1 ||
            activeIndex >= static_cast<long long>(documentCount))
        {
            error = "The project transaction journal metadata is invalid.";
            return false;
        }
        journal.activeIndex = activeIndex < 0
            ? ProjectTransactionNoDocument
            : static_cast<std::size_t>(activeIndex);

        const bool inactive =
            journal.activeIndex == ProjectTransactionNoDocument;
        const bool phaseMetadataValid =
            ((journal.phase == JournalPhase::Preparing ||
              journal.phase == JournalPhase::Prepared) &&
                journal.commitCount == 0 && inactive) ||
            (journal.phase == JournalPhase::Committed &&
                journal.commitCount == documentCount && inactive) ||
            (journal.phase == JournalPhase::RolledBack &&
                journal.commitCount == 0 && inactive) ||
            ((journal.phase == JournalPhase::Committing ||
              journal.phase == JournalPhase::RollingBack) &&
                (inactive || journal.activeIndex == journal.commitCount));
        if (!phaseMetadataValid)
        {
            error = "The project transaction journal phase metadata is inconsistent.";
            return false;
        }

        journal.documents.clear();
        journal.documents.reserve(documentCount);
        std::unordered_set<std::string> destinations;
        for (std::size_t index = 0; index < documentCount; ++index)
        {
            DocumentPlan document;
            std::string destination;
            std::string staged;
            std::string backup;
            std::string restore;
            int existed = 0;
            if (!(input >> label) || label != "document" ||
                !(input >> std::quoted(destination) >> std::quoted(staged) >>
                    std::quoted(backup) >> std::quoted(restore) >> existed) ||
                (existed != 0 && existed != 1))
            {
                error = "The project transaction journal document list is invalid.";
                return false;
            }
            document.destination = fs::u8path(destination).lexically_normal();
            document.staged = fs::u8path(staged).lexically_normal();
            document.backup = fs::u8path(backup).lexically_normal();
            document.restore = fs::u8path(restore).lexically_normal();
            document.existed = existed != 0;
            if (!IsSafeJournalDocument(document, journal.transactionId, error))
            {
                return false;
            }
            if (!destinations.insert(PathKey(document.destination)).second)
            {
                error = "The project transaction journal contains duplicate destinations.";
                return false;
            }
            journal.documents.push_back(std::move(document));
        }

        input >> std::ws;
        if (!input.eof())
        {
            error = "The project transaction journal contains trailing data.";
            return false;
        }
        error.clear();
        return true;
    }

    bool CleanupJournalArtifacts(
        const Journal& journal,
        const fs::path& journalPath,
        const ProjectDocumentTransactionOptions& options,
        ProjectDocumentTransactionResult& result,
        std::string& error)
    {
        for (std::size_t index = 0; index < journal.documents.size(); ++index)
        {
            const auto& document = journal.documents[index];
            const fs::path artifacts[] = {
                document.staged,
                document.restore,
                document.backup,
            };
            for (const auto& artifact : artifacts)
            {
                std::error_code existsError;
                if (!fs::exists(artifact, existsError))
                {
                    if (existsError)
                    {
                        error = "Could not inspect transaction artifact: " +
                            existsError.message();
                        return false;
                    }
                    continue;
                }

                std::string hookError;
                const auto action = InvokeHook(
                    options,
                    ProjectDocumentTransactionStage::Cleanup,
                    index,
                    artifact,
                    hookError);
                if (action != ProjectDocumentTransactionHookAction::Continue)
                {
                    error = hookError.empty()
                        ? "Transaction cleanup was interrupted."
                        : std::move(hookError);
                    return false;
                }

                std::error_code removeError;
                fs::remove(artifact, removeError);
                if (removeError)
                {
                    error = "Could not remove transaction artifact: " +
                        removeError.message();
                    return false;
                }
                AddEvent(
                    result,
                    ProjectDocumentTransactionStage::Cleanup,
                    index,
                    artifact,
                    "artifact_removed");
            }
        }

        std::string hookError;
        const auto action = InvokeHook(
            options,
            ProjectDocumentTransactionStage::Cleanup,
            ProjectTransactionNoDocument,
            journalPath,
            hookError);
        if (action != ProjectDocumentTransactionHookAction::Continue)
        {
            error = hookError.empty()
                ? "Transaction journal cleanup was interrupted."
                : std::move(hookError);
            return false;
        }

        std::error_code removeError;
        fs::remove(journalPath, removeError);
        if (removeError)
        {
            error = "Could not remove the transaction journal: " +
                removeError.message();
            return false;
        }
        AddEvent(
            result,
            ProjectDocumentTransactionStage::Cleanup,
            ProjectTransactionNoDocument,
            journalPath,
            "journal_removed");
        error.clear();
        return true;
    }

    bool RestoreDocument(
        const DocumentPlan& document,
        const std::size_t index,
        const ProjectDocumentTransactionOptions& options,
        ProjectDocumentTransactionResult& result,
        std::string& error)
    {
        std::string hookError;
        const auto action = InvokeHook(
            options,
            ProjectDocumentTransactionStage::Rollback,
            index,
            document.destination,
            hookError);
        if (action != ProjectDocumentTransactionHookAction::Continue)
        {
            error = hookError.empty()
                ? "Transaction rollback was interrupted."
                : std::move(hookError);
            return false;
        }

        if (!document.existed)
        {
            std::error_code removeError;
            fs::remove(document.destination, removeError);
            if (removeError)
            {
                error = "Could not remove a newly created transaction destination: " +
                    removeError.message();
                return false;
            }
            AddEvent(
                result,
                ProjectDocumentTransactionStage::Rollback,
                index,
                document.destination,
                "new_destination_removed");
            error.clear();
            return true;
        }

        std::error_code fileError;
        if (!fs::is_regular_file(document.backup, fileError) || fileError)
        {
            error = "The protected previous document is unavailable: " +
                document.backup.generic_u8string();
            return false;
        }

        fs::copy_file(
            document.backup,
            document.restore,
            fs::copy_options::overwrite_existing,
            fileError);
        if (fileError)
        {
            error = "Could not prepare a rollback copy: " + fileError.message();
            return false;
        }

        fileError.clear();
        if (!ReplaceFileAtomically(
                document.restore,
                document.destination,
                fileError))
        {
            RemoveWithoutThrow(document.restore);
            error = "Could not restore the previous document: " +
                fileError.message();
            return false;
        }
        AddEvent(
            result,
            ProjectDocumentTransactionStage::Rollback,
            index,
            document.destination,
            "previous_destination_restored");
        error.clear();
        return true;
    }

    bool RollbackJournal(
        Journal& journal,
        const fs::path& journalPath,
        const ProjectDocumentTransactionOptions& options,
        ProjectDocumentTransactionResult& result,
        std::string& error)
    {
        journal.phase = JournalPhase::RollingBack;
        std::string persistError;
        if (!PersistJournal(journal, journalPath, persistError))
        {
            error = "Could not record rollback state: " + persistError;
            return false;
        }

        std::size_t potentialCount = journal.commitCount;
        if (journal.activeIndex != ProjectTransactionNoDocument)
        {
            potentialCount = std::max(
                potentialCount,
                journal.activeIndex + 1);
        }
        potentialCount = std::min(potentialCount, journal.documents.size());

        for (std::size_t reverse = potentialCount; reverse > 0; --reverse)
        {
            const std::size_t index = reverse - 1;
            if (!RestoreDocument(
                    journal.documents[index],
                    index,
                    options,
                    result,
                    error))
            {
                return false;
            }
        }

        journal.phase = JournalPhase::RolledBack;
        journal.commitCount = 0;
        journal.activeIndex = ProjectTransactionNoDocument;
        if (!PersistJournal(journal, journalPath, error))
        {
            return false;
        }
        result.rolledBack = true;
        error.clear();
        return true;
    }

    ProjectDocumentTransactionResult FailureResult(
        ProjectDocumentTransactionResult result,
        const ProjectDocumentTransactionStage stage,
        const std::size_t index,
        std::string code,
        std::string message)
    {
        result.success = false;
        result.stage = stage;
        result.documentIndex = index;
        result.code = std::move(code);
        result.message = std::move(message);
        return result;
    }
}

namespace renegade::bridge
{
    ProjectDocumentTransactionResult ProjectDocumentTransaction::Execute(
        std::vector<ProjectDocumentWrite> documents,
        ProjectDocumentTransactionOptions options) const
    {
        ProjectDocumentTransactionResult result;
        result.transactionId = options.transactionId.empty()
            ? GenerateTransactionId()
            : options.transactionId;

        if (!IsSafeToken(result.transactionId))
        {
            return FailureResult(
                std::move(result),
                ProjectDocumentTransactionStage::Prepare,
                ProjectTransactionNoDocument,
                "invalid_transaction_id",
                "The project transaction ID contains unsafe characters.");
        }
        if (documents.empty())
        {
            return FailureResult(
                std::move(result),
                ProjectDocumentTransactionStage::Prepare,
                ProjectTransactionNoDocument,
                "empty_transaction",
                "A project document transaction requires at least one document.");
        }

        fs::path allowedRoot;
        std::string allowedRootError;
        if (!ResolveAllowedRoot(options, allowedRoot, allowedRootError))
        {
            return FailureResult(
                std::move(result),
                ProjectDocumentTransactionStage::Prepare,
                ProjectTransactionNoDocument,
                "invalid_allowed_root",
                std::move(allowedRootError));
        }

        Journal journal;
        journal.transactionId = result.transactionId;
        journal.documents.reserve(documents.size());
        std::unordered_set<std::string> destinations;

        for (std::size_t sourceIndex = 0;
            sourceIndex < documents.size(); ++sourceIndex)
        {
            auto& document = documents[sourceIndex];
            if (document.destinationPath.empty())
            {
                return FailureResult(
                    std::move(result),
                    ProjectDocumentTransactionStage::Prepare,
                    sourceIndex,
                    "missing_destination",
                    "A transaction document is missing its destination path.");
            }

            std::string hookError;
            const auto prepareAction = InvokeHook(
                options,
                ProjectDocumentTransactionStage::Prepare,
                sourceIndex,
                fs::u8path(document.destinationPath),
                hookError);
            if (prepareAction != ProjectDocumentTransactionHookAction::Continue)
            {
                return FailureResult(
                    std::move(result),
                    ProjectDocumentTransactionStage::Prepare,
                    sourceIndex,
                    prepareAction ==
                            ProjectDocumentTransactionHookAction::Interrupt
                        ? "preparation_interrupted"
                        : "preparation_failed",
                    hookError.empty()
                        ? "Project transaction preparation was interrupted."
                        : std::move(hookError));
            }

            std::error_code pathError;
            const fs::path destination = NormalizedAbsolute(
                fs::u8path(document.destinationPath),
                pathError);
            if (pathError || destination.filename().empty())
            {
                return FailureResult(
                    std::move(result),
                    ProjectDocumentTransactionStage::Prepare,
                    sourceIndex,
                    "invalid_destination",
                    "Could not resolve a transaction destination path: " +
                        pathError.message());
            }
            if (!allowedRoot.empty() &&
                !IsPathWithinRoot(allowedRoot, destination))
            {
                return FailureResult(
                    std::move(result),
                    ProjectDocumentTransactionStage::Prepare,
                    sourceIndex,
                    "destination_outside_allowed_root",
                    "A project transaction destination escapes its project root: " +
                        destination.generic_u8string());
            }
            if (!destinations.insert(PathKey(destination)).second)
            {
                return FailureResult(
                    std::move(result),
                    ProjectDocumentTransactionStage::Prepare,
                    sourceIndex,
                    "duplicate_destination",
                    "A project transaction contains the same destination more than once: " +
                        destination.generic_u8string());
            }

            const fs::path parent = destination.parent_path();
            fs::create_directories(parent, pathError);
            if (pathError)
            {
                return FailureResult(
                    std::move(result),
                    ProjectDocumentTransactionStage::Prepare,
                    sourceIndex,
                    "destination_directory_failed",
                    "Could not prepare a transaction destination directory: " +
                        pathError.message());
            }

            const bool exists = fs::exists(destination, pathError);
            if (pathError ||
                (exists && !fs::is_regular_file(destination, pathError)))
            {
                return FailureResult(
                    std::move(result),
                    ProjectDocumentTransactionStage::Prepare,
                    sourceIndex,
                    "invalid_destination_type",
                    "A transaction destination is not a regular file: " +
                        destination.generic_u8string());
            }

            DocumentPlan plan;
            plan.destination = destination;
            const std::string filename = destination.filename().generic_u8string();
            plan.staged = parent / fs::u8path(
                filename + ".renegade-stage-" + result.transactionId);
            plan.backup = parent / fs::u8path(
                filename + ".renegade-backup-" + result.transactionId);
            plan.restore = parent / fs::u8path(
                filename + ".renegade-restore-" + result.transactionId);
            plan.content = std::move(document.content);
            plan.validator = std::move(document.validator);
            plan.existed = exists;

            for (const fs::path* artifact :
                {&plan.staged, &plan.backup, &plan.restore})
            {
                if (fs::exists(*artifact, pathError) || pathError)
                {
                    return FailureResult(
                        std::move(result),
                        ProjectDocumentTransactionStage::Prepare,
                        sourceIndex,
                        "transaction_artifact_collision",
                        "A transaction artifact already exists: " +
                            artifact->generic_u8string());
                }
            }

            if (exists)
            {
                std::string compareError;
                const bool matches = FileMatchesContent(
                    destination,
                    plan.content,
                    compareError);
                if (!compareError.empty())
                {
                    return FailureResult(
                        std::move(result),
                        ProjectDocumentTransactionStage::Prepare,
                        sourceIndex,
                        "destination_compare_failed",
                        std::move(compareError));
                }
                plan.changed = !matches;
            }
            journal.documents.push_back(std::move(plan));
        }

        std::sort(
            journal.documents.begin(),
            journal.documents.end(),
            [](const DocumentPlan& left, const DocumentPlan& right)
            {
                return PathKey(left.destination) < PathKey(right.destination);
            });

        std::vector<DocumentPlan> changedDocuments;
        changedDocuments.reserve(journal.documents.size());
        for (std::size_t index = 0;
            index < journal.documents.size(); ++index)
        {
            auto& document = journal.documents[index];
            if (!document.changed)
            {
                std::string hookError;
                const auto action = InvokeHook(
                    options,
                    ProjectDocumentTransactionStage::Validate,
                    index,
                    document.destination,
                    hookError);
                if (action != ProjectDocumentTransactionHookAction::Continue)
                {
                    return FailureResult(
                        std::move(result),
                        ProjectDocumentTransactionStage::Validate,
                        index,
                        action == ProjectDocumentTransactionHookAction::Interrupt
                            ? "validation_interrupted"
                            : "validation_failed",
                        hookError.empty()
                            ? "Unchanged document validation was interrupted."
                            : std::move(hookError));
                }
                if (!InvokeValidator(
                        document.validator,
                        document.destination,
                        hookError))
                {
                    return FailureResult(
                        std::move(result),
                        ProjectDocumentTransactionStage::Validate,
                        index,
                        "validation_failed",
                        std::move(hookError));
                }
                AddEvent(
                    result,
                    ProjectDocumentTransactionStage::Validate,
                    index,
                    document.destination,
                    "unchanged_document_validated");
            }
            else
            {
                changedDocuments.push_back(std::move(document));
            }
        }
        journal.documents = std::move(changedDocuments);

        if (journal.documents.empty())
        {
            result.success = true;
            result.committed = true;
            result.noChanges = true;
            result.stage = ProjectDocumentTransactionStage::Complete;
            result.code = "no_changes";
            result.message = "All project documents already match the requested content.";
            return result;
        }

        std::error_code journalError;
        fs::path journalDirectory;
        if (options.journalDirectory.empty())
        {
            journalDirectory = journal.documents.front().destination.parent_path();
        }
        else
        {
            journalDirectory = NormalizedAbsolute(
                fs::u8path(options.journalDirectory),
                journalError);
        }
        if (journalError)
        {
            return FailureResult(
                std::move(result),
                ProjectDocumentTransactionStage::Journal,
                ProjectTransactionNoDocument,
                "journal_directory_failed",
                "Could not resolve the transaction journal directory: " +
                    journalError.message());
        }
        if (!allowedRoot.empty() &&
            !IsPathWithinRoot(allowedRoot, journalDirectory))
        {
            return FailureResult(
                std::move(result),
                ProjectDocumentTransactionStage::Journal,
                ProjectTransactionNoDocument,
                "journal_outside_allowed_root",
                "The project transaction journal directory escapes its project root: " +
                    journalDirectory.generic_u8string());
        }
        fs::create_directories(journalDirectory, journalError);
        if (journalError)
        {
            return FailureResult(
                std::move(result),
                ProjectDocumentTransactionStage::Journal,
                ProjectTransactionNoDocument,
                "journal_directory_failed",
                "Could not create the transaction journal directory: " +
                    journalError.message());
        }

        const fs::path journalPath = journalDirectory / fs::u8path(
            ".renegade-transaction-" + result.transactionId + ".journal");
        result.journalPath = journalPath.generic_u8string();
        if (fs::exists(journalPath, journalError) || journalError)
        {
            return FailureResult(
                std::move(result),
                ProjectDocumentTransactionStage::Journal,
                ProjectTransactionNoDocument,
                "journal_collision",
                "A transaction journal already exists: " +
                    journalPath.generic_u8string());
        }

        std::string operationError;
        auto journalAction = InvokeHook(
            options,
            ProjectDocumentTransactionStage::Journal,
            ProjectTransactionNoDocument,
            journalPath,
            operationError);
        if (journalAction != ProjectDocumentTransactionHookAction::Continue ||
            !PersistJournal(journal, journalPath, operationError))
        {
            return FailureResult(
                std::move(result),
                ProjectDocumentTransactionStage::Journal,
                ProjectTransactionNoDocument,
                journalAction == ProjectDocumentTransactionHookAction::Interrupt
                    ? "journal_interrupted"
                    : "journal_write_failed",
                operationError.empty()
                    ? "Could not establish the project transaction journal."
                    : std::move(operationError));
        }
        AddEvent(
            result,
            ProjectDocumentTransactionStage::Journal,
            ProjectTransactionNoDocument,
            journalPath,
            "journal_created");

        auto failBeforeCommit = [&](const ProjectDocumentTransactionStage stage,
                                    const std::size_t index,
                                    std::string code,
                                    std::string message,
                                    const bool interrupted)
        {
            if (interrupted)
            {
                result.recoveryRequired = true;
                return FailureResult(
                    std::move(result),
                    stage,
                    index,
                    std::move(code),
                    std::move(message));
            }

            std::string cleanupError;
            if (!CleanupJournalArtifacts(
                    journal,
                    journalPath,
                    options,
                    result,
                    cleanupError))
            {
                result.recoveryRequired = true;
                message += " Cleanup is still required: " + cleanupError;
            }
            return FailureResult(
                std::move(result),
                stage,
                index,
                std::move(code),
                std::move(message));
        };

        for (std::size_t index = 0;
            index < journal.documents.size(); ++index)
        {
            auto& document = journal.documents[index];
            auto action = InvokeHook(
                options,
                ProjectDocumentTransactionStage::StageWrite,
                index,
                document.staged,
                operationError);
            if (action != ProjectDocumentTransactionHookAction::Continue ||
                !WriteBytes(document.staged, document.content, operationError))
            {
                return failBeforeCommit(
                    ProjectDocumentTransactionStage::StageWrite,
                    index,
                    action == ProjectDocumentTransactionHookAction::Interrupt
                        ? "staging_interrupted"
                        : "staging_failed",
                    operationError.empty()
                        ? "Could not stage a project document."
                        : std::move(operationError),
                    action == ProjectDocumentTransactionHookAction::Interrupt);
            }
            AddEvent(
                result,
                ProjectDocumentTransactionStage::StageWrite,
                index,
                document.staged,
                "document_staged");

            action = InvokeHook(
                options,
                ProjectDocumentTransactionStage::Validate,
                index,
                document.staged,
                operationError);
            if (action != ProjectDocumentTransactionHookAction::Continue ||
                !ValidateStaged(document, operationError))
            {
                return failBeforeCommit(
                    ProjectDocumentTransactionStage::Validate,
                    index,
                    action == ProjectDocumentTransactionHookAction::Interrupt
                        ? "validation_interrupted"
                        : "validation_failed",
                    operationError.empty()
                        ? "A staged project document failed validation."
                        : std::move(operationError),
                    action == ProjectDocumentTransactionHookAction::Interrupt);
            }
            AddEvent(
                result,
                ProjectDocumentTransactionStage::Validate,
                index,
                document.staged,
                "staged_document_validated");

            if (document.existed)
            {
                action = InvokeHook(
                    options,
                    ProjectDocumentTransactionStage::Backup,
                    index,
                    document.backup,
                    operationError);
                std::error_code copyError;
                if (action == ProjectDocumentTransactionHookAction::Continue)
                {
                    fs::copy_file(
                        document.destination,
                        document.backup,
                        fs::copy_options::overwrite_existing,
                        copyError);
                    if (copyError)
                    {
                        operationError = "Could not protect the previous project document: " +
                            copyError.message();
                    }
                }
                if (action != ProjectDocumentTransactionHookAction::Continue ||
                    copyError)
                {
                    return failBeforeCommit(
                        ProjectDocumentTransactionStage::Backup,
                        index,
                        action == ProjectDocumentTransactionHookAction::Interrupt
                            ? "backup_interrupted"
                            : "backup_failed",
                        operationError.empty()
                            ? "Could not protect a previous project document."
                            : std::move(operationError),
                        action == ProjectDocumentTransactionHookAction::Interrupt);
                }
                AddEvent(
                    result,
                    ProjectDocumentTransactionStage::Backup,
                    index,
                    document.backup,
                    "previous_document_protected");
            }
        }

        journal.phase = JournalPhase::Prepared;
        if (!PersistJournal(journal, journalPath, operationError))
        {
            return failBeforeCommit(
                ProjectDocumentTransactionStage::Journal,
                ProjectTransactionNoDocument,
                "journal_write_failed",
                std::move(operationError),
                false);
        }

        auto failDuringCommit = [&](const ProjectDocumentTransactionStage stage,
                                    const std::size_t index,
                                    std::string code,
                                    std::string message,
                                    const bool interrupted)
        {
            if (interrupted)
            {
                result.recoveryRequired = true;
                return FailureResult(
                    std::move(result),
                    stage,
                    index,
                    std::move(code),
                    std::move(message));
            }

            std::string rollbackError;
            if (!RollbackJournal(
                    journal,
                    journalPath,
                    options,
                    result,
                    rollbackError))
            {
                result.recoveryRequired = true;
                message += " Rollback is incomplete: " + rollbackError;
                return FailureResult(
                    std::move(result),
                    ProjectDocumentTransactionStage::Rollback,
                    index,
                    "rollback_failed",
                    std::move(message));
            }

            std::string cleanupError;
            if (!CleanupJournalArtifacts(
                    journal,
                    journalPath,
                    options,
                    result,
                    cleanupError))
            {
                result.recoveryRequired = true;
                message += " Rollback completed, but cleanup remains: " +
                    cleanupError;
            }
            return FailureResult(
                std::move(result),
                stage,
                index,
                std::move(code),
                std::move(message));
        };

        for (std::size_t index = 0;
            index < journal.documents.size(); ++index)
        {
            auto& document = journal.documents[index];
            journal.phase = JournalPhase::Committing;
            journal.activeIndex = index;
            if (!PersistJournal(journal, journalPath, operationError))
            {
                return failDuringCommit(
                    ProjectDocumentTransactionStage::Journal,
                    index,
                    "journal_write_failed",
                    std::move(operationError),
                    false);
            }

            auto action = InvokeHook(
                options,
                ProjectDocumentTransactionStage::Replace,
                index,
                document.destination,
                operationError);
            std::error_code replaceError;
            if (action == ProjectDocumentTransactionHookAction::Continue &&
                !ReplaceFileAtomically(
                    document.staged,
                    document.destination,
                    replaceError))
            {
                operationError = "Could not replace a project document: " +
                    replaceError.message();
            }
            if (action != ProjectDocumentTransactionHookAction::Continue ||
                replaceError)
            {
                return failDuringCommit(
                    ProjectDocumentTransactionStage::Replace,
                    index,
                    action == ProjectDocumentTransactionHookAction::Interrupt
                        ? "replacement_interrupted"
                        : "replacement_failed",
                    operationError.empty()
                        ? "Could not replace a project document."
                        : std::move(operationError),
                    action == ProjectDocumentTransactionHookAction::Interrupt);
            }
            AddEvent(
                result,
                ProjectDocumentTransactionStage::Replace,
                index,
                document.destination,
                "destination_replaced");

            action = InvokeHook(
                options,
                ProjectDocumentTransactionStage::AfterReplace,
                index,
                document.destination,
                operationError);
            if (action != ProjectDocumentTransactionHookAction::Continue)
            {
                return failDuringCommit(
                    ProjectDocumentTransactionStage::AfterReplace,
                    index,
                    action == ProjectDocumentTransactionHookAction::Interrupt
                        ? "commit_interrupted"
                        : "post_replace_failed",
                    operationError.empty()
                        ? "Project transaction commit was interrupted."
                        : std::move(operationError),
                    action == ProjectDocumentTransactionHookAction::Interrupt);
            }

            if (!ValidateDocumentAtPath(
                    document,
                    document.destination,
                    operationError))
            {
                return failDuringCommit(
                    ProjectDocumentTransactionStage::Validate,
                    index,
                    "post_replace_validation_failed",
                    operationError.empty()
                        ? "A committed project document failed validation."
                        : std::move(operationError),
                    false);
            }
            AddEvent(
                result,
                ProjectDocumentTransactionStage::Validate,
                index,
                document.destination,
                "committed_document_validated");

            journal.commitCount = index + 1;
            journal.activeIndex = ProjectTransactionNoDocument;
            if (!PersistJournal(journal, journalPath, operationError))
            {
                // The persisted active index still identifies this replacement,
                // so recovery can conservatively restore it.
                journal.activeIndex = index;
                return failDuringCommit(
                    ProjectDocumentTransactionStage::Journal,
                    index,
                    "journal_write_failed",
                    std::move(operationError),
                    false);
            }
        }

        journal.phase = JournalPhase::Committed;
        journal.activeIndex = ProjectTransactionNoDocument;
        if (!PersistJournal(journal, journalPath, operationError))
        {
            // All destinations are replaced, but the durable state still says
            // committing. Conservatively require recovery rather than claiming
            // a completed transaction without a committed journal marker.
            result.recoveryRequired = true;
            return FailureResult(
                std::move(result),
                ProjectDocumentTransactionStage::Journal,
                ProjectTransactionNoDocument,
                "commit_marker_failed",
                std::move(operationError));
        }

        result.success = true;
        result.committed = true;
        result.stage = ProjectDocumentTransactionStage::Complete;
        result.code = "committed";
        result.message = "Project document transaction committed.";

        std::string cleanupError;
        if (!CleanupJournalArtifacts(
                journal,
                journalPath,
                options,
                result,
                cleanupError))
        {
            result.recoveryRequired = true;
            result.code = "committed_cleanup_pending";
            result.message = "Project documents committed, but transaction "
                "cleanup remains: " + cleanupError;
        }
        return result;
    }

    ProjectDocumentTransactionResult ProjectDocumentTransaction::Recover(
        const std::string& journalPathText,
        ProjectDocumentTransactionOptions options) const
    {
        ProjectDocumentTransactionResult result;
        result.stage = ProjectDocumentTransactionStage::Recover;
        if (journalPathText.empty())
        {
            return FailureResult(
                std::move(result),
                ProjectDocumentTransactionStage::Recover,
                ProjectTransactionNoDocument,
                "missing_journal",
                "A project transaction journal path is required.");
        }

        std::error_code pathError;
        const fs::path journalPath = NormalizedAbsolute(
            fs::u8path(journalPathText),
            pathError);
        result.journalPath = journalPath.generic_u8string();
        if (pathError)
        {
            return FailureResult(
                std::move(result),
                ProjectDocumentTransactionStage::Recover,
                ProjectTransactionNoDocument,
                "invalid_journal_path",
                "Could not resolve the transaction journal path: " +
                    pathError.message());
        }

        fs::path allowedRoot;
        std::string allowedRootError;
        if (!ResolveAllowedRoot(options, allowedRoot, allowedRootError))
        {
            return FailureResult(
                std::move(result),
                ProjectDocumentTransactionStage::Recover,
                ProjectTransactionNoDocument,
                "invalid_allowed_root",
                std::move(allowedRootError));
        }
        if (!allowedRoot.empty() &&
            !IsPathWithinRoot(allowedRoot, journalPath))
        {
            return FailureResult(
                std::move(result),
                ProjectDocumentTransactionStage::Recover,
                ProjectTransactionNoDocument,
                "journal_outside_allowed_root",
                "The project transaction journal escapes its project root: " +
                    journalPath.generic_u8string());
        }

        Journal journal;
        std::string operationError;
        if (!ReadJournal(journalPath, journal, operationError))
        {
            return FailureResult(
                std::move(result),
                ProjectDocumentTransactionStage::Recover,
                ProjectTransactionNoDocument,
                "invalid_journal",
                std::move(operationError));
        }
        if (!allowedRoot.empty())
        {
            for (std::size_t index = 0;
                index < journal.documents.size(); ++index)
            {
                const auto& document = journal.documents[index];
                const fs::path paths[] = {
                    document.destination,
                    document.staged,
                    document.backup,
                    document.restore,
                };
                for (const auto& path : paths)
                {
                    if (!IsPathWithinRoot(allowedRoot, path))
                    {
                        return FailureResult(
                            std::move(result),
                            ProjectDocumentTransactionStage::Recover,
                            index,
                            "journal_document_outside_allowed_root",
                            "A recovered transaction document escapes its project root: " +
                                path.generic_u8string());
                    }
                }
            }
        }
        result.transactionId = journal.transactionId;

        const auto action = InvokeHook(
            options,
            ProjectDocumentTransactionStage::Recover,
            ProjectTransactionNoDocument,
            journalPath,
            operationError);
        if (action != ProjectDocumentTransactionHookAction::Continue)
        {
            result.recoveryRequired = true;
            return FailureResult(
                std::move(result),
                ProjectDocumentTransactionStage::Recover,
                ProjectTransactionNoDocument,
                action == ProjectDocumentTransactionHookAction::Interrupt
                    ? "recovery_interrupted"
                    : "recovery_failed",
                operationError.empty()
                    ? "Project transaction recovery was interrupted."
                    : std::move(operationError));
        }

        if (journal.phase == JournalPhase::Committing ||
            journal.phase == JournalPhase::RollingBack)
        {
            if (!RollbackJournal(
                    journal,
                    journalPath,
                    options,
                    result,
                    operationError))
            {
                result.recoveryRequired = true;
                return FailureResult(
                    std::move(result),
                    ProjectDocumentTransactionStage::Rollback,
                    ProjectTransactionNoDocument,
                    "recovery_rollback_failed",
                    std::move(operationError));
            }
            result.code = "recovered_rolled_back";
            result.message = "Interrupted project transaction rolled back.";
        }
        else if (journal.phase == JournalPhase::Committed)
        {
            result.committed = true;
            result.code = "recovered_committed";
            result.message = "Committed project transaction cleanup completed.";
        }
        else if (journal.phase == JournalPhase::RolledBack)
        {
            result.rolledBack = true;
            result.code = "recovered_rolled_back";
            result.message = "Rolled-back project transaction cleanup completed.";
        }
        else
        {
            result.code = "recovered_precommit";
            result.message = "Interrupted pre-commit transaction artifacts removed.";
        }

        if (!CleanupJournalArtifacts(
                journal,
                journalPath,
                options,
                result,
                operationError))
        {
            result.recoveryRequired = true;
            return FailureResult(
                std::move(result),
                ProjectDocumentTransactionStage::Cleanup,
                ProjectTransactionNoDocument,
                "recovery_cleanup_failed",
                std::move(operationError));
        }

        result.success = true;
        result.stage = ProjectDocumentTransactionStage::Complete;
        result.recoveryRequired = false;
        return result;
    }
}
