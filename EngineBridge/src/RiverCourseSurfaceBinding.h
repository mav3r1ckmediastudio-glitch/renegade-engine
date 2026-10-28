#pragma once
#include "renegade/bridge/RiverCourseService.h"
#include <memory>
namespace renegade::bridge {
class RiverCourseSurfaceBinding {
public:
 RiverCourseSurfaceBinding();~RiverCourseSurfaceBinding();
 RiverCourseSurfaceBinding(const RiverCourseSurfaceBinding&)=delete;
 bool Configure(wi::scene::Scene&,wi::ecs::Entity,const RiverCourseSurface&,std::string&);
 bool Apply();void Restore()noexcept;
 bool Ready()const;const std::string& Error()const;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
