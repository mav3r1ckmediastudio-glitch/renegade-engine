#!/usr/bin/env python3
"""Fast source contract only; it does not prove native UI or an FBX import."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]


def check(path: str, tokens: list[str]) -> None:
    source = (root / path).read_text(encoding="utf-8")
    for token in tokens:
        assert token in source, f"{path}: missing {token!r}"


check("EngineBridge/include/renegade/bridge/CreatorImportDiagnostics.h", [
    "enum class CreatorImportStage", "std::uint64_t attemptId",
    "bool transactionCommitted = false", "bool browserRevealed = false",
])
check("EngineBridge/src/CreatorAssetWorkflowService.cpp", [
    "request.diagnosticAttemptId = result.diagnostics.attemptId;",
    "result.diagnostics = result.asset.diagnostics;",
    "result.diagnostics.sourceRetained = true;",
    "result.error = result.asset.error;",
])
check("EngineBridge/src/ReusableAssetService.cpp", [
    "CreatorImportStage::RecipeValidation", "CreatorImportStage::RegistryValidation",
    "CreatorImportStage::ScenePreparation", "CreatorImportStage::RecipeApplication",
    "CreatorImportStage::WisceneWrite", "CreatorImportStage::PackageSerialization",
    "CreatorImportStage::RegistryPreparation", "CreatorImportStage::AtomicCommit",
    "result.diagnostics.transactionCommitted = true;",
    "result.diagnostics.stage = CreatorImportStage::Committed;",
])
check("Studio/src/StudioApplication.cpp", [
    "state->attemptId", "CreatorImportStageName(details.stage)",
    '"\\n\\nReason: "', "Source snapshot staged (cleaned up on failure)",
    "Atomic transaction committed:", "Asset Browser revealed: no",
    "bridge::CreatorImportStage::BrowserReveal", "wi::backlog::Toggle();",
    "!wi::backlog::isActive()",
])
check("Tests/ReusableAssetTests.cpp", [
    "outsideSource.diagnostics.stage == CreatorImportStage::InputValidation",
    "existingProduct.diagnostics.stage == CreatorImportStage::InputValidation",
    "nonCanonicalSettings.diagnostics.stage == CreatorImportStage::RecipeValidation",
    "crossProject.diagnostics.stage == CreatorImportStage::RegistryValidation",
    '"rejected imports created an authoritative product"',
])

studio = (root / "Studio/src/StudioApplication.cpp").read_text(encoding="utf-8")
start = studio.index("void StudioRenderPath::ApplyImportScaleMode(")
end = studio.index("void StudioRenderPath::DismissImportScalePanel()", start)
importer = studio[start:end]
assert "PROCESSING // WRITING RASSET PACKAGE" not in importer, "Misleading write-stage progress label returned"
assert "!state->imported.succeeded" in importer and "failed during Asset Browser reveal" in importer
assert importer.index("if (!state->imported.succeeded)") < importer.index("RevealCreatorAsset("), "Reveal cannot precede governed success"
print("Importer A1 source-contract checks: PASS (source only, no native build)")
