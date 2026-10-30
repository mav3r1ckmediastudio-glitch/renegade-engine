#include "renegade/bridge/ForkLightningService.h"
#include "renegade/bridge/SheetLightningService.h"
#include <cmath>
#include <iostream>
#include <limits>

namespace
{
    int Fail(const char* detail)
    {
        std::cerr << "G4 fork lightning FAIL: " << detail << '\n';
        return 1;
    }
    bool Near(float a,float b,float eps=0.0001f)
    {
        return std::abs(a-b)<eps;
    }
    bool Near(const XMFLOAT3& a,const XMFLOAT3& b)
    {
        return Near(a.x,b.x)&&Near(a.y,b.y)&&Near(a.z,b.z);
    }
}

int main()
{
    using namespace renegade::bridge;
    const XMFLOAT3 observer{10.0f,1.8f,-25.0f}, facing{1.0f,0.0f,0.2f};
    ForkLightningController disabled;
    disabled.Configure({});
    for(int i=0;i<150;++i)disabled.Advance(0.1f,observer,facing);
    if(disabled.Frame().id!=0)return Fail("disabled weather spawned a bolt");

    ForkLightningSettings settings;
    settings.enabled=true;
    settings.seed=73823;
    settings.eventsPerMinute=24.0f;
    ForkLightningController a,b;
    a.Configure(settings);b.Configure(settings);
    if(!Near(a.TimeToNextStrike(),b.TimeToNextStrike()))
        return Fail("initial interval not deterministic");
    std::uint64_t previous=0;
    bool sawVisible=false,sawDark=false;
    for(int frame=0;frame<430;++frame)
    {
        a.Advance(0.05f,observer,facing);
        b.Advance(0.05f,observer,facing);
        const auto& one=a.Frame();
        const auto& two=b.Frame();
        if(one.id!=two.id || one.visible!=two.visible ||
           !Near(one.brightness,two.brightness))
            return Fail("strike timing differs for identical seeds");
        if(one.visible)sawVisible=true;
        if(one.id!=0 && !one.visible)sawDark=true;
        if(one.id!=0 && one.id!=previous)
        {
            previous=one.id;
            if(one.segments.size()<129 || one.segments.size()>159)
                return Fail("unbounded or under-branched geometry");
            if(!Near(one.segments.front().from,one.origin) ||
               !Near(one.segments[83].to,one.contact))
                return Fail("main channel not rooted in cloud/ground");
            const float drop=one.origin.y-one.contact.y;
            if(drop<1800.0f || drop>4500.0f)
                return Fail("cloud-to-ground vertical separation incorrect");
            for(std::size_t i=0;i<one.segments.size();++i)
            {
                const auto& s=one.segments[i];
                const auto& other=two.segments[i];
                if(!Near(s.from,other.from)||!Near(s.to,other.to) ||
                   !Near(s.radiusMetres,other.radiusMetres)||
                   !std::isfinite(s.from.x)||!std::isfinite(s.to.y)||
                   s.radiusMetres<=0.0f||s.radiusMetres>2.0f||
                   !std::isfinite(s.energy)||s.energy<0.0f||s.energy>1.0f||
                   s.from.y<s.to.y)
                    return Fail("non-deterministic/disconnected/inverted segment");
                if(i<84 && i>0 && !Near(s.from,one.segments[i-1].to))
                    return Fail("trunk has a visible disconnected gap");
                if(s.order>1)
                    return Fail("unexpected geometry branch order");
                if(i>=84 && s.order==1)
                {
                    bool connected=false;
                    for(std::size_t prior=0;prior<i;++prior)
                    {
                        if(Near(s.from,one.segments[prior].to) ||
                           Near(s.from,one.segments[prior].from))
                        {
                            connected=true;
                            break;
                        }
                    }
                    if(!connected)return Fail("free-floating unrooted fork");
                }
            }
            for(std::size_t i=84;i<one.segments.size();++i)
            {
                const auto& s=one.segments[i];
                if(s.order==1 && s.radiusMetres>0.8f)
                    return Fail("secondary fork too thick");
            }
        }
    }
    if(previous<5 || !sawVisible || !sawDark)
        return Fail("events not intermittent or not created");
    const auto count=a.Frame().id;
    a.Advance(-1.0f,observer,facing);
    a.Advance(std::numeric_limits<float>::infinity(),observer,facing);
    if(a.Frame().id!=count)
        return Fail("paused or invalid delta advanced the weather clock");

    // Regression: G4 fork cloud-glow can NEVER outlive the visible return
    // strokes, even though ordinary sheet lightning has a later third pulse.
    // Reuse the approved sheet formation but the FORK owns pulse timing.
    ForkLightningSettings linkedForkSettings;
    linkedForkSettings.enabled=true;
    linkedForkSettings.eventsPerMinute=1.0f; // avoid another event in this test
    linkedForkSettings.seed=4878;
    ForkLightningController linkedFork;
    linkedFork.Configure(linkedForkSettings);
    SheetLightningSettings linkedSheetSettings;
    linkedSheetSettings.enabled=true;
    linkedSheetSettings.intensity=0.14f;
    linkedSheetSettings.clusterSpreadMetres=720.0f;
    linkedSheetSettings.cloudLightRangeMetres=2340.0f;
    SheetLightningController linkedSheet;
    linkedSheet.Configure(linkedSheetSettings);
    bool sawFirst=false,sawSecond=false,sawGap=false,sawFinished=false;
    std::uint64_t observedId=0;
    for(int tick=0;tick<135;++tick)
    {
        linkedFork.Advance(0.025f,observer,facing);
        const auto& bolt=linkedFork.Frame();
        if(bolt.id && bolt.id!=observedId)
        {
            observedId=bolt.id;
            linkedSheet.TriggerAtPosition(bolt.origin,3200.0f);
        }
        if(!bolt.id)continue;
        const auto lit=linkedSheet.FrameDrivenByStrike(
            bolt.visible?bolt.brightness:0.0f,bolt.ageSeconds);
        float strongest=0.0f;
        for(const auto& cell:lit.cells)
            strongest=std::max(strongest,cell.power);
        if(bolt.visible != (strongest>0.0001f))
            return Fail("fork and in-cloud sheet flash timing diverged");
        if(bolt.ageSeconds<0.11f && strongest>0.05f)sawFirst=true;
        if(bolt.ageSeconds>=0.11f && bolt.ageSeconds<0.15f &&
           strongest<0.0001f)sawGap=true;
        if(bolt.ageSeconds>=0.15f && bolt.ageSeconds<0.265f &&
           strongest>0.05f)sawSecond=true;
        if(bolt.ageSeconds>=0.42f)
        {
            sawFinished=true;
            if(strongest>0.0001f || lit.flashing)
                return Fail("sheet lightning emitted AFTER fork disappeared");
        }
        if(bolt.ageSeconds>=0.70f)break;
    }
    if(!sawFirst || !sawGap || !sawSecond || !sawFinished ||
       observedId!=1)
        return Fail("linked flash and return-stroke phases not fully exercised");

    settings.eventsPerMinute=std::numeric_limits<float>::infinity();
    settings.cloudBaseMetres=std::numeric_limits<float>::quiet_NaN();
    a.Configure(settings);
    if(!std::isfinite(a.TimeToNextStrike()) ||
       a.TimeToNextStrike()<=0.0f)
        return Fail("nonfinite settings were not sanitized");
    std::cout<<"PASS: G4 fork geometry, tapered rooted branches, timing, pause, bounded settings\n";
    return 0;
}
