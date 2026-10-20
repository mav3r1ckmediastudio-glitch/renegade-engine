#pragma once
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>
namespace renegade::bridge {
struct ScreenGridSettings {
 int spacing=16; bool grid=true,snap=true;
 bool Valid()const{return spacing>=4&&spacing<=128;}
};
// Project authoring metadata; never consumed by Runtime or Screen geometry.
struct ScreenGridPreferences {
 ScreenGridSettings defaults;
 std::map<StableId,ScreenGridSettings> screens;
 ScreenGridSettings Resolve(const StableId& id)const{auto i=screens.find(id);return i==screens.end()?defaults:i->second;}
 static bool Parse(const std::string& bytes,const StableId& project,ScreenGridPreferences& out,std::string& error){
  if(bytes.size()>131072){error="Grid settings are oversized.";return false;}
  std::istringstream in(bytes);std::string version,identity,key;ScreenGridPreferences c;
  if(!(in>>version>>identity)||version!="renegade-ui-grid-v1"||identity!=project){error="Foreign or unsupported grid settings.";return false;}
  bool first=true;while(in>>key){int grid,snap;ScreenGridSettings s;
   if(!(in>>s.spacing>>grid>>snap)||!s.Valid()||(grid!=0&&grid!=1)||(snap!=0&&snap!=1)){error="Invalid grid settings.";return false;}
   s.grid=grid!=0;s.snap=snap!=0;
   if(first){if(key!="default"){error="Missing grid defaults.";return false;}c.defaults=s;first=false;}
   else if(!IsValidStableId(key)||!c.screens.emplace(key,s).second){error="Invalid duplicate Screen grid identity.";return false;}
  }
  if(first){error="Missing grid defaults.";return false;}out=std::move(c);error.clear();return true;
 }
 static bool ResolvePath(const std::string& root,const StableId& project,std::filesystem::path& path,std::string& error){
  namespace fs=std::filesystem;std::error_code ec;
  if(root.empty()||!IsValidStableId(project)){error="Grid settings require an active project.";return false;}
  const auto canonical=fs::canonical(fs::u8path(root),ec);
  if(ec||!fs::is_directory(canonical)){error="Grid project root is unavailable.";return false;}
  path=fs::weakly_canonical(canonical/"Config/ScreenAuthoring.grid",ec);const auto relative=path.lexically_relative(canonical);
  if(ec||relative.empty()||relative.is_absolute()||std::any_of(relative.begin(),relative.end(),[](const auto& p){return p=="..";})){
   error="Grid settings escape the project root.";return false;}
  return true;
 }
 static bool Load(const std::string& root,const StableId& project,ScreenGridPreferences& out,std::string& error){
  std::filesystem::path path;if(!ResolvePath(root,project,path,error))return false;std::error_code ec;
  if(!std::filesystem::exists(path,ec)&&!ec){out={};error.clear();return true;}
  if(ec||std::filesystem::file_size(path,ec)>131072||ec){error="Cannot read grid settings.";return false;}
  std::ifstream in(path,std::ios::binary);if(!in){error="Cannot read grid settings.";return false;}
  return Parse(std::string{std::istreambuf_iterator<char>(in),{}},project,out,error);
 }
 bool Save(const std::string& root,const StableId& project,std::string& error,ProjectDocumentTransactionHook hook={})const{
  if(!IsValidStableId(project)||!defaults.Valid()){error="Invalid grid project/defaults.";return false;}
  std::ostringstream out;out<<"renegade-ui-grid-v1\n"<<project<<"\n";
  auto row=[&](const std::string& key,const ScreenGridSettings& s){out<<key<<' '<<s.spacing<<' '<<int(s.grid)<<' '<<int(s.snap)<<'\n';};
  row("default",defaults);for(const auto& p:screens){if(!IsValidStableId(p.first)||!p.second.Valid()){error="Invalid Screen grid override.";return false;}row(p.first,p.second);}
  std::filesystem::path path;if(!ResolvePath(root,project,path,error))return false;
  const auto bytes=out.str();ProjectDocumentWrite w;w.destinationPath=path.generic_u8string();w.content.assign(bytes.begin(),bytes.end());
  w.validator=[project](const std::string& path,std::string& e){std::ifstream in(std::filesystem::u8path(path),std::ios::binary);ScreenGridPreferences v;return in&&Parse(std::string{std::istreambuf_iterator<char>(in),{}},project,v,e);};
  ProjectDocumentTransactionOptions options;options.allowedRoot=root;options.journalDirectory=(std::filesystem::u8path(root)/"Intermediate/Transactions").generic_u8string();options.operationHook=std::move(hook);
  const auto result=ProjectDocumentTransaction{}.Execute({std::move(w)},std::move(options));if(!result.success){error=result.message;return false;}error.clear();return true;
 }
};
}
