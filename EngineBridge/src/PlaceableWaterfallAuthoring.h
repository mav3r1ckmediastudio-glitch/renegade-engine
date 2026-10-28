#pragma once
// G13D: creator-owned waterfall records in native WISCENE metadata.
// Appearance/render construction is deliberately NOT owned by this authoring seam.
#include "PlaceableWaterfallEffect.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/CommandService.h"
#include <array>
#include <iomanip>
#include <locale>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <cmath>

namespace renegade::bridge::placeable_waterfall {
inline constexpr const char* AuthoredMetadataKey="renegade.placeable_waterfall.definition.v1";
inline constexpr const char* AuthoredFormat="RENEGADE_WATERFALL_V1";
using WaterfallId=renegade::bridge::StableId;
struct AuthoredWaterfall {
    WaterfallId id;
    wi::ecs::Entity entity=wi::ecs::INVALID_ENTITY;
    Definition settings{};
};
// The transform is a selection anchor, not a render-derived GPU entity.
// All authoring properties persist in WISCENE metadata; generated water
// meshes/emitters remain disposable and must be rebuilt on load.
inline std::string Encode(const Definition& d){
    (void)BuildGeometry(d); // same bounds apply to editor, Runtime and proof
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << AuthoredFormat << ' ' << std::setprecision(9);
    for(float x:{d.crest.x,d.crest.y,d.crest.z,d.landing.x,d.landing.y,d.landing.z,
                 d.width,d.intensity,d.spray,d.mist,d.impactSpray,d.turbulence})
        stream << x << ' ';
    return stream.str();
}
inline bool Decode(const std::string& encoded,Definition& d,std::string& error){
    std::istringstream stream(encoded);
    stream.imbue(std::locale::classic());
    std::string header;
    std::array<float,12> fields{};
    if(!(stream>>header)||header!=AuthoredFormat){
        error="Unknown placeable waterfall definition version";return false;
    }
    for(auto& value:fields){
        if(!(stream>>value)){error="Incomplete waterfall settings";return false;}
    }
    std::string tail;
    if(stream>>tail){error="Unexpected waterfall definition fields";return false;}
    Definition candidate;
    candidate.crest={fields[0],fields[1],fields[2]};
    candidate.landing={fields[3],fields[4],fields[5]};
    candidate.width=fields[6];candidate.intensity=fields[7];
    candidate.spray=fields[8];candidate.mist=fields[9];
    candidate.impactSpray=fields[10];candidate.turbulence=fields[11];
    try{(void)BuildGeometry(candidate);}
    catch(const std::exception& e){error=e.what();return false;}
    d=candidate;error.clear();return true;
}
inline bool IsWaterfall(const wi::scene::Scene& scene,wi::ecs::Entity entity){
    const auto* m=scene.metadatas.GetComponent(entity);
    return m&&m->string_values.has(AuthoredMetadataKey);
}
inline bool ReadWaterfall(const wi::scene::Scene& scene,
    wi::ecs::Entity entity,AuthoredWaterfall& result,std::string& error){
    if(!scene.transforms.Contains(entity)||!IsWaterfall(scene,entity)){
        error="Selection is not an authored waterfall";return false;
    }
    AuthoredWaterfall candidate;
    candidate.entity=entity;
    candidate.id=renegade::bridge::PersistentEntityId(scene,entity);
    if(candidate.id.empty()){
        error="Waterfall anchor lacks persistent identity";return false;
    }
    const auto* m=scene.metadatas.GetComponent(entity);
    if(!Decode(m->string_values.get(AuthoredMetadataKey),candidate.settings,error))
        return false;
    result=std::move(candidate);error.clear();return true;
}
inline bool EnumerateWaterfalls(const wi::scene::Scene& scene,
    std::vector<AuthoredWaterfall>& results,std::string& error){
    std::vector<AuthoredWaterfall> candidate;
    for(size_t i=0;i<scene.metadatas.GetCount();++i){
        const auto e=scene.metadatas.GetEntity(i);
        if(!IsWaterfall(scene,e))continue;
        AuthoredWaterfall record;
        if(!ReadWaterfall(scene,e,record,error))return false;
        for(const auto& prior:candidate)if(prior.id==record.id){
            error="Duplicate authored waterfall identity";return false;
        }
        candidate.push_back(std::move(record));
    }
    results=std::move(candidate);error.clear();return true;
}
inline constexpr const char* LandingHandleKey="renegade.placeable_waterfall.landing.v1";
inline wi::ecs::Entity WaterfallOwner(const wi::scene::Scene& scene,wi::ecs::Entity e){
 if(IsWaterfall(scene,e))return e;
 auto* h=scene.hierarchy.GetComponent(e);if(h&&IsWaterfall(scene,h->parentID))return h->parentID;
 return wi::ecs::INVALID_ENTITY;
}
inline wi::ecs::Entity LandingHandle(const wi::scene::Scene& scene,wi::ecs::Entity root){
 for(size_t i=0;i<scene.hierarchy.GetCount();++i){auto e=scene.hierarchy.GetEntity(i);if(scene.hierarchy[i].parentID==root){auto* m=scene.metadatas.GetComponent(e);if(m&&m->string_values.has(LandingHandleKey))return e;}}
 return wi::ecs::INVALID_ENTITY;
}
inline void SyncLandingHandle(wi::scene::Scene& scene,wi::ecs::Entity root,const Definition& d){
 auto e=LandingHandle(scene,root);if(e==wi::ecs::INVALID_ENTITY){e=scene.Entity_CreateTransform("Landing handle");scene.metadatas.Create(e).string_values.set(LandingHandleKey,"1");scene.Component_Attach(e,root);}
 auto* t=scene.transforms.GetComponent(e);t->translation_local={d.landing.x-d.crest.x,d.landing.y-d.crest.y,d.landing.z-d.crest.z};t->SetDirty();t->UpdateTransform();
}
inline bool CreateWaterfall(wi::scene::Scene& scene,
    const Definition& d,const WaterfallId& id,
    wi::ecs::Entity& created,std::string& error){
    std::string encoded;
    try{encoded=Encode(d);}
    catch(const std::exception& ex){error=ex.what();return false;}
    if(!renegade::bridge::IsValidStableId(id)){
        error="Invalid persistent waterfall ID";return false;
    }
    renegade::bridge::EntityIdentityIndex index;
    if(!index.Build(scene,error))return false;
    if(index.Resolve(id)!=wi::ecs::INVALID_ENTITY){
        error="Duplicate waterfall ID";return false;
    }
    const auto entity=scene.Entity_CreateTransform("Waterfall");
    auto* transform=scene.transforms.GetComponent(entity);
    transform->translation_local=d.crest;
    transform->SetDirty();transform->UpdateTransform();
    if(!renegade::bridge::AssignPersistentEntityId(scene,entity,id,error)){
        scene.Entity_Remove(entity);return false;
    }
    auto* metadata=scene.metadatas.GetComponent(entity);
    if(!metadata)metadata=&scene.metadatas.Create(entity);
    metadata->string_values.set(AuthoredMetadataKey,encoded);
    SyncLandingHandle(scene,entity,d);
    created=entity;error.clear();return true;
}
inline bool UpdateWaterfall(wi::scene::Scene& scene,
    wi::ecs::Entity entity,const Definition& d,std::string& error){
    AuthoredWaterfall previous;
    if(!ReadWaterfall(scene,entity,previous,error))return false;
    std::string encoded;
    try{encoded=Encode(d);}
    catch(const std::exception& ex){error=ex.what();return false;}
    scene.metadatas.GetComponent(entity)->string_values.set(AuthoredMetadataKey,encoded);
    auto* transform=scene.transforms.GetComponent(entity);
    transform->translation_local=d.crest;
    transform->SetDirty();transform->UpdateTransform();
    SyncLandingHandle(scene,entity,d);
    error.clear();return true;
}
inline wi::ecs::Entity FindWaterfallById(const wi::scene::Scene& scene,
    const WaterfallId& id){
    for(size_t i=0;i<scene.metadatas.GetCount();++i){
        const auto e=scene.metadatas.GetEntity(i);
        if(IsWaterfall(scene,e)&&renegade::bridge::PersistentEntityId(scene,e)==id)
            return e;
    }
    return wi::ecs::INVALID_ENTITY;
}
class WaterfallCommand final:public renegade::bridge::ICommand {
public:
    enum class Action{Create,Edit,Remove};
    WaterfallCommand(wi::scene::Scene& s,Action a,WaterfallId id,
                     Definition after={})
        :scene_(&s),action_(a),id_(std::move(id)),after_(after){}
    [[nodiscard]] const WaterfallId& Id()const noexcept{return id_;}
    [[nodiscard]] const std::string& Error()const noexcept{return error_;}
    bool Execute()override{
        if(!scene_)return false;
        const auto e=FindWaterfallById(*scene_,id_);
        if(action_==Action::Create){
            if(e!=wi::ecs::INVALID_ENTITY){error_="Waterfall exists";return false;}
            wi::ecs::Entity created;
            return CreateWaterfall(*scene_,after_,id_,created,error_);
        }
        if(e==wi::ecs::INVALID_ENTITY){error_="Waterfall anchor missing";return false;}
        AuthoredWaterfall current;
        if(!ReadWaterfall(*scene_,e,current,error_))return false;
        before_=current.settings;
        if(auto* name=scene_->names.GetComponent(e))beforeName_=name->name;
        if(action_==Action::Edit)return UpdateWaterfall(*scene_,e,after_,error_);
        scene_->Entity_Remove(e);
        error_.clear();return true;
    }
    void Undo()override{
        if(!scene_)return;
        const auto e=FindWaterfallById(*scene_,id_);
        if(action_==Action::Create){
            if(e!=wi::ecs::INVALID_ENTITY)scene_->Entity_Remove(e);
        }else if(action_==Action::Edit){
            if(e!=wi::ecs::INVALID_ENTITY){std::string error;UpdateWaterfall(*scene_,e,before_,error);}
        }else if(e==wi::ecs::INVALID_ENTITY){
            wi::ecs::Entity created;std::string error;
            if(CreateWaterfall(*scene_,before_,id_,created,error)&&!beforeName_.empty())scene_->names.GetComponent(created)->name=beforeName_;
        }
    }
private:
    wi::scene::Scene* scene_;
    Action action_;
    WaterfallId id_;
    Definition after_{},before_{};
    std::string error_,beforeName_;
};
inline std::unique_ptr<WaterfallCommand> MakeCreateWaterfallCommand(
    wi::scene::Scene& scene,Definition d){
    return std::make_unique<WaterfallCommand>(
        scene,WaterfallCommand::Action::Create,
        renegade::bridge::GenerateStableId(),d);
}
inline std::unique_ptr<WaterfallCommand> MakeEditWaterfallCommand(
    wi::scene::Scene& scene,WaterfallId id,Definition d){
    return std::make_unique<WaterfallCommand>(
        scene,WaterfallCommand::Action::Edit,std::move(id),d);
}
inline std::unique_ptr<WaterfallCommand> MakeRemoveWaterfallCommand(
    wi::scene::Scene& scene,WaterfallId id){
    return std::make_unique<WaterfallCommand>(
        scene,WaterfallCommand::Action::Remove,std::move(id));
}
} // namespace renegade::bridge::placeable_waterfall
