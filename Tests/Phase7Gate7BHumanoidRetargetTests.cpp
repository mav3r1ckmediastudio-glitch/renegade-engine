#include "renegade/bridge/HumanoidRetargetService.h"
#include "renegade/bridge/AnimationService.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using renegade::bridge::HumanoidBone;

    bool Require(const bool condition, const char* message)
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
}

int main()
{
    wi::scene::Scene scene;
    const auto rig = wi::ecs::CreateEntity();
    scene.names.Create(rig).name = "Mixamo Character";
    scene.transforms.Create(rig);
    auto& armature = scene.armatures.Create(rig);

    const std::vector<std::pair<const char*, HumanoidBone>> bones = {
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

    std::vector<wi::ecs::Entity> entities;
    entities.reserve(bones.size());
    for (const auto& [name, unused] : bones)
    {
        (void)unused;
        entities.push_back(AddBone(scene, armature, rig, name));
    }

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

    // External importer FBX sources can contain a named armature but no
    // HumanoidComponent. Opt-in preparation must create the real native map.
    std::string sourceMappingError;
    if (!Require(renegade::bridge::EnsureHumanoidAnimationSourceMapping(
            scene, sourceMappingError),
            "complete external source armature should auto-map")) return 1;
    if (!Require(renegade::bridge::IsHumanoidMappingValid(
            renegade::bridge::CaptureHumanoidMapping(scene, rig)),
            "source mapping must create a valid native HumanoidComponent")) return 1;
    scene.humanoids.Remove(rig);
    wi::scene::Scene incompleteSource;
    if (!Require(!renegade::bridge::EnsureHumanoidAnimationSourceMapping(
            incompleteSource, sourceMappingError) && !sourceMappingError.empty(),
            "unmapped external source must fail explicitly")) return 1;

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

    // Importer v3 drives Wicked's real transport in its own preview scene.
    // Exercise the native operations without touching an authored level clip.
    wi::scene::Scene preview;
    const auto previewClip = wi::ecs::CreateEntity();
    auto& nativeClip = preview.animations.Create(previewClip);
    nativeClip.start = 0.25f;
    nativeClip.end = 2.0f;
    nativeClip.timer = 1.0f;
    wi::scene::Scene authored;
    const auto authoredClip = wi::ecs::CreateEntity();
    authored.animations.Create(authoredClip).Play();
    if (!Require(renegade::bridge::PlayAnimation(preview, previewClip, true) &&
            nativeClip.IsPlaying() && nativeClip.timer == nativeClip.start,
            "preview must play the native clip from its authored start")) return 1;
    if (!Require(renegade::bridge::PauseAnimation(preview, previewClip) &&
            !nativeClip.IsPlaying(), "preview must pause the native component")) return 1;
    if (!Require(renegade::bridge::ScrubAnimation(preview, previewClip, 9.0f) &&
            nativeClip.timer == nativeClip.end,
            "preview scrub must respect the native clip range")) return 1;
    if (!Require(renegade::bridge::StopAnimation(preview, previewClip) &&
            !nativeClip.IsPlaying() && nativeClip.timer == nativeClip.start,
            "preview stop must rewind the native component")) return 1;
    if (!Require(authored.animations.GetComponent(authoredClip)->IsPlaying(),
            "preview transport must leave the separate authored scene unchanged")) return 1;

    // A synthetic external humanoid carries native animation data (not a
    // decorative UI clip). Exercise Wicked's baked retarget onto our rig.
    wi::scene::Scene external;
    const auto externalRig = wi::ecs::CreateEntity();
    external.names.Create(externalRig).name = "External Mixamo Rig";
    external.transforms.Create(externalRig);
    auto& externalArmature = external.armatures.Create(externalRig);
    wi::ecs::Entity externalHips = wi::ecs::INVALID_ENTITY;
    for (const auto& bone : bones)
    {
        const auto entity = AddBone(external, externalArmature, externalRig, bone.first);
        if (bone.second == HumanoidBone::Hips)
            externalHips = entity;
    }
    if (!Require(renegade::bridge::EnsureHumanoidAnimationSourceMapping(
            external, sourceMappingError),
            "synthetic external source needs a native humanoid map")) return 1;
    const auto sourceClip = wi::ecs::CreateEntity();
    auto& sourceAnimation = external.animations.Create(sourceClip);
    sourceAnimation.start = 0.0f;
    sourceAnimation.end = 1.0f;
    auto& channel = sourceAnimation.channels.emplace_back();
    channel.target = externalHips;
    channel.path = wi::scene::AnimationComponent::AnimationChannel::Path::ROTATION;
    channel.samplerIndex = 0;
    const auto animationData = wi::ecs::CreateEntity();
    auto& data = external.animation_datas.Create(animationData);
    data.keyframe_times = {0.0f, 1.0f};
    data.keyframe_data = {0.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f};
    sourceAnimation.samplers.emplace_back().data = animationData;
    external.Component_Attach(animationData, sourceClip);
    external.Component_Attach(sourceClip, externalRig);
    const auto bakedEntity = scene.RetargetAnimation(rig, sourceClip, true, &external);
    const auto* baked = scene.animations.GetComponent(bakedEntity);
    if (!Require(bakedEntity != wi::ecs::INVALID_ENTITY && baked != nullptr &&
            !baked->channels.empty() && !baked->samplers.empty(),
            "Wicked must create a real baked native destination clip")) return 1;
    const auto* bakedData = scene.animation_datas.GetComponent(baked->samplers.front().data);
    if (!Require(bakedData != nullptr && bakedData->keyframe_times.size() == 2 &&
            bakedData->keyframe_data.size() == 8 &&
            baked->samplers.front().scene == nullptr,
            "baked retarget must own animation keys without its external source scene")) return 1;

    std::cout << "Phase 7B humanoid mapping tests passed\n";
    return 0;
}
