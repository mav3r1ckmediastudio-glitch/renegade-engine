#include "RiverCourseRenderer.h"
#include "RiverShaderAssets.h"
#include "RiverCourseSurfaceBinding.h"
#include "RiverCourseField.h"
#include "RiverGradedTransport.h"
#include "RiverFoamAppearanceGpu.h"
#include "RiverFlowSourceSelection.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/RiverWaterwayService.h"
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <cstring>
#include <future>
#include <thread>
using namespace DirectX;
namespace renegade::bridge {
namespace {
using namespace wi::graphics;
void Require(bool v,const std::string& text){if(!v)throw std::runtime_error(text);}
uint32_t Bits(float f){uint32_t v;std::memcpy(&v,&f,4);return v;}
uint32_t SRV(const GPUResource& r){int n=GetDevice()->GetDescriptorIndex(&r,SubresourceType::SRV);Require(n>=0,"Course resource descriptor missing");return uint32_t(n);}
struct Params{XMFLOAT4 bounds;XMUINT4 control;XMFLOAT4 settings;XMUINT4 foamControl;XMFLOAT4 foamSettings;XMUINT4 motion,detail,artwork,path;};
struct Push{XMUINT4 resolution;XMFLOAT4 bounds,settings;};
static_assert(sizeof(Params)==144&&sizeof(Push)==48);
void Upload(Texture& target,const std::vector<XMFLOAT4>& data,unsigned w,unsigned h,bool writable=false){
 TextureDesc desc;desc.width=w;desc.height=h;desc.format=Format::R32G32B32A32_FLOAT;desc.layout=ResourceState::SHADER_RESOURCE;desc.bind_flags=BindFlag::SHADER_RESOURCE;if(writable)desc.bind_flags|=BindFlag::UNORDERED_ACCESS;
 SubresourceData initial;initial.data_ptr=data.data();initial.row_pitch=w*sizeof(XMFLOAT4);initial.slice_pitch=initial.row_pitch*h;Require(GetDevice()->CreateTexture(&desc,&initial,&target),"Course texture allocation failed");
}
struct Hash{uint64_t value=1469598103934665603ull;void Add(const void* p,size_t n){auto* b=static_cast<const unsigned char*>(p);while(n--){value^=*b++;value*=1099511628211ull;}}template<class T>void Add(const T& v){Add(&v,sizeof(v));}};
#include "RiverFlowGather.inc"
uint64_t AuthorHash(const wi::scene::Scene& scene,wi::ecs::Entity root,const RiverFlowProfile& p){
 Hash h;const auto* s=scene.splines.GetComponent(root);h.Add(s->width);h.Add(s->rotation);h.Add(s->mesh_generation_subdivision);
 XMFLOAT4X4 world;XMStoreFloat4x4(&world,scene.ComputeEntityMatrixRecursive(root));h.Add(world);
 for(auto n:s->spline_node_entities){const auto* t=scene.transforms.GetComponent(n);Require(t,"Course control missing");h.Add(t->translation_local);h.Add(t->rotation_local);h.Add(t->scale_local);}
 h.Add(p.reversed);return h.value;
}
}
struct RiverCourseRenderer::Impl {
 struct Entry {
  wi::ecs::Entity root=wi::ecs::INVALID_ENTITY;std::string id,status="Elevated river pending";RiverFlowProfile profile;RiverCourseSurface surface;
  RiverCourseSurfaceBinding geometry;RiverFoamAppearanceGpu appearance;Texture foam[2],flux,source,velocity,base,path;GPUBuffer parameters;Params params{};
  river_flow_detail::SourceBounds bounds;uint64_t author=0,sources=0,editRevision=~0ull;bool ready=false,bound=false,enabled=false;RiverFlowDiagnostics diagnostics;
  std::future<river_graded_transport::Domain> pending;
  uint64_t pendingSources=0,failedSources=0;bool sourceFailed=false,rockSourcesSuppressed=false;
  unsigned state=0,start=0,steps=0;float clock=0,step=0,maximumRate=0,builtSpeed=-1;mutable bool dispatched=true;
  int originalShader=-1;XMUINT4 originalUserdata{};std::vector<wi::renderer::RenderableLine> arrows;std::vector<XMFLOAT4> sourceRates;std::vector<unsigned char> automaticRapid;
 };
 struct MeshCache{uint64_t revision=~0ull,hash=0;size_t vertices=0,indices=0;};
 std::string directory;wi::scene::Scene* scene=nullptr;uint64_t revision=~0ull;bool suspended=false,dirty=true;
 std::unordered_map<wi::ecs::Entity,std::unique_ptr<Entry>> entries;std::unordered_map<wi::ecs::Entity,MeshCache> meshes;
 Shader ps,vs,shadow,simulation;int shader=-1;
 explicit Impl(std::string d):directory(std::move(d)){}
 uint64_t Sources(const river_flow_detail::SourceBounds& b,uint64_t revision,const RiverFlowProfile& p){
  Hash h;h.Add(p.speed);h.Add(p.sourceWidth);for(size_t k=0;k<scene->objects.GetCount();++k){auto e=scene->objects.GetEntity(k);const auto& o=scene->objects[k];const auto* m=scene->meshes.GetComponent(o.meshID);
   if(!m||!river_flow_detail::IsPotentialSource(*scene,e,o,*m,&b,true))continue;
   h.Add(e);h.Add(o.meshID);h.Add(o.IsRenderable());XMFLOAT4X4 matrix;XMStoreFloat4x4(&matrix,scene->ComputeEntityMatrixRecursive(e));h.Add(matrix);
   auto& c=meshes[o.meshID];if(c.revision!=revision||c.vertices!=m->vertex_positions.size()||c.indices!=m->indices.size()){
    Require(m->vertex_positions.size()<=2000000&&m->indices.size()<=6000000,"Course obstacle mesh exceeds limits");Hash content;content.Add(m->vertex_positions.data(),m->vertex_positions.size()*sizeof(XMFLOAT3));content.Add(m->indices.data(),m->indices.size()*sizeof(uint32_t));c={revision,content.value,m->vertex_positions.size(),m->indices.size()};
   }h.Add(c.hash);
  }return h.value;
 }
 // Only immutable CPU snapshots cross the worker boundary. No ECS, GPU,
 // renderer, or Entry reference is accessed by a worker; stale results are discarded.
 void Obstacles(Entry& e,const RiverFlowProfile& p,uint64_t sourceHash,uint64_t author,uint64_t editRevision){
  if(e.pending.valid()){
   if(e.pending.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
   auto requested=e.pendingSources;
   try{
    auto domain=e.pending.get();
    wi::backlog::post("RIVER_OBSTACLE_RESULT requested="+std::to_string(requested)+" latest="+std::to_string(sourceHash)+" stale="+std::to_string(requested!=sourceHash));
    if(requested==sourceHash){Rebuild(e,p,author,editRevision,&domain);e.sourceFailed=false;return;}
   }catch(const std::exception& ex){
    if(requested==sourceHash){e.failedSources=sourceHash;e.sourceFailed=true;e.status=std::string("River obstacle update failed: ")+ex.what();return;}
   }
  }
  if(e.sourceFailed&&e.failedSources==sourceHash)return;
  // A moved rock must stop emitting at its old location immediately.
  // Existing foam continues to advect/decay on the last valid flow field.
  if(!e.rockSourcesSuppressed){
   GetDevice()->WaitForGPU();auto rates=e.sourceRates;
   for(size_t k=0;k<rates.size();++k){rates[k].x=0;rates[k].w*=p.rapidIntensity*((p.rapids||e.automaticRapid[k])?1.f:0.f);}
   const auto desc=e.source.GetDesc();Upload(e.source,rates,desc.width,desc.height);
   e.params.foamControl.z=SRV(e.source);e.rockSourcesSuppressed=true;Parameters(e,p);
  }
  auto captureBegin=std::chrono::steady_clock::now();
  auto obstacles=GatherBounds(*scene,e.root,e.bounds,nullptr,true);
  size_t triangles=0;for(const auto& obstacle:obstacles)triangles+=obstacle.triangles.size();
  wi::backlog::post("RIVER_OBSTACLE_CAPTURE triangles="+std::to_string(triangles)+" milliseconds="+std::to_string(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-captureBegin).count()));
  auto flow=e.surface.flow;const float halfWidth=scene->splines.GetComponent(e.root)->width;
  const float inset=halfWidth-std::min(.35f,halfWidth*.15f)+.3f;
  const float speed=p.speed,sourceWidth=p.sourceWidth;
  std::promise<river_graded_transport::Domain> promise;e.pending=promise.get_future();e.pendingSources=sourceHash;
  std::thread([promise=std::move(promise),flow,obstacles=std::move(obstacles),inset,speed,sourceWidth]()mutable{
   try{river_graded::Settings settings;settings.inletSpeed=std::max(.051f,speed);settings.maximumGrade=8;settings.maximumHeightRange=2.5f;
    auto begin=std::chrono::steady_clock::now();river_course::Field field(*flow,settings,inset);
    auto domain=river_graded_transport::Build(field,obstacles,512,256,sourceWidth);
    wi::backlog::post("RIVER_OBSTACLE_SOLVE milliseconds="+std::to_string(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count())+" active="+std::to_string(domain.active.size()));
    promise.set_value(std::move(domain));
   }catch(...){promise.set_exception(std::current_exception());}
  }).detach();
  e.status="River ready | updating obstacle flow";
 }
 void Load(){
  if(shader>=0)return;auto* gpu=GetDevice();wi::vector<uint8_t> bytes;
  auto load=[&](const char* file,ShaderStage stage,Shader& output){Require(wi::helper::FileRead(directory+"/"+file,bytes),std::string("Missing course shader: ")+file);Require(gpu->CreateShader(stage,bytes.data(),bytes.size(),&output),"Course shader creation failed");};
  load("RiverCoursePS.cso",ShaderStage::PS,ps);load("RiverCourseVS.cso",ShaderStage::VS,vs);load("RiverCourseShadowVS.cso",ShaderStage::VS,shadow);load("RiverRapidsCS.cso",ShaderStage::CS,simulation);
  wi::renderer::CustomShader custom;custom.name="Renegade automatic river course";custom.filterMask=wi::enums::FILTER_TRANSPARENT|wi::enums::FILTER_WATER;
  PipelineStateDesc p;p.vs=&vs;p.ps=&ps;p.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENT);p.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_TRANSPARENT);p.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_DOUBLESIDED);
  Require(gpu->CreatePipelineState(&p,&custom.pso[wi::enums::RENDERPASS_MAIN]),"Course WATER pipeline failed");
  p.vs=&shadow;p.ps=wi::renderer::GetShader(wi::enums::PSTYPE_SHADOW_TRANSPARENT);p.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENTSHADOW);p.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_DEPTHREAD);p.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_SHADOW_DOUBLESIDED);
  Require(gpu->CreatePipelineState(&p,&custom.pso[wi::enums::RENDERPASS_SHADOW]),"Course shadow pipeline failed");shader=wi::renderer::RegisterCustomShader(custom);
 }
 void Parameters(Entry& e,const RiverFlowProfile& p){
  if(e.ready&&(p.rapidIntensity!=e.profile.rapidIntensity||p.rapids!=e.profile.rapids)){
   auto rates=e.sourceRates;for(size_t k=0;k<rates.size();++k){rates[k].w*=p.rapidIntensity*((p.rapids||e.automaticRapid[k])?1.f:0.f);if(e.rockSourcesSuppressed)rates[k].x=0;}
   const auto desc=e.source.GetDesc();Upload(e.source,rates,desc.width,desc.height);e.params.foamControl.z=SRV(e.source);
  }
  e.enabled=p.enabled&&p.speed>0;e.params.settings.w=p.enabled?1.f:0.f;e.profile=p;e.params.foamSettings={p.enabled&&p.speed>0&&p.foam?p.intensity:0,p.threshold,0,p.rapidIntensity};e.params.path.w=p.enabled?1u:0u;e.params.path.z=Bits(p.turbulence);
  e.params.control.z=Bits(p.enabled&&p.speed==0?0.f:1.f);
  GPUBufferDesc desc;desc.size=sizeof(Params);desc.bind_flags=BindFlag::SHADER_RESOURCE;desc.misc_flags=ResourceMiscFlag::BUFFER_RAW;
  Require(GetDevice()->CreateBuffer(&desc,&e.params,&e.parameters),"Course parameters allocation failed");
 }
 void Rebuild(Entry& e,const RiverFlowProfile& p,uint64_t author,uint64_t editRevision,river_graded_transport::Domain* prepared=nullptr){
  auto begin=std::chrono::steady_clock::now();e.ready=false;GetDevice()->WaitForGPU();
  Require(!p.reversed,"Elevated rivers follow downhill node order; reverse is supported on flat water");
  RiverWaterwayControls topology;std::string error;Require(ReadRiverWaterwayControls(*scene,e.root,topology,error),error);
  Require(!topology.connected,"Keep connected junctions level; elevated connected courses are not yet supported");
  const auto* material=scene->materials.GetComponent(e.root);Require(material&&material->customShaderID<0,"Course cannot replace an authored custom shader");
  if(!prepared||e.builtSpeed!=p.speed){
   Require(BuildRiverCourseSurface(*scene,e.root,p.speed,e.surface,error),error);
   Load();scene->Update(0);Require(e.geometry.Configure(*scene,e.root,e.surface,error),error);
  }
  e.bounds=river_flow_detail::CaptureSourceBounds(*e.surface.flow);
  unsigned w=8,h=8;std::vector<XMFLOAT4> zero(64),faces(zero),source(zero),velocity(zero),normals(zero),motion(zero);
  XMFLOAT4 bounds{e.bounds.minimum.x-.25f,e.bounds.minimum.z-.25f,e.bounds.maximum.x-e.bounds.minimum.x+.5f,e.bounds.maximum.z-e.bounds.minimum.z+.5f};
  e.arrows.clear();e.maximumRate=0;e.automaticRapid.assign(64,0);
  if(p.speed>0){
   river_graded::Settings settings;settings.inletSpeed=std::max(.051f,p.speed);settings.maximumGrade=8;settings.maximumHeightRange=2.5f;
   // Keep hydraulic ports inside the visible water, behind the buried native
   // mesh ends. Terrain can retain its accepted concealed rounded caps.
   const float halfWidth=scene->splines.GetComponent(e.root)->width;
   const float inset=halfWidth-std::min(.35f,halfWidth*.15f)+.3f;
   river_course::Field field(*e.surface.flow,settings,inset);
   auto domain=prepared?std::move(*prepared):river_graded_transport::Build(field,GatherBounds(*scene,e.root,e.bounds,nullptr,true),512,256,p.sourceWidth);
   w=domain.mask.width;h=domain.mask.height;bounds=domain.mask.bounds;zero.assign(size_t(w)*h,{});
   faces=std::move(domain.faces);source=std::move(domain.source);velocity=std::move(domain.velocity);normals=velocity;motion.assign(zero.size(),{});e.automaticRapid.assign(zero.size(),0);
   const float dx=bounds.z/w,dz=bounds.w/h;
   for(int k:domain.active){
    XMFLOAT2 q{bounds.x+(k%w+.5f)*dx,bounds.y+(k/w+.5f)*dz};auto section=field.Nearest(q);
    float bank=std::clamp(std::min(domain.mask.cells[k].y,domain.mask.cells[k].z)/.6f,0.f,1.f);
    float amplitude=std::min(.06f,domain.depth[k]*.10f)*bank*bank*(.15f+.85f*std::clamp(domain.froude[k]/2.f,0.f,1.f));
    motion[k]={section.travel*1.9f,amplitude,.5f,1};e.automaticRapid[k]=field.SurfaceScale(q)>=1.031f;
    // Same bounded energy-to-air calibration as accepted G10C/G10D, now on
    // native accumulated XZ arc after each actual rounded toe.
    if(p.startWhitewater&&p.foam&&p.enabled&&p.speed>0 &&
       section.along>=0 && section.along<5.f){
      source[k].y+=.10f*p.startWhitewaterAmount*
        std::exp(-section.along/1.45f)*bank;
    }
    for(const auto& impact:e.surface.impacts){float along=section.along-impact.alongMeters;if(along<0||along>4)continue;
     float power=1000*field.Discharge()*9.80665f*impact.dropMeters;
     source[k].w+=std::min(5.f,power/20000.f)*std::exp(-along/1.1f)*std::clamp(std::min(domain.mask.cells[k].y,domain.mask.cells[k].z)/.35f,0.f,1.f);
     e.automaticRapid[k]=1;
    }
   }
   e.maximumRate=domain.maximumRate;
  }
  e.rockSourcesSuppressed=false;e.sourceRates=source;for(size_t k=0;k<source.size();++k)source[k].w*=p.rapidIntensity*((p.rapids||e.automaticRapid[k])?1.f:0.f);
  const bool preserve=prepared&&e.foam[0].IsValid()&&e.foam[0].GetDesc().width==w&&e.foam[0].GetDesc().height==h;
  if(!preserve){Upload(e.foam[0],zero,w,h,true);Upload(e.foam[1],zero,w,h,true);}
  Upload(e.flux,faces,w,h);Upload(e.source,source,w,h);Upload(e.velocity,velocity,w,h);Upload(e.base,normals,w,h);
  Upload(e.path,e.surface.path,1,unsigned(e.surface.path.size()));
  if(!preserve){e.appearance.Configure(directory,w,h);Upload(e.appearance.waves,motion,w,h);e.state=0;e.clock=0;}
  e.start=e.state;e.steps=0;e.enabled=p.enabled&&p.speed>0;
  e.params=Params{bounds,{SRV(e.base),Bits(-1),Bits(1),0},{4,1.5f,0,p.enabled?1.f:0.f},{SRV(e.foam[0]),SRV(e.foam[1]),SRV(e.source),SRV(e.velocity)},{},{SRV(e.appearance.waves),0u,0,0},e.appearance.DetailControls(),e.appearance.ArtworkControls(),{SRV(e.path),Bits(e.surface.totalTravel),0,p.enabled?1u:0u}};
  Parameters(e,p);e.builtSpeed=p.speed;e.author=author;e.sources=Sources(e.bounds,editRevision,p);e.editRevision=editRevision;e.ready=true;e.dispatched=true;
  const auto& rows=e.surface.flow->NativeRibbon();size_t stride=std::max<size_t>(1,rows.size()/64);
  for(size_t k=0;k+1<rows.size();k+=stride){auto a=rows[k].centre,b=rows[std::min(k+stride,rows.size()-1)].centre;auto d=XMVector3Normalize(XMLoadFloat3(&b)-XMLoadFloat3(&a));XMFLOAT3 tip;XMStoreFloat3(&tip,XMLoadFloat3(&a)+d*.65f);a.y+=.08f;tip.y+=.08f;
   wi::renderer::RenderableLine line;line.start=a;line.end=tip;line.color_start=line.color_end={.05f,1,.8f,1};e.arrows.push_back(line);
   XMVECTOR side=XMVector3Cross(d,XMVectorSet(0,1,0,0));
   if(XMVectorGetX(XMVector3LengthSq(side))<.001f)side=XMVectorSet(1,0,0,0);
   side=XMVector3Normalize(side);
   for(float sign:{-1.f,1.f}){line.start=tip;XMStoreFloat3(&line.end,XMLoadFloat3(&tip)-d*.2f+side*(sign*.12f));e.arrows.push_back(line);}
  }
  ++e.diagnostics.rebuilds;e.diagnostics.cells=zero.size();e.diagnostics.lastBuildMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
  e.status=e.surface.hasCascade?"River ready | automatic smooth drop + transported whitewater":"River ready | graded flow";
 }
};
RiverCourseRenderer::RiverCourseRenderer(std::string d):impl_(std::make_unique<Impl>(
 d=="Content/river"?river_shader_assets::Directory():std::move(d))){}
RiverCourseRenderer::~RiverCourseRenderer(){Restore();}
void RiverCourseRenderer::Invalidate(){impl_->dirty=true;impl_->meshes.clear();}
void RiverCourseRenderer::SetSuspended(bool v){impl_->suspended=v;}
bool RiverCourseRenderer::Owns(wi::ecs::Entity e)const{return impl_->entries.count(e)!=0;}
size_t RiverCourseRenderer::ActiveCount()const{size_t count=0;for(const auto& p:impl_->entries)if(p.second->ready)++count;return count;}
const std::string& RiverCourseRenderer::Status(wi::ecs::Entity e)const{static const std::string none="Native water";auto it=impl_->entries.find(e);if(it==impl_->entries.end())return none;if(!it->second->geometry.Error().empty())return it->second->geometry.Error();return it->second->status;}
void RiverCourseRenderer::Prepare(wi::scene::Scene& scene,uint64_t revision,float dt,uint64_t editRevision){
 auto& s=*impl_;if(s.suspended)return;
 if(s.scene!=&scene||s.revision!=revision){if(!s.entries.empty())GetDevice()->WaitForGPU();s.entries.clear();s.meshes.clear();s.scene=&scene;s.revision=revision;s.dirty=true;}
 std::unordered_set<wi::ecs::Entity> seen;
 for(size_t k=0;k<scene.splines.GetCount();++k){auto root=scene.splines.GetEntity(k);auto* m=scene.materials.GetComponent(root);if(!m||m->shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER||!RiverCourseHasElevation(scene,root))continue;
  seen.insert(root);auto& holder=s.entries[root];if(!holder)holder=std::make_unique<Impl::Entry>();auto& e=*holder;e.root=root;e.steps=0;
  try{
   Require(seen.size()<=16,"At most 16 elevated river courses");auto id=PersistentEntityId(scene,root);Require(IsValidStableId(id),"River needs a stable identity");
   RiverFlowProfile p;std::string error;Require(ReadRiverFlowProfile(scene,root,p,error),error);
   auto hash=AuthorHash(scene,root,p);
   // Native spline regeneration can invalidate the temporary render binding
   // without changing the authored course. Repair only that binding: rebuilding
   // the transport here discards foam and restarts contact accumulation.
   if(!s.dirty&&e.ready&&e.id==id&&e.author==hash&&!e.geometry.Ready()){
    scene.Update(0);Require(e.geometry.Configure(scene,root,e.surface,error),error);
   }
   if(s.dirty||e.id!=id||e.author!=hash||(e.ready&&p.speed==0&&e.builtSpeed!=0)||(e.ready&&!e.geometry.Ready())||(!e.ready&&e.editRevision!=editRevision)){
    e.pending={};e.sourceFailed=false;e.id=id;e.author=hash;e.editRevision=editRevision;s.Rebuild(e,p,hash,editRevision);
   }else if(e.ready&&p.enabled&&p.speed>0){
    auto sources=s.Sources(e.bounds,editRevision,p);
    if(e.pending.valid()||e.sources!=sources)s.Obstacles(e,p,sources,hash,editRevision);
   }
   if(p.enabled!=e.profile.enabled||p.intensity!=e.profile.intensity||p.foam!=e.profile.foam||p.lifetime!=e.profile.lifetime||p.turbulence!=e.profile.turbulence||p.threshold!=e.profile.threshold||p.rapidIntensity!=e.profile.rapidIntensity||p.rapids!=e.profile.rapids){GetDevice()->WaitForGPU();s.Parameters(e,p);}
   if(e.ready&&e.enabled){float elapsed=std::isfinite(dt)?std::clamp(dt,0.f,.05f):0;e.steps=elapsed>0?unsigned(std::max(1.f,std::ceil(elapsed*e.maximumRate/.75f))):0;e.start=e.state;e.step=e.steps?elapsed/e.steps:0;e.clock+=elapsed;e.state^=e.steps&1;e.appearance.Prepare(elapsed);e.dispatched=false;}
  }catch(const std::exception& ex){e.ready=false;e.status=ex.what();}
 }
 for(auto it=s.entries.begin();it!=s.entries.end();)if(!seen.count(it->first)){GetDevice()->WaitForGPU();it=s.entries.erase(it);}else ++it;s.dirty=false;
}
void RiverCourseRenderer::Apply()const{
 auto& s=*impl_;if(!s.scene)return;
 for(auto& pair:s.entries){auto& e=*pair.second;if(!e.ready||!e.geometry.Apply())continue;auto* m=s.scene->materials.GetComponent(e.root);e.originalShader=m->customShaderID;e.originalUserdata=m->userdata;m->SetCustomShaderID(s.shader);m->userdata={SRV(e.parameters),0x52464c35,e.state|(e.appearance.State()<<1),Bits(e.clock)};m->SetDirty();e.bound=true;}
}
void RiverCourseRenderer::Restore()const{
 auto& s=*impl_;if(!s.scene)return;
 for(auto& pair:s.entries){auto& e=*pair.second;if(e.bound){if(auto* m=s.scene->materials.GetComponent(e.root)){m->SetCustomShaderID(e.originalShader);m->userdata=e.originalUserdata;m->SetDirty();}e.bound=false;}e.geometry.Restore();}
}
void RiverCourseRenderer::Dispatch()const{
 auto& s=*impl_;if(s.suspended)return;auto* gpu=GetDevice();
 for(auto& pair:s.entries){auto& e=*pair.second;if(!e.ready||e.dispatched||!e.enabled)continue;e.dispatched=true;auto cmd=gpu->BeginCommandList();wi::renderer::BindCommonResources(cmd);unsigned current=e.start;
  for(unsigned n=0;n<e.steps;++n){unsigned next=1-current;auto barrier=GPUBarrier::Image(&e.foam[next],ResourceState::SHADER_RESOURCE,ResourceState::UNORDERED_ACCESS);gpu->Barrier(&barrier,1,cmd);gpu->BindComputeShader(&s.simulation,cmd);
   gpu->BindResource(&e.foam[current],0,cmd);gpu->BindResource(&e.flux,1,cmd);gpu->BindResource(&e.source,2,cmd);gpu->BindResource(&e.velocity,3,cmd);gpu->BindUAV(&e.foam[next],0,cmd);
   const auto& d=e.foam[next].GetDesc();Push push{{d.width,d.height,15,0},e.params.bounds,{e.step,e.clock-(e.steps-1-n)*e.step,e.profile.lifetime,1}};gpu->PushConstants(&push,sizeof(push),cmd);gpu->Dispatch((d.width+7)/8,(d.height+7)/8,1,cmd);
   barrier=GPUBarrier::Image(&e.foam[next],ResourceState::UNORDERED_ACCESS,ResourceState::SHADER_RESOURCE);gpu->Barrier(&barrier,1,cmd);current=next;
  }e.appearance.Dispatch(e.source,e.velocity,e.foam[e.state],e.params.bounds,e.clock,15);
 }
}
void RiverCourseRenderer::DrawArrows(wi::ecs::Entity root)const{auto it=impl_->entries.find(root);if(it!=impl_->entries.end()&&it->second->ready)for(const auto& a:it->second->arrows)wi::renderer::DrawLine(a,false);}
bool RiverCourseRenderer::Diagnostics(wi::ecs::Entity root,RiverFlowDiagnostics& out,std::string& error,bool readback)const{
 auto it=impl_->entries.find(root);if(it==impl_->entries.end()||!it->second->ready){error=Status(root);return false;}const auto& e=*it->second;out=e.diagnostics;
 if(readback){GetDevice()->WaitForGPU();wi::vector<uint8_t> bytes;if(!wi::helper::saveTextureToMemory(e.foam[e.state],bytes)){error="Course density readback failed";return false;}
  for(size_t k=0;k<bytes.size();k+=sizeof(XMFLOAT4)){XMFLOAT4 q;std::memcpy(&q,bytes.data()+k,sizeof(q));if(!std::isfinite(q.x)||q.x<0){error="Invalid course foam state";return false;}out.foamMass+=q.x;out.maximumDensity=std::max(out.maximumDensity,q.x);}
  e.appearance.Readback(out.maximumDetail,out.detailSignature);
 }error.clear();return true;
}
}
