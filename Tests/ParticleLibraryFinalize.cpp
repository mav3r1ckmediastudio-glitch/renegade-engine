#include "renegade/bridge/ParticleEffectService.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <algorithm>
#include <cctype>
#include <cstdlib>
using namespace renegade::bridge;
namespace fs=std::filesystem;
int main(int argc,char**argv){
 if(argc!=4)return 2;std::string source=argv[1],project=argv[2],stage=argv[3],error;
 auto all=ListParticleEffects(source,project,error);if(!error.empty())return 3;
 auto get=[&](const std::string& id)->ParticleEffect{for(auto& p:all)if(p.id==id)return p;std::cerr<<"Missing "<<id<<'\n';std::exit(4);};
 auto brazier=get("1eb6890c-29af-45b2-aaa2-a9938166bc57");
 auto approvedFire=get("5fb87b17-6069-45dc-a5f1-e2d3e2c33657");
 auto approvedEmbers=get("587ce72c-6317-462c-8a38-ad64eb05e61f");
 auto leaves=get("35028440-8559-49e2-96ce-0f7ad7f9a774");
 if(approvedEmbers.layers.size()!=3||approvedEmbers.fields.size()!=2||leaves.layers.size()!=3)return 5;
 auto emberPreset=approvedEmbers;emberPreset.id="d8d03770-9b3f-40ba-99c0-406f84762622";emberPreset.name="Embers";
 brazier.layers.clear();brazier.fields=approvedEmbers.fields;brazier.name="Brazier Fire";brazier.timing={};
 ParticleEffect shimmer=get("0099efb3-0053-42f9-974e-93b07e196528");shimmer.name="Heat Shimmer";shimmer.layers.clear();shimmer.fields.clear();shimmer.timing={};
 for(auto l:approvedFire.layers){
  std::string lower=l.name;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});
  if(lower.find("ember")!=std::string::npos)continue;
  l.fieldInfluence.fill(0);brazier.layers.push_back(l);
  if(l.emitter.shaderType==wi::EmittedParticleSystem::SOFT_DISTORTION){l.name="Heat shimmer";shimmer.layers.push_back(l);}
 }
 if(brazier.layers.size()!=3||shimmer.layers.size()!=1)return 6;
 for(auto l:approvedEmbers.layers){l.emitter.size*=.35f;brazier.layers.push_back(l);}
 leaves.name="Swirling Leaves";
 std::vector<ParticleEffect> final={brazier,get("b4dea157-f13a-490c-8d20-2ebcd2e26ec5"),emberPreset,leaves,get("447d49de-06fb-4875-b804-d55bbb98aa92"),get("a520615a-c8c2-4801-9c94-425285e7a7e0"),get("a305cd72-b21a-4e8d-b43e-a96662771b51"),shimmer};
 auto dir=fs::path(stage)/"Content/Particles";fs::create_directories(dir);
 for(auto& effect:final){
  fs::copy_file(fs::path(source)/"Content/Particles"/(effect.id+".rparticle"),dir/(effect.id+".rparticle"),fs::copy_options::overwrite_existing);
  if(!SaveParticleEffect(stage,project,effect,false,error)){std::cerr<<error;return 7;}
 }
 auto read=ListParticleEffects(stage,project,error);if(read.size()!=8||!error.empty())return 8;
 for(auto& effect:read)std::cout<<effect.id<<" | "<<effect.name<<" | layers="<<effect.layers.size()<<" fields="<<effect.fields.size()<<'\n';
 auto check=[&](const ParticleEffectLayer& a,const ParticleEffectLayer& b){
  return a.texture==b.texture&&a.emitter.size==b.emitter.size&&a.emitter.emitCount==b.emitter.emitCount&&a.emitter.velocity.y==b.emitter.velocity.y&&a.emitter.life==b.emitter.life&&a.emitter.frameStart==b.emitter.frameStart&&a.sizeVariation==b.sizeVariation&&a.speedVariation==b.speedVariation&&a.fieldInfluence==b.fieldInfluence;
 };
 for(auto& effect:read)if(effect.name=="Brazier Fire"||effect.name=="Embers"){
  size_t offset=effect.name=="Brazier Fire"?3:0;
  if(effect.fields.size()!=approvedEmbers.fields.size())return 9;
  for(size_t n=0;n<3;++n){auto expected=approvedEmbers.layers[n];if(offset)expected.emitter.size*=.35f;if(!check(effect.layers[n+offset],expected))return 10;}
 }
 auto referenceRoot=stage+"-approved-reference";fs::create_directories(referenceRoot);auto reference=approvedEmbers;reference.id.clear();
 if(!SaveParticleEffect(referenceRoot,project,reference,true,error))return 11;
 auto embedded=approvedEmbers;embedded.id.clear();embedded.layers.assign(brazier.layers.begin()+3,brazier.layers.end());embedded.fields=brazier.fields;
 if(!SaveParticleEffect(referenceRoot,project,embedded,true,error))return 12;
 auto body=[](const fs::path& path){std::ifstream in(path);std::string line,text;for(int i=0;i<5;++i)std::getline(in,line);while(std::getline(in,line))text+=line+"\n";return text;};
 auto scaled=approvedEmbers;scaled.id.clear();for(auto& l:scaled.layers)l.emitter.size*=.35f;if(!SaveParticleEffect(referenceRoot,project,scaled,true,error))return 14;
 auto expected=body(fs::path(referenceRoot)/"Content/Particles"/(reference.id+".rparticle"));
 if(body(dir/(emberPreset.id+".rparticle"))!=expected||body(fs::path(referenceRoot)/"Content/Particles"/(embedded.id+".rparticle"))!=body(fs::path(referenceRoot)/"Content/Particles"/(scaled.id+".rparticle")))return 13;
 std::cout<<"CURATED_LIBRARY_PASS approved_ember_layers=3 brazier_size_multiplier=0.35 final_presets=8\n";return 0;
}
