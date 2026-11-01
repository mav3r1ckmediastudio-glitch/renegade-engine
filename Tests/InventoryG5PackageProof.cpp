#include "InventoryG5DisposableProject.h"
// G5E isolated production Build Game staging proof. Never promotes user builds.
#include "renegade/bridge/WindowsGameBuildProjectService.h"
#include "renegade/bridge/BuildService.h"
#include "renegade/bridge/BuildStageService.h"
#include "renegade/bridge/BuildIdentityService.h"
#include "renegade/bridge/PackageIntegrityService.h"
#include "renegade/bridge/ProjectService.h"
#include <WickedEngine.h>
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
namespace fs=std::filesystem;
using namespace renegade::bridge;
#define CHECK(c,message) do { if(!(c)) {std::cerr<<"G5E PACKAGE: "<<message<<"\n";std::cerr.flush();std::_Exit(1);} } while(false)
int main(int argc,char** argv) {
    CHECK(argc==3 || argc==4,"expected sandbox project, Studio BuildInputs and optional G5G staging ID");
    if(argc==4) CHECK(std::string(argv[3])=="inventory-river-integration-20261028-v1" ||
                      std::string(argv[3])=="g5g-input-proof-20261028-v1" ||
                      std::string(argv[3])=="g5g-input-proof-20261028-v2" ||
                      std::string(argv[3])=="g5h-use-drop-20261028-v1" ||
                      std::string(argv[3])=="g5h-use-drop-20261028-v2" ||
                      std::string(argv[3])=="g5h-use-drop-20261028-v3" ||
                      std::string(argv[3])=="g5h-health-hud-20261028-v1" ||
                      std::string(argv[3])=="g5h-health-hud-20261028-v2" ||
                      std::string(argv[3])=="g5h-inventory-signoff-20261028-v3",
                      "refusing unexpected stage name");
    std::error_code ec;
    auto root=fs::weakly_canonical(fs::u8path(argv[1]),ec);
    CHECK(!ec && root.filename()=="G5PackagePlay" &&
          IsDisposableG5Project(root,"G5PackagePlay"),
          "refusing any non-disposable project");
    const auto buildInputs=fs::weakly_canonical(fs::u8path(argv[2]),ec);
    CHECK(!ec && buildInputs.filename()=="BuildInputs","invalid BuildInputs");
    wi::Application app;
    HWND window=CreateWindowExW(0,L"STATIC",L"G5 package proof",WS_OVERLAPPEDWINDOW,
        0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    CHECK(window!=nullptr,"cannot create native proof window");
    app.allow_hdr=false;
    app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();

    ProjectService projects;
    ProjectMetadata project;
    std::string error;
    CHECK(projects.InspectProject((root/"ScreenProject.renegade").generic_u8string(),project,error),error);
    WindowsGameBuildProjectState state;
    const auto studio=buildInputs.parent_path();
    std::vector<WindowsGameBundledResource> bundle;
    for(const auto& item : std::vector<std::pair<std::string,std::string>> {
        {"terrain-basecolor","Content/terrain/default_grass/default_grass_basecolor.tga"},
        {"terrain-normal","Content/terrain/default_grass/default_grass_normal.tga"},
        {"terrain-surface","Content/terrain/default_grass/default_grass_surface.tga"},
        {"snowflake","Content/weather/snowflake.dds"} }) {
        const auto from=studio/fs::u8path(item.second);
        if(fs::is_regular_file(from))
            bundle.push_back({item.first,from.generic_u8string(),item.second});
    }
    // Match required river/waterfall support shipped by the production Studio controller.
    for(const auto* name : {
        "Content/river/waterNormalMap.jpg",
        "Content/river/RenegadeRiverUnderwaterCS.cso",
        "Content/river/RiverContactFoamPS.cso",
        "Content/river/RiverFoamTransportCS.cso",
        "Content/river/RiverWaterwayFoamPS.cso",
        "Content/river/RiverWaterwayFoamCS.cso",
        "Content/river/RiverRapidsPS.cso",
        "Content/river/RiverRapidsCS.cso",
        "Content/river/RiverRefinedFoamPS.cso",
        "Content/river/RiverFoamDetailCS.cso",
        "Content/river/RiverSurfaceVS.cso",
        "Content/river/RiverSurfaceShadowVS.cso",
        "Content/river/RiverCoursePS.cso",
        "Content/river/RiverCourseVS.cso",
        "Content/river/RiverCourseShadowVS.cso",
        "Content/river/OriginalFoamCoverage.png",
        "Content/waterfall/PlaceableWaterfallPS.cso",
        "Content/waterfall/PlaceableWaterfallVS.cso",
        "Content/waterfall/AeratedWater.png",
        "Content/waterfall/SteamMist.png",
        "Content/waterfall/WaterWisp.png"
    }) {
        const auto from=studio/fs::u8path(name);
        CHECK(fs::is_regular_file(from),std::string("missing required river/waterfall asset: ")+name);
        bundle.push_back({std::string("integration-")+name,from.generic_u8string(),name});
    }
    CHECK(PrepareWindowsGameBuildProjectState(project,bundle,state,error),error);
    std::cout<<"G5E_DISCOVERED_DEPENDENCIES="<<state.dependencyGraph.nodes.size()<<"\n";

    const auto runtime=studio.parent_path().parent_path()/"Runtime"/"Release";
    const auto executable=runtime/"RenegadeRuntime.exe";
    const auto dxc=runtime/"dxcompiler.dll";
    CHECK(fs::is_regular_file(executable) && fs::is_regular_file(dxc),
          "Release Runtime or dxcompiler.dll missing");
    WindowsGameBuildRequest request;
    request.gameName="Renegade G5 Packaged Inventory Proof";
    request.executableBaseName="G5InventoryProof";
    request.saveDataId=project.projectId;
    WindowsGameBuildStagingRequest stage;
    stage.projectRootPath=project.rootPath;
    stage.outputParentPath=(root/"Builds"/"Windows").generic_u8string();
    stage.stagingId=argc==4?argv[3]:"g5e-inventory-proof-20261028-v3";
    stage.renegadeRevision=RENEGADE_SOURCE_REVISION;
    stage.wickedRevision=RENEGADE_WICKED_REVISION;
    CHECK(SerializeAssetRegistry(state.assetRegistry,stage.assetRegistryJson,error),error);
    std::vector<WindowsRuntimeSupportInput> support;
    for(const auto& item : std::vector<std::pair<std::string,fs::path>>{
        {"G5InventoryProof.exe",executable},{"dxcompiler.dll",dxc}}) {
        WindowsGamePackageFileDigest digest;
        CHECK(DigestWindowsGamePackageFile(item.second.generic_u8string(),digest,error),error);
        support.push_back({item.first=="dxcompiler.dll"?"directx-shader-compiler":"renegade-runtime",
            item.first,digest.byteCount,digest.sha256,std::string("repo:")+RENEGADE_SOURCE_REVISION});
        stage.runtimeSupportSources.push_back({item.first,item.second.generic_u8string()});
    }
    for(const auto& resource:state.bundledResources) {
        WindowsGamePackageFileDigest digest;
        CHECK(DigestWindowsGamePackageFile(resource.sourcePath,digest,error),error);
        support.push_back({resource.logicalName,resource.destinationPath,
            digest.byteCount,digest.sha256,std::string("repo:")+RENEGADE_SOURCE_REVISION});
        stage.runtimeSupportSources.push_back({resource.destinationPath,resource.sourcePath});
    }
    for(const auto& name:{"ReadMe.txt","Renegade-Licence-or-Notice.txt",
           "WickedEngine-LICENSE.txt","WickedEngine-third_party_software.txt",
           "DirectXShaderCompiler-LICENSE.txt","DirectXShaderCompiler-ThirdPartyNotices.txt"}) {
        const auto source=buildInputs/name;
        CHECK(fs::is_regular_file(source),"missing governed package notice");
        const std::string destination=std::string(name)=="ReadMe.txt"
            ? name:std::string("Licences/")+name;
        stage.packageDocuments.push_back({destination,source.generic_u8string(),
            name,std::string("repo:")+RENEGADE_SOURCE_REVISION});
    }
    WindowsGameBuildPlan plan;
    CHECK(CreateWindowsGameBuildPlan(project,state.dependencyGraph,state.assetRegistry,
          request,support,plan,error),error);
    bool itemCatalogue=false,itemIcon=false,inventoryCompanion=false;
    bool inventoryBinding=false,assignedScreen=false,healthHud=false;
    for(const auto& file:plan.files) {
        itemCatalogue|=file.destinationPath=="GameData/Content/Gameplay/Inventory.renegade-items";
        itemIcon|=file.destinationPath=="GameData/Content/UI/InventoryIcons/potion.png";
        inventoryCompanion|=file.destinationPath=="GameData/Content/Scenes/LevelOne.wiscene.rinventory";
        inventoryBinding|=file.destinationPath=="GameData/Content/Gameplay/Inventory.renegade-ui";
        assignedScreen|=file.destinationPath=="GameData/Content/UI/InventoryG4B3B2Grid.renegade-screen";
        healthHud|=file.destinationPath=="GameData/Content/UI/G5HealthTestHud.renegade-screen";
    }
    CHECK(itemCatalogue && itemIcon && inventoryCompanion &&
          inventoryBinding && assignedScreen && healthHud,
        "production plan omitted assigned Inventory Screen or its inventory/graphics dependencies");
    WindowsGameBuildStageResult result;
    CHECK(StageWindowsGameBuild(plan,stage,result,error),error);

    const auto icon=root/"G5E-Proof.ico";
    {
        std::vector<unsigned char> bytes;
        auto word=[&](unsigned n){bytes.push_back(n&255);bytes.push_back((n>>8)&255);};
        auto dword=[&](unsigned n){word(n&65535);word(n>>16);};
        word(0);word(1);word(1);bytes.insert(bytes.end(),{1,1,0,0});
        word(1);word(32);dword(48);dword(22);
        dword(40);dword(1);dword(2);word(1);word(32);
        dword(0);dword(4);dword(0);dword(0);dword(0);dword(0);
        bytes.insert(bytes.end(),{210,130,60,255,0,0,0,0});
        std::ofstream out(icon,std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        CHECK(bool(out),"could not write disposable icon");
    }
    WindowsGameExecutableIdentityRequest identity;
    identity.developerPublisher="Maverick Media Studio";
    identity.description="G5E inventory package proof";
    identity.copyrightNotice="Copyright 2026 Maverick Media Studio";
    identity.internalBuildId="g5e-20261028-local";
    identity.buildTimestampUtc="2026-10-28T01:21:00Z";
    identity.iconSourcePath=icon.generic_u8string();
    WindowsGameExecutableIdentityResult identified;
    CHECK(ApplyWindowsGameExecutableIdentity(plan,identity,result,identified,error),error);
    CHECK(ValidateWindowsGameBuildStage(result,error),error);
    std::cout<<"G5E_STAGED_PACKAGE="<<result.stagingPath
             <<"\nG5E_GOVERNED_FILES="<<result.files.size()
             <<"\nG5E_CATALOGUE_ICON_INVENTORY=PASS"
             <<"\nG5G_EXPLICIT_INVENTORY_SCREEN_DEPENDENCY=PASS\n";
    std::cout.flush();
    std::_Exit(0);
}
