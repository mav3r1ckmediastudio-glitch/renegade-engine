#include "renegade/bridge/WeatherTransitionService.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }
    bool Close(float a, float b)
    {
        return std::abs(a-b)<0.0001f;
    }
}

int main()
{
    using namespace renegade::bridge;
    WeatherState clear;
    clear.skyMode = WeatherState::SkyMode::Realistic;
    clear.cloudCoverage = .05f;
    clear.cloudMovementSpeed = 8.0f;
    clear.skyExposure = 1.0f;
    clear.fogDensity = .002f;
    clear.stars = .65f;
    clear.ambientIntensity = .1f;
    const WeatherState storm = MakeWeatherPreset(clear, WeatherPreset::Storm);
    WeatherTransitionController controller;
    if (!controller.Start(clear,storm,4.0f)) return Fail("valid start refused");
    if (controller.Progress()!=0.0f || !controller.IsActive() ||
        controller.Current().skyMode!=clear.skyMode)
        return Fail("transition did not start at original state");

    controller.Advance(1.0f);
    const auto quarter=controller.Current();
    if (!Close(controller.Progress(),.25f) ||
        quarter.skyMode!=WeatherState::SkyMode::RealisticWithClouds ||
        !(quarter.cloudCoverage>0.0f && quarter.cloudCoverage<storm.cloudCoverage) ||
        !(quarter.skyExposure<clear.skyExposure && quarter.skyExposure>storm.skyExposure))
        return Fail("quarter blend or cloud handover incorrect");

    controller.Advance(1.0f);
    const auto midpoint=controller.Current();
    if (!Close(midpoint.cloudCoverage,storm.cloudCoverage*.5f) ||
        !Close(midpoint.skyExposure,(clear.skyExposure+storm.skyExposure)*.5f) ||
        !Close(midpoint.stars,clear.stars))
        return Fail("midpoint blend wrong or unrelated stars modified");

    controller.Advance(2.0f);
    if (controller.IsActive() || !Close(controller.Progress(),1.0f) ||
        controller.Current().skyMode!=storm.skyMode ||
        !Close(controller.Current().cloudCoverage,storm.cloudCoverage))
        return Fail("terminal state failed to match target");

    // Retargeting mid-transition must start from the currently visible value.
    if (!controller.Start(storm,clear,10.0f)) return Fail("restart refused");
    controller.Advance(2.5f);
    const auto current=controller.Current();
    const auto scattered=MakeWeatherPreset(current,WeatherPreset::Scattered);
    if (!controller.Start(current,scattered,5.0f)) return Fail("retarget refused");
    if (!Close(controller.Current().cloudCoverage,current.cloudCoverage))
        return Fail("retarget caused a coverage jump");
    controller.Advance(1.0f);
    controller.Cancel();
    if (controller.IsActive() || !Close(controller.Current().cloudCoverage,
        controller.From().cloudCoverage))
        return Fail("cancel did not retain interpolated value");

    WeatherTransitionController invalid;
    if (invalid.Start(clear,storm,-1.0f) ||
        invalid.Start(clear,storm,std::numeric_limits<float>::quiet_NaN()) ||
        invalid.Start(clear,storm,301.0f))
        return Fail("invalid duration was accepted");
    if (!invalid.Start(clear,storm,0.0f) || invalid.IsActive() ||
        !Close(invalid.Current().cloudCoverage,storm.cloudCoverage))
        return Fail("instant transition failed");
    // No overshoot, no NaN delta, no backwards time.
    if (!controller.Start(clear,storm,1.0f)) return Fail("restart failed");
    controller.Advance(-10.0f);
    controller.Advance(std::numeric_limits<float>::quiet_NaN());
    if (!Close(controller.Progress(),0.0f)) return Fail("invalid time advanced");
    controller.Advance(10000.0f);
    if (controller.IsActive() || !Close(controller.Progress(),1.0f))
        return Fail("time overshoot");
    std::cout << "PASS: weather G3 deterministic interpolation, retargeting, cancellation, "
                 "instant apply, cloud-mode continuity and duration guards\n";
    return 0;
}
