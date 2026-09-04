#include "renegade/bridge/ScriptReferenceAuthoringService.h"

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/SceneService.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    int Fail(const std::string& message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    bool ContainsLabel(
        const std::vector<renegade::bridge::ScriptReferenceOption>& options,
        const std::string& label)
    {
        return std::any_of(
            options.begin(), options.end(),
            [&](const auto& option) { return option.label == label; });
    }
}

int main()
{
    using namespace renegade::bridge;

    SceneService scenes;
    auto& scene = scenes.GetScene();
    const auto barrel = scene.Entity_CreateTransform("Barrel");
    const auto target = scene.Entity_CreateTransform("Target Crate");
    const auto walk = scene.Entity_CreateTransform("Walk");
    scene.animations.Create(walk);

    std::string error;
    if (!EnsurePersistentEntityIdentities(scene, error))
        return Fail("could not seed persistent Scene identities: " + error);

    const StableId targetId = PersistentEntityId(scene, target);
    const StableId walkId = PersistentEntityId(scene, walk);
    if (!IsValidStableId(targetId) || !IsValidStableId(walkId))
        return Fail("Scene references did not receive stable IDs");

    std::vector<ScriptReferenceOption> entityOptions;
    if (!BuildSceneScriptReferenceOptions(
            scenes, ScriptPropertyType::EntityReference, entityOptions, error))
    {
        return Fail("entity reference enumeration failed: " + error);
    }
    if (!ContainsLabel(entityOptions, "Barrel") ||
        !ContainsLabel(entityOptions, "Target Crate") ||
        !ContainsLabel(entityOptions, "Walk"))
    {
        return Fail("entity reference enumeration did not expose creator Scene entities");
    }

    std::vector<ScriptReferenceOption> animationOptions;
    if (!BuildSceneScriptReferenceOptions(
            scenes, ScriptPropertyType::Animation, animationOptions, error))
    {
        return Fail("animation reference enumeration failed: " + error);
    }
    if (animationOptions.size() != 1 ||
        animationOptions.front().referenceId != walkId ||
        animationOptions.front().label != "Walk")
    {
        return Fail("animation picker did not resolve the named animation entity");
    }

    AssetRegistry registry;
    registry.projectId = GenerateStableId();
    AssetRecord audio;
    audio.assetId = GenerateStableId();
    audio.projectRelativePath = "Content/Audio/door_open.wav";
    audio.sourceAvailable = true;
    registry.records.push_back(audio);
    AssetRecord model;
    model.assetId = GenerateStableId();
    model.projectRelativePath = "Content/Models/barrel.rasset";
    model.sourceAvailable = true;
    registry.records.push_back(model);
    AssetRecord missing;
    missing.assetId = GenerateStableId();
    missing.projectRelativePath = "Content/Textures/missing.png";
    missing.sourceAvailable = false;
    registry.records.push_back(missing);

    std::vector<ScriptReferenceOption> assetOptions;
    if (!BuildRegistryScriptReferenceOptions(
            registry, ScriptPropertyType::AssetReference, assetOptions, error))
    {
        return Fail("asset reference enumeration failed: " + error);
    }
    if (assetOptions.size() != 2 ||
        !ContainsLabel(assetOptions, "door_open.wav") ||
        !ContainsLabel(assetOptions, "barrel.rasset"))
    {
        return Fail("asset picker did not project current LC01 records");
    }

    std::vector<ScriptReferenceOption> audioOptions;
    if (!BuildRegistryScriptReferenceOptions(
            registry, ScriptPropertyType::Audio, audioOptions, error))
    {
        return Fail("audio reference enumeration failed: " + error);
    }
    if (audioOptions.size() != 1 ||
        audioOptions.front().referenceId != audio.assetId)
    {
        return Fail("audio picker did not filter the asset registry to audio");
    }

    ScriptMetadataPropertyDescriptor targetMetadata;
    targetMetadata.name = "target";
    targetMetadata.type = ScriptPropertyType::EntityReference;
    ScriptReferenceOption targetOption;
    targetOption.referenceId = targetId;
    targetOption.label = "Target Crate";
    ScriptPropertyValue property;
    if (!BuildScriptReferenceProperty(
            targetMetadata, targetOption, property, error) ||
        property.name != "target" ||
        property.type != ScriptPropertyType::EntityReference ||
        property.referenceId != targetId)
    {
        return Fail("resolved entity picker choice did not become an S2 property");
    }

    ScriptReferenceOption clear;
    if (!BuildScriptReferenceProperty(targetMetadata, clear, property, error) ||
        !property.referenceId.empty() || !property.pathHint.empty())
    {
        return Fail("explicit empty picker choice did not clear the reference");
    }

    ScriptReferenceOption unresolved;
    unresolved.referenceId = GenerateStableId();
    unresolved.label = "Unresolved";
    unresolved.resolved = false;
    if (BuildScriptReferenceProperty(targetMetadata, unresolved, property, error) ||
        error.empty())
    {
        return Fail("new unresolved reference assignment was accepted");
    }

    ScriptMetadataPropertyDescriptor wrong;
    wrong.name = "speed";
    wrong.type = ScriptPropertyType::Float;
    if (BuildScriptReferenceProperty(wrong, targetOption, property, error) ||
        error.empty())
    {
        return Fail("non-reference metadata entered S4D reference authoring");
    }

    std::cout << "PASS: S4D governed reference authoring\n";
    return 0;
}
