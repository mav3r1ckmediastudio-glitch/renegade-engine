#pragma once
#include "renegade/bridge/RiverFlowFieldService.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace renegade::bridge::river_flow_detail
{
struct SourceBounds
{
    DirectX::XMFLOAT3 minimum{INFINITY,INFINITY,INFINITY};
    DirectX::XMFLOAT3 maximum{-INFINITY,-INFINITY,-INFINITY};
};
inline SourceBounds CaptureSourceBounds(const RiverFlowField& field)
{
    SourceBounds bounds;
    for(const auto& row:field.NativeRibbon())
        for(const auto& point:{row.leftBank,row.rightBank})
        {
            bounds.minimum.x=std::min(bounds.minimum.x,point.x);
            bounds.minimum.y=std::min(bounds.minimum.y,point.y);
            bounds.minimum.z=std::min(bounds.minimum.z,point.z);
            bounds.maximum.x=std::max(bounds.maximum.x,point.x);
            bounds.maximum.y=std::max(bounds.maximum.y,point.y);
            bounds.maximum.z=std::max(bounds.maximum.z,point.z);
        }
    return bounds;
}
// The fingerprint and geometry capture must select the same world surfaces.
// Foreground arms/equipment are camera presentation, never river obstacles.
inline bool IsPotentialSource(const wi::scene::Scene& scene,
    wi::ecs::Entity entity,const wi::scene::ObjectComponent& object,
    const wi::scene::MeshComponent& mesh,const SourceBounds* bounds,bool includeSubmerged=false)
{
    if(!object.IsRenderable()||object.IsForeground()||!mesh.IsRenderable()||
        mesh.armatureID!=wi::ecs::INVALID_ENTITY||!mesh.morph_targets.empty())
        return false;
    // A placeable waterfall's runtime-generated water sheet is an animated
    // visual, not a solid river obstacle. Including it feeds its replacement
    // entity ID back into river geometry fingerprints every frame, triggering
    // an expensive river/foam rebuild and an invalid obstacle wake.
    // Real rocks, landscape and all authored solid objects remain eligible.
    if(const auto* md=scene.metadatas.GetComponent(entity);
        md&&md->string_values.has("renegade.waterfall.generated.v1"))
        return false;
    bool terrainSource=(object.filterMask&wi::enums::FILTER_TERRAIN)!=0;
    auto ancestor=entity;
    for(unsigned depth=0;ancestor!=wi::ecs::INVALID_ENTITY;++depth)
    {
        if(depth>=256)throw std::runtime_error("River source hierarchy exceeds limit");
        terrainSource=terrainSource||scene.terrains.Contains(ancestor);
        const auto* body=scene.rigidbodies.GetComponent(ancestor);
        if((body&&body->mass>0)||scene.characters.Contains(ancestor))return false;
        const auto* hierarchy=scene.hierarchy.GetComponent(ancestor);
        ancestor=hierarchy?hierarchy->parentID:wi::ecs::INVALID_ENTITY;
    }
    if(const auto* material=scene.materials.GetComponent(entity);
        material&&material->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER)
        return false;
    if(bounds)
    {
        const auto box=mesh.aabb.transform(scene.ComputeEntityMatrixRecursive(entity));
        const auto low=box.getMin(),high=box.getMax();
        for(float value:{low.x,low.y,low.z,high.x,high.y,high.z})
            if(!std::isfinite(value))throw std::runtime_error("Nonfinite river source bounds");
        // Terrain above the water must remain available for solid-bank masking.
        // Conservative vertical tolerance exceeds the triangle slice epsilon.
        // Recheck membership every frame so moving a rock into water is detected.
        if(high.x<bounds->minimum.x-.5f||low.x>bounds->maximum.x+.5f||
            high.z<bounds->minimum.z-.5f||low.z>bounds->maximum.z+.5f||
            (!terrainSource&&((includeSubmerged?high.y<bounds->minimum.y-4.f:high.y<bounds->minimum.y-.005f)||low.y>bounds->maximum.y+.005f)))
            return false;
    }
    return true;
}
}
