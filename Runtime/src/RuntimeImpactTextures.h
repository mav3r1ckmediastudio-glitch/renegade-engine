#pragma once
#include <wiScene.h>
#include <array>
#include <algorithm>
#include <cmath>

namespace renegade::runtime
{
    enum class ImpactTexture { Dust, Spark, Splinter, Chip, MetalMark, WoodMark, ConcreteMark, ConcreteDust, WaterDrop, GlassMark, BloodSpray, BloodSplat, BloodDrop, Count };

    enum class BuiltinImpactAtlas : unsigned {
        Blood8x8,
        ConcreteMarks,
        DirtMarks,
        GlassDebris,
        GlassMarks,
        MetalMarks,
        SkinEntryMarks,
        RockDebris,
        SkinMarks,
        StoneMarks,
        WoodDebris,
        WoodMarks,
        Count
    };
    // Original Renegade art, embedded in the Runtime executable.
    const wi::graphics::Texture& GetBuiltinImpactAtlas(BuiltinImpactAtlas kind);

    const wi::graphics::Texture& GetConcreteDustTexture();
    const wi::graphics::Texture& GetBloodSprayTexture();
    const wi::graphics::Texture& GetBloodSprayAtlasTexture();
    const wi::graphics::Texture& GetBloodSplatTexture();

    // Built-in masks are independent of project paths and are shared by all bursts.
    inline const wi::graphics::Texture& GetImpactTexture(ImpactTexture kind)
    {
        if(kind==ImpactTexture::BloodSpray)return GetBuiltinImpactAtlas(BuiltinImpactAtlas::Blood8x8);
        if(kind==ImpactTexture::BloodSplat)return GetBloodSplatTexture();
        if(kind==ImpactTexture::ConcreteDust)return GetConcreteDustTexture();
        static std::array<wi::graphics::Texture,static_cast<unsigned>(ImpactTexture::Count)> textures;
        auto& texture=textures[static_cast<unsigned>(kind)];
        auto* device=wi::graphics::GetDevice();
        if(texture.IsValid() || !device)return texture;
        constexpr unsigned width=64;
        std::array<unsigned char,width*width*4> pixels{};
        for(unsigned y=0;y<width;++y)for(unsigned x=0;x<width;++x)
        {
            const float u=(x+.5f)/32-1,v=(y+.5f)/32-1;
            const float r=std::sqrt(u*u+v*v),angle=std::atan2(v,u);
            const float noise=.5f+.5f*std::sin(u*29+std::sin(v*23)*2)*std::sin(v*31);
            float alpha=0,shade=1;
            switch(kind)
            {
            case ImpactTexture::Dust:
                alpha=std::pow(std::max(0.f,1-r),1.6f)*(.65f+.35f*noise);
                shade=.82f+.18f*noise;break;
            case ImpactTexture::Spark:
                alpha=std::max(0.f,1-std::abs(u)*7)*std::max(0.f,1-std::abs(v));
                break;
            case ImpactTexture::Splinter:
                alpha=std::clamp((.19f*(1-std::abs(v))+.025f-std::abs(u+.06f*v))*45,0.f,1.f)*
                    std::clamp((.90f-std::abs(v))*30,0.f,1.f);
                shade=.55f+.45f*(u+.2f)/.4f;break;
            case ImpactTexture::Chip:
                {
                constexpr float polygon[][2]={{-.72f,-.45f},{-.17f,-.76f},{.62f,-.24f},{.35f,.70f},{-.50f,.55f}};
                alpha=1;
                for(unsigned edge=0;edge<5;++edge)
                {
                    const auto& a=polygon[edge];const auto& b=polygon[(edge+1)%5];
                    const float cross=(b[0]-a[0])*(v-a[1])-(b[1]-a[1])*(u-a[0]);
                    alpha=std::min(alpha,std::clamp(cross*35,0.f,1.f));
                }
                shade=.48f+.22f*(u-v)+.18f*noise;break;
                }
            case ImpactTexture::BloodDrop:
            {
                const float ellipse=std::sqrt(u*u*3.2f+v*v*1.3f);
                alpha=std::clamp((1-ellipse)*20,0.f,1.f);
                shade=.8f+.2f*std::max(0.f,1-ellipse);break;
            }
            case ImpactTexture::WaterDrop:
            {
                const float ellipse=std::sqrt(u*u*4+v*v*1.6f);
                alpha=std::clamp((1-ellipse)*18,0.f,1.f)*(.05f+.60f*std::pow(ellipse,3.f));
                const float glint=std::max(0.f,1-std::sqrt((u+.18f)*(u+.18f)+(v+.28f)*(v+.28f))*8);
                alpha=std::max(alpha,glint*.9f);shade=.68f+.32f*glint;break;
            }
            case ImpactTexture::GlassMark:
            {
                alpha=std::clamp((.12f-r)*35,0.f,1.f)*.5f;
                const auto stroke=[&](float ax,float ay,float bx,float by)
                {
                    const float dx=bx-ax,dy=by-ay;
                    const float t=std::clamp(((u-ax)*dx+(v-ay)*dy)/(dx*dx+dy*dy),0.f,1.f);
                    const float x=u-ax-t*dx,y=v-ay-t*dy;
                    return std::clamp((.024f-std::sqrt(x*x+y*y))*65,0.f,1.f);
                };
                for(unsigned ray=0;ray<7;++ray)
                {
                    const float a=ray*6.283185f/7+.12f*std::sin(float(ray*9));
                    const float bx=std::cos(a+.16f)*.42f,by=std::sin(a+.16f)*.42f;
                    const float cx=std::cos(a)*(.70f+.12f*std::sin(float(ray*3))),
                        cy=std::sin(a)*(.70f+.12f*std::sin(float(ray*3)));
                    alpha=std::max(alpha,stroke(0,0,bx,by));
                    alpha=std::max(alpha,stroke(bx,by,cx,cy));
                    alpha=std::max(alpha,stroke(bx,by,std::cos(a+.40f)*.60f,std::sin(a+.40f)*.60f)*.7f);
                }
                shade=.95f;break;
            }
            case ImpactTexture::MetalMark:
                alpha=std::clamp((.30f+.035f*std::sin(angle*7)-r)*40,0.f,1.f);
                alpha=std::max(alpha,.38f*std::clamp((.54f-r)*25,0.f,1.f)*
                    std::clamp((r-.31f)*35,0.f,1.f));
                shade=r<.3f?.16f:.8f;break;
            case ImpactTexture::WoodMark:
                alpha=std::clamp((.23f*(1-std::abs(v))+.035f-std::abs(u))*30,0.f,1.f)*
                    std::clamp((.94f-std::abs(v))*25,0.f,1.f);
                alpha=std::max(alpha,.45f*std::clamp((.035f-std::abs(u-.13f*std::sin(v*8)))*40,0.f,1.f)*
                    std::clamp((.8f-std::abs(v))*20,0.f,1.f));
                shade=.32f+.35f*noise;break;
            case ImpactTexture::ConcreteMark:
                alpha=std::clamp((.58f+.10f*std::sin(angle*7)+.05f*std::cos(angle*11)-r)*25,0.f,1.f);
                shade=r<.27f?.2f:.5f+.4f*noise;break;
            default:break;
            }
            const auto i=(y*width+x)*4;
            const auto value=static_cast<unsigned char>(std::clamp(shade,0.f,1.f)*255);
            pixels[i]=pixels[i+1]=pixels[i+2]=value;
            pixels[i+3]=static_cast<unsigned char>(std::clamp(alpha,0.f,1.f)*255);
        }
        wi::graphics::TextureDesc desc;
        desc.width=desc.height=width;desc.format=wi::graphics::Format::R8G8B8A8_UNORM;
        desc.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
        wi::graphics::SubresourceData data;
        data.data_ptr=pixels.data();data.row_pitch=width*4;data.slice_pitch=pixels.size();
        device->CreateTexture(&desc,&data,&texture);
        return texture;
    }
}
