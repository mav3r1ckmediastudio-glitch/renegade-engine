#include "RuntimeGameSaveSlot.h"
#include "renegade/bridge/IdentityService.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <system_error>

namespace renegade::runtime {
namespace {
namespace fs=std::filesystem;
constexpr std::size_t MaxInventory=16u*1024u*1024u;
constexpr std::size_t MaxWorld=4u*1024u*1024u;
constexpr std::size_t MaxFile=MaxInventory+MaxWorld+1024u;

std::uint64_t Checksum(const std::string& bytes) {
    std::uint64_t hash=14695981039346656037ull;
    for(const unsigned char ch:bytes) {
        hash^=ch;
        hash*=1099511628211ull;
    }
    return hash;
}
bool ResolveSlot(const std::string& localAppData,const std::string& saveDataId,
                 fs::path& path,std::string& error) {
    if (!bridge::IsValidStableId(saveDataId) || localAppData.empty()) {
        error="Missing local user save directory or invalid packaged save identity.";
        return false;
    }
    std::error_code ec;
    const auto base=fs::weakly_canonical(fs::u8path(localAppData),ec);
    if(ec || !fs::is_directory(base)) {
        error="Player save-data directory is unavailable.";
        return false;
    }
    const auto target=base/"RenegadeEngine"/fs::u8path(saveDataId)/"Saves";
    fs::create_directories(target,ec);
    if(ec) {
        error="Could not create player save directory: "+ec.message();
        return false;
    }
    const auto canonical=fs::weakly_canonical(target,ec);
    if(ec) {error="Could not resolve player save directory.";return false;}
    const auto relative=fs::relative(canonical,base,ec);
    if(ec || relative.empty() || relative.is_absolute()) {
        error="Player save path escapes the local user-data directory.";
        return false;
    }
    for(const auto& part:relative)
        if(part=="..") {error="Player save path escapes the local user-data directory.";return false;}
    path=canonical/"quick.rsave";
    // A missing first-save slot is normal. On Windows is_symlink(path,ec)
    // alone reports ERROR_FILE_NOT_FOUND, which must not reject first save.
    const bool existing=fs::exists(path,ec);
    if(ec || (existing && fs::is_symlink(path,ec)) || ec) {
        error="Player save slot cannot be a symlink.";
        return false;
    }
    error.clear();
    return true;
}
bool Encode(const RuntimeGameSaveState& state,std::string& bytes,std::string& error) {
    if(!ValidateRuntimeGameSaveState(state,error))return false;
    // Encode the two existing authoritative gameplay snapshots, not duplicate
    // copies of inventory item definitions or world object meshes.
    std::ostringstream payload;
    payload<<std::setprecision(std::numeric_limits<float>::max_digits10);
    payload<<"RENEGADE_GAME_SAVE 1 "<<state.projectId<<' '<<state.sceneId<<' '
           <<state.playerHealth<<' '<<state.playerMaxHealth<<' '
           <<static_cast<unsigned>(state.playerDead)<<' '
           <<state.playerX<<' '<<state.playerY<<' '<<state.playerZ<<' '
           <<state.playerYaw<<' '<<state.playerPitch<<' '
           <<state.inventoryBytes.size()<<' '<<state.retiredWorldBytes.size()<<'\n';
    const std::string prefix=payload.str();
    const auto contents=prefix+state.inventoryBytes+state.retiredWorldBytes;
    std::ostringstream output;
    output<<std::hex<<std::setw(16)<<std::setfill('0')<<Checksum(contents)<<'\n';
    bytes=output.str()+contents;
    error.clear();
    return true;
}
bool Decode(const std::string& bytes,RuntimeGameSaveState& state,std::string& error) {
    if(bytes.empty() || bytes.size()>MaxFile) {
        error="Quick save is empty or exceeds its bounded format.";
        return false;
    }
    const auto checksumEnd=bytes.find('\n');
    if(checksumEnd!=16 || checksumEnd+1>=bytes.size()) {
        error="Quick save checksum header is missing.";
        return false;
    }
    const auto expected=bytes.substr(0,16);
    if(!std::all_of(expected.begin(),expected.end(),[](char c){
            return (c>='0' && c<='9') || (c>='a' && c<='f');})) {
        error="Quick save checksum header is malformed.";
        return false;
    }
    std::ostringstream actual;
    actual<<std::hex<<std::setw(16)<<std::setfill('0')
          <<Checksum(bytes.substr(checksumEnd+1));
    if(actual.str()!=expected) {
        error="Quick save failed its corruption check.";
        return false;
    }
    const auto headerStart=checksumEnd+1;
    const auto headerEnd=bytes.find('\n',headerStart);
    if(headerEnd==std::string::npos || headerEnd-headerStart>512) {
        error="Quick save header is missing or oversized.";
        return false;
    }
    std::istringstream input(bytes.substr(headerStart,headerEnd-headerStart));
    std::string magic,extra;
    unsigned version=0,dead=0;
    std::size_t inventorySize=0,worldSize=0;
    RuntimeGameSaveState parsed;
    if(!(input>>magic>>version>>parsed.projectId>>parsed.sceneId>>
            parsed.playerHealth>>parsed.playerMaxHealth>>dead>>
            parsed.playerX>>parsed.playerY>>parsed.playerZ>>
            parsed.playerYaw>>parsed.playerPitch>>inventorySize>>worldSize) ||
        (input>>extra) || magic!="RENEGADE_GAME_SAVE" || version!=1 ||
        dead>1 || inventorySize==0 || inventorySize>MaxInventory ||
        worldSize==0 || worldSize>MaxWorld ||
        inventorySize+worldSize!=bytes.size()-headerEnd-1) {
        error="Quick save header or payload lengths are invalid.";
        return false;
    }
    parsed.playerDead=dead==1;
    parsed.inventoryBytes=bytes.substr(headerEnd+1,inventorySize);
    parsed.retiredWorldBytes=bytes.substr(headerEnd+1+inventorySize,worldSize);
    if(!ValidateRuntimeGameSaveState(parsed,error))return false;
    state=std::move(parsed);
    error.clear();
    return true;
}
} // namespace

bool ValidateRuntimeGameSaveState(const RuntimeGameSaveState& state,
                                  std::string& error) {
    if(!bridge::IsValidStableId(state.projectId) ||
       !bridge::IsValidStableId(state.sceneId) ||
       state.inventoryBytes.empty() || state.inventoryBytes.size()>MaxInventory ||
       state.retiredWorldBytes.empty() || state.retiredWorldBytes.size()>MaxWorld ||
       !std::isfinite(state.playerHealth) ||
       !std::isfinite(state.playerMaxHealth) ||
       state.playerMaxHealth<=0 || state.playerMaxHealth>1000000.f ||
       state.playerHealth<0 || state.playerHealth>state.playerMaxHealth ||
       state.playerDead!=(state.playerHealth<=0) ||
       !std::isfinite(state.playerX) || !std::isfinite(state.playerY) ||
       !std::isfinite(state.playerZ) || !std::isfinite(state.playerYaw) ||
       !std::isfinite(state.playerPitch) ||
       std::fabs(state.playerX)>10000000.f ||
       std::fabs(state.playerY)>10000000.f ||
       std::fabs(state.playerZ)>10000000.f ||
       std::fabs(state.playerPitch)>3.141593f) {
        error="Quick save has invalid identities, gameplay payloads or health.";
        return false;
    }
    error.clear();
    return true;
}
bool RuntimeGameSaveSlot::WriteQuick(const std::string& localAppData,
        const std::string& saveDataId,const RuntimeGameSaveState& state,
        std::string& error) {
    fs::path destination;
    if(!ResolveSlot(localAppData,saveDataId,destination,error))return false;
    std::string bytes;
    if(!Encode(state,bytes,error))return false;
    const auto temporary=fs::u8path(destination.generic_u8string()+".pending-"+
                                   bridge::GenerateStableId());
    HANDLE file=CreateFileW(temporary.wstring().c_str(),GENERIC_WRITE,0,nullptr,
                            CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) {
        error="Could not create pending quick save.";
        return false;
    }
    std::size_t offset=0;
    bool okay=true;
    while(offset<bytes.size()) {
        DWORD written=0;
        const auto length=static_cast<DWORD>(std::min<std::size_t>(
            bytes.size()-offset,1024u*1024u));
        if(!WriteFile(file,bytes.data()+offset,length,&written,nullptr) ||
           written==0) {okay=false;break;}
        offset+=written;
    }
    if(okay && !FlushFileBuffers(file))okay=false;
    CloseHandle(file);
    if(!okay || !MoveFileExW(temporary.wstring().c_str(),
           destination.wstring().c_str(),
           MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        std::error_code ec;
        fs::remove(temporary,ec);
        error="Quick save could not be committed; existing slot is unchanged.";
        return false;
    }
    error.clear();
    return true;
}
bool RuntimeGameSaveSlot::ReadQuick(const std::string& localAppData,
        const std::string& saveDataId,
        const std::string& expectedProjectId,const std::string& expectedSceneId,
        RuntimeGameSaveState& state,std::string& error) {
    fs::path destination;
    if(!ResolveSlot(localAppData,saveDataId,destination,error))return false;
    std::error_code ec;
    if(!fs::is_regular_file(destination,ec) || ec ||
       fs::file_size(destination,ec)>MaxFile || ec) {
        error="No readable quick save exists for this game.";
        return false;
    }
    std::ifstream file(destination,std::ios::binary);
    if(!file) {error="Quick save could not be opened.";return false;}
    const std::string bytes((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
    if(!file.eof() && file.fail()) {
        error="Quick save could not be read in full.";
        return false;
    }
    RuntimeGameSaveState parsed;
    if(!Decode(bytes,parsed,error))return false;
    if(parsed.projectId!=expectedProjectId || parsed.sceneId!=expectedSceneId) {
        error="Quick save belongs to a different project or level.";
        return false;
    }
    state=std::move(parsed);
    error.clear();
    return true;
}
}
