#include "renegade/bridge/ParticleEffectService.h"
#include <algorithm>
#include <cmath>
#include <sstream>
namespace renegade::bridge {
bool ValidateParticleShape(const ParticleEmissionShape& s,std::string& error){
 if(int(s.type)<0||int(s.type)>6||!std::isfinite(s.radius)||!std::isfinite(s.height)||!std::isfinite(s.angle)||!std::isfinite(s.thickness)||s.radius<0||s.radius>10000||s.height<0||s.height>10000||s.angle<0||s.angle>90||s.thickness<0||s.thickness>10000){error="Invalid emission shape dimensions or angle.";return false;}error.clear();return true;
}
ParticleShapeSample SampleParticleShape(const ParticleEmissionShape& s,XMFLOAT3 u){
 u={std::clamp(u.x,0.0f,1.0f),std::clamp(u.y,0.0f,1.0f),std::clamp(u.z,0.0f,1.0f)};
 ParticleShapeSample sample;float phi=XM_2PI*u.y,c=std::cos(phi),a=std::sin(phi);
 if(s.type==ParticleShapeType::Box)sample.position={u.x*2-1,u.y*2-1,u.z*2-1};
 else if(s.type==ParticleShapeType::Sphere){
  float z=1-2*u.x,h=std::sqrt(std::max(0.0f,1-z*z)),radius=s.radius*(s.surface?1:std::cbrt(u.z));
  sample.normal={h*c,h*a,z};sample.position={sample.normal.x*radius,sample.normal.y*radius,z*radius};
 }else if(s.type==ParticleShapeType::Cone){
  float radius=s.radius*std::sqrt(u.x),z=1-u.z*(1-std::cos(XMConvertToRadians(s.angle))),h=std::sqrt(std::max(0.0f,1-z*z));
  sample.position={radius*c,radius*a,0};sample.normal={h*c,h*a,z};
 }else if(s.type==ParticleShapeType::Disc||s.type==ParticleShapeType::Ring||s.type==ParticleShapeType::Cylinder){
  float radius=s.radius*std::sqrt(u.x);
  if(s.type==ParticleShapeType::Ring){float lo=std::max(0.0f,s.radius-s.thickness*.5f),hi=s.radius+s.thickness*.5f;radius=std::sqrt(lo*lo+(hi*hi-lo*lo)*u.x);}
  sample.position={radius*c,radius*a,s.type==ParticleShapeType::Cylinder?(u.z-.5f)*s.height:0};
  sample.normal=s.type==ParticleShapeType::Cylinder?XMFLOAT3(c,a,0):XMFLOAT3(0,0,1);
 }return sample;
}
void WriteParticleShape(std::ostream& out,const ParticleEmissionShape& s){out<<int(s.type)<<' '<<s.radius<<' '<<s.height<<' '<<s.angle<<' '<<s.thickness<<' '<<s.surface<<'\n';}
bool ReadParticleShape(std::istream& in,ParticleEmissionShape& result){ParticleEmissionShape s;int type;if(!(in>>type>>s.radius>>s.height>>s.angle>>s.thickness>>s.surface))return false;s.type=ParticleShapeType(type);std::string error;if(!ValidateParticleShape(s,error))return false;result=s;return true;}
void HydrateParticleShape(wi::scene::Scene& scene,wi::ecs::Entity e){
 auto* emitter=scene.emitters.GetComponent(e);if(!emitter)return;auto* meta=scene.metadatas.GetComponent(e);
 if(!meta||!meta->string_values.has("renegade.particle.shape")){emitter->emissionShape=emitter->IsVolumeEnabled()?1:0;emitter->emissionShapeSource.clear();return;}
 const auto& source=meta->string_values.get("renegade.particle.shape");if(emitter->emissionShapeSource==source)return;
 ParticleEmissionShape s;std::istringstream in(source);if(!ReadParticleShape(in,s)){wi::backlog::post("Invalid particle emission shape.",wi::backlog::LogLevel::Error);return;}
 if(int(s.type)>1&&wi::graphics::GetDevice()){std::string error;if(!EnsureParticleMovementShaders(error)){wi::backlog::post(error,wi::backlog::LogLevel::Error);return;}}
 emitter->emissionShape=uint32_t(s.type);emitter->emissionShapeSurface=s.surface;emitter->emissionShapeParams={s.radius,s.height,std::cos(XMConvertToRadians(s.angle)),s.thickness};emitter->emissionShapeSource=source;
}
}
