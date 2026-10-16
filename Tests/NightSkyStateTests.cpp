#include "renegade/bridge/NightSkyService.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace
{
    bool Near(float a, float b) noexcept { return std::fabs(a - b) < 0.0001f; }
    [[noreturn]] void Fail(const char* message)
    {
        std::fprintf(stderr, "NightSkyState: %s\n", message);
        std::fflush(stderr);
        std::_Exit(1);
    }
}

int main()
{
    using namespace renegade::bridge;
    wi::scene::Scene scene;
    const auto environment = wi::ecs::CreateEntity();
    scene.weathers.Create(environment);

    const auto legacy = CaptureNightSky(scene, environment);
    if (!Near(legacy.gradient, 0.0f) || !Near(legacy.aurora, 0.0f))
        Fail("legacy scene must remain unchanged");

    const NightSkyState requested{0.70f, 0.25f, 0.45f};
    if (!StoreNightSky(scene, environment, requested))
        Fail("store rejected a valid Environment");
    const auto stored = CaptureNightSky(scene, environment);
    if (!Near(stored.gradient, 0.70f) || !Near(stored.aurora, 0.25f) ||
        !Near(stored.auroraSpeed, 0.45f))
        Fail("versioned metadata round-trip failed");

    // Native weather parameters are transient; rehydrate them from scene metadata.
    scene.weathers.GetComponent(environment)->night_sky_gradient = 0.0f;
    scene.weathers.GetComponent(environment)->aurora_intensity = 0.0f;
    RestoreNightSky(scene);
    if (!Near(scene.weathers.GetComponent(environment)->night_sky_gradient, 0.70f) ||
        !Near(scene.weathers.GetComponent(environment)->aurora_intensity, 0.25f))
        Fail("native weather hydration failed");

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const auto safe = SanitizeNightSky(NightSkyState{nan, 10.0f, -3.0f});
    if (!Near(safe.gradient, 0.0f) || !Near(safe.aurora, 1.0f) ||
        !Near(safe.auroraSpeed, 0.0f))
        Fail("non-finite/out-of-bounds authoring was not sanitized");

    // Authorable linear-HDR colours and aurora coverage.
    NightSkyState painted = CaptureNightSky(scene, environment);
    painted.zenith = XMFLOAT3(0.003f, 0.007f, 0.015f);
    painted.horizon = XMFLOAT3(0.024f, 0.020f, 0.032f);
    painted.horizonGlow = 1.3f;
    painted.auroraCoverage = 0.65f;
    if (!StoreNightSky(scene, environment, painted))
        Fail("painted night sky could not be stored");
    const auto restoredPaint = CaptureNightSky(scene, environment);
    if (!Near(restoredPaint.zenith.z, 0.015f) ||
        !Near(restoredPaint.horizon.y, 0.020f) ||
        !Near(restoredPaint.horizonGlow, 1.3f) ||
        !Near(restoredPaint.auroraCoverage, 0.65f))
        Fail("colour and coverage metadata round-trip failed");

    NightSkyState changed = painted;
    changed.aurora = 0.90f;
    SetNightSkyCommand command(scene, environment, changed);
    if (!command.Execute() || !Near(CaptureNightSky(scene, environment).aurora, 0.90f))
        Fail("Night Sky command Execute failed");
    command.Undo();
    if (!Near(CaptureNightSky(scene, environment).aurora, painted.aurora))
        Fail("Night Sky command Undo failed");
    if (!command.Execute() || !Near(CaptureNightSky(scene, environment).aurora, 0.90f))
        Fail("Night Sky command Redo failed");

    SetNightSkyCommand noOp(scene, environment, CaptureNightSky(scene, environment));
    if (noOp.Execute())
        Fail("unchanged Night Sky should not create an Undo command");

    // Native scene serializer, including metadata, entity ID remapping and
    // rehydration of the transient GPU-facing weather values after reopen.
    {
        wi::Archive archive;
        scene.Serialize(archive);
        archive.SetReadModeAndResetPos(true);
        wi::scene::Scene reopened;
        reopened.Serialize(archive);
        if (reopened.weathers.GetCount() != 1)
            Fail("WISCENE archive did not restore the Environment");
        const auto loadedEntity = reopened.weathers.GetEntity(0);
        const auto loadedSettings = CaptureNightSky(reopened, loadedEntity);
        if (!Near(loadedSettings.gradient, 0.70f) ||
            !Near(loadedSettings.aurora, 0.90f) ||
            !Near(loadedSettings.zenith.z, 0.015f) ||
            !Near(loadedSettings.horizon.y, 0.020f) ||
            !Near(loadedSettings.auroraCoverage, 0.65f))
            Fail("WISCENE archive did not persist Night Sky settings");
        RestoreNightSky(reopened);
        if (!Near(reopened.weathers.GetComponent(loadedEntity)->night_sky_gradient, 0.70f) ||
            !Near(reopened.weathers.GetComponent(loadedEntity)->aurora_intensity, 0.90f))
            Fail("WISCENE archive did not rehydrate the night renderer");
        reopened.Clear();
    }

    const auto missing = wi::ecs::CreateEntity();
    if (StoreNightSky(scene, missing, requested))
        Fail("non-Environment accepted authoring settings");

    // Unsupported future schema versions fail closed and restore the legacy sky.
    scene.weathers.GetComponent(environment)->night_sky_gradient = 0.0f;
    auto* metadata = scene.metadatas.GetComponent(environment);
    metadata->int_values.set(NightSkySchemaKey, 99);
    RestoreNightSky(scene);
    if (!Near(scene.weathers.GetComponent(environment)->night_sky_gradient, 0.0f))
        Fail("unknown metadata schema should default to legacy");

    scene.Clear();
    std::puts("NightSkyState: metadata, WISCENE archive, Undo/Redo and legacy checks passed");
    std::fflush(stdout);

    // Same convention as Tests/Gate4SceneFixture.cpp and
    // StoryFlowGate4BLevelReferenceTests.cpp: Wicked registers process-lifetime
    // static teardown hooks which stall after completed native serialization
    // fixtures. All assertions and owned scene cleanup have already completed.
    std::_Exit(0);
}
