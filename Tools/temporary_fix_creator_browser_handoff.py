from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    file = Path(path)
    text = file.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected exactly one replacement target, found {count}")
    file.write_text(text.replace(old, new, 1), encoding="utf-8")


# 1) Split creator catalogue presentation from the expensive LC01 disk recovery/hash pass.
replace_once(
    "EngineBridge/include/renegade/bridge/CreatorAssetWorkflowService.h",
    '''        [[nodiscard]] bool BuildCatalogue(\n            const std::string& projectRoot,\n            const StableId& projectId,\n            AssetCatalogue& catalogue,\n            std::string& error) const;\n\n        [[nodiscard]] CreatorModelImportResult ImportModel(\n''',
    '''        // Fast creator-facing projection of the last committed LC01 state.\n        // Unlike BuildCatalogue(), this never rescans or hashes Content/SourceAssets;\n        // Studio uses it for normal browser presentation and immediate post-import\n        // reveal so a committed stable ID cannot be replaced by a recovery pass.\n        [[nodiscard]] bool BuildCatalogueSnapshot(\n            const std::string& projectRoot,\n            const StableId& projectId,\n            AssetCatalogue& catalogue,\n            std::string& error) const;\n\n        // Explicit disk-recovery projection. This retains the existing LC01 moved /\n        // missing / stale refresh semantics for lifecycle checks and recovery flows.\n        [[nodiscard]] bool BuildCatalogue(\n            const std::string& projectRoot,\n            const StableId& projectId,\n            AssetCatalogue& catalogue,\n            std::string& error) const;\n\n        [[nodiscard]] CreatorModelImportResult ImportModel(\n''')

replace_once(
    "EngineBridge/src/CreatorAssetWorkflowService.cpp",
    '''    bool CreatorAssetWorkflowService::BuildCatalogue(\n        const std::string& projectRoot,\n        const StableId& projectId,\n        AssetCatalogue& catalogue,\n        std::string& error) const\n    {\n''',
    '''    bool CreatorAssetWorkflowService::BuildCatalogueSnapshot(\n        const std::string& projectRoot,\n        const StableId& projectId,\n        AssetCatalogue& catalogue,\n        std::string& error) const\n    {\n        if (!IsValidStableId(projectId))\n        {\n            error = "Creator Asset Browser requires a valid project ID.";\n            return false;\n        }\n        fs::path root;\n        if (!ResolveRoot(projectRoot, root, error))\n            return false;\n\n        // Studio already has an authoritative transaction result after import.\n        // Read the committed LC01 document exactly as written instead of running\n        // RefreshRegistryInternal(), which hashes every registered file and can\n        // turn a cheap reveal into a long synchronous editor stall.\n        AssetRegistry registry;\n        if (!ExistingRegistryOrEmpty(root, projectId, registry, error))\n            return false;\n\n        AssetCatalogueMetadataDocument metadata;\n        if (!ReadAssetCatalogueMetadata(\n                root.generic_u8string(), projectId, metadata, error))\n            return false;\n\n        if (!BuildAssetCatalogue(root.generic_u8string(), projectId,\n                registry, metadata, catalogue, error))\n            return false;\n\n        // Keep the same missing-product/source projection as the recovery build;\n        // only the disk recovery/hash pass is intentionally omitted.\n        for (auto& entry : catalogue.entries)\n        {\n            if (!entry.importedProduct || entry.productAvailable ||\n                entry.state != AssetCatalogueState::Missing ||\n                !IsValidStableId(entry.sourceAssetId))\n                continue;\n\n            const auto source = std::find_if(\n                registry.records.begin(), registry.records.end(),\n                [&entry](const AssetRecord& record)\n                { return record.assetId == entry.sourceAssetId; });\n            entry.sourceAvailable = source != registry.records.end() &&\n                source->sourceAvailable;\n        }\n        error.clear();\n        return true;\n    }\n\n    bool CreatorAssetWorkflowService::BuildCatalogue(\n        const std::string& projectRoot,\n        const StableId& projectId,\n        AssetCatalogue& catalogue,\n        std::string& error) const\n    {\n''')

# 2) Routine Studio Asset Browser refresh must use committed state, not a whole-project recovery scan.
replace_once(
    "Studio/src/RenegadeStudioChromeAssets.cpp",
    '''            if (!creatorAssetWorkflow_.BuildCatalogue(\n                    project.rootPath, project.projectId, catalogue, error))\n''',
    '''            if (!creatorAssetWorkflow_.BuildCatalogueSnapshot(\n                    project.rootPath, project.projectId, catalogue, error))\n''')

# 3) Confirm Import already reveals through CreatorAssetStudioChrome; do not first perform a second raw filesystem refresh.
replace_once(
    "Studio/src/StudioApplication.cpp",
    '''                        studioChrome_.SetActiveBottomTab(0, true);\n                        RefreshAssetBrowser();\n                        std::string browserError;\n''',
    '''                        studioChrome_.SetActiveBottomTab(0, true);\n                        std::string browserError;\n''')

# 4) Regression: the immediate, non-recovery browser projection must expose the exact committed product ID/path as placeable.
replace_once(
    "Tests/CreatorAssetWorkflowGraphicsProof.cpp",
    '''        if (!Require(workflow.SetCreatorTags(\n                projectRoot.generic_u8string(), ProjectId, productId,\n                {"Hero", "gate5"}, error),\n                "creator tags could not be persisted: " + error))\n            return false;\n\n        AssetCatalogue catalogue;\n''',
    '''        if (!Require(workflow.SetCreatorTags(\n                projectRoot.generic_u8string(), ProjectId, productId,\n                {"Hero", "gate5"}, error),\n                "creator tags could not be persisted: " + error))\n            return false;\n\n        AssetCatalogue committedSnapshot;\n        if (!Require(workflow.BuildCatalogueSnapshot(\n                projectRoot.generic_u8string(), ProjectId, committedSnapshot, error),\n                "committed creator catalogue snapshot failed: " + error))\n            return false;\n        const auto* committedEntry = FindEntry(committedSnapshot, productId);\n        if (!Require(committedEntry != nullptr &&\n                committedEntry->projectRelativePath ==\n                    imported.assetProjectRelativePath &&\n                committedEntry->state == AssetCatalogueState::Current &&\n                CanPlaceCreatorModelAsset(*committedEntry),\n                "committed browser snapshot did not expose the exact stable-ID/path as a placeable model"))\n            return false;\n\n        AssetCatalogue catalogue;\n''')

print("creator browser handoff patch applied")
