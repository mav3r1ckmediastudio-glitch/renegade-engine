#include "renegade/bridge/ParticleEffectService.h"
#include "renegade/bridge/ParticleEffectRuntimeService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <type_traits>
namespace renegade::bridge {
namespace {
constexpr MaterialTextureSlot ExtraSlots[]={MaterialTextureSlot::Normal,MaterialTextureSlot::Surface,MaterialTextureSlot::Emissive,MaterialTextureSlot::Occlusion};
template<class F> void Visit(ParticleEmitterState& s,F f){
f(s.shaderType);f(s.meshId);f(s.maxParticles);f(s.fixedTimestep);f(s.size);f(s.randomFactor);f(s.normalFactor);f(s.emitCount);f(s.life);f(s.randomLife);f(s.scaleX);f(s.scaleY);f(s.rotation);f(s.motionBlurAmount);f(s.mass);f(s.randomColor);f(s.opacityPeakStart);f(s.opacityPeakEnd);f(s.burstOnCreate);f(s.velocity.x);f(s.velocity.y);f(s.velocity.z);f(s.gravity.x);f(s.gravity.y);f(s.gravity.z);f(s.drag);f(s.restitution);f(s.sphH);f(s.sphK);f(s.sphP0);f(s.sphE);f(s.framesX);f(s.framesY);f(s.frameCount);f(s.frameStart);f(s.frameRate);f(s.paused);f(s.sorted);f(s.depthCollision);f(s.sph);f(s.volume);f(s.frameBlending);f(s.collidersDisabled);f(s.takeColorFromMesh);f(s.color.x);f(s.color.y);f(s.color.z);f(s.color.w);f(s.emissiveColor.x);f(s.emissiveColor.y);f(s.emissiveColor.z);f(s.emissiveStrength);
}
template<class F> void Visit(MaterialState& s,F f){
f(s.shaderType);f(s.blendMode);f(s.baseColor.x);f(s.baseColor.y);f(s.baseColor.z);f(s.baseColor.w);f(s.metalness);f(s.roughness);f(s.reflectance);f(s.normalMapStrength);f(s.alphaRef);f(s.emissiveColor.x);f(s.emissiveColor.y);f(s.emissiveColor.z);f(s.emissiveStrength);f(s.receiveShadow);f(s.castShadow);f(s.useVertexColors);f(s.doubleSided);f(s.texMulAdd.x);f(s.texMulAdd.y);f(s.texMulAdd.z);f(s.texMulAdd.w);f(s.parallaxOcclusionMapping);f(s.anisotropyStrength);f(s.anisotropyRotationDegrees);f(s.sheenColor.x);f(s.sheenColor.y);f(s.sheenColor.z);f(s.sheenRoughness);f(s.clearcoat);f(s.clearcoatRoughness);f(s.transmission);f(s.refraction);f(s.blendWithTerrainHeight);f(s.meshBlend);f(s.interiorMappingScale.x);f(s.interiorMappingScale.y);f(s.interiorMappingScale.z);f(s.interiorMappingOffset.x);f(s.interiorMappingOffset.y);f(s.interiorMappingOffset.z);f(s.interiorMappingRotationDegrees);
}

std::string CleanName(std::string name) {
 name.erase(std::remove_if(name.begin(),name.end(),[](unsigned char c){return c<32;}),name.end());
 if(name.empty())name="New Effect";if(name.size()>120)name.resize(120);return name;
}
template<class F> void VisitTransform(TransformState& t,F f) {
 for(float* v:{&t.translation.x,&t.translation.y,&t.translation.z,&t.rotation.x,&t.rotation.y,&t.rotation.z,&t.rotation.w,&t.scale.x,&t.scale.y,&t.scale.z})f(*v);
}
template<class F> void VisitField(ParticleMovementField& f,F visit) {
 VisitTransform(f.transform,visit);visit(f.strength);visit(f.falloff);visit(f.lift);visit(f.noiseScale);visit(f.animationSpeed);visit(f.enabled);visit(f.clockwise);
}
void WriteFields(std::ostream& out,const std::vector<ParticleMovementField>& fields) {
 out<<fields.size()<<'\n';
 for(auto f:fields){out<<f.id<<' '<<std::quoted(f.name)<<' '<<int(f.type)<<' ';VisitField(f,[&](auto v){out<<v<<' ';});out<<'\n';}
}
bool ReadFields(std::istream& in,std::vector<ParticleMovementField>& fields) {
 unsigned n;if(!(in>>n)||n>4)return false;
 for(unsigned i=0;i<n;++i){ParticleMovementField f;int type;if(!(in>>f.id>>std::quoted(f.name)>>type)||type<0||type>3)return false;f.type=ParticleFieldType(type);VisitField(f,[&](auto& v){in>>v;});if(!in)return false;fields.push_back(f);}return true;
}
XMMATRIX Matrix(const TransformState& t) {return XMMatrixScaling(t.scale.x,t.scale.y,t.scale.z)*XMMatrixRotationQuaternion(XMLoadFloat4(&t.rotation))*XMMatrixTranslation(t.translation.x,t.translation.y,t.translation.z);}
bool Read(const std::filesystem::path& file,const StableId& project,ParticleEffect& p) {
 std::ifstream in(file);std::string magic,owner;unsigned version,n;
 if(!(in>>magic>>version>>owner>>p.id>>std::quoted(p.name)>>n)||magic!="renegade-particle-effect"||(version<1||version>7)||owner!=project||n==0||n>32)return false;
 for(unsigned i=0;i<n;++i){
  ParticleEffectLayer l;
  if(!(in>>l.id>>std::quoted(l.name)>>l.enabled>>std::quoted(l.texture)))return false;
  if(version>=2)for(auto& texture:l.extraTextures)if(!(in>>std::quoted(texture)))return false;
  if(version>=3){int mode;if(!(in>>mode)||mode<0||mode>1)return false;l.playback=static_cast<ParticlePlayback>(mode);}
  if(version>=4)for(auto& weight:l.fieldInfluence)if(!(in>>weight))return false;
  if(version>=5&&!(in>>l.sizeVariation>>l.speedVariation))return false;
  if(version>=6&&!ReadParticleLifetime(in,l.lifetime))return false;
  if(version>=7&&!ReadParticleShape(in,l.shape))return false;
  auto read=[&](auto& v){using T=std::decay_t<decltype(v)>;if constexpr(std::is_enum_v<T>){int x=0;in>>x;v=static_cast<T>(x);}else in>>v;};
  Visit(l.emitter,read);Visit(l.material,read);VisitTransform(l.transform,read);if(version<7&&l.emitter.volume)l.shape.type=ParticleShapeType::Box;if(!in)return false;p.layers.push_back(l);
 }
 if(version>=4&&!ReadFields(in,p.fields))return false;
 if(version>=7&&!ReadParticleTiming(in,p.timing))return false;
 std::string error;return ValidateParticleEffect(p,error);
}
void Snapshot(wi::scene::Scene& s,wi::ecs::Entity e,wi::Archive& a,std::vector<std::pair<wi::ecs::Entity,wi::scene::MaterialComponent>>& mats){
 a=wi::Archive();wi::ecs::EntitySerializer ser;s.Entity_Serialize(a,ser,e,wi::scene::Scene::EntitySerializeFlags::RECURSIVE);wi::jobsystem::Wait(ser.ctx);
 mats.clear();for(size_t i=0;i<s.materials.GetCount();++i){auto x=s.materials.GetEntity(i);if(x==e||s.Entity_IsDescendant(x,e))mats.emplace_back(x,s.materials[i]);}
}
void Restore(wi::scene::Scene& s,wi::Archive& a,const std::vector<std::pair<wi::ecs::Entity,wi::scene::MaterialComponent>>& mats){
 a.SetReadModeAndResetPos(true);wi::ecs::EntitySerializer ser;ser.allow_remap=false;s.Entity_Serialize(a,ser,wi::ecs::INVALID_ENTITY,wi::scene::Scene::EntitySerializeFlags::RECURSIVE);wi::jobsystem::Wait(ser.ctx);
 for(const auto& m:mats)if(auto* v=s.materials.GetComponent(m.first)){*v=m.second;v->SetDirty();}
}
}

void UpdateParticleEffectSprites(wi::scene::Scene& s,float dt){
 HydrateParticleMovementFields(s);
 BeginParticleTimingFrame(s,dt);for(size_t i=0;i<s.emitters.GetCount();++i)ApplyParticleTimingLayer(s,s.emitters.GetEntity(i));
 for(size_t n=0;n<s.sprites.GetCount();++n){
  auto entity=s.sprites.GetEntity(n);auto* meta=s.metadatas.GetComponent(entity);
  if(!meta||!meta->string_values.has("renegade.particle.stationary.authored"))continue;
  auto& sp=s.sprites[n];auto* mat=s.materials.GetComponent(entity);if(!mat)continue;
  const auto slot=sp.params.isExtractNormalMapEnabled()?wi::scene::MaterialComponent::NORMALMAP:wi::scene::MaterialComponent::BASECOLORMAP;
  if(!sp.textureResource.IsValid())sp.textureResource=mat->textures[slot].resource;
  const bool disabled=meta->string_values.has("renegade.particle.disabled")||(meta->bool_values.has("renegade.particle.timing.hidden")&&meta->bool_values.get("renegade.particle.timing.hidden"));
  sp.SetHidden(disabled||!sp.textureResource.IsValid());
  if(disabled)continue;auto& anim=sp.anim.drawRectAnim;
  const int cols=std::max(1,anim.horizontalFrameCount),rows=std::max(1,meta->int_values.get("renegade.particle.sprite.rows"));
  const int first=std::clamp(meta->int_values.get("renegade.particle.sprite.start"),0,cols*rows-1),count=std::clamp(anim.frameCount,1,cols*rows-first);
  if(dt>0&&std::isfinite(dt)&&anim.frameRate>0&&(!meta->bool_values.has("renegade.particle.sprite.paused")||!meta->bool_values.get("renegade.particle.sprite.paused"))){
   anim._elapsedTime+=std::min(dt,1.0f)*anim.frameRate;
   int steps=int(anim._elapsedTime);anim._elapsedTime-=steps;
   anim._currentFrame=first+((std::max(0,anim._currentFrame-first)+steps)%count);
  }
  const int frame=std::clamp(anim._currentFrame,first,first+count-1);
  if(!sp.textureResource.IsValid())continue;const auto& tex=sp.textureResource.GetTexture().GetDesc();
  float w=float(tex.width)/cols,h=float(tex.height)/rows;
  if(h>0)sp.params.siz.x=sp.params.siz.y*(w/h);
  sp.params.enableDrawRect({float(frame%cols)*w,float(frame/cols)*h,w,h});
 }
}
wi::ecs::Entity ParticleEffectRoot(const wi::scene::Scene& s,wi::ecs::Entity e) noexcept {
 for(size_t i=0;e!=wi::ecs::INVALID_ENTITY&&i<=s.hierarchy.GetCount();++i){
  if(const auto* m=s.metadatas.GetComponent(e);m&&m->string_values.has(ParticleEffectMetadataKey))return e;
  const auto* h=s.hierarchy.GetComponent(e);if(!h)break;e=h->parentID;
 }return wi::ecs::INVALID_ENTITY;
}
ParticleEffect CaptureParticleEffect(const wi::scene::Scene& s,wi::ecs::Entity e) {
 ParticleEffect p;auto root=ParticleEffectRoot(s,e);if(root==wi::ecs::INVALID_ENTITY)root=e;
 if(const auto* n=s.names.GetComponent(root))p.name=n->name;
 if(const auto* m=s.metadatas.GetComponent(root);m&&m->string_values.has("renegade.particle.timing")){std::istringstream in(m->string_values.get("renegade.particle.timing"));ReadParticleTiming(in,p.timing);}
 if(const auto* m=s.metadatas.GetComponent(root);m&&m->string_values.has(ParticleEffectMetadataKey))p.id=m->string_values.get(ParticleEffectMetadataKey);
 if(const auto* m=s.metadatas.GetComponent(root);m&&m->string_values.has("renegade.particle.fields")){std::istringstream data(m->string_values.get("renegade.particle.fields"));ReadFields(data,p.fields);}
 for(size_t i=0;i<s.emitters.GetCount();++i){
  auto x=s.emitters.GetEntity(i);if(x!=root&&!s.Entity_IsDescendant(x,root))continue;
  ParticleEffectLayer l;l.id=PersistentEntityId(s,x);if(l.id.empty())l.id=GenerateStableId();
  if(const auto* n=s.names.GetComponent(x))l.name=n->name;
  l.emitter=CaptureParticleEmitter(s,x);l.shape.type=l.emitter.volume?ParticleShapeType::Box:ParticleShapeType::Point;
  if(const auto* m=s.metadatas.GetComponent(x);m&&m->string_values.has("renegade.particle.shape")){std::istringstream in(m->string_values.get("renegade.particle.shape"));ReadParticleShape(in,l.shape);}
  if(const auto* m=s.metadatas.GetComponent(x);m&&m->string_values.has("renegade.particle.lifetime")){std::istringstream data(m->string_values.get("renegade.particle.lifetime"));ReadParticleLifetime(data,l.lifetime);}
  if(const auto* m=s.metadatas.GetComponent(x);m&&m->string_values.has("renegade.particle.variation")){std::istringstream data(m->string_values.get("renegade.particle.variation"));data>>l.sizeVariation>>l.speedVariation;}
  if(const auto* m=s.metadatas.GetComponent(x);m&&m->string_values.has("renegade.particle.fieldWeights")){std::istringstream data(m->string_values.get("renegade.particle.fieldWeights"));for(auto& weight:l.fieldInfluence)data>>weight;}
  if(auto* m=s.materials.GetComponent(x))l.material=CaptureMaterial(*m);
  if(auto* m=s.metadatas.GetComponent(x);m&&m->string_values.has(MaterialBaseColorTextureAssetIdMetadataKey))l.texture=m->string_values.get(MaterialBaseColorTextureAssetIdMetadataKey);
  if(auto* m=s.metadatas.GetComponent(x))for(size_t slot=0;slot<4;++slot)if(m->string_values.has(MaterialTextureSlotMetadataKey(ExtraSlots[slot])))l.extraTextures[slot]=m->string_values.get(MaterialTextureSlotMetadataKey(ExtraSlots[slot]));
  if(auto* m=s.metadatas.GetComponent(x);m&&m->string_values.has("renegade.particle.stationary.authored")){
   l.playback=ParticlePlayback::StationaryLoop;std::istringstream data(m->string_values.get("renegade.particle.stationary.authored"));
   auto read=[&](auto& v){using T=std::decay_t<decltype(v)>;if constexpr(std::is_enum_v<T>){int x;data>>x;v=static_cast<T>(x);}else data>>v;};Visit(l.emitter,read);
  }
  if(const auto* t=s.transforms.GetComponent(x);t&&x!=root){l.transform.translation=t->translation_local;l.transform.rotation=t->rotation_local;l.transform.scale=t->scale_local;}
  if(auto* m=s.metadatas.GetComponent(x);m&&m->string_values.has("renegade.particle.disabled")){l.enabled=false;
   if(m->string_values.has("renegade.particle.disabled_authored")){
    std::istringstream original(m->string_values.get("renegade.particle.disabled_authored"));original>>l.emitter.emitCount>>l.emitter.burstOnCreate>>l.emitter.paused;
   }
  }
  if(const auto* m=s.metadatas.GetComponent(x);m&&m->string_values.has("renegade.particle.timing.authored")){std::istringstream in(m->string_values.get("renegade.particle.timing.authored"));bool enabled,paused;in>>l.emitter.emitCount>>l.emitter.burstOnCreate>>enabled>>paused;l.emitter.paused=paused;}
  p.layers.push_back(l);
 }return p;
}
bool ValidateParticleEffect(const ParticleEffect& p,std::string& error) {
 if(!ValidateParticleTiming(p.timing,error))return false;
 if(p.layers.empty()||p.layers.size()>32){error="An effect needs between 1 and 32 layers.";return false;}
 if(p.fields.size()>4){error="An effect supports up to four movement fields.";return false;}
 std::unordered_set<std::string> fieldIds;
 for(auto f:p.fields){
  bool finite=true;VisitField(f,[&](auto v){if constexpr(std::is_floating_point_v<decltype(v)>)finite&=std::isfinite(v);});
  const auto& q=f.transform.rotation;float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
  if(!finite||!IsValidStableId(f.id)||!fieldIds.insert(f.id).second||int(f.type)<0||int(f.type)>3||norm<.00001f||f.transform.scale.x<.01f||f.transform.scale.y<.01f||f.transform.scale.z<.01f||f.strength<0||f.strength>20||f.falloff<0||f.falloff>1||std::abs(f.lift)>20||f.noiseScale<.05f||f.noiseScale>20||f.animationSpeed<0||f.animationSpeed>10){error="Invalid movement field settings.";return false;}
 }
 std::unordered_set<std::string> ids;
 for(auto l:p.layers){
  if(!ValidateParticleLifetime(l.lifetime,error)||!ValidateParticleShape(l.shape,error))return false;
  if(int(l.shape.type)>1&&l.emitter.meshId!=wi::ecs::INVALID_ENTITY){error="Clear the mesh emission binding before choosing a geometric shape.";return false;}
  if(!std::isfinite(l.sizeVariation)||!std::isfinite(l.speedVariation)||l.sizeVariation<0||l.sizeVariation>1||l.speedVariation<0||l.speedVariation>1){error="Variation must be between 0 and 100%.";return false;}
  for(float weight:l.fieldInfluence)if(!std::isfinite(weight)||weight<0||weight>1){error="Layer field influence must be between 0 and 100%.";return false;}
  if(!IsValidStableId(l.id)||!ids.insert(l.id).second){error="Effect layers need unique identities.";return false;}
  if(!l.texture.empty()&&!IsValidStableId(l.texture)){error="Invalid layer texture identity.";return false;}
  for(auto& id:l.extraTextures)if(!id.empty()&&!IsValidStableId(id)){error="Invalid layer texture binding.";return false;}
  bool finite=true;auto check=[&](const auto& v){if constexpr(std::is_floating_point_v<std::decay_t<decltype(v)>>)finite&=std::isfinite(v);};
  Visit(l.emitter,check);Visit(l.material,check);VisitTransform(l.transform,check);
  const auto& q=l.transform.rotation;float qn=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
  if(!finite||qn<.00001f||l.transform.scale.x<=0||l.transform.scale.y<=0||l.transform.scale.z<=0){error="Invalid layer settings or transform.";return false;}
 }error.clear();return true;
}
bool SaveParticleEffect(const std::string& root,const StableId& project,ParticleEffect& p,bool asNew,std::string& error){
 if(!IsValidStableId(project)||!ValidateParticleEffect(p,error))return false;
 for(auto& l:p.layers)if(l.emitter.meshId!=wi::ecs::INVALID_ENTITY){error="Mesh emission needs a reusable mesh binding before saving this preset.";return false;}
 auto candidate=p;if(asNew||candidate.id.empty())candidate.id=GenerateStableId();
 if(!IsValidStableId(candidate.id)){error="Invalid effect preset identity.";return false;}
 auto path=std::filesystem::u8path(root)/"Content"/"Particles"/(candidate.id+".rparticle");
 if(!asNew&&!std::filesystem::exists(path)){error="Save a new preset before updating it.";return false;}
 candidate.name=CleanName(candidate.name);std::ostringstream out;out<<std::setprecision(9);
 out<<"renegade-particle-effect 7\n"<<project<<'\n'<<candidate.id<<'\n'<<std::quoted(candidate.name)<<'\n'<<candidate.layers.size()<<'\n';
 for(auto l:candidate.layers){
  out<<l.id<<' '<<std::quoted(CleanName(l.name))<<' '<<l.enabled<<' '<<std::quoted(l.texture)<<' ';for(auto& texture:l.extraTextures)out<<std::quoted(texture)<<' ';out<<int(l.playback)<<' ';for(float weight:l.fieldInfluence)out<<weight<<' ';out<<l.sizeVariation<<' '<<l.speedVariation<<'\n';
  auto write=[&](auto v){if constexpr(std::is_enum_v<decltype(v)>)out<<int(v);else out<<v;out<<' ';};
  WriteParticleLifetime(out,l.lifetime);WriteParticleShape(out,l.shape);Visit(l.emitter,write);Visit(l.material,write);VisitTransform(l.transform,write);out<<'\n';
 }
 WriteFields(out,candidate.fields);WriteParticleTiming(out,candidate.timing);
 ProjectDocumentWrite w;w.destinationPath=path.generic_u8string();auto str=out.str();w.content.assign(str.begin(),str.end());
 ProjectDocumentTransactionOptions options;options.allowedRoot=root;options.journalDirectory=(std::filesystem::u8path(root)/"Saved/Transactions").generic_u8string();
 auto result=ProjectDocumentTransaction().Execute({std::move(w)},options);if(!result.success){error=result.message;return false;}p=candidate;return true;
}
std::vector<ParticleEffect> ListParticleEffects(const std::string& root,const StableId& project,std::string& error){
 std::vector<ParticleEffect> out;std::error_code ec;auto dir=std::filesystem::u8path(root)/"Content/Particles";
 if(!std::filesystem::exists(dir,ec))return out;
 for(std::filesystem::directory_iterator it(dir,ec),end;!ec&&it!=end;it.increment(ec)){
  if(it->path().extension()!=".rparticle")continue;ParticleEffect p;if(Read(it->path(),project,p))out.push_back(std::move(p));else error="An effect preset could not be read.";
 }if(ec)error=ec.message();std::sort(out.begin(),out.end(),[](auto&a,auto&b){return a.name<b.name;});return out;
}
ParticleEffectLayer ScaleParticleLayer(const ParticleEffectLayer& base,float size,float amount,float height,float spread){
 auto l=base;auto finite=[](float x){return std::clamp(std::isfinite(x)?x:1.0f,.05f,10.0f);};size=finite(size);amount=std::clamp(std::isfinite(amount)?amount:1.0f,0.0f,10.0f);height=finite(height);spread=std::clamp(std::isfinite(spread)?spread:1.0f,0.0f,10.0f);
 l.shape.radius*=size;l.shape.height*=size;l.shape.thickness*=size;
 auto& e=l.emitter;e.size*=size;e.emitCount*=amount;e.maxParticles=std::min(1000000u,std::max(e.maxParticles,(unsigned)std::ceil(e.emitCount*e.life*2)));
 // Height preserves authored lifetime and overlap; scale rise and sprite elongation together.
 e.velocity.y*=size*height;e.emitCount*=height;e.velocity.x*=size*spread;e.velocity.z*=size*spread;e.randomFactor*=spread;e.gravity.x*=size;e.gravity.y*=size;e.gravity.z*=size;
 l.transform.translation.x*=size;l.transform.translation.y*=size;l.transform.translation.z*=size;
 return l;
}
void ApplyParticleEffectFieldMetadata(wi::scene::Scene& s,wi::ecs::Entity root,const std::vector<ParticleMovementField>& fields){
 auto* meta=s.metadatas.GetComponent(root);if(!meta)meta=&s.metadatas.Create(root);
 std::ostringstream data;data<<std::setprecision(9);WriteFields(data,fields);meta->string_values.set("renegade.particle.fields",data.str());
}
void ApplyParticleEffectLayerSettings(wi::scene::Scene& s,wi::ecs::Entity root,wi::ecs::Entity e,const ParticleEffectLayer& l,const std::vector<ParticleMovementField>& fields,bool resetSprite){
  (s.names.GetComponent(e)?s.names.GetComponent(e):&s.names.Create(e))->name=CleanName(l.name);
  auto& t=*s.transforms.GetComponent(e);
  if(e!=root){t.translation_local=l.transform.translation;t.rotation_local=l.transform.rotation;t.scale_local=l.transform.scale;t.SetDirty();t.UpdateTransform();}
  auto& meta=*(s.metadatas.GetComponent(e)?s.metadatas.GetComponent(e):&s.metadatas.Create(e));if(l.enabled)meta.string_values.erase("renegade.particle.disabled");else meta.string_values.set("renegade.particle.disabled","1");
  {
   std::ostringstream weights;weights<<std::setprecision(9);for(float weight:l.fieldInfluence)weights<<weight<<' ';meta.string_values.set("renegade.particle.fieldWeights",weights.str());
   std::ostringstream variation;variation<<std::setprecision(9)<<l.sizeVariation<<' '<<l.speedVariation;meta.string_values.set("renegade.particle.variation",variation.str());
   std::ostringstream runtime;runtime<<"v2 "<<std::setprecision(9)<<fields.size()<<' '<<(e==root)<<' '<<l.sizeVariation<<' '<<l.speedVariation<<' ';
   for(size_t field=0;field<fields.size();++field){auto f=fields[field];auto m=Matrix(f.transform);XMFLOAT4X4 matrix;XMStoreFloat4x4(&matrix,m);for(auto& row:matrix.m)for(float v:row)runtime<<v<<' ';
    runtime<<int(f.type)<<' '<<f.strength*(f.type==ParticleFieldType::Swirl&&f.clockwise?-1:1)<<' '<<(f.enabled&&l.enabled&&l.playback==ParticlePlayback::Continuous?l.fieldInfluence[field]:0)<<' '<<f.falloff<<' '<<f.lift<<' '<<f.noiseScale<<' '<<f.animationSpeed<<' ';
   }
   meta.string_values.set("renegade.particle.movement.runtime",runtime.str());
  }
  std::ostringstream timingBase;timingBase<<std::setprecision(9)<<l.emitter.emitCount<<' '<<l.emitter.burstOnCreate<<' '<<l.enabled<<' '<<l.emitter.paused;
  meta.bool_values.set("renegade.particle.timing.hidden",false);meta.string_values.set("renegade.particle.timing.authored",timingBase.str());
  auto state=l.emitter;
  if(l.playback==ParticlePlayback::StationaryLoop){
   std::ostringstream original;original<<std::setprecision(9);auto authoredEmitter=l.emitter;Visit(authoredEmitter,[&](auto v){if constexpr(std::is_enum_v<decltype(v)>)original<<int(v);else original<<v;original<<' ';});
   meta.string_values.set("renegade.particle.stationary.authored",original.str());
   meta.bool_values.set("renegade.particle.sprite.paused",l.emitter.paused);
   meta.int_values.set("renegade.particle.sprite.rows",int(l.emitter.framesY));meta.int_values.set("renegade.particle.sprite.start",int(l.emitter.frameStart));
   const bool initializeSprite=resetSprite||!s.sprites.Contains(e);
   auto& sp=*(s.sprites.GetComponent(e)?s.sprites.GetComponent(e):&s.sprites.Create(e));if(initializeSprite)sp=wi::Sprite();
   sp.SetCameraFacing(true);sp.SetCameraScaling(false);sp.SetDisableUpdate(true);sp.params.enableDepthTest();
   sp.params.pivot={.5f,.5f};sp.params.siz={2*l.emitter.size,2*l.emitter.size};
   sp.params.color=l.emitter.color;sp.params.intensity=1+l.emitter.emissiveStrength;sp.params.blendFlag=l.material.blendMode;
   sp.anim.repeatable=true;sp.anim.drawRectAnim.frameCount=int(l.emitter.frameCount);sp.anim.drawRectAnim.horizontalFrameCount=int(l.emitter.framesX);sp.anim.drawRectAnim.frameRate=l.emitter.frameRate;if(initializeSprite)sp.anim.drawRectAnim._currentFrame=int(l.emitter.frameStart);
   if(l.emitter.shaderType==wi::EmittedParticleSystem::SOFT_DISTORTION)sp.params.enableExtractNormalMap();
   sp.SetHidden(!l.enabled);
   state.emitCount=0;state.burstOnCreate=0;state.paused=true;
  }else {s.sprites.Remove(e);meta.string_values.erase("renegade.particle.stationary.authored");}
  if(!l.enabled){
   std::ostringstream authored;authored<<std::setprecision(9)<<l.emitter.emitCount<<' '<<l.emitter.burstOnCreate<<' '<<l.emitter.paused;
   meta.string_values.set("renegade.particle.disabled_authored",authored.str());
   state.emitCount=0;state.burstOnCreate=0;state.paused=false;
  }else meta.string_values.erase("renegade.particle.disabled_authored");
  ApplyMaterial(*s.materials.GetComponent(e),l.material);ApplyParticleEmitter(s,e,state);
  std::ostringstream curveData;curveData<<std::setprecision(9);WriteParticleLifetime(curveData,l.lifetime);
  meta.string_values.set("renegade.particle.lifetime",curveData.str());
  std::ostringstream shapeData;shapeData<<std::setprecision(9);WriteParticleShape(shapeData,l.shape);meta.string_values.set("renegade.particle.shape",shapeData.str());
  HydrateParticleShape(s,e);HydrateParticleLifetime(s,e);
}
ParticleEffectCommand::ParticleEffectCommand(wi::scene::Scene& s,std::string root,StableId project,ParticleEffect p,wi::ecs::Entity target,XMFLOAT3 pos):
 scene_(&s),root_(std::move(root)),project_(std::move(project)),effect_(std::move(p)),entity_(target),position_(pos),existing_(target!=wi::ecs::INVALID_ENTITY){}
bool ParticleEffectCommand::Execute(){
 auto& s=*scene_;if(captured_){ResetParticleTiming(s,entity_);if(existing_)s.Entity_Remove(entity_);Restore(s,after_,afterMaterials_);UpdateParticleEffectSprites(s,0);return true;}
 if(!ValidateParticleEffect(effect_,error_))return false;
 if((!effect_.fields.empty()||std::any_of(effect_.layers.begin(),effect_.layers.end(),[](auto& l){return l.sizeVariation>0||l.speedVariation>0||l.lifetime.enabled||int(l.shape.type)>1;}))&&wi::graphics::GetDevice()&&!EnsureParticleMovementShaders(error_))return false;
 std::vector<PreparedMaterialTextureAsset> textures(effect_.layers.size());
 for(size_t i=0;i<effect_.layers.size();++i)if(!effect_.layers[i].texture.empty()&&!PrepareMaterialTextureAsset(root_,project_,effect_.layers[i].texture,textures[i],error_))return false;
 std::vector<std::array<PreparedMaterialTextureAsset,4>> extra(effect_.layers.size());
 for(size_t i=0;i<effect_.layers.size();++i)for(size_t slot=0;slot<4;++slot)if(!effect_.layers[i].extraTextures[slot].empty()&&!PrepareMaterialTextureAsset(root_,project_,effect_.layers[i].extraTextures[slot],extra[i][slot],error_))return false;
 if(existing_){if(!s.transforms.Contains(entity_)){error_="Effect no longer exists.";return false;}Snapshot(s,entity_,before_,beforeMaterials_);}
 else {entity_=wi::ecs::CreateEntity();s.transforms.Create(entity_).Translate(position_);std::string err;if(!AssignNewPersistentEntityId(s,entity_,err)){error_=err;s.Entity_Remove(entity_);return false;}}
 const auto rollback=[&](){s.Entity_Remove(entity_);if(existing_)Restore(s,before_,beforeMaterials_);};
 SetParticleTimingMetadata(s,entity_,effect_.timing);ResetParticleTiming(s,entity_);
 auto& rootmeta=*(s.metadatas.GetComponent(entity_)?s.metadatas.GetComponent(entity_):&s.metadatas.Create(entity_));rootmeta.string_values.set(ParticleEffectMetadataKey,effect_.id);
 {std::ostringstream data;data<<std::setprecision(9);WriteFields(data,effect_.fields);rootmeta.string_values.set("renegade.particle.fields",data.str());}
 std::string effectName=CleanName(effect_.name);
 if(!existing_){unsigned suffix=2;bool found;do{found=false;for(size_t n=0;n<s.names.GetCount();++n)if(s.names.GetEntity(n)!=entity_&&s.names[n].name==effectName){found=true;break;}if(found)effectName=CleanName(effect_.name)+" "+std::to_string(suffix++);}while(found);}
 (s.names.GetComponent(entity_)?s.names.GetComponent(entity_):&s.names.Create(entity_))->name=effectName;
 std::vector<wi::ecs::Entity> prior;
 for(size_t i=0;i<s.emitters.GetCount();++i){auto e=s.emitters.GetEntity(i);if(e==entity_||s.Entity_IsDescendant(e,entity_))prior.push_back(e);}
 std::vector<wi::ecs::Entity> kept;
 for(size_t i=0;i<effect_.layers.size();++i){
  auto l=effect_.layers[i];wi::ecs::Entity e=wi::ecs::INVALID_ENTITY;
  for(auto old:prior)if(PersistentEntityId(s,old)==l.id){e=old;break;}
  if(e==wi::ecs::INVALID_ENTITY){
   e=s.Entity_CreateEmitter(CleanName(l.name));std::string err;if(!AssignNewPersistentEntityId(s,e,err)){error_=err;rollback();return false;}
   s.Component_Attach(e,entity_,true);
  }
  kept.push_back(e);ApplyParticleEffectLayerSettings(s,entity_,e,l,effect_.fields,true);
  if(!l.texture.empty()&&!ApplyPreparedMaterialTextureAsset(s,e,MaterialTextureSlot::BaseColor,textures[i],{},error_)){rollback();return false;}
  for(size_t slot=0;slot<4;++slot)if(!l.extraTextures[slot].empty()&&!ApplyPreparedMaterialTextureAsset(s,e,ExtraSlots[slot],extra[i][slot],{},error_)){rollback();return false;}

 }
 for(auto e:prior)if(std::find(kept.begin(),kept.end(),e)==kept.end()){
  if(e==entity_){s.emitters.Remove(e);s.materials.Remove(e);}else s.Entity_Remove(e);
 }
 UpdateParticleEffectSprites(s,0);Snapshot(s,entity_,after_,afterMaterials_);captured_=true;return true;
}
void ParticleEffectCommand::Undo(){if(!captured_)return;ResetParticleTiming(*scene_,entity_);scene_->Entity_Remove(entity_);if(existing_){Restore(*scene_,before_,beforeMaterials_);UpdateParticleEffectSprites(*scene_,0);}}
}
