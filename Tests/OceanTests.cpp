#include <cmath>
#include <iostream>
#include <memory>

#include "renegade/bridge/OceanService.h"

namespace
{
    bool NearlyEqual(const float left, const float right)
    {
        return std::abs(left - right) < 0.0001f;
    }

    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }
}

int main()
{
    wi::scene::Scene scene;
    const auto environment = wi::ecs::CreateEntity();
    auto& weather = scene.weathers.Create(environment);
    scene.weather = weather;

    const auto original = renegade::bridge::CaptureOcean(weather);
    if (original.enabled || original.displacementMapDimension != 512 ||
        !NearlyEqual(original.patchLength, 50.0f))
    {
        return Fail("native ocean defaults were not captured");
    }

    auto coastal = renegade::bridge::MakeOceanPreset(
        original,
        renegade::bridge::OceanPreset::Coastal);
    renegade::bridge::CommandService commands;
    if (!commands.Execute(std::make_unique<renegade::bridge::SetOceanCommand>(
            scene,
            environment,
            coastal)))
    {
        return Fail("coastal preset did not execute");
    }
    const auto applied = renegade::bridge::CaptureOcean(weather);
    if (!applied.enabled ||
        !NearlyEqual(applied.waveAmplitude, coastal.waveAmplitude) ||
        !NearlyEqual(applied.windSpeed, coastal.windSpeed) ||
        !NearlyEqual(applied.waterColor.w, coastal.waterColor.w) ||
        !scene.weather.IsOceanEnabled())
    {
        return Fail("ocean state did not reach serialized and runtime weather");
    }

    if (!commands.Undo() || weather.IsOceanEnabled())
    {
        return Fail("ocean Undo did not restore disabled state");
    }
    if (!commands.Redo() || !weather.IsOceanEnabled())
    {
        return Fail("ocean Redo did not restore enabled state");
    }

    auto manual = renegade::bridge::CaptureOcean(weather);
    manual.displacementMapDimension = 700;
    manual.patchLength = 333.0f;
    manual.timeScale = 0.75f;
    manual.waveAmplitude = 725.0f;
    manual.windAzimuthDegrees = 90.0f;
    manual.windSpeed = 444.0f;
    manual.windDependency = 0.21f;
    manual.choppyScale = 2.2f;
    manual.waterColor = XMFLOAT4(0.1f, 0.2f, 0.3f, 0.4f);
    manual.extinctionColor = XMFLOAT4(0.5f, 0.6f, 0.7f, 1.0f);
    manual.waterHeight = 12.5f;
    manual.surfaceDetail = 8;
    manual.surfaceDisplacementTolerance = 5.0f;
    renegade::bridge::ApplyOcean(scene, environment, manual);
    const auto recaptured = renegade::bridge::CaptureOcean(weather);
    if (recaptured.displacementMapDimension != 512 ||
        !NearlyEqual(recaptured.patchLength, 333.0f) ||
        !NearlyEqual(recaptured.timeScale, 0.75f) ||
        !NearlyEqual(recaptured.waveAmplitude, 725.0f) ||
        !NearlyEqual(weather.oceanParameters.wind_dir.x, 0.0f) ||
        !NearlyEqual(weather.oceanParameters.wind_dir.y, 1.0f) ||
        !NearlyEqual(recaptured.windSpeed, 444.0f) ||
        !NearlyEqual(recaptured.windDependency, 0.21f) ||
        !NearlyEqual(recaptured.choppyScale, 2.2f) ||
        !NearlyEqual(recaptured.waterColor.x, 0.1f) ||
        !NearlyEqual(recaptured.waterColor.w, 0.4f) ||
        !NearlyEqual(recaptured.extinctionColor.z, 0.7f) ||
        !NearlyEqual(recaptured.waterHeight, 12.5f) ||
        recaptured.surfaceDetail != 8u ||
        !NearlyEqual(recaptured.surfaceDisplacementTolerance, 5.0f))
    {
        return Fail("manual ocean fields or resolution safety did not apply");
    }

    renegade::bridge::SetOceanCommand noOp(
        scene,
        environment,
        recaptured,
        recaptured);
    if (noOp.Execute())
    {
        return Fail("identical ocean state polluted Undo history");
    }

    const auto alien = renegade::bridge::MakeOceanPreset(
        recaptured,
        renegade::bridge::OceanPreset::Alien);
    if (!alien.enabled || NearlyEqual(alien.waterColor.x, coastal.waterColor.x) ||
        NearlyEqual(alien.extinctionColor.y, coastal.extinctionColor.y))
    {
        return Fail("alien preset did not create a distinct native ocean");
    }

    std::cout << "PASS: native FFT ocean state, presets and Undo/Redo\n";
    return 0;
}
