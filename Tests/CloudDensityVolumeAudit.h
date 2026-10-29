#pragma once
#include "wiShaderCompiler.h"
#include <cstring>
#include "../Tools/night_sky/renegadeCloudRendererBindings.h"
// Explicit developer proof only. Uses the actual shared camera/shadow density
// function and actual native GPU noise resources, at fixed LOD 0.
inline bool SaveCloudDensityVolumeAudit(const std::filesystem::path& output,
    wi::scene::CameraComponent& camera, const wi::scene::WeatherComponent& weather)
{
    namespace fs=std::filesystem;
    using namespace wi::graphics;
    auto* device=GetDevice();
    const fs::path root=fs::path(__FILE__).parent_path().parent_path();
    wi::shadercompiler::CompilerInput input;
    input.format=device->GetShaderFormat();input.stage=ShaderStage::CS;
    input.minshadermodel=ShaderModel::SM_6_0;
    input.shadersourcefilename=(root/"Tools/night_sky/CloudDensityVolumeAuditCS.hlsl").string();
    input.include_directories.push_back((root/"WickedEngine/WickedEngine/shaders").string()+"/");
    wi::shadercompiler::CompilerOutput compiled;
    wi::shadercompiler::Compile(input,compiled);
    if(!compiled.IsValid()) {std::cerr<<compiled.error_message<<std::endl;return false;}
    Shader extraction;
    if(!device->CreateShader(ShaderStage::CS,compiled.shaderdata,compiled.shadersize,&extraction)) return false;
    fs::create_directories(output);
    if(!wi::shadercompiler::SaveShaderAndMetadata((output/"density-audit.cso").string(),compiled)) return false;
    Texture shape,detail,curl,weatherMap,density,parameters;
    const auto generate=[&](Texture& texture,int w,int h,int d,const char* name,int group)
    {
        TextureDesc desc;
        desc.type=d>1 ? TextureDesc::Type::TEXTURE_3D : TextureDesc::Type::TEXTURE_2D;
        desc.width=w;desc.height=h;desc.depth=d;desc.mip_levels=1;
        desc.format=Format::R8G8B8A8_UNORM;
        desc.bind_flags=BindFlag::SHADER_RESOURCE|BindFlag::UNORDERED_ACCESS;
        desc.layout=ResourceState::SHADER_RESOURCE_COMPUTE;
        if(!device->CreateTexture(&desc,nullptr,&texture)) return false;
        Shader shader;
        if(!wi::renderer::LoadShader(ShaderStage::CS,shader,name)) return false;
        auto cmd=device->BeginCommandList();
        wi::renderer::BindCommonResources(cmd);
        wi::renderer::BindCameraCB(camera,camera,camera,cmd);
        auto barrier=GPUBarrier::Image(&texture,desc.layout,ResourceState::UNORDERED_ACCESS);
        device->Barrier(&barrier,1,cmd);device->BindComputeShader(&shader,cmd);
        device->BindUAV(&texture,0,cmd);
        device->Dispatch((w+group-1)/group,(h+group-1)/group,d>1 ? (d+group-1)/group : 1,cmd);
        barrier=GPUBarrier::Image(&texture,ResourceState::UNORDERED_ACCESS,desc.layout);
        device->Barrier(&barrier,1,cmd);device->SubmitCommandLists();device->WaitForGPU();
        return true;
    };
    if(!generate(shape,64,64,64,"volumetricCloud_shapenoiseCS.cso",8) ||
       !generate(detail,32,32,32,"volumetricCloud_detailnoiseCS.cso",8) ||
       !generate(curl,128,128,1,"volumetricCloud_curlnoiseCS.cso",16) ||
       !generate(weatherMap,1024,1024,1,"volumetricCloud_weathermapCS.cso",16)) return false;
    TextureDesc desc;
    desc.type=TextureDesc::Type::TEXTURE_3D;
    desc.width=128;desc.height=128;desc.depth=128;desc.mip_levels=1;
    desc.format=Format::R32_FLOAT;
    desc.bind_flags=BindFlag::SHADER_RESOURCE|BindFlag::UNORDERED_ACCESS;
    desc.layout=ResourceState::SHADER_RESOURCE_COMPUTE;
    if(!device->CreateTexture(&desc,nullptr,&density)) return false;
    TextureDesc parametersDesc;
    parametersDesc.width=2;parametersDesc.height=1;parametersDesc.format=Format::R32G32B32A32_FLOAT;
    parametersDesc.bind_flags=BindFlag::SHADER_RESOURCE|BindFlag::UNORDERED_ACCESS;
    parametersDesc.layout=ResourceState::SHADER_RESOURCE_COMPUTE;
    if(!device->CreateTexture(&parametersDesc,nullptr,&parameters)) return false;
    auto cmd=device->BeginCommandList();
    wi::renderer::BindCommonResources(cmd);
    wi::renderer::BindCameraCB(camera,camera,camera,cmd);
    GPUBarrier barriers[]={
        GPUBarrier::Image(&density,desc.layout,ResourceState::UNORDERED_ACCESS),
        GPUBarrier::Image(&parameters,parametersDesc.layout,ResourceState::UNORDERED_ACCESS)};
    device->Barrier(barriers,2,cmd);
    wi::renderer::BindRenegadeCloudFormationResources(cmd);
    device->BindComputeShader(&extraction,cmd);
    device->BindResource(&shape,0,cmd);device->BindResource(&detail,1,cmd);
    device->BindResource(&curl,2,cmd);device->BindResource(&weatherMap,3,cmd);
    device->BindUAV(&density,0,cmd);device->BindUAV(&parameters,1,cmd);device->Dispatch(32,32,32,cmd);
    barriers[0]=GPUBarrier::Image(&density,ResourceState::UNORDERED_ACCESS,desc.layout);
    barriers[1]=GPUBarrier::Image(&parameters,ResourceState::UNORDERED_ACCESS,parametersDesc.layout);
    device->Barrier(barriers,2,cmd);device->SubmitCommandLists();device->WaitForGPU();
    wi::vector<uint8_t> bytes;
    if(!wi::helper::saveTextureToMemory(density,bytes) || bytes.size()!=128ull*128*128*4) return false;
    std::ofstream raw(output/"density-r32f-128x128x128.bin",std::ios::binary);
    raw.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());raw.close();
    wi::vector<uint8_t> gpuParameters;
    if(!wi::helper::saveTextureToMemory(parameters,gpuParameters)||gpuParameters.size()!=8*sizeof(float))return false;
    float values[8];std::memcpy(values,gpuParameters.data(),sizeof(values));
    std::ofstream manifest(output/"volume.json");
    manifest<<"{\"dimensions\":[128,128,128],\"x_metres\":[-6000,6000],"
        "\"z_metres\":[-6000,6000],\"layout\":\"x-fastest,y,z; little-endian float32\","
        "\"density_lod\":0,\"y_metres\":["
        <<weather.volumetricCloudParameters.cloudStartHeight<<","
        <<weather.volumetricCloudParameters.cloudStartHeight+weather.volumetricCloudParameters.cloudThickness
        <<"],\"coverage\":"<<weather.volumetricCloudParameters.layerFirst.coverageAmount
        <<",\"formation\":"<<weather.volumetricCloudParameters.layerFirst.padding0
        <<",\"gpu_wind_offset\":["<<values[0]<<","<<values[1]<<","<<values[2]<<"]"
        <<",\"gpu_time\":"<<values[3]<<",\"wind_up_amount\":"<<values[7]
        <<",\"scope\":\"actual primary shared density, no lighting or reconstruction\"}";
    return bool(raw)&&bool(manifest);
}
