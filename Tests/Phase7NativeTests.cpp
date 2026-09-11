#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "renegade/bridge/AnimationCreationService.h"
#include "renegade/bridge/AnimationService.h"
#include "renegade/bridge/SpecialistComponentService.h"

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    bool NearlyEqual(const float a, const float b)
    {
        return std::abs(a - b) < 0.001f;
    }
}

int main()
{
    using namespace renegade::bridge;

    // 7A: native AnimationComponent remains the authority and authored state
    // participates in normal CommandService Undo/Redo.
    {
        wi::scene::Scene scene;
        const auto root = wi::ecs::CreateEntity();
        scene.transforms.Create(root);
        scene.names.Create(root) = "Character";
        const auto clip = wi::ecs::CreateEntity();
        scene.transforms.Create(clip);
        scene.Component_Attach(clip, root);
        auto& animation = scene.animations.Create(clip);
        scene.names.Create(clip) = "Walk";
        animation.start = 0.0f;
        animation.end = 2.0f;
        animation.amount = 1.0f;
        animation.speed = 1.0f;

        const auto clips = CollectAnimationClips(scene, root);
        if (clips.size() != 1 || clips.front().entity != clip ||
            clips.front().name != "Walk")
        {
            return Fail("7A did not resolve a native animation below the selected hierarchy");
        }

        AnimationAuthoringState changed = CaptureAnimationAuthoring(animation);
        changed.amount = 0.25f;
        changed.speed = 1.5f;
        changed.playbackMode = AnimationPlaybackMode::PingPong;
        CommandService commands;
        if (!commands.Execute(std::make_unique<SetAnimationAuthoringCommand>(
                scene, clip, changed)) ||
            !NearlyEqual(animation.amount, 0.25f) ||
            !NearlyEqual(animation.speed, 1.5f) || !animation.IsPingPong())
        {
            return Fail("7A authored native animation state did not apply");
        }
        if (!commands.Undo() || !NearlyEqual(animation.amount, 1.0f) ||
            !animation.IsLooped())
        {
            return Fail("7A animation authoring did not undo");
        }
        if (!commands.Redo() || !animation.IsPingPong())
            return Fail("7A animation authoring did not redo");

        if (!PreviewAnimation(scene, clip, AnimationPreviewAction::PlayFromStart) ||
            !animation.IsPlaying() || !NearlyEqual(animation.timer, animation.start))
        {
            return Fail("7A preview transport did not use native animation state");
        }
        if (!PreviewAnimationTimer(scene, clip, 1.25f) ||
            !NearlyEqual(animation.timer, 1.25f))
        {
            return Fail("7A preview scrub did not reach the native timer");
        }
    }

    // 7B: automatic humanoid mapping uses the same native bone-name model and
    // stores results directly in HumanoidComponent.
    {
        wi::scene::Scene scene;
        const auto armatureEntity = wi::ecs::CreateEntity();
        scene.transforms.Create(armatureEntity);
        auto& armature = scene.armatures.Create(armatureEntity);

        const auto hips = wi::ecs::CreateEntity();
        scene.transforms.Create(hips);
        scene.names.Create(hips) = "mixamorig:Hips";
        const auto head = wi::ecs::CreateEntity();
        scene.transforms.Create(head);
        scene.names.Create(head) = "mixamorig:Head";
        armature.boneCollection = {hips, head};

        HumanoidMappingState mapping;
        std::string error;
        if (!BuildAutomaticHumanoidMapping(scene, armatureEntity, mapping, error) ||
            mapping.bones[static_cast<std::size_t>(wi::scene::HumanoidComponent::HumanoidBone::Hips)] != hips ||
            mapping.bones[static_cast<std::size_t>(wi::scene::HumanoidComponent::HumanoidBone::Head)] != head)
        {
            return Fail("7B native humanoid auto-map failed Mixamo-style names");
        }

        CommandService commands;
        if (!commands.Execute(std::make_unique<SetHumanoidMappingCommand>(
                scene, armatureEntity, mapping)) ||
            !scene.humanoids.Contains(armatureEntity))
        {
            return Fail("7B humanoid mapping command did not create native component");
        }
        if (!commands.Undo() || scene.humanoids.Contains(armatureEntity))
            return Fail("7B humanoid mapping command did not undo component creation");
        if (!commands.Redo() || !scene.humanoids.Contains(armatureEntity))
            return Fail("7B humanoid mapping command did not redo");
    }

    // 7C: native IK and Expression controls remain command-backed components.
    {
        wi::scene::Scene scene;
        const auto bone = wi::ecs::CreateEntity();
        const auto target = wi::ecs::CreateEntity();
        scene.transforms.Create(bone);
        scene.transforms.Create(target);

        InverseKinematicsState ik;
        ik.enabled = true;
        ik.target = target;
        ik.chainLength = 3;
        ik.iterations = 5;
        CommandService commands;
        if (!commands.Execute(std::make_unique<SetInverseKinematicsCommand>(
                scene, bone, ik, true)))
        {
            return Fail("7C IK command did not create native component");
        }
        const auto* nativeIk = scene.inverse_kinematics.GetComponent(bone);
        if (nativeIk == nullptr || nativeIk->target != target ||
            nativeIk->chain_length != 3 || nativeIk->iteration_count != 5)
        {
            return Fail("7C IK authoring did not reach native fields");
        }
        if (!commands.Undo() || scene.inverse_kinematics.Contains(bone))
            return Fail("7C IK creation did not undo");

        const auto face = wi::ecs::CreateEntity();
        scene.transforms.Create(face);
        if (!commands.Execute(std::make_unique<EnsureExpressionComponentCommand>(
                scene, face)) || !scene.expressions.Contains(face))
        {
            return Fail("7C expression component creation failed");
        }
        auto* expressions = scene.expressions.GetComponent(face);
        expressions->expressions.emplace_back().name = "Smile";
        ExpressionMasterState master;
        master.forceTalking = true;
        master.blinkFrequency = 0.25f;
        if (!commands.Execute(std::make_unique<SetExpressionMasterCommand>(
                scene, face, master)) || !expressions->IsForceTalkingEnabled())
        {
            return Fail("7C expression master state did not apply");
        }
        ExpressionItemState item;
        item.index = 0;
        item.weight = 0.75f;
        item.binary = true;
        if (!commands.Execute(std::make_unique<SetExpressionItemCommand>(
                scene, face, item)) ||
            !NearlyEqual(expressions->expressions[0].weight, 0.75f) ||
            !expressions->expressions[0].IsBinary())
        {
            return Fail("7C expression item state did not apply");
        }
    }

    // 7D: recording uses Wicked AnimationDataComponent rather than a Renegade
    // sequencer document, and repeat recording at the same time replaces a key.
    {
        wi::scene::Scene scene;
        const auto owner = wi::ecs::CreateEntity();
        scene.transforms.Create(owner);
        const auto target = wi::ecs::CreateEntity();
        auto& transform = scene.transforms.Create(target);
        transform.translation_local = XMFLOAT3(1, 2, 3);

        CommandService commands;
        auto create = std::make_unique<CreateNativeAnimationCommand>(
            scene, owner, "Timeline");
        auto* rawCreate = create.get();
        if (!commands.Execute(std::move(create)))
            return Fail("7D native timeline creation failed");
        const auto animationEntity = rawCreate->CreatedEntity();

        std::vector<float> current;
        std::string error;
        if (!CaptureCurrentAnimationValue(
                scene, target,
                wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION,
                current, error) || current.size() != 3)
        {
            return Fail("7D could not capture a native transform value");
        }
        if (!commands.Execute(std::make_unique<RecordAnimationKeyCommand>(
                scene, animationEntity, target,
                wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION,
                AnimationInterpolation::Linear, 0.5f, current)))
        {
            return Fail("7D native transform key recording failed");
        }
        auto channels = CollectTimelineChannels(scene, animationEntity);
        if (channels.size() != 1 || channels[0].keyCount != 1)
            return Fail("7D native channel/key count is wrong");

        transform.translation_local = XMFLOAT3(4, 5, 6);
        if (!CaptureCurrentAnimationValue(
                scene, target,
                wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION,
                current, error) ||
            !commands.Execute(std::make_unique<RecordAnimationKeyCommand>(
                scene, animationEntity, target,
                wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION,
                AnimationInterpolation::Linear, 0.5f, current)))
        {
            return Fail("7D native key replacement failed");
        }
        channels = CollectTimelineChannels(scene, animationEntity);
        if (channels[0].keyCount != 1)
            return Fail("7D repeat-time record appended instead of replacing");
        if (!commands.Undo() || CollectTimelineChannels(scene, animationEntity)[0].keyCount != 1)
            return Fail("7D key replacement undo failed");
    }

    // 7E: specialist authoring is a curated wrapper over native components.
    {
        wi::scene::Scene scene;
        const auto entity = wi::ecs::CreateEntity();
        scene.transforms.Create(entity);
        CommandService commands;

        if (!commands.Execute(std::make_unique<EnsureSpecialistComponentCommand>(
                scene, entity, SpecialistComponentKind::Hair)) ||
            !scene.hairs.Contains(entity))
        {
            return Fail("7E native hair component creation failed");
        }
        HairParticleState hair;
        hair.strandCount = 999999;
        hair.length = 99.0f;
        hair.segments = 0;
        const auto safeHair = SanitizeHairParticle(scene, hair);
        if (safeHair.strandCount != 100000 || !NearlyEqual(safeHair.length, 4.0f) ||
            safeHair.segments != 1)
        {
            return Fail("7E hair authoring did not use Wicked editor bounds");
        }
        if (!commands.Execute(std::make_unique<SetHairParticleCommand>(
                scene, entity, safeHair)) ||
            scene.hairs.GetComponent(entity)->strandCount != 100000)
        {
            return Fail("7E hair state did not reach native component");
        }

        if (!commands.Execute(std::make_unique<EnsureSpecialistComponentCommand>(
                scene, entity, SpecialistComponentKind::ForceField)))
            return Fail("7E native force field creation failed");
        ForceFieldState force;
        force.type = wi::scene::ForceFieldComponent::Type::Plane;
        force.gravity = 5.0f;
        force.range = 25.0f;
        if (!commands.Execute(std::make_unique<SetForceFieldCommand>(
                scene, entity, force)) ||
            scene.forces.GetComponent(entity)->type !=
                wi::scene::ForceFieldComponent::Type::Plane)
        {
            return Fail("7E force-field state did not reach native component");
        }

        if (!commands.Execute(std::make_unique<EnsureSpecialistComponentCommand>(
                scene, entity, SpecialistComponentKind::Spline)))
            return Fail("7E native spline creation failed");
        SplineAuthoringState spline;
        spline.looped = true;
        spline.filled = true;
        spline.width = 2.0f;
        spline.terrainModifier = 0.5f;
        if (!commands.Execute(std::make_unique<SetSplineAuthoringCommand>(
                scene, entity, spline)) ||
            !scene.splines.GetComponent(entity)->IsFilled())
        {
            return Fail("7E spline authoring did not reach native component");
        }
        auto addNode = std::make_unique<AddSplineNodeCommand>(
            scene, entity, XMFLOAT3(1, 0, 2));
        auto* rawNode = addNode.get();
        if (!commands.Execute(std::move(addNode)) ||
            rawNode->CreatedNode() == wi::ecs::INVALID_ENTITY ||
            scene.splines.GetComponent(entity)->spline_node_entities.size() != 1)
        {
            return Fail("7E spline node creation failed");
        }
    }

    // Terrain material painting edits native per-chunk blendmap layers and can
    // be captured as one command-friendly before/after state.
    {
        wi::scene::Scene scene;
        const auto terrainEntity = wi::ecs::CreateEntity();
        auto& terrain = scene.terrains.Create(terrainEntity);
        terrain.scene = &scene;
        terrain.terrainEntity = terrainEntity;
        wi::terrain::Chunk coordinate;
        coordinate.x = 0;
        coordinate.z = 0;
        auto& chunk = terrain.chunks[coordinate];
        chunk.entity = wi::ecs::CreateEntity();
        auto& transform = scene.transforms.Create(chunk.entity);
        transform.UpdateTransform();
        auto& mesh = scene.meshes.Create(chunk.entity);
        mesh.vertex_positions.resize(wi::terrain::vertexCount);
        for (int z = 0; z < wi::terrain::chunk_width; ++z)
        {
            for (int x = 0; x < wi::terrain::chunk_width; ++x)
            {
                mesh.vertex_positions[x + z * wi::terrain::chunk_width] =
                    XMFLOAT3(static_cast<float>(x), 0, static_cast<float>(z));
            }
        }

        TerrainPaintState before;
        TerrainPaintState after;
        std::string error;
        if (!PaintTerrainMaterial(
                terrain, XMFLOAT3(32, 0, 32), 10.0f, 1.0f,
                wi::terrain::MATERIAL_SLOPE, false,
                before, after, error) ||
            before.chunks.empty() || after.chunks.empty())
        {
            return Fail("7E terrain material paint produced no native blendmap edit");
        }
        const auto& pixels = terrain.chunks[coordinate]
            .blendmap_layers[wi::terrain::MATERIAL_SLOPE].pixels;
        if (*std::max_element(pixels.begin(), pixels.end()) == 0)
            return Fail("7E terrain material brush did not modify blendmap pixels");
        if (!ApplyTerrainPaint(terrain, before) ||
            *std::max_element(
                terrain.chunks[coordinate]
                    .blendmap_layers[wi::terrain::MATERIAL_SLOPE].pixels.begin(),
                terrain.chunks[coordinate]
                    .blendmap_layers[wi::terrain::MATERIAL_SLOPE].pixels.end()) != 0)
        {
            return Fail("7E terrain material paint did not restore before-state");
        }
    }

    std::cout << "Phase 7 native A-E bridge tests passed\n";
    return 0;
}
