#include "renegade/bridge/ImportService.h"

#include <cmath>
#include <iostream>
#include <utility>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "RenegadeImportTests: " << message << '\n';
        return 1;
    }

    void PopulateSyntheticRigScene(wi::scene::Scene& scene)
    {
        const auto bone = wi::ecs::CreateEntity();
        scene.transforms.Create(bone);

        const auto armatureEntity = wi::ecs::CreateEntity();
        auto& armature = scene.armatures.Create(armatureEntity);
        armature.boneCollection.push_back(bone);
        armature.inverseBindMatrices.push_back(XMFLOAT4X4(
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1));

        const auto meshEntity = wi::ecs::CreateEntity();
        auto& mesh = scene.meshes.Create(meshEntity);
        mesh.armatureID = armatureEntity;
        mesh.vertex_boneindices.push_back(XMUINT4(0, 0, 0, 0));
        mesh.vertex_boneweights.push_back(XMFLOAT4(1, 0, 0, 0));
        mesh.vertex_boneindices2.push_back(XMUINT4(0, 0, 0, 0));
        mesh.vertex_boneweights2.push_back(XMFLOAT4(0, 0, 0, 0));

        const auto animationDataEntity = wi::ecs::CreateEntity();
        auto& animationData = scene.animation_datas.Create(animationDataEntity);
        animationData.keyframe_times.push_back(0.0f);
        animationData.keyframe_times.push_back(1.0f);
        animationData.keyframe_data.push_back(0.0f);
        animationData.keyframe_data.push_back(1.0f);

        const auto animationEntity = wi::ecs::CreateEntity();
        auto& animation = scene.animations.Create(animationEntity);
        wi::scene::AnimationComponent::AnimationChannel channel;
        channel.path = wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION;
        channel.target = bone;
        channel.samplerIndex = 0;
        animation.channels.push_back(channel);
        wi::scene::AnimationComponent::AnimationSampler sampler;
        sampler.data = animationDataEntity;
        animation.samplers.push_back(sampler);
    }
}

int main()
{
    renegade::bridge::ImportService imports;

    // LP07 Gate 1 owns deterministic format classification separately from
    // actual conversion. FBX and GLB/GLTF are enabled; the remaining formats
    // are recognized so they fail explicitly rather than falling through to
    // the wrong converter.
    using renegade::bridge::ModelSourceFormat;
    if (renegade::bridge::ImportService::ClassifyModelSourceFormat("Hero.FBX") !=
            ModelSourceFormat::Fbx ||
        renegade::bridge::ImportService::ClassifyModelSourceFormat("scene.gltf") !=
            ModelSourceFormat::Gltf ||
        renegade::bridge::ImportService::ClassifyModelSourceFormat("scene.GLB") !=
            ModelSourceFormat::Glb ||
        renegade::bridge::ImportService::ClassifyModelSourceFormat("mesh.obj") !=
            ModelSourceFormat::Obj ||
        renegade::bridge::ImportService::ClassifyModelSourceFormat("cloud.ply") !=
            ModelSourceFormat::Ply ||
        renegade::bridge::ImportService::ClassifyModelSourceFormat("avatar.vrm") !=
            ModelSourceFormat::Vrm ||
        renegade::bridge::ImportService::ClassifyModelSourceFormat("motion.vrma") !=
            ModelSourceFormat::Vrma ||
        renegade::bridge::ImportService::ClassifyModelSourceFormat("readme.txt") !=
            ModelSourceFormat::Unknown)
    {
        return Fail("model source classification is not deterministic");
    }

    if (!renegade::bridge::ImportService::IsModelSourceFormatSupported(
            ModelSourceFormat::Fbx) ||
        !renegade::bridge::ImportService::IsModelSourceFormatSupported(
            ModelSourceFormat::Gltf) ||
        !renegade::bridge::ImportService::IsModelSourceFormatSupported(
            ModelSourceFormat::Glb) ||
        renegade::bridge::ImportService::IsModelSourceFormatSupported(
            ModelSourceFormat::Obj) ||
        renegade::bridge::ImportService::IsModelSourceFormatSupported(
            ModelSourceFormat::Ply) ||
        renegade::bridge::ImportService::IsModelSourceFormatSupported(
            ModelSourceFormat::Vrm) ||
        renegade::bridge::ImportService::IsModelSourceFormatSupported(
            ModelSourceFormat::Vrma))
    {
        return Fail("LP07 Gate 1 format support boundary is incorrect");
    }

    wi::scene::Scene fixture;
    const auto root = wi::ecs::CreateEntity();
    fixture.names.Create(root) = "Imported Root";
    fixture.transforms.Create(root);
    const auto summary = renegade::bridge::ImportService::Summarize(fixture);
    if (summary.names != 1 || summary.transforms != 1 || summary.meshes != 0)
    {
        return Fail("scene structural summary is incorrect");
    }

    // The LP07 evidence summary covers the rig/animation payload that simple
    // component counts cannot prove. This is deliberately synthetic and
    // graphics-free; real FBX conversion is exercised by the graphics-backed
    // Gate 1 proof rather than this headless harness.
    {
        renegade::bridge::ImportedModelEvidence staticEvidence;
        staticEvidence.rigAnimationFingerprint = 1234;
        if (staticEvidence.HasRigOrAnimationPayload())
        {
            return Fail("static model bookkeeping was mistaken for rig/animation payload");
        }

        // Wicked Scene is a large aggregate, so keep these fixtures off the
        // Windows worker stack. Two separately-created scenes also guarantee
        // different transient ECS IDs without invoking renderer/job-system
        // dependent WISCENE serialization in this headless unit test.
        auto rigged = wi::allocator::make_shared_single<wi::scene::Scene>();
        PopulateSyntheticRigScene(*rigged);
        const auto evidence =
            renegade::bridge::ImportService::SummarizeModelEvidence(*rigged);
        if (evidence.skinnedMeshes != 1 ||
            evidence.primaryInfluenceVertices != 1 ||
            evidence.secondaryInfluenceVertices != 1 ||
            evidence.armatureBones != 1 ||
            evidence.animationChannels != 1 ||
            evidence.animationSamplers != 1 ||
            evidence.animationData != 1 ||
            evidence.animationKeyframes != 2 ||
            evidence.animationValues != 2 ||
            evidence.rigAnimationFingerprint == 0 ||
            !evidence.HasRigOrAnimationPayload())
        {
            return Fail("rig/animation evidence summary did not capture synthetic payload");
        }
        rigged.reset();

        auto semanticTwin = wi::allocator::make_shared_single<wi::scene::Scene>();
        PopulateSyntheticRigScene(*semanticTwin);
        const auto twinEvidence =
            renegade::bridge::ImportService::SummarizeModelEvidence(*semanticTwin);
        if (!(evidence == twinEvidence))
        {
            return Fail("semantic rig/animation evidence depended on transient ECS entity IDs");
        }

        // Conversely, exact payload edits must remain fail-closed even when
        // every high-level count is unchanged. Guard manager counts before
        // indexing so corruption reports a normal test failure, never an AV.
        if (semanticTwin->meshes.GetCount() == 0)
            return Fail("synthetic semantic twin lost its mesh");
        auto* twinMesh = semanticTwin->meshes.GetComponent(
            semanticTwin->meshes.GetEntity(0));
        if (twinMesh == nullptr || twinMesh->vertex_boneweights.empty())
            return Fail("synthetic semantic twin lost its mesh weights");
        twinMesh->vertex_boneweights[0].x = 0.75f;
        const auto changedWeightEvidence =
            renegade::bridge::ImportService::SummarizeModelEvidence(*semanticTwin);
        if (evidence.rigAnimationFingerprint ==
                changedWeightEvidence.rigAnimationFingerprint ||
            evidence.skinWeightFingerprint == changedWeightEvidence.skinWeightFingerprint ||
            evidence.skinIndexFingerprint != changedWeightEvidence.skinIndexFingerprint ||
            evidence.animationTimesFingerprint != changedWeightEvidence.animationTimesFingerprint)
        {
            return Fail("rig evidence did not reject a changed bone weight");
        }
        twinMesh->vertex_boneweights[0].x = 1.0f;

        if (semanticTwin->armatures.GetCount() == 0)
            return Fail("synthetic semantic twin lost its armature");
        auto* twinArmature = semanticTwin->armatures.GetComponent(
            semanticTwin->armatures.GetEntity(0));
        if (twinArmature == nullptr || twinArmature->inverseBindMatrices.empty())
            return Fail("synthetic semantic twin lost its bind pose");
        twinArmature->inverseBindMatrices[0]._41 = 0.25f;
        const auto changedBindEvidence =
            renegade::bridge::ImportService::SummarizeModelEvidence(*semanticTwin);
        if (evidence.rigAnimationFingerprint ==
                changedBindEvidence.rigAnimationFingerprint ||
            evidence.inverseBindFingerprint == changedBindEvidence.inverseBindFingerprint ||
            evidence.boneHierarchyFingerprint != changedBindEvidence.boneHierarchyFingerprint ||
            evidence.animationValuesFingerprint != changedBindEvidence.animationValuesFingerprint)
        {
            return Fail("rig evidence did not reject a changed inverse bind matrix");
        }
        twinArmature->inverseBindMatrices[0]._41 = 0.0f;

        if (semanticTwin->animations.GetCount() == 0)
            return Fail("synthetic semantic twin lost its animation");
        auto* twinAnimation = semanticTwin->animations.GetComponent(
            semanticTwin->animations.GetEntity(0));
        if (twinAnimation == nullptr || twinAnimation->channels.empty())
            return Fail("synthetic semantic twin lost its target channel");
        const auto originalTarget = twinAnimation->channels[0].target;
        const auto alternateTarget = wi::ecs::CreateEntity();
        semanticTwin->names.Create(alternateTarget) = "Different Target";
        semanticTwin->transforms.Create(alternateTarget);
        twinAnimation->channels[0].target = alternateTarget;
        const auto changedTargetEvidence =
            renegade::bridge::ImportService::SummarizeModelEvidence(*semanticTwin);
        if (evidence.rigAnimationFingerprint ==
                changedTargetEvidence.rigAnimationFingerprint ||
            evidence.animationChannelFingerprint == changedTargetEvidence.animationChannelFingerprint ||
            evidence.skinWeightFingerprint != changedTargetEvidence.skinWeightFingerprint ||
            evidence.animationTimesFingerprint != changedTargetEvidence.animationTimesFingerprint)
        {
            return Fail("animation evidence did not reject a changed channel target");
        }
        twinAnimation->channels[0].target = originalTarget;

        if (semanticTwin->animation_datas.GetCount() == 0)
            return Fail("synthetic semantic twin lost its animation data");
        auto* twinData = semanticTwin->animation_datas.GetComponent(
            semanticTwin->animation_datas.GetEntity(0));
        if (twinData == nullptr || twinData->keyframe_data.size() < 2)
            return Fail("synthetic semantic twin lost keyframe data");
        twinData->keyframe_data[1] = 0.5f;
        const auto changedAnimationEvidence =
            renegade::bridge::ImportService::SummarizeModelEvidence(*semanticTwin);
        if (evidence.rigAnimationFingerprint ==
                changedAnimationEvidence.rigAnimationFingerprint ||
            evidence.animationValuesFingerprint == changedAnimationEvidence.animationValuesFingerprint ||
            evidence.animationTimesFingerprint != changedAnimationEvidence.animationTimesFingerprint ||
            evidence.skinWeightFingerprint != changedAnimationEvidence.skinWeightFingerprint)
        {
            return Fail("animation evidence did not reject changed keyframe data");
        }
    }

    // ResolveScaleFactor: Original/Meters are always a no-op for a
    // glTF-mandated metres source; Centimeters/Inches are fixed literal
    // multipliers; Automatic normalizes the union of every mesh's local
    // vertex-position bounds to the 2 m target extent.
    {
        wi::scene::Scene empty;
        const float originalFactor = renegade::bridge::ImportService::
            ResolveScaleFactor(
                renegade::bridge::ModelScaleMode::Original,
                empty);
        const float metersFactor = renegade::bridge::ImportService::
            ResolveScaleFactor(
                renegade::bridge::ModelScaleMode::Meters,
                empty);
        if (originalFactor != 1.0f || metersFactor != 1.0f)
        {
            return Fail("Original/Meters scale mode did not resolve to 1.0");
        }

        const float centimetersFactor = renegade::bridge::ImportService::
            ResolveScaleFactor(
                renegade::bridge::ModelScaleMode::Centimeters,
                empty);
        if (centimetersFactor != 0.01f)
        {
            return Fail("Centimeters scale mode did not resolve to 0.01");
        }

        const float inchesFactor = renegade::bridge::ImportService::
            ResolveScaleFactor(
                renegade::bridge::ModelScaleMode::Inches,
                empty);
        if (inchesFactor != 0.0254f)
        {
            return Fail("Inches scale mode did not resolve to 0.0254");
        }

        const float emptyAutomaticFactor = renegade::bridge::ImportService::
            ResolveScaleFactor(
                renegade::bridge::ModelScaleMode::Automatic,
                empty);
        if (emptyAutomaticFactor != 1.0f)
        {
            return Fail("Automatic scale mode did not fall back to 1.0 for an empty scene");
        }

        wi::scene::Scene withMesh;
        const auto meshEntity = wi::ecs::CreateEntity();
        auto& mesh = withMesh.meshes.Create(meshEntity);
        mesh.vertex_positions.push_back(XMFLOAT3(-10.0f, -10.0f, -10.0f));
        mesh.vertex_positions.push_back(XMFLOAT3(10.0f, 10.0f, 10.0f));
        const float automaticFactor = renegade::bridge::ImportService::
            ResolveScaleFactor(
                renegade::bridge::ModelScaleMode::Automatic,
                withMesh);
        if (std::abs(automaticFactor - 0.1f) > 0.0001f)
        {
            return Fail("Automatic scale mode did not normalize the largest mesh extent");
        }
        const auto measured = renegade::bridge::ImportService::MeasureModelBounds(
            withMesh);
        if (!measured.valid || measured.Extents().y != 20.0f)
        {
            return Fail("model bounds did not retain the actual source height");
        }
        const float humanFactor = renegade::bridge::ImportService::
            ResolveScaleFactorForTargetHeight(1.82f, withMesh);
        if (std::abs(humanFactor - 0.091f) > 0.0001f)
        {
            return Fail("1.82 m scale preset did not derive from actual model height");
        }
        const float groundedY = renegade::bridge::ImportService::
            ResolveGroundedPlacementY(3.0f, measured, automaticFactor);
        if (std::abs(groundedY - 4.0f) > 0.0001f)
        {
            return Fail("surface placement did not offset the scaled model bottom");
        }
        if (renegade::bridge::ImportService::ResolveGroundedPlacementY(
                3.0f, {}, automaticFactor) != 3.0f)
        {
            return Fail("surface placement did not fail closed for missing bounds");
        }
    }

    return 0;
}
