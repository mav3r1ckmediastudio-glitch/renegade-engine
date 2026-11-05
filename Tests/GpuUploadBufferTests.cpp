#include <WickedEngine.h>
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
static void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

// Independently reproduce the illegal descriptor through DX12 itself. This
// does not call Wicked's assert-on-failure wrapper or suppress any assertion.
static void VerifyUploadHeapRules()
{
    ComPtr<IDXGIFactory4> factory;
    Require(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))), "DXGI factory failed");
    ComPtr<IDXGIAdapter> warp;
    Require(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))), "WARP adapter missing");
    ComPtr<ID3D12Device> device;
    Require(SUCCEEDED(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device))), "WARP DX12 device failed");
    D3D12_FEATURE_DATA_ARCHITECTURE architecture{};
    Require(SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE,
        &architecture, sizeof(architecture))), "WARP architecture query failed");
    std::cout << "WARP_ARCHITECTURE UMA=" << architecture.UMA
              << " CacheCoherentUMA=" << architecture.CacheCoherentUMA << '\n';

    D3D12_HEAP_DESC heapDesc{};
    heapDesc.SizeInBytes = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    heapDesc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapDesc.Properties.CreationNodeMask = 1;
    heapDesc.Properties.VisibleNodeMask = 1;
    heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
    ComPtr<ID3D12Heap> heap;
    Require(SUCCEEDED(device->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap))),
        "upload heap creation failed");
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    ComPtr<ID3D12Resource> resource;
    HRESULT hr = device->CreatePlacedResource(heap.Get(), 0, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
    std::cout << "UPLOAD_UAV_NEGATIVE_HRESULT=0x" << std::hex
              << static_cast<unsigned>(hr) << std::dec << '\n';
    Require(hr == E_INVALIDARG && resource.Get() == nullptr,
        "illegal upload/UAV descriptor did not fail with E_INVALIDARG");
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    hr = device->CreatePlacedResource(heap.Get(), 0, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
    Require(SUCCEEDED(hr) && resource.Get() != nullptr, "read-only upload descriptor was rejected");
    void* mapped = nullptr;
    const D3D12_RANGE noRead{0, 0};
    Require(SUCCEEDED(resource->Map(0, &noRead, &mapped)) && mapped,
        "read-only GPU upload buffer did not map for CPU writes");
    std::memset(mapped, 0x5A, static_cast<std::size_t>(desc.Width));
    const D3D12_RANGE written{0, static_cast<SIZE_T>(desc.Width)};
    resource->Unmap(0, &written);
    std::cout << "NATIVE_DX12_UPLOAD_NEGATIVE_AND_POSITIVE=PASS\n";
}

int main()
{
    std::cout.setf(std::ios::unitbuf);
    try
    {
        VerifyUploadHeapRules();
        HWND window = CreateWindowExW(0, L"STATIC", L"GPU upload allocator regression",
            WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr,
            GetModuleHandleW(nullptr), nullptr);
        Require(window != nullptr, "native graphics fixture window failed");
        wi::Application app;
        app.allow_hdr = false;
        app.SetWindow(window);
        wi::initializer::InitializeComponentsImmediate();
        auto* device = wi::graphics::GetDevice();
        Require(device != nullptr, "native graphics device missing");
        struct Drain
        {
            ~Drain()
            {
                while (wi::renderer::IsPipelineCreationActive()) Sleep(10);
                wi::graphics::GetDevice()->WaitForGPU();
            }
        } drain;
        using namespace wi::graphics;
        const bool coherent = device->CheckCapability(GraphicsDeviceCapability::CACHE_COHERENT_UMA);
        std::cout << "ALLOCATOR_ADAPTER " << device->GetAdapterName()
                  << " CacheCoherentUMA=" << coherent << '\n';
        auto allocation = wi::renderer::SuballocateGPUBuffer(65536);
        Require(allocation.IsValid() && allocation.alias.IsValid(),
            "production GPU suballocator failed");
        const auto& parent = allocation.alias.GetDesc();
        Require(parent.usage == (coherent ? Usage::UPLOAD : Usage::DEFAULT),
            "allocator changed the native memory strategy");
        Require(has_flag(parent.bind_flags, BindFlag::UNORDERED_ACCESS) == !coherent,
            "allocator retained an illegal upload UAV or removed DEFAULT UAV support");
        std::array<std::uint8_t, 65536> bytes;
        bytes.fill(0x5A);
        GPUBufferDesc childDesc;
        childDesc.size = bytes.size();
        childDesc.usage = parent.usage;
        childDesc.bind_flags = BindFlag::VERTEX_BUFFER | BindFlag::INDEX_BUFFER | BindFlag::SHADER_RESOURCE;
        childDesc.misc_flags = ResourceMiscFlag::BUFFER_RAW | ResourceMiscFlag::NO_DEFAULT_DESCRIPTORS;
        GPUBuffer child;
        Require(device->CreateBuffer(&childDesc, bytes.data(), &child,
            &allocation.alias, allocation.allocation.byte_offset) && child.IsValid(),
            "native mesh-style aliased buffer creation failed");
        if (coherent)
            Require(child.mapped_data && std::memcmp(child.mapped_data, bytes.data(), bytes.size()) == 0,
                "UMA aliased buffer lost its CPU-uploaded contents");
        device->WaitForGPU();
        std::cout << "PRODUCTION_GPU_ALLOCATOR_AND_MESH_ALIAS=PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
