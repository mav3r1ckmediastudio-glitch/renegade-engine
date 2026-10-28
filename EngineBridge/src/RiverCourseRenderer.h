#pragma once
#include "renegade/bridge/RiverFlowRenderer.h"
namespace renegade::bridge {
class RiverCourseRenderer {
public:
 explicit RiverCourseRenderer(std::string directory);~RiverCourseRenderer();
 void Prepare(wi::scene::Scene&,uint64_t revision,float dt,uint64_t editRevision);
 void Invalidate();void SetSuspended(bool);
 bool Owns(wi::ecs::Entity)const;size_t ActiveCount()const;
 const std::string& Status(wi::ecs::Entity)const;
 void Apply()const;void Restore()const;void Dispatch()const;void DrawArrows(wi::ecs::Entity)const;
 bool Diagnostics(wi::ecs::Entity,RiverFlowDiagnostics&,std::string&,bool)const;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
