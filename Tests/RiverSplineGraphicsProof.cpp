#include "renegade/bridge/RiverSplineService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/SpecialistComponentService.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

using namespace DirectX;
namespace fs = std::filesystem;
using namespace renegade::bridge;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }
}

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3 ||
        (argc == 3 && std::string(argv[2]) != "sloped" &&
            std::string(argv[2]) != "reopen"))
    {
        std::cerr << "Usage: RenegadeRiverSplineGraphicsProof empty-output-folder [sloped]\n";
        return 2;
    }
    const auto output = fs::absolute(fs::u8path(argv[1]));
    const bool reopenOnly = argc == 3 && std::string(argv[2]) == "reopen";
    const bool sloped = argc == 3 && !reopenOnly;
    if (!reopenOnly && fs::exists(output) && !fs::is_empty(output))
    {
        std::cerr << "Graphics proof requires a new/empty output directory\n";
        return 2;
    }
    if (!reopenOnly) fs::create_directories(output);

    HWND window = CreateWindowExW(0, L"STATIC", L"Isolated River Graphics Proof",
        WS_OVERLAPPEDWINDOW, 0, 0, 960, 640,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (GetEnvironmentVariableW(L"RENEGADE_RIVER_GPU_DEBUG",nullptr,0)>0)
        wi::arguments::Parse(L"debugdevice");
    std::cout.setf(std::ios::unitbuf);
    wi::Application app;
    app.allow_hdr = false;
    app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();

    auto* device = wi::graphics::GetDevice();
    struct Drain
    {
        ~Drain()
        {
            while (wi::renderer::IsPipelineCreationActive()) Sleep(10);
            wi::graphics::GetDevice()->WaitForGPU();
        }
    } drain;
    try
    {
        wi::scene::Scene scene;
        if (reopenOnly)
        {
            wi::Archive saved(output.generic_u8string(), true, false);
            Require(saved.IsOpen(), "fresh-process scene file missing");
            scene.Serialize(saved);
            wi::scene::MaterialComponent* water = nullptr;
            for (std::size_t i=0; i<scene.materials.GetCount(); ++i)
                if (scene.materials[i].shaderType ==
                    wi::scene::MaterialComponent::SHADERTYPE_WATER)
                    water = &scene.materials[i];
            Require(water != nullptr, "fresh-process water material missing");
            std::cout << "WATER_REFERENCE refraction=" << water->refraction
                << " normalStrength=" << water->normalMapStrength
                << " roughness=" << water->roughness
                << " uv=" << water->texMulAdd.x << "," << water->texMulAdd.y
                << " animation=" << water->texAnimDirection.x << ","
                << water->texAnimDirection.y << "\n";
            const auto& normal = water->textures[
                wi::scene::MaterialComponent::NORMALMAP];
            Require(normal.resource.IsValid() && normal.resource.GetTexture().IsValid(),
                "fresh-process water normal texture failed to reload");
            std::cout << "READONLY_SAVED_WATER_RGBA=" << water->baseColor.x << ","
                << water->baseColor.y << "," << water->baseColor.z << ","
                << water->baseColor.w << " INSPECTOR_CLARITY="
                << 1.f-water->baseColor.w << "\n";
            std::cout << "FRESH_PROCESS_WATER_NORMAL=PASS\n";
            return 0;
        }
        // Completely disposable geometric backdrop; no user project is opened.
        const auto ground = scene.Entity_CreateCube("Proof Ground");
        Require(ground != wi::ecs::INVALID_ENTITY, "ground creation");
        if (auto* transform = scene.transforms.GetComponent(ground))
        {
            transform->scale_local = {95.0f, 0.4f, 95.0f};
            transform->translation_local = {0.0f, -0.55f, 0.0f};
            transform->SetDirty();
        }
        if (auto* material = scene.materials.GetComponent(ground))
        {
            material->SetBaseColor({0.20f, 0.32f, 0.21f, 1.0f});
            material->SetRoughness(0.95f);
            material->SetEmissiveColor({0.0f, 0.0f, 0.0f, 0.0f});
        }

        std::vector<XMFLOAT3> riverNodes = {
            {-35.0f, 0.2f, -30.0f},
            {-18.0f, 0.2f, -12.0f},
            {-8.0f, 0.2f, 5.0f},
            {12.0f, 0.2f, 14.0f},
            {33.0f, 0.2f, 29.0f}
        };
        if (sloped)
        {
            riverNodes[0].y = 1.0f;
            riverNodes[1].y = 0.65f;
            riverNodes[2].y = 0.35f;
            riverNodes[3].y = 0.1f;
            riverNodes[4].y = 0.2f;
        }
        RiverSplineOptions options;
        options.widthMeters = 8.0f;
        options.surfaceOffsetMeters = 0.15f;
        auto create = std::make_unique<CreateRiverSplineCommand>(scene, riverNodes, options);
        auto* command = create.get();
        CommandService commands;
        Require(commands.Execute(std::move(create)), "creating spline water");
        const auto river = command->CreatedEntity();

        const auto weatherEntity = scene.Entity_CreateTransform("Proof Weather");
        auto& weather = scene.weathers.Create(weatherEntity);
        weather.ambient = {0.08f, 0.08f, 0.08f};
        weather.horizon = {0.5f, 0.67f, 0.86f};
        weather.zenith = {0.18f, 0.32f, 0.64f};
        const auto sun = scene.Entity_CreateLight("Proof Sun", {0.0f, 35.0f, -20.0f},
            {1.0f, 0.95f, 0.85f}, 5.0f, 200.0f,
            wi::scene::LightComponent::DIRECTIONAL);
        scene.transforms.GetComponent(sun)->RotateRollPitchYaw(
            XMFLOAT3(1.35f, -0.78f, 0.0f));
        // Contrasting opaque objects make reflection/refraction failures
        // visible instead of comparing a ribbon against uniform sky/ground.
        for (int i = 0; i < 7; ++i)
        {
            const auto rock = scene.Entity_CreateCube("Proof Bank Rock " + std::to_string(i));
            auto* pose = scene.transforms.GetComponent(rock);
            pose->scale_local = {1.5f, 1.0f + i * 0.25f, 1.5f};
            pose->translation_local = {-24.0f + i * 8.0f, 0.0f, 10.0f};
            pose->SetDirty();
            scene.materials.GetComponent(rock)->SetBaseColor(
                XMFLOAT4(0.35f, 0.28f, 0.20f, 1.0f));
        }

        wi::scene::CameraComponent camera;
        camera.CreatePerspective(960, 640, 0.1f, 500.0f,
            XMConvertToRadians(60.0f));
        camera.TransformCamera(XMMatrixInverse(nullptr, XMMatrixLookAtLH(
            XMVectorSet(10.0f, 6.0f, -14.0f, 1.0f),
            XMVectorSet(-8.0f, 0.35f, 5.0f, 1.0f),
            XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))));
        camera.UpdateCamera();
        XMFLOAT3 originNdc;
        XMStoreFloat3(&originNdc, XMVector3TransformCoord(
            XMVectorSet(0, 0, 0, 1), XMLoadFloat4x4(&camera.VP)));
        std::cout << "CAMERA Eye=" << camera.Eye.x << "," << camera.Eye.y
            << "," << camera.Eye.z << " At=" << camera.At.x << ","
            << camera.At.y << "," << camera.At.z << " originNDC="
            << originNdc.x << "," << originNdc.y << "," << originNdc.z << "\n";

        wi::RenderPath3D render;
        render.scene = &scene;
        render.camera = &camera;
        render.init(960, 640, 96);
        render.setMotionBlurEnabled(false);
        // Offscreen proof has no swapchain occlusion-query history.
        // Do not use potentially stale/uninitialised occlusion query results.
        render.setOcclusionCullingEnabled(false);
        render.setDepthOfFieldEnabled(false);
        render.ResizeBuffers();
        // Keep the native RenderPath active through all captures. Disarm it
        // before the stack-owned RenderPath dies, even on an early return.
        struct PathDisarm
        {
            wi::Application& application;
            ~PathDisarm()
            {
                if (application.GetActivePath() != nullptr)
                    application.ActivatePath(nullptr);
            }
        } pathDisarm{app};
        wi::renderer::SetTemporalAAEnabled(false);

        for (int frame = 0; frame < 45; ++frame)
        {
            render.PreUpdate();
            render.Update(1.0f / 60.0f);
            render.PreRender();
            render.Render();
            device->SubmitCommandLists();
            wi::renderer::UpdateGPUSuballocator();
        }
        const bool pipelinesInitiallyPending =
            wi::renderer::IsPipelineCreationActive();
        int pipelineWaitCount = 0;
        while (wi::renderer::IsPipelineCreationActive() &&
            pipelineWaitCount < 800)
        {
            Sleep(25);
            ++pipelineWaitCount;
        }
        // PSOs may be compiled asynchronously during the first frames.
        // Render a fresh set after they become available.
        for (int frame = 0; frame < 30; ++frame)
        {
            render.PreUpdate();
            render.Update(1.0f / 60.0f);
            render.PreRender();
            render.Render();
            device->SubmitCommandLists();
            wi::renderer::UpdateGPUSuballocator();
        }
        device->WaitForGPU();
        std::cout << "PIPELINE_INITIALLY_PENDING=" << pipelinesInitiallyPending
            << " PIPELINE_WAIT_MS=" << pipelineWaitCount * 25
            << " PIPELINE_STILL_ACTIVE="
            << wi::renderer::IsPipelineCreationActive() << "\n";

        auto* water = scene.materials.GetComponent(river);
        const auto* mesh = scene.meshes.GetComponent(river);
        const auto* object = scene.objects.GetComponent(river);
        Require(water && mesh && object, "native spline failed to create render object");
        Require(water->shaderType == wi::scene::MaterialComponent::SHADERTYPE_WATER,
            "water shader not retained");
        const char* diagnosticFlag=std::getenv("RENEGADE_RIVER_LOCAL_DEPTH_TEST");
        const bool expectDepthTest=diagnosticFlag && std::string(diagnosticFlag)=="1";
        Require(water->IsCustomShader()==expectDepthTest,
            "native WATER or test-only local-depth pipeline selection failed");
        std::cout << (expectDepthTest
            ? "LOCAL_DEPTH_OPTIN_NATIVE_WATER_PIPELINE=PASS\n"
            : "NATIVE_WATER_PIPELINE_NO_CUSTOM_SURFACE=PASS\n");
        const auto& normal = water->textures[
            wi::scene::MaterialComponent::NORMALMAP];
        Require(normal.resource.IsValid() && normal.resource.GetTexture().IsValid(),
            "Wicked water normal texture did not load (not a real water material)");
        Require(water->texAnimFrameRate > 0 &&
            std::abs(water->texAnimDirection.y) > 0.0001f,
            "Wicked water UV animation not configured");
        Require((water->GetFilterMask() & wi::enums::FILTER_WATER) != 0,
            "water mesh not in renderer water pass");
        Require(object->meshID == river && object->IsRenderable(),
            "river object not renderable");
        Require(mesh->vertex_positions.size() >= 8 && mesh->indices.size() >= 6,
            "river spline generated no mesh geometry");
        Require(!mesh->subsets.empty() && mesh->subsets[0].materialID == river,
            "water material is not assigned to ribbon subset");

        // Check physical texture density from actual generated ribbon UVs.
        // This catches treating Wicked's width-normalized V as metres.
        const auto& a = mesh->vertex_positions[0];
        const auto& b = mesh->vertex_positions[1];
        const float fullWidth = std::sqrt((a.x-b.x)*(a.x-b.x) +
            (a.y-b.y)*(a.y-b.y) + (a.z-b.z)*(a.z-b.z));
        Require(std::abs(water->texMulAdd.x - fullWidth / 4.0f) < 0.01f &&
            std::abs(water->texMulAdd.y - fullWidth / 4.0f) < 0.01f,
            "river normal map does not have metric tiling on both axes");
        std::cout << "METRIC_WATER_UV=PASS fullWidth=" << fullWidth << "\n";
        const auto curveSamples = SampleSplineWorldPoints(scene, river,
            mesh->vertex_positions.size() / 2);
        Require(curveSamples.size() * 2 == mesh->vertex_positions.size(),
            "editor spline sample count differs from native ribbon");
        float curveError = 0.0f;
        for (std::size_t i = 0; i < curveSamples.size(); ++i)
        {
            const auto& right = mesh->vertex_positions[2*i];
            const auto& left = mesh->vertex_positions[2*i+1];
            curveError = std::max({curveError,
                std::abs(curveSamples[i].x - (right.x+left.x)*0.5f),
                std::abs(curveSamples[i].y - (right.y+left.y)*0.5f),
                std::abs(curveSamples[i].z - (right.z+left.z)*0.5f)});
        }
        Require(curveError < 0.002f,
            "editor spline curve disagrees with native rendered ribbon");
        std::cout << "EDITOR_NATIVE_CURVE_PARITY=PASS error=" << curveError << "\n";

        float lowest = 100000.0f, highest = -100000.0f;
        float minX = 100000.0f, maxX = -100000.0f;
        float minZ = 100000.0f, maxZ = -100000.0f;
        for (const auto& p : mesh->vertex_positions)
        {
            Require(std::isfinite(p.x) && std::isfinite(p.y) &&
                std::isfinite(p.z), "non-finite spline vertex");
            lowest = std::min(lowest, p.y);
            highest = std::max(highest, p.y);
            minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
            minZ = std::min(minZ, p.z); maxZ = std::max(maxZ, p.z);
        }
        Require(lowest > -1.0f && highest < 2.0f,
            "river ribbon has unexpected vertical geometry");
        if (sloped)
            Require(highest - lowest > 0.35f,
                "sloped spline did not retain river elevation differences");
        else
            Require(highest - lowest < 0.001f,
                "flat spline unexpectedly changed its elevation");

        for (const auto entity : {ground, river})
        {
            const auto* obj = scene.objects.GetComponent(entity);
            const auto* msh = scene.meshes.GetComponent(entity);
            std::cout << "DRAW_PROBE entity=" << entity
                << " renderable=" << (obj && obj->IsRenderable())
                << " filter=" << (obj ? obj->GetFilterMask() : 0)
                << " meshIndex=" << (obj ? obj->mesh_index : ~0u)
                << " center=" << (obj ? obj->center.x : -999) << ","
                << (obj ? obj->center.y : -999) << ","
                << (obj ? obj->center.z : -999)
                << " radius=" << (obj ? obj->radius : 0)
                << " fadeDist=" << (obj ? obj->fadeDistance : 0)
                << " vertices=" << (msh ? msh->vertex_positions.size() : 0)
                << " gpuVb=" << (msh && msh->generalBuffer.IsValid())
                << " subsets=" << (msh ? msh->subsets.size() : 0)
                << " subsetMatIndex=" << (msh && !msh->subsets.empty() ? msh->subsets[0].materialIndex : ~0u)
                << " subsetIndexCount=" << (msh && !msh->subsets.empty() ? msh->subsets[0].indexCount : 0)
                << " materialFilter=" << (scene.materials.GetComponent(entity) ?
                    scene.materials.GetComponent(entity)->GetFilterMask() : 0)
                << " meshRenderable=" << (msh && msh->IsRenderable())
                << " renderUpdate=" << render.getSceneUpdateEnabled()
                << "\n";
        }
        const auto reflectionPlane = render.visibility_main.reflectionPlane;
        float maxReflectionPlaneError = 0.0f;
        for (const auto& p : mesh->vertex_positions)
            maxReflectionPlaneError = std::max(maxReflectionPlaneError,
                std::abs(p.x*reflectionPlane.x + p.y*reflectionPlane.y +
                    p.z*reflectionPlane.z + reflectionPlane.w));
        std::cout << "NATIVE_SHARED_REFLECTION_PLANE maxSurfaceDistance="
            << maxReflectionPlaneError << " sloped=" << sloped << "\n";
        std::cout << "RIVER_MESH_BOUNDS X=" << minX << ".." << maxX
            << " Z=" << minZ << ".." << maxZ << "\n";
        std::cout << "RENDER_VISIBILITY objects=" << render.visibility_main.visibleObjects.size()
            << " sceneObjects=" << scene.objects.GetCount()
            << " sceneLights=" << scene.lights.GetCount()
            << " mainTextureValid=" << render.rtMain.IsValid() << "\n";
        const auto capture = (output / "river-manual-headless.png").generic_u8string();
        Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(), capture),
            "failed to save GPU screenshot");
        const std::string diagnosticLog = wi::backlog::getText();
        std::ofstream(output / "wicked-backlog.txt") << diagnosticLog;
        std::cout << "SHADER_ERRORS=" << wi::renderer::GetShaderErrorCount()
            << " SHADER_MISSING=" << wi::renderer::GetShaderMissingCount() << "\n";
        std::cout << "SHADER_DUMP_COUNT="
            << wi::renderer::GetShaderDumpCount() << " SHADER_PATH=" << wi::renderer::GetShaderPath()
            << " BACKLOG_TAIL=" << diagnosticLog.substr(
                diagnosticLog.size() > 3000 ? diagnosticLog.size() - 3000 : 0)
            << "\n";
        // Verify the actual scene material update scrolls the water
        // normal map rather than leaving a static tinted transparency.
        const float riverNormalUvBefore = water->texMulAdd.w;
        // Compare manual headless frame pumping with the actual Wicked
        // Application lifecycle (including SwapChain update/presentation).
        app.ActivatePath(&render);
        for (int tick = 0; tick < 24; ++tick)
        {
            app.Run();
            Sleep(8);
        }
        device->WaitForGPU();
        Require(std::abs(water->texMulAdd.w - riverNormalUvBefore) > 0.0001f,
            "water normal UV did not animate under Wicked application frames");
        std::cout << "WATER_NORMAL_ANIMATION=PASS uvDelta="
            << (water->texMulAdd.w - riverNormalUvBefore) << "\n";
        std::cout << "APPLICATION_DRIVER_CAPTURE="
            << wi::helper::saveTextureToFile(
                render.GetRenderResult3D(),
                (output / "river-water-dx12.png").generic_u8string())
            << "\n";

        // A real raster A/B: turn off normal perturbation without changing
        // geometry, colour, lighting or camera. Capture must visibly differ.
        const float authoredNormalStrength = water->normalMapStrength;
        water->SetNormalMapStrength(0.0f);
        for (int tick = 0; tick < 12; ++tick) { app.Run(); Sleep(8); }
        device->WaitForGPU();
        Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
            (output / "river-zero-normal-dx12.png").generic_u8string()),
            "failed to capture zero-normal diagnostic");
        water->SetNormalMapStrength(authoredNormalStrength);
        for (int tick = 0; tick < 12; ++tick) { app.Run(); Sleep(8); }
        device->WaitForGPU();
        Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
            (output / "river-restored-normal-dx12.png").generic_u8string()),
            "failed to capture restored-normal diagnostic");
        // Exercise the owner-reported strong-normal distortion and native UV
        // wrap using the real raster path, with animation frozen for inspection.
        const float normalRepairGameSpeed = wi::renderer::GetGameSpeed();
        wi::renderer::SetGameSpeed(0); // Freeze wave shape too for controlled UV-wrap and camera A/B.
        const auto normalRepairUv = water->texMulAdd;
        const float normalRepairRate = water->texAnimFrameRate;
        water->texAnimFrameRate = 0;
        water->SetNormalMapStrength(3.0f);
        for (int tick = 0; tick < 24; ++tick) { app.Run(); Sleep(8); }
        device->WaitForGPU();
        Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
            (output / "river-strength3-dx12.png").generic_u8string()),
            "failed to capture strong-normal diagnostic");
        water->texMulAdd.w = -0.9999f; water->SetDirty();
        for (int tick = 0; tick < 24; ++tick) { app.Run(); Sleep(8); }
        device->WaitForGPU();
        Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
            (output / "river-wrap-before-dx12.png").generic_u8string()),
            "failed to capture pre-wrap diagnostic");
        water->texMulAdd.w = 0.0001f; water->SetDirty();
        for (int tick = 0; tick < 24; ++tick) { app.Run(); Sleep(8); }
        device->WaitForGPU();
        Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
            (output / "river-wrap-after-dx12.png").generic_u8string()),
            "failed to capture post-wrap diagnostic");
        const auto normalRepairCamera = camera;
        const auto& closeTarget = curveSamples[curveSamples.size() / 2];
        for (int view = 0; view < 2; ++view)
        {
            const XMVECTOR target = XMLoadFloat3(&closeTarget);
            const XMVECTOR eye = target + (view == 0
                ? XMVectorSet(2.5f, 1.25f, -2.0f, 0)
                : XMVectorSet(.1f, 1.2f, -.15f, 0));
            camera.TransformCamera(XMMatrixInverse(nullptr, XMMatrixLookAtLH(
                eye, target, XMVectorSet(0, 1, 0, 0))));
            camera.UpdateCamera();
            for (int level = 0; level < 3; ++level)
            {
                water->SetNormalMapStrength(level == 0 ? 0.f : level == 1 ? authoredNormalStrength : 3.f);
                for (int tick = 0; tick < 32; ++tick) { app.Run(); Sleep(8); }
                device->WaitForGPU();
                const auto name = std::string(view == 0 ? "river-close-bank-" : "river-close-down-")
                    + std::to_string(level) + ".png";
                Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
                    (output / name).generic_u8string()), "failed to capture close-range normal diagnostic");
            }
        }
        wi::renderer::SetGameSpeed(normalRepairGameSpeed);
        camera = normalRepairCamera;
        water->texMulAdd = normalRepairUv;
        water->texAnimFrameRate = normalRepairRate;
        water->SetNormalMapStrength(authoredNormalStrength); water->SetDirty();
        std::cout << "STRONG_NORMAL_AND_UV_WRAP_RASTER_CAPTURES=PASS\n";

        std::cout << "OPAQUE_CAPTURE="
            << wi::helper::saveTextureToFile(
                render.rtMain, (output / "river-opaque-main.png").generic_u8string())
            << " PREPROCESS_CAPTURE="
            << wi::helper::saveTextureToFile(
                render.rtPostprocess, (output / "river-postprocess.png").generic_u8string())
            << "\n";

        // Editing a native control-point transform must regenerate the
        // geometry on the next application update, with no terrain mutation.
        const auto originalVertices = mesh->vertex_positions;
        const auto& controls = scene.splines.GetComponent(river)->spline_node_entities;
        Require(controls.size() > 2, "not enough river control points to edit");
        const auto controlPoint = controls[2];
        auto* controlTransform = scene.transforms.GetComponent(controlPoint);
        Require(controlTransform != nullptr, "middle spline control point missing");
        controlTransform->translation_local.x += 12.0f;
        controlTransform->SetDirty();
        for (int frame = 0; frame < 12; ++frame)
        {
            app.Run();
            Sleep(8);
        }
        device->WaitForGPU();
        Require(mesh->vertex_positions.size() == originalVertices.size(),
            "editing spline changed segment count unexpectedly");
        float maxDisplacement = 0.0f;
        for (std::size_t vertex = 0; vertex < originalVertices.size(); ++vertex)
        {
            maxDisplacement = std::max(maxDisplacement,
                std::abs(mesh->vertex_positions[vertex].x - originalVertices[vertex].x));
        }
        Require(maxDisplacement > 1.0f,
            "editing spline control point did not regenerate ribbon");
        Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
            (output / "river-edited-dx12.png").generic_u8string()),
            "failed to capture edited river");
        controlTransform->translation_local.x -= 12.0f;
        controlTransform->SetDirty();
        for (int frame = 0; frame < 12; ++frame)
        {
            app.Run();
            Sleep(8);
        }
        device->WaitForGPU();
        float maxRestoreError = 0.0f;
        for (std::size_t vertex = 0; vertex < originalVertices.size(); ++vertex)
        {
            const auto& a = originalVertices[vertex];
            const auto& b = mesh->vertex_positions[vertex];
            maxRestoreError = std::max(maxRestoreError,
                std::max({std::abs(a.x - b.x), std::abs(a.y - b.y),
                          std::abs(a.z - b.z)}));
        }
        Require(maxRestoreError < 0.01f, "returning control point did not restore ribbon");
        std::cout << "SPLINE_POINT_EDIT_REGENERATE=PASS maxMove="
            << maxDisplacement << " maxRestoreError=" << maxRestoreError << "\n";

        // Separate water-rendering problems from general scene capture.
        // A second screenshot without the water object isolates the ground
        // baseline; the original river is restored before serialization.
        auto* mutableObject = scene.objects.GetComponent(river);
        mutableObject->SetRenderable(false);
        for (int frame = 0; frame < 12; ++frame)
        {
            app.Run();
            Sleep(8);
        }
        device->WaitForGPU();
        const auto groundOnly = output / "river-proof-ground-only.png";
        std::cout << "GROUND_ONLY_VISIBILITY="
            << render.visibility_main.visibleObjects.size()
            << " GROUND_ONLY_CAPTURE="
            << wi::helper::saveTextureToFile(render.GetRenderResult3D(),
                groundOnly.generic_u8string()) << "\n";
        mutableObject->SetRenderable(true);

        const auto authoredScenePath = (output / "river-proof.wiscene").generic_u8string();
        // Match SceneDocumentService: the archive must know its destination
        // while serializing texture paths, not only at SaveFile time.
        wi::Archive archive(authoredScenePath, false, false);
        {
            ScopedRiverLocalDepthArchiveMask portableWater(scene);
            scene.Serialize(archive);
        }
        Require(archive.SaveFile(authoredScenePath),
            "could not save disposable GPU river scene");
        {
            auto preparedOpen = PrepareWickedSceneOpen(authoredScenePath);
            Require(preparedOpen.IsReady(),
                "Renegade could not validate the saved WISCENE");
            std::cout << "RENEGADE_SCENE_OPEN=PASS\n";
            wi::Archive fromDisk(authoredScenePath, true, false);
            Require(fromDisk.IsOpen(), "saved river WISCENE could not be reopened");
            wi::scene::Scene reopened;
            reopened.Serialize(fromDisk);
            const auto loadedRiver = reopened.Entity_FindByName("River Water");
            Require(loadedRiver != wi::ecs::INVALID_ENTITY &&
                reopened.splines.Contains(loadedRiver) &&
                reopened.materials.Contains(loadedRiver),
                "disk reopened scene lost the native river");
            const auto& loadedSpline = *reopened.splines.GetComponent(loadedRiver);
            Require(loadedSpline.spline_node_entities.size() == riverNodes.size(),
                "disk reopened river lost control points");
            const auto* loadedWater = reopened.materials.GetComponent(loadedRiver);
            Require(loadedWater->shaderType ==
                wi::scene::MaterialComponent::SHADERTYPE_WATER &&
                !loadedWater->IsCustomShader(),
                "disk must contain native WATER, never a process-local test shader ID");
            Require(BindRiverSurfaceShader(reopened,loadedRiver) &&
                loadedWater->IsCustomShader()==expectDepthTest,
                "adopting a saved native WATER river must select the current process mode");
            if(expectDepthTest) std::cout << "LOCAL_DEPTH_TEST_REBIND_AFTER_REOPEN=PASS\n";
            Require(std::abs(loadedWater->texAnimDirection.x - water->texAnimDirection.x)<.00001f &&
                std::abs(loadedWater->texAnimDirection.y - water->texAnimDirection.y)<.00001f &&
                std::abs(loadedWater->normalMapStrength - water->normalMapStrength)<.00001f &&
                std::abs(loadedWater->baseColor.w - water->baseColor.w)<.00001f,
                "disk reopened native WATER lost flow, ripple strength or clarity");
            std::cout << "NATIVE_WATER_REOPEN_CONTROLS_NO_CUSTOM_SHADER=PASS\n";
            const auto& loadedNormal = reopened.materials.GetComponent(
                loadedRiver)->textures[wi::scene::MaterialComponent::NORMALMAP];
            Require(loadedNormal.resource.IsValid() &&
                loadedNormal.resource.GetTexture().IsValid(),
                "disk reopened river lost its GPU water normal texture");
            Require(!normal.resource.GetFileData().empty(),
                "water normal source bytes not retained for packaging");
            std::cout << "DISK_WATER_NORMAL_RESOURCE=PASS\n";
            std::cout << "DISK_WISCENE_ROUNDTRIP=PASS nodes="
                << loadedSpline.spline_node_entities.size() << "\n";
        }

        // Stronger baseline: disabling rendering does not remove the entity
        // from the visibility candidates. Delete the disposable spline and
        // its ObjectComponent only AFTER saving the authored proof scene.
        // With the spline gone, the native update cannot recreate the object.
        device->WaitForGPU();
        scene.splines.Remove(river);
        scene.objects.Remove(river);
        for (int frame = 0; frame < 15; ++frame)
        {
            app.Run();
            Sleep(8);
        }
        device->WaitForGPU();
        std::cout << "BASELINE_WITHOUT_RIVER_VISIBILITY="
            << render.visibility_main.visibleObjects.size()
            << " BASELINE_CAPTURE="
            << wi::helper::saveTextureToFile(
                render.GetRenderResult3D(),
                (output / "baseline-without-river.png").generic_u8string())
            << "\n";

        std::cout << "PASS: native GPU-generated geometry "
            << mesh->vertex_positions.size() << " vertices, "
            << mesh->indices.size() << " indices; water material, render "
            << "object and scene snapshot valid; elevation "
            << lowest << ".." << highest << "\n";
        auto readImage = [](const fs::path& imagePath)
        {
            std::ifstream input(imagePath, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>());
        };
        const auto waterBytes = readImage(output / "river-water-dx12.png");
        const auto groundBytes = readImage(output / "river-proof-ground-only.png");
        const auto noRiverBytes = readImage(output / "baseline-without-river.png");
        if (waterBytes.empty() || waterBytes == groundBytes ||
            waterBytes == noRiverBytes)
        {
            std::cerr << "VISUAL FAIL: water-on screenshot does not differ "
                << "from the hidden or removed-water baseline; generated "
                << "geometry is NOT visually validated.\n";
            return 3;
        }
        std::cout << "VISUAL PRELIMINARY: images differ; owner inspection "
                  << "is still required\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << "\n";
        return 1;
    }
}
