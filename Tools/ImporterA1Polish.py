#!/usr/bin/env python3
"""Tighten milestone accuracy after the fail-closed A1 patch is applied."""
from pathlib import Path


def once(path: str, before: str, after: str) -> None:
    source = Path(path).read_text(encoding="utf-8")
    if source.count(before) != 1:
        raise RuntimeError(f"{path}: expected one polish anchor, found {source.count(before)}")
    Path(path).write_text(source.replace(before, after, 1), encoding="utf-8", newline="\n")


bridge = "EngineBridge/src/ReusableAssetService.cpp"
once(bridge,
    '            cleanupTemporary();\n            return result;\n        }\n        if (!BuildModelMetadata(result.import, *preparedScene,',
    '            cleanupTemporary();\n            return result;\n        }\n        result.diagnostics.wisceneWritten = true;\n        result.diagnostics.stage = CreatorImportStage::RegistryPreparation;\n        if (!BuildModelMetadata(result.import, *preparedScene,')
once(bridge,
    '        result.diagnostics.wisceneWritten = true;\n        result.diagnostics.stage = CreatorImportStage::PackageSerialization;\n        ReusableModelAssetDocument assetDocument;',
    '        result.diagnostics.stage = CreatorImportStage::PackageSerialization;\n        ReusableModelAssetDocument assetDocument;')

studio = "Studio/src/StudioApplication.cpp"
once(studio,
    '<< "\\nSource retained: " << yesNo(details.sourceRetained)',
    '<< "\\nSource snapshot staged (cleaned up on failure): "\n                                << yesNo(details.sourceRetained)')
once(studio,
    '                        std::string browserError;\n                        if (!studioChrome_.RevealCreatorAsset(',
    '                        std::string browserError;\n                        state->imported.diagnostics.stage =\n                            bridge::CreatorImportStage::BrowserReveal;\n                        if (!studioChrome_.RevealCreatorAsset(')
print("A1 milestone meanings and browser-reveal stage: PASS")
