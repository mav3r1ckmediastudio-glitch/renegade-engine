from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected exactly one anchor, found {count}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


# 1) Recover stranded official S7 v1.0 stock files without weakening creator ownership.
replace_once(
    "EngineBridge/src/ScriptLibraryService.cpp",
    '    constexpr const char* S7StockPackageId = "renegade.stock.actions.wave_a";\n\n',
    '''    constexpr const char* S7StockPackageId = "renegade.stock.actions.wave_a";\n\n    bool IsRecoverableStockProjectFile(\n        const std::string& packageId,\n        const std::string& packageVersion,\n        const std::string& filePath,\n        const std::string& projectHash,\n        const std::string& installedHash) noexcept\n    {\n        if (packageId != S7StockPackageId)\n            return false;\n\n        // Current byte-identical stock content is always safe to reclaim.\n        if (projectHash == installedHash)\n            return true;\n\n        // S7 v1.0 shipped before the package-ownership repair. Those builds\n        // could strand an official Action on disk without its lock record.\n        // v1.1 is allowed to migrate only the exact known official v1.0 bytes;\n        // arbitrary or creator-edited files remain collisions.\n        if (packageVersion != "1.1.0")\n            return false;\n\n        return\n            (filePath == "Door.lua" &&\n                projectHash == "fnv1a64:13d97d4492f8089e") ||\n            (filePath == "Switch.lua" &&\n                projectHash == "fnv1a64:e0aa38cd6391622c") ||\n            (filePath == "Pickup.lua" &&\n                projectHash == "fnv1a64:9fa1dd43736335c5");\n    }\n\n''')
replace_once(
    "EngineBridge/src/ScriptLibraryService.cpp",
    '''                if (package.packageId != S7StockPackageId ||\n                    currentHash != file->contentHash)\n                {\n''',
    '''                if (!IsRecoverableStockProjectFile(\n                        package.packageId,\n                        package.packageVersion,\n                        file->path,\n                        currentHash,\n                        file->contentHash))\n                {\n''')

# Avoid relying on transitive standard-library includes for std::size.
replace_once(
    "Tests/S6ScriptLibraryTests.cpp",
    '#include <iostream>\n#include <sstream>\n',
    '#include <iostream>\n#include <iterator>\n#include <sstream>\n')
replace_once(
    "Tests/S6ScriptLibraryTests.cpp",
    '            << "  \\\"package_version\\\": \\\"1.0.0\\\",\\n"\n',
    '            << "  \\\"package_version\\\": \\\"1.1.0\\\",\\n"\n')

# 2) Every committed reusable placement gets fresh persistent scene identity
# before its Undo/Redo snapshot is captured. This covers both direct placement
# and the actual Studio drag-ghost adoption path.
replace_once(
    "EngineBridge/src/ReusableAssetInstanceService.cpp",
    '#include "renegade/bridge/CreatorModelImportRecipe.h"\n\n',
    '#include "renegade/bridge/CreatorModelImportRecipe.h"\n#include "renegade/bridge/IdentityService.h"\n\n')
replace_once(
    "EngineBridge/src/ReusableAssetInstanceService.cpp",
    '''        bool WrapperExists(\n            const wi::scene::Scene& scene,\n            const wi::ecs::Entity entity) noexcept\n        {\n            return entity != wi::ecs::INVALID_ENTITY &&\n                scene.transforms.GetComponent(entity) != nullptr;\n        }\n\n''',
    '''        bool WrapperExists(\n            const wi::scene::Scene& scene,\n            const wi::ecs::Entity entity) noexcept\n        {\n            return entity != wi::ecs::INVALID_ENTITY &&\n                scene.transforms.GetComponent(entity) != nullptr;\n        }\n\n        bool AssignFreshReusableHierarchyIdentities(\n            wi::scene::Scene& scene,\n            const wi::ecs::Entity root) noexcept\n        {\n            if (root == wi::ecs::INVALID_ENTITY)\n                return false;\n\n            std::string error;\n            for (const wi::ecs::Entity entity :\n                EnumeratePersistentSceneEntities(scene))\n            {\n                if (entity != root &&\n                    !scene.Entity_IsDescendant(entity, root))\n                {\n                    continue;\n                }\n\n                // Prefab/template instances can carry copied metadata. Replace\n                // it unconditionally at creator placement time so every scene\n                // instance is addressable independently. The command snapshot\n                // is captured afterwards, therefore Undo/Redo retains these IDs.\n                if (!AssignNewPersistentEntityId(scene, entity, error))\n                    return false;\n            }\n            return true;\n        }\n\n''')
replace_once(
    "EngineBridge/src/ReusableAssetInstanceService.cpp",
    '''                ApplyReusableAssetName(*scene_, entity_, payloadRoot_, displayName_);\n\n                CaptureMaterialResources(firstMaterialIndex_);\n''',
    '''                ApplyReusableAssetName(*scene_, entity_, payloadRoot_, displayName_);\n                if (!AssignFreshReusableHierarchyIdentities(*scene_, entity_))\n                    return false;\n\n                CaptureMaterialResources(firstMaterialIndex_);\n''')
replace_once(
    "EngineBridge/src/ReusableAssetInstanceService.cpp",
    '''            scene_->Component_Attach(payloadRoot_, entity_, true);\n            ApplyReusableAssetName(*scene_, entity_, payloadRoot_, displayName_);\n\n            for (std::size_t index = animationCountBefore;\n''',
    '''            scene_->Component_Attach(payloadRoot_, entity_, true);\n            ApplyReusableAssetName(*scene_, entity_, payloadRoot_, displayName_);\n            if (!AssignFreshReusableHierarchyIdentities(*scene_, entity_))\n                return false;\n\n            for (std::size_t index = animationCountBefore;\n''')

replace_once(
    "Tests/ReusableAssetInstanceTests.cpp",
    '#include "renegade/bridge/CreatorModelImportRecipe.h"\n',
    '#include "renegade/bridge/CreatorModelImportRecipe.h"\n#include "renegade/bridge/IdentityService.h"\n')
replace_once(
    "Tests/ReusableAssetInstanceTests.cpp",
    '''        !Require(instances.front().payloadRoot == command.PayloadRootEntity(),\n            "instance inspection did not return the marked payload root"))\n        return 1;\n\n    const auto* wrapperTransform = target.transforms.GetComponent(wrapper);\n''',
    '''        !Require(instances.front().payloadRoot == command.PayloadRootEntity(),\n            "instance inspection did not return the marked payload root"))\n        return 1;\n\n    const wi::ecs::Entity placedPayloadChild =\n        FindNamedEntity(target, "Payload Child");\n    const StableId wrapperSceneId = PersistentEntityId(target, wrapper);\n    const StableId payloadSceneId =\n        PersistentEntityId(target, command.PayloadRootEntity());\n    const StableId payloadChildSceneId =\n        PersistentEntityId(target, placedPayloadChild);\n    if (!Require(placedPayloadChild != wi::ecs::INVALID_ENTITY,\n            "placed reusable payload child is missing") ||\n        !Require(IsValidStableId(wrapperSceneId) &&\n                 IsValidStableId(payloadSceneId) &&\n                 IsValidStableId(payloadChildSceneId),\n            "fresh reusable hierarchy did not receive persistent scene IDs") ||\n        !Require(wrapperSceneId != payloadSceneId &&\n                 wrapperSceneId != payloadChildSceneId &&\n                 payloadSceneId != payloadChildSceneId,\n            "fresh reusable hierarchy reused a persistent scene ID"))\n        return 1;\n\n    const auto* wrapperTransform = target.transforms.GetComponent(wrapper);\n''')
replace_once(
    "Tests/ReusableAssetInstanceTests.cpp",
    '''        !Require(instances.size() == 1 &&\n                 instances.front().assetId == AssetId &&\n                 instances.front().instanceRoot == wrapper,\n            "Redo did not restore the same wrapper identity and stable asset ID"))\n        return 1;\n\n    const fs::path scenePath = outputRoot / "ReusableInstance.wiscene";\n''',
    '''        !Require(instances.size() == 1 &&\n                 instances.front().assetId == AssetId &&\n                 instances.front().instanceRoot == wrapper,\n            "Redo did not restore the same wrapper identity and stable asset ID"))\n        return 1;\n\n    const wi::ecs::Entity redonePayloadChild =\n        FindNamedEntity(target, "Payload Child");\n    if (!Require(PersistentEntityId(target, wrapper) == wrapperSceneId &&\n                 PersistentEntityId(target, command.PayloadRootEntity()) ==\n                    payloadSceneId &&\n                 PersistentEntityId(target, redonePayloadChild) ==\n                    payloadChildSceneId,\n            "Undo/Redo changed reusable persistent scene identities"))\n        return 1;\n\n    auto secondPayload = wi::allocator::make_shared<wi::scene::Scene>();\n    const wi::ecs::Entity secondPayloadRoot =\n        secondPayload->Entity_CreateTransform("Second Payload Root");\n    const wi::ecs::Entity secondPayloadChild =\n        secondPayload->Entity_CreateTransform("Second Payload Child");\n    secondPayload->Component_Attach(secondPayloadChild, secondPayloadRoot);\n    PlaceReusableModelCommand secondCommand(\n        target,\n        std::move(secondPayload),\n        AssetId,\n        XMFLOAT3(12.0f, 5.0f, 6.0f),\n        1.0f);\n    if (!Require(secondCommand.Execute(),\n            "second reusable placement failed"))\n        return 1;\n    const wi::ecs::Entity secondPlacedChild =\n        FindNamedEntity(target, "Second Payload Child");\n    const StableId secondWrapperId =\n        PersistentEntityId(target, secondCommand.PlacedEntity());\n    const StableId secondPayloadId =\n        PersistentEntityId(target, secondCommand.PayloadRootEntity());\n    const StableId secondChildId =\n        PersistentEntityId(target, secondPlacedChild);\n    if (!Require(IsValidStableId(secondWrapperId) &&\n                 IsValidStableId(secondPayloadId) &&\n                 IsValidStableId(secondChildId),\n            "second reusable hierarchy did not receive persistent scene IDs") ||\n        !Require(secondWrapperId != wrapperSceneId &&\n                 secondWrapperId != payloadSceneId &&\n                 secondWrapperId != payloadChildSceneId &&\n                 secondPayloadId != wrapperSceneId &&\n                 secondPayloadId != payloadSceneId &&\n                 secondPayloadId != payloadChildSceneId &&\n                 secondChildId != wrapperSceneId &&\n                 secondChildId != payloadSceneId &&\n                 secondChildId != payloadChildSceneId,\n            "separate reusable placements shared persistent scene identity"))\n        return 1;\n    secondCommand.Undo();\n\n    const fs::path scenePath = outputRoot / "ReusableInstance.wiscene";\n''')
replace_once(
    "Tests/ReusableAssetInstanceTests.cpp",
    '''    if (!Require(adoptedCommand.Execute(),\n            "live cursor instance adoption failed") ||\n        !Require(adoptedCommand.PlacedEntity() == adoptedWrapper,\n            "live adoption replaced the visible cursor wrapper") ||\n        !Require(adoptedCommand.PayloadRootEntity() == adoptedPayload,\n            "live adoption replaced the visible payload root") ||\n        !Require(adoptedTarget.transforms.GetCount() == adoptedTransformCount,\n            "live adoption cloned or merged extra transforms"))\n        return 1;\n\n    const auto* adoptedAfter =\n''',
    '''    if (!Require(adoptedCommand.Execute(),\n            "live cursor instance adoption failed") ||\n        !Require(adoptedCommand.PlacedEntity() == adoptedWrapper,\n            "live adoption replaced the visible cursor wrapper") ||\n        !Require(adoptedCommand.PayloadRootEntity() == adoptedPayload,\n            "live adoption replaced the visible payload root") ||\n        !Require(adoptedTarget.transforms.GetCount() == adoptedTransformCount,\n            "live adoption cloned or merged extra transforms"))\n        return 1;\n\n    const StableId adoptedWrapperSceneId =\n        PersistentEntityId(adoptedTarget, adoptedWrapper);\n    const StableId adoptedPayloadSceneId =\n        PersistentEntityId(adoptedTarget, adoptedPayload);\n    if (!Require(IsValidStableId(adoptedWrapperSceneId) &&\n                 IsValidStableId(adoptedPayloadSceneId) &&\n                 adoptedWrapperSceneId != adoptedPayloadSceneId,\n            "live drag adoption did not stamp distinct persistent scene IDs"))\n        return 1;\n\n    const auto* adoptedAfter =\n''')
replace_once(
    "Tests/ReusableAssetInstanceTests.cpp",
    '''    if (!Require(adoptedCommand.Execute(),\n            "Redo of adopted live instance failed") ||\n        !Require(adoptedCommand.PlacedEntity() == adoptedWrapper,\n            "Redo remapped the adopted wrapper identity"))\n        return 1;\n    const auto* adoptedRedo =\n''',
    '''    if (!Require(adoptedCommand.Execute(),\n            "Redo of adopted live instance failed") ||\n        !Require(adoptedCommand.PlacedEntity() == adoptedWrapper,\n            "Redo remapped the adopted wrapper identity") ||\n        !Require(PersistentEntityId(adoptedTarget, adoptedWrapper) ==\n                    adoptedWrapperSceneId &&\n                 PersistentEntityId(adoptedTarget, adoptedPayload) ==\n                    adoptedPayloadSceneId,\n            "Redo changed live-adopted persistent scene IDs"))\n        return 1;\n    const auto* adoptedRedo =\n''')

# 3) A serialized wi::Resource wrapper is not a live texture after process restart.
replace_once(
    "EngineBridge/src/MaterialTextureAssetService.cpp",
    '''            if (texture.resource.IsValid() && texture.name.empty())\n            {\n''',
    '''            if (texture.resource.IsValid() &&\n                texture.resource.GetTexture().IsValid() &&\n                texture.name.empty())\n            {\n''')
replace_once(
    "Tests/MaterialTextureAssetTests.cpp",
    '''    Require(secondRestore.succeeded && secondRestore.discovered == 1 &&\n            secondRestore.restored == 0 && secondRestore.alreadyLive == 1 &&\n            fakeLoaderCalls == 1,\n        "material texture restore is not idempotent once the resource is live");\n''',
    '''    Require(secondRestore.succeeded && secondRestore.discovered == 1 &&\n            secondRestore.restored == 1 && secondRestore.alreadyLive == 0 &&\n            fakeLoaderCalls == 2,\n        "resource wrapper without a live texture was incorrectly treated as already rehydrated");\n''')

# 4) Draw the editor grid on the authored terrain reference plane instead of
# absolute world Y=0.02, preserving the no-terrain and import-preview fallbacks.
replace_once(
    "Studio/src/StudioApplication.cpp",
    '''        // w is the grid plane height. The generated deck's top surface is at\n        // exactly y = 0, so a grid drawn at y = 0 is coplanar with it and\n        // loses the GREATER depth test. 2 cm is invisible at any working\n        // camera distance and is the same trick Wicked's own helper uses.\n        constants.cameraPosition = XMFLOAT4(\n            camera->Eye.x,\n            camera->Eye.y,\n            camera->Eye.z,\n            creatorModelImporter.active ? CreatorImportStageHeight + 0.02f : 0.02f);\n''',
    '''        // w is the grid plane height. Renegade terrain uses bottomLevel as\n        // its authored reference plane (the standard terrain starts at -20 m),\n        // so an absolute y=0 grid visibly floats above a standard landscape.\n        // Keep a 2 cm depth epsilon and retain y=0.02 only when no terrain is\n        // present. The isolated creator-import stage keeps its own plane.\n        float gridPlaneHeight = 0.02f;\n        if (!creatorModelImporter.active && session_ != nullptr)\n        {\n            const auto& gridScene = session_->Scenes().GetScene();\n            if (gridScene.terrains.GetCount() != 0)\n            {\n                gridPlaneHeight =\n                    bridge::CaptureTerrain(gridScene.terrains[0]).minimumHeight +\n                    0.02f;\n            }\n        }\n        constants.cameraPosition = XMFLOAT4(\n            camera->Eye.x,\n            camera->Eye.y,\n            camera->Eye.z,\n            creatorModelImporter.active\n                ? CreatorImportStageHeight + 0.02f\n                : gridPlaneHeight);\n''')

# 5) Entity reference picker regression: reusable payload internals collapse to
# one creator-facing wrapper label and stable scene reference.
replace_once(
    "Tests/S4DScriptReferenceAuthoringTests.cpp",
    '#include "renegade/bridge/IdentityService.h"\n#include "renegade/bridge/SceneService.h"\n',
    '#include "renegade/bridge/IdentityService.h"\n#include "renegade/bridge/ReusableAssetInstanceService.h"\n#include "renegade/bridge/SceneService.h"\n')
replace_once(
    "Tests/S4DScriptReferenceAuthoringTests.cpp",
    '''    bool ContainsLabel(\n        const std::vector<renegade::bridge::ScriptReferenceOption>& options,\n        const std::string& label)\n    {\n        return std::any_of(\n            options.begin(), options.end(),\n            [&](const auto& option) { return option.label == label; });\n    }\n''',
    '''    bool ContainsLabel(\n        const std::vector<renegade::bridge::ScriptReferenceOption>& options,\n        const std::string& label)\n    {\n        return std::any_of(\n            options.begin(), options.end(),\n            [&](const auto& option) { return option.label == label; });\n    }\n\n    std::size_t CountLabel(\n        const std::vector<renegade::bridge::ScriptReferenceOption>& options,\n        const std::string& label)\n    {\n        return static_cast<std::size_t>(std::count_if(\n            options.begin(), options.end(),\n            [&](const auto& option) { return option.label == label; }));\n    }\n''')
replace_once(
    "Tests/S4DScriptReferenceAuthoringTests.cpp",
    '''    const auto walk = scene.Entity_CreateTransform("Walk");\n    scene.animations.Create(walk);\n\n    std::string error;\n''',
    '''    const auto walk = scene.Entity_CreateTransform("Walk");\n    scene.animations.Create(walk);\n\n    const auto reusableWrapper =\n        scene.Entity_CreateTransform("Crate 002");\n    const auto reusablePayload = scene.Entity_CreateTransform("730");\n    const auto reusableChild = scene.Entity_CreateTransform("757");\n    scene.Component_Attach(reusablePayload, reusableWrapper, true);\n    scene.Component_Attach(reusableChild, reusablePayload, true);\n    auto& reusableMetadata = scene.metadatas.Create(reusableWrapper);\n    reusableMetadata.string_values.set(\n        ReusableAssetInstanceIdMetadataKey, GenerateStableId());\n    reusableMetadata.int_values.set(\n        ReusableAssetInstanceVersionMetadataKey,\n        ReusableAssetInstanceVersion);\n\n    std::string error;\n''')
replace_once(
    "Tests/S4DScriptReferenceAuthoringTests.cpp",
    '''    const StableId targetId = PersistentEntityId(scene, target);\n    const StableId walkId = PersistentEntityId(scene, walk);\n    if (!IsValidStableId(targetId) || !IsValidStableId(walkId))\n        return Fail("Scene references did not receive stable IDs");\n''',
    '''    const StableId targetId = PersistentEntityId(scene, target);\n    const StableId walkId = PersistentEntityId(scene, walk);\n    const StableId reusableWrapperId =\n        PersistentEntityId(scene, reusableWrapper);\n    if (!IsValidStableId(targetId) || !IsValidStableId(walkId) ||\n        !IsValidStableId(reusableWrapperId))\n        return Fail("Scene references did not receive stable IDs");\n''')
replace_once(
    "Tests/S4DScriptReferenceAuthoringTests.cpp",
    '''    if (!ContainsLabel(entityOptions, "Barrel") ||\n        !ContainsLabel(entityOptions, "Target Crate") ||\n        !ContainsLabel(entityOptions, "Walk"))\n    {\n        return Fail("entity reference enumeration did not expose creator Scene entities");\n    }\n\n    std::vector<ScriptReferenceOption> animationOptions;\n''',
    '''    if (!ContainsLabel(entityOptions, "Barrel") ||\n        !ContainsLabel(entityOptions, "Target Crate") ||\n        !ContainsLabel(entityOptions, "Walk"))\n    {\n        return Fail("entity reference enumeration did not expose creator Scene entities");\n    }\n    const auto reusableOption = std::find_if(\n        entityOptions.begin(), entityOptions.end(),\n        [](const ScriptReferenceOption& option)\n        {\n            return option.label == "Crate 002";\n        });\n    if (CountLabel(entityOptions, "Crate 002") != 1 ||\n        ContainsLabel(entityOptions, "730") ||\n        ContainsLabel(entityOptions, "757") ||\n        reusableOption == entityOptions.end() ||\n        reusableOption->referenceId != reusableWrapperId)\n    {\n        return Fail(\n            "reusable asset target picker leaked payload entities instead of one creator-facing wrapper");\n    }\n\n    std::vector<ScriptReferenceOption> animationOptions;\n''')

print("S7 owner hardening source patch applied")
