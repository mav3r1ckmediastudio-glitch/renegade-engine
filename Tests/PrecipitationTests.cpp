#include <cmath>
#include <iostream>
#include <memory>

#include "renegade/bridge/PrecipitationService.h"

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

    auto snow = renegade::bridge::MakePrecipitationProfile(
        renegade::bridge::CapturePrecipitation(weather),
        renegade::bridge::PrecipitationMode::Snow);
    if (snow.mode != renegade::bridge::PrecipitationMode::Snow ||
        snow.intensity <= 0.0f || snow.fallSpeed >= 0.5f ||
        snow.particleScale <= 0.02f || snow.splashScale != 0.0f)
    {
        return Fail("snow profile is not visually distinct from rain");
    }

    renegade::bridge::CommandService commands;
    if (!commands.Execute(
            std::make_unique<renegade::bridge::SetPrecipitationCommand>(
                scene,
                environment,
                snow)))
    {
        return Fail("snow command did not execute");
    }
    if (!NearlyEqual(weather.rain_amount, snow.intensity) ||
        !NearlyEqual(weather.rain_speed, snow.fallSpeed) ||
        !NearlyEqual(weather.rain_length, 0.0f) ||
        !NearlyEqual(weather.rain_splash_scale, 0.0f) ||
        !NearlyEqual(scene.weather.rain_amount, weather.rain_amount) ||
        renegade::bridge::CapturePrecipitation(weather).mode !=
            renegade::bridge::PrecipitationMode::Snow)
    {
        return Fail("snow profile did not reach native weather state");
    }
    if (wi::helper::GetFileNameFromPath(
            scene.rainMaterial.textures[
                wi::scene::MaterialComponent::BASECOLORMAP].name) !=
        "snowflake.dds")
    {
        return Fail("snow profile did not bind the dedicated flake texture");
    }

    if (!commands.Undo() || weather.rain_amount != 0.0f)
    {
        return Fail("snow undo did not restore disabled precipitation");
    }
    if (!commands.Redo() ||
        renegade::bridge::CapturePrecipitation(weather).mode !=
            renegade::bridge::PrecipitationMode::Snow)
    {
        return Fail("snow redo did not restore the authored profile");
    }

    snow.particleScale = 0.005f;
    renegade::bridge::ApplyPrecipitation(weather, snow);
    if (renegade::bridge::CapturePrecipitation(weather).mode !=
        renegade::bridge::PrecipitationMode::Snow)
    {
        return Fail("snow mode was lost at the minimum particle size");
    }

    auto rain = renegade::bridge::MakePrecipitationProfile(
        renegade::bridge::CapturePrecipitation(weather),
        renegade::bridge::PrecipitationMode::Rain);
    renegade::bridge::ApplyPrecipitation(weather, rain);
    scene.weather = weather;
    renegade::bridge::RefreshPrecipitationVisual(scene);
    if (renegade::bridge::CapturePrecipitation(weather).mode !=
            renegade::bridge::PrecipitationMode::Rain ||
        weather.rain_length <= 0.005f || weather.rain_splash_scale <= 0.0f)
    {
        return Fail("rain profile did not restore streak and splash behavior");
    }
    if (wi::helper::GetFileNameFromPath(
            scene.rainMaterial.textures[
                wi::scene::MaterialComponent::BASECOLORMAP].name) ==
        "snowflake.dds")
    {
        return Fail("rain profile retained the snowflake texture");
    }

    const auto preserved = renegade::bridge::CapturePrecipitation(weather);
    auto edited = preserved;
    edited.windAzimuthDegrees = 90.0f;
    edited.windSpeed = 7.0f;
    edited.turbulence = 9.0f;
    renegade::bridge::ApplyPrecipitation(weather, edited);
    if (!NearlyEqual(weather.windDirection.x, 0.0f) ||
        !NearlyEqual(weather.windDirection.z, 1.0f) ||
        !NearlyEqual(weather.windSpeed, 7.0f) ||
        !NearlyEqual(weather.windRandomness, 9.0f))
    {
        return Fail("precipitation wind controls did not apply");
    }

    // Wind is a world/environment setting, not conditional on precipitation.
    // A dry, windy scene must remain editable, serializable and undoable.
    auto dryAndWindy = renegade::bridge::CapturePrecipitation(weather);
    dryAndWindy.mode = renegade::bridge::PrecipitationMode::None;
    dryAndWindy.intensity = 0.0f;
    dryAndWindy.windAzimuthDegrees = -45.0f;
    dryAndWindy.windSpeed = 11.0f;
    dryAndWindy.turbulence = 4.0f;
    renegade::bridge::CommandService dryCommands;
    if (!dryCommands.Execute(
            std::make_unique<renegade::bridge::SetPrecipitationCommand>(
                scene, environment, dryAndWindy)))
        return Fail("dry wind command could not execute");
    const auto dryActual = renegade::bridge::CapturePrecipitation(weather);
    if (dryActual.mode != renegade::bridge::PrecipitationMode::None ||
        !NearlyEqual(weather.rain_amount, 0.0f) ||
        !NearlyEqual(dryActual.windSpeed, 11.0f) ||
        !NearlyEqual(dryActual.turbulence, 4.0f) ||
        !NearlyEqual(dryActual.windAzimuthDegrees, -45.0f) ||
        !NearlyEqual(scene.weather.windSpeed, 11.0f))
        return Fail("global wind failed with precipitation Off");
    if (!dryCommands.Undo() ||
        !NearlyEqual(weather.windSpeed, edited.windSpeed) ||
        renegade::bridge::CapturePrecipitation(weather).mode !=
            renegade::bridge::PrecipitationMode::Rain)
        return Fail("dry wind Undo failed to restore rain and wind");
    if (!dryCommands.Redo() ||
        !NearlyEqual(weather.rain_amount, 0.0f) ||
        !NearlyEqual(weather.windSpeed, 11.0f))
        return Fail("dry wind Redo failed");

    // Rain and Snow are precipitation modes, not wind presets. They must
    // never silently reset the global wind when the mode changes.
    for (auto mode : {renegade::bridge::PrecipitationMode::Rain,
                      renegade::bridge::PrecipitationMode::Snow})
    {
        const auto next = renegade::bridge::MakePrecipitationProfile(
            dryActual, mode);
        if (!NearlyEqual(next.windAzimuthDegrees, -45.0f) ||
            !NearlyEqual(next.windSpeed, 11.0f) ||
            !NearlyEqual(next.turbulence, 4.0f))
            return Fail("switching precipitation mode unexpectedly reset global wind");
    }

    std::cout << "PASS: native rain, snow profile, independent dry wind and Undo/Redo\n";
    return 0;
}
