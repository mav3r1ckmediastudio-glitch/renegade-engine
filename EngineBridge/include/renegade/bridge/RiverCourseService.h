#pragma once
#include "RiverFlowFieldService.h"
namespace renegade::bridge {
// Derived geometry only. Creator transforms/UUIDs remain authoritative and
// are the sole serialized representation. All positions below are WORLD space.
struct RiverCourseImpact {float alongMeters=0,dropMeters=0;};
struct RiverCourseSurface {
    std::vector<RiverCourseImpact> impacts;
    std::shared_ptr<const RiverFlowField> flow;
    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT2> uv,travelUV;
    std::vector<uint32_t> indices;
    std::vector<XMFLOAT4> path; // arc from latest toe; entrainment; width; speed
    std::vector<wi::scene::TransformComponent> nativeControls;
    float nativeHalfWidth=0,totalTravel=0,minimumHeight=0,maximumHeight=0;
    int nativeSubdivisions=0;
    bool hasCascade=false;
};
bool RiverCourseHasElevation(const wi::scene::Scene&,wi::ecs::Entity) noexcept;
bool BuildRiverCourseSurface(const wi::scene::Scene&,wi::ecs::Entity,float inletSpeed,
    RiverCourseSurface&,std::string& error);
// Exact native derived cache used by rendering AND linked-bed composition.
// Does not mutate authored scene, create persistent entities or allocate GPU data.
}
