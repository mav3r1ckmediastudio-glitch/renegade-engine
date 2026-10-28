#include "renegade/bridge/StudioSession.h"
#include "WindowsGameBuildController.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <map>
int main(int argc,char** argv){
 if(argc!=2&&argc!=3)return 2;std::cout.setf(std::ios::unitbuf);
 HWND window=CreateWindowExW(0,L"STATIC",L"River G6 build verification",WS_OVERLAPPEDWINDOW,0,0,128,128,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 try{
  wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
  renegade::bridge::StudioSession session;session.Projects().Initialize("package-proof-recents.ini");
  if(!session.Projects().OpenProject(std::filesystem::absolute(argv[1]).generic_u8string()))throw std::runtime_error(session.Projects().LastError());
  if(!session.Projects().CommitPendingProject())throw std::runtime_error(session.Projects().LastError());
  if(argc==3){
   // Explicit fixture-only preparation. Never called by production Studio.
   namespace fs=std::filesystem;
   auto root=fs::absolute(argv[1]).parent_path();
   if(std::string(argv[2])!="--adopt-fixture"||root.parent_path().filename()!="g6-export-input")
    throw std::runtime_error("Fixture adoption restricted to G6 export copy");
   auto scenePath=root/"Content/Scenes/River.wiscene";
   if(!session.Documents().Open(scenePath.generic_u8string()))throw std::runtime_error(session.Scenes().LastError());
   auto& scene=session.Scenes().GetScene();std::map<std::string,std::string> adopted;
   auto adoptMaterial=[&](wi::scene::MaterialComponent& material){
    for(auto& texture:material.textures){
     if(texture.name.empty())continue;
     auto found=adopted.find(texture.name);
     if(found!=adopted.end()){texture.name=found->second;continue;}
     wi::vector<uint8_t> data;
     if(texture.resource.IsValid())data=texture.resource.GetFileData();
     if(data.empty()){
      std::ifstream input(fs::u8path(texture.name),std::ios::binary);
      data.assign(std::istreambuf_iterator<char>(input),{});
     }
     if(data.empty()&&fs::u8path(texture.name).filename()=="waterNormalMap.jpg"){
      // The relocated proof archive retains its old proof-project path.
      // Resolve only this known native fixture texture from the accepted G5 content.
      auto native=fs::u8path("C:/Users/paulw/source/repos/renegade-spline-water-20261020/BUILD/foam-repair-20261023/Content/river/waterNormalMap.jpg");
      std::ifstream input(native,std::ios::binary);data.assign(std::istreambuf_iterator<char>(input),{});
      std::cout<<"G6_FIXTURE_NORMAL_SOURCE="<<native.generic_u8string()<<"\n";
     }
     if(data.empty())throw std::runtime_error("Missing fixture texture payload: "+texture.name);
     auto destination=root/"Content/Textures"/(std::to_string(adopted.size())+"_"+fs::u8path(texture.name).filename().u8string());
     fs::create_directories(destination.parent_path());
     std::ofstream output(destination,std::ios::binary);output.write(reinterpret_cast<const char*>(data.data()),data.size());output.close();
     if(!output)throw std::runtime_error("Fixture texture write failed");
     std::cout<<"G6_ADOPT_TEXTURE="<<texture.name<<" -> "<<destination.generic_u8string()<<" bytes="<<data.size()<<"\n";
     auto original=texture.name;texture.name=destination.generic_u8string();adopted.emplace(original,texture.name);
    }
   };
   for(size_t n=0;n<scene.materials.GetCount();++n)adoptMaterial(scene.materials[n]);
   for(size_t n=0;n<scene.terrains.GetCount();++n)adoptMaterial(scene.terrains[n].grass_material);
   if(!session.Documents().Save(scenePath.generic_u8string()))throw std::runtime_error(session.Scenes().LastError());
   std::cout<<"G6_FIXTURE_ADOPTED="<<adopted.size()<<"\n";
  }
  auto result=renegade::studio::BuildActiveWindowsGame();
  std::cout<<"G6_BUILD_GAME_SUCCESS="<<result.succeeded<<"\n"<<result.message<<"\nG6_PACKAGE="<<result.finalOutputPath<<"\n";
  return result.succeeded?0:1;
 }catch(const std::exception& e){std::cerr<<"G6_PACKAGE_FAIL="<<e.what()<<"\n";return 1;}
}
