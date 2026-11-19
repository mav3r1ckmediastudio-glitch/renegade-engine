#include <WickedEngine.h>
#include "../Runtime/src/RuntimePropModelResidency.h"
#include <Windows.h>
#include <Psapi.h>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

// Self-contained, original DX10 RGBA8 DDS with a complete mip chain.
static bool WriteFixture(const std::filesystem::path& destination)
{
    constexpr unsigned width = 1024;
    std::array<uint32_t, 37> header{};
    header[0] = 0x20534444;
    header[1] = 124;
    header[2] = 0x2100f;
    header[3] = header[4] = width;
    header[5] = width * 4;
    header[7] = 11;
    header[19] = 32;
    header[20] = 4;
    header[21] = 0x30315844;
    header[27] = 0x401008;
    header[32] = 28; // DXGI_FORMAT_R8G8B8A8_UNORM
    header[33] = 3;  // D3D10_RESOURCE_DIMENSION_TEXTURE2D
    header[35] = 1;
    std::ofstream out(destination, std::ios::binary);
    out.write(reinterpret_cast<const char*>(header.data()), sizeof(header));
    for (unsigned dimension = width; dimension; dimension /= 2)
    {
        std::vector<uint32_t> pixels(dimension * dimension, 0xff407050u);
        out.write(reinterpret_cast<const char*>(pixels.data()), pixels.size() * 4);
    }
    out.close();
    return static_cast<bool>(out);
}

int MeasureTerrainPropResidency(const std::filesystem::path& directory, wi::scene::MaterialComponent* material)
{
    std::filesystem::create_directories(directory);
    const auto path = directory / "terrain-prop-probe.dds";
    if (!WriteFixture(path)) return 3;
    auto* device = wi::graphics::GetDevice();
    const auto backingBytes = std::filesystem::file_size(path);
    auto resource = wi::resourcemanager::Load(path.string(), wi::resourcemanager::Flags::STREAMING,
        nullptr, backingBytes);
    if (!resource.IsValid() || !resource.GetTexture().IsValid()) return 6;
    if (!resource.GetFileData().empty()) return 10;
    std::cout << "adapter=" << device->GetAdapterName() << std::endl;

    using Material = wi::scene::MaterialComponent;
    if (material)
    {
        auto& slot = material->textures[Material::BASECOLORMAP];
        slot.resource = resource;
        material->SetDirty();
    }
    auto report = [&](const char* phase)
    {
        const auto& observed = material ? material->textures[Material::BASECOLORMAP].resource : resource;
        const auto desc = observed.GetTexture().desc;
        const auto usage = device->GetMemoryUsage();
        PROCESS_MEMORY_COUNTERS process{};
        if (!GetProcessMemoryInfo(GetCurrentProcess(), &process, sizeof(process)))
            throw std::runtime_error("Could not sample process memory.");
        std::cout << phase << " source=" << (material ? "shared_prop_material" : "resource")
            << " process_peak_working_set=" << process.PeakWorkingSetSize
            << " backing_file_bytes=" << backingBytes << " width=" << desc.width << " resident_mips=" << desc.mip_levels
            << " full_mips=" << resource.GetTextureFullMipCount()
            << " resident_payload_bytes=" << wi::graphics::ComputeTextureMemorySizeInBytes(desc)
            << " device_usage_bytes=" << usage.usage << " device_budget_bytes=" << usage.budget << std::endl;
    };
    report("cold");
    const unsigned cold = resource.GetTexture().desc.width;
    if (cold >= 1024 || resource.GetTextureFullMipCount() != 11) return 7;
    auto reach = [&](unsigned request, unsigned target)
    {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (resource.GetTexture().desc.width != target && std::chrono::steady_clock::now() < deadline)
        {
            resource.StreamingRequestResolution(request);
            wi::resourcemanager::UpdateStreamingResources(1.f / 60.f);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return resource.GetTexture().desc.width == target;
    };
    // Deny backing-file reads after initial upload. A retained full-chain CPU
    // copy would allow the request to succeed despite this exclusive handle.
    const HANDLE locked = CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (locked == INVALID_HANDLE_VALUE) return 11;
    for (unsigned frame = 0; frame < 24; ++frame)
    {
        resource.StreamingRequestResolution(1024);
        wi::resourcemanager::UpdateStreamingResources(1.f / 60.f);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const bool stayedCold = resource.GetTexture().desc.width == cold;
    CloseHandle(locked);
    if (!stayedCold) return 12;
    report("backing_locked");
    if (!reach(1024, 1024)) return 8;
    report("near");
    if (!reach(0, cold)) return 9;
    report("far");
    return 0;
}

int RunGovernedTextureProof(const std::filesystem::path&, const std::filesystem::path&);
int RunTerrainPropSlice(const std::filesystem::path&, const std::filesystem::path&);

// Read-only probe for separately registered owner-supplied detail products.
static int MeasureMeshDetail(const char* root, const char* project, const char* fine, const char* coarse)
{
    using namespace renegade::bridge;
    wi::scene::Scene scene;
    TerrainPropProfile profile;
    profile.id[0] = 1;
    profile.modelAssetId = fine;
    profile.coarseModelAssetId = coarse;
    const auto layer = GenerateStableId();
    TerrainPropProjection fallback(scene,false);
    fallback.SetLayerIdentity(layer);
    TerrainPropDetailProjection detail(scene, layer, profile, {root,project,fine});
    TerrainPropPage a;
    a.profiles.push_back(profile.id);
    a.placements.push_back({1,0,{0,0,0},0,1});
    auto b = a;
    b.cellX = 1;
    b.placements.front().id = 2;
    b.placements.front().position[0] = 32;
    std::string error;
    const auto acquire = [&](TerrainPropDetailProjection& projection, const TerrainPropPage& page) {
        if (!projection.Acquire(page,error)) throw std::runtime_error(error);
    };
    const auto measure = [&](const char* phase) {
        uint64_t bytes = 0, triangles = 0;
        for (size_t i = 0; i < scene.meshes.GetCount(); ++i) {
            const auto& m = scene.meshes[i];
            if (!m.generalBuffer.IsValid() || !m.ib.IsValid() || !m.vb_pos_wind.IsValid())
                throw std::runtime_error("Missing native mesh buffer views");
            for (const auto* view : {&m.ib,&m.vb_pos_wind,&m.vb_nor,&m.vb_tan,&m.vb_uvs,&m.vb_atl,&m.vb_col})
                if (view->IsValid()) bytes += view->size;
            triangles += m.indices.size()/3;
        }
        std::cout << phase << " mesh_view_bytes=" << bytes << " unique_triangles=" << triangles
                  << " meshes=" << scene.meshes.GetCount() << " objects=" << scene.objects.GetCount() << std::endl;
        return bytes;
    };
    {
        auto prepared = ReusableAssetService().PrepareModelAssetPlacement({root,project,coarse});
        if (!prepared.IsReady()) throw std::runtime_error(prepared.Result().error);
        if (!fallback.Register(profile.id,*prepared.PeekScene(),profile.collision,error))
            throw std::runtime_error(error);
        fallback.SetRepresentation(profile.id,TerrainPropProjection::Representation::Coarse);
        if (!fallback.Place(a,error) || !fallback.Place(b,error)) throw std::runtime_error(error);
    }
    const auto low = measure("coarse");
    acquire(detail,a);
    const auto shared = measure("fine_one_cell");
    acquire(detail,b);
    if (measure("fine_two_cells") != shared) return 41;
    detail.Release({0,0});
    if (measure("fine_one_remaining") != shared) return 42;
    detail.Release({1,0});
    if (measure("fine_released") != low || shared <= low) return 43;
    acquire(detail,b);
    if (measure("fine_reloaded") != shared) return 44;
    detail.Release({1,0});
    // Drive the same distance scheduler used by Runtime, with real coarse anchors.
    TerrainPropDocument doc;
    doc.layerId=layer;
    doc.nextPlacementId=3;
    doc.profiles.push_back(profile);
    doc.pages={a,b};
    auto world=TerrainPropCollisionWorld::Create(scene,{doc},false,error);
    if (!world) throw std::runtime_error(error);
    renegade::runtime::RuntimePropModelResidency scheduler;
    auto travel = [&](float x,const char* phase,uint64_t expected) {
        do { scheduler.Update(scene,1,world.get(),root,project,{x,0,0}); } while (!scheduler.Settled());
        size_t visible=0;
        for (size_t i=0; i<scene.objects.GetCount(); ++i) if (scene.objects[i].IsRenderable()) ++visible;
        if (visible!=2 || measure(phase)!=expected) throw std::runtime_error("Distance transition lost coverage or mesh residency");
    };
    travel(400,"distance_far",low);
    travel(0,"distance_near",shared);
    travel(400,"distance_released",low);
    travel(0,"distance_return",shared);

    return 0;
}

int main(int argc, char** argv)
{
    const bool meshDetail = argc == 6 && std::string(argv[2]) == "--mesh-detail";
    if (!meshDetail && (argc < 2 || argc > 4 || (argc == 4 && std::string(argv[2]) != "--governed-texture"))) return 2;
    const std::filesystem::path directory(argv[1]);
    const HWND window = CreateWindowExW(0, L"STATIC", L"Terrain prop probe",
        WS_POPUP, 0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return 4;
    wi::Application application;
    application.allow_hdr = false;
    application.SetWindow(window);
    auto* device = wi::graphics::GetDevice();
    if (!device) return 5;
    wi::initializer::InitializeComponentsImmediate();
    struct DrainDeviceWork
    {
        ~DrainDeviceWork()
        {
            while (wi::renderer::IsPipelineCreationActive()) Sleep(5);
            wi::jobsystem::ShutDown();
            wi::graphics::GetDevice()->WaitForGPU();
        }
    } drain;

    if (meshDetail) {
        try { return MeasureMeshDetail(argv[1],argv[3],argv[4],argv[5]); }
        catch (const std::exception& e) { std::cerr << e.what() << std::endl; return 40; }
    }
    if (argc == 4) return RunGovernedTextureProof(directory, argv[3]);
    if (argc == 3) return RunTerrainPropSlice(directory, argv[2]);
    return MeasureTerrainPropResidency(directory, nullptr);
}
