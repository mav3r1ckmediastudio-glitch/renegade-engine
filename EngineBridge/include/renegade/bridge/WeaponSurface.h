#pragma once
#include <WickedEngine.h>
#include <set>
#include <map>
#include <array>
#include <string>
#include <functional>

namespace renegade::bridge {
using WeaponSurface = std::array<float,3>; // roughness, metalness, reflectance
using WeaponSurfaces = std::map<std::string,WeaponSurface>;
inline void VisitWeaponMaterials(wi::scene::Scene& scene,wi::ecs::Entity root,bool outside,
    const std::function<void(std::string,wi::scene::MaterialComponent&)>& visit){
    std::set<wi::ecs::Entity> seen;
    for(size_t i=0;i<scene.objects.GetCount();++i){
        auto object=scene.objects.GetEntity(i);
        if(root && ((object==root || scene.Entity_IsDescendant(object,root))==outside))continue;
        auto mesh=scene.meshes.GetComponent(scene.objects[i].meshID);
        if(!mesh)continue;
        for(size_t index=0;index<mesh->subsets.size();++index){
            auto id=mesh->subsets[index].materialID;
            if(!seen.insert(id).second)continue;
            auto mat=scene.materials.GetComponent(id);
            auto name=scene.names.GetComponent(id);
            if(mat)visit(name && !name->name.empty()?name->name:"Material "+std::to_string(index+1),*mat);
        }
    }
}
inline void SetWeaponSurface(wi::scene::MaterialComponent& material,const WeaponSurface& values){
    material.SetRoughness(values[0]);material.SetMetalness(values[1]);material.SetReflectance(values[2]);
}
}
