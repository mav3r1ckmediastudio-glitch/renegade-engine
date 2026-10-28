#include "renegade/bridge/StudioSession.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <fstream>
using namespace renegade::bridge;namespace fs=std::filesystem;
static void Check(bool v,const std::string& text){if(!v)throw std::runtime_error(text);}
int main(int argc,char** argv){
 if(argc!=2)return 2;auto project=fs::absolute(argv[1]);if(project.parent_path().filename()!="RenegadeCourseBankReview_20261025")return 2;
 HWND window=CreateWindowExW(0,L"STATIC",L"Course disposable asset closure",WS_OVERLAPPEDWINDOW,0,0,128,128,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 try{
 wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);StudioSession session;struct Drain{~Drain(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();}}drain;session.Projects().Initialize("course-fixture-recents.ini");
 bool ok=session.Projects().OpenProject(project.generic_string());Check(ok,session.Projects().LastError());ok=session.Projects().CommitPendingProject();Check(ok,session.Projects().LastError());
 auto path=project.parent_path()/"Content/Scenes/River.wiscene";ok=session.Documents().Open(path.generic_string());Check(ok,session.Scenes().LastError());auto& scene=session.Scenes().GetScene();std::map<std::string,std::string> copied;
 auto adopt=[&](wi::scene::MaterialComponent& material){for(auto& slot:material.textures){
  if(slot.name.empty())continue;auto found=copied.find(slot.name);if(found!=copied.end()){slot.name=found->second;continue;}
  wi::vector<uint8_t> data;if(slot.resource.IsValid())data=slot.resource.GetFileData();if(data.empty())wi::helper::FileRead(slot.name,data);
  Check(!data.empty(),"Missing owned fixture texture: "+slot.name);
  auto destination=project.parent_path()/"Content/Textures"/(std::to_string(copied.size())+"_"+fs::u8path(slot.name).filename().string());fs::create_directories(destination.parent_path());
  std::ofstream output(destination,std::ios::binary);output.write(reinterpret_cast<const char*>(data.data()),data.size());output.close();Check(bool(output),"Fixture texture write failed");
  auto original=slot.name;slot.name=destination.generic_string();copied.emplace(original,slot.name);std::cout<<"COURSE_FIXTURE_TEXTURE "<<slot.name<<" bytes="<<data.size()<<std::endl;
 }material.SetDirty();};
 for(size_t k=0;k<scene.materials.GetCount();++k)adopt(scene.materials[k]);for(size_t k=0;k<scene.terrains.GetCount();++k)adopt(scene.terrains[k].grass_material);
 ok=session.Documents().Save(path.generic_string());Check(ok,session.Scenes().LastError());std::cout<<"COURSE_FIXTURE_PORTABLE_TEXTURES_PASS count="<<copied.size()<<std::endl;return 0;
 }catch(const std::exception& e){std::cerr<<"COURSE_FIXTURE_TEXTURE_FAIL "<<e.what()<<std::endl;return 1;}
}
