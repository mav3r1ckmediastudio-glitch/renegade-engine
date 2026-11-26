#pragma once
#include <string>
#include <array>
#include <map>

namespace renegade::bridge
{
    struct ClipJointPin
    {
        std::map<std::string,std::array<float,3>> floorOffsets;
        std::string joint;
        std::string relativeTo;
        float offsetX = 0, offsetY = 0, offsetZ = 0; // In the captured joint's local axes.
        int parents = 2;
        float captureTime = 0;
        float start = 0;
        float end = 0;
        float blend = 0;
        bool position = true;
        bool rotation = false;
    };
}
