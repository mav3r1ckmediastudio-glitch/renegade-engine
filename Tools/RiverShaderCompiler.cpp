#include <WickedEngine.h>
#include <filesystem>
#include <iostream>
int main(int argc,char** argv)
{
    // Optional diagnostic PS compilation; the normal production path stays CS.
    if(argc!=4 && argc!=5)return 2;
    const bool proofVertex = argc==5 && std::string(argv[4])=="vs";
    const bool proofPixel = argc==5 && std::string(argv[4])=="ps";
    if(argc==5 && !proofPixel && !proofVertex)return 2;
    wi::shadercompiler::CompilerInput input;
    input.stage=proofVertex ? wi::graphics::ShaderStage::VS : proofPixel ? wi::graphics::ShaderStage::PS : wi::graphics::ShaderStage::CS;
    input.format=wi::graphics::ShaderFormat::HLSL6;
    input.minshadermodel=(proofPixel||proofVertex) ? wi::graphics::ShaderModel::SM_6_0 : wi::graphics::ShaderModel::SM_6_6;
    input.shadersourcefilename=argv[1];input.include_directories.push_back(std::string(argv[2])+"/");
    wi::shadercompiler::CompilerOutput output;wi::shadercompiler::Compile(input,output);
    if(!output.IsValid()){std::cerr<<output.error_message<<'\n';return 1;}
    std::filesystem::create_directories(std::filesystem::u8path(argv[3]).parent_path());
    if(!wi::helper::FileWrite(argv[3],output.shaderdata,output.shadersize))return 1;
    std::cout<<"RIVER_UNDERWATER_SHADER_COMPILED bytes="<<output.shadersize<<'\n';return 0;
}
