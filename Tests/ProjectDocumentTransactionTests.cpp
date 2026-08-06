#include "renegade/bridge/ProjectDocumentTransaction.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using renegade::bridge::ProjectDocumentTransaction;
    using renegade::bridge::ProjectDocumentTransactionHookAction;
    using renegade::bridge::ProjectDocumentTransactionOptions;
    using renegade::bridge::ProjectDocumentTransactionStage;
    using renegade::bridge::ProjectDocumentWrite;

    int failures = 0;

    void Check(const bool condition, const char* message)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    std::vector<std::uint8_t> Bytes(const std::string& text)
    {
        return std::vector<std::uint8_t>(text.begin(), text.end());
    }

    void WriteText(const fs::path& path, const std::string& text)
    {
        fs::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    std::string ReadText(const fs::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return std::string(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
    }

    bool HasTransactionArtifacts(const fs::path& root)
    {
        std::error_code error;
        for (fs::recursive_directory_iterator iterator(root, error);
            !error && iterator != fs::recursive_directory_iterator();
            iterator.increment(error))
        {
            const std::string name = iterator->path().filename().generic_u8string();
            if (name.find(".renegade-stage-") != std::string::npos ||
                name.find(".renegade-backup-") != std::string::npos ||
                name.find(".renegade-restore-") != std::string::npos ||
                name.find(".renegade-transaction-") != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }

    ProjectDocumentTransactionOptions Options(
        const fs::path& root,
        const std::string& id)
    {
        ProjectDocumentTransactionOptions options;
        options.transactionId = id;
        options.journalDirectory = (root / "Saved/Transactions").generic_u8string();
        return options;
    }

    ProjectDocumentWrite TextDocument(
        const fs::path& path,
        const std::string& content)
    {
        ProjectDocumentWrite document;
        document.destinationPath = path.generic_u8string();
        document.content = Bytes(content);
        document.validator = [](const std::string& stagedPath, std::string& error)
        {
            std::ifstream stream(fs::u8path(stagedPath), std::ios::binary);
            const std::string text{
                std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>()};
            if (!stream && !stream.eof())
            {
                error = "validator could not read staged file";
                return false;
            }
            if (text.empty())
            {
                error = "validator rejected empty content";
                return false;
            }
            error.clear();
            return true;
        };
        return document;
    }

    void TestSuccessfulDeterministicCommit(const fs::path& root)
    {
        const fs::path a = root / "Content/A.document";
        const fs::path b = root / "Content/B.document";
        WriteText(a, "old-a");
        WriteText(b, "old-b");

        std::vector<std::string> replacementOrder;
        auto options = Options(root, "success");
        options.operationHook = [&replacementOrder](
            const ProjectDocumentTransactionStage stage,
            const std::size_t,
            const std::string& path,
            std::string&)
        {
            if (stage == ProjectDocumentTransactionStage::Replace)
            {
                replacementOrder.push_back(path);
            }
            return ProjectDocumentTransactionHookAction::Continue;
        };

        ProjectDocumentTransaction transaction;
        auto result = transaction.Execute(
            {TextDocument(b, "new-b"), TextDocument(a, "new-a")},
            options);

        Check(result.success && result.committed,
            "successful transaction did not commit");
        Check(ReadText(a) == "new-a" && ReadText(b) == "new-b",
            "successful transaction wrote incorrect content");
        Check(replacementOrder.size() == 2 &&
                fs::u8path(replacementOrder[0]).filename() == a.filename() &&
                fs::u8path(replacementOrder[1]).filename() == b.filename(),
            "destinations were not replaced in deterministic path order");
        Check(!HasTransactionArtifacts(root),
            "successful transaction left recovery artifacts");
    }

    void TestNoOp(const fs::path& root)
    {
        const fs::path target = root / "NoOp.document";
        WriteText(target, "same");
        bool validatorCalled = false;
        auto document = TextDocument(target, "same");
        document.validator = [&validatorCalled](const std::string&, std::string&)
        {
            validatorCalled = true;
            return true;
        };

        ProjectDocumentTransaction transaction;
        auto result = transaction.Execute(
            {std::move(document)},
            Options(root, "noop"));

        Check(result.success && result.committed && result.noChanges,
            "no-op transaction was not reported as a successful no-op");
        Check(validatorCalled, "no-op destination was not validated");
        Check(ReadText(target) == "same", "no-op transaction changed content");
        Check(!HasTransactionArtifacts(root),
            "no-op transaction created recovery artifacts");
    }


    void TestPreparationFailure(const fs::path& root)
    {
        const fs::path target = root / "Preparation.document";
        WriteText(target, "old");
        auto options = Options(root, "preparation-failure");
        options.operationHook = [](const ProjectDocumentTransactionStage stage,
                                   const std::size_t,
                                   const std::string&,
                                   std::string& error)
        {
            if (stage == ProjectDocumentTransactionStage::Prepare)
            {
                error = "forced preparation failure";
                return ProjectDocumentTransactionHookAction::Fail;
            }
            return ProjectDocumentTransactionHookAction::Continue;
        };

        ProjectDocumentTransaction transaction;
        auto result = transaction.Execute(
            {TextDocument(target, "new")},
            options);

        Check(!result.success &&
                result.stage == ProjectDocumentTransactionStage::Prepare,
            "preparation failure was not reported at preparation stage");
        Check(ReadText(target) == "old",
            "preparation failure changed the live destination");
        Check(!result.recoveryRequired && !HasTransactionArtifacts(root),
            "preparation failure created recovery artifacts");
    }

    void TestJournalFailure(const fs::path& root)
    {
        const fs::path target = root / "Journal.document";
        WriteText(target, "old");
        auto options = Options(root, "journal-failure");
        options.operationHook = [](const ProjectDocumentTransactionStage stage,
                                   const std::size_t,
                                   const std::string&,
                                   std::string& error)
        {
            if (stage == ProjectDocumentTransactionStage::Journal)
            {
                error = "forced journal failure";
                return ProjectDocumentTransactionHookAction::Fail;
            }
            return ProjectDocumentTransactionHookAction::Continue;
        };

        ProjectDocumentTransaction transaction;
        auto result = transaction.Execute(
            {TextDocument(target, "new")},
            options);

        Check(!result.success &&
                result.stage == ProjectDocumentTransactionStage::Journal,
            "journal failure was not reported at journal stage");
        Check(ReadText(target) == "old",
            "journal failure changed the live destination");
        Check(!result.recoveryRequired && !HasTransactionArtifacts(root),
            "journal failure created recovery artifacts");
    }

    void TestStagingFailure(const fs::path& root)
    {
        const fs::path target = root / "Staging.document";
        WriteText(target, "old");
        auto options = Options(root, "staging-failure");
        options.operationHook = [](const ProjectDocumentTransactionStage stage,
                                   const std::size_t,
                                   const std::string&,
                                   std::string& error)
        {
            if (stage == ProjectDocumentTransactionStage::StageWrite)
            {
                error = "forced staging failure";
                return ProjectDocumentTransactionHookAction::Fail;
            }
            return ProjectDocumentTransactionHookAction::Continue;
        };

        ProjectDocumentTransaction transaction;
        auto result = transaction.Execute(
            {TextDocument(target, "new")},
            options);

        Check(!result.success &&
                result.stage == ProjectDocumentTransactionStage::StageWrite,
            "staging failure was not reported at staging stage");
        Check(ReadText(target) == "old",
            "staging failure changed the live destination");
        Check(!result.recoveryRequired && !HasTransactionArtifacts(root),
            "staging failure did not clean its journal");
    }

    void TestValidationFailure(const fs::path& root)
    {
        const fs::path target = root / "Validation.document";
        WriteText(target, "old");
        auto document = TextDocument(target, "invalid");
        document.validator = [](const std::string&, std::string& error)
        {
            error = "forced validation failure";
            return false;
        };

        ProjectDocumentTransaction transaction;
        auto result = transaction.Execute(
            {std::move(document)},
            Options(root, "validation-failure"));

        Check(!result.success &&
                result.stage == ProjectDocumentTransactionStage::Validate,
            "validation failure was not reported at validation stage");
        Check(ReadText(target) == "old",
            "validation failure changed the live destination");
        Check(!result.recoveryRequired && !HasTransactionArtifacts(root),
            "validation failure did not clean pre-commit artifacts");
    }

    void TestBackupFailure(const fs::path& root)
    {
        const fs::path target = root / "Backup.document";
        WriteText(target, "old");
        auto options = Options(root, "backup-failure");
        options.operationHook = [](const ProjectDocumentTransactionStage stage,
                                   const std::size_t,
                                   const std::string&,
                                   std::string& error)
        {
            if (stage == ProjectDocumentTransactionStage::Backup)
            {
                error = "forced backup failure";
                return ProjectDocumentTransactionHookAction::Fail;
            }
            return ProjectDocumentTransactionHookAction::Continue;
        };

        ProjectDocumentTransaction transaction;
        auto result = transaction.Execute(
            {TextDocument(target, "new")},
            options);

        Check(!result.success &&
                result.stage == ProjectDocumentTransactionStage::Backup,
            "backup failure was not reported at backup stage");
        Check(ReadText(target) == "old",
            "backup failure changed the live destination");
        Check(!result.recoveryRequired && !HasTransactionArtifacts(root),
            "backup failure did not clean pre-commit artifacts");
    }

    void TestFirstReplacementFailure(const fs::path& root)
    {
        const fs::path a = root / "First/A.document";
        const fs::path b = root / "First/B.document";
        WriteText(a, "old-a");
        WriteText(b, "old-b");
        auto options = Options(root, "first-replace-failure");
        options.operationHook = [](const ProjectDocumentTransactionStage stage,
                                   const std::size_t index,
                                   const std::string&,
                                   std::string& error)
        {
            if (stage == ProjectDocumentTransactionStage::Replace && index == 0)
            {
                error = "forced first replacement failure";
                return ProjectDocumentTransactionHookAction::Fail;
            }
            return ProjectDocumentTransactionHookAction::Continue;
        };

        ProjectDocumentTransaction transaction;
        auto result = transaction.Execute(
            {TextDocument(a, "new-a"), TextDocument(b, "new-b")},
            options);

        Check(!result.success && result.rolledBack,
            "first replacement failure did not complete rollback");
        Check(ReadText(a) == "old-a" && ReadText(b) == "old-b",
            "first replacement failure did not preserve destinations");
        Check(!result.recoveryRequired && !HasTransactionArtifacts(root),
            "first replacement failure left recovery work");
    }

    void TestLaterReplacementFailure(const fs::path& root)
    {
        const fs::path a = root / "Later/A.document";
        const fs::path b = root / "Later/B.document";
        WriteText(a, "old-a");
        WriteText(b, "old-b");
        auto options = Options(root, "later-replace-failure");
        options.operationHook = [](const ProjectDocumentTransactionStage stage,
                                   const std::size_t index,
                                   const std::string&,
                                   std::string& error)
        {
            if (stage == ProjectDocumentTransactionStage::Replace && index == 1)
            {
                error = "forced later replacement failure";
                return ProjectDocumentTransactionHookAction::Fail;
            }
            return ProjectDocumentTransactionHookAction::Continue;
        };

        ProjectDocumentTransaction transaction;
        auto result = transaction.Execute(
            {TextDocument(a, "new-a"), TextDocument(b, "new-b")},
            options);

        Check(!result.success && result.rolledBack,
            "later replacement failure did not report rollback");
        Check(ReadText(a) == "old-a" && ReadText(b) == "old-b",
            "later replacement failure did not restore earlier replacement");
        Check(!result.recoveryRequired && !HasTransactionArtifacts(root),
            "later replacement failure left recovery work");
    }

    void TestRollbackFailureThenRecovery(const fs::path& root)
    {
        const fs::path a = root / "Rollback/A.document";
        const fs::path b = root / "Rollback/B.document";
        WriteText(a, "old-a");
        WriteText(b, "old-b");
        auto options = Options(root, "rollback-recovery");
        options.operationHook = [](const ProjectDocumentTransactionStage stage,
                                   const std::size_t index,
                                   const std::string&,
                                   std::string& error)
        {
            if (stage == ProjectDocumentTransactionStage::Replace && index == 1)
            {
                error = "forced later replacement failure";
                return ProjectDocumentTransactionHookAction::Fail;
            }
            if (stage == ProjectDocumentTransactionStage::Rollback && index == 0)
            {
                error = "forced rollback failure";
                return ProjectDocumentTransactionHookAction::Fail;
            }
            return ProjectDocumentTransactionHookAction::Continue;
        };

        ProjectDocumentTransaction transaction;
        auto failed = transaction.Execute(
            {TextDocument(a, "new-a"), TextDocument(b, "new-b")},
            options);

        Check(!failed.success && failed.recoveryRequired,
            "rollback failure did not require recovery");
        Check(fs::is_regular_file(fs::u8path(failed.journalPath)),
            "rollback failure did not retain its journal");

        auto recovered = transaction.Recover(failed.journalPath);
        Check(recovered.success && recovered.rolledBack,
            "retained rollback journal did not recover");
        Check(ReadText(a) == "old-a" && ReadText(b) == "old-b",
            "recovery did not restore all original destinations");
        Check(!HasTransactionArtifacts(root),
            "recovery left rollback artifacts");
    }

    void TestInterruptedCommitRecovery(const fs::path& root)
    {
        const fs::path a = root / "Interrupted/A.document";
        const fs::path b = root / "Interrupted/B.document";
        WriteText(a, "old-a");
        WriteText(b, "old-b");
        auto options = Options(root, "interrupted-commit");
        options.operationHook = [](const ProjectDocumentTransactionStage stage,
                                   const std::size_t index,
                                   const std::string&,
                                   std::string& error)
        {
            if (stage == ProjectDocumentTransactionStage::AfterReplace && index == 0)
            {
                error = "simulated process interruption";
                return ProjectDocumentTransactionHookAction::Interrupt;
            }
            return ProjectDocumentTransactionHookAction::Continue;
        };

        ProjectDocumentTransaction transaction;
        auto interrupted = transaction.Execute(
            {TextDocument(a, "new-a"), TextDocument(b, "new-b")},
            options);

        Check(!interrupted.success && interrupted.recoveryRequired,
            "simulated interruption did not retain recovery state");
        Check(ReadText(a) == "new-a" && ReadText(b) == "old-b",
            "simulated interruption did not stop at the expected boundary");

        auto recovered = transaction.Recover(interrupted.journalPath);
        Check(recovered.success && recovered.rolledBack,
            "interrupted transaction recovery failed");
        Check(ReadText(a) == "old-a" && ReadText(b) == "old-b",
            "interrupted transaction recovery did not restore originals");
        Check(!HasTransactionArtifacts(root),
            "interrupted transaction recovery left artifacts");
    }

    void TestNewDestinationRollback(const fs::path& root)
    {
        const fs::path a = root / "NewFile/A.document";
        const fs::path b = root / "NewFile/B.document";
        WriteText(a, "old-a");
        fs::create_directories(b.parent_path());
        auto options = Options(root, "new-file-rollback");
        options.operationHook = [](const ProjectDocumentTransactionStage stage,
                                   const std::size_t index,
                                   const std::string&,
                                   std::string& error)
        {
            if (stage == ProjectDocumentTransactionStage::AfterReplace && index == 1)
            {
                error = "forced failure after new file replacement";
                return ProjectDocumentTransactionHookAction::Fail;
            }
            return ProjectDocumentTransactionHookAction::Continue;
        };

        ProjectDocumentTransaction transaction;
        auto result = transaction.Execute(
            {TextDocument(a, "new-a"), TextDocument(b, "new-b")},
            options);

        Check(!result.success && result.rolledBack,
            "new destination failure did not roll back");
        Check(ReadText(a) == "old-a",
            "new destination rollback did not restore existing file");
        Check(!fs::exists(b),
            "new destination rollback did not remove newly created file");
        Check(!HasTransactionArtifacts(root),
            "new destination rollback left artifacts");
    }

    void TestPostReplaceValidationRollback(const fs::path& root)
    {
        const fs::path target = root / "PostValidate.document";
        WriteText(target, "old");

        auto options = Options(root, "post-validation");
        options.operationHook = [](
            const ProjectDocumentTransactionStage stage,
            const std::size_t,
            const std::string& path,
            std::string&)
        {
            if (stage == ProjectDocumentTransactionStage::AfterReplace)
            {
                WriteText(fs::u8path(path), "corrupt");
            }
            return ProjectDocumentTransactionHookAction::Continue;
        };

        ProjectDocumentTransaction transaction;
        auto result = transaction.Execute(
            {TextDocument(target, "new")},
            options);

        Check(!result.success && result.rolledBack &&
                result.code == "post_replace_validation_failed",
            "post-replacement corruption was not rejected and rolled back");
        Check(ReadText(target) == "old",
            "post-replacement validation failure did not restore old content");
        Check(!result.recoveryRequired && !HasTransactionArtifacts(root),
            "post-replacement validation rollback left recovery artifacts");
    }

    void TestCommittedCleanupRecovery(const fs::path& root)
    {
        const fs::path target = root / "Cleanup.document";
        WriteText(target, "old");
        auto options = Options(root, "cleanup-recovery");
        bool blocked = false;
        options.operationHook = [&blocked](
            const ProjectDocumentTransactionStage stage,
            const std::size_t,
            const std::string&,
            std::string& error)
        {
            if (stage == ProjectDocumentTransactionStage::Cleanup && !blocked)
            {
                blocked = true;
                error = "forced cleanup failure";
                return ProjectDocumentTransactionHookAction::Fail;
            }
            return ProjectDocumentTransactionHookAction::Continue;
        };

        ProjectDocumentTransaction transaction;
        auto committed = transaction.Execute(
            {TextDocument(target, "new")},
            options);

        Check(committed.success && committed.committed &&
                committed.recoveryRequired,
            "cleanup failure incorrectly invalidated committed content");
        Check(ReadText(target) == "new",
            "cleanup failure rolled back committed content");

        auto recovered = transaction.Recover(committed.journalPath);
        Check(recovered.success && recovered.committed,
            "committed journal cleanup recovery failed");
        Check(ReadText(target) == "new",
            "committed cleanup recovery changed committed content");
        Check(!HasTransactionArtifacts(root),
            "committed cleanup recovery left artifacts");
    }

    void TestAllowedRootContainment(const fs::path& root)
    {
        const fs::path project = root / "Project";
        const fs::path outside = root / "Outside.document";
        fs::create_directories(project);

        ProjectDocumentTransactionOptions options;
        options.transactionId = "allowed-root";
        options.journalDirectory =
            (project / "Intermediate/Transactions").generic_u8string();
        options.allowedRoot = project.generic_u8string();

        ProjectDocumentTransaction transaction;
        auto escaped = transaction.Execute(
            {TextDocument(outside, "forbidden")},
            options);
        Check(!escaped.success &&
                escaped.code == "destination_outside_allowed_root",
            "allowed-root boundary accepted an escaped destination");
        Check(!fs::exists(outside),
            "allowed-root rejection touched the escaped destination");

        options.transactionId = "escaped-journal";
        options.journalDirectory = (root / "ExternalJournal").generic_u8string();
        auto journalEscape = transaction.Execute(
            {TextDocument(project / "Inside.document", "inside")},
            options);
        Check(!journalEscape.success &&
                journalEscape.code == "journal_outside_allowed_root",
            "allowed-root boundary accepted an escaped journal directory");
        Check(!fs::exists(project / "Inside.document"),
            "escaped journal rejection committed project content");
    }

    void TestMalformedJournalRejected(const fs::path& root)
    {
        const fs::path target = root / "Protected.document";
        const fs::path journal = root / "bad.journal";
        WriteText(target, "protected");
        WriteText(journal, "not a renegade transaction\n");

        ProjectDocumentTransaction transaction;
        auto result = transaction.Recover(journal.generic_u8string());

        Check(!result.success && result.code == "invalid_journal",
            "malformed journal was not rejected");
        Check(ReadText(target) == "protected",
            "malformed journal recovery touched unrelated content");
    }
}

int main()
{
    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        fs::u8path("renegade-project-transaction-" + std::to_string(unique));
    fs::create_directories(root);

    TestSuccessfulDeterministicCommit(root / "01-success");
    TestNoOp(root / "02-noop");
    TestPreparationFailure(root / "03-preparation");
    TestJournalFailure(root / "04-journal");
    TestStagingFailure(root / "05-staging");
    TestValidationFailure(root / "06-validation");
    TestBackupFailure(root / "07-backup");
    TestFirstReplacementFailure(root / "08-first-replace");
    TestLaterReplacementFailure(root / "09-later-replace");
    TestRollbackFailureThenRecovery(root / "10-rollback-recovery");
    TestInterruptedCommitRecovery(root / "11-interrupted");
    TestNewDestinationRollback(root / "12-new-file");
    TestPostReplaceValidationRollback(root / "13-post-validation");
    TestCommittedCleanupRecovery(root / "14-cleanup");
    TestAllowedRootContainment(root / "15-containment");
    TestMalformedJournalRejected(root / "16-malformed");

    std::error_code ignored;
    fs::remove_all(root, ignored);

    if (failures != 0)
    {
        std::cerr << failures << " project document transaction checks failed\n";
        return 1;
    }

    std::cout << "PASS: project document transaction commit, rollback and recovery\n";
    return 0;
}
