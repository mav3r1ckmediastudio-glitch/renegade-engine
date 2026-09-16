#include "renegade/bridge/CreatorImportDiagnostics.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <type_traits>

int main()
{
    using namespace renegade::bridge;
    static_assert(std::is_trivially_copyable_v<CreatorImportDiagnostics>);

    CreatorImportDiagnostics result;
    assert(result.attemptId == 0);
    assert(result.stage == CreatorImportStage::InputValidation);
    assert(!result.sourceRetained);
    assert(!result.preparedSceneReady);
    assert(!result.recipeApplied);
    assert(!result.wisceneWritten);
    assert(!result.packageSerialized);
    assert(!result.transactionCommitted);
    assert(!result.browserRevealed);

    const std::uint64_t first = NextCreatorImportAttemptId();
    const std::uint64_t second = NextCreatorImportAttemptId();
    assert(first != 0 && second == first + 1);

    // Serialization, transaction commit and browser reveal are distinct
    // milestones. A successful earlier stage must never imply a later one.
    result.attemptId = first;
    result.stage = CreatorImportStage::PackageSerialization;
    result.packageSerialized = true;
    assert(!result.transactionCommitted && !result.browserRevealed);
    result.stage = CreatorImportStage::Committed;
    result.transactionCommitted = true;
    assert(!result.browserRevealed);
    result.stage = CreatorImportStage::BrowserReveal;
    assert(std::strcmp(CreatorImportStageName(result.stage),
        "Asset Browser reveal") == 0);
    result.browserRevealed = true;
    result.stage = CreatorImportStage::Complete;
    assert(result.packageSerialized && result.transactionCommitted &&
        result.browserRevealed);

    const CreatorImportStage stages[] = {
        CreatorImportStage::InputValidation,
        CreatorImportStage::SourceRetention,
        CreatorImportStage::RecipeValidation,
        CreatorImportStage::RegistryValidation,
        CreatorImportStage::ScenePreparation,
        CreatorImportStage::RecipeApplication,
        CreatorImportStage::WisceneWrite,
        CreatorImportStage::PackageSerialization,
        CreatorImportStage::RegistryPreparation,
        CreatorImportStage::AtomicCommit,
        CreatorImportStage::Committed,
        CreatorImportStage::BrowserReveal,
        CreatorImportStage::Complete,
    };
    for (const CreatorImportStage stage : stages)
    {
        const char* name = CreatorImportStageName(stage);
        assert(name != nullptr && *name != '\0');
        assert(std::strcmp(name, "unknown stage") != 0);
    }

    std::cout << "Importer A1 diagnostic contract: PASS\n";
    return 0;
}
