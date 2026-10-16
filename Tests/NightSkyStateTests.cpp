#include "renegade/bridge/NightSkyService.h"

#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
    bool Near(float a, float b) noexcept { return std::fabs(a - b) < 0.0001f; }
    int Fail(const char* msg) { std::fprintf(stderr, "NightSkyState: %s\n", msg); return 1; }
}

int main()
{
    using namespace renegade::bridge;
    wi::scene::Scene scene;
    const auto environment = wi::ecs::CreateEntity();
    scene.weathers.Create(environment);

    const auto legacy = CaptureNightSky(scene, environment);
    if (!Near(legacy.gradient, 0.0f) || !Near(legacy.aurora, 0.0f))
        return Fail("legacy scene must remain unchanged");

    const NightSkyState requested{0.70f, 0.25f, 0.45f};
    if (!StoreNightSky(scene, environment, requested))
        return Fail("store rejected a valid Environment");
    const auto stored = CaptureNightSky(scene, environment);
    if (!Near(stored.gradient, 0.70f) || !Near(stored.aurora, 0.25f) ||
        !Near(stored.auroraSpeed, 0.45f))
        return Fail("versioned metadata round-trip failed");

    // Native weather parameters are transient; loading metadata must re-hydrate.
    scene.weathers.GetComponent(environment)->night_sky_gradient = 0.0f;
    scene.weathers.GetComponent(environment)->aurora_intensity = 0.0f;
    RestoreNightSky(scene);
    if (!Near(scene.weathers.GetComponent(environment)->night_sky_gradient, 0.70f) ||
        !Near(scene.weathers.GetComponent(environment)->aurora_intensity, 0.25f))
        return Fail("native weather hydration failed");

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const auto safe = SanitizeNightSky(NightSkyState{nan, 10.0f, -3.0f});
    if (!Near(safe.gradient, 0.0f) || !Near(safe.aurora, 1.0f) ||
        !Near(safe.auroraSpeed, 0.0f))
        return Fail("non-finite/out-of-bounds authoring was not sanitized");

    // User-authored channel colours and horizon lift survive a v1 metadata roundtrip.
    NightSkyState painted = CaptureNightSky(scene, environment);
    painted.zenith = XMFLOAT3(0.003f,0.007f,0.015f);
    painted.horizon = XMFLOAT3(0.024f,0.020f,0.032f);
    painted.horizonGlow = 1.3f;
    painted.auroraCoverage = 0.65f;
    if (!StoreNightSky(scene, environment, painted))
        return Fail("painted night sky could not be stored");
    const auto restoredPaint = CaptureNightSky(scene, environment);
    if (!Near(restoredPaint.zenith.z, 0.015f) ||
        !Near(restoredPaint.horizon.y, 0.020f) ||
        !Near(restoredPaint.horizonGlow, 1.3f) ||
        !Near(restoredPaint.auroraCoverage, 0.65f))
        return Fail("colour and coverage round-trip failed");
    NightSkyState changed = painted;
    changed.aurora = 0.9f;
    SetNightSkyCommand command(scene, environment, changed);
    if (!command.Execute() || !Near(CaptureNightSky(scene, environment).aurora, 0.9f))
        return Fail("Night Sky command Execute failed");
    command.Undo();
    if (!Near(CaptureNightSky(scene, environment).aurora, painted.aurora))
        return Fail("Night Sky command Undo failed");
    if (!command.Execute() || !Near(CaptureNightSky(scene, environment).aurora, 0.9f))
        return Fail("Night Sky command Redo failed");
    SetNightSkyCommand noOp(scene, environment, CaptureNightSky(scene, environment));
    if (noOp.Execute()) return Fail("unchanged Night Sky should not create an Undo command");

    const auto missing = wi::ecs::CreateEntity();
    if (StoreNightSky(scene, missing, requested))
        return Fail("non-Environment accepted authoring settings");

    scene.weathers.GetComponent(environment)->night_sky_gradient = 0.0f;
    auto* metadata = scene.metadatas.GetComponent(environment);
    metadata->int_values.set(NightSkySchemaKey, 99);
    RestoreNightSky(scene);
    if (!Near(scene.weathers.GetComponent(environment)->night_sky_gradient, 0.0f))
        return Fail("unknown schema should default to legacy");
    std::puts("NightSkyState: metadata, hydration, validation and legacy checks passed");
    return 0;
}
