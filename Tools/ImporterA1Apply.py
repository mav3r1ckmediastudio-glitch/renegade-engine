#!/usr/bin/env python3
"""Apply the bounded A1 diagnostics changes in a GitHub Actions checkout.

This is intentionally fail-closed. Every source file must have the exact audited
Git blob identity and every patch anchor must occur exactly once. Nothing is
written until all patches have been validated. No private fixture is read.
"""

from pathlib import Path
import subprocess

EXPECTED = {
    "EngineBridge/include/renegade/bridge/ReusableAssetService.h": "e91c4ace813ce5bb74f5c4e9a753d71d42757fd6",
    "EngineBridge/include/renegade/bridge/CreatorAssetWorkflowService.h": "724609bdf873c5e7a77b113842a4f189d8d539fe",
    "EngineBridge/src/ReusableAssetService.cpp": "b1d6f1521031df2a93b662f9a800c2d2d047066b",
    "EngineBridge/src/CreatorAssetWorkflowService.cpp": "36e1d8ba04b8416d90025c8b9af164f40d3f9801",
    "Studio/src/StudioApplication.cpp": "caf762915e323b9813e391720aa4f01ed498c788",
}


def replace_once(text: str, old: str, new: str, path: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected exactly one anchor, found {count}: {old[:100]!r}")
    return text.replace(old, new, 1)


def edit(path: str, replacements: list[tuple[str, str]]) -> str:
    text = Path(path).read_text(encoding="utf-8")
    for old, new in replacements:
        text = replace_once(text, old, new, path)
    return text


def main() -> None:
    for path, expected in EXPECTED.items():
        current = subprocess.check_output(
            ["git", "rev-parse", f"HEAD:{path}"], text=True).strip()
        if current != expected:
            raise RuntimeError(f"Refusing to patch changed source {path}: {current}")
    updated = {}

    path = "EngineBridge/include/renegade/bridge/ReusableAssetService.h"
    updated[path] = edit(path, [
        ('#include "renegade/bridge/ImportService.h"\n',
         '#include "renegade/bridge/ImportService.h"\n#include "renegade/bridge/CreatorImportDiagnostics.h"\n'),
        ('        std::vector<std::uint8_t> thumbnailPngBytes;\n    };\n\n    struct ReusableModelImportOptions',
         '        std::vector<std::uint8_t> thumbnailPngBytes;\n        std::uint64_t diagnosticAttemptId = 0;\n    };\n\n    struct ReusableModelImportOptions'),
        ('    struct ReusableModelImportResult\n    {\n        bool succeeded = false;\n',
         '    struct ReusableModelImportResult\n    {\n        bool succeeded = false;\n        CreatorImportDiagnostics diagnostics;\n'),
    ])

    path = "EngineBridge/include/renegade/bridge/CreatorAssetWorkflowService.h"
    updated[path] = edit(path, [
        ('    struct CreatorModelImportResult\n    {\n        bool succeeded = false;\n',
         '    struct CreatorModelImportResult\n    {\n        bool succeeded = false;\n        CreatorImportDiagnostics diagnostics;\n'),
        ('        const std::string& thumbnailSourcePath = {},\n        PreparedReusableModelPlacement* preparedPlacement = nullptr) const;',
         '        const std::string& thumbnailSourcePath = {},\n        PreparedReusableModelPlacement* preparedPlacement = nullptr,\n        std::uint64_t diagnosticAttemptId = 0) const;'),
    ])

    path = "EngineBridge/src/ReusableAssetService.cpp"
    updated[path] = edit(path, [
        ('        ReusableModelImportResult result;\n        if (preparedPlacement != nullptr)',
         '        ReusableModelImportResult result;\n        result.diagnostics.attemptId = request.diagnosticAttemptId != 0\n            ? request.diagnosticAttemptId : NextCreatorImportAttemptId();\n        if (preparedPlacement != nullptr)'),
        ('        std::string recipeError;\n        const std::string recipeJson = BuildRecipeJson(format, request.settingsJson, recipeError);',
         '        result.diagnostics.stage = CreatorImportStage::RecipeValidation;\n        std::string recipeError;\n        const std::string recipeJson = BuildRecipeJson(format, request.settingsJson, recipeError);'),
        ('        AssetRegistry registry;\n        if (!ReadRegistryOrCreate(root, request.projectId, registry, result.error))',
         '        result.diagnostics.stage = CreatorImportStage::RegistryValidation;\n        AssetRegistry registry;\n        if (!ReadRegistryOrCreate(root, request.projectId, registry, result.error))'),
        ('        const fs::path importDirectory = root / "Intermediate" / "Imports";',
         '        result.diagnostics.stage = CreatorImportStage::ScenePreparation;\n        const fs::path importDirectory = root / "Intermediate" / "Imports";'),
        ('        CreatorModelImportRecipe creatorRecipe;\n        if (!ParseCreatorModelImportOptions(request.settingsJson, creatorRecipe, result.error) ||',
         '        result.diagnostics.preparedSceneReady = true;\n        result.diagnostics.stage = CreatorImportStage::RecipeApplication;\n        CreatorModelImportRecipe creatorRecipe;\n        if (!ParseCreatorModelImportOptions(request.settingsJson, creatorRecipe, result.error) ||'),
        ('        result.import = importer.SavePreparedModelAsset(prepared);',
         '        result.diagnostics.recipeApplied = true;\n        result.diagnostics.stage = CreatorImportStage::WisceneWrite;\n        result.import = importer.SavePreparedModelAsset(prepared);'),
        ('        ReusableModelAssetDocument assetDocument;\n        assetDocument.manifest.projectId = request.projectId;',
         '        result.diagnostics.wisceneWritten = true;\n        result.diagnostics.stage = CreatorImportStage::PackageSerialization;\n        ReusableModelAssetDocument assetDocument;\n        assetDocument.manifest.projectId = request.projectId;'),
        ('        if (!SerializeReusableModelAssetDocument(assetDocument, assetBytes, result.error))\n            return result;\n        const std::string assetHash = HashBytes(assetBytes);',
         '        if (!SerializeReusableModelAssetDocument(assetDocument, assetBytes, result.error))\n            return result;\n        result.diagnostics.packageSerialized = true;\n        const std::string assetHash = HashBytes(assetBytes);'),
        ('        ReusableModelManagedProjection projection;\n        projection.projectId = request.projectId;',
         '        result.diagnostics.stage = CreatorImportStage::RegistryPreparation;\n        ReusableModelManagedProjection projection;\n        projection.projectId = request.projectId;'),
        ('        ProjectDocumentTransactionOptions transactionOptions;\n        transactionOptions.transactionId = std::move(options.transactionId);',
         '        result.diagnostics.stage = CreatorImportStage::AtomicCommit;\n        ProjectDocumentTransactionOptions transactionOptions;\n        transactionOptions.transactionId = std::move(options.transactionId);'),
        ('        if (preparedPlacement != nullptr)\n            *preparedPlacement = std::move(pendingPlacement);\n        result.succeeded = true;\n        result.error.clear();',
         '        if (preparedPlacement != nullptr)\n            *preparedPlacement = std::move(pendingPlacement);\n        result.diagnostics.transactionCommitted = true;\n        result.diagnostics.stage = CreatorImportStage::Committed;\n        result.succeeded = true;\n        result.error.clear();'),
    ])

    path = "EngineBridge/src/CreatorAssetWorkflowService.cpp"
    updated[path] = edit(path, [
        ('        const std::string& thumbnailSourcePath,\n        PreparedReusableModelPlacement* preparedPlacement) const\n    {\n        CreatorModelImportResult result;',
         '        const std::string& thumbnailSourcePath,\n        PreparedReusableModelPlacement* preparedPlacement,\n        const std::uint64_t diagnosticAttemptId) const\n    {\n        CreatorModelImportResult result;\n        result.diagnostics.attemptId = diagnosticAttemptId != 0\n            ? diagnosticAttemptId : NextCreatorImportAttemptId();'),
        ('        fs::create_directories(snapshotDirectory, ec);\n        if (ec)',
         '        result.diagnostics.stage = CreatorImportStage::SourceRetention;\n        fs::create_directories(snapshotDirectory, ec);\n        if (ec)'),
        ('        if (!CopyGltfExternalFiles(source, snapshotDirectory, result.error))\n        {\n            cleanupSnapshot();\n            return result;\n        }\n\n        result.stagedSourceProjectRelativePath',
         '        if (!CopyGltfExternalFiles(source, snapshotDirectory, result.error))\n        {\n            cleanupSnapshot();\n            return result;\n        }\n        result.diagnostics.sourceRetained = true;\n\n        result.stagedSourceProjectRelativePath'),
        ('        request.thumbnailPngBytes = std::move(thumbnailPngBytes);\n        result.asset = ReusableAssetService().ImportModelAsset(',
         '        request.thumbnailPngBytes = std::move(thumbnailPngBytes);\n        request.diagnosticAttemptId = result.diagnostics.attemptId;\n        result.asset = ReusableAssetService().ImportModelAsset('),
        ('            request, {}, std::move(preparedModel), preparedPlacement);\n        if (!result.asset.succeeded)',
         '            request, {}, std::move(preparedModel), preparedPlacement);\n        result.diagnostics = result.asset.diagnostics;\n        result.diagnostics.sourceRetained = true;\n        if (!result.asset.succeeded)'),
    ])

    path = "Studio/src/StudioApplication.cpp"
    updated[path] = edit(path, [
        ('        struct GovernedCommitState\n        {\n            std::string projectRoot;',
         '        struct GovernedCommitState\n        {\n            std::uint64_t attemptId = bridge::NextCreatorImportAttemptId();\n            std::string failureStage = "material preparation";\n            std::string projectRoot;'),
        ('                if (state->imported.error.empty())\n                {\n                    wi::eventhandler::Subscribe_Once(',
         '                if (state->imported.error.empty())\n                {\n                    state->failureStage = "governed import";\n                    wi::eventhandler::Subscribe_Once('),
        ('                                    "PROCESSING // WRITING RASSET PACKAGE");\n                                studioChrome_.SetStatusText(\n                                    "IMPORT MODEL // PROCESSING // WRITING RASSET PACKAGE");',
         '                                    "PROCESSING // GOVERNED IMPORT");\n                                studioChrome_.SetStatusText(\n                                    "IMPORT MODEL // PROCESSING // GOVERNED IMPORT");'),
        ('                &state->warmedPlacement);\n                    state->packageSeconds',
         '                &state->warmedPlacement,\n                state->attemptId);\n                    state->packageSeconds'),
        ('                        if (!state->imported.succeeded)\n                        {\n                            studioChrome_.SetStatusText("IMPORT MODEL // COMMIT FAILED");\n                            ShowStudioMessageBox(\n                                "The preview was discarded safely, but the governed asset could not be committed.\\n\\nReason: " +\n                                    state->imported.error,\n                                "Import Model");\n                            return;\n                        }',
         '                        if (!state->imported.succeeded)\n                        {\n                            const auto& details = state->imported.diagnostics;\n                            const std::string failedStage = details.attemptId != 0\n                                ? bridge::CreatorImportStageName(details.stage)\n                                : state->failureStage;\n                            const auto yesNo = [](const bool value)\n                            {\n                                return value ? "yes" : "no";\n                            };\n                            studioChrome_.SetStatusText(\n                                "IMPORT MODEL // FAILED // " + failedStage +\n                                " // OPEN DIAGNOSTICS");\n                            std::ostringstream report;\n                            report << "Import attempt " << state->attemptId\n                                << " failed during " << failedStage\n                                << ".\\n\\nReason: "\n                                << (state->imported.error.empty()\n                                    ? "No detailed error was returned."\n                                    : state->imported.error)\n                                << "\\n\\nRequested folder: " << state->destinationFolder\n                                << "\\nSource retained: " << yesNo(details.sourceRetained)\n                                << "\\nPrepared scene: " << yesNo(details.preparedSceneReady)\n                                << "\\nRecipe applied: " << yesNo(details.recipeApplied)\n                                << "\\nWISCENE written: " << yesNo(details.wisceneWritten)\n                                << "\\nRASSET serialized: " << yesNo(details.packageSerialized)\n                                << "\\nAtomic transaction committed: "\n                                << yesNo(details.transactionCommitted)\n                                << "\\nAsset Browser revealed: no";\n                            if (!state->imported.asset.assetId.empty())\n                                report << "\\nProduct ID: " << state->imported.asset.assetId;\n                            if (!state->imported.assetProjectRelativePath.empty())\n                                report << "\\nProduct: "\n                                    << state->imported.assetProjectRelativePath;\n                            ShowStudioMessageBox(report.str(), "Import Model");\n                            return;\n                        }'),
        ('                            ShowStudioMessageBox(\n                                "The governed asset was committed, but Studio could not verify it in the Asset Browser. Do not import it again.\\n\\nAsset: " +',
         '                            ShowStudioMessageBox(\n                                "Import attempt " + std::to_string(state->attemptId) +\n                                    " failed during Asset Browser reveal. The governed asset was committed; do not import it again.\\n\\nAsset: " +'),
        ('                        std::ostringstream completed;\n                        completed << std::fixed',
         '                        state->imported.diagnostics.browserRevealed = true;\n                        state->imported.diagnostics.stage = bridge::CreatorImportStage::Complete;\n                        std::ostringstream completed;\n                        completed << std::fixed'),
        ('        wi::backlog::post(\n            caption + " // " + message,\n            wi::backlog::LogLevel::Error);\n        const std::size_t firstLine',
         '        wi::backlog::post(\n            caption + " // " + message,\n            wi::backlog::LogLevel::Error);\n        // The native, non-modal Backlog is the full-detail diagnostics view.\n        // The status strip can only display a short first line.\n        if (caption == "Import Model" && !wi::backlog::isActive())\n            wi::backlog::Toggle();\n        const std::size_t firstLine'),
    ])

    # Source contracts: fail before writing if a stage was omitted.
    checks = {
        "EngineBridge/src/ReusableAssetService.cpp": [
            "CreatorImportStage::RecipeValidation", "CreatorImportStage::RegistryValidation",
            "CreatorImportStage::ScenePreparation", "CreatorImportStage::RecipeApplication",
            "CreatorImportStage::WisceneWrite", "CreatorImportStage::PackageSerialization",
            "CreatorImportStage::RegistryPreparation", "CreatorImportStage::AtomicCommit",
            "CreatorImportStage::Committed", "diagnostics.transactionCommitted = true",
        ],
        "Studio/src/StudioApplication.cpp": [
            'CreatorImportStageName(details.stage)', 'wi::backlog::Toggle()',
            'failed during Asset Browser reveal', 'Asset Browser revealed: no',
        ],
    }
    for path, needles in checks.items():
        for needle in needles:
            if needle not in updated[path]:
                raise RuntimeError(f"{path}: missing diagnostic contract {needle}")

    for path, content in updated.items():
        Path(path).write_text(content, encoding="utf-8", newline="\n")
        print(f"Patched {path}")
    print("A1 source anchors and stage contracts: PASS; native compilation not attempted")


if __name__ == "__main__":
    main()
