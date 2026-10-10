#include "RuntimeImpactTextures.h"
#include <wiResourceManager.h>
#include <windows.h>
#include <array>

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

    const wi::graphics::Texture& GetBuiltinImpactAtlas(BuiltinImpactAtlas kind)
    {
        static wi::graphics::Texture unavailable;
        static std::array<wi::Resource,static_cast<unsigned>(BuiltinImpactAtlas::Count)> resources;
        static constexpr unsigned ids[] = {7310,7311,7312,7313,7314,7315,7316,7317,7318,7319,7320,7321};
        static constexpr const char* names[] = {
            "renegade_original_BloodImpact8x8.png",
            "renegade_original_ConcreteMarks2x2.png",
            "renegade_original_DirtMarks2x2.png",
            "renegade_original_GlassDebris2x2.png",
            "renegade_original_GlassMarks2x2.png",
            "renegade_original_MetalMarks2x2.png",
            "renegade_original_SkinEntryMarks2x2.png",
            "renegade_original_RockDebris2x2.png",
            "renegade_original_SkinMarks2x2.png",
            "renegade_original_StoneMarks2x2.png",
            "renegade_original_WoodDebris2x2.png",
            "renegade_original_WoodMarks2x2.png",
        };
        const auto index=static_cast<unsigned>(kind);
        if(index>=resources.size())return unavailable;
        return EmbeddedTexture(ids[index],names[index],resources[index]);
    }
}
