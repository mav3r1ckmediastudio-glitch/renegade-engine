#include "renegade/bridge/SpecialistComponentService.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "Phase7Gate7E test failure: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    bool NearlyEqual(float a, float b)
    {
        return std::abs(a - b) <= 0.0001f;
    }
}

int main()
{
    wi::scene::Scene scene;
    const auto entity = wi::ecs::CreateEntity();
    scene.names.Create(entity) = "Specialist Test Entity";
    scene.transforms.Create(entity);

    // Native component creation remains command-backed and reversible.
    renegade::bridge::CreateSpecialistComponentCommand addHair(
        scene, entity, renegade::bridge::SpecialistComponentKind::HairParticle);
    Require(addHair.Execute(), "hair component should be created");
    Require(scene.hairs.Contains(entity), "native hair manager should own created component");
    Require(scene.materials.Contains(entity), "hair creation should supply native material when absent");
    addHair.Undo();
    Require(!scene.hairs.Contains(entity), "hair undo should remove native component");
    Require(!scene.materials.Contains(entity), "hair undo should remove command-created material");
    Require(addHair.Execute(), "hair redo should recreate native component");

    auto* hair = scene.hairs.GetComponent(entity);
    Require(hair != nullptr, "hair should exist after redo");
    auto hairState = renegade::bridge::CaptureHairParticle(*hair);
    hairState.strandCount = 4321;
    hairState.segmentCount = 4;
    hairState.billboardCount = 3;
    hairState.length = 2.25f;
    hairState.width = 0.35f;
    hairState.stiffness = 1.5f;
    hairState.drag = 0.25f;
    hairState.gravity = 0.4f;
    hairState.randomness = 0.65f;
    hairState.viewDistance = 350.0f;
    hairState.uniformity = 0.75f;
    hairState.cameraBend = false;
    hairState.atlasRects.push_back({XMFLOAT4(0.5f, 0.5f, 0.5f, 0.0f), 1.25f});
    renegade::bridge::SetHairParticleStateCommand setHair(scene, entity, hairState);
    Require(setHair.Execute(), "hair authored state should apply");
    Require(hair->strandCount == 4321, "native strand count should update");
    Require(hair->segmentCount == 4, "native segment count should update");
    Require(hair->atlas_rects.size() == 1, "native atlas variant should be authored");
    Require(NearlyEqual(hair->atlas_rects.front().texMulAdd.x, 0.5f), "atlas scale should persist");
    setHair.Undo();
    Require(hair->strandCount != 4321, "hair undo should restore previous state");

    // Force fields use Wicked's native Point/Plane component.
    scene.forces.Create(entity);
    auto forceState = renegade::bridge::CaptureForceField(*scene.forces.GetComponent(entity));
    forceState.type = wi::scene::ForceFieldComponent::Type::Plane;
    forceState.gravity = -12.5f;
    forceState.range = 42.0f;
    renegade::bridge::SetForceFieldStateCommand setForce(scene, entity, forceState);
    Require(setForce.Execute(), "force-field state should apply");
    auto* force = scene.forces.GetComponent(entity);
    Require(force->type == wi::scene::ForceFieldComponent::Type::Plane, "native force type should update");
    Require(NearlyEqual(force->gravity, -12.5f), "native force gravity should update");
    Require(NearlyEqual(force->range, 42.0f), "native force range should update");
    setForce.Undo();
    Require(force->type != wi::scene::ForceFieldComponent::Type::Plane || !NearlyEqual(force->gravity, -12.5f),
        "force undo should restore previous state");

    // Video authored loop state is testable without requiring an external MP4.
    scene.videos.Create(entity);
    auto videoState = renegade::bridge::CaptureVideoAuthoredState(*scene.videos.GetComponent(entity));
    videoState.looped = !videoState.looped;
    renegade::bridge::SetVideoAuthoredStateCommand setVideo(scene, entity, videoState);
    Require(setVideo.Execute(), "video loop state should apply without loading media");
    Require(scene.videos.GetComponent(entity)->IsLooped() == videoState.looped,
        "native VideoComponent loop flag should update");
    setVideo.Undo();
    Require(scene.videos.GetComponent(entity)->IsLooped() != videoState.looped,
        "video undo should restore previous loop state");

    // Spline flags and node entity ownership remain native and reversible.
    scene.splines.Create(entity);
    auto splineState = renegade::bridge::CaptureSpline(*scene.splines.GetComponent(entity));
    splineState.looped = true;
    splineState.filled = true;
    splineState.drawAligned = true;
    splineState.width = 3.0f;
    splineState.horizontalSubdivisions = 8;
    splineState.verticalSubdivisions = 6;
    splineState.terrainModifier = 0.4f;
    renegade::bridge::SetSplineStateCommand setSpline(scene, entity, splineState);
    Require(setSpline.Execute(), "spline authored state should apply");
    auto* spline = scene.splines.GetComponent(entity);
    Require(spline->IsLooped() && spline->IsFilled() && spline->IsDrawAligned(),
        "native spline flags should update");
    Require(NearlyEqual(spline->width, 3.0f), "native spline width should update");

    renegade::bridge::AddSplineNodeCommand addNode(scene, entity);
    Require(addNode.Execute(), "spline node should be created");
    const auto node = addNode.CreatedEntity();
    Require(node != wi::ecs::INVALID_ENTITY, "created spline node should have identity");
    Require(spline->spline_node_entities.size() == 1, "native spline should reference new node");
    Require(scene.transforms.Contains(node), "native spline node should own a transform");
    addNode.Undo();
    Require(spline->spline_node_entities.empty(), "spline node undo should remove native reference");
    Require(addNode.Execute(), "spline node redo should restore serialized node");
    Require(spline->spline_node_entities.size() == 1, "spline node redo should restore reference");

    renegade::bridge::RemoveLastSplineNodeCommand removeNode(scene, entity);
    Require(removeNode.Execute(), "last spline node should remove");
    Require(spline->spline_node_entities.empty(), "native node list should be empty after remove");
    removeNode.Undo();
    Require(spline->spline_node_entities.size() == 1, "remove-node undo should restore native node");

    // Gaussian splats are deliberately inspection-first, matching Wicked.
    const auto gaussianEntity = wi::ecs::CreateEntity();
    scene.gaussian_splats.Create(gaussianEntity);
    const auto gaussian = renegade::bridge::CaptureGaussianSplatInfo(scene, gaussianEntity);
    Require(gaussian.valid, "native Gaussian component should be detectable");
    Require(gaussian.splatCount == 0, "empty native Gaussian component should report zero splats");

    std::cout << "Phase 7E native specialist component tests passed\n";
    return EXIT_SUCCESS;
}
