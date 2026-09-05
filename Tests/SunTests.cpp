#include <cmath>
#include <iostream>
#include <memory>

#include "renegade/bridge/SunService.h"

namespace
{
    bool NearlyEqual(const float left, const float right)
    {
        return std::abs(left - right) < 0.001f;
    }

    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }
}

int main()
{
    {
        wi::scene::Scene weatherOnlyScene;
        const auto weatherOnlyEnvironment = wi::ecs::CreateEntity();
        auto& weatherOnly =
            weatherOnlyScene.weathers.Create(weatherOnlyEnvironment);
        weatherOnly.SetRealisticSky(true);
        weatherOnlyScene.weather = weatherOnly;

        renegade::bridge::CommandService recoveryCommands;
        auto createSun =
            std::make_unique<renegade::bridge::CreateSunCommand>(
                weatherOnlyScene,
                weatherOnlyEnvironment);
        auto* createSunResult = createSun.get();
        if (!recoveryCommands.Execute(std::move(createSun)))
        {
            return Fail("Weather-only Level Sun recovery did not execute");
        }
        const auto recoveredSun = createSunResult->CreatedEntity();
        if (recoveredSun == wi::ecs::INVALID_ENTITY ||
            renegade::bridge::FindPrimarySunLight(weatherOnlyScene) !=
                recoveredSun ||
            weatherOnlyScene.weather.sunColor.x <= 0.0f ||
            !recoveryCommands.Undo() ||
            renegade::bridge::FindPrimarySunLight(weatherOnlyScene) !=
                wi::ecs::INVALID_ENTITY ||
            !recoveryCommands.Redo() ||
            renegade::bridge::FindPrimarySunLight(weatherOnlyScene) !=
                recoveredSun)
        {
            return Fail("Weather-only Level Sun recovery Undo/Redo failed");
        }
    }

    wi::scene::Scene scene;
    const auto environment = wi::ecs::CreateEntity();
    auto& weather = scene.weathers.Create(environment);
    scene.weather = weather;

    const auto sunEntity = wi::ecs::CreateEntity();
    scene.names.Create(sunEntity).name = "Sun";
    scene.transforms.Create(sunEntity);
    auto& light = scene.lights.Create(sunEntity);
    light.SetType(wi::scene::LightComponent::DIRECTIONAL);

    auto noon = renegade::bridge::CaptureSun(scene, environment);
    renegade::bridge::SetSunTime(noon, 12.0f);
    if (!NearlyEqual(noon.azimuthDegrees, 0.0f) ||
        !NearlyEqual(noon.elevationDegrees, 75.0f) ||
        noon.direction.y < 0.9f)
    {
        return Fail("midday solar path is incorrect");
    }

    renegade::bridge::CommandService commands;
    if (!commands.Execute(std::make_unique<renegade::bridge::SetSunCommand>(
            scene,
            environment,
            noon)))
    {
        return Fail("sun command did not execute");
    }
    if (!NearlyEqual(weather.sunDirection.y, noon.direction.y) ||
        !NearlyEqual(scene.weather.sunDirection.y, noon.direction.y) ||
        !NearlyEqual(light.direction.y, -noon.direction.y))
    {
        return Fail("sun state did not reach weather and directional light");
    }

    const auto dusk = renegade::bridge::MakeSunPreset(
        noon,
        renegade::bridge::SunPreset::Dusk);
    if (dusk.timeHours < 18.0f || dusk.direction.y >= 0.0f)
    {
        return Fail("dusk preset did not move the sun below the horizon");
    }

    if (!commands.Undo() || weather.sunDirection.y < 0.99f)
    {
        return Fail("sun undo did not restore the original direction");
    }
    if (!commands.Redo() || weather.sunDirection.y < 0.9f)
    {
        return Fail("sun redo did not restore midday");
    }

    auto manual = noon;
    renegade::bridge::SetSunAzimuth(manual, -90.0f);
    renegade::bridge::SetSunElevation(manual, 10.0f);
    if (!NearlyEqual(manual.timeHours, 6.0f) ||
        manual.direction.x > -0.9f || manual.direction.y < 0.1f)
    {
        return Fail("manual sun angles are incorrect");
    }

    std::cout << "PASS: sun clock, angles, light sync and Undo/Redo\n";
    return 0;
}
