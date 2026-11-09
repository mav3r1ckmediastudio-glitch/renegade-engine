#include "renegade/bridge/ParticleTextureLibraryService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace renegade::bridge;
namespace fs=std::filesystem;
int failures=0;
void Check(bool value,const std::string& name){if(!value){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
int main(int argc,char**argv){
 if(argc!=2)return 2;std::string error;std::vector<ParticleTextureEntry> included;
 Check(ReadIncludedParticleTextures(argv[1],included,error),"read curated collection: "+error);Check(included.size()==50,"exactly fifty included textures");if(included.size()!=50)return 1;
 for(auto& e:included)if(e.id=="supplied-candle")Check(e.blend==wi::enums::BLENDMODE_ADDITIVE,"RGB candle has additive blending");else if(e.id=="supplied-smoke")Check(e.blend==wi::enums::BLENDMODE_ALPHA,"smoke has alpha blending");
 auto sheets=FilterParticleTextures(included,"All textures","",2),images=FilterParticleTextures(included,"All textures","",1);
 Check(sheets.size()==8&&images.size()==42,"eight supplied animated sheets and forty-two static/map entries");
 auto leaves=FilterParticleTextures(included,"Nature","leaf",2);Check(leaves.size()==3,"all three supplied leaf sheets");
 auto embers=FilterParticleTextures(included,"Fire & sparks","Ember fleck",1);Check(embers.size()==1,"ember atlas is a static selection");
 auto ember=included[embers.front()];Check(ember.columns==2&&ember.rows==2&&ember.start==3&&ember.frames==4&&ember.fps==0,"ember uses only the actual ember cell");
 auto invalid=ember;invalid.frames=0;Check(!ValidateParticleTextureEntry(invalid,error),"reject empty frame sequence");invalid=ember;invalid.fps=std::numeric_limits<float>::quiet_NaN();Check(!ValidateParticleTextureEntry(invalid,error),"reject nonfinite playback");
 const auto base=fs::temp_directory_path()/("particle-texture-library-"+GenerateStableId()),root=base/"Project";fs::create_directories(root/"Content");fs::create_directories(root/"SourceAssets");fs::create_directories(root/"Intermediate/Transactions");
 auto project=GenerateStableId();AssetRegistry registry;registry.projectId=project;Check(WriteAssetRegistry(root.string(),registry).success,"create governed registry");
 auto entry=included.front();entry.id="adoption-proof";auto external=base/"source.png";fs::copy_file(entry.sourcePath,external);entry.sourcePath=external.string();
 Check(AdoptParticleTexture(root.string(),project,entry,error),"first-use project adoption: "+error);Check(IsValidStableId(entry.assetId),"adoption returns governed texture identity");auto asset=entry.assetId;
 Check(ReadAssetRegistry(root.string(),project,registry,error),"read adopted registry");auto count=registry.records.size();fs::remove(external);
 entry.assetId.clear();Check(AdoptParticleTexture(root.string(),project,entry,error),"second use works without installed/source texture");Check(entry.assetId==asset,"second use preserves project texture identity");Check(ReadAssetRegistry(root.string(),project,registry,error)&&registry.records.size()==count,"second use adds no registry duplicates");
 entry.animated=true;entry.columns=entry.rows=4;entry.frames=16;entry.fps=18;entry.frameBlend=true;Check(SaveParticleTextureProfile(root.string(),project,entry,error),"save creator sheet interpretation");
 std::vector<ParticleTextureEntry> cold;Check(ReadProjectParticleTextures(root.string(),project,cold,error),"cold project catalogue");Check(cold.size()==1&&cold[0].assetId==asset&&cold[0].animated&&cold[0].frames==16&&cold[0].fps==18&&cold[0].frameBlend,"cold read retains complete layout and identity");
 PreparedMaterialTextureAsset prepared;Check(PrepareMaterialTextureAsset(root.string(),project,asset,prepared,error)&&!prepared.payload.empty(),"adopted product remains independently resolvable");
 Check(!ReadProjectParticleTextures(root.string(),GenerateStableId(),cold,error),"cross-project profile rejection");
 ParticleEffectLayer layer;layer.id=GenerateStableId();layer.emitter=MakeNewParticleEmitterState();layer.emitter.velocity={1,2,3};layer.emitter.gravity={0,-2,0};layer.emitter.life=7;layer.emitter.emitCount=19;layer.sizeVariation=.6f;layer.fieldInfluence={0,.4f,.8f,1};layer.transform.translation={4,5,6};
 auto untouched=layer;auto before=layer.emitter;ApplyParticleTextureToLayer(layer,entry,MaterialTextureSlot::BaseColor);
 Check(layer.texture==asset&&layer.emitter.framesX==4&&layer.emitter.frameCount==16&&layer.emitter.frameRate==18,"selection applies image and animation metadata");
 Check(layer.id==untouched.id&&layer.emitter.life==before.life&&layer.emitter.emitCount==before.emitCount&&layer.emitter.velocity.x==1&&layer.emitter.velocity.y==2&&layer.emitter.gravity.y==-2&&layer.sizeVariation==.6f&&layer.fieldInfluence[1]==.4f&&layer.transform.translation.y==5,"texture replacement preserves movement, variation, fields and layer transform");
 Check(untouched.texture.empty()&&untouched.emitter.frameCount==before.frameCount,"other layer remains unchanged");
 ember.assetId=asset;ApplyParticleTextureToLayer(layer,ember,MaterialTextureSlot::BaseColor);Check(layer.emitter.frameStart==3&&layer.emitter.framesX==2&&layer.emitter.frameRate==0,"static atlas selection preserves selected cell");
 auto normal=entry;normal.distortion=true;normal.animated=false;normal.columns=normal.rows=normal.frames=1;normal.start=0;normal.fps=0;ApplyParticleTextureToLayer(layer,normal,MaterialTextureSlot::BaseColor);Check(layer.extraTextures[0]==asset&&layer.emitter.shaderType==wi::EmittedParticleSystem::SOFT_DISTORTION&&layer.emitter.life==7,"distortion profile uses normal slot while preserving movement");
 fs::create_directories(base/"BadLibrary");{std::ofstream f(base/"BadLibrary/catalog.json");f<<R"({"format":"renegade-particle-textures","version":1,"entries":[{"id":"bad","name":"Bad","file":"../outside.png"}]})";}
 Check(!ReadIncludedParticleTextures((base/"BadLibrary").string(),cold,error),"reject escaping installed file paths");
 auto profilePath=root/"Content/Particles/TextureProfiles.rparticle-textures";{std::ofstream f(profilePath);f<<"broken";}
 Check(!ReadProjectParticleTextures(root.string(),project,cold,error),"malformed profiles fail visibly rather than hiding lost settings");
 fs::remove_all(base);std::cout<<"ParticleTextureLibraryTests failures="<<failures<<'\n';return failures?1:0;
}
