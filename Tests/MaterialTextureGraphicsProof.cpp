#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"

#include <WickedEngine.h>
#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    constexpr wchar_t WindowClassName[] =
        L"RenegadeLP08Gate3MaterialTextureProofWindow";
    constexpr const char* ProjectId =
        "82222222-2222-4222-8222-222222222222";

    LRESULT CALLBACK WindowProc(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        if (message == WM_CLOSE)
        {
            DestroyWindow(window);
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    bool Require(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "LP08 GATE 3 GRAPHICS FAIL // " << message << '\n';
        return false;
    }

    const std::vector<std::uint8_t>& PngBytes()
    {
        static const std::vector<std::uint8_t> bytes = {
            137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,
            0,0,0,1,0,0,0,1,8,4,0,0,0,181,28,12,2,
            0,0,0,11,73,68,65,84,120,218,99,100,248,15,0,1,
            5,1,1,39,24,227,102,0,0,0,0,73,69,78,68,174,66,96,130};
        return bytes;
    }

    bool PrepareProject(const fs::path& root, const fs::path& external)
    {
        std::error_code ec;
        fs::remove_all(root, ec);
        fs::create_directories(root / "Content", ec);
        if (ec) return false;
        fs::create_directories(root / "SourceAssets", ec);
        if (ec) return false;
        fs::create_directories(root / "Intermediate" / "Transactions", ec);
        if (ec) return false;
        fs::create_directories(external.parent_path(), ec);
        if (ec) return false;

        std::ofstream source(external, std::ios::binary | std::ios::trunc);
        const auto& png = PngBytes();
        source.write(reinterpret_cast<const char*>(png.data()),
            static_cast<std::streamsize>(png.size()));
        if (!source) return false;

        AssetRegistry registry;
        registry.projectId = ProjectId;
        registry.schemaVersion = AssetRegistry::CurrentSchemaVersion;
        std::string json;
        std::string error;
        if (!SerializeAssetRegistry(registry, json, error))
            return false;
        std::ofstream registryFile(root / AssetRegistryDocumentName,
            std::ios::binary | std::ios::trunc);
        registryFile << json;
        return static_cast<bool>(registryFile);
    }

    bool RunProof(const fs::path& outputRoot)
    {
        const fs::path projectRoot = outputRoot / "project";
        const fs::path external = outputRoot / "external" / "proof.png";
        if (!Require(PrepareProject(projectRoot, external),
                "could not prepare texture proof project"))
            return false;

        CreatorTextureWorkflowService creator;
        const auto imported = creator.ImportTexture(
            projectRoot.generic_u8string(), ProjectId,
            external.generic_u8string());
        if (!Require(imported.succeeded && imported.committed,
                "creator texture import failed: " + imported.error))
            return false;

        PreparedMaterialTextureAsset prepared;
        std::string error;
        if (!Require(PrepareMaterialTextureAsset(
                projectRoot.generic_u8string(), ProjectId,
                imported.assetId, prepared, error),
                "governed texture could not prepare: " + error))
            return false;

        wi::Resource direct = LoadPreparedMaterialTextureAsset(prepared, error);
        if (!Require(direct.IsValid() && direct.GetTexture().IsValid(),
                "pinned Wicked did not decode governed PNG memory payload: " + error))
            return false;

        wi::scene::Scene scene;
        const wi::ecs::Entity materialEntity = wi::ecs::CreateEntity();
        auto& material = scene.materials.Create(materialEntity);
        material.textures[wi::scene::MaterialComponent::BASECOLORMAP].uvset = 1;
        CommandService commands;
        auto command = std::make_unique<SetMaterialBaseColorTextureAssetCommand>(
            scene, materialEntity, prepared);
        if (!Require(commands.Execute(std::move(command)),
                "real Wicked texture assignment command failed"))
            return false;
        if (!Require(material.textures[
                    wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid() &&
                material.textures[
                    wi::scene::MaterialComponent::BASECOLORMAP].GetGPUResource() != nullptr &&
                material.textures[
                    wi::scene::MaterialComponent::BASECOLORMAP].name.empty() &&
                material.textures[
                    wi::scene::MaterialComponent::BASECOLORMAP].uvset == 1,
                "material did not receive a live governed Wicked texture"))
            return false;

        const fs::path scenePath = projectRoot / "Content" / "Gate3Graphics.wiscene";
        {
            wi::Archive archive(scenePath.generic_u8string(), false, false);
            if (!Require(archive.IsOpen(), "could not create graphics proof WISCENE"))
                return false;
            archive.SetCompressionEnabled(true);
            scene.Serialize(archive);
            if (!Require(archive.SaveFile(scenePath.generic_u8string()),
                    "could not save graphics proof WISCENE"))
                return false;
            archive = wi::Archive();
        }

        wi::scene::Scene reopened;
        {
            wi::Archive archive(scenePath.generic_u8string(), true, false);
            if (!Require(archive.IsOpen(), "could not reopen graphics proof WISCENE"))
                return false;
            reopened.Serialize(archive);
            if (!Require(archive.GetPos() == archive.GetSize(),
                    "graphics proof WISCENE did not deserialize completely"))
                return false;
        }

        std::vector<MaterialTextureBindingRecord> bindings;
        if (!Require(InspectMaterialTextureBindings(reopened, bindings, error) &&
                bindings.size() == 1 &&
                bindings.front().baseColorTextureAssetId == imported.assetId,
                "WISCENE lost stable governed texture identity: " + error))
            return false;

        wi::resourcemanager::Clear();
        const auto restored = RestoreMaterialTextureBindings(
            reopened, projectRoot.generic_u8string(), ProjectId);
        if (!Require(restored.succeeded && restored.discovered == 1 &&
                restored.restored == 1,
                "WISCENE could not rehydrate governed texture through Wicked: " +
                    restored.error))
            return false;
        auto* reopenedMaterial = reopened.materials.GetComponent(
            bindings.front().materialEntity);
        if (!Require(reopenedMaterial != nullptr &&
                reopenedMaterial->textures[
                    wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid() &&
                reopenedMaterial->textures[
                    wi::scene::MaterialComponent::BASECOLORMAP].GetGPUResource() != nullptr,
                "restored WISCENE material has no GPU texture"))
            return false;

        fs::remove(external);
        PreparedMaterialTextureAsset productOnly;
        if (!Require(PrepareMaterialTextureAsset(
                projectRoot.generic_u8string(), ProjectId,
                imported.assetId, productOnly, error) &&
                productOnly.payload == PngBytes(),
                "governed texture unexpectedly depended on the external source after import"))
            return false;

        return true;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: RenegadeMaterialTextureGraphicsProof <output-directory>\n";
        return 2;
    }
    const fs::path outputRoot = fs::absolute(fs::u8path(argv[1]));

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = WindowClassName;
    RegisterClassExW(&windowClass);
    const HWND window = CreateWindowExW(
        0, WindowClassName, L"Renegade LP08 Gate 3 Material Texture Proof",
        WS_OVERLAPPEDWINDOW, 0, 0, 64, 64,
        nullptr, nullptr, instance, nullptr);
    if (!Require(window != nullptr, "could not create graphics proof window"))
        return 3;

    int exitCode = 0;
    {
        wi::Application application;
        application.allow_hdr = false;
        application.SetWindow(window);
        if (!Require(wi::graphics::GetDevice() != nullptr,
                "Wicked graphics device was not initialized"))
            exitCode = 4;
        else if (!RunProof(outputRoot))
            exitCode = 5;
    }

    if (IsWindow(window))
        DestroyWindow(window);
    UnregisterClassW(WindowClassName, instance);
    if (exitCode == 0)
        std::cout << "LP08 GATE 3 MATERIAL TEXTURE GRAPHICS PASS // governed_png save_open\n";
    return exitCode;
}
