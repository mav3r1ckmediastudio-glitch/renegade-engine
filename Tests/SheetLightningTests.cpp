#include "renegade/bridge/SheetLightningService.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace
{
    int Fail(const char* why)
    {
        std::cerr << "G4 sheet lightning FAIL: " << why << '\n';
        return 1;
    }
    bool Near(float a, float b)
    {
        return std::abs(a - b) < 0.0001f;
    }
}

int main()
{
    using namespace renegade::bridge;
    const XMFLOAT3 observer{20.0f, 1.7f, -40.0f};

    SheetLightningController off;
    off.Configure({});
    for (unsigned i = 0; i < 150; ++i)
        off.Advance(0.1f, observer, 750.0f, 6500.0f);
    if (off.Frame().eventId != 0 || off.Frame().flashing)
        return Fail("disabled sheet lightning unexpectedly active");

    SheetLightningSettings settings;
    settings.enabled = true;
    settings.eventsPerMinute = 24.0f;
    settings.seed = 1243;
    SheetLightningController a, b;
    a.Configure(settings);
    b.Configure(settings);
    if (!Near(a.TimeUntilNextEvent(), b.TimeUntilNextEvent()))
        return Fail("seed did not repeat event schedule");

    bool sawPulse = false;
    bool sawDarkGap = false;
    bool sawSpatialSeparation = false;
    std::uint64_t last = 0;
    for (unsigned frame = 0; frame < 260; ++frame)
    {
        a.Advance(0.05f, observer, 750.0f, 6500.0f);
        b.Advance(0.05f, observer, 750.0f, 6500.0f);
        const auto& one = a.Frame();
        const auto& two = b.Frame();
        if (one.eventId != two.eventId ||
            one.flashing != two.flashing)
            return Fail("timing nondeterministic");
        for (unsigned i = 0; i < 3; ++i)
        {
            if (!Near(one.cells[i].power, two.cells[i].power) ||
                !Near(one.cells[i].position.x, two.cells[i].position.x) ||
                !Near(one.cells[i].position.y, two.cells[i].position.y) ||
                !Near(one.cells[i].position.z, two.cells[i].position.z))
                return Fail("spatial pulse nondeterministic");
            if (!std::isfinite(one.cells[i].power) ||
                one.cells[i].power < 0.0f ||
                one.cells[i].power > 1.0f)
                return Fail("pulse energy out of bounds");
            if (one.cells[i].power > 0.05f)
                sawPulse = true;
        }
        if (one.eventId > 0 &&
            one.cells[0].power == 0.0f &&
            one.cells[1].power == 0.0f &&
            one.cells[2].power == 0.0f)
            sawDarkGap = true;
        if (one.eventId != 0 && one.eventId != last)
        {
            last = one.eventId;
            if (std::hypot(one.cells[0].position.x-one.cells[2].position.x,
                           one.cells[0].position.z-one.cells[2].position.z) > 100.0f)
                sawSpatialSeparation = true;
            for (const auto& cell : one.cells)
                if (cell.position.y < 150.0f ||
                    cell.rangeMetres > 2400.0f ||
                    cell.rangeMetres < 200.0f)
                    return Fail("cell outside bounded cloud volume");
        }
    }
    if (last < 2 || !sawPulse || !sawDarkGap || !sawSpatialSeparation)
        return Fail("no useful clustered flashes or quiet intervals");

    const auto oldEvent = a.Frame().eventId;
    a.Advance(std::numeric_limits<float>::infinity(),
              observer, 750.0f, 6500.0f);
    a.Advance(-1.0f, observer, 750.0f, 6500.0f);
    if (a.Frame().eventId != oldEvent)
        return Fail("invalid or paused game delta advanced scheduler");

    settings.eventsPerMinute = std::numeric_limits<float>::quiet_NaN();
    settings.intensity = std::numeric_limits<float>::infinity();
    settings.clusterSpreadMetres = -5.0f;
    a.Configure(settings);
    if (!std::isfinite(a.TimeUntilNextEvent()) ||
        a.TimeUntilNextEvent() < 0.0f)
        return Fail("invalid settings not sanitized");
    std::cout << "PASS: opt-in volumetric sheet lightning scheduling, "
                 "spatial clusters, dark gaps, deterministic timing and pause\n";
    return 0;
}
