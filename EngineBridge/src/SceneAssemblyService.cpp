#include "renegade/bridge/SceneAssemblyService.h"
#include "renegade/bridge/SceneAssemblyScriptService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
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
std::vector<Entity> BoxSelectionHits(const std::vector<SelectionScreenBounds>& objects,
    float left,float top,float right,float bottom,bool fullyEnclosed) {
    // Test the union of an assembly's visible pieces, not each child separately.
    // Preserve discovery order for a predictable active object.
    // A crossing gesture must intersect a visible piece, not just empty space
    // between an assembly's components.
    if(!fullyEnclosed) {
        std::vector<Entity> hits;std::unordered_map<Entity,bool> seen;
        for(const auto& b:objects)if(b.left<=right&&b.right>=left&&b.top<=bottom&&b.bottom>=top)
            if(seen.emplace(b.root,true).second)hits.push_back(b.root);
        return hits;
    }
    std::vector<SelectionScreenBounds> units;
    std::unordered_map<Entity,size_t> indices;
    for(const auto& b:objects) {
        const auto inserted=indices.emplace(b.root,units.size());
        if(inserted.second)units.push_back(b);
        else {
            auto& unit=units[inserted.first->second];
            unit.left=std::min(unit.left,b.left);unit.top=std::min(unit.top,b.top);
            unit.right=std::max(unit.right,b.right);unit.bottom=std::max(unit.bottom,b.bottom);
        }
    }
    std::vector<Entity> hits;
    for(const auto& u:units)if(fullyEnclosed ?
        u.left>=left&&u.right<=right&&u.top>=top&&u.bottom<=bottom :
        u.left<=right&&u.right>=left&&u.top<=bottom&&u.bottom>=top)hits.push_back(u.root);
    return hits;
}
std::unique_ptr<ICommand> CreateSelectionTransformCommand(wi::scene::Scene& scene,
    const std::vector<Entity>& selected,SelectionTransformOperation operation,int axis,float value,std::string& error) {
    error.clear();
    const auto roots=SelectionRoots(scene,selected);
    if(roots.empty()||axis<0||axis>2||!std::isfinite(value)) {
        error="Enter a finite transform value for the selected objects.";return {};
    }
    if(operation==SelectionTransformOperation::ScaleFactor&&value<=0.00001f) {
        error="Enter a scale factor greater than zero. 1 keeps the current size.";return {};
    }
    XMVECTOR center=XMVectorZero();
    for(auto e:roots)center+=AssemblyWorldMatrix(scene,e).r[3];
    center/=float(roots.size());XMFLOAT3 pivot;XMStoreFloat3(&pivot,center);
    XMFLOAT3 delta={};
    auto setAxis=[&](XMFLOAT3& v,float f){if(axis==0)v.x=f;else if(axis==1)v.y=f;else v.z=f;};
    XMMATRIX change;
    if(operation==SelectionTransformOperation::Position) {
        const float current=axis==0?pivot.x:axis==1?pivot.y:pivot.z;
        setAxis(delta,value-current);change=XMMatrixTranslation(delta.x,delta.y,delta.z);
    } else {
        XMMATRIX edit;
        if(operation==SelectionTransformOperation::RotateDegrees) {
            setAxis(delta,XMConvertToRadians(value));edit=XMMatrixRotationRollPitchYaw(delta.x,delta.y,delta.z);
        } else {
            delta={1,1,1};setAxis(delta,value);edit=XMMatrixScaling(delta.x,delta.y,delta.z);
        }
        change=XMMatrixTranslation(-pivot.x,-pivot.y,-pivot.z)*edit*XMMatrixTranslation(pivot.x,pivot.y,pivot.z);
    }
    auto batch=std::make_unique<SelectionBatchCommand>();
    for(auto e:roots) {
        const auto before=CaptureTransform(*scene.transforms.GetComponent(e));
        auto local=AssemblyWorldMatrix(scene,e)*change;
        if(auto* h=scene.hierarchy.GetComponent(e))
            local*=XMMatrixInverse(nullptr,AssemblyWorldMatrix(scene,h->parentID));
        XMVECTOR scale,rotation,translation;
        if(XMMatrixIsNaN(local)||XMMatrixIsInfinite(local)||
            !XMMatrixDecompose(&scale,&rotation,&translation,local)) {
            error="These parent transforms cannot support that edit. Nothing was changed.";return {};
        }
        TransformState after;
        XMStoreFloat3(&after.translation,translation);XMStoreFloat4(&after.rotation,rotation);XMStoreFloat3(&after.scale,scale);
        const auto rebuilt=XMMatrixScalingFromVector(scale)*XMMatrixRotationQuaternion(rotation)*XMMatrixTranslationFromVector(translation);
        XMFLOAT4X4 wanted,actual;XMStoreFloat4x4(&wanted,local);XMStoreFloat4x4(&actual,rebuilt);
        for(int row=0;row<4;++row)for(int column=0;column<4;++column)
            if(std::abs(wanted.m[row][column]-actual.m[row][column])>0.0001f*std::max(1.f,std::abs(wanted.m[row][column]))) {
                error="Use uniform parent scales for this edit. Nothing was changed.";return {};
            }
        if(AssemblyTransformChanged(before,after))batch->Add(std::make_unique<SetTransformCommand>(scene,e,before,after));
    }
    return batch;
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
SceneAssemblyCommand::SceneAssemblyCommand(wi::scene::Scene& s,Entity root,bool preserveRoot)
    :scene_(s),root_(root),unpack_(true),keepRoot_(preserveRoot){}
static void ApplyLocal(wi::scene::TransformComponent& t,const TransformState& v) {
    t.translation_local=v.translation;t.rotation_local=v.rotation;t.scale_local=v.scale;t.SetDirty();t.UpdateTransform();
}
static bool AuthoredContainer(const wi::scene::Scene& scene,Entity e) {
    if(const auto* m=scene.metadatas.GetComponent(e);m&&m->bool_values.has(SceneAssemblyAuthoredRootKey)&&
        m->bool_values.get(SceneAssemblyAuthoredRootKey))return true;
    // Discover native component types through their registered managers only
    // during explicit Unpack; never discard an authored light/controller/etc.
    for(const auto& entry:scene.componentLibrary.entries) {
        const auto* manager=entry.second.component_manager.get();
        if(manager==&scene.names||manager==&scene.transforms||manager==&scene.hierarchy||
            manager==&scene.metadatas||manager==&scene.layers)continue;
        if(manager->Contains(e))return true;
    }
    return false;
}
static void StripAssemblyContainer(wi::scene::Scene& scene,Entity e) {
    auto* m=scene.metadatas.GetComponent(e);if(!m)return;
    if(m->string_values.has("renegade.scene_assembly.authored_name"))
        if(auto* name=scene.names.GetComponent(e))name->name=m->string_values.get("renegade.scene_assembly.authored_name");
    m->string_values.erase(SceneAssemblyKey);
    m->bool_values.erase("renegade.scene_assembly_payload");m->bool_values.erase(SceneAssemblyAuthoredRootKey);
    m->string_values.erase(ReusableAssetInstanceIdMetadataKey);
    m->int_values.erase(ReusableAssetInstanceVersionMetadataKey);
    m->bool_values.erase(ReusableAssetPayloadRootMetadataKey);
    m->string_values.erase(ReusableAssetInstanceDisplayNameMetadataKey);
}
bool SceneAssemblyCommand::Execute() {
    if(!initialized_) {
        if(unpack_) {
            if(!IsSceneAssembly(scene_,root_))return false;
            keepRoot_=keepRoot_||AuthoredContainer(scene_,root_);
            if(keepRoot_){pieces_.push_back(root_);authoredRoots_.push_back(root_);}
            auto gather=[&](auto&& self,Entity parent)->void {
                for(size_t i=0;i<scene_.hierarchy.GetCount();++i) {
                    if(scene_.hierarchy[i].parentID!=parent)continue;
                    const auto e=scene_.hierarchy.GetEntity(i);
                    auto* m=scene_.metadatas.GetComponent(e);
                    if(m&&m->bool_values.has("renegade.scene_assembly_payload")&&m->bool_values.get("renegade.scene_assembly_payload")) {
                        if(AuthoredContainer(scene_,e)) {
                            pieces_.push_back(e);authoredRoots_.push_back(e);
                        } else {
                            payloadSnapshots_.emplace_back(e,wi::Archive{});
                            wi::ecs::EntitySerializer serializer;
                            scene_.Entity_Serialize(payloadSnapshots_.back().second,serializer,e,wi::scene::Scene::EntitySerializeFlags::NONE);
                            wi::jobsystem::Wait(serializer.ctx);
                        }
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
            const auto* m=scene_.metadatas.GetComponent(e);const auto* name=scene_.names.GetComponent(e);
            before_.push_back({e,h?h->parentID:INVALID_ENTITY,CaptureTransform(*t),m!=nullptr,
                m?*m:wi::scene::MetadataComponent{},name?name->name:std::string{}});
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
        for(auto e:authoredRoots_)StripAssemblyContainer(scene_,e);
        for(auto e:pieces_) {
            auto* m=scene_.metadatas.GetComponent(e);if(!m)m=&scene_.metadatas.Create(e);
            m->bool_values.set(SceneAssemblyPreserveContentsKey,true);
        }
        for(const auto& snapshot:payloadSnapshots_)scene_.Entity_Remove(snapshot.first,false);
        if(!keepRoot_)scene_.Entity_Remove(root_,false);
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
        if(unpack_) {
            // Restore only metadata changed by Unpack. Persistent IDs adopted by
            // a later scene Save remain valid even when an older operation is undone.
            const auto liveId=PersistentEntityId(scene_,v.entity);
            if(v.hadMetadata) {
                auto* m=scene_.metadatas.GetComponent(v.entity);if(!m)m=&scene_.metadatas.Create(v.entity);*m=v.metadata;
            } else scene_.metadatas.Remove(v.entity);
            if(IsValidStableId(liveId)&&PersistentEntityId(scene_,v.entity).empty()) {
                auto* m=scene_.metadatas.GetComponent(v.entity);if(!m)m=&scene_.metadatas.Create(v.entity);
                m->string_values.set(PersistentEntityIdMetadataKey,liveId);
            }
            if(auto* name=scene_.names.GetComponent(v.entity))name->name=v.name;
        }
    }
}
void SceneAssemblyCommand::Undo() {
    if(unpack_) {
        if(!keepRoot_) {
            rootSnapshot_.SetReadModeAndResetPos(true);wi::ecs::EntitySerializer ser;ser.allow_remap=false;
            scene_.Entity_Serialize(rootSnapshot_,ser,INVALID_ENTITY,wi::scene::Scene::EntitySerializeFlags::NONE);
            wi::jobsystem::Wait(ser.ctx);
        }
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
