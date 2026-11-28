#pragma once
#include <string>
#include <cmath>
namespace renegade::bridge
{
    // Mask and liquid settings travel with the Progress widget. Glass/frame art
    // remains an ordinary, independently styled layer in its prefab hierarchy.
    struct ScreenLiquid
    {
        std::string mask;
        float left=0,right=1,top=0,bottom=1;
        float wavePixels=1.3f;
        int bubbles=5;
        bool animated=true;
        std::string decreaseSound,increaseSound;
    };
    inline bool ValidateScreenLiquidNumbers(const ScreenLiquid& value) noexcept
    {
        return std::isfinite(value.left)&&std::isfinite(value.right)&&std::isfinite(value.top)&&
            std::isfinite(value.bottom)&&std::isfinite(value.wavePixels)&&
            value.left>=0&&value.right<=1&&value.left<value.right&&value.top>=0&&
            value.bottom<=1&&value.top<value.bottom&&value.wavePixels>=0&&value.wavePixels<=16&&
            value.bubbles>=0&&value.bubbles<=32;
    }
}
