#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "renegade/bridge/HumanoidRetargetService.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using renegade::bridge::HumanoidBone;

    constexpr const char* ProjectId = "88888888-8888-4888-8888-888888888888";

    bool Require(const bool condition, const std::string& message)
    {
        if (!condition)
            std::cerr << "Phase7Gate7B test failure: " << message << '\n';
        return condition;
    }

    wi::ecs::Entity AddBone(
        wi::scene::Scene& scene,
        wi::scene::ArmatureComponent& armature,
        const wi::ecs::Entity rig,
        const std::string& name)
    {
        const auto entity = wi::ecs::CreateEntity();
        scene.names.Create(entity).name = name;
        scene.transforms.Create(entity);
        scene.Component_Attach(entity, rig);
        armature.boneCollection.push_back(entity);
        armature.inverseBindMatrices.push_back(wi::math::IDENTITY_MATRIX);
        return entity;
    }

    const std::vector<std::pair<const char*, HumanoidBone>>& MixamoCoreBones()
    {
        static const std::vector<std::pair<const char*, HumanoidBone>> bones = {
            {"mixamorig:Hips", HumanoidBone::Hips},
            {"mixamorig:Spine", HumanoidBone::Spine},
            {"mixamorig:Spine1", HumanoidBone::Chest},
            {"mixamorig:Spine2", HumanoidBone::UpperChest},
            {"mixamorig:Neck", HumanoidBone::Neck},
            {"mixamorig:Head", HumanoidBone::Head},
            {"mixamorig:LeftShoulder", HumanoidBone::LeftShoulder},
            {"mixamorig:LeftArm", HumanoidBone::LeftUpperArm},
            {"mixamorig:LeftForeArm", HumanoidBone::LeftLowerArm},
            {"mixamorig:LeftHand", HumanoidBone::LeftHand},
            {"mixamorig:RightShoulder", HumanoidBone::RightShoulder},
            {"mixamorig:RightArm", HumanoidBone::RightUpperArm},
            {"mixamorig:RightForeArm", HumanoidBone::RightLowerArm},
            {"mixamorig:RightHand", HumanoidBone::RightHand},
            {"mixamorig:LeftUpLeg", HumanoidBone::LeftUpperLeg},
            {"mixamorig:LeftLeg", HumanoidBone::LeftLowerLeg},
            {"mixamorig:LeftFoot", HumanoidBone::LeftFoot},
            {"mixamorig:LeftToeBase", HumanoidBone::LeftToes},
            {"mixamorig:RightUpLeg", HumanoidBone::RightUpperLeg},
            {"mixamorig:RightLeg", HumanoidBone::RightLowerLeg},
            {"mixamorig:RightFoot", HumanoidBone::RightFoot},
            {"mixamorig:RightToeBase", HumanoidBone::RightToes},
            {"mixamorig:LeftHandIndex1", HumanoidBone::LeftIndexProximal},
            {"mixamorig:RightHandIndex1", HumanoidBone::RightIndexProximal},
        };
        return bones;
    }

    wi::ecs::Entity AddMixamoRig(
        wi::scene::Scene& scene,
        std::vector<wi::ecs::Entity>* entities = nullptr)
    {
        const auto rig = wi::ecs::CreateEntity();
        scene.names.Create(rig).name = "Mixamo Character";
        scene.transforms.Create(rig);
        auto& armature = scene.armatures.Create(rig);

        if (entities != nullptr)
        {
            entities->clear();
            entities->reserve(MixamoCoreBones().size());
        }
        for (const auto& [name, unused] : MixamoCoreBones())
        {
            (void)unused;
            const auto entity = AddBone(scene, armature, rig, name);
            if (entities != nullptr)
                entities->push_back(entity);
        }
        return rig;
    }
}

int main()
{
    wi::scene::Scene scene;
    std::vector<wi::ecs::Entity> entities;
    const auto rig = AddMixamoRig(scene, &entities);
    auto& armature = *scene.armatures.GetComponent(rig);

    const auto mapped = renegade::bridge::BuildAutoHumanoidMapping(scene, rig);
    if (!Require(mapped.mappedBones >= 22, "expected core Mixamo bones to auto-map")) return 1;
    if (!Require(
            mapped.mapping.bones[static_cast<std::size_t>(HumanoidBone::Hips)] == entities[0],
            "hips mapping mismatch")) return 1;
    if (!Require(
            mapped.mapping.bones[static_cast<std::size_t>(HumanoidBone::LeftHand)] == entities[9],
            "generic LeftHand must not steal LeftHandIndex1")) return 1;
    if (!Require(
            mapped.mapping.bones[static_cast<std::size_t>(HumanoidBone::LeftIndexProximal)] == entities[22],
            "left index proximal mapping mismatch")) return 1;

    renegade::bridge::CommandService commands;
    if (!Require(commands.Execute(
            std::make_unique<renegade::bridge::SetHumanoidMappingCommand>(
                scene, rig, mapped.mapping)),
            "auto-map command should commit")) return 1;
    if (!Require(scene.humanoids.Contains(rig), "auto-map should create HumanoidComponent")) return 1;

    if (!Require(commands.Undo(), "auto-map undo should succeed")) return 1;
    if (!Require(!scene.humanoids.Contains(rig), "undo should remove newly-created HumanoidComponent")) return 1;
    if (!Require(commands.Redo(), "auto-map redo should succeed")) return 1;
    if (!Require(scene.humanoids.Contains(rig), "redo should restore HumanoidComponent")) return 1;

    const auto jaw = wi::ecs::CreateEntity();
    scene.names.Create(jaw).name = "ManualJaw";
    scene.transforms.Create(jaw);
    scene.Component_Attach(jaw, rig);
    armature.boneCollection.push_back(jaw);
    armature.inverseBindMatrices.push_back(wi::math::IDENTITY_MATRIX);

    if (!Require(commands.Execute(
            std::make_unique<renegade::bridge::SetHumanoidBoneCommand>(
                scene, rig, HumanoidBone::Jaw, jaw)),
            "manual bone command should commit")) return 1;
    if (!Require(
            scene.humanoids.GetComponent(rig)->bones[static_cast<std::size_t>(HumanoidBone::Jaw)] == jaw,
            "manual jaw mapping mismatch")) return 1;
    if (!Require(commands.Undo(), "manual mapping undo should succeed")) return 1;
    if (!Require(
            scene.humanoids.GetComponent(rig)->bones[static_cast<std::size_t>(HumanoidBone::Jaw)] ==
                wi::ecs::INVALID_ENTITY,
            "manual mapping undo should restore prior target")) return 1;

    if (!Require(
            renegade::bridge::ClassifyHumanoidAnimationSource("walk.VRMA") ==
                renegade::bridge::HumanoidAnimationSourceFormat::Vrma,
            "VRMA classification failed")) return 1;
    if (!Require(
            renegade::bridge::ClassifyHumanoidAnimationSource("hero.vrm") ==
                renegade::bridge::HumanoidAnimationSourceFormat::Vrm,
            "VRM classification failed")) return 1;
    if (!Require(
            renegade::bridge::ClassifyHumanoidAnimationSource("clip.fbx") ==
                renegade::bridge::HumanoidAnimationSourceFormat::Fbx,
            "FBX classification failed")) return 1;

    // CW-03: the governed Character import path must prepare a Mixamo-style
    // destination during import rather than requiring the creator to place it
    // in a scene and run HUMANOID / RETARGET manually afterwards.
    wi::scene::Scene importedCharacter;
    const auto importedRig = AddMixamoRig(importedCharacter);
    renegade::bridge::CreatorModelImportRecipe characterRecipe;
    characterRecipe.assetKind = renegade::bridge::CreatorAssetImportKind::Character;
    std::string error;
    if (!Require(
            renegade::bridge::ApplyCreatorModelImportRecipe(
                importedCharacter, "", ProjectId, characterRecipe, error),
            "Character import should auto-prepare a supported Mixamo humanoid: " + error)) return 1;
    const auto* preparedHumanoid = importedCharacter.humanoids.GetComponent(importedRig);
    if (!Require(preparedHumanoid != nullptr && preparedHumanoid->IsValid(),
            "Character import did not persist a valid native Wicked humanoid mapping")) return 1;

    // External animation provenance is Character-only. Generic MODEL recipes
    // must fail before touching a retained source file.
    renegade::bridge::CreatorModelImportRecipe modelWithExternal;
    renegade::bridge::CreatorExternalAnimationImportRecipe external;
    external.sourceProjectRelativePath =
        "SourceAssets/Animations/Snapshots/Walk_deadbeef/Walk.fbx";
    external.sourceAnimationIndex = 0;
    external.name = "Walk";
    external.start = 0.0f;
    external.end = 1.0f;
    external.enabled = true;
    modelWithExternal.externalAnimations.push_back(external);
    error.clear();
    if (!Require(
            !renegade::bridge::ApplyCreatorModelImportRecipe(
                importedCharacter, "", ProjectId, modelWithExternal, error) &&
                error.find("only be committed by a Character import recipe") != std::string::npos,
            "generic Model recipe did not reject Character external animation provenance")) return 1;

    // A missing retained source must fail loudly with an actionable import
    // error instead of silently finalising a Character without its requested
    // external animation.
    characterRecipe.externalAnimations.push_back(external);
    error.clear();
    if (!Require(
            !renegade::bridge::ApplyCreatorModelImportRecipe(
                importedCharacter, "", ProjectId, characterRecipe, error) &&
                error.find("active project root") != std::string::npos,
            "Character import did not expose a clear retained-source repair error")) return 1;

    std::cout << "Phase 7B / CW-03 humanoid preparation tests passed\n";
    return 0;
}
