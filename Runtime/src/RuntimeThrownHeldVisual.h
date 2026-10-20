#pragma once
// Presentation-only visibility for a single thrown reference's held grenade.
// Flight projectiles are independent world entities owned by ProjectileSession.
#include <WickedEngine.h>
#include <string>
namespace renegade::runtime {
    inline bool IsWithinViewModel(wi::scene::Scene& scene,
                                  wi::ecs::Entity child,wi::ecs::Entity root) {
        for(unsigned depth=0;depth<96 && child!=wi::ecs::INVALID_ENTITY;++depth) {
            if(child==root)return true;
            const auto* h=scene.hierarchy.GetComponent(child);
            if(!h)return false;
            child=h->parentID;
        }
        return false;
    }
    inline unsigned SetHeldGrenadeBodyVisible(wi::scene::Scene& scene,
                    wi::ecs::Entity viewModelRoot,bool visible) {
        if(viewModelRoot==wi::ecs::INVALID_ENTITY)return 0;
        unsigned changed=0;
        for(size_t i=0;i<scene.names.GetCount();++i) {
            if(scene.names[i].name.find("grenade_base_held")==std::string::npos)
                continue;
            const auto entity=scene.names.GetEntity(i);
            if(!IsWithinViewModel(scene,entity,viewModelRoot))continue;
            auto* object=scene.objects.GetComponent(entity);
            if(!object)continue;
            if(object->IsRenderable()!=visible)object->SetRenderable(visible);
            ++changed;
        }
        return changed;
    }
}
