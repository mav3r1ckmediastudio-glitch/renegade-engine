#pragma once
// Studio-only gizmo affordance. The stored spline-node width remains local
// scale.x for compatibility with Wicked spline interpolation and saved scenes.
// Choose the horizontal gizmo axis closest to the river's lateral direction,
// rather than assuming global X always points across the river.
#include <WickedEngine.h>
#include <algorithm>
#include <cmath>

namespace renegade::studio::detail {
inline bool RiverNodeWidthUsesZHandle(
    wi::scene::Scene& scene, wi::ecs::Entity selected) {
    for (std::size_t i=0;i<scene.splines.GetCount();++i) {
        const auto& nodes=scene.splines[i].spline_node_entities;
        const auto it=std::find(nodes.begin(),nodes.end(),selected);
        if(it==nodes.end() || nodes.size()<2) continue;
        const auto index=std::size_t(it-nodes.begin());
        const auto p=nodes[index==0?0:index-1];
        const auto n=nodes[std::min(index+1,nodes.size()-1)];
        DirectX::XMFLOAT3 a,b;
        DirectX::XMStoreFloat3(&a,scene.ComputeEntityMatrixRecursive(p).r[3]);
        DirectX::XMStoreFloat3(&b,scene.ComputeEntityMatrixRecursive(n).r[3]);
        // A river mostly following world X is widened by scaling the visible
        // world Z handle; a river following Z uses the X handle.
        return std::abs(b.x-a.x)>=std::abs(b.z-a.z);
    }
    return true;
}
} // namespace renegade::studio::detail
