#pragma once
// Shared transient G8H appearance for Studio and Runtime. Physical foam is external.
#include "WickedEngine.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include <stdexcept>
namespace renegade::bridge {
class RiverFoamAppearanceGpu {
 using Texture=wi::graphics::Texture;
 wi::graphics::Shader shader_;
 wi::Resource artwork_;
 Texture detail_[2],forward_;
 unsigned state_=0,start_=0,steps_=0;float step_=0;
 static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
 static unsigned Descriptor(const wi::graphics::GPUResource& r){
  int n=wi::graphics::GetDevice()->GetDescriptorIndex(&r,wi::graphics::SubresourceType::SRV);
  Check(n>=0,"Foam appearance descriptor");return unsigned(n);
 }
public:
 Texture waves;
 void Configure(const std::string& directory,unsigned width,unsigned height){
  using namespace wi::graphics;auto* gpu=GetDevice();wi::vector<uint8_t> bytes;
  if(!shader_.IsValid()){
   Check(wi::helper::FileRead(directory+"/RiverFoamDetailCS.cso",bytes),"Missing foam appearance compute shader");
   Check(gpu->CreateShader(ShaderStage::CS,bytes.data(),bytes.size(),&shader_),"Foam appearance compute creation");
  }
  artwork_=wi::resourcemanager::Load(directory+"/OriginalFoamCoverage.png");
  Check(artwork_.IsValid()&&artwork_.GetTexture().IsValid(),"Missing original foam artwork");
  TextureDesc td;td.width=std::min(1024u,width*2);td.height=std::min(512u,height*2);
  td.format=Format::R32_FLOAT;td.layout=ResourceState::SHADER_RESOURCE;td.bind_flags=BindFlag::SHADER_RESOURCE|BindFlag::UNORDERED_ACCESS;
  std::vector<float> blank(size_t(td.width)*td.height);
  SubresourceData init;init.data_ptr=blank.data();init.row_pitch=td.width*sizeof(float);init.slice_pitch=init.row_pitch*td.height;
  for(auto* texture:{&detail_[0],&detail_[1],&forward_})Check(gpu->CreateTexture(&td,&init,texture),"Foam appearance texture");
  state_=start_=steps_=0;step_=0;
 }
 DirectX::XMUINT4 DetailControls()const{return {Descriptor(detail_[0]),Descriptor(detail_[1]),0,0};}
 DirectX::XMUINT4 ArtworkControls()const{
  float tile=4;unsigned bits;std::memcpy(&bits,&tile,4);return {Descriptor(artwork_.GetTexture()),bits,0,0};
 }
 unsigned State()const{return state_;}
 void Readback(float& maximum,uint64_t& signature)const{
  wi::graphics::GetDevice()->WaitForGPU();wi::vector<uint8_t> bytes;
  Check(wi::helper::saveTextureToMemory(detail_[state_],bytes),"Foam detail readback");
  Check(bytes.size()==size_t(detail_[state_].GetDesc().width)*detail_[state_].GetDesc().height*sizeof(float),"Foam detail readback size");
  maximum=0;signature=1469598103934665603ull;
  for(size_t k=0;k<bytes.size();++k){signature^=bytes[k];signature*=1099511628211ull;}
  for(size_t k=0;k<bytes.size();k+=sizeof(float)){float value;std::memcpy(&value,bytes.data()+k,sizeof(value));
   Check(std::isfinite(value)&&value>=0&&value<=1,"Invalid bounded foam appearance state");maximum=std::max(maximum,value);}
 }

 void Prepare(float elapsed){
  start_=state_;steps_=elapsed>0?unsigned(std::ceil(elapsed/.025f)):0;
  step_=steps_?elapsed/steps_:0;state_^=steps_&1;
 }
 void Dispatch(const Texture& source,const Texture& velocity,const Texture& bulk,
     DirectX::XMFLOAT4 bounds,float clock,unsigned sourceMask)const{
  if(!steps_)return;using namespace wi::graphics;auto* gpu=GetDevice();auto cmd=gpu->BeginCommandList();
  wi::renderer::BindCommonResources(cmd);gpu->BindComputeShader(&shader_,cmd);
  const auto& td=detail_[0].GetDesc();unsigned current=start_;
  struct Push{DirectX::XMUINT4 resolution;DirectX::XMFLOAT4 bounds,settings;};
  for(unsigned n=0;n<steps_;++n){
   gpu->BindResource(&detail_[current],0,cmd);gpu->BindResource(&detail_[current],1,cmd);
   gpu->BindResource(&source,2,cmd);gpu->BindResource(&velocity,3,cmd);gpu->BindResource(&bulk,4,cmd);gpu->BindResource(&artwork_.GetTexture(),5,cmd);
   for(unsigned pass=0;pass<2;++pass){
    auto* target=pass?&detail_[1-current]:&forward_;
    auto barrier=GPUBarrier::Image(target,ResourceState::SHADER_RESOURCE,ResourceState::UNORDERED_ACCESS);gpu->Barrier(&barrier,1,cmd);
    gpu->BindUAV(target,0,cmd);Push push{{td.width,td.height,pass,sourceMask},bounds,{step_,clock-(steps_-1-n)*step_,6,1}};
    gpu->PushConstants(&push,sizeof(push),cmd);gpu->Dispatch((td.width+7)/8,(td.height+7)/8,1,cmd);
    barrier=GPUBarrier::Image(target,ResourceState::UNORDERED_ACCESS,ResourceState::SHADER_RESOURCE);gpu->Barrier(&barrier,1,cmd);
    if(pass==0)gpu->BindResource(&forward_,1,cmd);
   }current^=1;
  }
 }
};
}
