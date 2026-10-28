#include "renegade/bridge/RiverFlowNetworkService.h"
#include <Windows.h>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace renegade::bridge;
using namespace DirectX;
namespace fs = std::filesystem;
static void Check(bool condition, const std::string& reason)
{
    if (!condition) throw std::runtime_error(reason);
}
static wi::ecs::Entity Reach(wi::scene::Scene& scene, const char* id, const std::vector<XMFLOAT3>& points)
{
    const auto root=scene.Entity_CreateTransform(id);
    scene.splines.Create(root);
    auto& m=scene.materials.Create(root);
    m.shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
    m.baseColor={.12f,.24f,.28f,.7f}; m.normalMapStrength=.8f;
    for (const auto p : points) {
        const auto node=scene.Entity_CreateTransform("Control");
        auto* t=scene.transforms.GetComponent(node);
        t->translation_local=p; t->SetDirty(); t->UpdateTransform();
        scene.Component_Attach(node,root);
        scene.splines.GetComponent(root)->spline_node_entities.push_back(node);
    }
    auto* spline=scene.splines.GetComponent(root);
    spline->width=1; spline->mesh_generation_subdivision=24; spline->SetDrawAligned(true);
    return root;
}
static std::vector<RiverFlowNetworkReachInput> Inputs(wi::scene::Scene& scene)
{
    std::vector<RiverFlowNetworkReachInput> result;
    for (const char* id : {"west","east","outlet"})
        { RiverFlowSettings flow; flow.downstreamReversed=std::string(id)=="west";
          result.push_back({id,scene.Entity_FindByName(id),flow}); }
    return result;
}
static std::shared_ptr<const RiverFlowNetwork> Verify(wi::scene::Scene& scene)
{
    std::shared_ptr<const RiverFlowNetwork> result; std::string error;
    Check(BuildRiverFlowNetwork(scene,Inputs(scene),{{"confluence",{"west","east"},"outlet"}},result,error),error);
    double maximum=0, minimumDot=1;
    for (const auto& input : Inputs(scene)) {
        const auto* m=scene.meshes.GetComponent(input.river);
        Check(m && m->vertex_positions.size()>=4, "Actual native generated ribbon missing");
        const auto vertices=m->vertex_positions; const auto indices=m->indices;
        const auto* optics=scene.materials.GetComponent(input.river);
        const auto colour=optics->baseColor; const auto shader=optics->customShaderID;
        const auto world=scene.ComputeEntityMatrixRecursive(input.river);
        const auto* reach=result->FindReach(input.id);
        for (bool first : {true,false}) {
            const bool nativeFirst=first != input.flow.downstreamReversed;
            const auto k=nativeFirst ? std::size_t(0) : vertices.size()-2;
            XMFLOAT3 a,b;
            XMStoreFloat3(&a,XMVector3TransformCoord(XMLoadFloat3(&vertices[k]),world));
            XMStoreFloat3(&b,XMVector3TransformCoord(XMLoadFloat3(&vertices[k+1]),world));
            const auto& port=first ? reach->upstream : reach->downstream;
            auto distance=[](XMFLOAT3 p,XMFLOAT3 q) {
                return std::sqrt(double(p.x-q.x)*(p.x-q.x)+double(p.y-q.y)*(p.y-q.y)+double(p.z-q.z)*(p.z-q.z));
            };
            maximum=std::max({maximum,distance(a,port.rightBank),distance(b,port.leftBank),
                distance({(a.x+b.x)*.5f,(a.y+b.y)*.5f,(a.z+b.z)*.5f},port.centre)});
            Check(std::abs(std::hypot(a.x-b.x,a.z-b.z)-port.widthMeters)<1e-4f, "Native cap width mismatch");
            const auto& tangent=m->vertex_tangents[k];
            XMFLOAT3 direction; XMStoreFloat3(&direction,XMVector3TransformNormal(
                XMVectorSet(tangent.x,tangent.y,tangent.z,0),world));
            const double dot=(direction.x*port.directionXZ.x+direction.z*port.directionXZ.y)/
                std::hypot(direction.x,direction.z)*(input.flow.downstreamReversed ? -1 : 1);
            minimumDot=std::min(minimumDot,dot);
        }
        std::shared_ptr<const RiverFlowNetwork> repeat; std::string again;
        Check(BuildRiverFlowNetwork(scene,Inputs(scene),{{"confluence",{"east","west"},"outlet"}},repeat,again),again);
        Check(repeat->DeterministicHash()==result->DeterministicHash(),"Stable hash");
        Check(vertices.size()==m->vertex_positions.size() && std::memcmp(vertices.data(),m->vertex_positions.data(),vertices.size()*sizeof(XMFLOAT3))==0 && indices==m->indices, "Network mutated native geometry");
        Check(optics->baseColor.x==colour.x && optics->baseColor.w==colour.w &&
            optics->customShaderID==shader && optics->normalMapStrength==.8f, "Network mutated native optics");
    }
    Check(maximum<1e-4 && minimumDot>.995, "Actual native port geometry/flow mismatch");
    std::cout<<"NATIVE_PORT_PARITY=PASS maxMetres="<<maximum<<" minTangentDot="<<minimumDot
        <<" hash="<<result->DeterministicHash()<<"\n";
    return result;
}
int main(int argc,char** argv)
{
    if(argc!=3) return 2;
    std::cout.setf(std::ios::unitbuf);
    const bool reopen=std::string(argv[1])=="--reopen";
    const fs::path path=fs::absolute(argv[2]);
    if(!reopen && fs::exists(path)) return 2;
    HWND window=CreateWindowExW(0,L"STATIC",L"River G7A native geometry oracle",
        WS_OVERLAPPEDWINDOW,0,0,320,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    struct App:wi::Application { void Initialize() override { wi::Application::Initialize(); alwaysactive=true; } } app;
    app.allow_hdr=false; app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    auto* gpu=wi::graphics::GetDevice();
    try {
        wi::scene::Scene scene;
        if(reopen) {
            wi::Archive archive(path.generic_u8string(),true);
            Check(archive.IsOpen(),"Archive open"); scene.Serialize(archive);
        } else {
            Reach(scene,"west",{{0,3,0},{-6,3,-4},{-12,3,-10},{-20,3,-10}});
            Reach(scene,"east",{{-20,3,10},{-12,3,10},{-6,3,4},{0,3,0}});
            Reach(scene,"outlet",{{0,3,0},{8,3,0},{17,3,-3},{25,3,-3}});
            const auto parent=scene.Entity_CreateTransform("Common transformed parent");
            for(const auto& input:Inputs(scene)) scene.Component_Attach(input.river,parent);
            auto* t=scene.transforms.GetComponent(parent);
            t->translation_local={40,5,-30}; t->scale_local={1.5f,1,.8f};
            XMStoreFloat4(&t->rotation_local,XMQuaternionRotationRollPitchYaw(0,.7f,0));
            t->SetDirty();t->UpdateTransform();
        }
        wi::scene::CameraComponent camera; camera.CreatePerspective(320,240,.1f,500,XMConvertToRadians(60));
        camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(
            XMVectorSet(40,30,-45,1),XMVectorSet(40,8,-30,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
        wi::RenderPath3D render;render.scene=&scene;render.camera=&camera;
        render.init(320,240,96);render.ResizeBuffers();
        struct Scope { wi::Application& app; ~Scope() { app.ActivatePath(nullptr);wi::graphics::GetDevice()->WaitForGPU(); } } scope{app};
        app.ActivatePath(&render);
        for(int n=0;n<60;++n) { app.Run(); Sleep(2); }
        while(wi::renderer::IsPipelineCreationActive()) Sleep(10);
        gpu->WaitForGPU();
        const auto result=Verify(scene);
        if(reopen) {
            std::ifstream expected(path.string()+".hash");std::uint64_t hash=0;expected>>hash;
            Check(hash==result->DeterministicHash(),"Fresh process archive graph hash mismatch");
        } else {
            fs::create_directories(path.parent_path());
            { wi::Archive archive(path.generic_u8string(),false);Check(archive.IsOpen(),"Archive write");scene.Serialize(archive); }
            std::ofstream(path.string()+".hash")<<result->DeterministicHash();
            // Move a real endpoint, regenerate Wicked geometry, then require graph refusal.
            const auto west=scene.Entity_FindByName("west");
            const auto control=scene.splines.GetComponent(west)->spline_node_entities.front();
            auto* t=scene.transforms.GetComponent(control);t->translation_local.x+=.2f;t->SetDirty();
            scene.splines.GetComponent(west)->prev_mesh_generation_nodes=-1;
            for(int n=0;n<3;++n) app.Run();
            gpu->WaitForGPU();
            auto preserved=result;std::string reason;
            Check(!BuildRiverFlowNetwork(scene,Inputs(scene),{{"confluence",{"west","east"},"outlet"}},preserved,reason),
                "Moved actual native cap accepted despite gap");
            Check(preserved==result && reason.find("cap centres")!=std::string::npos,reason);
            std::cout<<"NATIVE_EDIT_REFUSAL=PASS "<<reason<<"\n";
        }
        Check(wi::renderer::GetShaderErrorCount()==0,"Native shader errors");
        std::cout<<"RIVER_FLOW_NETWORK_NATIVE="<<(reopen?"FRESH_REOPEN_PASS":"PASS")<<"\n";
        return 0;
    } catch(const std::exception& ex) { std::cerr<<ex.what()<<"\n";gpu->WaitForGPU();return 1; }
}
