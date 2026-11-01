#include "InventoryG5DisposableProject.h"
#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/InventoryPickupAuthoringService.h"
#include "renegade/bridge/InventoryItemCatalogueService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/RiverFlowRenderer.h"
#include "renegade/bridge/RiverFlowFieldService.h"
#include <algorithm>
#include <cmath>
#include "renegade/bridge/RiverTerrainLayerService.h"
#include "renegade/bridge/TestLevelSnapshotService.h"
#include "../EngineBridge/src/PlaceableWaterfallAuthoring.h"
#include "../EngineBridge/src/WaterfallCollisionPaths.h"
#include <WickedEngine.h>
#include <windows.h>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <limits>
#include <sstream>
#include <memory>

namespace fs=std::filesystem;
using namespace renegade::bridge;
static void Need(bool yes,const std::string& why) {
    if(!yes) {std::cerr<<"COMBINED_PROOF_FAIL: "<<why<<std::endl;std::_Exit(1);}
}
// Exact native chunk triangles; nearest vertices and a metadata-only "linked" flag
// are insufficient to prove the buried, rounded water/terrain intersection.
static float GroundY(const wi::scene::Scene& scene,float x,float z) {
    constexpr int side=wi::terrain::chunk_width;
    for(size_t t=0;t<scene.terrains.GetCount();++t)
        for(const auto& chunk:scene.terrains[t].chunks) {
            const auto* mesh=scene.meshes.GetComponent(chunk.second.entity);
            if(!mesh || mesh->vertex_positions.size()!=size_t(side*side))continue;
            const auto world=scene.ComputeEntityMatrixRecursive(chunk.second.entity);
            auto transformed=[&](size_t i) {
                DirectX::XMFLOAT3 p;
                DirectX::XMStoreFloat3(&p,DirectX::XMVector3TransformCoord(
                    DirectX::XMLoadFloat3(&mesh->vertex_positions[i]),world));
                return p;
            };
            const auto origin=transformed(0),p1=transformed(1),pz=transformed(side);
            const float dx=p1.x-origin.x,dz=pz.z-origin.z;
            if(std::abs(dx)<1e-5f || std::abs(dz)<1e-5f)continue;
            const float u=(x-origin.x)/dx,v=(z-origin.z)/dz;
            if(u<0 || v<0 || u>side-1 || v>side-1)continue;
            const int ix=std::min(int(u),side-2),iz=std::min(int(v),side-2);
            const float a=u-ix,b=v-iz;
            const size_t idx=size_t(ix+iz*side);
            const float h00=transformed(idx).y,h10=transformed(idx+1).y,
                        h01=transformed(idx+side).y,h11=transformed(idx+side+1).y;
            return a+b<=1 ? h00*(1-a-b)+h10*a+h01*b
                : h01*(1-a)+h10*(1-b)+h11*(a+b-1);
        }
    return std::numeric_limits<float>::quiet_NaN();
}
static std::pair<float,float> BaselineWorldRange(wi::scene::Scene& scene,
    wi::ecs::Entity terrainId,std::string& error) {
    TerrainSculptState baseline;
    Need(ReadRiverTerrainBaseline(scene,terrainId,baseline,error),error);
    float low=INFINITY,high=-INFINITY;
    for(const auto& chunk:baseline.chunks) {
        const auto* mesh=scene.meshes.GetComponent(chunk.entity);
        Need(mesh && mesh->vertex_positions.size()==chunk.heights.size(),
             "Baseline vertex/terrain mismatch");
        const auto world=scene.ComputeEntityMatrixRecursive(chunk.entity);
        for(size_t i=0;i<chunk.heights.size();++i) {
            auto local=mesh->vertex_positions[i];
            local.y=chunk.heights[i];
            DirectX::XMFLOAT3 w;
            DirectX::XMStoreFloat3(&w,DirectX::XMVector3TransformCoord(
                DirectX::XMLoadFloat3(&local),world));
            low=std::min(low,w.y);high=std::max(high,w.y);
        }
    }
    Need(std::isfinite(low)&&std::isfinite(high),"Nonfinite baseline shelf");
    return {low,high};
}
static bool VerifyWaterfallLandingMatch(wi::scene::Scene& scene,std::string& why) {
    std::vector<placeable_waterfall::AuthoredWaterfall> waterfalls;
    if(!placeable_waterfall::EnumerateWaterfalls(scene,waterfalls,why) || waterfalls.size()!=1) {
        why="Combined fixture requires exactly one authored waterfall";return false;
    }
    if(scene.splines.GetCount()!=1 || scene.splines[0].spline_node_entities.size()!=6) {
        why="Missing six native receiving water controls";return false;
    }
    const auto landingNode=scene.splines[0].spline_node_entities[1];
    DirectX::XMFLOAT3 water;
    DirectX::XMStoreFloat3(&water,scene.ComputeEntityMatrixRecursive(landingNode).r[3]);
    const auto& target=waterfalls.front().settings.landing;
    const float horizontal=std::hypot(target.x-water.x,target.z-water.z);
    const float vertical=std::abs(target.y-water.y);
    std::ostringstream out;
    out<<"waterY="<<water.y<<" waterfallLandingY="<<target.y
       <<" deltaY="<<vertical<<" deltaXZ="<<horizontal;
    why=out.str();
    return vertical<.02f && horizontal<.02f;
}
static bool VerifyContainedBankGeometry(wi::scene::Scene& scene,
    wi::ecs::Entity river,std::string& message) {
    std::shared_ptr<const RiverFlowField> flow;
    RiverFlowSettings params;
    if(!BuildRiverFlowField(scene,river,params,flow,message))return false;
    const auto& rows=flow->NativeRibbon();
    if(rows.size()<8){message="Too few actual native water rows";return false;}
    float minBurial=INFINITY,minDepth=INFINITY;
    for(size_t k=0;k<rows.size();k+=std::max(size_t(1),rows.size()/16)) {
        const auto& row=rows[k];
        for(const auto p:{row.leftBank,row.rightBank}) {
            const float h=GroundY(scene,p.x,p.z);
            if(!std::isfinite(h)){message="Bank height outside native terrain grid";return false;}
            minBurial=std::min(minBurial,h-p.y);
        }
        if(k>=rows.size()/4 && k<=rows.size()*3/4) {
            const float h=GroundY(scene,row.centre.x,row.centre.z);
            if(!std::isfinite(h)){message="Water centre outside terrain grid";return false;}
            minDepth=std::min(minDepth,row.centre.y-h);
        }
    }
    for(const auto& row:{rows.front(),rows.back()}) {
        const float h=GroundY(scene,row.centre.x,row.centre.z);
        if(!std::isfinite(h)){message="Rounded end outside terrain grid";return false;}
        minBurial=std::min(minBurial,h-row.centre.y);
    }
    std::ostringstream status;
    status<<"Native banks and rounded ends: minBurial="<<minBurial
          <<"m, wet centre minimum depth="<<minDepth<<"m; rows="<<rows.size();
    message=status.str();
    return minBurial>=.08f && minDepth>=.25f;
}
int main(int argc,char** argv) {
 try {
    const bool diagnose=argc==3 && std::string(argv[2])=="--diagnose";
    const bool refreshSnapshot=argc==3 && std::string(argv[2])=="--refresh-snapshot";
    const bool concealBanks=argc==3 && std::string(argv[2])=="--conceal-banks";
    const bool landWaterfall=argc==3 && std::string(argv[2])=="--land-waterfall";
    const bool surveyFalls=argc==3 && std::string(argv[2])=="--survey-falls";
    Need(argc==2 || diagnose || refreshSnapshot || concealBanks || landWaterfall || surveyFalls,"Expected disposable combined project or diagnostic/fixture maintenance mode");
    std::error_code ec;
    const auto root=fs::weakly_canonical(fs::u8path(argv[1]),ec);
    Need(!ec && IsDisposableG5Project(root,"G5PackagePlay"),"Refusing non-disposable project");
    const auto marker=root/"INVENTORY-RIVER-AUTHORED.txt";
    Need(diagnose || refreshSnapshot || concealBanks || landWaterfall || surveyFalls || !fs::exists(marker),"Refusing repeated fixture authoring");
    wi::Application app;
    const HWND window=CreateWindowExW(0,L"STATIC",L"Combined inventory river proof",WS_OVERLAPPEDWINDOW,
        0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Need(window!=nullptr,"Native proof window failed");
    app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
    wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
    StudioSession session;
    session.Projects().Initialize((root/"CombinedProof.ini").generic_u8string());
    Need(session.Projects().OpenProject((root/"ScreenProject.renegade").generic_u8string()),session.Projects().LastError());
    Need(session.LoadScene(session.Projects().StartupScenePath()),session.Scenes().LastError());
    auto& scene=session.Scenes().GetScene();
    std::string error;
    if(surveyFalls) {
        std::vector<placeable_waterfall::AuthoredWaterfall> falls;
        Need(placeable_waterfall::EnumerateWaterfalls(scene,falls,error)&&falls.size()==1,
             "Collision survey needs one authored waterfall");
        scene.Update(0);
        const auto& def=falls[0].settings;
        auto paths=placeable_waterfall::TraceCollisionPaths(def,[&](DirectX::XMFLOAT3 a,DirectX::XMFLOAT3 b){
            DirectX::XMVECTOR delta=DirectX::XMLoadFloat3(&b)-DirectX::XMLoadFloat3(&a);
            const float len=DirectX::XMVectorGetX(DirectX::XMVector3Length(delta));
            if(len<1e-5f)return placeable_waterfall::PathHit{};
            const auto hit=scene.Intersects(wi::primitive::Ray(DirectX::XMLoadFloat3(&a),delta/len,0,len));
            if(hit.entity==wi::ecs::INVALID_ENTITY) return placeable_waterfall::PathHit{};
            auto normal=hit.normal;
            if(hit.vertexID0>=0){
                const auto* obj=scene.objects.GetComponent(hit.entity);
                if(obj){
                    const auto* mesh=scene.meshes.GetComponent(obj->meshID);
                    if(mesh){
                        const auto matrix=scene.ComputeEntityMatrixRecursive(hit.entity);
                        auto x=DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&mesh->vertex_positions[hit.vertexID0]),matrix),
                             y=DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&mesh->vertex_positions[hit.vertexID1]),matrix),
                             z=DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&mesh->vertex_positions[hit.vertexID2]),matrix);
                        DirectX::XMStoreFloat3(&normal,DirectX::XMVector3Normalize(DirectX::XMVector3Cross(y-x,z-x)));
                    }
                }
            }
            return placeable_waterfall::PathHit{true,hit.position,normal};
        });
        size_t reaches=0,misses=0,stalled=0;
        float minY=INFINITY,maxY=-INFINITY;
        for(size_t i=0;i<paths.paths.size();++i){
            const auto& path=paths.paths[i];
            const auto& last=path.points.back();
            if(last.position.y<=def.landing.y+.02f)++reaches;else ++misses;
            if(path.points.size()>=650)++stalled;
            minY=std::min(minY,last.position.y);maxY=std::max(maxY,last.position.y);
            size_t lowContact=0;for(size_t k=0;k<path.points.size();++k) {
                if(path.points[k].contact &&
                   path.points[k].position.y<=def.landing.y+.6f) {lowContact=k;break;}
            }
            if(i<32)std::cout<<"PATH "<<i<<" end="<<last.position.x<<","
                <<last.position.y<<","<<last.position.z<<" steps="<<path.points.size()
                <<" contact="<<last.contact<<" firstBankContact="<<lowContact
                <<(lowContact ? " height=" : " noHeight=")
                <<(lowContact?path.points[lowContact].position.y:0) <<std::endl;
        }
        std::cout<<"WATERFALL_SURVEY landing="<<def.landing.y<<" reaches="<<reaches
                 <<" misses="<<misses<<" stalled="<<stalled
                 <<" minFinalY="<<minY<<" maxFinalY="<<maxY
                 <<" contacts="<<paths.contacts<<" impacts="<<paths.impacts.size()<<std::endl;
        std::_Exit(0);
    }
    if(landWaterfall) {
        Need(fs::is_regular_file(marker),"Refusing waterfall edit outside an authored disposable integration fixture");
        Need(scene.terrains.GetCount()==1 && scene.splines.GetCount()==1,
             "Waterfall correction requires exactly one linked receiving river and terrain");
        const auto terrain=scene.terrains.GetEntity(0),river=scene.splines.GetEntity(0);
        RiverTerrainLayerSettings bed;
        Need(ReadRiverTerrainLayerSettings(scene,terrain,river,bed,error)&&bed.linked,
             "Waterfall correction requires valid linked bed");
        std::string geom;
        Need(VerifyContainedBankGeometry(scene,river,geom),
             "Correct river bank containment BEFORE changing waterfall landing: "+geom);
        std::vector<placeable_waterfall::AuthoredWaterfall> waterfall;
        Need(placeable_waterfall::EnumerateWaterfalls(scene,waterfall,error)
             && waterfall.size()==1,"One authored G13 waterfall required: "+error);
        const auto& original=waterfall.front();
        Need(std::abs(original.settings.landing.y)<.01f,
             "Unexpected waterfall landing height; refusing unrelated/duplicate correction");
        const auto receiver=scene.splines[0].spline_node_entities[1];
        DirectX::XMFLOAT3 water;
        DirectX::XMStoreFloat3(&water,scene.ComputeEntityMatrixRecursive(receiver).r[3]);
        Need(std::abs(original.settings.landing.x-water.x)<.02f &&
             std::abs(original.settings.landing.z-water.z)<.02f &&
             std::abs(water.y+.85f)<.02f,
             "Receiving water node is not at expected G11B impact position");
        auto revised=original.settings;
        revised.landing.y=water.y;
        Need(session.Commands().Execute(placeable_waterfall::MakeEditWaterfallCommand(
             scene,original.id,revised)),"Native waterfall landing edit refused");
        std::string verify;
        Need(VerifyWaterfallLandingMatch(scene,verify),
             "Edited waterfall misses native river: "+verify);
        const auto path=session.Scenes().CurrentPath();
        Need(session.SaveScene(path),"Could not save waterfall landing correction");
        Need(session.LoadScene(path),"Could not reopen waterfall landing correction");
        Need(VerifyWaterfallLandingMatch(scene,verify),
             "Waterfall landing not aligned after save/reopen: "+verify);
        Need(VerifyContainedBankGeometry(scene,scene.splines.GetEntity(0),geom),
             "Water containment regressed after waterfall update: "+geom);
        Need(placeable_waterfall::EnumerateWaterfalls(scene,waterfall,error)&&
             waterfall.size()==1&&
             std::abs(waterfall.front().settings.crest.y-original.settings.crest.y)<.0001f,
             "Waterfall crest modified or record lost during landing correction");
        std::cout<<"G13_WATERFALL_ON_G11B_WATERLINE=PASS "
                 <<verify<<" crest_preserved=1 bank_contained=1"<<std::endl;
        std::_Exit(0);
    }
    if(concealBanks) {
        Need(fs::is_regular_file(marker),"Only the authored disposable combined fixture may be corrected");
        Need(scene.terrains.GetCount()==1 && scene.splines.GetCount()==1,
             "Unexpected terrain/river count; cannot correct fixture");
        const auto terrainId=scene.terrains.GetEntity(0);
        const auto riverId=scene.splines.GetEntity(0);
        RiverTerrainLayerSettings settings;
        Need(ReadRiverTerrainLayerSettings(scene,terrainId,riverId,settings,error)
             && settings.linked && settings.depthMeters>1.f,
             "No accepted governed carved channel: "+error);
        const auto [baseLow,baseHigh]=BaselineWorldRange(scene,terrainId,error);
        Need(baseHigh-baseLow<.015f,"Fixture is not a flat uniform terrain shelf; do not globally lower water");
        const float target=baseLow-.30f; // G11B's original 30-cm concealed shelf, not an ad hoc clamp.
        const auto nodes=scene.splines[0].spline_node_entities;
        Need(nodes.size()==6,"Unexpected river node count");
        float delta=0;
        for(size_t k=0;k<nodes.size();++k) {
            const auto* tr=scene.transforms.GetComponent(nodes[k]);
            Need(tr!=nullptr && std::abs(tr->translation_local.y+.52f)<.003f,
                 "Refusing to modify a different/already corrected water surface");
            if(k==0)delta=target-tr->translation_local.y;
        }
        std::string beforeStatus;
        const bool beforeContained=VerifyContainedBankGeometry(scene,riverId,beforeStatus);
        std::cout<<"PREVIOUS_BANK_CONTAINMENT="<<beforeContained
                 <<" "<<beforeStatus<<" BASELINE="<<baseLow<<","<<baseHigh<<std::endl;
        Need(!beforeContained,"Water is already fully bank-contained; refusing repeat correction");
        for(const auto node:nodes) {
            auto* tr=scene.transforms.GetComponent(node);
            auto before=CaptureTransform(*tr),after=before;
            after.translation.y+=delta;
            SetTransformCommand move(scene,node,before,after);
            Need(move.Execute(),"Failed to move native water control");
        }
        Need(RecomposeRiverTerrainLayers(scene,terrainId,error),
             "Native linked riverbed recompose failed: "+error);
        std::string actualStatus;
        Need(VerifyContainedBankGeometry(scene,riverId,actualStatus),
             "Native bank / rounded endpoint containment not proven: "+actualStatus);
        const auto scenePath=session.Scenes().CurrentPath();
        Need(session.SaveScene(scenePath),"Unable to save disposable contained river fixture");
        Need(session.LoadScene(scenePath),"Unable to reopen disposable contained river fixture");
        Need(VerifyContainedBankGeometry(scene,scene.splines.GetEntity(0),actualStatus),
             "Bank or rounded endpoint containment failed after disk reopen: "+actualStatus);
        std::cout<<"G11B_NATIVE_CONCEALED_BANKS_ENDS=PASS"
                 <<" base="<<baseLow<<" water="<<target<<" delta="<<delta
                 <<" "<<actualStatus<<std::endl;
        std::_Exit(0);
    }
    if(refreshSnapshot) {
        Need(fs::is_regular_file(marker),"Expected already-authored disposable combined scene");
        Need(scene.terrains.GetCount()==1 && scene.splines.GetCount()==1,
             "Missing combined river/terrain scene");
        const auto terrainId=scene.terrains.GetEntity(0), riverId=scene.splines.GetEntity(0);
        RiverTerrainLayerSettings settings;
        Need(ReadRiverTerrainLayerSettings(scene,terrainId,riverId,settings,error)
             && settings.linked,"Expected linked terrain before refreshed snapshot: "+error);
        std::string containment;
        Need(VerifyContainedBankGeometry(scene,riverId,containment),
             "Refusing snapshot with exposed banks or endpoints: "+containment);
        Need(VerifyWaterfallLandingMatch(scene,containment),
             "Refusing snapshot with detached waterfall impact: "+containment);
        TestLevelSnapshotService snapshots(session.Scenes(),session.Commands());
        TestLevelSnapshot snapshot;
        Need(snapshots.Create(session.Projects().CurrentProject(),snapshot,error),error);
        const auto shadow=fs::u8path(snapshot.sessionDirectory);
        for(const auto* required : {"Content/Gameplay/Inventory.renegade-items",
             "Content/Gameplay/Inventory.renegade-ui",
             "Content/UI/InventoryG4B3B2Grid.renegade-screen",
             "Content/UI/InventoryIcons/potion.png"})
            Need(fs::is_regular_file(shadow/fs::u8path(required)),
                 std::string("Refreshed snapshot missing: ")+required);
        std::ofstream(root/"combined-snapshot-descriptor.txt")<<snapshot.descriptorPath;
        std::cout<<"REFRESHED_INVENTORY_RIVER_SNAPSHOT=PASS path="<<snapshot.descriptorPath<<std::endl;
        std::_Exit(0);
    }
    if(diagnose) {
        auto terrainId=scene.terrains.GetEntity(0);
        const auto [baseLow,baseHigh]=BaselineWorldRange(scene,terrainId,error);
        std::cout<<"DIAG_WORLD_BASELINE_MIN="<<baseLow<<" MAX="<<baseHigh<<std::endl;
        std::string containment;
        const bool contained=VerifyContainedBankGeometry(scene,scene.splines.GetEntity(0),containment);
        std::cout<<"DIAG_CONCEALED_BANKS_AND_ENDS="<<contained<<" DETAIL="<<containment<<std::endl;
        TerrainSculptState baseline;
        Need(ReadRiverTerrainBaseline(scene,terrainId,baseline,error),error);
        float baseMin=INFINITY,baseMax=-INFINITY;
        size_t baseCount=0;
        for(const auto& chunk:baseline.chunks)
            for(const float h:chunk.heights) {
                baseMin=std::min(baseMin,h);baseMax=std::max(baseMax,h);++baseCount;
            }
        std::cout<<"DIAG_BASELINE_HEIGHT_MIN="<<baseMin
                 <<" MAX="<<baseMax<<" COUNT="<<baseCount<<std::endl;
        if(scene.splines.GetCount()==1) {
            std::shared_ptr<const RiverFlowField> field;
            RiverFlowSettings options;
            if(BuildRiverFlowField(scene,scene.splines.GetEntity(0),options,field,error)) {
                const auto& rows=field->NativeRibbon();
                std::cout<<"DIAG_NATIVE_ROWS="<<rows.size()<<std::endl;
                auto terrainAt=[&](DirectX::XMFLOAT3 p) {
                    auto hit=wi::scene::Pick(wi::primitive::Ray(
                        DirectX::XMFLOAT3(p.x,50,p.z),DirectX::XMFLOAT3(0,-1,0),
                        0,100),wi::enums::FILTER_TERRAIN,~0u,scene);
                    return hit.entity==wi::ecs::INVALID_ENTITY?9999.f:hit.position.y;
                };
                for(size_t k:{size_t(0),rows.size()/4,rows.size()/2,
                               rows.size()*3/4,rows.size()-1}) {
                    const auto& row=rows[k];
                    std::cout<<"DIAG_NATIVE_ROW="<<k<<" CENTRE_Y="<<row.centre.y
                             <<" GROUND_CENTER="<<terrainAt(row.centre)
                             <<" WATER_WIDTH="<<std::hypot(row.rightBank.x-row.leftBank.x,
                                                         row.rightBank.z-row.leftBank.z)
                             <<" GROUND_LEFT="<<terrainAt(row.leftBank)
                             <<" GROUND_RIGHT="<<terrainAt(row.rightBank)
                             <<std::endl;
                }
            }else std::cout<<"DIAG_NATIVE_FIELD_ERROR="<<error<<std::endl;
        }
        std::cout << "DIAG_TERRAIN_COUNT=" << scene.terrains.GetCount()
                  << " RIVER_COUNT=" << scene.splines.GetCount() << std::endl;
        for(size_t t=0;t<scene.terrains.GetCount();++t) {
            auto terrainId=scene.terrains.GetEntity(t);
            auto& terrain=scene.terrains[t];
            std::cout << "DIAG_TERRAIN_CHUNKS=" << terrain.chunks.size()
                      << " LAYERS=" << HasRiverTerrainLayers(scene,terrainId) << std::endl;
            for(size_t s=0;s<scene.splines.GetCount();++s) {
                auto riverId=scene.splines.GetEntity(s);
                RiverTerrainLayerSettings settings;
                bool ok=ReadRiverTerrainLayerSettings(scene,terrainId,riverId,settings,error);
                std::cout << "DIAG_RIVER_LINK_OK=" << ok << " LINKED=" << settings.linked
                          << " DEPTH=" << settings.depthMeters << " BANK=" << settings.bankWidthMeters
                          << " ERR=" << error << std::endl;
                for(const auto node:scene.splines[s].spline_node_entities) {
                    const auto matrix=scene.ComputeEntityMatrixRecursive(node);
                    DirectX::XMFLOAT3 pos;
                    DirectX::XMStoreFloat3(&pos,DirectX::XMVector3TransformCoord(DirectX::XMVectorZero(),matrix));
                    float nearest=999999.0f, ground=999999.0f;
                    for(const auto& chunk:terrain.chunks) {
                        const auto* mesh=scene.meshes.GetComponent(chunk.second.entity);
                        if(!mesh)continue;
                        const auto world=scene.ComputeEntityMatrixRecursive(chunk.second.entity);
                        for(const auto& v:mesh->vertex_positions) {
                            DirectX::XMFLOAT3 q;
                            DirectX::XMStoreFloat3(&q,DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&v),world));
                            float dist=(q.x-pos.x)*(q.x-pos.x)+(q.z-pos.z)*(q.z-pos.z);
                            if(dist<nearest){nearest=dist;ground=q.y;}
                        }
                    }
                    std::cout << "DIAG_NODE_XYZ=" << pos.x << "," << pos.y << "," << pos.z
                              << " NEAREST_GROUND_Y=" << ground << " DIST2=" << nearest << std::endl;
                }
            }
        }
        std::cout << "DIAG_COMPLETE" << std::endl;
        std::_Exit(0);
    }
    std::vector<placeable_waterfall::AuthoredWaterfall> waterfalls;
    Need(placeable_waterfall::EnumerateWaterfalls(scene,waterfalls,error)&&waterfalls.size()==1,"Expected one authored waterfall");
    Need(scene.terrains.GetCount()==1&&scene.splines.GetCount()==1,"Expected native terrain and G11 river");
    std::string geometry;
    Need(VerifyContainedBankGeometry(scene,scene.splines.GetEntity(0),geometry),
         "Combined authoring proof has exposed water banks or ends: "+geometry);
    Need(VerifyWaterfallLandingMatch(scene,geometry),
         "Combined authoring proof waterfall impact not at waterline: "+geometry);
    Need(scene.splines[0].spline_node_entities.size()==6,"Expected six river nodes");
    const auto start=ResolvePlayerStart(scene);
    Need(start.resolution==PlayerStartResolution::Success,"Real Player Start missing");
    const auto p=scene.transforms.GetComponent(start.start.entity)->GetPosition();
    std::cout<<"PLAYER_START="<<p.x<<","<<p.y<<","<<p.z<<std::endl;
    const auto pickup=scene.Entity_CreateSphere("INTEGRATION Health Potion x3 (PRESS E)",.22f,12,12);
    auto* transform=scene.transforms.GetComponent(pickup);
    Need(transform!=nullptr,"Pickup transform missing");
    transform->Translate(DirectX::XMFLOAT3(p.x,p.y,p.z+1.2f));transform->UpdateTransform();
    InventoryPickupConfig config;
    config.canCollect=true;config.itemId="health-potion";config.quantity=3;
    config.method=InventoryPickupMethod::PressE;config.interactDistance=3;
    Need(SetInventoryPickupConfigCommand(scene,pickup,config).Execute(),"Could not author pickup");
    Need(IsValidStableId(PersistentEntityId(scene,pickup)),"Pickup stable ID missing");
    // Known copied fixture texture names are project-local; never admit arbitrary external inputs.
    const auto sceneFolder=root/"Content/Scenes";
    for(size_t i=0;i<scene.materials.GetCount();++i)for(auto& texture:scene.materials[i].textures) {
        if(texture.name.empty())continue;
        const auto name=fs::u8path(texture.name).filename();
        auto local=root/"Content/waterfall"/name;
        if(!fs::is_regular_file(local))local=sceneFolder/name;
        if(fs::is_regular_file(local))texture.name=fs::relative(local,sceneFolder).generic_u8string();
    }
    const auto path=session.Scenes().CurrentPath();
    Need(session.SaveScene(path),session.Scenes().LastError());
    Need(session.LoadScene(path),session.Scenes().LastError());
    const auto savedPickup=scene.Entity_FindByName("INTEGRATION Health Potion x3 (PRESS E)");
    Need(savedPickup!=wi::ecs::INVALID_ENTITY&&IsValidStableId(PersistentEntityId(scene,savedPickup)),"Pickup save/reopen failed");
    Need(placeable_waterfall::EnumerateWaterfalls(scene,waterfalls,error)&&waterfalls.size()==1&&
        scene.terrains.GetCount()==1&&scene.splines.GetCount()==1&&scene.splines[0].spline_node_entities.size()==6,
        "River/waterfall lost during inventory native save/reopen");
    TestLevelSnapshotService snapshots(session.Scenes(),session.Commands());
    TestLevelSnapshot snapshot;
    Need(snapshots.Create(session.Projects().CurrentProject(),snapshot,error),error);
    Need(fs::is_regular_file(fs::u8path(snapshot.scenePath+".rinventory")),"Snapshot omitted inventory companion");
    const auto shadow=fs::u8path(snapshot.sessionDirectory);
    for(const auto* required : {"Content/Gameplay/Inventory.renegade-items",
        "Content/Gameplay/Inventory.renegade-ui",
        "Content/UI/InventoryG4B3B2Grid.renegade-screen",
        "Content/UI/InventoryIcons/potion.png"})
        Need(fs::is_regular_file(shadow/fs::u8path(required)),std::string("Snapshot omitted G5 input: ")+required);
    std::ofstream(root/"combined-snapshot-descriptor.txt")<<snapshot.descriptorPath;
    std::ofstream(marker)<<"Native save/reopen and Test Level snapshot PASS: inventory, river, waterfall, terrain\n";
    std::cout<<"COMBINED_AUTHOR_SAVE_REOPEN_SNAPSHOT=PASS\n";std::cout.flush();
    std::_Exit(0);
 } catch(const std::exception& e) {std::cerr<<"COMBINED_PROOF_FAIL: "<<e.what()<<std::endl;return 1;}
}
