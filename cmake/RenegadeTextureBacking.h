#pragma once
#include "wiResourceManager.h"
#include <memory>

namespace wi::resourcemanager
{
    // Offsets are relative to one verified DDS within a governed container.
    // The resource and its pending jobs retain the same backing generation.
    struct GovernedTextureBacking
    {
        virtual ~GovernedTextureBacking() = default;
        virtual bool Read(size_t offset, size_t length, wi::vector<uint8_t>& data) const = 0;
    };

    Resource LoadGovernedTexture(const std::string& name, const uint8_t* filedata,
        size_t filesize, const std::string& container, size_t offset,
        std::shared_ptr<const GovernedTextureBacking> backing);
}
