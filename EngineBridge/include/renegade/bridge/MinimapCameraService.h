#pragma once
#include "renegade/bridge/WorldNavigationService.h"
#include <WickedEngine.h>
namespace renegade::bridge
{
    // Transient native camera components share Wicked's existing render-to-texture
    // pass. No second Scene update, clone, simulation or renderer is created.
    class MinimapCameraService
    {
        std::map<StableId,wi::ecs::Entity> cameras_;
        std::uint64_t generation_=0;
    public:
        void Reset(wi::scene::Scene& scene)
        {
            for(const auto& [id,e]:cameras_)if(scene.cameras.Contains(e))scene.Entity_Remove(e,false);
            cameras_.clear();generation_=0;
        }
        void Update(wi::scene::Scene& scene,std::uint64_t generation,const WorldNavigationSnapshot& state,
            const std::set<StableId>& visible)
        {
            if(generation_!=generation){Reset(scene);generation_=generation;}
            std::set<StableId> active;
            for(const auto& [id,view]:state.views){
                if(active.size()>=4)break;
                const auto& w=view.widget;
                if(!visible.count(id)||!w.navigation||!w.navigation->minimap||!w.navigation->minimap->live)continue;
                const auto& c=*w.navigation;const auto& m=*c.minimap;
                const auto f=BuildMinimapFrame(c,state.camera,state.player,{},state.entities,w.rect.width,w.rect.height);
                if(!f.available)continue;active.insert(id);
                auto& e=cameras_[id];
                if(!scene.cameras.Contains(e)){
                    e=scene.Entity_CreateCamera("Runtime minimap "+id,256,256);scene.cameras.GetComponent(e)->render_to_texture.time_accumulator=1;
                }
                const float aspect=m.round?1:w.rect.width/std::max(1.f,w.rect.height);
                auto* camera=scene.cameras.GetComponent(e);
                const unsigned width=std::max(1u,unsigned(m.resolution*std::min(1.f,aspect))),height=std::max(1u,unsigned(m.resolution*std::min(1.f,1/aspect)));
                camera->CreateOrtho(float(width),float(height),.1f,m.cameraHeight+10000,2*m.range);
                camera->render_to_texture.resolution={width,height};
                camera->render_to_texture.sample_count=1;camera->render_to_texture.update_interval=1/m.updateHz;
                const float a=f.heading*XM_PI/180;
                auto eye=XMVectorSet(f.observer.position.x,f.observer.position.y+m.cameraHeight,f.observer.position.z,1);
                auto world=XMMatrixInverse(nullptr,XMMatrixLookToLH(eye,XMVectorSet(0,-1,0,0),XMVectorSet(std::sin(a),0,std::cos(a),0)));
                if(auto* t=scene.transforms.GetComponent(e)){t->ClearTransform();t->MatrixTransform(world);t->UpdateTransform();}
                camera->TransformCamera(world);camera->UpdateCamera();
            }
            for(auto i=cameras_.begin();i!=cameras_.end();)if(!active.count(i->first)){
                if(scene.cameras.Contains(i->second))scene.Entity_Remove(i->second,false);i=cameras_.erase(i);
            }else ++i;
        }
        const wi::graphics::Texture* Texture(const wi::scene::Scene& scene,const StableId& id)const
        {
            const auto i=cameras_.find(id);if(i==cameras_.end())return nullptr;
            const auto* camera=scene.cameras.GetComponent(i->second);
            return camera&&camera->render_to_texture.rendertarget_render.IsValid()?&camera->render_to_texture.rendertarget_render:nullptr;
        }
        size_t Count()const{return cameras_.size();}
    };
}
