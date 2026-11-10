#include "renegade/bridge/ParticleTextureLibraryService.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include "json.hpp"
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <unordered_map>

namespace renegade::bridge {
namespace {
namespace fs=std::filesystem;using Json=nlohmann::json;
std::string Lower(std::string s){for(auto& c:s)c=char(std::tolower(static_cast<unsigned char>(c)));return s;}
fs::path Profiles(const std::string& root){return fs::path(root)/"Content/Particles/TextureProfiles.rparticle-textures";}
bool ReadJson(const fs::path& path,Json& j,std::string& error){
 try{std::error_code ec;if(fs::file_size(path,ec)>4*1024*1024||ec){error="Cannot read particle texture catalogue.";return false;}std::ifstream f(path);f>>j;return true;}
 catch(const std::exception&){error="Particle texture catalogue is malformed.";return false;}
}
Json Encode(const ParticleTextureEntry& e){return Json{{"id",e.id},{"asset_id",e.assetId},{"name",e.name},{"category",e.category},{"file",e.file},{"width",e.width},{"height",e.height},{"columns",e.columns},{"rows",e.rows},{"frames",e.frames},{"start",e.start},{"fps",e.fps},{"animated",e.animated},{"distortion",e.distortion},{"layout_known",e.layoutKnown},{"frame_blending",e.frameBlend},{"blend",int(e.blend)}};}
bool Decode(const Json& j,ParticleTextureEntry& e,std::string& error){
 try{e.id=j.value("id","");e.assetId=j.value("asset_id","");e.name=j.at("name").get<std::string>();e.category=j.value("category","Project");e.file=j.value("file","");e.width=j.value("width",0u);e.height=j.value("height",0u);e.columns=j.value("columns",1u);e.rows=j.value("rows",1u);e.frames=j.value("frames",1u);e.start=j.value("start",0u);e.fps=j.value("fps",0.f);e.animated=j.value("animated",false);e.distortion=j.value("distortion",false);e.layoutKnown=j.value("layout_known",true);e.frameBlend=j.value("frame_blending",false);e.blend=static_cast<wi::enums::BLENDMODE>(j.value("blend",int(wi::enums::BLENDMODE_ALPHA)));return ValidateParticleTextureEntry(e,error);}
 catch(const std::exception&){error="Invalid particle texture profile.";return false;}
}
bool ReadProfiles(const std::string& root,const StableId& project,std::vector<ParticleTextureEntry>& out,std::string& error){
 out.clear();std::error_code ec;if(!fs::exists(Profiles(root),ec)){if(ec){error=ec.message();return false;}return true;}
 Json j;if(!ReadJson(Profiles(root),j,error))return false;
 try{if(j.at("format")!="renegade-particle-texture-profiles"||j.at("version")!=1||j.at("project_id")!=project||!j.at("entries").is_array()||j.at("entries").size()>10000){error="Particle texture profiles belong to another project or version.";return false;}
 std::set<std::string> ids;for(const auto& item:j.at("entries")){ParticleTextureEntry e;if(!Decode(item,e,error)||!IsValidStableId(e.assetId)||!ids.insert(e.assetId).second){error="Invalid or repeated project texture profile.";return false;}out.push_back(std::move(e));}return true;}
 catch(const std::exception&){error="Invalid particle texture profile document.";return false;}
}
}
bool ValidateParticleTextureEntry(const ParticleTextureEntry& e,std::string& error){
 if(e.name.empty()||e.name.size()>160||e.category.size()>80||e.id.size()>160||e.columns<1||e.columns>32||e.rows<1||e.rows>32||e.frames<1||e.frames>e.columns*e.rows||e.start>=e.frames||!std::isfinite(e.fps)||e.fps<0||e.fps>240||int(e.blend)<0||int(e.blend)>=wi::enums::BLENDMODE_COUNT){error="Invalid particle texture name, atlas layout or playback settings.";return false;}error.clear();return true;
}
std::string InstalledParticleTextureDirectory(){
 wchar_t p[32768];auto n=GetModuleFileNameW(nullptr,p,32768);if(!n||n>=32768)return {};return (fs::path(std::wstring(p,n)).parent_path()/"Content/Particles/TextureLibrary").string();
}
bool ReadIncludedParticleTextures(const std::string& directory,std::vector<ParticleTextureEntry>& out,std::string& error){
 out.clear();Json j;if(!ReadJson(fs::path(directory)/"catalog.json",j,error))return false;
 try{if(j.at("format")!="renegade-particle-textures"||j.at("version")!=1||!j.at("entries").is_array()||j.at("entries").size()>2048){error="Unsupported included particle texture catalogue.";return false;}
 std::set<std::string> ids;std::error_code ec;auto base=fs::weakly_canonical(directory,ec);if(ec){error=ec.message();return false;}
 for(const auto& item:j.at("entries")){ParticleTextureEntry e;if(!Decode(item,e,error)||e.id.empty()||!ids.insert(e.id).second){error="Invalid or repeated included particle texture.";return false;}
 auto relative=fs::path(e.file);if(relative.empty()||relative.is_absolute()||relative.has_root_name()){error="Particle texture path must be relative.";return false;}
 for(const auto& part:relative)if(part==".."){error="Particle texture path escapes its library.";return false;}
 auto resolved=fs::weakly_canonical(base/relative,ec);auto inside=resolved.lexically_relative(base);if(ec||inside.empty()||*inside.begin()==".."||!fs::is_regular_file(resolved,ec)){error="Included particle texture is missing or outside its library.";return false;}
 auto ext=Lower(resolved.extension().string());if(ext!=".png"&&ext!=".tga"&&ext!=".dds"&&ext!=".jpg"&&ext!=".jpeg"){error="Unsupported particle image format.";return false;}
 e.sourcePath=resolved.string();e.included=true;out.push_back(std::move(e));}error.clear();return true;}
 catch(const std::exception&){error="Invalid included particle texture catalogue.";return false;}
}
bool ReadProjectParticleTextures(const std::string& root,const StableId& project,std::vector<ParticleTextureEntry>& out,std::string& error){
 out.clear();std::vector<ParticleTextureEntry> profiles;if(!ReadProfiles(root,project,profiles,error))return false;
 AssetRegistry registry;std::error_code ec;if(!fs::exists(fs::path(root)/"AssetRegistry.renegade-assets",ec))return true;
 if(!ReadAssetRegistry(root,project,registry,error))return false;
 std::unordered_map<std::string,ParticleTextureEntry> byId;for(auto& p:profiles)byId.emplace(p.assetId,p);
 for(const auto& a:registry.records)if(a.dependencyClass==DependencyClass::Texture&&Lower(fs::path(a.projectRelativePath).extension().string())==".rasset"){
  auto it=byId.find(a.assetId);ParticleTextureEntry e;if(it!=byId.end())e=it->second;else{e.id="project-"+a.assetId;e.assetId=a.assetId;e.name=fs::path(a.projectRelativePath).stem().string();e.layoutKnown=false;}
  e.included=false;e.sourcePath.clear();out.push_back(std::move(e));
 }
 std::sort(out.begin(),out.end(),[](auto& a,auto& b){return a.name<b.name;});error.clear();return true;
}
bool SaveParticleTextureProfile(const std::string& root,const StableId& project,const ParticleTextureEntry& entry,std::string& error){
 if(!IsValidStableId(project)||!IsValidStableId(entry.assetId)||!ValidateParticleTextureEntry(entry,error))return false;
 // Profiles supplement registered asset identities; never register another ID.
 AssetRegistry registry;if(!ReadAssetRegistry(root,project,registry,error))return false;
 auto found=std::find_if(registry.records.begin(),registry.records.end(),[&](auto& a){return a.assetId==entry.assetId&&a.dependencyClass==DependencyClass::Texture;});
 if(found==registry.records.end()){error="Texture profile has no registered project texture.";return false;}
 std::vector<ParticleTextureEntry> profiles;if(!ReadProfiles(root,project,profiles,error))return false;
 auto it=std::find_if(profiles.begin(),profiles.end(),[&](auto& p){return p.assetId==entry.assetId;});if(it==profiles.end())profiles.push_back(entry);else *it=entry;
 Json j={{"format","renegade-particle-texture-profiles"},{"version",1},{"project_id",project},{"entries",Json::array()}};for(auto& p:profiles)j["entries"].push_back(Encode(p));
 std::string json=j.dump(2);ProjectDocumentWrite write;write.destinationPath=Profiles(root).string();write.content.assign(json.begin(),json.end());
 write.validator=[project](const std::string& path,std::string& error){Json j;if(!ReadJson(path,j,error))return false;try{return j.at("project_id")==project&&j.at("format")=="renegade-particle-texture-profiles";}catch(...){return false;}};
 ProjectDocumentTransactionOptions options;options.allowedRoot=fs::absolute(root).string();options.journalDirectory=(fs::path(root)/"Intermediate/Transactions").string();auto result=ProjectDocumentTransaction{}.Execute({std::move(write)},options);if(!result.success){error=result.message;return false;}error.clear();return true;
}
bool AdoptParticleTexture(const std::string& root,const StableId& project,ParticleTextureEntry& e,std::string& error){
 if(!ValidateParticleTextureEntry(e,error))return false;
 std::vector<ParticleTextureEntry> existing;if(!ReadProfiles(root,project,existing,error))return false;
 if(e.included){auto it=std::find_if(existing.begin(),existing.end(),[&](auto& p){return p.id==e.id;});
  if(it!=existing.end()){PreparedMaterialTextureAsset prepared;if(PrepareMaterialTextureAsset(root,project,it->assetId,prepared,error))e.assetId=it->assetId;else return false;}
  else{auto imported=CreatorTextureWorkflowService{}.ImportTexture(root,project,e.sourcePath);if(!imported.succeeded){error=imported.error;return false;}e.assetId=imported.assetId;}
 }
 return SaveParticleTextureProfile(root,project,e,error);
}
bool ImportParticleTexture(const std::string& root,const StableId& project,const std::string& path,ParticleTextureEntry& e,std::string& error){
 auto imported=CreatorTextureWorkflowService{}.ImportTexture(root,project,path);if(!imported.succeeded){error=imported.error;return false;}
 e={};e.assetId=imported.assetId;e.id="project-"+e.assetId;e.name=fs::path(path).stem().string();e.category="Project";e.layoutKnown=false;return SaveParticleTextureProfile(root,project,e,error);
}
wi::Resource LoadParticleTexturePreview(const std::string& root,const StableId& project,const ParticleTextureEntry& e,std::string& error){
 if(e.included){auto resource=wi::resourcemanager::Load(e.sourcePath);if(!resource.IsValid())error="Cannot preview this included texture.";return resource;}
 PreparedMaterialTextureAsset p;if(!PrepareMaterialTextureAsset(root,project,e.assetId,p,error))return {};return LoadPreparedMaterialTextureAsset(p,error);
}
void ApplyParticleTextureToLayer(ParticleEffectLayer& layer,const ParticleTextureEntry& e,MaterialTextureSlot slot,bool recommended){
 if(e.distortion){slot=MaterialTextureSlot::Normal;layer.emitter.shaderType=wi::EmittedParticleSystem::SOFT_DISTORTION;}
 if(slot==MaterialTextureSlot::BaseColor)layer.texture=e.assetId;else{
  size_t index=slot==MaterialTextureSlot::Normal?0:slot==MaterialTextureSlot::Surface?1:slot==MaterialTextureSlot::Emissive?2:3;layer.extraTextures[index]=e.assetId;
 }
 if(slot==MaterialTextureSlot::BaseColor||e.distortion){layer.emitter.framesX=e.columns;layer.emitter.framesY=e.rows;layer.emitter.frameCount=e.frames;layer.emitter.frameStart=e.start;layer.emitter.frameRate=e.fps;layer.emitter.frameBlending=e.frameBlend;if(recommended)layer.material.blendMode=e.blend;}
}
std::vector<std::size_t> FilterParticleTextures(const std::vector<ParticleTextureEntry>& entries,const std::string& category,const std::string& search,int kind){
 std::vector<std::size_t> out;auto query=Lower(search);for(size_t i=0;i<entries.size();++i){auto& e=entries[i];if(!category.empty()&&category!="All textures"&&e.category!=category)continue;if(kind==1&&e.animated)continue;if(kind==2&&!e.animated)continue;if(Lower(e.name+" "+e.category).find(query)==std::string::npos)continue;out.push_back(i);}return out;
}
}
