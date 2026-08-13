from pathlib import Path

def replace_once(path, old, new, label):
    p = Path(path)
    s = p.read_text(encoding='utf-8')
    if old not in s:
        raise SystemExit(f'{label}: marker not found in {path}')
    s = s.replace(old, new, 1)
    p.write_text(s, encoding='utf-8')

# ImportService: creator edits must refresh round-trip evidence before save.
path = 'EngineBridge/src/ImportServiceModel.cpp'
marker = '''    ImportResult ImportService::SavePreparedModelAsset(\n        PreparedModelImport& prepared) const\n    {'''
insert = '''    bool ImportService::RefreshPreparedModelEvidence(\n        PreparedModelImport& prepared,\n        std::string& error) const\n    {\n        if (!prepared.IsReady() || prepared.scene_ == nullptr)\n        {\n            error = "Prepared model import is not ready for creator recipe edits.";\n            return false;\n        }\n        prepared.result_.imported = Summarize(*prepared.scene_);\n        prepared.result_.importedEvidence = SummarizeModelEvidence(*prepared.scene_);\n        if (prepared.result_.imported.meshes == 0 ||\n            prepared.result_.imported.objects == 0)\n        {\n            error = "Creator recipe removed all reusable mesh/object content.";\n            return false;\n        }\n        error.clear();\n        return true;\n    }\n\n'''
replace_once(path, marker, insert + marker, 'refresh evidence')

# Creator workflow: pass canonical creator settings into the governed LP07 request.
path = 'EngineBridge/src/CreatorAssetWorkflowService.cpp'
replace_once(path,
'''    CreatorModelImportResult CreatorAssetWorkflowService::ImportModel(\n        const std::string& projectRoot,\n        const StableId& projectId,\n        const std::string& externalSourcePath) const\n    {''',
'''    CreatorModelImportResult CreatorAssetWorkflowService::ImportModel(\n        const std::string& projectRoot,\n        const StableId& projectId,\n        const std::string& externalSourcePath,\n        const std::string& settingsJson) const\n    {''',
'workflow signature')
replace_once(path,
'''        request.assetProjectRelativePath = result.assetProjectRelativePath;\n        request.expectedFormat = format;\n''',
'''        request.assetProjectRelativePath = result.assetProjectRelativePath;\n        request.settingsJson = settingsJson;\n        request.expectedFormat = format;\n''',
'workflow settings')

# First import: accept the supported creator options object and apply it to the
# isolated Wicked scene before WISCENE serialization.
path = 'EngineBridge/src/ReusableAssetService.cpp'
replace_once(path,
'''#include "renegade/bridge/ReusableAssetService.h"\n''',
'''#include "renegade/bridge/ReusableAssetService.h"\n#include "renegade/bridge/CreatorModelImportRecipe.h"\n''',
'import recipe include')
replace_once(path,
'''                if (!options.empty())\n                {\n                    error = "Reusable model import version-1 settings do not support options.";\n                    return {};\n                }\n                nlohmann::json recipe;\n''',
'''                CreatorModelImportRecipe creatorRecipe;\n                if (!ParseCreatorModelImportOptions(optionsJson, creatorRecipe, error))\n                    return {};\n                nlohmann::json recipe;\n''',
'accept recipe options')
replace_once(path,
'''        const wi::scene::Scene* preparedScene = prepared.PeekScene();\n        if (preparedScene == nullptr)\n        {\n            result.error = "Reusable model conversion lost its prepared scene before validation.";\n            cleanupTemporary();\n            return result;\n        }\n\n        result.import = importer.SavePreparedModelAsset(prepared);\n''',
'''        wi::scene::Scene* preparedScene = prepared.PeekMutableScene();\n        if (preparedScene == nullptr)\n        {\n            result.error = "Reusable model conversion lost its prepared scene before validation.";\n            cleanupTemporary();\n            return result;\n        }\n        CreatorModelImportRecipe creatorRecipe;\n        if (!ParseCreatorModelImportOptions(request.settingsJson, creatorRecipe, result.error) ||\n            !ApplyCreatorModelImportRecipe(*preparedScene, root.generic_u8string(),\n                request.projectId, creatorRecipe, result.error) ||\n            !importer.RefreshPreparedModelEvidence(prepared, result.error))\n        {\n            cleanupTemporary();\n            return result;\n        }\n\n        result.import = importer.SavePreparedModelAsset(prepared);\n''',
'apply first-import recipe')

# Reimport: parse and replay exactly the accepted creator options.
path = 'EngineBridge/src/ReusableAssetReimportService.cpp'
replace_once(path,
'''#include "renegade/bridge/ReusableAssetService.h"\n''',
'''#include "renegade/bridge/ReusableAssetService.h"\n#include "renegade/bridge/CreatorModelImportRecipe.h"\n''',
'reimport recipe include')
replace_once(path,
'''        bool ParseStoredRecipe(\n            const ImportedProductRecord& provenance,\n            ModelSourceFormat& format,\n            std::string& error)\n''',
'''        bool ParseStoredRecipe(\n            const ImportedProductRecord& provenance,\n            ModelSourceFormat& format,\n            std::string& optionsJson,\n            std::string& error)\n''',
'parse signature')
replace_once(path,
'''                if (!recipe.at("options").empty())\n                {\n                    error = "Stored reusable-model version-1 import options are unsupported.";\n                    return false;\n                }\n                const std::string token = recipe.at("source_format").get<std::string>();\n''',
'''                optionsJson = recipe.at("options").dump();\n                CreatorModelImportRecipe creatorRecipe;\n                if (!ParseCreatorModelImportOptions(optionsJson, creatorRecipe, error))\n                    return false;\n                const std::string token = recipe.at("source_format").get<std::string>();\n''',
'parse stored options')
replace_once(path,
'''        ModelSourceFormat format = ModelSourceFormat::Unknown;\n        if (!ParseStoredRecipe(acceptedProvenance, format, result.error))\n            return result;\n''',
'''        ModelSourceFormat format = ModelSourceFormat::Unknown;\n        std::string creatorOptionsJson;\n        if (!ParseStoredRecipe(acceptedProvenance, format, creatorOptionsJson, result.error))\n            return result;\n''',
'call parse stored recipe')
replace_once(path,
'''        const wi::scene::Scene* preparedScene = prepared.PeekScene();\n        if (preparedScene == nullptr)\n        {\n            result.error = "Reusable model reimport lost its prepared scene before validation.";\n            cleanupTemporary();\n            return result;\n        }\n\n        result.import = importer.SavePreparedModelAsset(prepared);\n''',
'''        wi::scene::Scene* preparedScene = prepared.PeekMutableScene();\n        if (preparedScene == nullptr)\n        {\n            result.error = "Reusable model reimport lost its prepared scene before validation.";\n            cleanupTemporary();\n            return result;\n        }\n        CreatorModelImportRecipe creatorRecipe;\n        if (!ParseCreatorModelImportOptions(creatorOptionsJson, creatorRecipe, result.error) ||\n            !ApplyCreatorModelImportRecipe(*preparedScene, root.generic_u8string(),\n                request.projectId, creatorRecipe, result.error) ||\n            !importer.RefreshPreparedModelEvidence(prepared, result.error))\n        {\n            cleanupTemporary();\n            return result;\n        }\n\n        result.import = importer.SavePreparedModelAsset(prepared);\n''',
'replay recipe')
