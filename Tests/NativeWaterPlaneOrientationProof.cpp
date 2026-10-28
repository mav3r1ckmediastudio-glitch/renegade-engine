#include "renegade/bridge/RiverSplineService.h"
#include <Windows.h>
#include <cmath>
#include <cfloat>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace DirectX;
using namespace renegade::bridge;
namespace fs=std::filesystem;

static void Require(bool ok,const char* what)
{
    if(!ok) throw std::runtime_error(what);
}
static void SetPose(wi::scene::Scene& scene,wi::ecs::Entity entity,
                    float pitchDegrees,float y)
{
    auto* t=scene.transforms.GetComponent(entity);
    Require(t!=nullptr,"missing plane transform");
    t->translation_local={0,y,0};
    const XMVECTOR q=XMQuaternionRotationRollPitchYaw(
        XMConvertToRadians(pitchDegrees),0,0);
    XMStoreFloat4(&t->rotation_local,q);
    t->scale_local={7,1,24};
    t->SetDirty();
}
int main(int argc,char** argv)
{
    const bool inspectOnly=(argc==3 || argc==4) && std::string(argv[1])=="inspect";
    const bool captureInspect=inspectOnly && argc==4;
    if(argc!=2 && !inspectOnly) {std::cerr<<"Usage: NativeWaterPlaneOrientationProof output-folder OR inspect path-to-WISCENE [empty-capture-folder]\n";return 2;}
    const fs::path out=inspectOnly?(captureInspect?fs::absolute(fs::u8path(argv[3])):fs::path{}):fs::absolute(fs::u8path(argv[1]));
    if((!inspectOnly || captureInspect) && fs::exists(out) && !fs::is_empty(out))
    {
        std::cerr<<"Only a new or empty output folder is permitted\n";
        return 2;
    }
    if(!inspectOnly || captureInspect) fs::create_directories(out);
    HWND window=CreateWindowExW(0,L"STATIC",L"Native Wicked Water Angle Proof",
        WS_OVERLAPPEDWINDOW,0,0,960,640,nullptr,nullptr,
        GetModuleHandleW(nullptr),nullptr);
    std::cout.setf(std::ios::unitbuf);
    wi::Application app;
    app.allow_hdr=false;
    app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    auto* gpu=wi::graphics::GetDevice();
    struct GPUDrain
    {
        ~GPUDrain()
        {
            while(wi::renderer::IsPipelineCreationActive()) Sleep(10);
            wi::graphics::GetDevice()->WaitForGPU();
        }
    } drain;
    try
    {
        wi::scene::Scene scene;
        if(inspectOnly)
        {
            const fs::path saved=fs::absolute(fs::u8path(argv[2]));
            wi::Archive disk(saved.generic_u8string(),true,false);
            Require(disk.IsOpen(),"read-only owner WISCENE could not open");
            scene.Serialize(disk);
            scene.Update(0.f);
            const auto river=scene.Entity_FindByName("River Water");
            Require(river!=wi::ecs::INVALID_ENTITY &&
                scene.splines.Contains(river),"reference scene has no named River Water");
            auto* spline=scene.splines.GetComponent(river);
            std::cout<<"OWNER_SCENE_INSPECT_READ_ONLY=1"
                <<" SplinePoints="<<spline->spline_node_entities.size()<<"\n";
            XMFLOAT3 first{},last{};
            float minY=FLT_MAX,maxY=-FLT_MAX;
            for(size_t i=0;i<spline->spline_node_entities.size();++i)
            {
                const auto* t=scene.transforms.GetComponent(
                    spline->spline_node_entities[i]);
                Require(t!=nullptr,"owner spline child control transform missing");
                XMFLOAT3 world;
                XMStoreFloat3(&world,XMVector3TransformCoord(
                    XMVectorZero(),t->GetWorldMatrix()));
                if(i==0)first=world;
                last=world;
                minY=std::min(minY,world.y);
                maxY=std::max(maxY,world.y);
                std::cout<<"OWNER_NODE_"<<i<<"="<<world.x<<","
                    <<world.y<<","<<world.z<<"\n";
            }
            std::cout<<"OWNER_RIVER_ENDPOINT_DROP_M="<<first.y-last.y
                <<" WORLD_HEIGHT_RANGE_M="<<maxY-minY<<"\n";
            const auto* root=scene.transforms.GetComponent(river);
            const auto* material=scene.materials.GetComponent(river);
            int extraChildren=0;
            for(size_t i=0;i<scene.hierarchy.GetCount();++i)
                if(scene.hierarchy[i].parentID==river &&
                    std::find(spline->spline_node_entities.begin(),
                        spline->spline_node_entities.end(),
                        scene.hierarchy.GetEntity(i))==
                            spline->spline_node_entities.end()) ++extraChildren;
            std::cout<<"OWNER_ELIGIBILITY_COMPONENTS root="
                <<(root!=nullptr)<<" rootParent="<<scene.hierarchy.Contains(river)
                <<" rootScale="<<(root?root->scale_local.x:0)<<","
                <<(root?root->scale_local.y:0)<<","
                <<(root?root->scale_local.z:0)
                <<" rootRotation="<<(root?root->rotation_local.x:0)<<","
                <<(root?root->rotation_local.y:0)<<","
                <<(root?root->rotation_local.z:0)<<","
                <<(root?root->rotation_local.w:0)
                <<" material="<<(material!=nullptr)
                <<" native="<<(material && material->shaderType==
                    wi::scene::MaterialComponent::SHADERTYPE_WATER)
                <<" custom="<<(material && material->IsCustomShader())
                <<" terrain_modifier_amount="<<spline->terrain_modifier_amount
                <<" looped="<<spline->IsLooped()
                <<" extraChildren="<<extraChildren<<"\n";
            Require(BindRiverSurfaceShader(scene,river),
                "normal Studio native WATER adoption cleanup failed");
            std::cout<<"OWNER_NATIVE_WATER_POST_ADOPTION custom="
                <<scene.materials.GetComponent(river)->IsCustomShader()<<"\n";
            if(captureInspect)
            {
                for(size_t i=0;i<scene.terrains.GetCount();++i)
                {
                    scene.terrains[i].scene=&scene;
                    scene.terrains[i].terrainEntity=scene.terrains.GetEntity(i);
                }
                wi::scene::CameraComponent ownerCam;
                ownerCam.CreatePerspective(960,640,.1f,1000.f,
                    XMConvertToRadians(60.f));
                wi::RenderPath3D path;
                path.scene=&scene;
                path.camera=&ownerCam;
                path.init(960,640,96);
                path.setMotionBlurEnabled(false);
                path.setOcclusionCullingEnabled(false);
                path.setDepthOfFieldEnabled(false);
                path.ResizeBuffers();
                struct ClearPath
                {
                    wi::Application& app;
                    ~ClearPath(){if(app.GetActivePath())app.ActivatePath(nullptr);}
                } clearPath{app};
                wi::renderer::SetTemporalAAEnabled(false);
                wi::renderer::SetGameSpeed(0);
                app.ActivatePath(&path);
                auto setCamera=[&](int cameraKind)
                {
                    const XMVECTOR eye=cameraKind==0
                        ?XMVectorSet(0,58,-42,1)
                        :XMVectorSet(0,76,-14,1);
                    const XMVECTOR focus=XMVectorSet(0,28,0,1);
                    ownerCam.TransformCamera(XMMatrixInverse(nullptr,
                        XMMatrixLookAtLH(eye,focus,
                            XMVectorSet(0,1,0,0))));
                    ownerCam.UpdateCamera();
                };
                const auto shoot=[&](const char* tag)
                {
                    for(int view=0;view<2;++view)
                    {
                        setCamera(view);
                        for(int frame=0;frame<36;++frame){app.Run();Sleep(2);}
                        gpu->WaitForGPU();
                        const auto file=(out/(std::string(tag)+
                            (view==0?"-side.png":"-overhead.png"))).generic_u8string();
                        Require(wi::helper::saveTextureToFile(
                            path.GetRenderResult3D(),file),
                            "owner read-only native GPU capture failed");
                        std::cout<<"SAVED_MOUNTAIN_DX12_CAPTURE "<<file
                            <<" REFLECTION_PLANE="
                            <<path.visibility_main.reflectionPlane.x<<","
                            <<path.visibility_main.reflectionPlane.y<<","
                            <<path.visibility_main.reflectionPlane.z<<","
                            <<path.visibility_main.reflectionPlane.w<<"\n";
                    }
                };
                std::vector<XMFLOAT3> originalOwnerPoints;
                for(const auto id:spline->spline_node_entities)
                {
                    XMFLOAT3 point;
                    XMStoreFloat3(&point,XMVector3TransformCoord(
                        XMVectorZero(),
                        scene.transforms.GetComponent(id)->GetWorldMatrix()));
                    originalOwnerPoints.push_back(point);
                }
                const auto checkOwnerPoints=[&](const char* stage)
                {
                    float worst=0;
                    for(size_t i=0;i<originalOwnerPoints.size();++i)
                    {
                        XMFLOAT3 now;
                        XMStoreFloat3(&now,XMVector3TransformCoord(
                            XMVectorZero(),
                            scene.transforms.GetComponent(
                                spline->spline_node_entities[i])->GetWorldMatrix()));
                        const auto& before=originalOwnerPoints[i];
                        worst=std::max(worst,std::sqrt(
                            (now.x-before.x)*(now.x-before.x)+
                            (now.y-before.y)*(now.y-before.y)+
                            (now.z-before.z)*(now.z-before.z)));
                    }
                    Require(worst<.002f,"actual saved mountain control world shift");
                    std::cout<<"OWNER_75_POINT_WORLD_PARITY_"<<stage
                        <<" MAX_SHIFT_M="<<worst<<"\n";
                };
                shoot("saved-mountain-original-native");
                CommandService proofCommand;
                Require(proofCommand.Execute(std::make_unique<
                    OrientNearPlanarRiverWaterPlaneCommand>(scene,river)),
                    "saved mountain near-planar orientation rejected");
                checkOwnerPoints("AFTER");
                shoot("saved-mountain-tilted-root-native");
                Require(proofCommand.Undo(),"saved mountain frame undo failed");
                checkOwnerPoints("UNDO");
                shoot("saved-mountain-undo-native");
                std::cout<<"READONLY_OWNER_NATIVE_WATER_BA_Undo=PASS"
                    <<" FILE_WRITE=NEVER\n";
                return 0;
            }
            CommandService trial;
            const bool suitable=trial.Execute(
                std::make_unique<OrientNearPlanarRiverWaterPlaneCommand>(scene,river));
            if(suitable)Require(trial.Undo(),"in-memory eligibility undo");
            std::cout<<"OWNER_SAVED_RIVER_NEAR_PLANAR_ELIGIBLE="
                <<suitable<<" FILE_WRITE=NEVER\n";
            return 0;
        }
        const std::vector<XMFLOAT3> points={
            {-3,0,-15},{0,0,-5},{0,0,5},{3,0,15}};
        RiverSplineOptions options;
        options.widthMeters=14.f;
        options.surfaceOffsetMeters=0.f;
        auto factory=std::make_unique<CreateRiverSplineCommand>(scene,points,options);
        auto* command=factory.get();
        CommandService history;
        Require(history.Execute(std::move(factory)),"stock native WATER factory");
        const auto prototype=command->CreatedEntity();
        Require(prototype!=wi::ecs::INVALID_ENTITY,"stock river entity");
        const auto* stockMaterial=scene.materials.GetComponent(prototype);
        Require(stockMaterial!=nullptr && !stockMaterial->IsCustomShader() &&
            stockMaterial->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER,
            "the stock river must use genuine native Wicked WATER");
        // Creating another material may reallocate Wicked ECS component storage.
        // Snapshot the factory material BEFORE adding plane or riverbed.
        const wi::scene::MaterialComponent stock=*stockMaterial;
        const auto plane=scene.Entity_CreatePlane("Identical WATER Plane");
        auto* material=scene.materials.GetComponent(plane);
        Require(material!=nullptr,"WATER plane material");
        *material=stock;
        // Native river ribbon UVs are metric. For a 14x48 metre *rectangular*
        // plane, set U/V so both axes have the SAME 4m normal-map tile scale.
        material->texMulAdd={14.f/4.f,48.f/4.f,0,0};
        material->texAnimFrameRate=0; // frozen identical UV phase for each capture
        material->SetDirty();
        Require(material->textures[wi::scene::MaterialComponent::NORMALMAP]
            .resource.GetTexture().IsValid(),"stock water normal texture");
        // The spline render object is generated asynchronously on Update.
        // Dispose of the material-only factory before any frame is rendered,
        // leaving exactly ONE WATER object for reflection selection.
        scene.Entity_Remove(prototype,true);
        const auto riverbed=scene.Entity_CreatePlane("Constant 40cm Underlay");
        auto* sediment=scene.materials.GetComponent(riverbed);
        sediment->SetBaseColor({0.28f,0.25f,0.19f,1});
        sediment->SetRoughness(.95f);
        sediment->SetDirty();
        // ECS component storage may relocate after adding the riverbed.
        material=scene.materials.GetComponent(plane);
        Require(material!=nullptr,"water material relocated");
        SetPose(scene,plane,0,24);
        SetPose(scene,riverbed,0,23.6f);
        auto weatherEntity=scene.Entity_CreateTransform("Consistent Daylight");
        auto& weather=scene.weathers.Create(weatherEntity);
        weather.ambient={.08f,.08f,.08f};
        weather.horizon={.5f,.67f,.86f};
        weather.zenith={.18f,.32f,.64f};
        const auto sun=scene.Entity_CreateLight("Fixed Sun",
            {0,80,-20},{1,.95f,.85f},5,200,
            wi::scene::LightComponent::DIRECTIONAL);
        scene.transforms.GetComponent(sun)->RotateRollPitchYaw(
            XMFLOAT3(1.35f,-.78f,0));
        wi::scene::CameraComponent camera;
        camera.CreatePerspective(960,640,.1f,500.f,XMConvertToRadians(60));
        wi::RenderPath3D render;
        render.scene=&scene;
        render.camera=&camera;
        render.init(960,640,96);
        render.setMotionBlurEnabled(false);
        render.setOcclusionCullingEnabled(false);
        render.setDepthOfFieldEnabled(false);
        render.ResizeBuffers();
        struct PathGuard
        {
            wi::Application& app;
            ~PathGuard()
            {
                if(app.GetActivePath()!=nullptr)app.ActivatePath(nullptr);
            }
        } pathGuard{app};
        wi::renderer::SetTemporalAAEnabled(false);
        app.ActivatePath(&render);
        const auto poseCamera=[&](float waterY,bool highView)
        {
            const XMVECTOR eye=XMVectorSet(0,waterY+(highView ? 75.f : 16.f),-52,1);
            const XMVECTOR look=XMVectorSet(0,waterY,0,1);
            camera.TransformCamera(XMMatrixInverse(nullptr,
                XMMatrixLookAtLH(eye,look,XMVectorSet(0,1,0,0))));
            camera.UpdateCamera();
        };
        poseCamera(24,false);
        for(int i=0;i<56;++i){app.Run();Sleep(3);}
        int wait=0;
        while(wi::renderer::IsPipelineCreationActive() && wait++<800)Sleep(25);
        Require(!wi::renderer::IsPipelineCreationActive(),"native WATER pipelines pending");
        wi::renderer::SetGameSpeed(0);
        const float normalStrength=stock.normalMapStrength;
        const auto take=[&](const std::string& tag)
        {
            for(int frame=0;frame<12;++frame){app.Run();Sleep(2);}
            gpu->WaitForGPU();
            const fs::path image=out/(tag+".png");
            Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
                image.generic_u8string()),"DX12 capture failed");
        };
        char murkProbe[8]{};
        if(GetEnvironmentVariableA("RENEGADE_NATIVE_MURK_PROOF",
            murkProbe,sizeof(murkProbe))==1 && murkProbe[0]=='1')
        {
            // No shader or material change beyond the existing Inspector
            // WATER CLARITY parameter; capture two physically fixed depths.
            Require(!material->IsCustomShader(),"murk proof must use native WATER");
            const auto stockColor=material->baseColor;
            const auto origStrength=material->normalMapStrength;
            const auto setCam=[&](bool isCloseCamera)
            {
                const XMVECTOR eye=isCloseCamera
                    ? XMVectorSet(0,27,-10,1)
                    : XMVectorSet(0,40,-52,1);
                const XMVECTOR look=XMVectorSet(0,24,0,1);
                camera.TransformCamera(XMMatrixInverse(nullptr,
                    XMMatrixLookAtLH(eye,look,XMVectorSet(0,1,0,0))));
                camera.UpdateCamera();
            };
            for(const auto depth:{.15f,.4f,1.35f})
            {
                SetPose(scene,riverbed,0,24-depth);
                for(const bool isCloseCamera:{false,true})
                {
                    setCam(isCloseCamera);
                    const std::string stem="depth-"+std::to_string(int(depth*100))+
                        (isCloseCamera?"-near":"-far");
                    scene.objects.GetComponent(plane)->SetRenderable(false);
                    take(stem+"-underlay");
                    scene.objects.GetComponent(plane)->SetRenderable(true);
                    for(const float fog:{.126f,.947f,1.f,3.f,8.f,16.f})
                    {
                        auto colour=stockColor;
                        colour.w=fog;
                        material->SetBaseColor(colour);
                        material->SetDirty();
                        take(stem+"-fog-"+std::to_string(int(fog*1000)));
                        std::cout<<"NATIVE_MURK_CAPTURE="<<stem
                            <<" alpha="<<fog
                            <<" formulaTransmission="<<std::exp(-depth*fog)
                            <<" reflectionPlane="<<render.visibility_main.reflectionPlane.x
                            <<","<<render.visibility_main.reflectionPlane.y<<","
                            <<render.visibility_main.reflectionPlane.z<<","
                            <<render.visibility_main.reflectionPlane.w<<"\n";
                    }
                }
            }
            material->SetBaseColor(stockColor);
            material->SetNormalMapStrength(origStrength);
            material->SetDirty();
            std::cout<<"NATIVE_WATER_MURK_REFRACTION_PROOF=PASS\n";
            return 0;
        }
        const auto sample=[&](const std::string& tag,float pitch,float waterY,bool highView)
        {
            SetPose(scene,plane,pitch,waterY);
            SetPose(scene,riverbed,pitch,waterY-.4f);
            poseCamera(waterY,highView);
            material->SetNormalMapStrength(normalStrength);
            material->SetDirty();
            scene.objects.GetComponent(plane)->SetRenderable(true);
            take(tag+"-normal");
            material->SetNormalMapStrength(0.f);
            material->SetDirty();
            take(tag+"-flat-normal");
            material->SetNormalMapStrength(normalStrength);
            material->SetDirty();
            scene.objects.GetComponent(plane)->SetRenderable(false);
            take(tag+"-underlay-only");
            scene.objects.GetComponent(plane)->SetRenderable(true);
            const float pitchRadians=XMConvertToRadians(pitch);
            const float facing=std::cos(pitchRadians)*(highView?75.f:16.f)
                -std::sin(pitchRadians)*52.f;
            std::cout<<"PROOF "<<tag<<" pitch="<<pitch<<" y="<<waterY
                <<" faceCameraDot="<<facing<<" isBackFacing="<<(facing<0.f)
                <<" normalStrength="<<normalStrength<<" opacity="
                <<material->baseColor.w<<" nativeWater="
                <<(material->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER)
                <<" customShader="<<material->IsCustomShader()
                <<" reflectionPlane="<<render.visibility_main.reflectionPlane.x
                <<","<<render.visibility_main.reflectionPlane.y<<","
                <<render.visibility_main.reflectionPlane.z<<","
                <<render.visibility_main.reflectionPlane.w<<"\n";
        };
        const bool fastSplineOnly=GetEnvironmentVariableA(
            "RENEGADE_PROOF_SPLINE_ONLY",nullptr,0)>0;
        if(!fastSplineOnly)
        {
            for(const int pitch:{0,10,20,30,45})
            {
                sample("angle-"+std::to_string(pitch)+"-low",float(pitch),24,false);
                sample("angle-"+std::to_string(pitch)+"-high",float(pitch),24,true);
            }
            for(const int elev:{0,12,48,80})
                sample("height-"+std::to_string(elev),0,float(elev),false);
        }
        // Fourth control: native spline with exactly the same nominal 30°
        // channel grade as the 30° rigid plane, using the same Wicked WATER.
        // This checks spline-generated vertex normals against REAL triangles.
        scene.objects.GetComponent(plane)->SetRenderable(false);
        scene.Entity_Remove(plane,true); // hidden WATER still requests reflections!
        const float radians=XMConvertToRadians(30.f);
        const float halfZ=24.f*std::cos(radians);
        const float ySpan=24.f*std::sin(radians);
        std::vector<XMFLOAT3> slopePoints={
            {0,24.f+ySpan,-halfZ},
            {0,24.f+ySpan*.5f,-halfZ*.5f},
            {0,24.f,0},
            {0,24.f-ySpan*.5f,halfZ*.5f},
            {0,24.f-ySpan,halfZ},
        };
        auto newSpline=std::make_unique<CreateRiverSplineCommand>(
            scene,slopePoints,options);
        auto* splineCommand=newSpline.get();
        Require(history.Execute(std::move(newSpline)),"create 30deg spline proof");
        const auto slopeEntity=splineCommand->CreatedEntity();
        SetPose(scene,riverbed,30.f,23.6f);
        poseCamera(24,true);
        for(int i=0;i<28;++i){app.Run();Sleep(3);}
        auto* slopeWater=scene.materials.GetComponent(slopeEntity);
        const auto* slopeMesh=scene.meshes.GetComponent(slopeEntity);
        Require(slopeWater && slopeMesh && slopeMesh->vertex_positions.size()>6,
            "native 30deg spline mesh failed to generate");
        Require(slopeWater->shaderType==
            wi::scene::MaterialComponent::SHADERTYPE_WATER &&
            !slopeWater->IsCustomShader(),"30deg spline uses nonnative WATER");
        const float expectedX=0.f,expectedY=std::cos(radians),
            expectedZ=std::sin(radians);
        double storedToPlaneDot=0,storedToMeshDot=0;
        int measured=0;
        for(size_t j=0;j+5<slopeMesh->vertex_positions.size();j+=2)
        {
            const auto& A=slopeMesh->vertex_positions[j];
            const auto& B=slopeMesh->vertex_positions[j+1];
            const auto& C=slopeMesh->vertex_positions[j+2];
            const auto& N=slopeMesh->vertex_normals[j];
            const XMVECTOR va=XMLoadFloat3(&A);
            const XMVECTOR vb=XMLoadFloat3(&B);
            const XMVECTOR vc=XMLoadFloat3(&C);
            const XMVECTOR cross=XMVector3Normalize(
                XMVector3Cross(vb-va,vc-va));
            const XMVECTOR ns=XMLoadFloat3(&N);
            storedToPlaneDot+=N.x*expectedX+N.y*expectedY+N.z*expectedZ;
            storedToMeshDot+=std::abs(XMVectorGetX(
                XMVector3Dot(cross,ns)));
            ++measured;
        }
        Require(measured>0,"no native 30deg spline triangles measured");
        std::cout<<"SPLINE30_STORED_NORMAL_DOT_RIGID30_NORMAL="
            <<storedToPlaneDot/measured
            <<" SPLINE30_STORED_NORMAL_DOT_ACTUAL_FACE_ABS="
            <<storedToMeshDot/measured
            <<" SAMPLED_SEGMENTS="<<measured
            <<" TRIANGLES="<<slopeMesh->indices.size()/3
            <<" SPLINE_NORMALMAP_STRENGTH="<<slopeWater->normalMapStrength
            <<"\n";
        slopeWater->texAnimFrameRate=0;
        slopeWater->SetNormalMapStrength(normalStrength);
        take("spline-30-high-normal");
        std::cout<<"SPLINE30_REFLECTION_PLANE="
            <<render.visibility_main.reflectionPlane.x<<","
            <<render.visibility_main.reflectionPlane.y<<","
            <<render.visibility_main.reflectionPlane.z<<","
            <<render.visibility_main.reflectionPlane.w
            <<" vs RIGID30_REFLECTION_PLANE=0,0.866025,0.5,-20.7846"
            <<" SPLINE_ROOT_ROTATION="
            <<scene.transforms.GetComponent(slopeEntity)->rotation_local.x<<","
            <<scene.transforms.GetComponent(slopeEntity)->rotation_local.y<<","
            <<scene.transforms.GetComponent(slopeEntity)->rotation_local.z<<","
            <<scene.transforms.GetComponent(slopeEntity)->rotation_local.w<<"\n";
        slopeWater->SetNormalMapStrength(0);
        take("spline-30-high-flat-normal");
        slopeWater->SetNormalMapStrength(normalStrength);
        scene.objects.GetComponent(slopeEntity)->SetRenderable(false);
        take("spline-30-high-underlay-only");
        // Re-orient the already-authored WORLD-HEIGHT spline in-place,
        // preserving all control-point world coordinates and entity identities.
        // Critical for avoiding terrain bed movement when converting a
        // previously saved sloped river into a root-tilted native WATER reach.
        const auto sourceNodeIds=scene.splines.GetComponent(slopeEntity)->
            spline_node_entities;
        Require(sourceNodeIds.size()==5,"source node IDs missing");
        std::vector<XMFLOAT3> worldNodesBefore;
        worldNodesBefore.reserve(sourceNodeIds.size());
        for(const auto nodeId:sourceNodeIds)
        {
            const auto* t=scene.transforms.GetComponent(nodeId);
            Require(t!=nullptr,"source spline node transform missing");
            XMFLOAT3 pos;
            XMStoreFloat3(&pos,XMVector3TransformCoord(
                XMVectorZero(),t->GetWorldMatrix()));
            worldNodesBefore.push_back(pos);
        }
        const auto originalMesh=*scene.meshes.GetComponent(slopeEntity);
        std::vector<XMFLOAT3> originalWorldVertices;
        for(const auto& v:originalMesh.vertex_positions)
            originalWorldVertices.push_back(v);
        CommandService frameCommands;
        Require(frameCommands.Execute(
            std::make_unique<OrientNearPlanarRiverWaterPlaneCommand>(
                scene,slopeEntity)),
            "opt-in bridge must orient existing straight native WATER reach");
        std::cout<<"ORIENT_STRAIGHT_NATIVE_WATER_BRIDGE_COMMAND=PASS\n";
        float worstControlShift=0;
        for(size_t i=0;i<sourceNodeIds.size();++i)
        {
            const auto* t=scene.transforms.GetComponent(sourceNodeIds[i]);
            XMFLOAT3 pos;
            XMStoreFloat3(&pos,XMVector3TransformCoord(
                XMVectorZero(),t->GetWorldMatrix()));
            const auto& prev=worldNodesBefore[i];
            const float dist=std::sqrt((pos.x-prev.x)*(pos.x-prev.x)
                +(pos.y-prev.y)*(pos.y-prev.y)
                +(pos.z-prev.z)*(pos.z-prev.z));
            worstControlShift=std::max(worstControlShift,dist);
        }
        Require(worstControlShift<.002f,
            "root correction moved one or more authored control points");
        for(int i=0;i<36;++i){app.Run();Sleep(3);}
        auto* correctedMaterial=scene.materials.GetComponent(slopeEntity);
        const auto* correctedMesh=scene.meshes.GetComponent(slopeEntity);
        Require(correctedMaterial && correctedMesh &&
            correctedMesh->vertex_positions.size()==originalWorldVertices.size(),
            "root correction did not regenerate matching spline geometry");
        const auto* correctedRoot=scene.transforms.GetComponent(slopeEntity);
        const XMMATRIX correctedWorld=correctedRoot->GetWorldMatrix();
        float worstSurfaceShift=0;
        for(size_t i=0;i<correctedMesh->vertex_positions.size();++i)
        {
            XMFLOAT3 w;
            XMStoreFloat3(&w,XMVector3TransformCoord(
                XMLoadFloat3(&correctedMesh->vertex_positions[i]),
                correctedWorld));
            const auto& prev=originalWorldVertices[i];
            worstSurfaceShift=std::max(worstSurfaceShift,
                std::sqrt((w.x-prev.x)*(w.x-prev.x)
                    +(w.y-prev.y)*(w.y-prev.y)
                    +(w.z-prev.z)*(w.z-prev.z)));
        }
        std::cout<<"COMPENSATED_ROOT_REFLECTION_PLANE="
            <<render.visibility_main.reflectionPlane.x<<","
            <<render.visibility_main.reflectionPlane.y<<","
            <<render.visibility_main.reflectionPlane.z<<","
            <<render.visibility_main.reflectionPlane.w
            <<" MAX_CONTROL_WORLD_SHIFT_M="<<worstControlShift
            <<" MAX_RIBBON_WORLD_SHIFT_M="<<worstSurfaceShift
            <<" NODE_COUNT="<<sourceNodeIds.size()<<"\n";
        for(size_t i:{size_t(0),size_t(1),size_t(2),
            originalWorldVertices.size()/2,originalWorldVertices.size()-1})
        {
            const auto& a=originalWorldVertices[i];
            const auto& b=correctedMesh->vertex_positions[i];
            XMFLOAT3 transformed;
            XMStoreFloat3(&transformed,XMVector3TransformCoord(
                XMLoadFloat3(&b),correctedWorld));
            std::cout<<"ROOT_REMAP_VERTEX["<<i<<"] original="
                <<a.x<<","<<a.y<<","<<a.z
                <<" regeneratedLocal="<<b.x<<","<<b.y<<","<<b.z
                <<" regeneratedWorld="<<transformed.x<<","
                <<transformed.y<<","<<transformed.z<<"\n";
        }
        const auto* debugNode=scene.transforms.GetComponent(sourceNodeIds[0]);
        std::cout<<"ROOT_REMAP_STATE rootWorldTranslation="
            <<correctedRoot->world._41<<","<<correctedRoot->world._42
            <<","<<correctedRoot->world._43
            <<" firstNodeLocal="<<debugNode->translation_local.x<<","
            <<debugNode->translation_local.y<<","
            <<debugNode->translation_local.z
            <<" firstNodeWorld="<<debugNode->world._41<<","
            <<debugNode->world._42<<","<<debugNode->world._43<<"\n";
        Require(worstSurfaceShift<.02f,
            "root compensation changed existing water surface world geometry");
        // Disk roundtrip of the FIXED original spline, BEFORE removing
        // disposable entities. Check the native WATER material, parent graph,
        // control world positions, slope transform and regenerated mesh.
        const auto roundtripFile=(out/"root-compensated-proof.wiscene").generic_u8string();
        {
            wi::Archive disk(roundtripFile,false,false);
            ScopedRiverLocalDepthArchiveMask nativeOnly(scene);
            scene.Serialize(disk);
            Require(disk.SaveFile(roundtripFile),
                "failed to save root-remapped native WATER WISCENE");
        }
        {
            wi::Archive disk(roundtripFile,true,false);
            Require(disk.IsOpen(),"saved root-remapped WISCENE missing");
            wi::scene::Scene reopened;
            reopened.Serialize(disk);
            const auto storedEntity=reopened.Entity_FindByName("River Water");
            Require(storedEntity!=wi::ecs::INVALID_ENTITY,
                "root-remapped WATER missing from reopened WISCENE");
            const auto* savedWater=reopened.materials.GetComponent(storedEntity);
            Require(savedWater &&
                savedWater->shaderType==
                    wi::scene::MaterialComponent::SHADERTYPE_WATER &&
                !savedWater->IsCustomShader(),
                "root-remapped WISCENE replaced native WATER shader");
            const auto* restoredNodes=reopened.splines.GetComponent(storedEntity);
            Require(restoredNodes &&
                restoredNodes->spline_node_entities.size()==sourceNodeIds.size(),
                "saved root-remapped WISCENE lost spline nodes");
            reopened.Update(0.016f);
            const auto* savedRoot=reopened.transforms.GetComponent(storedEntity);
            Require(savedRoot,"saved root transform missing");
            const XMMATRIX loadedMatrix=savedRoot->GetWorldMatrix();
            float reopenedShift=0;
            for(size_t i=0;i<restoredNodes->spline_node_entities.size();++i)
            {
                const auto id=restoredNodes->spline_node_entities[i];
                const auto* child=reopened.transforms.GetComponent(id);
                const auto* hierarchy=reopened.hierarchy.GetComponent(id);
                Require(child && hierarchy &&
                    hierarchy->parentID==storedEntity,
                    "WISCENE root remapping lost child hierarchy");
                XMFLOAT3 world;
                XMStoreFloat3(&world,XMVector3TransformCoord(
                    XMVectorZero(),child->GetWorldMatrix()));
                const auto& expected=worldNodesBefore[i];
                const float d=std::sqrt(
                    (world.x-expected.x)*(world.x-expected.x)+
                    (world.y-expected.y)*(world.y-expected.y)+
                    (world.z-expected.z)*(world.z-expected.z));
                reopenedShift=std::max(reopenedShift,d);
            }
            Require(reopenedShift<.002f,
                "reopened root remap changed authored point world positions");
            const auto* loadedMesh=reopened.meshes.GetComponent(storedEntity);
            Require(loadedMesh &&
                loadedMesh->vertex_positions.size()==originalWorldVertices.size(),
                "reopened root remap lost native generated spline mesh");
            float reopenedMeshShift=0;
            for(size_t i=0;i<originalWorldVertices.size();++i)
            {
                XMFLOAT3 a;
                XMStoreFloat3(&a,XMVector3TransformCoord(
                    XMLoadFloat3(&loadedMesh->vertex_positions[i]),
                    loadedMatrix));
                const auto& expected=originalWorldVertices[i];
                reopenedMeshShift=std::max(reopenedMeshShift,std::sqrt(
                    (a.x-expected.x)*(a.x-expected.x)+
                    (a.y-expected.y)*(a.y-expected.y)+
                    (a.z-expected.z)*(a.z-expected.z)));
            }
            Require(reopenedMeshShift<.02f,
                "reopened native ribbon moved relative to original terrain");
            std::cout<<"ROOT_REMAP_WISCENE_ROUNDTRIP=PASS"
                <<" worldNodeShift="<<reopenedShift
                <<" meshWorldShift="<<reopenedMeshShift
                <<" nativeMaterial=1\n";
        }
        take("spline-30-compensated-existing-root-normal");
        Require(frameCommands.Undo(),"undo straight-WATER orientation command");
        for(int i=0;i<22;++i){app.Run();Sleep(2);}
        const auto* undoneRoot=scene.transforms.GetComponent(slopeEntity);
        Require(undoneRoot && std::abs(undoneRoot->rotation_local.x)<.0001f &&
            std::abs(undoneRoot->rotation_local.z)<.0001f &&
            std::abs(render.visibility_main.reflectionPlane.z)<.001f,
            "undo did not restore original horizontal-root reflection plane");
        const auto* undoneMesh=scene.meshes.GetComponent(slopeEntity);
        Require(undoneMesh && undoneMesh->vertex_positions.size()==
            originalWorldVertices.size(),"undo changed water mesh vertex count");
        float undoWorldShift=0;
        for(size_t i=0;i<originalWorldVertices.size();++i)
        {
            const auto& u=undoneMesh->vertex_positions[i];
            const auto& p=originalWorldVertices[i];
            undoWorldShift=std::max(undoWorldShift,std::sqrt(
                (u.x-p.x)*(u.x-p.x)+(u.y-p.y)*(u.y-p.y)+
                (u.z-p.z)*(u.z-p.z)));
        }
        Require(undoWorldShift<.02f,"undo moved saved river mesh");
        take("spline-30-orientation-undone-native");
        Require(frameCommands.Redo(),"redo straight-WATER orientation command");
        for(int i=0;i<22;++i){app.Run();Sleep(2);}
        Require(std::abs(render.visibility_main.reflectionPlane.z-.5f)<.001f,
            "redo did not restore tilted native reflection plane");
        const auto* redoneMesh=scene.meshes.GetComponent(slopeEntity);
        const auto* redoneRoot=scene.transforms.GetComponent(slopeEntity);
        float redoWorldShift=0;
        for(size_t i=0;i<originalWorldVertices.size();++i)
        {
            XMFLOAT3 v;
            XMStoreFloat3(&v,XMVector3TransformCoord(
                XMLoadFloat3(&redoneMesh->vertex_positions[i]),
                redoneRoot->GetWorldMatrix()));
            const auto& p=originalWorldVertices[i];
            redoWorldShift=std::max(redoWorldShift,std::sqrt(
                (v.x-p.x)*(v.x-p.x)+(v.y-p.y)*(v.y-p.y)+
                (v.z-p.z)*(v.z-p.z)));
        }
        Require(redoWorldShift<.02f,"redo moved authored river mesh");
        take("spline-30-orientation-redone-native");
        CommandService duplicateFrame;
        Require(!duplicateFrame.Execute(std::make_unique<
            OrientNearPlanarRiverWaterPlaneCommand>(scene,slopeEntity)),
            "near-planar root correction incorrectly applied twice");
        std::cout<<"NATIVE_WATER_ROOT_UNDO_REDO=PASS"
            <<" undoMeshShift="<<undoWorldShift
            <<" redoMeshShift="<<redoWorldShift<<"\\n";
        // Remove source after A/B and ensure no stale WATER reflection
        // request contaminates the third variant.
        scene.Entity_Remove(slopeEntity,true); // no reflection from prior river
        std::cout<<"NATIVE_WATER_SPLINE_VS_PLANE30=PASS\n";
        // Exact same 30-degree world-plane, but encode the slope in the
        // spline ROOT transform rather than baking it into each spline point.
        // Native reflection selection uses the root world-up vector.
        std::vector<XMFLOAT3> rootedPoints={
            {0,0,-24},{0,0,-12},{0,0,0},{0,0,12},{0,0,24}
        };
        auto rotatedSpline=std::make_unique<CreateRiverSplineCommand>(
            scene,rootedPoints,options);
        auto* rotatedCommand=rotatedSpline.get();
        Require(history.Execute(std::move(rotatedSpline)),
            "create root-rotated native WATER spline");
        const auto rootedEntity=rotatedCommand->CreatedEntity();
        auto* rootTransform=scene.transforms.GetComponent(rootedEntity);
        Require(rootTransform!=nullptr,"new spline root transform");
        rootTransform->translation_local={0,24,0};
        XMStoreFloat4(&rootTransform->rotation_local,
            XMQuaternionRotationRollPitchYaw(radians,0,0));
        rootTransform->SetDirty();
        rootTransform->UpdateTransform();
        for(int i=0;i<32;++i){app.Run();Sleep(3);}
        auto* rootWater=scene.materials.GetComponent(rootedEntity);
        const auto* rootMesh=scene.meshes.GetComponent(rootedEntity);
        Require(rootWater && rootMesh && rootMesh->vertex_positions.size()>6,
            "root-rotated WATER spline is missing render mesh");
        Require(!rootWater->IsCustomShader() &&
            rootWater->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER,
            "root-rotated WATER is not native shader");
        rootWater->texAnimFrameRate=0;
        take("spline-30-root-rotated-normal");
        std::cout<<"ROOTED_SPLINE30_REFLECTION_PLANE="
            <<render.visibility_main.reflectionPlane.x<<","
            <<render.visibility_main.reflectionPlane.y<<","
            <<render.visibility_main.reflectionPlane.z<<","
            <<render.visibility_main.reflectionPlane.w
            <<" ROOT_WORLD_TRANSFORM_ROTATION="
            <<rootTransform->rotation_local.x<<","
            <<rootTransform->rotation_local.y<<","
            <<rootTransform->rotation_local.z<<","
            <<rootTransform->rotation_local.w
            <<" ROOTED_VERTEX_NORMAL_LOCAL="
            <<rootMesh->vertex_normals[0].x<<","
            <<rootMesh->vertex_normals[0].y<<","
            <<rootMesh->vertex_normals[0].z<<"\n";
        rootWater->SetNormalMapStrength(0);
        take("spline-30-root-rotated-flat-normal");
        rootWater->SetNormalMapStrength(normalStrength);
        scene.objects.GetComponent(rootedEntity)->SetRenderable(false);
        take("spline-30-root-rotated-underlay-only");
        std::cout<<"NATIVE_WATER_SAME_SLOPE_ROOT_ROTATED=PASS\n";

        std::cout<<"WICKED_NATIVE_WATER_ORIENTATION_CAPTURE=PASS\n";
        std::cout<<"SHADER_ERRORS="<<wi::renderer::GetShaderErrorCount()
            <<" SHADER_MISSING="<<wi::renderer::GetShaderMissingCount()<<"\n";
        return 0;
    }
    catch(const std::exception& e)
    {
        std::cerr<<"WICKED_NATIVE_WATER_ORIENTATION_CAPTURE=FAIL "<<e.what()<<"\n";
        return 1;
    }
}
