#include "RuntimeImpactTextures.h"
#include <wiResourceManager.h>
#include <windows.h>

namespace renegade::runtime
{
    static const wi::graphics::Texture& EmbeddedTexture(unsigned id,const char* name,wi::Resource& resource)
    {
        static wi::graphics::Texture unavailable;
        if (!wi::graphics::GetDevice()) return unavailable;
        if (!resource.IsValid())
        {
            const auto module=GetModuleHandleW(nullptr);
            const auto entry=FindResourceW(module,MAKEINTRESOURCEW(id),RT_RCDATA);
            if(!entry)return unavailable;
            const auto size=SizeofResource(module,entry);
            const auto bytes=LockResource(LoadResource(module,entry));
            if(!bytes || !size)return unavailable;
            resource=wi::resourcemanager::Load(name,wi::resourcemanager::Flags::NONE,
                static_cast<const uint8_t*>(bytes),size);
        }
        return resource.IsValid()?resource.GetTexture():unavailable;
    }
    const wi::graphics::Texture& GetConcreteDustTexture()
    {static wi::Resource resource;return EmbeddedTexture(7301,"renegade_builtin_concrete_dust.png",resource);}
    const wi::graphics::Texture& GetBloodSprayTexture()
    {static wi::Resource resource;return EmbeddedTexture(7302,"renegade_builtin_blood_spray.png",resource);}
    const wi::graphics::Texture& GetBloodSprayAtlasTexture()
    {static wi::Resource resource;return EmbeddedTexture(7304,"renegade_builtin_blood_spray_atlas.png",resource);}
    const wi::graphics::Texture& GetBloodSplatTexture()
    {static wi::Resource resource;return EmbeddedTexture(7303,"renegade_builtin_blood_splat.png",resource);}
}
