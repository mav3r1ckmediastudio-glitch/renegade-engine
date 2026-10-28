#include "RiverWaterwayRenderer.h"
#include "RiverShaderAssets.h"
#include "RiverFoamAppearanceGpu.h"
#include "RiverSurfaceMotion.h"
#include "RiverWaterwaySurfaceBinding.h"
#include "RiverWaterwayTransport.h"
#include "RiverRapidsTransport.h"
#include "RiverFlowSourceSelection.h"
#include "renegade/bridge/RiverWaterwayService.h"
#include "json.hpp"
#include <chrono>
#include <cstring>
#include <optional>
#include <unordered_map>
#include <unordered_set>
using namespace DirectX;
namespace renegade::bridge {
namespace {
using namespace wi::graphics;
void Require(bool ok,const std::string& error){if(!ok)throw std::runtime_error(error);}
uint32_t Bits(float f){uint32_t b;std::memcpy(&b,&f,4);return b;}
constexpr uint32_t Tag=0x52464c35;
struct Params{XMFLOAT4 bounds;XMUINT4 control;XMFLOAT4 settings;XMUINT4 foamControl;XMFLOAT4 foamSettings;XMUINT4 motion,detail,artwork;};
struct Push{XMUINT4 resolution;XMFLOAT4 bounds,settings;};
static_assert(sizeof(Params)==128&&sizeof(Push)==48);
struct Hash {
 uint64_t value=1469598103934665603ull;
 void Add(const void* p,size_t n){auto* b=static_cast<const unsigned char*>(p);for(size_t k=0;k<n;++k){value^=b[k];value*=1099511628211ull;}}
 template<class T>void Add(const T& v){Add(&v,sizeof(v));}
 void Text(const std::string& text){Add(text.size());Add(text.data(),text.size());}
};
struct MeshRevision{uint64_t epoch=~0ull,hash=0;size_t vertices=0,indices=0;};
using MeshRevisions=std::unordered_map<wi::ecs::Entity,MeshRevision>;
void Upload(Texture& texture,const std::vector<XMFLOAT4>& data,unsigned width,unsigned height,bool writable=false){
 TextureDesc desc;desc.width=width;desc.height=height;desc.format=Format::R32G32B32A32_FLOAT;desc.layout=ResourceState::SHADER_RESOURCE;desc.bind_flags=BindFlag::SHADER_RESOURCE;if(writable)desc.bind_flags|=BindFlag::UNORDERED_ACCESS;
 SubresourceData initial;initial.data_ptr=data.data();initial.row_pitch=width*sizeof(XMFLOAT4);initial.slice_pitch=initial.row_pitch*height;
 Require(GetDevice()->CreateTexture(&desc,&initial,&texture),"Connected river texture allocation failed");
}
uint32_t SRV(const GPUResource& resource){int index=GetDevice()->GetDescriptorIndex(&resource,SubresourceType::SRV);Require(index>=0,"Connected river resource descriptor missing");return uint32_t(index);}
#include "RiverFlowGather.inc"
uint64_t Fingerprint(const wi::scene::Scene& scene,uint64_t editRevision,const river_flow_detail::SourceBounds* bounds,MeshRevisions& caches,bool& hasConnections,bool includeSubmerged){
 Hash hash;hasConnections=false;size_t carriers=0;
 for(size_t k=0;k<scene.splines.GetCount();++k){RiverFlowProfile p;std::string error;if(ReadRiverFlowProfile(scene,scene.splines.GetEntity(k),p,error)&&p.rapids)includeSubmerged=true;}
 for(size_t k=0;k<scene.metadatas.GetCount();++k){const auto& m=scene.metadatas[k];if(!m.string_values.has(RiverConnectionMetadataKey))continue;
  const auto& text=m.string_values.get(RiverConnectionMetadataKey);Require(++carriers<=15&&text.size()<=4096,"Connected river metadata limit exceeded");
  hasConnections=true;hash.Add(scene.metadatas.GetEntity(k));hash.Text(text);
 }
 if(!hasConnections)return hash.value;
 size_t rivers=0;
 for(size_t k=0;k<scene.splines.GetCount();++k){const auto e=scene.splines.GetEntity(k);const auto* m=scene.materials.GetComponent(e);if(!m||m->shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER)continue;
  Require(++rivers<=128,"Connected river source budget exceeded");const auto& s=scene.splines[k];Require(s.spline_node_entities.size()<=512,"Connected river control budget exceeded");
  hash.Add(e);hash.Text(PersistentEntityId(scene,e));hash.Add(s.width);hash.Add(s.rotation);hash.Add(s.mesh_generation_subdivision);hash.Add(s.mesh_generation_vertical_subdivision);
  const bool flags[]={s.IsLooped(),s.IsFilled(),s.IsDrawAligned()};hash.Add(flags);// Native material Z/W offsets animate every frame; only UV scale
  // changes geometry. Animation must retain transport resources and history.
  hash.Add(m->texMulAdd.x);hash.Add(m->texMulAdd.y);
  XMFLOAT4X4 world;XMStoreFloat4x4(&world,scene.ComputeEntityMatrixRecursive(e));hash.Add(world);
  if(const auto* object=scene.objects.GetComponent(e))hash.Add(object->_flags);
  for(auto node:s.spline_node_entities){hash.Add(node);if(const auto* t=scene.transforms.GetComponent(node)){hash.Add(t->translation_local);hash.Add(t->rotation_local);hash.Add(t->scale_local);}}
  RiverFlowProfile p;std::string error;Require(ReadRiverFlowProfile(scene,e,p,error),error);hash.Add(p.enabled);hash.Add(p.speed);hash.Add(p.reversed);hash.Add(p.rapids);
 }
 // The first build derives bounds from the validated native graph before hashing any source vertices.
 if(!bounds)return hash.value;
 for(size_t k=0;k<scene.objects.GetCount();++k){const auto& object=scene.objects[k];auto* mesh=scene.meshes.GetComponent(object.meshID);const auto e=scene.objects.GetEntity(k);
  if(!mesh||!river_flow_detail::IsPotentialSource(scene,e,object,*mesh,bounds,includeSubmerged))continue;
  hash.Add(e);hash.Add(object.meshID);hash.Add(object.IsRenderable());XMFLOAT4X4 world;XMStoreFloat4x4(&world,scene.ComputeEntityMatrixRecursive(e));hash.Add(world);
  auto& cached=caches[object.meshID];if(cached.epoch!=editRevision||cached.vertices!=mesh->vertex_positions.size()||cached.indices!=mesh->indices.size()){
   Require(mesh->vertex_positions.size()<=2000000&&mesh->indices.size()<=6000000,"Connected river static source geometry limit exceeded");Hash geometry;geometry.Add(mesh->vertex_positions.data(),mesh->vertex_positions.size()*sizeof(XMFLOAT3));geometry.Add(mesh->indices.data(),mesh->indices.size()*sizeof(uint32_t));cached={editRevision,geometry.value,mesh->vertex_positions.size(),mesh->indices.size()};}
  hash.Add(cached.hash);
 }
 return hash.value;
}
}
struct RiverWaterwayRenderer::Impl {
 wi::scene::Scene* scene=nullptr;uint64_t revision=~0ull,fingerprint=0;bool dirty=true,suspended=false,ready=false,flow=false,hasBounds=false,terrainDeferred=false;
 std::unordered_set<wi::ecs::Entity> members;wi::ecs::Entity root=wi::ecs::INVALID_ENTITY;std::string status="Native waterway";
 RiverWaterwaySurfaceBinding geometry;std::optional<RiverWaterwaySurfaceBinding::Scope> scope;MeshRevisions meshes;
 river_flow_detail::SourceBounds sourceBounds;RiverFlowProfile profile;RiverFlowDiagnostics diagnostics;
 std::string shaderDirectory=river_shader_assets::Directory();Shader pixel[2],simulation[2];int shaderID[2]={-1,-1};Texture foam[2],normals,flux,source,velocity;GPUBuffer parameters;XMFLOAT4 bounds{};
 RiverFoamAppearanceGpu appearance;Shader surfaceVS,surfaceShadowVS;std::vector<XMFLOAT4> motionTexels;
 std::vector<wi::renderer::RenderableLine> arrows;
 uint64_t transportSignature=0;
 Params params{};
 unsigned state=0,start=0,steps=0;float clock=0,step=0,maximumRate=0;mutable bool dispatched=true;bool shaderBound=false;int originalShader=-1;XMUINT4 originalUserdata{};
 void Clear(){Require(!scope,"Connected renderer bindings must be scoped");if(ready||foam[0].IsValid())GetDevice()->WaitForGPU();geometry.Reset();arrows.clear();ready=flow=false;root=wi::ecs::INVALID_ENTITY;foam[0]={};foam[1]={};normals={};flux={};source={};velocity={};parameters={};state=start=steps=0;clock=0;hasBounds=false;}
 void LoadShaders(){
  const unsigned mode=profile.rapids?1:0;if(shaderID[mode]>=0)return;wi::vector<uint8_t> bytes;auto* gpu=GetDevice();
  Require(wi::helper::FileRead(shaderDirectory+"/RiverRefinedFoamPS.cso",bytes),"Missing connected WATER pixel shader");Require(gpu->CreateShader(ShaderStage::PS,bytes.data(),bytes.size(),&pixel[mode]),"Connected WATER shader creation failed");
  Require(wi::helper::FileRead(shaderDirectory+(mode?"/RiverRapidsCS.cso":"/RiverWaterwayFoamCS.cso"),bytes),"Missing connected WATER transport shader");Require(gpu->CreateShader(ShaderStage::CS,bytes.data(),bytes.size(),&simulation[mode]),"Connected WATER transport shader creation failed");
  if(mode){
   Require(wi::helper::FileRead(shaderDirectory+"/RiverSurfaceVS.cso",bytes),"Missing river surface VS");Require(gpu->CreateShader(ShaderStage::VS,bytes.data(),bytes.size(),&surfaceVS),"River surface VS");
   Require(wi::helper::FileRead(shaderDirectory+"/RiverSurfaceShadowVS.cso",bytes),"Missing river surface shadow VS");Require(gpu->CreateShader(ShaderStage::VS,bytes.data(),bytes.size(),&surfaceShadowVS),"River surface shadow VS");
  }
  wi::renderer::CustomShader custom;custom.name="Renegade River V2 connected WATER";custom.filterMask=wi::enums::FILTER_TRANSPARENT|wi::enums::FILTER_WATER;
  PipelineStateDesc pso;pso.vs=mode?&surfaceVS:wi::renderer::GetShader(wi::enums::VSTYPE_OBJECT_COMMON);pso.ps=&pixel[mode];pso.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENT);pso.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_TRANSPARENT);pso.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_DOUBLESIDED);
  Require(gpu->CreatePipelineState(&pso,&custom.pso[wi::enums::RENDERPASS_MAIN]),"Connected WATER pipeline creation failed");
  pso.vs=mode?&surfaceShadowVS:wi::renderer::GetShader(wi::enums::VSTYPE_SHADOW_TRANSPARENT);pso.ps=wi::renderer::GetShader(wi::enums::PSTYPE_SHADOW_TRANSPARENT);pso.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENTSHADOW);pso.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_DEPTHREAD);pso.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_SHADOW_DOUBLESIDED);
  Require(gpu->CreatePipelineState(&pso,&custom.pso[wi::enums::RENDERPASS_SHADOW]),"Connected WATER shadow pipeline creation failed");shaderID[mode]=wi::renderer::RegisterCustomShader(custom);
 }
 void DiscoverMembers(){
  EntityIdentityIndex index;std::string error;Require(index.Build(*scene,error),error);std::unordered_set<wi::ecs::Entity> found;
  for(size_t k=0;k<scene->metadatas.GetCount();++k){const auto& m=scene->metadatas[k];if(!m.string_values.has(RiverConnectionMetadataKey))continue;found.insert(scene->metadatas.GetEntity(k));
   try{auto j=nlohmann::json::parse(m.string_values.get(RiverConnectionMetadataKey));auto e=index.Resolve(j.at("receiver").get<std::string>());if(e!=wi::ecs::INVALID_ENTITY)found.insert(e);}catch(...){}
  }
  // Retain prior known members on invalid topology so fallback cannot accidentally
  // run separate single-river effects through a previously joined confluence.
  members.insert(found.begin(),found.end());
 }
 struct Payload{
  river_v2_classification::Mask mask;
  std::vector<XMFLOAT4> faces,velocity,source;std::vector<int> index,active;
  float maximumRate=0,maximumSpeed=0;
 };
 Payload BuildDomain(const RiverFlowNetwork& network,const RiverJunctionSurface& surface,const RiverFlowProfile& p){
  const auto obstacles=GatherBounds(*scene,root,sourceBounds,nullptr,p.rapids);
  auto copy=[&](auto&& d){Payload out;out.mask=std::move(d.mask);out.faces=std::move(d.faces);out.velocity=std::move(d.velocity);out.source=std::move(d.source);out.index=std::move(d.index);out.active=std::move(d.active);out.maximumRate=d.maximumRate;out.maximumSpeed=d.maximumSpeed;return out;};
  if(p.rapids){auto d=river_rapids_transport::Build(network,surface,obstacles,640,320,p.sourceWidth);motionTexels=river_surface_motion::Build(network,d,network.Junctions().front().outgoing).texels;for(auto& q:d.source)q.w*=p.rapidIntensity;return copy(std::move(d));}
  return copy(river_waterway_transport::Build(network,surface,obstacles,640,320,p.sourceWidth));
 }
 void Rebuild(){
  auto began=std::chrono::steady_clock::now();ready=flow=false;arrows.clear();DiscoverMembers();
  RiverWaterway graph;std::string error;Require(ReadRiverWaterway(*scene,graph,error)&&graph.network,error);
  Require(graph.network->Junctions().size()==1&&graph.network->Reaches().size()==3,"Connected rendering supports one two-inlet flat confluence");
  std::shared_ptr<const RiverJunctionSurface> surface;Require(BuildRiverJunctionSurface(*graph.network,surface,error),error);
  auto outlet=graph.network->Junctions().front().outgoing;auto reach=std::find_if(graph.reaches.begin(),graph.reaches.end(),[&](const auto& r){return r.id==outlet;});Require(reach!=graph.reaches.end(),"Connected outlet native root missing");root=reach->river;
  std::set<wi::ecs::Entity> unique;for(auto r:graph.reaches)unique.insert(r.river);members=std::unordered_set<wi::ecs::Entity>(unique.begin(),unique.end());
  Require(ReadRiverFlowProfile(*scene,root,profile,error),error);const auto* material=scene->materials.GetComponent(root);Require(material&&material->customShaderID<0,"Connected flow cannot replace an authored custom shader");
  std::shared_ptr<const RiverFlowField> full;Require(BuildRiverFlowField(*scene,root,{},full,error),error);const float tile=full->Samples().front().widthMeters/material->texMulAdd.x;
  sourceBounds={};for(const auto& r:graph.network->Reaches()){auto b=river_flow_detail::CaptureSourceBounds(*r.field);sourceBounds.minimum={std::min(sourceBounds.minimum.x,b.minimum.x),std::min(sourceBounds.minimum.y,b.minimum.y),std::min(sourceBounds.minimum.z,b.minimum.z)};sourceBounds.maximum={std::max(sourceBounds.maximum.x,b.maximum.x),std::max(sourceBounds.maximum.y,b.maximum.y),std::max(sourceBounds.maximum.z,b.maximum.z)};}hasBounds=true;
  GetDevice()->WaitForGPU();scene->Update(0); // Rebuild-only native cache settlement, never a per-frame geometry scan.
  Require(geometry.Configure(*scene,root,std::vector<wi::ecs::Entity>(unique.begin(),unique.end()),*surface,tile,error,profile.enabled&&profile.rapids),error);
  if(profile.enabled){
   LoadShaders();auto domain=BuildDomain(*graph.network,*surface,profile);
   {Hash h;h.Add(domain.index.data(),domain.index.size()*sizeof(int));h.Add(domain.faces.data(),domain.faces.size()*sizeof(XMFLOAT4));transportSignature=h.value;}
   // Cache a bounded, world-spaced overlay from the actual projected fluid
   // velocities. Never rebuild/read back transport just to show arrows.
   const auto& mask=domain.mask;
   const unsigned sx=std::max(1u,unsigned(std::ceil(2.f*mask.width/mask.bounds.z)));
   const unsigned sz=std::max(1u,unsigned(std::ceil(2.f*mask.height/mask.bounds.w)));
   for(unsigned z=sz/2;z<mask.height&&arrows.size()<1152;z+=sz)
    for(unsigned x=sx/2;x<mask.width&&arrows.size()<1152;x+=sx){
     const size_t k=size_t(z)*mask.width+x;if(domain.index[k]<0)continue;
     const auto v=domain.velocity[k];const float speed=std::hypot(v.x,v.y);
     if(speed<1e-5f)continue;const float ux=v.x/speed,uz=v.y/speed;
     const XMFLOAT3 a{mask.bounds.x+(x+.5f)*mask.bounds.z/mask.width,
       full->Samples().front().worldPosition.y+.12f,
       mask.bounds.y+(z+.5f)*mask.bounds.w/mask.height};
     const XMFLOAT3 tip{a.x+ux*.7f,a.y,a.z+uz*.7f};
     wi::renderer::RenderableLine line;line.color_start=line.color_end={.05f,1.f,.8f,1};
     line.start=a;line.end=tip;arrows.push_back(line);
     line.start=tip;line.end={tip.x-ux*.22f-uz*.15f,tip.y,tip.z-uz*.22f+ux*.15f};arrows.push_back(line);
     line.end={tip.x-ux*.22f+uz*.15f,tip.y,tip.z-uz*.22f-ux*.15f};arrows.push_back(line);
    }
   std::vector<XMFLOAT4> base(domain.faces.size()),zero(base.size());
   for(size_t k=0;k<base.size();++k){auto q=surface->Evaluate({domain.mask.bounds.x+(k%domain.mask.width+.5f)/domain.mask.width*domain.mask.bounds.z,domain.mask.bounds.y+(k/domain.mask.width+.5f)/domain.mask.height*domain.mask.bounds.w});if(q.valid){const float speed=std::hypot(q.velocityXZ.x,q.velocityXZ.y);if(speed>1e-7f)base[k]={q.velocityXZ.x/speed,q.velocityXZ.y/speed,speed,1};}}
   for(int k:domain.active)base[k]=domain.velocity[k];
   Upload(normals,base,domain.mask.width,domain.mask.height);Upload(flux,domain.faces,domain.mask.width,domain.mask.height);Upload(source,domain.source,domain.mask.width,domain.mask.height);Upload(velocity,domain.velocity,domain.mask.width,domain.mask.height);
   Upload(foam[0],zero,domain.mask.width,domain.mask.height,true);Upload(foam[1],zero,domain.mask.width,domain.mask.height,true);state=start=steps=0;clock=0;bounds=domain.mask.bounds;maximumRate=domain.maximumRate;
   appearance.Configure(shaderDirectory,domain.mask.width,domain.mask.height);
   if(profile.rapids)Upload(appearance.waves,motionTexels,domain.mask.width,domain.mask.height);
   params=Params{bounds,{SRV(normals),Bits(-1),Bits(1),0},{tile,std::min(4.f,tile/std::max(domain.maximumSpeed,.001f)),0,1},{SRV(foam[0]),SRV(foam[1]),SRV(source),SRV(velocity)},{profile.foam?profile.intensity:0,profile.threshold,0,0},{profile.rapids?SRV(appearance.waves):0,profile.rapids?1u:0u,0,0},appearance.DetailControls(),appearance.ArtworkControls()};
   GPUBufferDesc desc;desc.size=sizeof(params);desc.bind_flags=BindFlag::SHADER_RESOURCE;desc.misc_flags=ResourceMiscFlag::BUFFER_RAW;Require(GetDevice()->CreateBuffer(&desc,&params,&parameters),"Connected WATER parameters allocation failed");
   flow=true;diagnostics.cells=base.size();
  }
  ++diagnostics.rebuilds;diagnostics.lastBuildMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();ready=true;
  status=flow?(profile.rapids?"Connected WATER | measured depth + rapids + contact foam":"Connected WATER | shared flow + contact foam"):"Connected native WATER | flow disabled";
 }
};
RiverWaterwayRenderer::RiverWaterwayRenderer(std::string shaderDirectory):impl_(std::make_unique<Impl>()){
 impl_->shaderDirectory=shaderDirectory=="Content/river"?
    river_shader_assets::Directory():std::move(shaderDirectory);
}
RiverWaterwayRenderer::~RiverWaterwayRenderer(){Restore();}
void RiverWaterwayRenderer::Invalidate(){impl_->dirty=true;impl_->meshes.clear();}
void RiverWaterwayRenderer::SetSuspended(bool value){impl_->suspended=value;}
bool RiverWaterwayRenderer::Owns(wi::ecs::Entity e)const{return impl_->members.count(e)!=0;}
bool RiverWaterwayRenderer::Ready()const{return impl_->ready;}
const std::string& RiverWaterwayRenderer::Status()const{return impl_->status;}
void RiverWaterwayRenderer::Prepare(wi::scene::Scene& scene,uint64_t revision,float dt,uint64_t editRevision){
 auto& s=*impl_;Require(!s.scope,"Connected renderer preparation inside a binding scope");if(s.suspended)return;
 for(size_t k=0;k<scene.terrains.GetCount();++k)if(scene.terrains[k].IsGenerationBusy()){s.terrainDeferred=true;return;}
 if(s.terrainDeferred){s.terrainDeferred=false;s.dirty=true;s.meshes.clear();}
 if(s.scene!=&scene||s.revision!=revision){s.Clear();s.members.clear();s.meshes.clear();s.scene=&scene;s.revision=revision;s.dirty=true;}
 try{
  bool connected=false;auto hash=Fingerprint(scene,editRevision,s.hasBounds?&s.sourceBounds:nullptr,s.meshes,connected,false);
  if(!connected){if(!s.members.empty()||s.ready)s.Clear();s.members.clear();s.dirty=false;s.status="Native waterway";return;}
  if(s.dirty||s.fingerprint!=hash){s.Rebuild();s.fingerprint=Fingerprint(scene,editRevision,s.hasBounds?&s.sourceBounds:nullptr,s.meshes,connected,false);s.dirty=false;}
  if(s.ready&&s.flow){
   RiverFlowProfile latest;std::string error;Require(ReadRiverFlowProfile(scene,s.root,latest,error),error);
   bool sourceChanged=false;
   if(latest.sourceWidth!=s.profile.sourceWidth||latest.rapidIntensity!=s.profile.rapidIntensity){
    RiverWaterway graph;Require(ReadRiverWaterway(scene,graph,error)&&graph.network,error);
    std::shared_ptr<const RiverJunctionSurface> surface;Require(BuildRiverJunctionSurface(*graph.network,surface,error),error);
    auto domain=s.BuildDomain(*graph.network,*surface,latest);
    Hash h;h.Add(domain.index.data(),domain.index.size()*sizeof(int));h.Add(domain.faces.data(),domain.faces.size()*sizeof(XMFLOAT4));
    Require(h.value==s.transportSignature,"Contact width unexpectedly changed fluid topology or flux");
    GetDevice()->WaitForGPU();Texture next;Upload(next,domain.source,domain.mask.width,domain.mask.height);s.source=std::move(next);s.params.foamControl.z=SRV(s.source);sourceChanged=true;
   }
   const float amount=latest.foam?latest.intensity:0;
   if(sourceChanged||s.params.foamSettings.x!=amount||s.params.foamSettings.y!=latest.threshold){
    // Committed UI edits only: retire the tiny parameter buffer safely, retaining
    // both density textures, transport/domain resources, state index and clock.
    GetDevice()->WaitForGPU();s.params.foamSettings.x=amount;s.params.foamSettings.y=latest.threshold;
    GPUBuffer next;GPUBufferDesc desc;desc.size=sizeof(Params);desc.bind_flags=BindFlag::SHADER_RESOURCE;desc.misc_flags=ResourceMiscFlag::BUFFER_RAW;
    Require(GetDevice()->CreateBuffer(&desc,&s.params,&next),"Connected live parameters allocation failed");s.parameters=std::move(next);
   }
   s.profile=latest;
  }
  s.steps=0;s.dispatched=false;if(s.ready&&s.flow){float elapsed=std::isfinite(dt)?std::clamp(dt,0.f,1.f/30):0;
   s.steps=elapsed>0?unsigned(std::max(1.f,std::ceil(elapsed*s.maximumRate/.75f))):0;Require(s.steps<=64,"Connected flow substep budget exceeded");s.step=s.steps?elapsed/s.steps:0;s.start=s.state;s.clock+=elapsed;s.state^=s.steps&1;s.appearance.Prepare(elapsed);}
 }catch(const std::exception& ex){s.ready=s.flow=false;s.steps=0;s.status=ex.what();s.dirty=false;
  try{bool connected=false;s.fingerprint=Fingerprint(scene,editRevision,s.hasBounds?&s.sourceBounds:nullptr,s.meshes,connected,false);}catch(...){s.fingerprint=0;}
 }
}
void RiverWaterwayRenderer::DrawArrows()const{
 const auto& s=*impl_;if(!s.ready||!s.flow||s.terrainDeferred)return;
 for(const auto& line:s.arrows)wi::renderer::DrawLine(line,false);
}
void RiverWaterwayRenderer::Apply()const{
 auto& s=*impl_;if(!s.ready||s.suspended||s.terrainDeferred||s.scope)return;
 s.scope.emplace(s.geometry);if(!s.scope->Applied()){s.status=s.geometry.Error();s.scope.reset();return;}
 if(s.flow){auto* material=s.scene->materials.GetComponent(s.root);s.originalShader=material->customShaderID;s.originalUserdata=material->userdata;material->SetCustomShaderID(s.shaderID[s.profile.rapids?1:0]);material->userdata={SRV(s.parameters),Tag,s.state|(s.appearance.State()<<1),Bits(s.clock)};material->SetDirty();s.shaderBound=true;}
}
void RiverWaterwayRenderer::Restore()const{
 auto& s=*impl_;if(s.shaderBound){if(auto* material=s.scene->materials.GetComponent(s.root)){material->SetCustomShaderID(s.originalShader);material->userdata=s.originalUserdata;material->SetDirty();}s.shaderBound=false;}s.scope.reset();
}
void RiverWaterwayRenderer::Dispatch()const{
 auto& s=*impl_;if(!s.ready||!s.flow||s.suspended||s.terrainDeferred||s.dispatched)return;s.dispatched=true;if(!s.steps)return;auto* gpu=GetDevice();auto cmd=gpu->BeginCommandList();wi::renderer::BindCommonResources(cmd);unsigned current=s.start;
 for(unsigned n=0;n<s.steps;++n){unsigned next=1-current;auto barrier=GPUBarrier::Image(&s.foam[next],ResourceState::SHADER_RESOURCE,ResourceState::UNORDERED_ACCESS);gpu->Barrier(&barrier,1,cmd);gpu->BindComputeShader(&s.simulation[s.profile.rapids?1:0],cmd);
  gpu->BindResource(&s.foam[current],0,cmd);gpu->BindResource(&s.flux,1,cmd);gpu->BindResource(&s.source,2,cmd);gpu->BindResource(&s.velocity,3,cmd);gpu->BindUAV(&s.foam[next],0,cmd);
  const auto& desc=s.foam[next].GetDesc();Push push{{desc.width,desc.height,s.profile.rapids?15u:7u,0},s.bounds,{s.step,s.clock-(s.steps-1-n)*s.step,s.profile.lifetime,1}};gpu->PushConstants(&push,sizeof(push),cmd);gpu->Dispatch((desc.width+7)/8,(desc.height+7)/8,1,cmd);barrier=GPUBarrier::Image(&s.foam[next],ResourceState::UNORDERED_ACCESS,ResourceState::SHADER_RESOURCE);gpu->Barrier(&barrier,1,cmd);current=next;
 }
 s.appearance.Dispatch(s.source,s.velocity,s.foam[s.state],s.bounds,s.clock,s.profile.rapids?15u:7u);
}
bool RiverWaterwayRenderer::Diagnostics(RiverFlowDiagnostics& out,std::string& error,bool readback)const{
 const auto& s=*impl_;error.clear();if(!s.ready){error=s.status;return false;}out=s.diagnostics;
 if(readback&&s.flow){s.appearance.Readback(out.maximumDetail,out.detailSignature);GetDevice()->WaitForGPU();wi::vector<uint8_t> bytes;if(!wi::helper::saveTextureToMemory(s.foam[s.state],bytes)||bytes.size()!=out.cells*sizeof(XMFLOAT4)){error="Connected foam readback failed";return false;}
  for(size_t k=0;k<out.cells;++k){XMFLOAT4 v;std::memcpy(&v,bytes.data()+k*sizeof(v),sizeof(v));if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.w)||v.x<-.000001f){error="Connected foam contains invalid GPU state";return false;}out.foamMass+=v.x;out.maximumDensity=std::max(out.maximumDensity,v.x);}}
 return true;
}
}
