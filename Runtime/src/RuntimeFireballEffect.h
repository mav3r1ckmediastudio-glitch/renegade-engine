#pragma once
#include "renegade/bridge/ProjectileEffectRuntime.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace renegade::runtime {
// Presentation only: native GPU particles retain world-space positions after birth.
// Parenting moves their emission point, never their buoyancy or existing trail.
struct RuntimeFireballEffect {
    wi::ecs::Entity root=wi::ecs::INVALID_ENTITY, light=wi::ecs::INVALID_ENTITY;
    std::array<wi::ecs::Entity,4> particles{wi::ecs::INVALID_ENTITY,wi::ecs::INVALID_ENTITY,wi::ecs::INVALID_ENTITY,wi::ecs::INVALID_ENTITY};
    float seconds=0, charge=0, frameSeconds=0;
    bool airborne=false, previousPositionValid=false;
    XMFLOAT3 previousPosition{}, previousVelocity{};
    bool Ready() const {return root!=wi::ecs::INVALID_ENTITY;}
    void Reset(wi::scene::Scene& scene) {if(Ready())scene.Entity_Remove(root);*this={};}
    static float Noise(float x,float y) {
        const int ix=int(std::floor(x)),iy=int(std::floor(y));
        float fx=x-ix,fy=y-iy;fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy);
        const auto h=[](int a,int b){std::uint32_t v=std::uint32_t(a)*73856093u ^ std::uint32_t(b)*19349663u;v^=v>>13;v*=1274126177u;v^=v>>16;return float(v&65535u)/65535.f;};
        const float a=h(ix,iy)*(1-fx)+h(ix+1,iy)*fx,b=h(ix,iy+1)*(1-fx)+h(ix+1,iy+1)*fx;
        return a*(1-fy)+b*fy;
    }
    // Original animated turbulence masks; no disk/private asset dependency.
    static const wi::graphics::Texture& Texture(bool smoke) {
        static std::array<wi::graphics::Texture,2> textures;
        auto& texture=textures[smoke?1:0];auto* device=wi::graphics::GetDevice();
        if(!device || texture.IsValid())return texture;
        constexpr unsigned tile=128,side=tile*4;
        std::vector<unsigned char> pixels(side*side*4,0);
        for(unsigned frame=0;frame<16;++frame)for(unsigned y=0;y<tile;++y)for(unsigned x=0;x<tile;++x) {
            const float px=(x+.5f)/tile*2-1,py=(y+.5f)/tile*2-1,t=frame/16.f*6.2831853f;
            const float warp=.12f*std::sin(py*7+t)+.06f*std::sin(py*15-t*2);
            const float radius=(px+warp)*(px+warp)+py*py;
            const float noise=.52f*Noise(px*3+std::sin(t),py*3+std::cos(t))+.27f*Noise(px*7+std::sin(t)*2,py*7+std::cos(t)*2)+.14f*Noise(px*15+std::sin(t)*3,py*15+std::cos(t)*3)+.07f*Noise(px*31,py*31);
            const float edge=std::clamp((1-radius+.2f*(noise-.5f))*3,0.f,1.f);
            const float opacity=edge*edge*std::clamp(smoke?noise:((noise-.15f)*1.5f),0.f,1.f);
            const float heat=std::clamp((1-radius)*.7f+noise*.4f,0.f,1.f);
            const unsigned i=(((frame/4)*tile+y)*side+(frame%4)*tile+x)*4;
            pixels[i]=smoke?210:255;pixels[i+1]=smoke?210:static_cast<unsigned char>(70+heat*180);
            pixels[i+2]=smoke?210:static_cast<unsigned char>(8+heat*95);pixels[i+3]=static_cast<unsigned char>(opacity*255);
        }
        wi::graphics::TextureDesc desc;desc.width=desc.height=side;desc.format=wi::graphics::Format::R8G8B8A8_UNORM;
        desc.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
        wi::graphics::SubresourceData data;data.data_ptr=pixels.data();data.row_pitch=side*4;data.slice_pitch=pixels.size();
        device->CreateTexture(&desc,&data,&texture);return texture;
    }
    bool Create(wi::scene::Scene& scene,wi::ecs::Entity parent) {
        if(!scene.transforms.Contains(parent))return false;
        root=scene.Entity_CreateTransform("Fireball presentation");Attach(scene,parent);
        const std::array<bridge::ProjectileEffectKind,4> kinds={bridge::ProjectileEffectKind::Flame,bridge::ProjectileEffectKind::Flame,bridge::ProjectileEffectKind::Smoke,bridge::ProjectileEffectKind::Sparks};
        for(unsigned i=0;i<4;++i) {
            particles[i]=bridge::CreateProjectileEffectEmitter(scene,{kinds[i],{},.1f,50,.3f},{});
            scene.Component_Attach(particles[i],root,true);
            auto& t=*scene.transforms.GetComponent(particles[i]);t.ClearTransform();t.UpdateTransform();
            auto& e=*scene.emitters.GetComponent(particles[i]);e.SetMaxParticleCount(i==2?128:256);
            if(wi::graphics::GetDevice())e.SetOpacityCurveControl(.03f,.2f);e.normal_factor=i==3?1.4f:.12f;e.random_factor=i==3?1.f:.6f;
            e.random_life=.3f;e.random_color=i==3?.35f:.05f;e.rotation=i==3?0:.7f;
            e.scaleX=e.scaleY=1;e.SetSorted(i==2);e.SetFrameBlendingEnabled(true);
            e.shaderType=i==2?wi::EmittedParticleSystem::SOFT_LIGHTING:wi::EmittedParticleSystem::SOFT;
            auto& m=*scene.materials.GetComponent(particles[i]);
            m.baseColor=i==2?XMFLOAT4{.22f,.20f,.18f,.32f}:
                i==0?XMFLOAT4{1,.75f,.4f,.75f}:i==1?XMFLOAT4{1,.45f,.10f,.6f}:XMFLOAT4{1,.30f,.025f,.85f};
            m.emissiveColor=i==2?XMFLOAT4{}:XMFLOAT4{1,.7f,.3f,1};
            if(i==1 || i==2){const auto& texture=Texture(i==2);if(texture.IsValid())m.textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(texture);e.framesX=e.framesY=4;e.frameCount=16;e.frameRate=24;}
            m.SetDirty();
        }
        light=scene.Entity_CreateLight("Fireball glow",{},XMFLOAT3{1,.28f,.035f},1,2,wi::scene::LightComponent::POINT);
        scene.Component_Attach(light,root,true);auto& t=*scene.transforms.GetComponent(light);t.ClearTransform();t.UpdateTransform();
        Apply(scene,0,{});return true;
    }
    void Attach(wi::scene::Scene& scene,wi::ecs::Entity parent) {
        scene.Component_Detach(root);scene.Component_Attach(root,parent,true);
        auto& t=*scene.transforms.GetComponent(root);t.ClearTransform();t.UpdateTransform();
    }
    void Pause(wi::scene::Scene& scene,bool paused) {for(const auto e:particles)if(auto* p=scene.emitters.GetComponent(e))p->SetPaused(paused);}
    void Hand(wi::scene::Scene& scene,float dt,bool growing) {
        frameSeconds=std::isfinite(dt)?dt:0;Pause(scene,frameSeconds<=0);if(frameSeconds<=0)return;
        seconds+=dt;if(growing)charge=std::min(1.f,charge+dt/2.5f);
        Apply(scene,dt,{});
    }
    void Flight(wi::scene::Scene& scene,float dt,const XMFLOAT3& velocity) {
        frameSeconds=std::isfinite(dt)?dt:0;Pause(scene,frameSeconds<=0);if(frameSeconds<=0)return;seconds+=dt;Apply(scene,dt,velocity);
    }
    void Apply(wi::scene::Scene& scene,float,const XMFLOAT3& velocity) {
        const bool flying=velocity.x*velocity.x+velocity.y*velocity.y+velocity.z*velocity.z>.00001f;
        airborne=flying;
        const float growth=.22f+.78f*(charge*charge*(3-2*charge));
        const float flicker=1+.02f*std::sin(seconds*7)+.01f*std::sin(seconds*11);
        const std::array<float,4> sizes={.045f,.075f,.07f,.0025f};
        // Denser, individually fainter held particles preserve integrated opacity
        // while preventing a few bright births/deaths from pulsing the whole ball.
        // Native opacity is applied twice and packed to eight bits while fading.
        // Preserve alpha precision; compensate additive layers through radiance.
        const std::array<float,4> rates=flying?std::array<float,4>{100,48,16,16}:std::array<float,4>{400,192,16,16};
        const std::array<float,4> lives={flying?.045f:.09f,flying?.22f:.34f,flying?.55f:.42f,.40f};
        const std::array<float,4> inherited={1,.50f,.08f,.25f};
        const std::array<float,4> rise={0,flying?.40f:.11f,flying?.30f:.16f,.45f};
        for(unsigned i=0;i<4;++i)if(auto* e=scene.emitters.GetComponent(particles[i])) {
            e->size=sizes[i]*(i==3?.6f+.4f*growth:growth)*(flying && i<3?1.6f:1.f);
            e->count=rates[i]*(.25f+.75f*growth);e->life=lives[i];
            e->velocity={velocity.x*inherited[i],velocity.y*inherited[i]+rise[i],velocity.z*inherited[i]};
            e->gravity=i==3?XMFLOAT3{0,-1.1f,0}:XMFLOAT3{0,i==2?.2f: i==1?.35f:0,0};
            e->motionBlurAmount=i==3?.3f:0;
            auto* m=scene.materials.GetComponent(particles[i]);
            if(m){
                const std::array<XMFLOAT4,4> colors={XMFLOAT4{1,.75f,.4f,.75f},XMFLOAT4{1,.45f,.1f,.6f},XMFLOAT4{.22f,.20f,.18f,.32f},XMFLOAT4{1,.30f,.025f,.85f}};
                m->baseColor=colors[i];m->baseColor.w*=flying || i==0?1.f:.5f;
                if(!flying && i<2){m->baseColor.x*=.125f;m->baseColor.y*=.125f;m->baseColor.z*=.125f;}
                m->SetDirty();
            }
            if(m && i!=2){m->emissiveColor.w=(i==0?.20f:.14f)*(.5f+growth)*flicker;m->SetDirty();}
        }
        if(auto* l=scene.lights.GetComponent(light)){const float lightCharge=charge*charge*(3-2*charge);l->intensity=.041f*lightCharge*lightCharge*flicker;l->range=.35f+1.45f*lightCharge;}
    }
    // Wicked transforms initial velocity by its emitter world matrix. Preserve
    // the completed native palm position but remove rotation/scale for GPU emission.
    // No second animation evaluation or extra particle simulation is performed.
    void RefreshEmissionFrame(wi::scene::Scene& scene,const XMFLOAT3* eye=nullptr) {
        const auto* anchor=scene.transforms.GetComponent(particles[0]);if(!anchor)return;
        XMFLOAT3 position;XMStoreFloat3(&position,anchor->GetWorldMatrix().r[3]);
        // Palm anatomy follows the native wrist; visual clearance follows the
        // current camera direction so wrist rotation cannot orbit the fireball.
        if(!airborne && eye) {
            const auto towardEye=XMLoadFloat3(eye)-XMLoadFloat3(&position);
            if(XMVectorGetX(XMVector3LengthSq(towardEye))>.000001f)
                XMStoreFloat3(&position,XMLoadFloat3(&position)+XMVector3Normalize(towardEye)*.14f);
            if(auto* l=scene.lights.GetComponent(light))l->position=position;
        }

        XMFLOAT3 motion{},acceleration{};
        const bool advancing=frameSeconds>0 && std::isfinite(frameSeconds);
        const bool following=!airborne && previousPositionValid && advancing;
        if(following) {
            motion={(position.x-previousPosition.x)/frameSeconds,(position.y-previousPosition.y)/frameSeconds,(position.z-previousPosition.z)/frameSeconds};
            const float speed=std::sqrt(motion.x*motion.x+motion.y*motion.y+motion.z*motion.z);
            if(speed>25){const float scale=25/speed;motion.x*=scale;motion.y*=scale;motion.z*=scale;}
            acceleration={(motion.x-previousVelocity.x)/frameSeconds,(motion.y-previousVelocity.y)/frameSeconds,(motion.z-previousVelocity.z)/frameSeconds};
        }
        for(const auto entity:particles) {
            auto* e=scene.emitters.GetComponent(entity);const auto* t=scene.transforms.GetComponent(entity);if(!e || !t)continue;
            XMFLOAT3 origin;XMStoreFloat3(&origin,t->GetWorldMatrix().r[3]);
            if(!airborne && eye)origin=position;
            e->center=position;
            if(following) {
                // A uniform native acceleration transports existing particles by
                // the hand displacement while preserving their world-up motion.
                // New particles begin at the previous anchor and integrate into
                // the current palm position in the same native GPU step.
                e->velocity.x+=previousVelocity.x;e->velocity.y+=previousVelocity.y;e->velocity.z+=previousVelocity.z;
                e->gravity.x+=acceleration.x;e->gravity.y+=acceleration.y;e->gravity.z+=acceleration.z;
                origin=previousPosition;
            } else if(airborne && advancing) {
                origin.x-=e->velocity.x*frameSeconds;origin.y-=e->velocity.y*frameSeconds;origin.z-=e->velocity.z*frameSeconds;
            }
            XMStoreFloat4x4(&e->worldMatrix,XMMatrixTranslation(origin.x,origin.y,origin.z));
        }
        if(advancing){previousPosition=position;previousVelocity=motion;previousPositionValid=true;}
    }
    void Retire(wi::scene::Scene& scene) {for(const auto e:particles)if(auto* p=scene.emitters.GetComponent(e)){p->count=0;p->SetPaused(false);}if(auto* l=scene.lights.GetComponent(light))l->intensity=0;scene.Component_Detach(root);}
};
}
