// Manual Windows GPU proof: snapshot and stage the fixed no-imported-arms scene.
// Uses production snapshot/staging services; does not claim Studio UI acceptance.
#include "renegade/bridge/BuildStageService.h"
#include "renegade/bridge/BuildIdentityService.h"
#include "renegade/bridge/WindowsGameBuildProjectService.h"
#include <stdexcept>
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/PackageIntegrityService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/TestLevelSnapshotService.h"
#include <WickedEngine.h>
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
namespace fs = std::filesystem;
using namespace renegade::bridge;
LRESULT CALLBACK ParityWindow(HWND h, UINT m, WPARAM w, LPARAM l)
{ return DefWindowProcW(h,m,w,l); }
std::string ContentHash(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read content input");
    std::uint64_t hash=1469598103934665603ull;
    for (char c; in.get(c);) { hash ^= static_cast<unsigned char>(c); hash *= 1099511628211ull; }
    if (!in.eof()) throw std::runtime_error("Cannot complete content read");
    std::ostringstream out; out<<"fnv1a64:"<<std::hex<<std::setfill('0')<<std::setw(16)<<hash;
    return out.str();
}
int main(int argc, char** argv)
{
    if (argc==3 && std::string(argv[1])=="--inspect-export-project") {
        WNDCLASSEXW c={}; c.cbSize=sizeof(c); c.lpfnWndProc=ParityWindow;
        c.hInstance=GetModuleHandleW(nullptr); c.lpszClassName=L"P1ExportPreflight";
        RegisterClassExW(&c);
        HWND h=CreateWindowExW(0,c.lpszClassName,L"P1 Export Preflight",WS_OVERLAPPEDWINDOW,
            0,0,64,64,nullptr,nullptr,c.hInstance,nullptr);
        if(!h) return 2;
        wi::Application app; app.allow_hdr=false; app.SetWindow(h);
        wi::initializer::InitializeComponentsImmediate();
        ProjectService projects; ProjectMetadata project; std::string error;
        WindowsGameBuildProjectState state;
        const bool inspected=projects.InspectProject(fs::absolute(argv[2]).generic_u8string(),project,error);
        std::cout<<"PROJECT_INSPECTED="<<inspected<<"\n";
        const bool prepared=inspected && PrepareWindowsGameBuildProjectState(project,state,error);
        std::cout<<"EXPORT_PREPARED="<<prepared<<"\n";
        while(wi::renderer::IsPipelineCreationActive()>0) Sleep(10);
        wi::graphics::GetDevice()->WaitForGPU();
        if(!prepared) {std::cerr<<error<<"\n"; return 1;}
        std::cout<<"EXPORT_PREFLIGHT=PASS\nLEVEL_COMPLETIONS="<<state.levelCompletionCount<<"\n";
        for(const auto& step:state.expectedFlowTrace) std::cout<<step<<"\n";
        std::cout<<std::flush;
        return 0;
    }
    if (argc!=7) { std::cerr<<"Expected project, Runtime, dxcompiler, BuildInputs, output, source revision\n"; return 2; }
    try {
        std::string error;
        ProjectService projects;
        ProjectMetadata project;
        if (!projects.InspectProject(fs::absolute(argv[1]).generic_u8string(), project, error))
            throw std::runtime_error(error);
        WNDCLASSEXW c={}; c.cbSize=sizeof(c); c.lpfnWndProc=ParityWindow;
        c.hInstance=GetModuleHandleW(nullptr); c.lpszClassName=L"P1ParityProof";
        RegisterClassExW(&c);
        HWND h=CreateWindowExW(0,c.lpszClassName,L"P1 Parity",WS_OVERLAPPEDWINDOW,
            0,0,64,64,nullptr,nullptr,c.hInstance,nullptr);
        if (!h) throw std::runtime_error("Cannot create GPU fixture window");
        {
            wi::Application app; app.allow_hdr=false; app.SetWindow(h);
            wi::initializer::InitializeComponentsImmediate();
            SceneService scenes;
            if (!scenes.LoadScene((fs::u8path(project.rootPath)/fs::u8path(project.startupScene)).generic_u8string()))
                throw std::runtime_error(scenes.LastError());
            const auto start=ResolvePlayerStart(scenes.GetScene());
            if(start.resolution!=PlayerStartResolution::Success || !start.start.settings.firstPersonArmsAssetId.empty())
                throw std::runtime_error("Expected one Player Start and no imported arms");
            const auto originalPath=scenes.CurrentPath();
            CommandService commands;
            TestLevelSnapshotService snapshots(scenes,commands);
            TestLevelSnapshot snapshot;
            if(!snapshots.Create(project,snapshot,error) || !snapshot.IsRuntimeReady())
                throw std::runtime_error(error);
            SceneService reopened;
            if(!reopened.LoadScene(snapshot.scenePath) || scenes.CurrentPath()!=originalPath)
                throw std::runtime_error("Snapshot reload or authoritative path preservation failed");
            const auto copied=ResolvePlayerStart(reopened.GetScene());
            if(copied.resolution!=PlayerStartResolution::Success ||
               !copied.start.settings.firstPersonArmsAssetId.empty())
                throw std::runtime_error("Snapshot changed Player Start or arms assignment");
            std::cout<<"SNAPSHOT_DESCRIPTOR="<<snapshot.descriptorPath<<"\n";
            while(wi::renderer::IsPipelineCreationActive()>0) Sleep(10);
            wi::graphics::GetDevice()->WaitForGPU();
        }
        DestroyWindow(h);
        WindowsGameBuildPlan plan;
        plan.projectId=project.projectId; plan.gameName="P1 Proxy Parity";
        plan.executableFileName="P1ProxyParity.exe"; plan.buildFolderName="P1 Proxy Parity";
        plan.publicVersion="0.1.0-p1-proof"; plan.saveDataId=project.projectId;
        const std::vector<std::pair<std::string,DependencyClass>> content={
            {fs::u8path(project.descriptorPath).filename().generic_u8string(),DependencyClass::ProjectDocument},
            {project.startupScene,DependencyClass::Scene},
            {"Content/Data/GameplayInput.renegade-input",DependencyClass::Data}};
        for(std::size_t i=0;i<content.size();++i) {
            WindowsGameBuildFile f; f.destinationPath="GameData/"+content[i].first;
            f.projectRelativeSourcePath=content[i].first; f.dependencyClass=content[i].second;
            f.assetId="88000000-0000-4000-8000-00000000000"+std::to_string(i+1);
            f.sourceContentHash=ContentHash(fs::u8path(project.rootPath)/fs::u8path(content[i].first));
            f.provenance={"p1:manual-proxy-parity"}; plan.files.push_back(f);
        }
        WindowsGameBuildStagingRequest request;
        request.projectRootPath=project.rootPath; request.outputParentPath=fs::absolute(argv[5]).generic_u8string();
        request.stagingId="p1-proxy-parity-"+std::to_string(GetCurrentProcessId()); request.renegadeRevision=argv[6];
        request.wickedRevision="3a800b7134aafe58461093c8abb2e274d4e64033";
        for(const auto& support: std::vector<std::pair<std::string,std::string>>{
            {"P1ProxyParity.exe",argv[2]},{"dxcompiler.dll",argv[3]}}) {
            WindowsGamePackageFileDigest digest;
            if(!DigestWindowsGamePackageFile(fs::absolute(support.second).generic_u8string(),digest,error))
                throw std::runtime_error(error);
            WindowsGameBuildFile f; f.kind=WindowsGameBuildFileKind::RuntimeSupport;
            f.destinationPath=support.first; f.runtimeSupportName=support.first=="dxcompiler.dll"?"directx-shader-compiler":"renegade-runtime";
            f.byteCount=digest.byteCount; f.sha256=digest.sha256;
            f.provenance={"p1:owner-verified-runtime"}; plan.files.push_back(f);
            request.runtimeSupportSources.push_back({support.first,fs::absolute(support.second).generic_u8string()});
        }
        const fs::path docs=fs::absolute(argv[4]);
        for(const auto& name:{"ReadMe.txt","Renegade-Licence-or-Notice.txt","WickedEngine-LICENSE.txt",
                "WickedEngine-third_party_software.txt","DirectXShaderCompiler-LICENSE.txt",
                "DirectXShaderCompiler-ThirdPartyNotices.txt"}) {
            const std::string destination=std::string(name)=="ReadMe.txt"?name:std::string("Licences/")+name;
            request.packageDocuments.push_back({destination,(docs/name).generic_u8string(),name,"repo:p1-parity"});
        }
        WindowsGameBuildStageResult stage;
        if(!StageWindowsGameBuild(plan,request,stage,error) || !ValidateWindowsGameBuildStage(stage,error))
            throw std::runtime_error(error);
        // Finish the same Gate 3 identity boundary used by Build Game.
        // A minimal proof icon is a 1x1 32-bit ICO, independent of owner assets.
        const fs::path icon=fs::absolute(argv[5])/"P1ProxyParity.ico";
        std::vector<unsigned char> bytes;
        auto word=[&](unsigned n){bytes.push_back(n&255);bytes.push_back((n>>8)&255);};
        auto dword=[&](unsigned n){word(n&65535);word(n>>16);};
        word(0); word(1); word(1); bytes.insert(bytes.end(),{1,1,0,0});
        word(1); word(32); dword(48); dword(22);
        dword(40); dword(1); dword(2); word(1); word(32);
        dword(0); dword(4); dword(0); dword(0); dword(0); dword(0);
        bytes.insert(bytes.end(),{210,130,60,255,0,0,0,0});
        {std::ofstream out(icon,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
         if(!out) throw std::runtime_error("Cannot write proof icon");}
        WindowsGameExecutableIdentityRequest identity;
        identity.developerPublisher="Maverick Media Studio"; identity.description="P1 proxy parity proof";
        identity.copyrightNotice="Copyright 2026 Maverick Media Studio";
        identity.internalBuildId="p1-camera-sync-"+std::string(argv[6]);
        identity.buildTimestampUtc="2026-10-03T21:59:00Z"; identity.iconSourcePath=icon.generic_u8string();
        WindowsGameExecutableIdentityResult identityResult;
        if(!ApplyWindowsGameExecutableIdentity(plan,identity,stage,identityResult,error) ||
           !ValidateWindowsGameBuildStage(stage,error)) throw std::runtime_error(error);
        std::cout<<"STAGED_PACKAGE="<<stage.stagingPath<<"\nPACKAGE_EXE_SHA256="
                 <<identityResult.executableSha256<<"\nPARITY_FIXTURE_RESULT=0\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<"\n"; return 1; }
}
