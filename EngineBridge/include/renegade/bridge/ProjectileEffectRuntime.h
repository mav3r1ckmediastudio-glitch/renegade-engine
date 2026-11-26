#pragma once
#include "ProjectileAssetService.h"
#include <wiScene.h>
#include <array>
#include <cmath>

namespace renegade::bridge
{
    // Built-in procedural particle appearances require no external texture files.
    // Wicked owns GPU simulation/rendering; Renegade owns validated presets.
    inline wi::ecs::Entity CreateProjectileEffectEmitter(wi::scene::Scene& scene,
        const ProjectileEffectLayer& layer, const XMFLOAT3& position, bool burst = false)
    {
        using Kind = ProjectileEffectKind;
        if(layer.kind==Kind::None)return wi::ecs::INVALID_ENTITY;
        const auto entity=scene.Entity_CreateEmitter("Projectile effect",position);
        auto& emitter=*scene.emitters.GetComponent(entity);
        emitter.SetMaxParticleCount(256);
        emitter.size=layer.sizeMetres;emitter.life=layer.particleLifeSeconds;
        emitter.random_life=.35f;emitter.random_factor=.2f;emitter.normal_factor=0;
        emitter.count=burst?0:layer.particlesPerSecond;
        emitter.shaderType=wi::EmittedParticleSystem::SOFT;
        emitter.velocity=layer.kind==Kind::Smoke?XMFLOAT3{0,.35f,0}:XMFLOAT3{0,.12f,0};
        emitter.gravity=layer.kind==Kind::Sparks?XMFLOAT3{0,-2,0}:XMFLOAT3{};
        emitter.motionBlurAmount=layer.kind==Kind::Tracer?1.5f:0;
        emitter.scaleX=layer.kind==Kind::Flame?.25f:1;
        emitter.scaleY=layer.kind==Kind::Flame?.7f:1;
        emitter.rotation=.4f;emitter.random_color=.15f;
        emitter.SetCollidersDisabled(true);
        auto& material=*scene.materials.GetComponent(entity);
        material.userBlendMode=layer.kind==Kind::Smoke?wi::enums::BLENDMODE_ALPHA:wi::enums::BLENDMODE_ADDITIVE;
        material.baseColor=layer.kind==Kind::Smoke?XMFLOAT4{.18f,.18f,.2f,.45f}:
            layer.kind==Kind::Tracer?XMFLOAT4{.6f,.85f,1,1}:XMFLOAT4{1,.28f,.025f,1};
        material.emissiveColor=layer.kind==Kind::Smoke?XMFLOAT4{0,0,0,0}:XMFLOAT4{1,.35f,.06f,2};
        // A shared radial alpha mask keeps built-in effects soft without an
        // external texture dependency. CPU-only asset tests have no GPU device.
        static wi::graphics::Texture mask;
        if(auto* device=wi::graphics::GetDevice()) {
            if(!mask.IsValid()) {
                std::array<unsigned char,32*32*4> pixels{};
                for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x) {
                    const float dx=(float(x)+.5f)/16-1,dy=(float(y)+.5f)/16-1;
                    const float opacity=std::max(0.0f,1-dx*dx-dy*dy);
                    const auto i=(y*32+x)*4;
                    pixels[i]=pixels[i+1]=pixels[i+2]=255;
                    pixels[i+3]=static_cast<unsigned char>(opacity*opacity*255);
                }
                wi::graphics::TextureDesc desc;
                desc.width=desc.height=32;desc.format=wi::graphics::Format::R8G8B8A8_UNORM;
                desc.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
                wi::graphics::SubresourceData data;
                data.data_ptr=pixels.data();data.row_pitch=32*4;data.slice_pitch=pixels.size();
                device->CreateTexture(&desc,&data,&mask);
            }
            if(mask.IsValid())material.textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(mask);
        }
        material.SetDirty();
        // Impact emitters are created after the scene update. Burst() would make
        // them drawable immediately, before UpdateCPU creates indirect buffers.
        // Let Wicked activate the burst during its next normal emitter update.
        if(burst)emitter.burst_on_create=48;
        return entity;
    }
        inline XMMATRIX ProjectileVisualPose(const ProjectileAssetDocument& d,
            const ProjectileVector& velocity, const ProjectileVector& position,
            float grenadeSpinAngle=0.f) {
            if(d.grenade.enabled) {
                // Grenades tumble continuously. Aligning the mesh with velocity
                // on every bounce made it snap/flip as soon as gravity reversed.
                const auto spin=XMMatrixRotationAxis(
                    XMVector3Normalize(XMVectorSet(.73f,.11f,.67f,0)),
                    grenadeSpinAngle);
                return XMMatrixScaling(d.visualScale,d.visualScale,d.visualScale) *
                    XMMatrixRotationRollPitchYaw(
                        XMConvertToRadians(d.visualRotationDegrees[0]),
                        XMConvertToRadians(d.visualRotationDegrees[1]),
                        XMConvertToRadians(d.visualRotationDegrees[2])) *
                    spin * XMMatrixTranslation(position.x,position.y,position.z);
            }
            auto direction=XMVectorSet(velocity.x,velocity.y,velocity.z,0);
            direction=XMVectorGetX(XMVector3LengthSq(direction))>.00000001f?
                XMVector3Normalize(direction):XMVectorSet(0,0,1,0);
            const auto up=std::abs(XMVectorGetY(direction))>.99f?XMVectorSet(1,0,0,0):XMVectorSet(0,1,0,0);
            const auto facing=XMMatrixInverse(nullptr,XMMatrixLookToLH(XMVectorZero(),direction,up));
            return XMMatrixScaling(d.visualScale,d.visualScale,d.visualScale) *
                XMMatrixRotationRollPitchYaw(XMConvertToRadians(d.visualRotationDegrees[0]),
                    XMConvertToRadians(d.visualRotationDegrees[1]),XMConvertToRadians(d.visualRotationDegrees[2])) *
                facing * XMMatrixTranslation(position.x,position.y,position.z);
        }
}
