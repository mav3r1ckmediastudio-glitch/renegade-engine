#include "renegade/bridge/RiverFlowRenderer.h"
#include "renegade/bridge/RiverFlowFieldService.h"
#include "renegade/bridge/IdentityService.h"
#include "RiverFlowTransport.h"
#include "RiverFlowSourceSelection.h"
#include "RiverWaterwayRenderer.h"
#include "RiverCourseRenderer.h"
#include "RiverFoamAppearanceGpu.h"
#include "RiverShaderAssets.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
using namespace DirectX;
namespace renegade::bridge {
namespace {
using namespace wi::graphics;
constexpr uint32_t Tag=0x52464c35;
void Require(bool ok,const std::string& text){if(!ok)throw std::runtime_error(text);}
uint32_t Bits(float f){uint32_t n;std::memcpy(&n,&f,4);return n;}
struct Params {XMFLOAT4 bounds;XMUINT4 control;XMFLOAT4 settings;XMUINT4 foamControl;XMFLOAT4 foamSettings;XMUINT4 motion,detail,artwork;};
struct SimPush {XMUINT4 resolution;XMFLOAT4 bounds,settings;};
static_assert(sizeof(Params)==128&&sizeof(SimPush)==48);
#include "RiverFlowGather.inc"
struct Hash {
 uint64_t value=1469598103934665603ull;
 void Add(const void* p,size_t count){auto* b=static_cast<const unsigned char*>(p);for(size_t i=0;i<count;++i){value^=b[i];value*=1099511628211ull;}}
 template<class T>void Add(const T& v){Add(&v,sizeof(v));}
 void Text(const std::string& s){Add(s.data(),s.size());}
};
struct MeshRevision {uint64_t epoch=~0ull,hash=0;size_t vertices=0,indices=0;};
using MeshRevisions=std::unordered_map<wi::ecs::Entity,MeshRevision>;
uint64_t Fingerprint(const wi::scene::Scene& s,wi::ecs::Entity river,uint64_t revision,const RiverFlowField* field,MeshRevisions& meshRevisions) {
 Hash h;
 if(const auto* m=s.materials.GetComponent(river)){h.Add(m->texMulAdd.x);h.Add(m->texMulAdd.y);}
 const auto* spline=s.splines.GetComponent(river);h.Add(spline->width);h.Add(spline->rotation);h.Add(spline->mesh_generation_subdivision);
 XMFLOAT4X4 world;XMStoreFloat4x4(&world,s.ComputeEntityMatrixRecursive(river));h.Add(world);
 for(auto e:spline->spline_node_entities){h.Add(e);if(const auto* t=s.transforms.GetComponent(e)){h.Add(t->translation_local);h.Add(t->rotation_local);h.Add(t->scale_local);}}
 const auto* metadata=s.metadatas.GetComponent(river);
 if(metadata&&metadata->string_values.has(RiverFlowProfileMetadataKey))h.Text(metadata->string_values.get(RiverFlowProfileMetadataKey));
 // Cheap static-component revision scan; no vertices or terrain sampling here.
 river_flow_detail::SourceBounds bounds;
 if(field)bounds=river_flow_detail::CaptureSourceBounds(*field);
 for(size_t i=0;i<s.objects.GetCount();++i) {
  const auto& object=s.objects[i];const auto* mesh=s.meshes.GetComponent(object.meshID);
  const auto e=s.objects.GetEntity(i);
  if(!mesh||!river_flow_detail::IsPotentialSource(s,e,object,*mesh,field?&bounds:nullptr))continue;
  h.Add(e);h.Add(object.meshID);h.Add(object.IsRenderable());
  XMStoreFloat4x4(&world,s.ComputeEntityMatrixRecursive(e));h.Add(world);
  auto& cached=meshRevisions[object.meshID];
  if(cached.epoch!=revision||cached.vertices!=mesh->vertex_positions.size()||cached.indices!=mesh->indices.size()){
   Require(mesh->vertex_positions.size()<=2000000&&mesh->indices.size()<=6000000,"Static source geometry exceeds river cache limits");
   Hash geometry;geometry.Add(mesh->vertex_positions.data(),mesh->vertex_positions.size()*sizeof(XMFLOAT3));
   geometry.Add(mesh->indices.data(),mesh->indices.size()*sizeof(uint32_t));
   cached={revision,geometry.value,mesh->vertex_positions.size(),mesh->indices.size()};
  }
  h.Add(cached.hash);
 }
 return h.value;
}
void Upload(Texture& texture,const std::vector<XMFLOAT4>& data,unsigned width,unsigned height,bool writable=false) {
 TextureDesc td;td.width=width;td.height=height;td.format=Format::R32G32B32A32_FLOAT;
 td.layout=ResourceState::SHADER_RESOURCE;td.bind_flags=BindFlag::SHADER_RESOURCE;
 if(writable)td.bind_flags|=BindFlag::UNORDERED_ACCESS;
 SubresourceData sd;sd.data_ptr=data.data();sd.row_pitch=width*sizeof(XMFLOAT4);sd.slice_pitch=sd.row_pitch*height;
 Require(GetDevice()->CreateTexture(&td,&sd,&texture),"River texture allocation failed");
}
uint32_t SRV(const GPUResource& resource) {
 int index=GetDevice()->GetDescriptorIndex(&resource,SubresourceType::SRV);
 Require(index>=0,"River resource SRV missing");return uint32_t(index);
}
}
struct RiverFlowRenderer::Impl {
 RiverWaterwayRenderer waterways;RiverCourseRenderer courses;
 explicit Impl(std::string directory):waterways(directory),courses(std::move(directory)){}
 struct Entry {
  wi::ecs::Entity entity=wi::ecs::INVALID_ENTITY;std::string id,error="Native water";
  RiverFlowDiagnostics diagnostics;
  uint64_t fingerprint=0;RiverFlowProfile profile;bool ready=false,bound=false;
  float clock=0,step=0;unsigned state=0;mutable bool dispatched=true;
  int originalShader=-1;XMUINT4 originalUserdata{};
  std::shared_ptr<const RiverFlowField> field;
  RiverFoamAppearanceGpu appearance;Texture foam[2],velocity,source,base;GPUBuffer parameters;XMFLOAT4 bounds{};
 };
 wi::scene::Scene* scene=nullptr;uint64_t revision=~0ull;
 std::unordered_map<wi::ecs::Entity,Entry> entries;
 Shader pixel,simulation;int shaderID=-1;bool shaderAttempted=false;std::string shaderError;
 bool dirty=true,suspended=false,terrainDeferred=false;MeshRevisions meshRevisions;
 void LoadShaders() {
  if(shaderAttempted){Require(shaderID>=0,shaderError);return;}shaderAttempted=true;
  try{
   auto* gpu=GetDevice();wi::vector<uint8_t> bytes;
   auto load=[&](const char* file,ShaderStage stage,Shader& shader){
    Require(wi::helper::FileRead(river_shader_assets::Directory()+"/"+file,bytes),
      std::string("Missing river shader in executable Content/river: ")+file);
    Require(gpu->CreateShader(stage,bytes.data(),bytes.size(),&shader),"River shader creation failed");};
   load("RiverRefinedFoamPS.cso",ShaderStage::PS,pixel);load("RiverFoamTransportCS.cso",ShaderStage::CS,simulation);
   wi::renderer::CustomShader custom;custom.name="Renegade River V2";custom.filterMask=wi::enums::FILTER_TRANSPARENT|wi::enums::FILTER_WATER;
   PipelineStateDesc pso;pso.vs=wi::renderer::GetShader(wi::enums::VSTYPE_OBJECT_COMMON);pso.ps=&pixel;
   pso.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENT);
   pso.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_TRANSPARENT);
   pso.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_DOUBLESIDED);
   Require(gpu->CreatePipelineState(&pso,&custom.pso[wi::enums::RENDERPASS_MAIN]),"River main PSO failed");
   pso.vs=wi::renderer::GetShader(wi::enums::VSTYPE_SHADOW_TRANSPARENT);pso.ps=wi::renderer::GetShader(wi::enums::PSTYPE_SHADOW_TRANSPARENT);
   pso.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENTSHADOW);
   pso.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_DEPTHREAD);
   pso.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_SHADOW_DOUBLESIDED);
   Require(gpu->CreatePipelineState(&pso,&custom.pso[wi::enums::RENDERPASS_SHADOW]),"River shadow PSO failed");
   shaderID=wi::renderer::RegisterCustomShader(custom);
  }catch(const std::exception& x){shaderError=x.what();throw;}
 }
 void Rebuild(Entry& entry,const RiverFlowProfile& profile,uint64_t fingerprint) {
  auto start=std::chrono::steady_clock::now();
  auto* gpu=GetDevice();gpu->WaitForGPU();entry.ready=false;
  entry.profile=profile;entry.fingerprint=fingerprint;
  const auto* material=scene->materials.GetComponent(entry.entity);
  Require(material->customShaderID<0,"River flow cannot replace another custom material shader");
  entry.originalShader=material->customShaderID;entry.originalUserdata=material->userdata;
  RiverFlowSettings settings;settings.baseSpeedMetersPerSecond=profile.speed;settings.downstreamReversed=profile.reversed;
  std::string error;std::shared_ptr<const RiverFlowField> field;
  Require(BuildRiverFlowField(*scene,entry.entity,settings,field,error),error);
  auto surfaces=Gather(*scene,entry.entity,*field);
  auto mask=river_v2_classification::Build(*field,surfaces);
  auto transport=river_v2_transport::Prepare(*field,mask,1,profile.sourceWidth);
  // Separate actual obstacle slices shed separate wakes, not one aggregate wake
  // at the centre of all rocks in a watercourse.
  struct Wake {XMFLOAT2 centre{};float radius=.25f;};
  std::vector<Wake> wakes;
  for(const auto& surface:surfaces)if(!surface.terrain){
   std::vector<river_v2_classification::Segment> lines;
   for(const auto& triangle:surface.triangles){auto slice=river_v2_classification::Slice(triangle,field->NativeRibbon().front().centre.y);lines.insert(lines.end(),slice.begin(),slice.end());}
   if(lines.empty())continue;Wake w;
   for(auto l:lines){w.centre.x+=l.a.x+l.b.x;w.centre.y+=l.a.y+l.b.y;}
   w.centre.x/=float(lines.size()*2);w.centre.y/=float(lines.size()*2);
   for(auto l:lines)for(auto p:{l.a,l.b})w.radius=std::max(w.radius,std::hypot(p.x-w.centre.x,p.y-w.centre.y));
   wakes.push_back(w);
  }
  // Author may choose ANY original spline control point as the whitewater
  // origin (e.g. the locally widened Point 2 under a waterfall).
  // Resolve ONCE at rebuild, never per texel or per frame. Existing projects
  // keep -1 and emit at their normal upstream head.
  float whitewaterAlong=0.f;
  bool whitewaterSourceValid=true;
  if(profile.startWhitewaterNodeIndex>=0 ||
     !profile.startWhitewaterNodeId.empty()){
   const auto node=ResolveRiverWhitewaterSourceNode(*scene,entry.entity,profile);
   if(node==wi::ecs::INVALID_ENTITY){
    // Deleted source: preserve water, wakes and regular obstacle foam, but
    // NEVER paint the selected-origin foam at a different numbered node.
    whitewaterSourceValid=false;
   }else{
    auto pose=scene->ComputeEntityMatrixRecursive(node);
    XMFLOAT3 world;
    XMStoreFloat3(&world,pose.r[3]);
    const auto start=field->Evaluate({world.x,world.z});
    if(start.valid&&!start.ambiguous)whitewaterAlong=start.alongMeters;
    else whitewaterSourceValid=false;
   }
  }
  std::vector<XMFLOAT4> normals(mask.cells.size());
  for(size_t k=0;k<normals.size();++k){
   XMFLOAT2 p{mask.bounds.x+(k%mask.width+.5f)/mask.width*mask.bounds.z,mask.bounds.y+(k/mask.width+.5f)/mask.height*mask.bounds.w};
   auto q=field->Evaluate(p);if(q.valid)normals[k]={q.directionXZ.x,q.directionXZ.y,q.speedMetersPerSecond,1};
   float wake=0,lateral=0;
   if(q.valid&&profile.speed>0)for(auto w:wakes){
    float a=(p.x-w.centre.x)*q.directionXZ.x+(p.y-w.centre.y)*q.directionXZ.y-w.radius;
    float b=-(p.x-w.centre.x)*q.directionXZ.y+(p.y-w.centre.y)*q.directionXZ.x;
    float width=w.radius*.65f+std::max(0.f,a+w.radius)*.12f;
    float influence=a>0?std::clamp(a/.7f,0.f,1.f)*std::exp(-a/5.f-b*b/(width*width)):0;
    if(influence>wake){wake=influence;lateral=b;}
   }
   transport.source[k].z=wake;transport.source[k].w=lateral;
   // The start of a standalone river can act as the whitewater impact zone.
   // Source is injected into G11's EXISTING transported-foam channel (G),
   // so it decays/breaks up downstream; no extra particle, shader or water.
   if(profile.startWhitewater && whitewaterSourceValid &&
      profile.foam && profile.enabled &&
      profile.speed>0 && q.valid && !q.ambiguous){
    const float delta=q.alongMeters-whitewaterAlong;
    const float downstream=std::max(0.f,delta);
    if(delta>=-.01f && downstream<5.f){
      const float shore=std::clamp(q.distanceToBankMeters/.34f,0.f,1.f);
      const float longitudinal=std::exp(-downstream/1.45f);
      const float cross=1.f-.28f*std::abs(q.lateralFraction);
      // Calibrated as a modest extra SOURCE rate, not a white blanket.
      transport.source[k].y+=.10f*profile.startWhitewaterAmount*
        longitudinal*shore*cross;
    }
   }
  }
  Upload(entry.velocity,transport.velocity,mask.width,mask.height);
  Upload(entry.source,transport.source,mask.width,mask.height);Upload(entry.base,normals,mask.width,mask.height);
  // A changed domain must not reinterpret old density at unrelated world positions.
  // Profile-only edits retain state when the exact world-space bounds match.
  bool same=entry.foam[0].IsValid()&&entry.bounds.x==mask.bounds.x&&entry.bounds.y==mask.bounds.y&&entry.bounds.z==mask.bounds.z&&entry.bounds.w==mask.bounds.w;
  if(!same){std::vector<XMFLOAT4> zero(normals.size());Upload(entry.foam[0],zero,mask.width,mask.height,true);Upload(entry.foam[1],zero,mask.width,mask.height,true);entry.state=0;entry.clock=0;}
  entry.bounds=mask.bounds;entry.field=std::move(field);
  const float tile=entry.field->Samples().front().widthMeters/material->texMulAdd.x;
  Require(std::isfinite(tile)&&tile>.01f&&tile<1000,"River normal tiling is unsupported");
  if(!same)entry.appearance.Configure(river_shader_assets::Directory(),mask.width,mask.height);
  Params params{mask.bounds,{SRV(entry.base),Bits(-1),Bits(1),0},{tile,std::min(4.f,tile/std::max(profile.speed,.001f)),0,1},
   {SRV(entry.foam[0]),SRV(entry.foam[1]),SRV(entry.source),SRV(entry.velocity)},{profile.foam?profile.intensity:0,profile.threshold,0,0},{0,0,0,0},entry.appearance.DetailControls(),entry.appearance.ArtworkControls()};
  GPUBufferDesc bd;bd.size=sizeof(params);bd.bind_flags=BindFlag::SHADER_RESOURCE;bd.misc_flags=ResourceMiscFlag::BUFFER_RAW;
  Require(gpu->CreateBuffer(&bd,&params,&entry.parameters),"River parameter allocation failed");
  ++entry.diagnostics.rebuilds;entry.diagnostics.cells=normals.size();
  entry.diagnostics.lastBuildMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  entry.ready=true;entry.error="Flow ready | "+std::to_string(profile.speed)+" m/s | "+(profile.foam?"contact foam":"normals only");
  if(profile.startWhitewater && !whitewaterSourceValid)
    entry.error+=" | WHITEWATER SOURCE MISSING - select a new river node";
 }
};
RiverFlowRenderer::RiverFlowRenderer(std::string directory):impl_(std::make_unique<Impl>([&]{
 // Native project/model browsers can temporarily change the process working
 // directory. Engine-owned water resources belong to this executable, never
 // the project selected in that browser. Explicit custom paths remain supported.
 auto path=std::filesystem::u8path(directory);
 if(path.is_relative())path=directory=="Content/river"
  ? std::filesystem::u8path(wi::helper::GetExecutablePath()).parent_path()/path
  : std::filesystem::absolute(path);
 return path.lexically_normal().generic_u8string();
}())){}
RiverFlowRenderer::~RiverFlowRenderer()=default;
void RiverFlowRenderer::Invalidate(){impl_->dirty=true;impl_->meshRevisions.clear();impl_->waterways.Invalidate();impl_->courses.Invalidate();}
void RiverFlowRenderer::SetSuspended(bool value){impl_->suspended=value;impl_->waterways.SetSuspended(value);impl_->courses.SetSuspended(value);}
void RiverFlowRenderer::Prepare(wi::scene::Scene& scene,uint64_t revision,float dt,uint64_t editRevision){
 auto& state=*impl_;if(state.suspended)return;
 for(size_t k=0;k<scene.terrains.GetCount();++k)if(scene.terrains[k].IsGenerationBusy()){state.terrainDeferred=true;return;}
 if(state.terrainDeferred){state.terrainDeferred=false;state.dirty=true;state.meshRevisions.clear();}
 Require(!std::any_of(state.entries.begin(),state.entries.end(),[](const auto& e){return e.second.bound;}),"River bindings must be scoped");
 if(state.scene!=&scene||state.revision!=revision){
  // Scene replacement and command revisions both invalidate source caches.
  // Only replacement changes scene ownership; preserve histories on edit.
  if(state.scene!=&scene||state.revision!=revision){if(!state.entries.empty())GetDevice()->WaitForGPU();state.entries.clear();state.meshRevisions.clear();}
  state.scene=&scene;state.revision=revision;state.dirty=true;
 }
 state.waterways.Prepare(scene,revision,dt,editRevision);
 state.courses.Prepare(scene,revision,dt,editRevision);
 std::unordered_set<wi::ecs::Entity> seen;unsigned active=(state.waterways.Ready()?1:0)+unsigned(state.courses.ActiveCount());
 for(size_t i=0;i<scene.splines.GetCount();++i){
  auto entity=scene.splines.GetEntity(i);const auto* material=scene.materials.GetComponent(entity);
  if(!material||material->shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER)continue;
  if(state.waterways.Owns(entity)||state.courses.Owns(entity))continue;
  RiverFlowProfile profile;std::string error;
  bool valid=ReadRiverFlowProfile(scene,entity,profile,error);
  auto existing=state.entries.find(entity);
  if(valid&&!profile.enabled){if(existing!=state.entries.end()){GetDevice()->WaitForGPU();state.entries.erase(existing);}continue;}
  seen.insert(entity);auto& entry=state.entries[entity];entry.entity=entity;entry.step=0;
  if(!valid){entry.ready=false;entry.error=error;continue;}
  if(++active>16){entry.ready=false;entry.error="At most16 active flow rivers per scene";continue;}
  try{
   state.LoadShaders();auto id=PersistentEntityId(scene,entity);Require(IsValidStableId(id),"River needs a stable persistent identity");
   // Scene replacement may reuse entity IDs. The renderer's caller folds the
   // scene lifecycle into revision, and identity is checked independently.
   if(entry.id!=id){if(entry.foam[0].IsValid())GetDevice()->WaitForGPU();entry.ready=false;entry.foam[0]={};entry.foam[1]={};entry.id=id;}
   auto hash=Fingerprint(scene,entity,editRevision,entry.field.get(),state.meshRevisions);
   if(state.dirty||entry.fingerprint!=hash){state.Rebuild(entry,profile,hash);entry.fingerprint=Fingerprint(scene,entity,editRevision,entry.field.get(),state.meshRevisions);}
   if(entry.ready){entry.step=std::isfinite(dt)?std::clamp(dt,0.f,1.f/30):0;entry.clock+=entry.step;entry.state=1-entry.state;entry.appearance.Prepare(entry.step);entry.dispatched=false;}
  }catch(const std::exception& x){entry.ready=false;entry.error=x.what();}
 }
 for(auto it=state.entries.begin();it!=state.entries.end();)if(!seen.count(it->first)){GetDevice()->WaitForGPU();it=state.entries.erase(it);}else ++it;
 state.dirty=false;
}
void RiverFlowRenderer::Apply()const{
 auto& state=*impl_;if(!state.scene||state.terrainDeferred)return;
 state.courses.Apply();if(state.suspended)return;
 state.waterways.Apply();
 for(auto& pair:state.entries){auto& e=pair.second;if(!e.ready)continue;
  auto* material=state.scene->materials.GetComponent(e.entity);if(!material||PersistentEntityId(*state.scene,e.entity)!=e.id)continue;
  e.originalShader=material->customShaderID;e.originalUserdata=material->userdata;
  material->SetCustomShaderID(state.shaderID);material->userdata={SRV(e.parameters),Tag,e.state|(e.appearance.State()<<1),Bits(e.clock)};material->SetDirty();e.bound=true;
 }
}
void RiverFlowRenderer::Restore()const{
 auto& state=*impl_;if(!state.scene)return;
 for(auto& pair:state.entries){auto& e=pair.second;if(!e.bound)continue;
  auto* material=state.scene->materials.GetComponent(e.entity);
  if(material&&material->userdata.y==Tag){material->SetCustomShaderID(e.originalShader);material->userdata=e.originalUserdata;material->SetDirty();}
  e.bound=false;
 }
 state.courses.Restore();state.waterways.Restore();
}
RiverFlowRenderer::BindingScope::BindingScope(const RiverFlowRenderer& owner):owner_(owner){owner_.Apply();}
RiverFlowRenderer::BindingScope::~BindingScope(){owner_.Restore();}
void RiverFlowRenderer::Dispatch()const{
 const auto& state=*impl_;if(state.suspended||state.terrainDeferred)return;state.waterways.Dispatch();state.courses.Dispatch();auto* gpu=GetDevice();
 for(const auto& pair:state.entries){const auto& e=pair.second;if(!e.ready||e.dispatched)continue;e.dispatched=true;
  auto cmd=gpu->BeginCommandList();auto first=GPUBarrier::Image(&e.foam[e.state],ResourceState::SHADER_RESOURCE,ResourceState::UNORDERED_ACCESS);
  gpu->Barrier(&first,1,cmd);wi::renderer::BindCommonResources(cmd);gpu->BindComputeShader(&state.simulation,cmd);
  gpu->BindResource(&e.foam[1-e.state],0,cmd);gpu->BindResource(&e.velocity,1,cmd);gpu->BindResource(&e.source,2,cmd);gpu->BindResource(&e.base,3,cmd);gpu->BindUAV(&e.foam[e.state],0,cmd);
  const auto& td=e.foam[e.state].GetDesc();SimPush push{{td.width,td.height,Bits(e.profile.turbulence),0},e.bounds,{e.step,e.clock,e.profile.lifetime,1}};
  gpu->PushConstants(&push,sizeof(push),cmd);gpu->Dispatch((td.width+7)/8,(td.height+7)/8,1,cmd);
  auto end=GPUBarrier::Image(&e.foam[e.state],ResourceState::UNORDERED_ACCESS,ResourceState::SHADER_RESOURCE);gpu->Barrier(&end,1,cmd);
  e.appearance.Dispatch(e.source,e.velocity,e.foam[e.state],e.bounds,e.clock,3);
 }
}
void RiverFlowRenderer::DrawArrows(wi::ecs::Entity entity)const{
 if(impl_->courses.Owns(entity)){impl_->courses.DrawArrows(entity);return;}
 if(impl_->waterways.Owns(entity)){impl_->waterways.DrawArrows();return;}
 auto it=impl_->entries.find(entity);if(it==impl_->entries.end()||!it->second.ready)return;
 const auto& samples=it->second.field->Samples();size_t step=std::max<size_t>(1,samples.size()/64);
 for(size_t i=0;i<samples.size();i+=step){auto a=samples[i].worldPosition;a.y+=.06f;auto b=a;b.x+=samples[i].worldXZUnitTangent.x*.5f;b.z+=samples[i].worldXZUnitTangent.y*.5f;
 wi::renderer::RenderableLine line;line.start=a;line.end=b;line.color_start=line.color_end=XMFLOAT4(.1f,.9f,1,1);
 wi::renderer::DrawLine(line,false);
 auto tip=b;tip.x-=samples[i].worldXZUnitTangent.x*.15f;tip.z-=samples[i].worldXZUnitTangent.y*.15f;
 line.start=b;line.end={tip.x-samples[i].worldXZUnitTangent.y*.1f,tip.y,tip.z+samples[i].worldXZUnitTangent.x*.1f};wi::renderer::DrawLine(line,false);
 line.end={tip.x+samples[i].worldXZUnitTangent.y*.1f,tip.y,tip.z-samples[i].worldXZUnitTangent.x*.1f};wi::renderer::DrawLine(line,false);}
}
const std::string& RiverFlowRenderer::Status(wi::ecs::Entity e)const{
 static const std::string pending="Flow preview waits until editing and terrain generation settle";
 static const std::string native="Native water | flow disabled";
 if(impl_->courses.Owns(e))return impl_->courses.Status(e);
 if(impl_->waterways.Owns(e))return impl_->waterways.Status();
 if((impl_->suspended||impl_->terrainDeferred)&&impl_->entries.count(e))return pending;auto it=impl_->entries.find(e);return it==impl_->entries.end()?native:it->second.error;
}
size_t RiverFlowRenderer::ActiveCount()const{size_t count=(impl_->waterways.Ready()?1:0)+impl_->courses.ActiveCount();for(const auto& p:impl_->entries)if(p.second.ready)++count;return count;}
bool RiverFlowRenderer::Diagnostics(wi::ecs::Entity entity,RiverFlowDiagnostics& out,std::string& error,bool readback)const{
 if(impl_->courses.Owns(entity))return impl_->courses.Diagnostics(entity,out,error,readback);
 if(impl_->waterways.Owns(entity))return impl_->waterways.Diagnostics(out,error,readback);
 auto it=impl_->entries.find(entity);error.clear();
 if(it==impl_->entries.end()||!it->second.ready){error=Status(entity);return false;}
 const auto& e=it->second;out=e.diagnostics;
 if(readback){e.appearance.Readback(out.maximumDetail,out.detailSignature);GetDevice()->WaitForGPU();wi::vector<uint8_t> bytes;
  if(!wi::helper::saveTextureToMemory(e.foam[e.state],bytes)||bytes.size()!=out.cells*sizeof(XMFLOAT4)){error="Foam readback failed";return false;}
  for(size_t k=0;k<out.cells;++k){XMFLOAT4 value;std::memcpy(&value,bytes.data()+k*sizeof(value),sizeof(value));
   if(!std::isfinite(value.x)||!std::isfinite(value.y)||!std::isfinite(value.z)||!std::isfinite(value.w)||value.x<0||value.x>3.001f){error="Invalid GPU foam state";return false;}
   out.foamMass+=value.x;out.maximumDensity=std::max(out.maximumDensity,value.x);
  }
 }
 return true;
}

}
