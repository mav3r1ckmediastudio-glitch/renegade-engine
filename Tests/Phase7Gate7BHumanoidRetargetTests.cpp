#include "renegade/bridge/HumanoidRetargetService.h"
#include "renegade/bridge/AnimationService.h"

#include <chrono>
#include <filesystem>
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
    // An imported FBX has no authored look-at target. Wicked enables look-at
    // by default, which turns the head toward world origin on the first tick.
    auto* importedHumanoid = scene.humanoids.GetComponent(rig);
    if (!Require(importedHumanoid != nullptr && importedHumanoid->IsLookAtEnabled(),
            "test rig should exhibit Wicked default look-at")) return 1;
    if (!Require(renegade::bridge::DisableDefaultHumanoidLookAt(scene) == 1 &&
            !importedHumanoid->IsLookAtEnabled(),
            "default look-at must be disabled before import preview and commit")) return 1;
    if (!Require(renegade::bridge::DisableDefaultHumanoidLookAt(scene) == 0,
            "default look-at normalization must be idempotent")) return 1;
    importedHumanoid->SetLookAtEnabled(true);
    importedHumanoid->lookAt = XMFLOAT3(0.0f, 0.0f, 5.0f);
    if (!Require(renegade::bridge::DisableDefaultHumanoidLookAt(scene) == 0 &&
            importedHumanoid->IsLookAtEnabled(),
            "an explicitly configured look-at position must survive")) return 1;
    importedHumanoid->lookAt = XMFLOAT3(0.0f, 0.0f, 0.0f);
    importedHumanoid->lookAtEntity = entities[0];
    if (!Require(renegade::bridge::DisableDefaultHumanoidLookAt(scene) == 0 &&
            importedHumanoid->IsLookAtEnabled(),
            "an explicitly configured look-at entity must survive")) return 1;
    importedHumanoid->lookAtEntity = wi::ecs::INVALID_ENTITY;
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

    // The actual importer uses a FILE path, not an in-memory source. Save a
    // synthetic WISCENE, retarget it through the public bridge command, make
    // the source unavailable, redo the baked result, and reopen the scene.
    namespace fs = std::filesystem;
    std::error_code ioError;
    const fs::path proofDirectory = fs::temp_directory_path(ioError) /
        ("renegade-v3-retarget-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if (!Require(!ioError && fs::create_directories(proofDirectory, ioError) &&
            !ioError, "physical retarget test directory could not be created")) return 1;
    const fs::path fileSource = proofDirectory / "external-motion.wiscene";
    {
        wi::Archive writer(fileSource.generic_u8string(), false, false);
        if (!Require(writer.IsOpen(), "could not open external animation archive")) return 1;
        writer.SetCompressionEnabled(true);
        external.Serialize(writer);
        if (!Require(writer.SaveFile(fileSource.generic_u8string()),
                "could not persist external animation archive")) return 1;
    }
    renegade::bridge::RetargetHumanoidAnimationsCommand externalCommand(
        scene, rig, fileSource.generic_u8string(), true);
    if (!Require(externalCommand.Execute() &&
            externalCommand.Result().createdAnimations.size() == 1,
            "file-backed native humanoid retarget should produce one baked clip")) return 1;
    const auto retargetedClip = externalCommand.Result().createdAnimations.front();
    const auto* externalBaked = scene.animations.GetComponent(retargetedClip);
    if (!Require(externalBaked != nullptr && externalBaked->channels.size() == 1 &&
            externalBaked->samplers.size() == 1 &&
            scene.animation_datas.Contains(externalBaked->samplers.front().data),
            "file-backed clip must own its baked data")) return 1;
    if (!Require(fs::remove(fileSource, ioError) && !ioError,
            "source file must become unavailable for redo test")) return 1;
    externalCommand.Undo();
    if (!Require(!scene.animations.Contains(retargetedClip),
            "native retarget Undo must remove the new clip")) return 1;
    if (!Require(externalCommand.Execute() &&
            scene.animations.Contains(retargetedClip),
            "native retarget Redo must restore without the external file")) return 1;
    const fs::path reopenedPath = proofDirectory / "baked-character.wiscene";
    {
        wi::Archive writer(reopenedPath.generic_u8string(), false, false);
        if (!Require(writer.IsOpen(), "cannot open baked-character archive")) return 1;
        writer.SetCompressionEnabled(true);
        scene.Serialize(writer);
        if (!Require(writer.SaveFile(reopenedPath.generic_u8string()),
                "cannot save baked-character scene")) return 1;
    }
    wi::scene::Scene reopened;
    {
        wi::Archive reader(reopenedPath.generic_u8string(), true);
        if (!Require(reader.IsOpen(), "cannot reopen baked-character archive")) return 1;
        reopened.Serialize(reader);
    }
    bool bakedReopened = false;
    for (std::size_t index = 0; index < reopened.animations.GetCount(); ++index)
    {
        const auto& animation = reopened.animations[index];
        if (animation.channels.size() != 1 || animation.samplers.size() != 1)
            continue;
        const auto* reopenedData = reopened.animation_datas.GetComponent(
            animation.samplers.front().data);
        if (reopenedData != nullptr && reopenedData->keyframe_times.size() == 2 &&
            reopenedData->keyframe_data.size() == 8 &&
            animation.samplers.front().scene == nullptr)
            bakedReopened = true;
    }
    if (!Require(bakedReopened,
            "physical WISCENE reopen must retain baked animation without source")) return 1;
    fs::remove_all(proofDirectory, ioError);

    std::cout << "Phase 7B humanoid mapping tests passed\n";
    return 0;
}
