#pragma once
#include "renegade/bridge/DependencyService.h"
#include "renegade/bridge/IdentityService.h"
namespace renegade::bridge {
// Project presets form the runtime particle catalog: dynamic Lua names need the
// whole project catalog, while the installed texture browser remains editor-only.
bool CollectParticleEffectRuntimeDependencies(const std::string& root,const StableId& project,std::vector<DependencyCandidate>&,std::string&);
bool SnapshotParticleEffectRuntimeInputs(const std::string& root,const StableId& project,const std::string& destination,std::string&);
class ParticleEffectRuntimeDependencyProvider final:public IDependencyProvider {
 StableId project_;
public:
 explicit ParticleEffectRuntimeDependencyProvider(StableId p):project_(std::move(p)){}
 const char* Name()const noexcept override{return "renegade.particle.runtime-catalog";}
 uint32_t Version()const noexcept override{return 1;}
 bool Supports(DependencyClass c)const noexcept override{return c==DependencyClass::ProjectDocument;}
 bool Discover(const DependencyProviderContext&,const DependencyCandidateSink&,const DependencyDiagnosticSink&,std::string&)const override;
};
}
