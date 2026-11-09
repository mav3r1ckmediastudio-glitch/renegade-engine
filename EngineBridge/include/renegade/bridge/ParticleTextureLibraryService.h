#pragma once
#include "renegade/bridge/ParticleEffectService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include <optional>

namespace renegade::bridge {
// Texture profiles describe atlas interpretation, never emitter movement.
struct ParticleTextureEntry {
 std::string id, assetId, name, category="Project", file, sourcePath;
 std::uint32_t width=0,height=0,columns=1,rows=1,frames=1,start=0;
 float fps=0;
 bool animated=false,distortion=false,frameBlend=false,included=false,layoutKnown=true;
 wi::enums::BLENDMODE blend=wi::enums::BLENDMODE_ALPHA;
};
bool ValidateParticleTextureEntry(const ParticleTextureEntry&,std::string&);
bool ReadIncludedParticleTextures(const std::string& directory,std::vector<ParticleTextureEntry>&,std::string&);
bool ReadProjectParticleTextures(const std::string& root,const StableId& project,std::vector<ParticleTextureEntry>&,std::string&);
bool SaveParticleTextureProfile(const std::string& root,const StableId& project,const ParticleTextureEntry&,std::string&);
bool AdoptParticleTexture(const std::string& root,const StableId& project,ParticleTextureEntry&,std::string&);
bool ImportParticleTexture(const std::string& root,const StableId& project,const std::string& path,ParticleTextureEntry&,std::string&);
wi::Resource LoadParticleTexturePreview(const std::string& root,const StableId& project,const ParticleTextureEntry&,std::string&);
void ApplyParticleTextureToLayer(ParticleEffectLayer&,const ParticleTextureEntry&,MaterialTextureSlot slot,bool recommendedBlend=true);
std::vector<std::size_t> FilterParticleTextures(const std::vector<ParticleTextureEntry>&,const std::string& category,const std::string& search,int kind);
std::string InstalledParticleTextureDirectory();
}
