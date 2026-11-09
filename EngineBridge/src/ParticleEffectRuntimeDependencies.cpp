#include "renegade/bridge/ParticleEffectRuntimeDependencies.h"
#include "renegade/bridge/ParticleEffectService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include <filesystem>
#include <set>
namespace renegade::bridge {
bool CollectParticleEffectRuntimeDependencies(const std::string& root,const StableId& project,std::vector<DependencyCandidate>& out,std::string& error){
 out.clear();error.clear();if(!IsValidStableId(project)){error="Particle runtime catalog requires a valid project identity.";return false;}
 auto presets=ListParticleEffects(root,project,error);if(!error.empty())return false;
 std::set<std::string> paths;bool hasTextures=false;
 auto add=[&](const std::string& path,DependencyClass kind){
  if(paths.insert(path).second)out.push_back({path,kind,DependencyRequirement::Required,"particle.runtime-catalog"});
 };
 for(const auto& p:presets){
  if(!IsValidStableId(p.id)){error="Particle preset has an invalid identity.";return false;}
  add("Content/Particles/"+p.id+".rparticle",DependencyClass::Data);
  for(const auto& l:p.layers){
   std::vector<StableId> textures={l.texture};textures.insert(textures.end(),l.extraTextures.begin(),l.extraTextures.end());
   for(const auto& id:textures)if(!id.empty()){
    PreparedMaterialTextureAsset prepared;if(!PrepareMaterialTextureAsset(root,project,id,prepared,error))return false;
    add(prepared.productProjectRelativePath,DependencyClass::Texture);hasTextures=true;
   }
  }
 }
 if(hasTextures){std::string registry;if(!ResolveAssetRegistryDocumentPath(root,registry,error))return false;
  auto relativeRegistry=std::filesystem::u8path(registry).lexically_relative(std::filesystem::u8path(root)).generic_u8string();
  auto resolved=ResolveDependencyPath(root,relativeRegistry);if(!resolved.accepted||!resolved.exists){error="Particle texture registry is unavailable inside the project.";return false;}add(resolved.canonicalRelativePath,DependencyClass::Data);}
 for(const auto& c:out){auto path=ResolveDependencyPath(root,c.declaredPath);if(!path.accepted||!path.exists){error="Particle runtime dependency is missing or outside this project: "+c.declaredPath;return false;}}
 return true;
}
bool SnapshotParticleEffectRuntimeInputs(const std::string& root,const StableId& project,const std::string& target,std::string& error){
 std::vector<DependencyCandidate> inputs;if(!CollectParticleEffectRuntimeDependencies(root,project,inputs,error))return false;
 std::error_code ec;
 for(const auto& input:inputs){auto path=ResolveDependencyPath(root,input.declaredPath);auto dest=std::filesystem::u8path(target)/std::filesystem::u8path(path.canonicalRelativePath);
  std::filesystem::create_directories(dest.parent_path(),ec);if(!ec)std::filesystem::copy_file(std::filesystem::u8path(path.absolutePath),dest,std::filesystem::copy_options::overwrite_existing,ec);
  if(ec){error="Could not snapshot particle input: "+input.declaredPath+": "+ec.message();return false;}}
 return true;
}
bool ParticleEffectRuntimeDependencyProvider::Discover(const DependencyProviderContext& c,const DependencyCandidateSink& emit,const DependencyDiagnosticSink&,std::string& error)const{
 if(!c.source){error="Particle dependency source is missing.";return false;}
 // Only the actual project descriptor is a catalog root, not other documents.
 auto ext=std::filesystem::u8path(c.source->projectRelativePath).extension().generic_u8string();if(ext!=".renegade"){error.clear();return true;}
 std::vector<DependencyCandidate> inputs;if(!CollectParticleEffectRuntimeDependencies(c.projectRoot,project_,inputs,error))return false;
 for(const auto& input:inputs)emit(input);return true;
}
}
