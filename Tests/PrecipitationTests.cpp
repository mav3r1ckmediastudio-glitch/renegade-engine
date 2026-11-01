#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>

#include "renegade/bridge/PrecipitationService.h"
#include "renegade/bridge/LightningAuthoringService.h"

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

    // Authoring is opt-in, persisted on the Weather entity, and undoable.
    using namespace renegade::bridge;
    if(CaptureLightning(scene,environment).mode!=LightningMode::Off ||
       HasLightning(scene,environment))
        return Fail("new scenes should not contain enabled lightning");
    LightningAuthoringState authored;
    authored.mode=LightningMode::Both;
    authored.sheetPerMinute=15;
    authored.forkPerMinute=6;
    authored.sheetIntensity=.14f;
    authored.forkBranchDensity=1.5f;
    authored.thunder=true;
    authored.thunderSoundSet=1;
    authored.distanceDelay=true;
    authored.thunderVolume=.7f;
    CommandService lightningHistory;
    if(!lightningHistory.Execute(std::make_unique<SetLightningCommand>(scene,environment,authored)))
        return Fail("lightning command failed");
    const auto saved=CaptureLightning(scene,environment);
    if(!HasLightning(scene,environment) || !SameLightning(authored,saved))
        return Fail("lightning metadata did not roundtrip");
    if(!lightningHistory.Undo() || HasLightning(scene,environment) ||
       CaptureLightning(scene,environment).mode!=LightningMode::Off)
        return Fail("lightning Undo failed to restore absence of metadata");
    if(!lightningHistory.Redo() || !SameLightning(CaptureLightning(scene,environment),authored))
        return Fail("lightning Redo failed to restore authored controls");
    // Persist the authored settings into a real .wiscene, reopen and verify.
    const std::string lightningFixture="RenegadeLightningAuthoringRoundtrip.wiscene";
    wi::Archive lightningArchive;
    scene.Serialize(lightningArchive);
    if(!lightningArchive.SaveFile(lightningFixture))
        return Fail("lightning scene file could not be saved");
    wi::scene::Scene reopenedLightning;
    wi::Archive lightningInput(lightningFixture,true,false);
    if(!lightningInput.IsOpen())
        return Fail("lightning scene file could not be reopened");
    reopenedLightning.Serialize(lightningInput);
    if(reopenedLightning.weathers.GetCount()==0 ||
       !SameLightning(CaptureLightning(reopenedLightning,reopenedLightning.weathers.GetEntity(0)),authored))
        return Fail("lightning authored metadata lost after scene file reopening");
    reopenedLightning.Clear();
    std::filesystem::remove(lightningFixture);
    auto malformed=authored;
    malformed.sheetPerMinute=500;
    malformed.thunderSoundSet=99;
    malformed.forkBranchDensity=-3;
    if(SanitizeLightning(malformed).sheetPerMinute!=30 ||
       SanitizeLightning(malformed).thunderSoundSet!=10 ||
       SanitizeLightning(malformed).forkBranchDensity!=0)
        return Fail("lightning authoring bounds were not enforced");

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
    // Snow is NOT a textured variant of Wicked's rain emitter anymore.
    if (weather.HasNativeRain() || !weather.HasRenegadeSnow() ||
        scene.rainMaterial.textures[
            wi::scene::MaterialComponent::BASECOLORMAP].name.find(
                "snowflake.dds") != std::string::npos)
    {
        return Fail("snow still activates or modifies Wicked's rain renderer");
    }
    if (scene.emitters.GetCount() != 0)
        return Fail("headless snow created a GPU emitter prematurely");
    const auto snowVisual = renegade::bridge::MakeSnowParticleSettings(snow);
    if (snowVisual.emissionPerSecond <= 0.0f ||
        snowVisual.lifetimeSeconds < 4.0f ||
        snowVisual.sizeMetres >= 0.05f ||
        snowVisual.velocity.y >= 0.0f)
        return Fail("separate snow particle settings are missing");
    // Snow must have genuinely smaller, independently bounded sprites.
    const float expectedEmission = 1150.0f * snow.intensity +
        8300.0f * snow.intensity * snow.intensity * snow.intensity;
    if (!NearlyEqual(snowVisual.sizeMetres, 0.44f * snow.particleScale) ||
        !NearlyEqual(snowVisual.emissionPerSecond, expectedEmission) ||
        snowVisual.emissionPerSecond >= 9450.0f ||
        snowVisual.lifetimeSeconds > 4.8f)
        return Fail("snow particle size/density budget regressed");
    auto strongSnow = snow;
    strongSnow.intensity = 1.0f;
    strongSnow.windAzimuthDegrees = 90.0f;
    strongSnow.windSpeed = 12.0f;
    const auto blizzard = renegade::bridge::MakeSnowParticleSettings(strongSnow);
    if (blizzard.emissionPerSecond <= snowVisual.emissionPerSecond ||
        !NearlyEqual(blizzard.sizeMetres, snowVisual.sizeMetres) ||
        blizzard.velocity.z < 1.0f ||
        !NearlyEqual(blizzard.velocity.x, 0.0f))
        return Fail("snow intensity/wind drift does not independently scale");
    if (blizzard.emissionPerSecond < 9000.0f ||
        blizzard.emissionPerSecond * blizzard.lifetimeSeconds >= 56000.0f)
        return Fail("maximum snow not storm-dense or exceeds particle budget");
    strongSnow.windAzimuthDegrees = -90.0f;
    const auto oppositeWind = renegade::bridge::MakeSnowParticleSettings(strongSnow);
    if (oppositeWind.velocity.z > -1.0f)
        return Fail("reversing wind did not reverse snow travel");
    wi::EmittedParticleSystem snowEmitterFlag;
    snowEmitterFlag.SetWeatherSnow(true);
    if (!snowEmitterFlag.IsWeatherSnow())
        return Fail("snow-only GPU live-wind flag failed to enable");
    snowEmitterFlag.SetWeatherSnow(false);
    if (snowEmitterFlag.IsWeatherSnow())
        return Fail("snow-only GPU live-wind flag failed to disable");
    strongSnow.particleScale = 1.0f; // extreme Inspector/input value
    if (renegade::bridge::MakeSnowParticleSettings(strongSnow).sizeMetres >
        0.022001f)
        return Fail("extreme snow size reintroduced giant foreground flakes");

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
    if (!weather.HasNativeRain() || weather.HasRenegadeSnow())
        return Fail("switching back to Rain did not restore native rain");
    // Real rain has a bounded, nonlinear particle budget (not 1 million
    // raindrops/sec), and a fall speed independent of spawn-volume scale.
    wi::scene::WeatherComponent heavyRain = weather;
    heavyRain.rain_amount = 1.0f;
    if (!NearlyEqual(heavyRain.GetNativeRainEmissionRate(), 96000.0f) ||
        heavyRain.GetNativeRainEmissionRate() > 120000.0f ||
        weather.GetNativeRainEmissionRate() <= 0.0f ||
        weather.GetNativeRainEmissionRate() >= heavyRain.GetNativeRainEmissionRate() ||
        !NearlyEqual(weather.GetNativeRainFallMetresPerSecond(), 16.25f))
        return Fail("native rain emission density or world fall speed regressed");
    heavyRain.windRandomness = 20.0f;
    if (heavyRain.GetNativeRainDropSizeJitter() > 0.90001f ||
        heavyRain.GetNativeRainDropSizeJitter() < 0.89999f)
        return Fail("storm turbulence generated oversized raindrops");
    heavyRain.windRandomness = 0.0f;
    if (!NearlyEqual(heavyRain.GetNativeRainDropSizeJitter(), 0.10f))
        return Fail("calm rain size variation lost its minimum");
    heavyRain.rain_length = 0.0f;
    heavyRain.rain_splash_scale = 0.0f; // Snow must not enter rain budget.
    if (heavyRain.GetNativeRainEmissionRate() != 0.0f ||
        heavyRain.GetNativeRainDropSizeJitter() != 0.0f)
        return Fail("native rain budget leaked into snow");
    wi::EmittedParticleSystem nativeRainFlag;
    nativeRainFlag.SetWeatherRain(true);
    if (!nativeRainFlag.IsWeatherRain() || nativeRainFlag.IsWeatherSnow())
        return Fail("native rain world-velocity mode was not isolated");
    nativeRainFlag.SetWeatherRain(false);
    if (nativeRainFlag.IsWeatherRain())
        return Fail("native rain world-velocity flag did not disable");
    if (wi::helper::GetFileNameFromPath(
            scene.rainMaterial.textures[
                wi::scene::MaterialComponent::BASECOLORMAP].name) ==
        "snowflake.dds")
        return Fail("rain retained the snowflake material");

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
