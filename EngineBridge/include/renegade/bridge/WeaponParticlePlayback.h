#pragma once
#include "renegade/bridge/ParticleEffectService.h"
#include <filesystem>
#include <map>
#include <sstream>
#include <memory>
namespace renegade::bridge {
struct WeaponParticlePlayback {
    using Scene=wi::scene::Scene;
    using Entity=wi::ecs::Entity;
    std::map<std::filesystem::path,std::shared_ptr<Scene>> templates;
    struct Live { Entity root; float remaining; };
    std::vector<Live> live;
    bool Prepare(const std::filesystem::path& file){
        if(templates.count(file))return true;
        if(file.empty()||!std::filesystem::is_regular_file(file))return false;
        wi::Archive archive(file.u8string(),true);
        if(!archive.IsOpen())return false;
        auto source=std::make_shared<Scene>();source->Serialize(archive);
        if(!source->emitters.GetCount())return false;
        templates.emplace(file,std::move(source));return true;
    }
    bool Spawn(Scene& scene,const std::filesystem::path& file,DirectX::FXMMATRIX pose,float emission){
        if(!Prepare(file))return false;
        auto root=scene.Instantiate(*templates.at(file),true);
        auto tr=scene.transforms.GetComponent(root);if(!tr)return false;
        auto world=pose;for(int i=0;i<3;++i)world.r[i]=DirectX::XMVector3Normalize(world.r[i]);
        tr->MatrixTransform(world);tr->UpdateTransform();
        float tail=0;
        for(size_t i=0;i<scene.metadatas.GetCount();++i){
            auto node=scene.metadatas.GetEntity(i);auto& m=scene.metadatas[i];
            if(!m.string_values.has(ParticleEffectMetadataKey)||!scene.Entity_IsDescendant(node,root))continue;
            ParticleEffectTiming timing;std::istringstream input(m.string_values.get("renegade.particle.timing"));
            ReadParticleTiming(input,timing);timing.enabled=true;timing.loop=false;timing.delay=0;timing.duration=emission;
            SetParticleTimingMetadata(scene,node,timing);ResetParticleTiming(scene,node);
        }
        for(size_t i=0;i<scene.emitters.GetCount();++i)if(scene.Entity_IsDescendant(scene.emitters.GetEntity(i),root)){
            const auto& emitter=scene.emitters[i];tail=std::max(tail,emitter.life*(1+std::abs(emitter.random_life)));
        }
        live.push_back({root,emission+tail+.05f});return true;
    }
    void Update(Scene& scene,float dt){
        for(auto it=live.begin();it!=live.end();){
            it->remaining-=dt;
            if(it->remaining>0){++it;continue;}
            scene.Entity_Remove(it->root);it=live.erase(it);
        }
    }
    void Clear(Scene& scene){for(auto& p:live)scene.Entity_Remove(p.root);live.clear();}
};
}
