#include "renegade/bridge/ParticleEffectService.h"
#include <Windows.h>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <cmath>
#include <algorithm>
namespace renegade::bridge {
bool EnsureParticleMovementShaders(std::string& error) {
 if(!wi::graphics::GetDevice()||wi::EmittedParticleSystem::MovementFieldShadersReady())return true;
 wchar_t path[32768];DWORD size=GetModuleFileNameW(nullptr,path,32768);
 if(!size||size>=32768){error="Cannot locate particle movement shaders.";return false;}
 auto directory=std::filesystem::path(std::wstring(path,size)).parent_path()/"Content/particles";
 wi::graphics::Shader shaders[7];
 for(int i=0;i<7;++i){
  auto file=directory/("MovementField"+std::to_string(i)+".cso");std::ifstream input(file,std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
  if(bytes.empty()||!wi::graphics::GetDevice()->CreateShader(wi::graphics::ShaderStage::CS,bytes.data(),bytes.size(),&shaders[i])){error="Particle movement shader missing or invalid: "+file.filename().string();return false;}
 }
 wi::EmittedParticleSystem::SetMovementFieldShaders(shaders);error.clear();return true;
}
void HydrateParticleMovementFields(wi::scene::Scene& scene) {
 bool ready=false;
 for(size_t i=0;i<scene.emitters.GetCount();++i){
  auto& emitter=scene.emitters[i];auto entity=scene.emitters.GetEntity(i);HydrateParticleShape(scene,entity);HydrateParticleLifetime(scene,entity);const auto* meta=scene.metadatas.GetComponent(entity);
  const bool authored=meta&&meta->string_values.has("renegade.particle.movement.runtime");
  if(!authored){if(!emitter.movementFieldSource.empty()){emitter.movementFieldSource.clear();emitter.movementFields.clear();emitter.movementSizeVariation=emitter.movementSpeedVariation=0;}continue;}
  const auto& source=meta->string_values.get("renegade.particle.movement.runtime");
  if(source!=emitter.movementFieldSource){
   emitter.movementFieldSource=source;emitter.movementFields.clear();std::istringstream data(source);unsigned count;bool root;
   std::string version;data>>version;bool latest=version=="v2";if(!latest){data.clear();data.str(source);}
   if(!(data>>count>>root)||count>4)continue;emitter.movementFieldRootSpace=root;emitter.movementSizeVariation=emitter.movementSpeedVariation=0;
   if(latest&&!(data>>emitter.movementSizeVariation>>emitter.movementSpeedVariation))continue;
   for(unsigned n=0;n<count;++n){wi::EmittedParticleSystem::MovementField f;
    for(auto& row:f.localToEffect.m)for(auto& value:row)data>>value;
    data>>f.type>>f.strength>>f.influence>>f.falloff>>f.lift>>f.noiseScale>>f.animationSpeed;
    if(!data){emitter.movementFields.clear();break;}if(f.influence>0)emitter.movementFields.push_back(f);
   }
  }
  if(emitter.movementFields.empty()&&emitter.movementSizeVariation==0&&emitter.movementSpeedVariation==0)continue;
  if(!ready){std::string error;ready=EnsureParticleMovementShaders(error);if(!ready){wi::backlog::post(error,wi::backlog::LogLevel::Error);return;}}
  auto* transform=scene.transforms.GetComponent(entity);
  if(transform&&!emitter.movementFieldRootSpace){
   auto local=XMMatrixScaling(transform->scale_local.x,transform->scale_local.y,transform->scale_local.z)*XMMatrixRotationQuaternion(XMLoadFloat4(&transform->rotation_local))*XMMatrixTranslation(transform->translation_local.x,transform->translation_local.y,transform->translation_local.z);
   XMStoreFloat4x4(&emitter.movementFieldParentToEmitter,XMMatrixInverse(nullptr,local));
  }else emitter.movementFieldParentToEmitter=wi::math::IDENTITY_MATRIX;
 }
}
XMFLOAT3 SampleParticleMovementField(const ParticleMovementField& f,const XMFLOAT3& position,float time) {
 if(!f.enabled)return {};
 const auto& t=f.transform;
 auto matrix=XMMatrixScaling(t.scale.x,t.scale.y,t.scale.z)*XMMatrixRotationQuaternion(XMLoadFloat4(&t.rotation))*XMMatrixTranslation(t.translation.x,t.translation.y,t.translation.z);
 XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(XMLoadFloat3(&position),XMMatrixInverse(nullptr,matrix)));
 float edge=std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)});if(edge>=1)return {};
 float fade=f.falloff<=0?1:std::clamp((1-edge)/f.falloff,0.0f,1.0f);fade=fade*fade*(3-2*fade);
 float strength=f.strength*(f.type==ParticleFieldType::Swirl&&f.clockwise?-1:1);XMFLOAT3 v={};
 if(f.type==ParticleFieldType::Swirl){float radius=std::sqrt(p.x*p.x+p.z*p.z);if(radius>.00001f){float s=strength*std::min(radius/.2f,1.0f)/radius;v={-p.z*s,0,p.x*s};}v.y=f.lift;}
 else if(f.type==ParticleFieldType::Turbulence){float x=p.x*f.noiseScale*XM_PI,y=p.y*f.noiseScale*XM_PI,z=p.z*f.noiseScale*XM_PI,phase=time*f.animationSpeed;
  float a=std::cos(y+1.7f*z+phase),b=std::cos(z+1.3f*x-.8f*phase),c=std::cos(x+1.9f*y+.6f*phase);
  v={(1.9f*c-b)*.4f*strength,(1.7f*a-c)*.4f*strength+f.lift,(1.3f*b-a)*.4f*strength};
 }else if(f.type==ParticleFieldType::Updraft)v={0,strength+f.lift,0};else v={0,f.lift,strength};
 auto rotation=XMMatrixRotationQuaternion(XMLoadFloat4(&t.rotation));XMFLOAT3 result;XMStoreFloat3(&result,XMVector3TransformNormal(XMLoadFloat3(&v)*fade,rotation));return result;
}
}
