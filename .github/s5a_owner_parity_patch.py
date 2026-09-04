from pathlib import Path


def replace(path, old, new, expected=1):
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    actual = text.count(old)
    if actual != expected:
        raise SystemExit(
            f"{path}: expected {expected} occurrence(s), found {actual}: {old[:160]!r}"
        )
    p.write_text(text.replace(old, new, expected), encoding="utf-8")


def insert_before(path, marker, block):
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    actual = text.count(marker)
    if actual != 1:
        raise SystemExit(
            f"{path}: expected one insertion marker, found {actual}: {marker[:160]!r}"
        )
    p.write_text(text.replace(marker, block + marker, 1), encoding="utf-8")


# Test Level owns the exact live scripting authority. No StudioSession::Current()
# discovery and no silent disk fallback when Studio supplied a live service.
replace(
    "EngineBridge/include/renegade/bridge/TestLevelSnapshotService.h",
    "    class CommandService;\n    struct ProjectMetadata;\n    class SceneService;\n",
    "    class CommandService;\n    struct ProjectMetadata;\n    class SceneService;\n    class ScriptAuthoringService;\n",
)
replace(
    "EngineBridge/include/renegade/bridge/TestLevelSnapshotService.h",
    "        TestLevelSnapshotService(\n            SceneService& scenes,\n            const CommandService& commands) noexcept;\n",
    "        TestLevelSnapshotService(\n            SceneService& scenes,\n            const CommandService& commands,\n            ScriptAuthoringService* scripts = nullptr) noexcept;\n",
)
replace(
    "EngineBridge/include/renegade/bridge/TestLevelSnapshotService.h",
    "        SceneService& scenes_;\n        const CommandService& commands_;\n",
    "        SceneService& scenes_;\n        const CommandService& commands_;\n        ScriptAuthoringService* scripts_ = nullptr;\n",
)

replace(
    "EngineBridge/src/TestLevelSnapshotService.cpp",
    '#include "renegade/bridge/CommandService.h"\n',
    '#include "renegade/bridge/CommandService.h"\n#include "renegade/bridge/AssetRegistryService.h"\n#include "renegade/bridge/MaterialTextureAssetService.h"\n',
)
replace(
    "EngineBridge/src/TestLevelSnapshotService.cpp",
    '#include "renegade/bridge/StudioSession.h"\n',
    "",
)
replace(
    "EngineBridge/src/TestLevelSnapshotService.cpp",
    "#include <atomic>\n",
    "#include <algorithm>\n#include <atomic>\n#include <unordered_set>\n#include <vector>\n",
)

material_helper = r'''    bool IsSafeSnapshotContentPath(const fs::path& value)
    {
        const fs::path normalized = value.lexically_normal();
        if (normalized.empty() || normalized.is_absolute() ||
            normalized.has_root_name() || normalized.has_root_directory())
        {
            return false;
        }
        auto part = normalized.begin();
        if (part == normalized.end() || (*part).generic_u8string() != "Content")
            return false;
        for (; part != normalized.end(); ++part)
        {
            if (*part == "." || *part == "..")
                return false;
        }
        return true;
    }

    bool SnapshotGovernedMaterialInputs(
        const renegade::bridge::ProjectMetadata& project,
        const renegade::bridge::TestLevelSnapshot& snapshot,
        const wi::scene::Scene& scene,
        std::string& error)
    {
        using namespace renegade::bridge;

        std::vector<MaterialTextureBindingRecord> bindings;
        if (!InspectMaterialTextureBindings(scene, bindings, error))
        {
            error = "Could not inspect governed material bindings for Test Level: " + error;
            return false;
        }
        if (bindings.empty())
        {
            error.clear();
            return true;
        }

        AssetRegistry registry;
        if (!ReadAssetRegistry(project.rootPath, project.projectId, registry, error))
        {
            error = "Could not read the governed asset registry for Test Level: " + error;
            return false;
        }

        std::string registrySourceText;
        if (!ResolveAssetRegistryDocumentPath(project.rootPath, registrySourceText, error))
        {
            error = "Could not resolve the governed asset registry for Test Level: " + error;
            return false;
        }

        const fs::path sourceRoot = fs::u8path(project.rootPath);
        const fs::path snapshotRoot = fs::u8path(snapshot.sessionDirectory);
        std::error_code ec;
        fs::copy_file(
            fs::u8path(registrySourceText),
            snapshotRoot / AssetRegistryDocumentName,
            fs::copy_options::overwrite_existing,
            ec);
        if (ec)
        {
            error = "Could not snapshot the governed asset registry: " + ec.message();
            return false;
        }

        std::unordered_set<StableId> copiedAssets;
        for (const auto& binding : bindings)
        {
            if (!copiedAssets.insert(binding.textureAssetId).second)
                continue;

            const auto record = std::find_if(
                registry.records.begin(), registry.records.end(),
                [&](const AssetRecord& candidate)
                {
                    return candidate.assetId == binding.textureAssetId;
                });
            if (record == registry.records.end() ||
                record->dependencyClass != DependencyClass::Texture ||
                !record->sourceAvailable)
            {
                error = "Test Level material binding references an unavailable governed texture: " +
                    binding.textureAssetId;
                return false;
            }

            const fs::path relative =
                fs::u8path(record->projectRelativePath).lexically_normal();
            if (!IsSafeSnapshotContentPath(relative))
            {
                error = "Governed Test Level texture product escaped Content: " +
                    record->projectRelativePath;
                return false;
            }

            ec.clear();
            const fs::path source = sourceRoot / relative;
            const fs::file_status status = fs::symlink_status(source, ec);
            if (ec || fs::is_symlink(status) || !fs::is_regular_file(status))
            {
                error = "Governed Test Level texture product is unavailable: " +
                    source.generic_u8string();
                if (ec)
                    error += ": " + ec.message();
                return false;
            }

            const fs::path destination = snapshotRoot / relative;
            fs::create_directories(destination.parent_path(), ec);
            if (!ec)
            {
                fs::copy_file(
                    source,
                    destination,
                    fs::copy_options::overwrite_existing,
                    ec);
            }
            if (ec)
            {
                error = "Could not snapshot governed Test Level texture product: " +
                    source.generic_u8string() + ": " + ec.message();
                return false;
            }
        }

        error.clear();
        return true;
    }

'''
insert_before(
    "EngineBridge/src/TestLevelSnapshotService.cpp",
    "    bool SnapshotCreatorScripts(\n",
    material_helper,
)

replace(
    "EngineBridge/src/TestLevelSnapshotService.cpp",
    "    bool SnapshotCreatorScripts(\n        const renegade::bridge::ProjectMetadata& project,\n        const renegade::bridge::TestLevelSnapshot& snapshot,\n        renegade::bridge::SceneService& scenes,\n        const renegade::bridge::CommandService& commands,\n        std::string& error)\n",
    "    bool SnapshotCreatorScripts(\n        const renegade::bridge::ProjectMetadata& project,\n        const renegade::bridge::TestLevelSnapshot& snapshot,\n        renegade::bridge::SceneService& scenes,\n        renegade::bridge::ScriptAuthoringService* scripts,\n        std::string& error)\n",
)

old_live = r'''        StudioSession* session = StudioSession::Current();
        if (session != nullptr &&
            &session->Scenes() == &scenes &&
            &session->Commands() == &commands &&
            session->Projects().HasProject() &&
            session->Projects().CurrentProject().projectId == project.projectId &&
            session->Scripts().IsLoaded() &&
            fs::u8path(session->Scripts().LoadedScenePath()).lexically_normal() ==
                fs::u8path(sourceScenePath).lexically_normal())
        {
            const ScriptDocument* live = session->Scripts().Document();
            if (live == nullptr)
            {
                error =
                    "Studio reported loaded creator scripting without a live document.";
                return false;
            }
            document = *live;
            haveDocument = true;
            liveAuthority = true;
        }
'''
new_live = r'''        if (scripts != nullptr)
        {
            if (!scripts->EnsureCurrent(error))
            {
                error = "Could not bind live creator scripting for Test Level: " + error;
                return false;
            }
            const ScriptDocument* live = scripts->Document();
            if (live == nullptr)
            {
                error =
                    "Studio scripting authority did not provide a live document.";
                return false;
            }

            std::error_code pathError;
            const bool sameScene = fs::equivalent(
                fs::u8path(scripts->LoadedScenePath()),
                fs::u8path(sourceScenePath),
                pathError);
            if (pathError || !sameScene)
            {
                error =
                    "Live creator scripting is bound to a different Scene than Test Level.";
                return false;
            }

            document = *live;
            haveDocument = true;
            liveAuthority = true;
        }
'''
replace("EngineBridge/src/TestLevelSnapshotService.cpp", old_live, new_live)

replace(
    "EngineBridge/src/TestLevelSnapshotService.cpp",
    "    TestLevelSnapshotService::TestLevelSnapshotService(\n        SceneService& scenes,\n        const CommandService& commands) noexcept\n        : scenes_(scenes),\n          commands_(commands)\n",
    "    TestLevelSnapshotService::TestLevelSnapshotService(\n        SceneService& scenes,\n        const CommandService& commands,\n        ScriptAuthoringService* scripts) noexcept\n        : scenes_(scenes),\n          commands_(commands),\n          scripts_(scripts)\n",
)

old_snapshot_call = r'''            if (!SnapshotCreatorScripts(
                    project,
                    created,
                    scenes_,
                    commands_,
                    error))
            {
                return failAndCleanup(
                    "Could not snapshot creator scripting state: " + error);
            }

            const fs::path descriptorPath =
'''
new_snapshot_call = r'''            if (!SnapshotCreatorScripts(
                    project,
                    created,
                    scenes_,
                    scripts_,
                    error))
            {
                return failAndCleanup(
                    "Could not snapshot creator scripting state: " + error);
            }

            if (!SnapshotGovernedMaterialInputs(
                    project,
                    created,
                    scenes_.GetScene(),
                    error))
            {
                return failAndCleanup(
                    "Could not snapshot governed material state: " + error);
            }

            const fs::path descriptorPath =
'''
replace("EngineBridge/src/TestLevelSnapshotService.cpp", old_snapshot_call, new_snapshot_call)

# Production Studio passes the exact ScriptAuthoringService it owns.
replace(
    "Studio/src/StudioApplication.cpp",
    "        bridge::TestLevelSnapshotService snapshotService(\n            session_->Scenes(),\n            session_->Commands());\n",
    "        bridge::TestLevelSnapshotService snapshotService(\n            session_->Scenes(),\n            session_->Commands(),\n            &session_->Scripts());\n",
)

# Explicit Runtime/Test Level launches must rehydrate governed material resources.
replace(
    "Runtime/src/RuntimeBootstrap.h",
    '#include "renegade/bridge/FlowService.h"\n',
    '#include "renegade/bridge/FlowService.h"\n#include "renegade/bridge/MaterialTextureAssetService.h"\n',
)
replace(
    "Runtime/src/RuntimeBootstrap.h",
    "    // Gate 6 post-load package refresh. Explicit --project/Test Level launches\n    // are unchanged; package-relative launches resolve and refresh every saved\n    // reusable asset instance from the already integrity-validated package.\n    [[nodiscard]] bool RefreshRuntimeReusableAssets(\n        bridge::SceneService& scenes,\n        RuntimeBootstrapResult& result,\n        std::string& error);\n",
    "    // Post-load governed resource refresh. Explicit --project/Test Level\n    // launches rehydrate material textures from their authored/shadow project;\n    // package-relative launches retain the integrity-validated package refresh.\n    [[nodiscard]] bool RefreshRuntimeReusableAssets(\n        bridge::SceneService& scenes,\n        RuntimeBootstrapResult& result,\n        std::string& error,\n        bridge::MaterialTextureResourceLoader authoringTextureLoader = {});\n",
)
replace(
    "Runtime/src/RuntimeBootstrap.h",
    "    [[nodiscard]] RuntimeBootstrapResult LoadRuntimeProjectScene(\n        bridge::SceneService& scenes,\n        RuntimeBootstrapResult result);\n",
    "    [[nodiscard]] RuntimeBootstrapResult LoadRuntimeProjectScene(\n        bridge::SceneService& scenes,\n        RuntimeBootstrapResult result,\n        bridge::MaterialTextureResourceLoader authoringTextureLoader = {});\n",
)

replace(
    "Runtime/src/RuntimeReusableAssets.cpp",
    '#include "renegade/bridge/ResourceAssetRuntimeService.h"\n',
    '#include "renegade/bridge/ResourceAssetRuntimeService.h"\n#include "renegade/bridge/MaterialTextureAssetService.h"\n',
)
replace(
    "Runtime/src/RuntimeReusableAssets.cpp",
    "#include <sstream>\n",
    "#include <sstream>\n#include <utility>\n",
)
replace(
    "Runtime/src/RuntimeReusableAssets.cpp",
    "    bool RefreshRuntimeReusableAssets(\n        bridge::SceneService& scenes,\n        RuntimeBootstrapResult& result,\n        std::string& error)\n",
    "    bool RefreshRuntimeReusableAssets(\n        bridge::SceneService& scenes,\n        RuntimeBootstrapResult& result,\n        std::string& error,\n        bridge::MaterialTextureResourceLoader authoringTextureLoader)\n",
)
replace(
    "Runtime/src/RuntimeReusableAssets.cpp",
    "        // Studio Test Level and other explicit --project launches continue to\n        // consume their already-authored scene exactly as before. Package-only\n        // refresh is reserved for the integrity-validated LP06 launch path.\n        if (!result.packageRelativeLaunch)\n        {\n            error.clear();\n            return true;\n        }\n",
    "        // Authored/Test Level WISCENEs persist governed texture stable IDs,\n        // not live Wicked Resource handles. Rehydrate those bindings from the\n        // explicit project root before gameplay starts. Test Level snapshots\n        // carry the registry plus only the referenced governed texture products.\n        if (!result.packageRelativeLaunch)\n        {\n            const auto restored = bridge::RestoreMaterialTextureBindings(\n                scenes.GetScene(),\n                result.project.rootPath,\n                result.project.projectId,\n                std::move(authoringTextureLoader));\n            if (!restored.succeeded)\n            {\n                error = \"Authored Runtime governed material restore failed: \" +\n                    restored.error;\n                return false;\n            }\n            error.clear();\n            return true;\n        }\n",
)

replace(
    "Runtime/src/RuntimeBootstrap.cpp",
    "    RuntimeBootstrapResult LoadRuntimeProjectScene(\n        bridge::SceneService& scenes,\n        RuntimeBootstrapResult result)\n",
    "    RuntimeBootstrapResult LoadRuntimeProjectScene(\n        bridge::SceneService& scenes,\n        RuntimeBootstrapResult result,\n        bridge::MaterialTextureResourceLoader authoringTextureLoader)\n",
)
replace(
    "Runtime/src/RuntimeBootstrap.cpp",
    "        if (!RefreshRuntimeReusableAssets(scenes, result, refreshError))\n",
    "        if (!RefreshRuntimeReusableAssets(\n                scenes, result, refreshError, std::move(authoringTextureLoader)))\n",
)

# S5 owner workflow explicitly exercises the live ScriptAuthoringService path.
replace(
    "Tests/S5CoreGameplayLuaTests.cpp",
    "    TestLevelSnapshotService snapshots(\n        session.Scenes(),\n        session.Commands());\n",
    "    TestLevelSnapshotService snapshots(\n        session.Scenes(),\n        session.Commands(),\n        &session.Scripts());\n",
)

# Extend real Test Level snapshot->Runtime proof with governed material parity.
replace(
    "Tests/TestLevelSnapshotRuntimeTests.cpp",
    '#include "RuntimeBootstrap.h"\n#include "renegade/bridge/CommandService.h"\n',
    '#include "RuntimeBootstrap.h"\n#include "renegade/bridge/AssetRegistryService.h"\n#include "renegade/bridge/CommandService.h"\n#include "renegade/bridge/CreatorTextureWorkflowService.h"\n#include "renegade/bridge/MaterialTextureAssetService.h"\n',
)

test_helpers = r'''    const std::vector<std::uint8_t>& PngBytes()
    {
        static const std::vector<std::uint8_t> bytes = {
            137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,
            0,0,0,1,0,0,0,1,8,4,0,0,0,181,28,12,2,
            0,0,0,11,73,68,65,84,120,218,99,100,248,15,0,1,
            5,1,1,39,24,227,102,0,0,0,0,73,69,78,68,174,66,96,130};
        return bytes;
    }

    bool WriteBytes(const fs::path& path, const std::vector<std::uint8_t>& bytes)
    {
        fs::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        return static_cast<bool>(stream);
    }

    bool WriteEmptyRegistry(
        const fs::path& root,
        const renegade::bridge::StableId& projectId)
    {
        renegade::bridge::AssetRegistry registry;
        registry.projectId = projectId;
        registry.schemaVersion =
            renegade::bridge::AssetRegistry::CurrentSchemaVersion;
        std::string json;
        std::string error;
        if (!renegade::bridge::SerializeAssetRegistry(registry, json, error))
            return false;
        std::ofstream stream(
            root / renegade::bridge::AssetRegistryDocumentName,
            std::ios::binary | std::ios::trunc);
        stream << json;
        return static_cast<bool>(stream);
    }

    wi::Resource FakeTextureLoader(
        const renegade::bridge::PreparedMaterialTextureAsset& prepared,
        std::string& error)
    {
        wi::vector<std::uint8_t> bytes;
        bytes.assign(prepared.payload.begin(), prepared.payload.end());
        wi::Resource resource;
        resource.SetFileData(std::move(bytes));
        if (!resource.IsValid())
        {
            error = "fake Test Level texture loader could not retain payload bytes";
            return {};
        }
        error.clear();
        return resource;
    }

'''
insert_before(
    "Tests/TestLevelSnapshotRuntimeTests.cpp",
    "    bool FindTranslationX(\n",
    test_helpers,
)

texture_setup = r'''    fs::create_directories(projectRoot / "SourceAssets");
    fs::create_directories(projectRoot / "Intermediate" / "Transactions");
    if (!WriteEmptyRegistry(projectRoot, project.projectId))
    {
        return Fail("LP04 material parity fixture could not create the asset registry");
    }

    const fs::path externalTexture = fixture.path / "test-level-base.png";
    if (!WriteBytes(externalTexture, PngBytes()))
        return Fail("LP04 material parity fixture could not write its texture source");

    renegade::bridge::CreatorTextureWorkflowService textureWorkflow;
    const auto importedTexture = textureWorkflow.ImportTexture(
        projectRoot.generic_u8string(),
        project.projectId,
        externalTexture.generic_u8string());
    if (!importedTexture.succeeded || !importedTexture.committed)
    {
        std::cerr << importedTexture.error << '\n';
        return Fail("LP04 material parity fixture could not import a governed texture");
    }

    renegade::bridge::PreparedMaterialTextureAsset preparedTexture;
    std::string materialError;
    if (!renegade::bridge::PrepareMaterialTextureAsset(
            projectRoot.generic_u8string(),
            project.projectId,
            importedTexture.assetId,
            preparedTexture,
            materialError))
    {
        std::cerr << materialError << '\n';
        return Fail("LP04 material parity fixture could not prepare its governed texture");
    }

'''
insert_before(
    "Tests/TestLevelSnapshotRuntimeTests.cpp",
    "    renegade::bridge::StudioSession session;\n",
    texture_setup,
)

material_apply = r'''    const wi::ecs::Entity governedMaterial = wi::ecs::CreateEntity();
    session.Scenes().GetScene().materials.Create(governedMaterial);
    if (!renegade::bridge::ApplyPreparedMaterialTextureAsset(
            session.Scenes().GetScene(),
            governedMaterial,
            renegade::bridge::MaterialTextureSlot::BaseColor,
            preparedTexture,
            FakeTextureLoader,
            materialError))
    {
        std::cerr << materialError << '\n';
        return Fail("LP04 material parity fixture could not bind its governed texture");
    }

'''
insert_before(
    "Tests/TestLevelSnapshotRuntimeTests.cpp",
    "    if (!session.SaveScene(scenePath.generic_u8string()))\n",
    material_apply,
)

snapshot_material_assert = r'''    const fs::path snapshotRoot = fs::u8path(snapshot.sessionDirectory);
    if (!fs::is_regular_file(
            snapshotRoot / renegade::bridge::AssetRegistryDocumentName) ||
        !fs::is_regular_file(
            snapshotRoot / fs::u8path(importedTexture.assetProjectRelativePath)))
    {
        return Fail("LP04 Test Level snapshot omitted governed material Runtime inputs");
    }

'''
insert_before(
    "Tests/TestLevelSnapshotRuntimeTests.cpp",
    "    renegade::bridge::ProjectService inspector;\n",
    snapshot_material_assert,
)

replace(
    "Tests/TestLevelSnapshotRuntimeTests.cpp",
    "    auto loaded = renegade::runtime::LoadRuntimeProjectScene(\n        runtimeScenes,\n        resolved);\n",
    "    auto loaded = renegade::runtime::LoadRuntimeProjectScene(\n        runtimeScenes,\n        resolved,\n        FakeTextureLoader);\n",
)

runtime_material_assert = r'''    std::vector<renegade::bridge::MaterialTextureBindingRecord> runtimeBindings;
    std::string runtimeMaterialError;
    if (!renegade::bridge::InspectMaterialTextureBindings(
            runtimeScenes.GetScene(), runtimeBindings, runtimeMaterialError) ||
        runtimeBindings.size() != 1)
    {
        std::cerr << runtimeMaterialError << '\n';
        return Fail("Runtime did not preserve the Test Level governed material binding");
    }
    const auto* runtimeMaterial = runtimeScenes.GetScene().materials.GetComponent(
        runtimeBindings.front().materialEntity);
    if (runtimeMaterial == nullptr ||
        !runtimeMaterial->textures[
            wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid())
    {
        return Fail("Runtime did not rehydrate the Test Level governed material resource");
    }

'''
insert_before(
    "Tests/TestLevelSnapshotRuntimeTests.cpp",
    "    float runtimeX = 0.0f;\n",
    runtime_material_assert,
)

print("Applied S5A owner/Test Level material parity repair.")
