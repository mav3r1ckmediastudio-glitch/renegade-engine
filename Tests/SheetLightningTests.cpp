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

    // Flash-reconstruction history remains responsive for a fraction of a
    // second after the emitted light ends, then returns to normal.
    SheetLightningSettings recoverySettings;
    recoverySettings.enabled = true;
    recoverySettings.eventsPerMinute = 1.0f;
    recoverySettings.seed = 7;
    SheetLightningController recovery;
    recovery.Configure(recoverySettings);
    for (int frame = 0; frame < 100 && recovery.Frame().eventId == 0; ++frame)
        recovery.Advance(0.05f, observer, 750.0f, 6500.0f);
    if (recovery.Frame().eventId != 1 ||
        !recovery.Frame().temporalRecovery)
        return Fail("first flash did not enter temporal recovery");
    for (int frame = 0; frame < 17; ++frame)
        recovery.Advance(0.05f, observer, 750.0f, 6500.0f);
    if (recovery.Frame().flashing || !recovery.Frame().temporalRecovery)
        return Fail("reconstruction recovery ends before light fades");
    for (int frame = 0; frame < 10; ++frame)
        recovery.Advance(0.05f, observer, 750.0f, 6500.0f);
    if (recovery.Frame().temporalRecovery ||
        recovery.Frame().cells[0].power > 0.0001f)
        return Fail("reconstruction recovery persists beyond safety window");

    // G4 owner refinement: a 30% larger cloud footprint must preserve
    // deterministic event positions/timing while keeping the core energy
    // equal or slightly lower; cells retain independent per-event radii.
    SheetLightningSettings narrowSettings;
    narrowSettings.enabled = true;
    narrowSettings.eventsPerMinute = 24.0f;
    narrowSettings.seed = 43012;
    narrowSettings.intensity = 0.15f;
    narrowSettings.cloudLightRangeMetres = 1800.0f;
    SheetLightningSettings wideSettings = narrowSettings;
    wideSettings.intensity = 0.14f;
    wideSettings.cloudLightRangeMetres = 2340.0f;
    SheetLightningController narrow, wider;
    narrow.Configure(narrowSettings);
    wider.Configure(wideSettings);
    for (int frame = 0; frame < 120 && narrow.Frame().eventId == 0; ++frame)
    {
        narrow.Advance(0.05f, observer, 750.0f, 6500.0f);
        wider.Advance(0.05f, observer, 750.0f, 6500.0f);
    }
    if (narrow.Frame().eventId == 0 ||
        narrow.Frame().eventId != wider.Frame().eventId)
        return Fail("30 percent width variant changed event scheduling");
    for (unsigned i = 0; i < 3; ++i)
    {
        const auto& before = narrow.Frame().cells[i];
        const auto& after = wider.Frame().cells[i];
        if (!Near(after.rangeMetres / before.rangeMetres, 1.30f) ||
            !Near(after.position.x, before.position.x) ||
            !Near(after.position.y, before.position.y) ||
            !Near(after.position.z, before.position.z))
            return Fail("30 percent width variant changed geometry other than radius");
        if (after.power > before.power + 0.0001f)
            return Fail("expanded sheet lightning unexpectedly increased peak brightness");
    }

    // Fork strike origin must use the approved sheet cluster and pulse.
    // It must not drift or schedule unrelated autonomous flashes.
    SheetLightningSettings linkedSettings;
    linkedSettings.enabled = true;
    linkedSettings.seed = 1243;
    linkedSettings.intensity = 0.14f;
    linkedSettings.clusterSpreadMetres = 720.0f;
    linkedSettings.cloudLightRangeMetres = 2340.0f;
    SheetLightningController linkedA, linkedB;
    linkedA.Configure(linkedSettings);
    linkedB.Configure(linkedSettings);
    const XMFLOAT3 strikeOrigin{4200.0f, 2850.0f, -1500.0f};
    linkedA.TriggerAtPosition(strikeOrigin, 3200.0f);
    linkedB.TriggerAtPosition(strikeOrigin, 3200.0f);
    if (linkedA.Frame().eventId != 1 ||
        linkedB.Frame().eventId != 1)
        return Fail("fork failed to trigger existing sheet cluster");
    XMFLOAT3 center{};
    for (unsigned i=0;i<3;++i)
    {
        const auto& c=linkedA.Frame().cells[i];
        const auto& d=linkedB.Frame().cells[i];
        center.x+=c.position.x / 3.0f;
        center.y+=c.position.y / 3.0f;
        center.z+=c.position.z / 3.0f;
        if (!Near(c.position.x,d.position.x) ||
            !Near(c.position.y,d.position.y) ||
            !Near(c.position.z,d.position.z) ||
            !Near(c.rangeMetres,d.rangeMetres))
            return Fail("fork-triggered approved cluster is not deterministic");
    }
    if (std::abs(center.x-strikeOrigin.x)>0.02f ||
        std::abs(center.y-strikeOrigin.y)>0.02f ||
        std::abs(center.z-strikeOrigin.z)>0.02f)
        return Fail("fork sheet cells not anchored to cloud origin");
    bool linkedPulse=false;
    for(int i=0;i<35;++i)
    {
        linkedA.Advance(0.05f,observer,750.0f,6500.0f);
        linkedB.Advance(0.05f,observer,750.0f,6500.0f);
        if(linkedA.Frame().eventId!=1 ||
            linkedB.Frame().eventId!=1)
            return Fail("fork sheet unexpectedly re-scheduled itself");
        if(linkedA.Frame().cells[0].power>0.04f)linkedPulse=true;
    }
    if(!linkedPulse || linkedA.Frame().temporalRecovery)
        return Fail("fork sheet did not reproduce normal bounded pulses");

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
