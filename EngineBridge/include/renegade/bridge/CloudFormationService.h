#pragma once
#include "renegade/bridge/CommandService.h"
#include <WickedEngine.h>
#include <algorithm>
namespace renegade::bridge
{
    enum class CloudFormation : int { Legacy, Cumulus, ToweringCumulus, Stratocumulus, Overcast, Cirrus };
    inline CloudFormation CaptureCloudFormation(const wi::scene::Scene& scene, wi::ecs::Entity e) noexcept
    {
        const auto* m=scene.metadatas.GetComponent(e);
        if(!m || !m->int_values.has("renegade.clouds.schema") ||
           m->int_values.get("renegade.clouds.schema")!=1 ||
           !m->int_values.has("renegade.clouds.formation")) return CloudFormation::Legacy;
        const int v=m->int_values.get("renegade.clouds.formation");
        return v>=0 && v<=5 ? static_cast<CloudFormation>(v) : CloudFormation::Legacy;
    }
    inline void ApplyCloudFormationRuntime(wi::scene::Scene& scene, wi::ecs::Entity e, CloudFormation type) noexcept
    {
        auto* w=scene.weathers.GetComponent(e);
        if(!w) return;
        const int id=std::clamp(static_cast<int>(type),0,5);
        // Reserved GPU padding register; no archive or ABI layout change.
        w->volumetricCloudParameters.layerFirst.padding0=float(id);
        // Transient local-weather calibration; padding is not authored/archive state.
        w->volumetricCloudParameters.layerFirst.padding1=id>0 ? .80f : 0.0f;
        // Preserve authored second-layer parameters and native rendering.
        w->volumetricCloudParameters.layerSecond.padding0=0;
        if(scene.weathers.GetCount() && scene.weathers.GetEntity(0)==e) scene.weather=*w;
    }
    inline bool StoreCloudFormation(wi::scene::Scene& scene, wi::ecs::Entity e, CloudFormation type) noexcept
    {
        if(!scene.weathers.Contains(e)) return false;
        auto* m=scene.metadatas.GetComponent(e);
        if(!m) m=&scene.metadatas.Create(e);
        m->int_values.set("renegade.clouds.schema",1);
        m->int_values.set("renegade.clouds.formation",std::clamp(static_cast<int>(type),0,5));
        ApplyCloudFormationRuntime(scene,e,type);
        return true;
    }
    inline void ClearCloudFormation(wi::scene::Scene& scene, wi::ecs::Entity e) noexcept
    {
        auto* m=scene.metadatas.GetComponent(e);
        if(m) {m->int_values.erase("renegade.clouds.schema");m->int_values.erase("renegade.clouds.formation");}
        ApplyCloudFormationRuntime(scene,e,CloudFormation::Legacy);
    }
    inline void RestoreCloudFormations(wi::scene::Scene& scene) noexcept
    {
        for(size_t i=0;i<scene.weathers.GetCount();++i)
        { const auto e=scene.weathers.GetEntity(i);ApplyCloudFormationRuntime(scene,e,CaptureCloudFormation(scene,e)); }
    }
    class SetCloudFormationCommand final : public ICommand
    {
    public:
        SetCloudFormationCommand(wi::scene::Scene& s,wi::ecs::Entity e,CloudFormation type) noexcept
            :scene_(&s),entity_(e),before_(CaptureCloudFormation(s,e)),after_(type)
        {
            const auto* m=s.metadatas.GetComponent(e);
            existed_=m && m->int_values.has("renegade.clouds.schema");
            if(existed_) schemaBefore_=m->int_values.get("renegade.clouds.schema");
            typeExisted_=m && m->int_values.has("renegade.clouds.formation");
            if(typeExisted_) typeBefore_=m->int_values.get("renegade.clouds.formation");
        }
        bool Execute() override
        {
            if(before_==after_) return false;
            return StoreCloudFormation(*scene_,entity_,after_);
        }
        void Undo() override
        {
            auto* m=scene_->metadatas.GetComponent(entity_);
            if(!m || !scene_->weathers.Contains(entity_)) return;
            if(existed_) m->int_values.set("renegade.clouds.schema",schemaBefore_);
            else m->int_values.erase("renegade.clouds.schema");
            if(typeExisted_) m->int_values.set("renegade.clouds.formation",typeBefore_);
            else m->int_values.erase("renegade.clouds.formation");
            ApplyCloudFormationRuntime(*scene_,entity_,before_);
        }
    private:
        wi::scene::Scene* scene_;
        wi::ecs::Entity entity_;
        CloudFormation before_,after_;
        bool existed_;
        bool typeExisted_=false;
        int schemaBefore_=0,typeBefore_=0;
    };
}
