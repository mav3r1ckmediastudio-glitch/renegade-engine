#pragma once
#include "renegade/bridge/ScreenLiquid.h"
#include "renegade/bridge/IdentityService.h"
namespace renegade::bridge
{
    // Inspect/import white-on-black or transparent mask artwork, deriving the
    // effective liquid bounds. Failure leaves the supplied settings unchanged.
    bool InspectScreenLiquidMask(const std::string& root,const std::string& relative,
        ScreenLiquid& settings,std::string& error);
    bool ImportScreenLiquidMask(const std::string& root,const StableId& project,
        const std::string& source,ScreenLiquid& settings,std::string& error);
}
