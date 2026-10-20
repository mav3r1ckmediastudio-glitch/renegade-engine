#include "renegade/bridge/GameplayAttributeProfile.h"
#include <iostream>
#include <limits>
using namespace renegade::bridge;
int main()
{
    std::string error;
    auto fail=[&](const char* message) { std::cerr << message << ": " << error << '\n'; return 1; };
    GameplayAttributeProfile profile{GenerateStableId(),"Resource profile",
        {{"energy","Energy",50.0,0.0,100.0,10.0},
         {"active","Active",true,{},{},0.0}}};
    GameplayAttributeInstance first,second;
    if (!first.ApplyProfile(profile,{},true,error) ||
        !second.ApplyProfile(profile,{},true,error) ||
        !first.Spend("energy",20,error) ||
        std::get<double>(*second.Find("energy")) != 50)
        return fail("Instances share live state");
    if (!first.Tick(1.0,true,error) ||
        std::get<double>(*first.Find("energy")) != 30.0 ||
        !first.Tick(1.0,false,error) ||
        std::get<double>(*first.Find("energy")) != 40.0)
        return fail("Pause/regeneration contract failed");
    const auto revision = first.Revision();
    if (first.Spend("energy",1000,error) || first.Revision()!=revision ||
        first.Set("energy",false,error) ||
        first.Tick(std::numeric_limits<double>::quiet_NaN(),false,error))
        return fail("Invalid rule/cost/type changed the instance");
    profile.attributes[0].initial=60.0;
    profile.attributes[0].maximum=120.0;
    GameplayAttributeOverride local;
    local.attributeId="energy";
    local.maximum=80.0;
    if (!first.ApplyProfile(profile,{local},false,error) ||
        std::get<double>(*first.Find("energy"))!=40.0)
        return fail("Profile edit reset live state");
    if (!first.ApplyProfile(profile,{local},true,error) ||
        std::get<double>(*first.Find("energy"))!=60.0)
        return fail("Reset did not inherit initial value");
    if (!first.Tick(3.0,false,error) ||
        std::get<double>(*first.Find("energy"))!=80.0)
        return fail("Per-property override was not preserved");
    const auto before=first.Revision();
    profile.attributes[0].minimum=200.0;
    if (first.ApplyProfile(profile,{local},false,error) || first.Revision()!=before ||
        std::get<double>(*first.Find("energy"))!=80.0)
        return fail("Invalid profile update was partially committed");
    profile.attributes[0].minimum=0.0;
    if (first.ApplyProfile(profile,{local,local},false,error))
        return fail("Duplicate overrides were accepted");
    std::cout << "PASS: typed profiles, inheritance, per-property overrides, live isolation, pause and bounded rules\n";
}
