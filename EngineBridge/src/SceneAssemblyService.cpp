#include "renegade/bridge/SceneAssemblyService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include <algorithm>
#include <cmath>
namespace renegade::bridge {
using namespace wi::ecs;
bool AssemblyTransformChanged(const TransformState& a,const TransformState& b) noexcept {
    const float before[]={a.translation.x,a.translation.y,a.translation.z,a.rotation.x,a.rotation.y,a.rotation.z,a.rotation.w,a.scale.x,a.scale.y,a.scale.z};
    const float after[]={b.translation.x,b.translation.y,b.translation.z,b.rotation.x,b.rotation.y,b.rotation.z,b.rotation.w,b.scale.x,b.scale.y,b.scale.z};
    for(size_t i=0;i<10;++i)if(std::abs(after[i]-before[i])>0.00001f)return true;
    return false;
}
XMMATRIX AssemblyWorldMatrix(const wi::scene::Scene& scene,Entity e) {
    auto m=XMMatrixIdentity();size_t remaining=scene.hierarchy.GetCount()+1;
    while(e!=INVALID_ENTITY && remaining--) {
        if(auto* t=scene.transforms.GetComponent(e))m*=t->GetLocalMatrix();
        auto* h=scene.hierarchy.GetComponent(e);if(!h)break;e=h->parentID;
    } return m;
}
bool IsSceneAssembly(const wi::scene::Scene& scene,Entity e) {
    auto* m=scene.metadatas.GetComponent(e);return m&&m->string_values.has(SceneAssemblyKey) && !(m->bool_values.has("renegade.scene_assembly_payload")&&m->bool_values.get("renegade.scene_assembly_payload"));
}
Entity ResolveAssemblySelection(const wi::scene::Scene& scene,Entity e,Entity editing) {
    Entity result=e;size_t remaining=scene.hierarchy.GetCount()+1;
    while(e!=INVALID_ENTITY && remaining--) {
        if(e==editing)break;
        auto* m=scene.metadatas.GetComponent(e);
        if(IsSceneAssembly(scene,e)||(m&&m->string_values.has(ReusableAssetInstanceIdMetadataKey)))result=e;
        auto* h=scene.hierarchy.GetComponent(e);if(!h)break;e=h->parentID;
    }return result;
}
std::vector<Entity> SelectionRoots(const wi::scene::Scene& scene,const std::vector<Entity>& values) {
    std::vector<Entity> result;
    for(auto e:values) {
        if(!scene.transforms.Contains(e)||std::find(result.begin(),result.end(),e)!=result.end())continue;
        bool covered=false;auto* h=scene.hierarchy.GetComponent(e);size_t remaining=scene.hierarchy.GetCount()+1;
        while(h&&remaining--) {
            if(std::find(values.begin(),values.end(),h->parentID)!=values.end()){covered=true;break;}
            h=scene.hierarchy.GetComponent(h->parentID);
        }if(!covered)result.push_back(e);
    }return result;
}
SelectionHistoryCommand::SelectionHistoryCommand(SelectionService& selection,std::unique_ptr<ICommand> command,
    std::function<Selection()> after)
    :selection_(selection),command_(std::move(command)),resolveAfter_(std::move(after)),before_(selection.Entities()){}
bool SelectionHistoryCommand::Execute() {
    if(!command_ || !command_->Execute())return false;
    if(!capturedAfter_){after_=resolveAfter_?resolveAfter_():before_;capturedAfter_=true;}
    selection_.Set(after_);return true;
}
void SelectionHistoryCommand::Undo(){command_->Undo();selection_.Set(before_);}
bool SelectionBatchCommand::Execute() {
    if(commands_.empty())return false;
    size_t done=0;
    for(auto& c:commands_) {if(!c->Execute()){while(done)commands_[--done]->Undo();return false;}++done;}
    return true;
}
void SelectionBatchCommand::Undo() {for(auto it=commands_.rbegin();it!=commands_.rend();++it)(*it)->Undo();}
SceneAssemblyCommand::SceneAssemblyCommand(wi::scene::Scene& s,std::vector<Entity> pieces,std::string name)
    :scene_(s),pieces_(SelectionRoots(s,pieces)),name_(std::move(name)){}
SceneAssemblyCommand::SceneAssemblyCommand(wi::scene::Scene& s,Entity root)
    :scene_(s),root_(root),unpack_(true){}
static void ApplyLocal(wi::scene::TransformComponent& t,const TransformState& v) {
    t.translation_local=v.translation;t.rotation_local=v.rotation;t.scale_local=v.scale;t.SetDirty();t.UpdateTransform();
}
bool SceneAssemblyCommand::Execute() {
    if(!initialized_) {
        if(unpack_) {
            if(!IsSceneAssembly(scene_,root_))return false;
            auto gather=[&](auto&& self,Entity parent)->void {
                for(size_t i=0;i<scene_.hierarchy.GetCount();++i) {
                    if(scene_.hierarchy[i].parentID!=parent)continue;
                    const auto e=scene_.hierarchy.GetEntity(i);
                    auto* m=scene_.metadatas.GetComponent(e);
                    if(m&&m->bool_values.has("renegade.scene_assembly_payload")&&m->bool_values.get("renegade.scene_assembly_payload")) {
                        payloadSnapshots_.emplace_back(e,wi::Archive{});
                        wi::ecs::EntitySerializer serializer;
                        scene_.Entity_Serialize(payloadSnapshots_.back().second,serializer,e,wi::scene::Scene::EntitySerializeFlags::NONE);
                        wi::jobsystem::Wait(serializer.ctx);
                        self(self,e);
                    } else pieces_.push_back(e);
                }
            };gather(gather,root_);
            if(pieces_.empty())return false;
            wi::ecs::EntitySerializer ser;
            scene_.Entity_Serialize(rootSnapshot_,ser,root_,wi::scene::Scene::EntitySerializeFlags::NONE);
            wi::jobsystem::Wait(ser.ctx);
        } else if(pieces_.size()<2)return false;
        for(auto e:pieces_) {
            auto* t=scene_.transforms.GetComponent(e);if(!t)return false;
            auto* h=scene_.hierarchy.GetComponent(e);
            before_.push_back({e,h?h->parentID:INVALID_ENTITY,CaptureTransform(*t)});
        }
        if(!unpack_) {
            XMVECTOR center=XMVectorZero();
            for(auto e:pieces_)center+=AssemblyWorldMatrix(scene_,e).r[3];
            center/=float(pieces_.size());
            rootTransform_.translation={XMVectorGetX(center),XMVectorGetY(center),XMVectorGetZ(center)};
            root_=wi::ecs::CreateEntity();
        }
        initialized_=true;
    }
    for(auto e:pieces_)if(!scene_.transforms.Contains(e))return false;
    if(unpack_) {
        auto* h=scene_.hierarchy.GetComponent(root_);const auto parent=h?h->parentID:INVALID_ENTITY;
        // Publish each current world matrix before native reparenting.
        for(auto e:pieces_) {
            auto world=AssemblyWorldMatrix(scene_,e);
            XMStoreFloat4x4(&scene_.transforms.GetComponent(e)->world,world);
            if(parent!=INVALID_ENTITY)scene_.Component_Attach(e,parent);
            else scene_.Component_Detach(e);
        }
        for(const auto& snapshot:payloadSnapshots_)scene_.Entity_Remove(snapshot.first,false);
        scene_.Entity_Remove(root_,false);
    } else {
        if(scene_.transforms.Contains(root_))return false;
        if(hasRootSnapshot_) {
            rootSnapshot_.SetReadModeAndResetPos(true);wi::ecs::EntitySerializer reader;reader.allow_remap=false;
            scene_.Entity_Serialize(rootSnapshot_,reader,INVALID_ENTITY,wi::scene::Scene::EntitySerializeFlags::NONE);
            wi::jobsystem::Wait(reader.ctx);
        } else {
            scene_.names.Create(root_).name=name_;
            ApplyLocal(scene_.transforms.Create(root_),rootTransform_);
            scene_.metadatas.Create(root_).string_values.set(SceneAssemblyKey,"1");
            std::string error;
            if(!AssignNewPersistentEntityId(scene_,root_,error)){scene_.Entity_Remove(root_,false);return false;}
            wi::ecs::EntitySerializer writer;
            scene_.Entity_Serialize(rootSnapshot_,writer,root_,wi::scene::Scene::EntitySerializeFlags::NONE);
            wi::jobsystem::Wait(writer.ctx);hasRootSnapshot_=true;
        }
        for(auto e:pieces_) {
            auto world=AssemblyWorldMatrix(scene_,e);
            XMStoreFloat4x4(&scene_.transforms.GetComponent(e)->world,world);
            scene_.Component_Attach(e,root_);
        }
    } return true;
}
void SceneAssemblyCommand::Restore() {
    for(const auto& v:before_) {
        scene_.hierarchy.Remove(v.entity);
        if(v.parent!=INVALID_ENTITY)scene_.Component_Attach(v.entity,v.parent,true);
        if(auto* t=scene_.transforms.GetComponent(v.entity))ApplyLocal(*t,v.transform);
    }
}
void SceneAssemblyCommand::Undo() {
    if(unpack_) {
        rootSnapshot_.SetReadModeAndResetPos(true);wi::ecs::EntitySerializer ser;ser.allow_remap=false;
        scene_.Entity_Serialize(rootSnapshot_,ser,INVALID_ENTITY,wi::scene::Scene::EntitySerializeFlags::NONE);
        wi::jobsystem::Wait(ser.ctx);
        for(auto& snapshot:payloadSnapshots_) {
            snapshot.second.SetReadModeAndResetPos(true);
            wi::ecs::EntitySerializer reader;reader.allow_remap=false;
            scene_.Entity_Serialize(snapshot.second,reader,INVALID_ENTITY,wi::scene::Scene::EntitySerializeFlags::NONE);
            wi::jobsystem::Wait(reader.ctx);
        }
        Restore();
    } else {Restore();scene_.Entity_Remove(root_,false);}
}
}
