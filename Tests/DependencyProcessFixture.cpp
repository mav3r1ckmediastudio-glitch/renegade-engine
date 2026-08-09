#include "renegade/bridge/DependencyService.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
    using namespace renegade::bridge;

    class PackagedProofProvider final : public IDependencyProvider
    {
    public:
        const char* Name() const noexcept override { return "lp05-packaged-proof"; }
        std::uint32_t Version() const noexcept override { return 1; }
        bool Supports(DependencyClass) const noexcept override { return true; }

        bool Discover(
            const DependencyProviderContext& context,
            const DependencyCandidateSink& emit,
            const DependencyDiagnosticSink&,
            std::string& error) const override
        {
            if (context.source == nullptr)
            {
                error = "Packaged proof provider requires a source node.";
                return false;
            }
            const auto& path = context.source->projectRelativePath;
            if (path == "representative.renegade")
            {
                emit({"Content/Scenes/Startup.wiscene", DependencyClass::Scene,
                    DependencyRequirement::Required, "project.startup_scene", false});
                emit({"Content/Flow/Main.renegade-flow",
                    DependencyClass::StoryFlowDocument,
                    DependencyRequirement::Required, "project.startup_flow", false});
            }
            else if (path == "Content/Flow/Main.renegade-flow")
            {
                emit({"Content/Scenes/Secondary.wiscene", DependencyClass::Scene,
                    DependencyRequirement::Required, "flow.level[1]", false});
            }
            else if (path == "Content/Scenes/Startup.wiscene")
            {
                emit({"Content/Scripts/main.lua", DependencyClass::Script,
                    DependencyRequirement::Required, "scene.script[0]", false});
                emit({"Content/Textures/albedo.png", DependencyClass::Texture,
                    DependencyRequirement::Required, "scene.material[0]", false});
            }
            else if (path == "Content/Scenes/Secondary.wiscene")
            {
                emit({"Content/Scenes/Startup.wiscene", DependencyClass::Scene,
                    DependencyRequirement::Required, "scene.return_to_start", false});
            }
            else if (path == "Content/Scripts/main.lua")
            {
                emit({"Content/Scripts/shared.lua", DependencyClass::Script,
                    DependencyRequirement::Required, "lua.declared[0]", false});
            }
            else if (path == "Content/Scripts/shared.lua")
            {
                emit({"Content/Scripts/main.lua", DependencyClass::Script,
                    DependencyRequirement::Required, "lua.cycle", false});
                emit({"Content/Data/missing.json", DependencyClass::Data,
                    DependencyRequirement::Required, "lua.data", false});
            }
            error.clear();
            return true;
        }
    };
}

int main(int argc, char** argv)
{
    namespace fs = std::filesystem;
    if (argc != 3)
    {
        std::cerr << "Usage: RenegadeDependencyProcessFixture <project-root> <output-json>\n";
        return 2;
    }

    const fs::path projectRoot = fs::absolute(fs::u8path(argv[1])).lexically_normal();
    const fs::path outputPath = fs::absolute(fs::u8path(argv[2])).lexically_normal();
    if (!fs::is_regular_file(projectRoot / "representative.renegade"))
    {
        std::cerr << "Packaged proof project descriptor is missing.\n";
        return 3;
    }
    const auto relativeOutput = outputPath.lexically_relative(projectRoot);
    if (!relativeOutput.empty() && *relativeOutput.begin() != "..")
    {
        std::cerr << "Proof output must be outside the read-only project fixture.\n";
        return 4;
    }

    PackagedProofProvider provider;
    DependencyCollector collector(projectRoot.generic_u8string());
    std::string error;
    if (!collector.RegisterProvider(provider, error) ||
        !collector.AddRoot({"representative.renegade",
            DependencyClass::ProjectDocument,
            DependencyRequirement::Required,
            "lp05.packaged.root"}, error) ||
        !collector.DiscoverTransitiveDependencies(error))
    {
        std::cerr << error << '\n';
        return 5;
    }
    const auto& graph = collector.Graph();
    if (graph.nodes.size() != 8 || graph.edges.size() != 9 ||
        graph.diagnostics.size() != 3)
    {
        std::cerr << "Packaged proof closure is incomplete.\n";
        return 6;
    }

    std::string json;
    if (!SerializeDependencyGraph(graph, json, error))
    {
        std::cerr << error << '\n';
        return 7;
    }
    fs::create_directories(outputPath.parent_path());
    std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
    output.write(json.data(), static_cast<std::streamsize>(json.size()));
    if (!output)
    {
        std::cerr << "Could not write packaged proof graph.\n";
        return 8;
    }

    std::cout << "LP05_GATE8_PASS nodes=" << graph.nodes.size()
              << " edges=" << graph.edges.size()
              << " diagnostics=" << graph.diagnostics.size()
              << '\n';
    return 0;
}
