#include "renegade/bridge/ImportService.h"

#include <iostream>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "RenegadeImportTests: " << message << '\n';
        return 1;
    }
}

int main()
{
    renegade::bridge::ImportService imports;

    const auto missing = imports.PrepareGltfAsset(
        "missing.glb",
        "missing.wiscene");
    if (missing.IsReady() ||
        missing.Result().error.find("does not exist") == std::string::npos)
    {
        return Fail("missing source validation did not fail clearly");
    }

    const auto unsupported = imports.PrepareGltfAsset(
        "model.fbx",
        "model.wiscene");
    if (unsupported.IsReady() ||
        unsupported.Result().error.find("only accepts") == std::string::npos)
    {
        return Fail("unsupported format validation did not fail clearly");
    }

    const auto wrongDestination = imports.PrepareGltfAsset(
        "model.glb",
        "model.asset");
    if (wrongDestination.IsReady() ||
        wrongDestination.Result().error.find(".wiscene") == std::string::npos)
    {
        return Fail("destination format validation did not fail clearly");
    }

    wi::scene::Scene fixture;
    const auto root = wi::ecs::CreateEntity();
    fixture.names.Create(root) = "Imported Root";
    fixture.transforms.Create(root);
    const auto summary = renegade::bridge::ImportService::Summarize(fixture);
    if (summary.names != 1 || summary.transforms != 1 || summary.meshes != 0)
    {
        return Fail("scene structural summary is incorrect");
    }

    const auto incomplete = imports.CompleteGltfAsset({});
    if (incomplete.succeeded || incomplete.error.find("not ready") == std::string::npos)
    {
        return Fail("unprepared WISCENE completion did not fail clearly");
    }

    std::cout << "RenegadeImportTests passed\n";
    return 0;
}
