#include "renegade/bridge/NightSkyService.h"
#include "../Studio/src/EnvironmentSectionLayout.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SunService.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <limits>

// Wicked weather data lives inside a GPU constant buffer. HLSL packs every
// float4 on a 16-byte boundary: verify the corresponding native C++ layout.
static_assert(offsetof(ShaderScene, weather) == 112);
static_assert(offsetof(ShaderWeather, stars) == 28);
static_assert(offsetof(ShaderWeather, night_sky_gradient) == 64);
static_assert(offsetof(ShaderWeather, night_zenith) == 80);
static_assert(offsetof(ShaderWeather, night_horizon) == 96);
static_assert(offsetof(ShaderWeather, aurora_coverage) == 112);
static_assert(offsetof(ShaderWeather, sky_rotation_sin) == 128);
static_assert(offsetof(ShaderWeather, sky_exposure) == 136);
static_assert(offsetof(ShaderWeather, rain_color) == 160);
static_assert(offsetof(ShaderWeather, fog) == 176);
static_assert(offsetof(ShaderWeather, atmosphere) == 272);
#include "NightSkyGraphicsProof.h"
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

int main(int argc, char** argv)
{
    if (argc >= 3 && std::string(argv[1]) == "--graphics-proof")
        return RunNightSkyGraphicsProof(argc, argv);
    std::fprintf(stderr, "CXX WEATHER ABI: scene.weather=%zu stars=%zu gradient=%zu zenith=%zu horizon=%zu coverage=%zu sky_rotation=%zu fog=%zu atmosphere=%zu sizeof_weather=%zu scene_size=%zu\n",
        offsetof(ShaderScene, weather), offsetof(ShaderWeather, stars),
        offsetof(ShaderWeather, night_sky_gradient), offsetof(ShaderWeather, night_zenith),
        offsetof(ShaderWeather, night_horizon), offsetof(ShaderWeather, aurora_coverage),
        offsetof(ShaderWeather, sky_rotation_sin), offsetof(ShaderWeather, fog),
        offsetof(ShaderWeather, atmosphere), sizeof(ShaderWeather), sizeof(ShaderScene));
    if (argc == 4 && std::string(argv[1]) == "--make-preview-project")
    {
        const auto parent = std::filesystem::absolute(std::filesystem::u8path(argv[2]));
        const auto fixture = std::filesystem::absolute(std::filesystem::u8path(argv[3]));
        if (!std::filesystem::is_directory(parent) || !std::filesystem::is_regular_file(fixture))
            Fail("preview parent folder or night fixture is missing");
        renegade::bridge::ProjectService projects;
        projects.Initialize((parent / ".night-sky-preview-recents.json").generic_u8string());
        if (!projects.CreateProject(parent.generic_u8string(),
                "Renegade Night Sky Visual Test", fixture.generic_u8string()))
            Fail(projects.LastError().c_str());
        renegade::bridge::ProjectMetadata verified;
        std::string verifyError;
        if (!projects.InspectProject(projects.CurrentProject().descriptorPath, verified, verifyError))
            Fail(verifyError.c_str());
        std::printf("NIGHT SKY PROJECT CREATED: %s\n", verified.descriptorPath.c_str());
        std::fflush(stdout);
        std::_Exit(0);
    }    if (argc == 3 && std::string(argv[1]) == "--verify-fixture")
    {
        wi::Archive archive(argv[2], true, false);
        if (!archive.IsOpen()) Fail("preview fixture cannot be opened from disk");
        wi::scene::Scene loaded;
        loaded.Serialize(archive);
        if (loaded.weathers.GetCount() != 1)
            Fail("preview fixture has no Environment weather entity");
        const auto environment = loaded.weathers.GetEntity(0);
        const auto authored = renegade::bridge::CaptureNightSky(loaded, environment);
        const auto* nativeWeather = loaded.weathers.GetComponent(environment);
        if (!Near(authored.gradient, 0.85f) || !Near(authored.aurora, 0.75f) ||
            !Near(authored.auroraCoverage, 0.65f) || !Near(nativeWeather->stars, 0.85f) ||
            nativeWeather->sunDirection.y > -0.85f)
            Fail("preview fixture has incorrect or missing midnight sky settings");
        renegade::bridge::RestoreNightSky(loaded);
        if (!Near(loaded.weathers.GetComponent(environment)->aurora_intensity, 0.75f))
            Fail("preview fixture did not hydrate aurora into the renderer");
        std::puts("NightSky preview fixture: disk reopen, midnight sun, stars and aurora PASSED");
        std::fflush(stdout);
        std::_Exit(0);
    }
    if (argc == 3 && std::string(argv[1]) == "--verify-saved-scene")
    {
        wi::Archive archive(argv[2], true, false);
        if (!archive.IsOpen()) Fail("saved preview scene cannot be reopened");
        wi::scene::Scene loaded;
        loaded.Serialize(archive);
        if (!loaded.weathers.GetCount()) Fail("saved preview has no Environment");
        renegade::bridge::RestoreNightSky(loaded);
        for (std::size_t i = 0; i < loaded.weathers.GetCount(); ++i)
        {
            const auto entity = loaded.weathers.GetEntity(i);
            const auto authored = renegade::bridge::CaptureNightSky(loaded, entity);
            const auto* native = loaded.weathers.GetComponent(entity);
            if (!Near(native->night_sky_gradient, authored.gradient) ||
                !Near(native->aurora_intensity, authored.aurora) ||
                !Near(native->aurora_coverage, authored.auroraCoverage) ||
                !Near(native->aurora_speed, authored.auroraSpeed) ||
                !Near(native->night_zenith.x, authored.zenith.x) ||
                !Near(native->night_zenith.y, authored.zenith.y) ||
                !Near(native->night_zenith.z, authored.zenith.z) ||
                !Near(native->night_horizon.x, authored.horizon.x) ||
                !Near(native->night_horizon.y, authored.horizon.y) ||
                !Near(native->night_horizon.z, authored.horizon.z) ||
                !Near(native->night_horizon.w, authored.horizonGlow) ||
                !std::isfinite(native->skyExposure) || !std::isfinite(native->stars))
                Fail("saved preview failed renderer hydration");
            std::printf("Saved Environment: gradient=%g aurora=%g speed=%g coverage=%g stars=%g glow=%g\n",
                authored.gradient, authored.aurora, authored.auroraSpeed,
                authored.auroraCoverage, native->stars, authored.horizonGlow);
        }
        std::puts("Saved preview scene reopened and renderer settings hydrated successfully");
        std::fflush(stdout);
        std::_Exit(0);
    }
    if (argc == 3 && std::string(argv[1]) == "--fixture")
    {
        namespace fs = std::filesystem;
        const fs::path destination = fs::absolute(fs::u8path(argv[2]));
        std::error_code ec;
        fs::create_directories(destination.parent_path(), ec);
        if (ec) Fail("could not create Night Sky fixture directory");
        wi::scene::Scene fixture;
        const auto environment = wi::ecs::CreateEntity();
        fixture.names.Create(environment) = "Night Sky Preview - Midnight Aurora";
        auto& weather = fixture.weathers.Create(environment);
        weather.SetRealisticSky(true);
        weather.SetRealisticSkyAerialPerspective(true);
        weather.skyExposure = 0.85f;
        weather.stars = 0.85f;
        weather.sunDirection = XMFLOAT3(0.0f, -0.96f, 0.28f);
        renegade::bridge::NightSkyState night;
        night.gradient = 0.85f;
        night.aurora = 0.75f;
        night.auroraCoverage = 0.65f;
        night.auroraSpeed = 0.35f;
        if (!renegade::bridge::StoreNightSky(fixture, environment, night))
            Fail("could not set Night Sky fixture values");
        // Match the real editor lifecycle: a serialized directional Sun owns
        // the same midnight direction as Weather, so opening Environment is read-only.
        if (renegade::bridge::EnsurePrimarySunLight(fixture, environment) == wi::ecs::INVALID_ENTITY)
            Fail("could not create fixture sun");
        auto midnight = renegade::bridge::CaptureSun(fixture, environment);
        renegade::bridge::SetSunTime(midnight, 0.0f);
        if (!renegade::bridge::ApplySun(fixture, environment, midnight))
            Fail("could not apply fixture midnight sun");
        fixture.weather = *fixture.weathers.GetComponent(environment);
        wi::Archive archive;
        fixture.Serialize(archive);
        if (!archive.SaveFile(destination.generic_u8string()))
            Fail("could not save Night Sky preview WISCENE");
        std::printf("NightSky preview WISCENE: %s\n", destination.generic_u8string().c_str());
        std::fflush(stdout);
        // Gate4SceneFixture convention: avoid unrelated Wicked process teardown.
        std::_Exit(0);
    }
    if (argc != 1) Fail("usage: RenegadeNightSkyTests [--fixture|--verify-fixture <path.wiscene>]");
    // Every disclosure combination: collapsed children consume no rows and
    // the next header follows exactly, including the last group's action rows.
    const std::size_t counts[] = {7, 5, 4, 1, 5, 20, 7, 4, 4};
    for (unsigned mask = 0; mask < 512; ++mask)
    {
        float top = 44.0f;
        std::size_t rows = 9;
        for (unsigned i = 0; i < 9; ++i)
        {
            const bool expanded = (mask & (1u << i)) != 0;
            const auto bounds = renegade::studio::MeasureEnvironmentSection(top, expanded, counts[i]);
            if (!Near(bounds.headerTop, top) || !Near(bounds.contentTop, top + 34.0f) ||
                bounds.nextTop < bounds.contentTop)
                Fail("Environment disclosure overlaps or leaves a header gap");
            top = bounds.nextTop;
            if (expanded) rows += counts[i];
        }
        if (!Near(top, 44.0f + rows * 34.0f))
            Fail("Environment collapsed children left empty scroll rows");
    }
    using namespace renegade::bridge;
    // Opening Environment repairs a weather-only scene by creating a sun.
    // That operation must not silently turn its authored night into midday.
    {
        wi::scene::Scene legacyNight;
        const auto entity = wi::ecs::CreateEntity();
        auto& weather = legacyNight.weathers.Create(entity);
        weather.sunDirection = XMFLOAT3(0.0f, -0.96f, 0.28f);
        legacyNight.weather = weather;
        const auto before = CaptureSun(legacyNight, entity);
        CreateSunCommand createSun(legacyNight, entity);
        if (!createSun.Execute()) Fail("weather-only sun repair failed");
        const auto after = CaptureSun(legacyNight, entity);
        const auto lightUp = legacyNight.transforms.GetComponent(after.lightEntity)->GetUp();
        if (!Near(lightUp.y, before.direction.y) || !Near(lightUp.z, before.direction.z))
            Fail("repaired sun transform would replace night on the next native update");
        if (!Near(before.direction.y, after.direction.y) ||
            !Near(before.direction.z, after.direction.z))
            Fail("opening Environment replaced authored nighttime direction");
        createSun.Undo();
        if (FindPrimarySunLight(legacyNight) != wi::ecs::INVALID_ENTITY ||
            !Near(CaptureSun(legacyNight, entity).direction.y, before.direction.y))
            Fail("sun repair Undo lost original weather-only direction");
        if (!createSun.Execute() ||
            !Near(CaptureSun(legacyNight, entity).direction.y, before.direction.y))
            Fail("sun repair Redo lost authored night direction");
        legacyNight.Clear();
    }
    wi::scene::Scene scene;
    const auto environment = wi::ecs::CreateEntity();
    scene.weathers.Create(environment);
    auto* baselineWeather = scene.weathers.GetComponent(environment);
    baselineWeather->stars = 0.83f;
    baselineWeather->skyExposure = 1.7f;

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

    // Reopen actual file bytes, rather than relying only on the memory archive.
    {
        const std::string path = "NightSkyState-roundtrip.wiscene";
        wi::Archive output;
        scene.Serialize(output);
        if (!output.SaveFile(path)) Fail("disk save failed");
        wi::Archive input(path, true, false);
        if (!input.IsOpen()) Fail("disk reopen failed");
        wi::scene::Scene reopened;
        reopened.Serialize(input);
        RestoreNightSky(reopened);
        const auto loadedEntity = reopened.weathers.GetEntity(0);
        const auto* loadedWeather = reopened.weathers.GetComponent(loadedEntity);
        if (!Near(CaptureNightSky(reopened, loadedEntity).aurora, 0.90f) ||
            !Near(loadedWeather->stars, 0.83f) || !Near(loadedWeather->skyExposure, 1.7f))
            Fail("disk reopen changed native stars/exposure or lost aurora");
        reopened.Clear();
        std::filesystem::remove(path);
    }
    ClearNightSky(scene, environment);
    if (!Near(scene.weathers.GetComponent(environment)->stars, 0.83f) ||
        !Near(scene.weathers.GetComponent(environment)->skyExposure, 1.7f))
        Fail("disabling extensions changed native stars or sky exposure");
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
