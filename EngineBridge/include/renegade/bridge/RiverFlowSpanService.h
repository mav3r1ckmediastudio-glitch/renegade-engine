#pragma once
#include "RiverFlowFieldService.h"
namespace renegade::bridge {
// Clip a validated native ribbon by its original native parameters. This does
// not insert control points, resample Catmull curves, or mutate authored meshes.
// Interior cuts use the actual native bank edges, with non-convex cuts refused.
bool BuildRiverFlowFieldSpan(const RiverFlowField& original,float first,float last,
    std::shared_ptr<const RiverFlowField>& result,std::string& error);
}
