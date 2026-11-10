#include "renegade/bridge/ParticleEffectService.h"
#include <algorithm>
#include <cmath>
#include <sstream>
namespace renegade::bridge {
bool ValidateParticleLifetime(const ParticleLifetimeCurve& curve,std::string& error){
 if(curve.keys.size()<2||curve.keys.size()>8){error="Lifetime curves need 2-8 points.";return false;}
 if(curve.keys.front().time!=0||curve.keys.back().time!=1){error="Lifetime curves must start at 0% and end at 100%.";return false;}
 float previous=-1;
 for(const auto& k:curve.keys){
  if(!std::isfinite(k.time)||k.time<=previous||k.time<0||k.time>1||!std::isfinite(k.size)||k.size<0||k.size>4){
   error="Lifetime point times must increase; size must be between 0 and 400%.";return false;}
  for(float v:{k.colour.x,k.colour.y,k.colour.z,k.colour.w})if(!std::isfinite(v)||v<0||v>1){error="Lifetime colour and opacity must be between 0 and 100%.";return false;}
  previous=k.time;
 }error.clear();return true;
}
ParticleLifetimeKey SampleParticleLifetime(const ParticleLifetimeCurve& c,float time){
 ParticleLifetimeKey result;result.time=std::clamp(std::isfinite(time)?time:0.0f,0.0f,1.0f);
 if(!c.enabled||c.keys.size()<2)return result;
 auto upper=std::upper_bound(c.keys.begin(),c.keys.end(),result.time,[](float t,const auto& k){return t<k.time;});
 if(upper==c.keys.begin())return *upper;
 if(upper==c.keys.end())return c.keys.back();
 const auto& a=*(upper-1);const auto& b=*upper;
 float t=(result.time-a.time)/(b.time-a.time);
 result.size=a.size+(b.size-a.size)*t;
 result.colour={a.colour.x+(b.colour.x-a.colour.x)*t,a.colour.y+(b.colour.y-a.colour.y)*t,a.colour.z+(b.colour.z-a.colour.z)*t,a.colour.w+(b.colour.w-a.colour.w)*t};
 return result;
}
void WriteParticleLifetime(std::ostream& out,const ParticleLifetimeCurve& c){
 out<<c.enabled<<' '<<c.keys.size()<<' ';
 for(const auto& k:c.keys)out<<k.time<<' '<<k.size<<' '<<k.colour.x<<' '<<k.colour.y<<' '<<k.colour.z<<' '<<k.colour.w<<' ';
 out<<'\n';
}
bool ReadParticleLifetime(std::istream& in,ParticleLifetimeCurve& result){
 ParticleLifetimeCurve c;unsigned count;
 if(!(in>>c.enabled>>count)||count<2||count>8)return false;c.keys.resize(count);
 for(auto& k:c.keys)if(!(in>>k.time>>k.size>>k.colour.x>>k.colour.y>>k.colour.z>>k.colour.w))return false;
 std::string error;if(!ValidateParticleLifetime(c,error))return false;result=std::move(c);return true;
}
void HydrateParticleLifetime(wi::scene::Scene& scene,wi::ecs::Entity entity){
 auto* emitter=scene.emitters.GetComponent(entity);auto* meta=scene.metadatas.GetComponent(entity);
 if(!emitter||!wi::graphics::GetDevice())return;
 const bool custom=emitter->GetOpacityCurveTex()->GetDesc().array_size==2;
 const bool authored=meta&&meta->string_values.has("renegade.particle.lifetime");
 if(!authored||meta->string_values.has("renegade.particle.stationary.authored")){
  if(custom)emitter->SetOpacityCurveControl(emitter->opacityCurveControlPeakStart,emitter->opacityCurveControlPeakEnd);return;
 }
 const auto& source=meta->string_values.get("renegade.particle.lifetime");
 if(!custom&&source.rfind("0 ",0)==0)return;
 if(custom&&meta->string_values.has("renegade.particle.lifetime.applied")&&meta->string_values.get("renegade.particle.lifetime.applied")==source)return;
 ParticleLifetimeCurve curve;std::istringstream input(source);
 if(!ReadParticleLifetime(input,curve)){
  wi::backlog::post("Invalid particle lifetime metadata.",wi::backlog::LogLevel::Error);return;
 }
 if(!curve.enabled){if(custom)emitter->SetOpacityCurveControl(emitter->opacityCurveControlPeakStart,emitter->opacityCurveControlPeakEnd);return;}
 if(custom&&meta->string_values.has("renegade.particle.lifetime.applied")&&meta->string_values.get("renegade.particle.lifetime.applied")==source)return;
 std::string error;if(!EnsureParticleMovementShaders(error)){wi::backlog::post(error,wi::backlog::LogLevel::Error);return;}
 constexpr size_t width=256;
 std::array<XMFLOAT4,width> colours,sizes;
 for(size_t n=0;n<width;++n){
  // Texel centres map exactly to normalized lifetime samples n/(width-1).
  auto k=SampleParticleLifetime(curve,float(n)/(width-1));colours[n]=k.colour;sizes[n]={k.size,0,0,0};
 }
 wi::graphics::TextureDesc desc;desc.type=wi::graphics::TextureDesc::Type::TEXTURE_1D;desc.width=width;desc.array_size=2;desc.format=wi::graphics::Format::R32G32B32A32_FLOAT;desc.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
 wi::graphics::SubresourceData data[2];data[0].data_ptr=colours.data();data[0].row_pitch=sizeof(colours);data[0].slice_pitch=sizeof(colours);data[1].data_ptr=sizes.data();data[1].row_pitch=sizeof(sizes);data[1].slice_pitch=sizeof(sizes);
 wi::graphics::Texture texture;
 if(!wi::graphics::GetDevice()->CreateTexture(&desc,data,&texture)){wi::backlog::post("Could not create particle lifetime lookup.",wi::backlog::LogLevel::Error);return;}
 wi::graphics::GetDevice()->SetName(&texture,"Renegade particle lifetime");
 emitter->SetOpacityCurveTex(texture);const auto applied=source;meta->string_values.set("renegade.particle.lifetime.applied",applied);
}
}
