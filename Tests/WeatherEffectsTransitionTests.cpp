#include "renegade/bridge/WeatherEffectsTransitionService.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace
{
    bool Near(float a, float b, float tolerance = 0.0002f)
    {
        return std::abs(a - b) <= tolerance;
    }

    int Fail(const char* message)
    {
        std::cerr << "G4 weather effects transition failed: " << message << '\n';
        return 1;
    }
}

int main()
{
    using namespace renegade::bridge;

    PrecipitationState clear;
    clear.mode = PrecipitationMode::None;
    clear.intensity = 0.0f;
    clear.windAzimuthDegrees = 170.0f;
    clear.windSpeed = 1.0f;

    PrecipitationState storm = MakePrecipitationProfile(
        clear, PrecipitationMode::Rain);
    storm.intensity = 0.80f;
    storm.windAzimuthDegrees = -170.0f;
    storm.windSpeed = 10.0f;
    storm.turbulence = 9.0f;

    WeatherEffectsTransitionController blend;
    if (!blend.Start(clear, storm, 4.0f))
        return Fail("valid transition rejected");
    if (!blend.IsActive() || blend.Progress() != 0.0f ||
        blend.Current().mode != PrecipitationMode::None)
        return Fail("source endpoint or start state incorrect");

    blend.Advance(1.0f);
    auto quarter = blend.Current();
    if (!Near(blend.Progress(), 0.25f) ||
        quarter.mode != PrecipitationMode::Rain ||
        !Near(quarter.intensity, 0.125f) ||
        !(quarter.windSpeed > clear.windSpeed &&
          quarter.windSpeed < storm.windSpeed))
        return Fail("quarter easing, rain fade-in or wind interpolation failed");
    blend.Advance(1.0f);
    const auto midpoint = blend.Current();
    if (!Near(midpoint.intensity, 0.4f) ||
        !Near(std::abs(midpoint.windAzimuthDegrees), 180.0f) ||
        !Near(midpoint.windSpeed, 5.5f))
        return Fail("midpoint or shortest-arc wind angle failed");

    blend.Advance(std::numeric_limits<float>::quiet_NaN());
    blend.Advance(-1.0f);
    if (!Near(blend.Progress(), 0.5f))
        return Fail("invalid or negative delta advanced weather");

    // Cancel must hold the interpolated state without snapping to Clear.
    blend.Cancel();
    if (blend.IsActive() ||
        !Near(blend.Current().intensity, midpoint.intensity) ||
        !Near(blend.Current().windSpeed, midpoint.windSpeed))
        return Fail("cancel did not retain intermediate effect values");

    if (blend.Start(clear, storm, -1.0f) ||
        blend.Start(clear, storm, 301.0f) ||
        blend.Start(clear, storm, std::numeric_limits<float>::infinity()))
        return Fail("invalid duration accepted");
    if (!Near(blend.Current().intensity, midpoint.intensity))
        return Fail("invalid start overwrote prior state");

    if (!blend.Start(midpoint, storm, 1.0f))
        return Fail("retarget rejected");
    blend.Advance(1.0f);
    if (blend.IsActive() || blend.Current().mode != PrecipitationMode::Rain ||
        !Near(blend.Current().intensity, 0.8f))
        return Fail("retarget endpoint failed");

    if (!blend.Start(storm, clear, 0.0f) ||
        blend.IsActive() ||
        blend.Current().mode != PrecipitationMode::None)
        return Fail("instant dry weather not applied");

    // Shared single emitter cannot crossfade Rain and existing Snow
    // simultaneously: both sides of midpoint approach zero intensity.
    auto snow = MakePrecipitationProfile(storm, PrecipitationMode::Snow);
    snow.intensity = 0.5f;
    const auto rainFade = BlendWeatherEffects(storm, snow, 0.25f);
    const auto switchPoint = BlendWeatherEffects(storm, snow, 0.5f);
    const auto snowFade = BlendWeatherEffects(storm, snow, 0.75f);
    if (rainFade.mode != PrecipitationMode::Rain ||
        !(rainFade.intensity > 0.0f && rainFade.intensity < storm.intensity) ||
        switchPoint.mode != PrecipitationMode::Snow ||
        !Near(switchPoint.intensity, 0.0f) ||
        snowFade.mode != PrecipitationMode::Snow ||
        !(snowFade.intensity > 0.0f && snowFade.intensity < snow.intensity))
        return Fail("rain to legacy snow single-emitter handoff failed");

    // G4 state blending remains compatible with the pinned Wicked carrier.
    wi::scene::WeatherComponent nativeWeather;
    ApplyPrecipitation(nativeWeather, midpoint);
    const auto roundTrip = CapturePrecipitation(nativeWeather);
    if (roundTrip.mode != PrecipitationMode::Rain ||
        !Near(roundTrip.intensity, midpoint.intensity) ||
        !Near(roundTrip.windSpeed, midpoint.windSpeed))
        return Fail("native precipitation round-trip failed");

    if (BlendWeatherEffects(clear, storm, 0.0f).mode !=
            PrecipitationMode::None ||
        !Near(BlendWeatherEffects(clear, storm, 1.0f).intensity, 0.8f))
        return Fail("exact authored endpoints lost");

    std::cout << "PASS: G4 rain/wind interpolation, endpoint, pause-aware delta, "
                 "cancel, retarget, shortest wind arc, native round-trip\n";
    return 0;
}
