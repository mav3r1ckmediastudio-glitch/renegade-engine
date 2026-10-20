#pragma once
// RESEARCH ONLY: isolated complete single-strike visual. Adds two luminous
// endpoints and real native GPU sparks to the owner-reviewed textured bolt.
// The production Storm, beam, damage and equipment remain entirely untouched.
#include "../Runtime/src/RuntimeChannelVisual.h"

namespace renegade::lightning_research {
struct CompleteStrike {
    SingleBolt bolt;
    wi::ecs::Entity sourceCore=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity sourceHalo=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity impactCore=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity impactHalo=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity impactLight=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity sourceLight=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity sparks=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity topStar=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity impactStar=wi::ecs::INVALID_ENTITY;
    wi::graphics::Texture topStarTexture{};
    std::array<wi::graphics::Texture,SingleBolt::Frames> impactStarTextures{};

    static wi::ecs::Entity Orb(wi::scene::Scene& scene,const char* name,
                               XMFLOAT4 color,XMFLOAT4 emissive) {
        auto id=scene.Entity_CreateSphere(name);
        if(auto* m=scene.materials.GetComponent(id)){
            m->baseColor=color;
            m->emissiveColor=emissive;
            m->userBlendMode=wi::enums::BLENDMODE_ADDITIVE;
            m->SetCastShadow(false);
            m->SetDirty();
        }
        if(auto* o=scene.objects.GetComponent(id)){
            o->filterMask=wi::enums::FILTER_TRANSPARENT;
            o->SetCastShadow(false);
        }
        return id;
    }
    static void At(wi::scene::Scene& scene,wi::ecs::Entity id,
                   float x,float y,float z,float size,float flattenY=1.f) {
        if(auto* t=scene.transforms.GetComponent(id)){
            t->ClearTransform();
            t->Scale(XMFLOAT3{size,size*flattenY,size});
            t->Translate(XMFLOAT3{x,y,z});
            t->UpdateTransform();
        }
    }
    // Native alpha textures replace hard-edged flare circles and make thin
    // independently animated electrical sparks visible in isolated captures.
    // This is deliberately a flare at each endpoint, not a second bolt.
    static std::vector<unsigned char> StarPixels(unsigned phase,bool top){
        constexpr unsigned n=256;
        std::vector<unsigned char> rgba(n*n*4,0);
        const float power=top?1.f:(phase<3?.07f:phase<7?1.f:phase==7?.46f:.1f);
        const float seed=float(phase*31u);
        const float radius=top?5.5f:9.f;
        for(unsigned y=0;y<n;++y)for(unsigned x=0;x<n;++x){
            const float dx=float(x)-127.5f,dy=float(y)-127.5f;
            const float r2=dx*dx+dy*dy;
            const float inner=std::exp(-r2/(radius*radius));
            const float halo=std::exp(-r2/(top?1350.f:1900.f));
            // Star-pointed flash, with wispy independent spikes and no
            // flat opaque circular geometry over the ground.
            const float cardinal=std::exp(-std::abs(dx)*.52f)*std::exp(-std::abs(dy)*.019f)
                                +std::exp(-std::abs(dy)*.52f)*std::exp(-std::abs(dx)*.022f);
            const float diag=std::exp(-std::abs(dx-dy)*.26f)*std::exp(-std::abs(dx+dy)*.018f)
                             +std::exp(-std::abs(dx+dy)*.26f)*std::exp(-std::abs(dx-dy)*.019f);
            const float star=.31f*cardinal+.20f*diag;
            const float a=std::clamp(power*(inner*.82f+halo*.14f+star*.72f),0.f,1.f);
            const auto pos=(size_t(y)*n+x)*4;
            rgba[pos]=static_cast<unsigned char>(205+50*std::clamp(inner+star,0.f,1.f));
            rgba[pos+1]=static_cast<unsigned char>(222+33*std::clamp(inner+star,0.f,1.f));
            rgba[pos+2]=255;
            rgba[pos+3]=static_cast<unsigned char>(255*a);
        }
        if(!top && phase>=3 && phase<=7){
            for(unsigned k=0;k<24;++k){
                const float angle=SingleBolt::Random(500u+phase*33u+k*89u)*XM_2PI;
                const float extent=36.f+SingleBolt::Random(99u+phase*3u+k*41u)*82.f;
                const float start=11.f+SingleBolt::Random(k*112u+phase*99u)*25.f;
                const float cx=128.f+std::cos(angle)*start,cy=128.f+std::sin(angle)*start;
                const float endx=128.f+std::cos(angle)*extent,endy=128.f+std::sin(angle)*extent;
                SingleBolt::Stroke(rgba,cx,cy,endx,endy,
                    .55f+.35f*SingleBolt::Random(k+1400u),.58f*power,true);
            }
        }
        return rgba;
    }
    static wi::ecs::Entity StarCard(wi::scene::Scene& scene,const char* name){
        const auto id=scene.Entity_CreateCube(name);
        if(auto* m=scene.materials.GetComponent(id)){
            m->baseColor={.86f,.94f,1.f,.86f};
            m->emissiveColor={.56f,.78f,1.f,2.75f};
            m->userBlendMode=wi::enums::BLENDMODE_ADDITIVE;
            m->SetCastShadow(false);m->SetDoubleSided(true);m->SetDirty();
        }
        if(auto* obj=scene.objects.GetComponent(id)){
            obj->filterMask=wi::enums::FILTER_TRANSPARENT;
            obj->SetCastShadow(false);
        }
        return id;
    }
    bool BuildStarTextures(){
        auto* gpu=wi::graphics::GetDevice();
        if(!gpu)return false;
        wi::graphics::TextureDesc desc;
        desc.width=desc.height=256;
        desc.format=wi::graphics::Format::R8G8B8A8_UNORM;
        desc.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
        const auto upload=[&](const std::vector<unsigned char>& pixels,wi::graphics::Texture& target){
            wi::graphics::SubresourceData data;
            data.data_ptr=pixels.data();data.row_pitch=256*4;data.slice_pitch=unsigned(pixels.size());
            return gpu->CreateTexture(&desc,&data,&target);
        };
        if(!upload(StarPixels(4,true),topStarTexture))return false;
        for(unsigned i=0;i<impactStarTextures.size();++i)
            if(!upload(StarPixels(i,false),impactStarTextures[i]))return false;
        return true;
    }
    bool Create(wi::scene::Scene& scene) {
        if(!bolt.Create(scene) || !BuildStarTextures())return false;
        topStar=StarCard(scene,"Stage2 textured star flare around source sphere");
        impactStar=StarCard(scene,"Stage2 sparking impact flare (not solid circle)");
        if(auto* m=scene.materials.GetComponent(topStar)){
            m->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(topStarTexture);
            m->SetDirty();
        }
        At(scene,topStar,0,5.56f,4.5f,1.f);
        if(auto* t=scene.transforms.GetComponent(topStar)){
            t->ClearTransform();t->Scale(XMFLOAT3{.76f,.76f,.001f});
            t->Translate(XMFLOAT3{0,5.56f,4.5f});t->UpdateTransform();
        }
        At(scene,impactStar,0,.16f,4.5f,.00001f);
        // Keep a real sphere at the top of the single strike. A compact white
        // centre has a slightly larger translucent violet-blue charged shell.
        sourceHalo=Orb(scene,"Stage2 luminous top charge shell",
                       {.35f,.49f,1.f,.10f},{.26f,.38f,.9f,.65f});
        sourceCore=Orb(scene,"Stage2 white-hot top source",
                       {.89f,.95f,1.f,.52f},{.76f,.86f,1.f,3.1f});
        At(scene,sourceHalo,0,5.56f,4.5f,.48f);
        At(scene,sourceCore,0,5.56f,4.5f,.21f);
        sourceLight=scene.Entity_CreateLight("Stage2 source illumination",{},
            XMFLOAT3{.31f,.44f,1.f},2.6f,3.6f,
            wi::scene::LightComponent::POINT);
        At(scene,sourceLight,0,5.52f,4.5f,1.f);
        // An energetic contact, not a second bolt. Restore the genuinely
        // illuminating impact point-light used in early Storm tests.
        impactHalo=Orb(scene,"Stage2 blue-white impact glow",
                       {.42f,.66f,1.f,.08f},{.39f,.58f,1.f,.82f});
        impactCore=Orb(scene,"Stage2 white-hot floor contact",
                       {1.f,.98f,1.f,.45f},{.89f,.95f,1.f,3.8f});
        At(scene,impactHalo,0,.058f,4.5f,.00001f);
        At(scene,impactCore,0,.073f,4.5f,.00001f);
        impactLight=scene.Entity_CreateLight("Stage2 ORIGINAL STYLE impact point LIGHT",{},
            XMFLOAT3{.66f,.82f,1.f},0.f,4.2f,
            wi::scene::LightComponent::POINT);
        At(scene,impactLight,0,.34f,4.5f,1.f);
        sparks=bridge::CreateProjectileEffectEmitter(scene,
            {bridge::ProjectileEffectKind::Sparks,{},.028f,0.f,.30f},{});
        if(sparks==wi::ecs::INVALID_ENTITY)return false;
        At(scene,sparks,0,.058f,4.5f,1.f);
        if(auto* e=scene.emitters.GetComponent(sparks)){
            e->SetMaxParticleCount(230);
            e->life=.21f;e->random_life=.48f;
            e->velocity={0,3.1f,0};
            e->gravity={0,-8.6f,0};
            e->random_factor=1.15f;
            e->normal_factor=1.1f;
            e->SetCollidersDisabled(false);
            e->count=0;
        }
        if(auto* m=scene.materials.GetComponent(sparks)){
            m->baseColor={.67f,.83f,1.f,.88f};
            m->emissiveColor={.63f,.8f,1.f,2.3f};
            m->SetDirty();
        }
        return true;
    }
    bool Select(wi::scene::Scene& scene,unsigned frame) {
        if(!bolt.Select(scene,frame))return false;
        constexpr std::array<float,SingleBolt::Frames> impact{
            0.f,0.f,.03f,.19f,.95f,1.f,.66f,.21f,.02f
        };
        constexpr std::array<float,SingleBolt::Frames> sparksBurst{
            0.f,0.f,0.f,70.f,240.f,185.f,75.f,0.f,0.f
        };
        constexpr std::array<float,SingleBolt::Frames> sourcePulse{
            .48f,.7f,.82f,1.0f,1.0f,.92f,.76f,.47f,.23f
        };
        const float hit=impact[frame],charge=sourcePulse[frame];
        At(scene,sourceHalo,0,5.56f,4.5f,.36f+.16f*charge);
        At(scene,sourceCore,0,5.56f,4.5f,.15f+.08f*charge);
        if(auto* l=scene.lights.GetComponent(sourceLight))l->intensity=1.2f+3.f*charge;
        // Keep the impact sphere small; create most of the blast through
        // local illumination and a low, horizontally spreading corona.
        At(scene,impactCore,0,.085f,4.5f,hit>.02f?.075f+.11f*std::sqrt(hit):.00001f);
        At(scene,impactHalo,0,.055f,4.5f,hit>.02f?.13f+.52f*std::sqrt(hit):.00001f,.07f);
        if(auto* m=scene.materials.GetComponent(impactStar)){
            m->textures[wi::scene::MaterialComponent::BASECOLORMAP]
               .resource.SetTexture(impactStarTextures[frame]);
            m->SetDirty();
        }
        if(auto* t=scene.transforms.GetComponent(impactStar)){
            t->ClearTransform();
            t->Scale(XMFLOAT3{1.10f,1.10f,.001f});
            t->Translate(XMFLOAT3{0,.16f,4.5f});
            t->UpdateTransform();
        }
        if(auto* l=scene.lights.GetComponent(impactLight))
            l->intensity=19.f*hit; // Real lighting, not fake additive overlay.
        if(auto* e=scene.emitters.GetComponent(sparks))
            e->count=sparksBurst[frame];
        return true;
    }
};
}

static bool StandaloneLightningStage2Proof(const fs::path& output){
    using namespace renegade::lightning_research;
    if(fs::exists(output/"phase-00.png")){
        std::cerr<<"Stage2 review already exists; refusing overwrite\n";
        return false;
    }
    fs::create_directories(output);
    wi::scene::Scene scene;
    const auto floor=scene.Entity_CreateCube("Neutral strike proof ground");
    if(auto* t=scene.transforms.GetComponent(floor)){
        t->Scale(XMFLOAT3{2.4f,.06f,2.4f});
        t->Translate(XMFLOAT3{0,-.16f,4.5f});t->UpdateTransform();
    }
    if(auto* m=scene.materials.GetComponent(floor)){
        m->baseColor={.10f,.12f,.19f,1.f};
        m->SetRoughness(.96f);m->SetDirty();
    }
    scene.weathers.Create(wi::ecs::CreateEntity());
    CompleteStrike strike;
    if(!strike.Create(scene))return false;
    for(unsigned phase=0;phase<SingleBolt::Frames;++phase){
        if(!strike.Select(scene,phase))return false;
        // Warm the native spark emitter without skipping the bolt pose.
        for(unsigned tick=0;tick<4;++tick)scene.Update(1.f/60.f);
        const auto file=output/("phase-0"+std::to_string(phase)+".png");
        if(!Capture(scene,file,false,false,true,.86f))return false;
        std::cout<<"STAGE2 LIGHTNING CONTACT FRAME "<<phase<<" "<<file.generic_u8string()<<"\n";
    }
    if(!scene.lights.Contains(strike.impactLight) ||
       !scene.emitters.Contains(strike.sparks) ||
       !scene.materials.Contains(strike.sourceCore) ||
       !scene.materials.Contains(strike.impactCore))return false;
    std::ofstream(output/"STAGE-2-RESEARCH.txt")<<
        "STAGE2 SINGLE STRIKE NATIVE PROOF; TOP CHARGE SPHERE, WHITE-BLUE IMPACT, RESTORED LIVE POINT LIGHT AND REAL GPU SPARKS.\n"
        "Preserves Stage 1 bolt geometry and textures. NOT the AoE storm; no targeting or gameplay changes. Owner visual approval pending.\n";
    std::cout<<"STAGE2 COMPLETE SINGLE LIGHTNING STRIKE - 9 GPU FRAMES, SOURCE ORB, RESTORED IMPACT LIGHT, NATIVE SPARKS PASS\n";
    return true;
}
