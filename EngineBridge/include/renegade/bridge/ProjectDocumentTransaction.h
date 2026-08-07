#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace renegade::bridge
{
    inline constexpr std::size_t ProjectTransactionNoDocument =
        static_cast<std::size_t>(-1);

    enum class ProjectDocumentTransactionStage : std::uint8_t
    {
        None,
        Prepare,
        StageWrite,
        Validate,
        Backup,
        Journal,
        Replace,
        AfterReplace,
        Rollback,
        Cleanup,
        Recover,
        Complete,
    };

    enum class ProjectDocumentTransactionHookAction : std::uint8_t
    {
        Continue,
        Fail,
        Interrupt,
    };

    using ProjectDocumentValidator = std::function<bool(
        const std::string& stagedPath,
        std::string& error)>;

    using ProjectDocumentTransactionHook = std::function<
        ProjectDocumentTransactionHookAction(
            ProjectDocumentTransactionStage stage,
            std::size_t documentIndex,
            const std::string& path,
            std::string& error)>;

    struct ProjectDocumentWrite
    {
        std::string destinationPath;
        std::vector<std::uint8_t> content;
        ProjectDocumentValidator validator;
    };

    struct ProjectDocumentTransactionOptions
    {
        // Optional deterministic token for tests/evidence. Production callers
        // normally leave this empty and receive a generated token.
        std::string transactionId;

        // Journal files live here. Staged and backup files always live beside
        // their destination so the final replacement remains same-volume.
        std::string journalDirectory;

        // Optional containment boundary. When set, every destination, journal
        // and recovery artifact must remain inside this absolute project root.
        std::string allowedRoot;

        // Optional fault/interrupt seam used by automated failure testing.
        ProjectDocumentTransactionHook operationHook;
    };

    struct ProjectDocumentTransactionEvent
    {
        ProjectDocumentTransactionStage stage =
            ProjectDocumentTransactionStage::None;
        std::size_t documentIndex = ProjectTransactionNoDocument;
        std::string path;
        std::string code;
    };

    struct ProjectDocumentTransactionResult
    {
        bool success = false;
        bool committed = false;
        bool rolledBack = false;
        bool recoveryRequired = false;
        bool noChanges = false;

        ProjectDocumentTransactionStage stage =
            ProjectDocumentTransactionStage::None;
        std::size_t documentIndex = ProjectTransactionNoDocument;

        std::string transactionId;
        std::string journalPath;
        std::string code;
        std::string message;
        std::vector<ProjectDocumentTransactionEvent> events;
    };

    // UI-free, project-agnostic multi-document disk transaction.
    //
    // Execute() writes and validates same-directory staged files, protects all
    // existing destinations, then replaces destinations in deterministic path
    // order. Any commit failure rolls earlier replacements back. A durable
    // journal permits a later Recover() call to finish rollback or cleanup after
    // interruption.
    class ProjectDocumentTransaction
    {
    public:
        [[nodiscard]] ProjectDocumentTransactionResult Execute(
            std::vector<ProjectDocumentWrite> documents,
            ProjectDocumentTransactionOptions options = {}) const;

        [[nodiscard]] ProjectDocumentTransactionResult Recover(
            const std::string& journalPath,
            ProjectDocumentTransactionOptions options = {}) const;
    };
}
