#include "renegade/bridge/ModelImporterFailureAdapter.h"

#include <WickedEngine.h>
#include <ModelImporter.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace
{
    bool Require(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "LP07 IMPORT FAILURE ADAPTER FAIL // " << message << '\n';
        return false;
    }

    bool WriteText(const fs::path& path, const std::string& text)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            return false;
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(output);
    }

    bool RunFbxFailure(const fs::path& path)
    {
        using namespace renegade::bridge;
        ClearWickedModelImporterFailureDiagnostic();
        wi::scene::Scene scene;
        ImportModel_FBX(path.generic_u8string(), scene);
        const std::string diagnostic =
            ConsumeWickedModelImporterFailureDiagnostic();
        return Require(!diagnostic.empty(),
                "malformed FBX did not reach Renegade's non-modal adapter") &&
            Require(diagnostic.find("FBX") != std::string::npos,
                "malformed FBX diagnostic lost its converter context") &&
            Require(scene.meshes.GetCount() == 0 && scene.objects.GetCount() == 0,
                "malformed FBX unexpectedly produced reusable scene content");
    }

    bool RunGltfFailure(const fs::path& path)
    {
        using namespace renegade::bridge;
        ClearWickedModelImporterFailureDiagnostic();
        wi::scene::Scene scene;
        ImportModel_GLTF(path.generic_u8string(), scene);
        const std::string diagnostic =
            ConsumeWickedModelImporterFailureDiagnostic();
        return Require(!diagnostic.empty(),
                "malformed GLTF did not reach Renegade's non-modal adapter") &&
            Require(diagnostic.find("glTF") != std::string::npos,
                "malformed GLTF diagnostic lost its converter context") &&
            Require(scene.meshes.GetCount() == 0 && scene.objects.GetCount() == 0,
                "malformed GLTF unexpectedly produced reusable scene content");
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: RenegadeModelImporterFailureAdapterTests <output-directory>\n";
        return 2;
    }

    const fs::path outputRoot = fs::u8path(argv[1]);
    std::error_code ec;
    fs::remove_all(outputRoot, ec);
    ec.clear();
    fs::create_directories(outputRoot, ec);
    if (!Require(!ec, "could not create malformed-input test directory"))
        return 3;

    const fs::path badFbx = outputRoot / "malformed.fbx";
    const fs::path badGltf = outputRoot / "malformed.gltf";
    if (!Require(WriteText(badFbx, "this is not an FBX file"),
            "could not write malformed FBX fixture") ||
        !Require(WriteText(badGltf, "{"),
            "could not write malformed GLTF fixture"))
    {
        fs::remove_all(outputRoot, ec);
        return 4;
    }

    const bool fbxPassed = RunFbxFailure(badFbx);
    const bool gltfPassed = RunGltfFailure(badGltf);
    fs::remove_all(outputRoot, ec);
    if (!fbxPassed || !gltfPassed)
        return 5;

    std::cout << "LP07 IMPORT FAILURE ADAPTER PASS\n";
    return 0;
}
