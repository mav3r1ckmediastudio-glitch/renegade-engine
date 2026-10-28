#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/RiverFlowFieldService.h"
#include <cstring>
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace DirectX;
namespace fs = std::filesystem;
static void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

static std::shared_ptr<const renegade::bridge::RiverFlowField> VerifyNativeField(
    const wi::scene::Scene& scene, wi::ecs::Entity river, const char* label, bool reverse=false)
{
    using namespace renegade::bridge;
    const auto* mesh=scene.meshes.GetComponent(river);
    Check(mesh && mesh->vertex_positions.size()>=4,"Native generated mesh missing");
    const auto vertices=mesh->vertex_positions;
    const auto indices=mesh->indices;
    const auto* material=scene.materials.GetComponent(river);
    const auto optics=*material;
    RiverFlowSettings settings; settings.downstreamReversed=reverse;
    std::shared_ptr<const RiverFlowField> field;
    std::string error;
    Check(BuildRiverFlowField(scene,river,settings,field,error),error.c_str());
    const auto world=scene.ComputeEntityMatrixRecursive(river);
    Check(field->NativeRibbon().size()*2==vertices.size(),"Native row count parity");
    double maximumCentre=0,maximumBank=0,maximumWidth=0,minimumDot=1;
    for(std::size_t i=0;i<field->NativeRibbon().size();++i) {
        const auto& row=field->NativeRibbon()[i];
        XMFLOAT3 right,left,forward;
        XMStoreFloat3(&right,XMVector3TransformCoord(XMLoadFloat3(&vertices[i*2]),world));
        XMStoreFloat3(&left,XMVector3TransformCoord(XMLoadFloat3(&vertices[i*2+1]),world));
        const auto& tangent=mesh->vertex_tangents[i*2];
        XMStoreFloat3(&forward,XMVector3TransformNormal(XMVectorSet(tangent.x,tangent.y,tangent.z,0),world));
        const XMFLOAT3 centre{(right.x+left.x)*.5f,(right.y+left.y)*.5f,(right.z+left.z)*.5f};
        auto length=[](XMFLOAT3 a,XMFLOAT3 b) {
            return std::sqrt(double(a.x-b.x)*(a.x-b.x)+double(a.y-b.y)*(a.y-b.y)+double(a.z-b.z)*(a.z-b.z));
        };
        maximumCentre=std::max(maximumCentre,length(centre,row.centre));
        maximumBank=std::max({maximumBank,length(right,row.rightBank),length(left,row.leftBank)});
        const auto sample=std::lower_bound(field->Samples().begin(),field->Samples().end(),row.nativeParameter,
            [](const auto& s,float t){return s.nativeParameter<t;});
        Check(sample!=field->Samples().end() && sample->nativeParameter==row.nativeParameter,
            "Exact native row parameter not retained");
        maximumWidth=std::max(maximumWidth,double(std::abs(sample->widthMeters-
            std::hypot(right.x-left.x,right.z-left.z))));
        const double dot=(sample->worldXZUnitTangent.x*forward.x+
            sample->worldXZUnitTangent.y*forward.z)/std::hypot(forward.x,forward.z)*(reverse?-1:1);
        minimumDot=std::min(minimumDot,dot);
        const auto query=field->Evaluate({centre.x,centre.z});
        Check(query.valid && !query.ambiguous,"Actual generated mesh centre not covered by flow");
        if(i>0 && i+1<field->NativeRibbon().size()) {
            const auto bank=field->Evaluate({right.x,right.z});
            if(!bank.valid) std::cerr<<"BANK_QUERY_FAIL label="<<label<<" row="<<i
                <<" x="<<right.x<<" z="<<right.z<<" bankError="<<maximumBank
                <<" ambiguous="<<bank.ambiguous<<"\n";
            Check(bank.valid,"Actual native bank not included");
            Check(std::abs(bank.lateralFraction-(reverse?-1.f:1.f))<.002f,"Native bank lateral fraction");
            Check(bank.distanceToBankMeters<.0001f,"Native bank distance");
        }
    }
    Check(maximumCentre<.0001 && maximumBank<.0001 && maximumWidth<.0001,
        "Generated native mesh/flow geometry mismatch");
    Check(minimumDot>.995,"Generated native tangent/flow orientation mismatch");
    Check(vertices.size()==mesh->vertex_positions.size() &&
        std::memcmp(vertices.data(),mesh->vertex_positions.data(),vertices.size()*sizeof(XMFLOAT3))==0 && indices==mesh->indices,"CPU field mutated native geometry");
    Check(material->baseColor.x==optics.baseColor.x && material->baseColor.w==optics.baseColor.w &&
        material->normalMapStrength==optics.normalMapStrength && material->texMulAdd.x==optics.texMulAdd.x &&
        material->IsCustomShader()==optics.IsCustomShader() &&
        material->userdata.x==optics.userdata.x,"CPU field changed native material");
    std::cout<<"NATIVE_MESH_PARITY="<<label<<" centreError="<<maximumCentre
        <<" bankError="<<maximumBank<<" widthError="<<maximumWidth<<" minTangentDot="<<minimumDot
        <<" rows="<<field->NativeRibbon().size()<<" samples="<<field->Samples().size()
        <<" hash="<<field->DeterministicHash()<<"\n";
    return field;
}

static void NativeGeometryCases()
{
    // The oracle is Scene::Update's ACTUAL RunSplineUpdateSystem output, including
    // native backturn correction and published vertex positions/tangents.
    for (int variant=0;variant<5;++variant) {
        wi::scene::Scene scene;
        const auto root=scene.Entity_CreateTransform("Parity River");
        scene.splines.Create(root);
        scene.materials.Create(root).shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
        const float side=variant==1?-1.f:1.f;
        const XMFLOAT3 positions[]={{0,0,0},{10,0,0},{20,0,10*side},{20,0,25*side}};
        for(int i=0;i<4;++i) {
            const auto node=scene.Entity_CreateTransform("Native Parity Node");
            auto* transform=scene.transforms.GetComponent(node);
            transform->translation_local=positions[i]; transform->SetDirty();transform->UpdateTransform();
            scene.Component_Attach(node,root);
            scene.splines.GetComponent(root)->spline_node_entities.push_back(node);
            if(variant==3) scene.transforms.GetComponent(node)->scale_local.x=1+i*.15f;
        }
        auto* spline=scene.splines.GetComponent(root);
        spline->width=.1f;spline->mesh_generation_subdivision=24;spline->SetDrawAligned(true);
        if(variant==2) {
            const auto parent=scene.Entity_CreateTransform("Native Parity Parent");
            scene.Component_Attach(root,parent);
            auto* r=scene.transforms.GetComponent(root);
            r->scale_local={1.7f,1,.8f}; r->SetDirty();
            auto* p=scene.transforms.GetComponent(parent);
            p->translation_local={20,7,-30};
            XMStoreFloat4(&p->rotation_local,XMQuaternionRotationRollPitchYaw(.05f,.7f,0));
            p->SetDirty();
        }
        if(variant==4) {
            const auto n=spline->spline_node_entities[1];
            scene.transforms.GetComponent(n)->translation_local={10,0,6};
            scene.transforms.GetComponent(n)->SetDirty();
        }
        scene.Update(0);wi::graphics::GetDevice()->WaitForGPU();
        const char* labels[]={"right-90","left-90","parent-nonuniform","width-profile","edited-control"};
        VerifyNativeField(scene,root,labels[variant]);
        VerifyNativeField(scene,root,labels[variant],true);
    }
    // A sharp editing failure must be refused when native backturn clamping
    // collapses banks, even though the analytic curve itself remains finite.
    wi::scene::Scene folded;
    const auto root=folded.Entity_CreateTransform("Folded native river");
    folded.splines.Create(root);
    folded.materials.Create(root).shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
    for(const XMFLOAT3 p: {XMFLOAT3{0,0,0},XMFLOAT3{10,0,0},XMFLOAT3{.1f,0,.1f},XMFLOAT3{10,0,.2f}}) {
        const auto node=folded.Entity_CreateTransform("Folded control");
        auto* t=folded.transforms.GetComponent(node);
        t->translation_local=p;t->SetDirty();t->UpdateTransform();
        folded.Component_Attach(node,root);
        folded.splines.GetComponent(root)->spline_node_entities.push_back(node);
    }
    auto* s=folded.splines.GetComponent(root);
    s->width=2;s->mesh_generation_subdivision=24;s->SetDrawAligned(true);
    folded.Update(0);wi::graphics::GetDevice()->WaitForGPU();
    std::size_t collapsed=0;
    const auto* m=folded.meshes.GetComponent(root);
    for(std::size_t i=3;i<m->vertex_positions.size();i+=2) {
        const auto& a=m->vertex_positions[i-2];const auto& b=m->vertex_positions[i];
        if(std::hypot(a.x-b.x,a.z-b.z)<.000001f)++collapsed;
    }
    std::shared_ptr<const renegade::bridge::RiverFlowField> refused;
    std::string error;
    Check(!renegade::bridge::BuildRiverFlowField(folded,root,{},refused,error),
        "Folded native geometry must not produce a valid field");
    Check(collapsed>0,"Expected actual native backturn correction was not reproduced");
    std::cout<<"NATIVE_BACKTURN_REFUSAL=PASS collapsedBankEdges="<<collapsed<<" error="<<error<<"\n";
}

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    std::cout.setf(std::ios::unitbuf);
    const fs::path out = fs::absolute(argv[1]);
    if (fs::exists(out) && !fs::is_empty(out)) return 2;
    fs::create_directories(out);
    HWND window = CreateWindowExW(0,L"STATIC",L"River V2 native baseline",
        WS_OVERLAPPEDWINDOW,0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    struct App : wi::Application { void Initialize() override {
        wi::Application::Initialize(); alwaysactive = true; } } app;
    app.allow_hdr = false; app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    auto* gpu = wi::graphics::GetDevice();
    struct Drain { ~Drain() { while (wi::renderer::IsPipelineCreationActive()) Sleep(10);
        wi::graphics::GetDevice()->WaitForGPU(); } } drain;
    try {
        wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
        renegade::bridge::SceneService service;
        Check(service.LoadScene(fs::absolute(argv[2]).generic_u8string()), service.LastError().c_str());
        auto& scene = service.GetScene();
        Check(scene.terrains.GetCount() == 1, "Native terrain fixture missing");
        for (const char* name : {"Rock Foam Contact - Test","Rock Foam Wake - Test"})
            if (const auto e = scene.Entity_FindByName(name); e != wi::ecs::INVALID_ENTITY)
                scene.Entity_Remove(e);
        const auto river = scene.Entity_FindByName("River Water 5");
        auto* spline = scene.splines.GetComponent(river);
        Check(spline && spline->spline_node_entities.size()>=2,"River controls missing");
        // A fresh curved disposable copy, without changing the source file.
        for (const auto index : {std::size_t(0), spline->spline_node_entities.size()-1}) {
            auto* node = scene.transforms.GetComponent(spline->spline_node_entities[index]);
            node->translation_local.z += 2.5f; node->SetDirty();
        }
        spline->prev_mesh_generation_nodes = -1;
        const auto* water = scene.materials.GetComponent(river);
        Check(spline && water && water->shaderType == wi::scene::MaterialComponent::SHADERTYPE_WATER
            && !water->IsCustomShader(), "Native flat WATER required");
        Check(water->textures[wi::scene::MaterialComponent::NORMALMAP].resource.GetTexture().IsValid(),
            "Native normal texture missing");
        std::cout << "NATIVE_OPTICS rgb=" << water->baseColor.x << "," << water->baseColor.y
            << "," << water->baseColor.z << " depthHaze=" << water->baseColor.w
            << " normal=" << water->normalMapStrength << " refraction=" << water->refraction
            << " customShader=0 nodes=" << spline->spline_node_entities.size() << "\n";
        for (auto node : spline->spline_node_entities) {
            XMFLOAT3 p; XMStoreFloat3(&p,scene.ComputeEntityMatrixRecursive(node).r[3]);
            std::cout << "NATIVE_NODE=" << p.x << "," << p.y << "," << p.z << "\n";
        }
        wi::scene::CameraComponent camera;
        camera.CreatePerspective(944,601,.05f,1000,XMConvertToRadians(60));
        wi::RenderPath3D render; render.scene = &scene; render.camera = &camera;
        render.init(944,601,96); render.setMotionBlurEnabled(false);
        render.setDepthOfFieldEnabled(false); render.setOcclusionCullingEnabled(false);
        render.ResizeBuffers();
        struct Guard { wi::Application& app; ~Guard() { app.ActivatePath(nullptr); } } guard{app};
        wi::renderer::SetTemporalAAEnabled(false);
        app.ActivatePath(&render);
        auto pose = [&](bool high) {
            const auto eye = high ? XMVectorSet(0,28,48,1) : XMVectorSet(0,1.2f,64,1);
            camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(
                eye,XMVectorSet(0,.15f,68,1),XMVectorSet(0,1,0,0)))); camera.UpdateCamera();
        };
        auto capture = [&](const std::string& name) {
            gpu->WaitForGPU();
            Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
                (out/(name+".png")).generic_u8string()),"Capture failed");
        };
        pose(false);
        for (int i=0;i<100;++i) { app.Run(); Sleep(4); }
        while (wi::renderer::IsPipelineCreationActive()) Sleep(10);
        capture("native-waterline");
        const auto start = std::chrono::steady_clock::now();
        for (int i=0;i<60;++i) app.Run();
        const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::cout << "NATIVE_FPS_UNPACED=" << 60.0/seconds << "\n";
        // Thirty samples span three wall-clock seconds; native normal animation remains enabled.
        const auto animationStart = std::chrono::steady_clock::now();
        for (int i=0;i<30;++i) {
            do { app.Run(); Sleep(2); } while(std::chrono::duration<double>(
                std::chrono::steady_clock::now()-animationStart).count() < i*.1);
            capture("native-frame-"+std::to_string(i));
        }
        pose(true); for (int i=0;i<20;++i) app.Run(); capture("native-overhead");
        // Build only CPU data between two readbacks of the same native frame.
        capture("cpu-build-before");
        const auto field=VerifyNativeField(scene,river,"real-flat-river");
        capture("cpu-build-after");
        const auto saved = out/"native-baseline.wiscene";
        { wi::Archive archive(saved.generic_u8string(),false); scene.Serialize(archive); }
        { wi::Archive archive(saved.generic_u8string(),true); wi::scene::Scene fresh; fresh.Serialize(archive);
            const auto* m = fresh.materials.GetComponent(fresh.Entity_FindByName("River Water 5"));
            Check(m && !m->IsCustomShader() && m->shaderType == water->shaderType &&
                m->baseColor.x == water->baseColor.x && m->baseColor.w == water->baseColor.w,
                "Native archive optics changed");
            std::shared_ptr<const renegade::bridge::RiverFlowField> rebuilt; std::string error;
            Check(renegade::bridge::BuildRiverFlowField(fresh,fresh.Entity_FindByName("River Water 5"),{},rebuilt,error),error.c_str());
            Check(rebuilt->DeterministicHash()==field->DeterministicHash(),"Cold archive flow rebuild hash changed");
        }
        Check(wi::renderer::GetShaderErrorCount()==0,"Shader errors");
        NativeGeometryCases();
        std::cout << "G0_G1_NATIVE_BASELINE_AND_GEOMETRY=PASS shader_errors=0 native_archive=PASS\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << "\n"; return 1; }
}
