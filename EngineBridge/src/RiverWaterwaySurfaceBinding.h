#pragma once
#include "renegade/bridge/RiverJunctionSurfaceService.h"
#include <memory>
namespace renegade::bridge {
// Per-path derived geometry. Native source meshes/components stay authoritative.
// Configure only at a safe point after native Scene::Update has settled source
// spline caches. Bind around each native Update/PreRender/Render, never authoring
// or serialization. Member entities/components must not change inside a scope.
class RiverWaterwaySurfaceBinding {
public:
 RiverWaterwaySurfaceBinding();~RiverWaterwaySurfaceBinding();
 RiverWaterwaySurfaceBinding(const RiverWaterwaySurfaceBinding&)=delete;
 bool Configure(wi::scene::Scene&,wi::ecs::Entity outletRoot,
     const std::vector<wi::ecs::Entity>& nativeMembers,const RiverJunctionSurface&,
     float tileMeters,std::string& error,bool surfaceMotion=false);
 void Reset();
 bool Ready()const;bool IsBound()const;
 const std::string& Error()const;
 class Scope {
 public:
  explicit Scope(RiverWaterwaySurfaceBinding&);~Scope();
  Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
  bool Applied()const{return applied_;}
 private:RiverWaterwaySurfaceBinding& owner_;bool applied_;
 };
 Scope Bind(){return Scope(*this);}
private:
 struct Impl;std::unique_ptr<Impl> impl_;
 bool Apply();void Restore()noexcept;
};
}
