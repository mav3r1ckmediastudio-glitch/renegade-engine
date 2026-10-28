#pragma once
#include "renegade/bridge/RiverFlowRenderer.h"
#include <memory>
namespace renegade::bridge {
// Per-path shared flat-confluence renderer. Composed by RiverFlowRenderer;
// no scene entities or persistent renderer handles are authored.
class RiverWaterwayRenderer {
public:
 explicit RiverWaterwayRenderer(std::string shaderDirectory="Content/river");~RiverWaterwayRenderer();
 RiverWaterwayRenderer(const RiverWaterwayRenderer&)=delete;
 void Prepare(wi::scene::Scene&,std::uint64_t sceneRevision,float dt,std::uint64_t editRevision);
 void Invalidate();void SetSuspended(bool);
 bool Owns(wi::ecs::Entity)const;bool Ready()const;
 const std::string& Status()const;
 void Apply()const;void Restore()const;void Dispatch()const;
 void DrawArrows()const;
 bool Diagnostics(RiverFlowDiagnostics&,std::string&,bool readback)const;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
