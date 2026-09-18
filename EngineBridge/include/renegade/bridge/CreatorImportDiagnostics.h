#pragma once

#include <atomic>
#include <cstdint>

namespace renegade::bridge
{
    // The stage records where the attempt was when it returned, not a claim
    // that an earlier visible progress label was the failing operation.
    enum class CreatorImportStage
    {
        InputValidation,
        SourceRetention,
        RecipeValidation,
        RegistryValidation,
        ScenePreparation,
        RecipeApplication,
        WisceneWrite,
        PackageSerialization,
        RegistryPreparation,
        AtomicCommit,
        Committed,
        BrowserReveal,
        Complete,
    };

    inline const char* CreatorImportStageName(const CreatorImportStage stage) noexcept
    {
        switch (stage)
        {
        case CreatorImportStage::InputValidation: return "input validation";
        case CreatorImportStage::SourceRetention: return "source retention";
        case CreatorImportStage::RecipeValidation: return "recipe validation";
        case CreatorImportStage::RegistryValidation: return "registry validation";
        case CreatorImportStage::ScenePreparation: return "scene preparation / conversion";
        case CreatorImportStage::RecipeApplication: return "recipe application";
        case CreatorImportStage::WisceneWrite: return "WISCENE write";
        case CreatorImportStage::PackageSerialization: return "RASSET serialization";
        case CreatorImportStage::RegistryPreparation: return "registry / metadata preparation";
        case CreatorImportStage::AtomicCommit: return "atomic commit";
        case CreatorImportStage::Committed: return "product committed (browser not yet verified)";
        case CreatorImportStage::BrowserReveal: return "Asset Browser reveal";
        case CreatorImportStage::Complete: return "import complete";
        }
        return "unknown stage";
    }

    // Facts are set only after the corresponding operation actually succeeds.
    // In particular, packageSerialized does not imply transactionCommitted;
    // transactionCommitted does not imply browserRevealed.
    struct CreatorImportDiagnostics
    {
        std::uint64_t attemptId = 0;
        CreatorImportStage stage = CreatorImportStage::InputValidation;
        bool sourceRetained = false;
        bool preparedSceneReady = false;
        bool recipeApplied = false;
        bool wisceneWritten = false;
        bool packageSerialized = false;
        bool transactionCommitted = false;
        bool browserRevealed = false;
    };

    inline std::uint64_t NextCreatorImportAttemptId() noexcept
    {
        static std::atomic<std::uint64_t> next{1};
        return next.fetch_add(1, std::memory_order_relaxed);
    }
}
