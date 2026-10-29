#include <WickedEngine.h>
#include <mutex>
#include "MoonSurfaceData.inc"

namespace renegade::bridge
{
    int MoonSurfaceDescriptor() noexcept
    {
        auto* device=wi::graphics::GetDevice();
        if(!device) return -1; // headless metadata/save tests never create GPU resources
        static std::mutex mutex;
        const std::lock_guard<std::mutex> lock(mutex);
        static wi::Resource surface;
        if(!surface.IsValid())
            surface=wi::resourcemanager::Load("__renegade_core_moon_albedo.jpg",
                wi::resourcemanager::Flags::NONE,MoonSurfaceBytes,sizeof(MoonSurfaceBytes));
        if(!surface.IsValid() || !surface.GetTexture().IsValid()) return -1;
        return device->GetDescriptorIndex(&surface.GetTexture(),
            wi::graphics::SubresourceType::SRV,surface.GetTextureSRGBSubresource());
    }
}
