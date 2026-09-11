#include "renegade/bridge/AnimationService.h"
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"

#include <cmath>
#include <iostream>
#include <memory>

namespace
{
    bool Check(const bool condition, const char* message)
    {
        if (condition)
            return true;
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }

    bool NearlyEqual(const float a, const float b)
    {
        return std::fabs(a - b) <= 0.0001f;
    }
}

int main()
{
    using namespace renegade::bridge;

    bool ok = true;
    wi::scene::Scene scene;

    const auto root = scene.Entity_CreateTransform("Animated Character");
    const auto payload = scene.Entity_CreateTransform("Imported Payload");
    const auto animationEntity = scene.Entity_CreateTransform("Walk");
    const auto unrelatedAnimationEntity =
        scene.Entity_CreateTransform("Unrelated Animation");
    scene.Component_Attach(payload, root, true);
    scene.Component_Attach(animationEntity, payload, true);

    auto& metadata = scene.metadatas.Create(root);
    metadata.string_values.set(
        ReusableAssetInstanceIdMetadataKey,
        "asset-animation-test-001");
    metadata.int_values.set(
        ReusableAssetInstanceVersionMetadataKey,
        ReusableAssetInstanceVersion);

    auto& animation = scene.animations.Create(animationEntity);
    animation.start = 1.0f;
    animation.end = 3.5f;
    animation.timer = 1.75f;
    animation.last_update_time = animation.timer;
    animation.amount = 0.35f;
    animation.speed = 1.25f;
    animation.SetLooped();
    animation.RootMotionOff();
    animation.Play();

    auto& unrelated = scene.animations.Create(unrelatedAnimationEntity);
    unrelated.start = 0.0f;
    unrelated.end = 8.0f;
    unrelated.amount = 1.0f;

    const auto clips = InspectAnimationsForSelection(scene, payload);
    ok &= Check(clips.size() == 1u,
        "selected reusable hierarchy did not isolate its native animation");
    if (!clips.empty())
    {
        ok &= Check(clips.front().entity == animationEntity,
            "wrong native animation was resolved for reusable hierarchy");
        ok &= Check(clips.front().name == "Walk",
            "native animation entity name was not preserved");
        ok &= Check(clips.front().playing,
            "native playing state was not reported");
        ok &= Check(NearlyEqual(clips.front().length, 2.5f),
            "native animation length was not reported from Wicked");
    }

    const auto authoredBeforePreview =
        CaptureAnimationAuthoredState(scene, animationEntity);
    ok &= Check(PauseAnimationPreview(scene, animationEntity),
        "preview pause failed");
    ok &= Check(!animation.IsPlaying(),
        "preview pause did not call native Wicked pause");
    ok &= Check(AnimationAuthoredStateEquals(
            authoredBeforePreview,
            CaptureAnimationAuthoredState(scene, animationEntity)),
        "preview pause mutated authored animation state");

    ok &= Check(ScrubAnimationPreview(scene, animationEntity, 99.0f),
        "preview scrub failed");
    ok &= Check(NearlyEqual(animation.timer, animation.end),
        "preview scrub did not clamp to authored end");
    ok &= Check(AnimationAuthoredStateEquals(
            authoredBeforePreview,
            CaptureAnimationAuthoredState(scene, animationEntity)),
        "preview scrub mutated authored animation state");

    ok &= Check(!PlayAnimationPreview(scene, animationEntity),
        "plain preview play should reject an already-ended clip");
    ok &= Check(PlayAnimationPreviewFromStart(scene, animationEntity),
        "play-from-start preview failed");
    ok &= Check(animation.IsPlaying() &&
            NearlyEqual(animation.timer, animation.start),
        "play-from-start did not reset native Wicked playback to authored start");
    ok &= Check(NearlyEqual(animation.speed, authoredBeforePreview.speed),
        "preview playback changed authored speed");

    ok &= Check(StopAnimationPreview(scene, animationEntity),
        "preview stop failed");
    ok &= Check(!animation.IsPlaying() &&
            NearlyEqual(animation.timer, animation.start),
        "preview stop did not pause at authored start");

    AnimationAuthoredState requested = authoredBeforePreview;
    requested.start = 0.5f;
    requested.end = 4.0f;
    requested.amount = 0.8f;
    requested.speed = 2.0f;
    requested.playbackMode = AnimationPlaybackMode::PingPong;
    requested.rootMotion = true;

    CommandService commands;
    ok &= Check(commands.Execute(
            std::make_unique<SetAnimationAuthoredStateCommand>(
                scene,
                animationEntity,
                authoredBeforePreview,
                requested)),
        "animation authored-state command failed");
    ok &= Check(NearlyEqual(animation.start, 0.5f) &&
            NearlyEqual(animation.end, 4.0f) &&
            NearlyEqual(animation.amount, 0.8f) &&
            NearlyEqual(animation.speed, 2.0f),
        "animation authored-state command did not apply native values");
    ok &= Check(animation.IsPingPong() && !animation.IsLooped(),
        "animation command did not apply native ping-pong mode");
    ok &= Check(animation.IsRootMotion(),
        "animation command did not apply native root-motion flag");

    ok &= Check(commands.Undo(), "animation authored-state Undo failed");
    ok &= Check(AnimationAuthoredStateEquals(
            authoredBeforePreview,
            CaptureAnimationAuthoredState(scene, animationEntity)),
        "animation authored-state Undo did not restore exact state");

    ok &= Check(commands.Redo(), "animation authored-state Redo failed");
    ok &= Check(AnimationAuthoredStateEquals(
            SanitizeAnimationAuthoredState(requested),
            CaptureAnimationAuthoredState(scene, animationEntity)),
        "animation authored-state Redo did not restore edited state");

    AnimationAuthoredState unsafe;
    unsafe.start = 5.0f;
    unsafe.end = 2.0f;
    unsafe.amount = 3.0f;
    unsafe.speed = 99.0f;
    const auto safe = SanitizeAnimationAuthoredState(unsafe);
    ok &= Check(NearlyEqual(safe.end, safe.start),
        "animation range sanitizer allowed end before start");
    ok &= Check(NearlyEqual(safe.amount, 1.0f),
        "animation blend sanitizer did not clamp to native range");
    ok &= Check(NearlyEqual(safe.speed, 4.0f),
        "animation speed sanitizer did not clamp creator range");

    if (!ok)
        return 1;

    std::cout << "Phase 7A native animation bridge tests passed.\n";
    return 0;
}
