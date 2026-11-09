#include <WickedEngine.h>
#include <filesystem>
#include <iostream>
int main(int argc,char** argv){
 if(argc!=4)return 2;
 for(int variant=0;variant<7;++variant){
  wi::shadercompiler::CompilerInput input;input.stage=wi::graphics::ShaderStage::CS;
  input.format=wi::graphics::ShaderFormat::HLSL6;input.minshadermodel=wi::graphics::ShaderModel::SM_6_6;
  input.shadersourcefilename=std::string(argv[1])+(variant<4?"/emittedparticle_simulateCS.hlsl":"/emittedparticle_emitCS.hlsl");
  input.include_directories.push_back(std::string(argv[1])+"/");input.include_directories.push_back(std::string(argv[2])+"/");
  if(variant<4){input.defines.push_back("RENEGADE_MOVEMENT_FIELDS");if(variant&1)input.defines.push_back("SORTING");if(variant&2)input.defines.push_back("DEPTHCOLLISIONS");}else{input.defines.push_back("RENEGADE_PARTICLE_VARIATION");if(variant==5)input.defines.push_back("EMITTER_VOLUME");if(variant==6)input.defines.push_back("EMIT_FROM_MESH");}
  wi::shadercompiler::CompilerOutput out;wi::shadercompiler::Compile(input,out);if(!out.IsValid()){std::cerr<<out.error_message;return 1;}
  auto path=std::filesystem::u8path(argv[3])/("MovementField"+std::to_string(variant)+".cso");std::filesystem::create_directories(path.parent_path());
  if(!wi::helper::FileWrite(path.string(),out.shaderdata,out.shadersize))return 1;
  std::cout<<"FIELD_SHADER "<<variant<<" bytes="<<out.shadersize<<'\n';
 }
}
