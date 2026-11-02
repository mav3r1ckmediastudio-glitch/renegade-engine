#pragma once

// Included by the native importer graphics proof after its shared helpers.
namespace
{
    bool RunImportReliabilityProof(const fs::path& root, const fs::path& fixtures, bool cold)
    {
        const StableId projectId = "88888888-8888-4888-8888-888888888888";
        const auto project = root / "project";
        const auto sources = root / "sources";
        auto write = [](const fs::path& path, const std::string& value) {
            fs::create_directories(path.parent_path());
            std::ofstream stream(path, std::ios::binary);
            stream << value;
        };
        if (cold)
        {
            nlohmann::json records;
            std::ifstream(root / "audit-records.json") >> records;
            for (const auto& record : records)
            {
                const auto id = record.at("id").get<std::string>();
                if (!VerifyPlacement(project, projectId, id)) return false;
                ReusableModelAssetDocument document;
                std::string error;
                if (!Require(ReadReusableModelAssetDocument(
                    (project / fs::u8path(record.at("asset").get<std::string>())).generic_u8string(),
                    document, error), error)) return false;
                CreatorModelImportRecipe recipe;
                const auto settings = nlohmann::json::parse(document.manifest.settingsJson);
                if (!Require(ParseCreatorModelImportOptions(settings.at("options").dump(), recipe, error), error))
                    return false;
                auto reimported = ModelImportCandidateService().PrepareModel(
                    (project / fs::u8path(record.at("source").get<std::string>())).generic_u8string());
                if (!Require(reimported.IsReady(), reimported.Error()) ||
                    !Require(ApplyCreatorModelImportRecipe(*reimported.PeekMutableScene(),
                        project.generic_u8string(), projectId, recipe, error), error)) return false;
                size_t maps = 0;
                for (size_t m = 0; m < reimported.PeekScene()->materials.GetCount(); ++m)
                    for (const auto& texture : reimported.PeekScene()->materials[m].textures)
                        if (!texture.name.empty())
                        {
                            if (!Require(texture.resource.IsValid() && texture.resource.GetTexture().IsValid(),
                                "cold reimport lost a material map")) return false;
                            ++maps;
                        }
                if (!Require(maps == record.at("maps").get<size_t>(), "cold reimport map count changed")) return false;
                std::cout << "COLD REOPEN / RETAINED REIMPORT PASS: " << record.at("asset") << '\n';
            }
            // Ordinary standalone-player scene containing the exact committed assets.
            wi::scene::Scene runtimeScene;
            size_t placed = 0;
            for (const auto& record : records)
            {
                auto prepared = CreatorAssetWorkflowService().PrepareModelPlacement(
                    project.generic_u8string(), projectId, record.at("id").get<std::string>());
                if (!Require(prepared.IsReady(), "runtime-scene asset preparation failed")) return false;
                PlaceReusableModelCommand place(runtimeScene, prepared.ReleaseScene(),
                    record.at("id").get<std::string>(), XMFLOAT3(float(placed % 4) * 3, 1, float(placed / 4) * 3),
                    1, record.at("asset").get<std::string>());
                if (!Require(place.Execute(), "runtime-scene placement failed")) return false;
                ++placed;
            }
            TransformState start;
            start.translation = XMFLOAT3(4, 2, -8);
            CreatePlayerStartCommand player(runtimeScene, start);
            if (!Require(player.Execute(), "runtime-scene player start failed")) return false;
            auto floor = runtimeScene.Entity_CreateCube("Importer proof floor");
            auto* floorTransform = runtimeScene.transforms.GetComponent(floor);
            floorTransform->Translate(XMFLOAT3(4, -0.5f, 3));
            floorTransform->Scale(XMFLOAT3(20, 0.5f, 20));
            floorTransform->UpdateTransform();
            auto& body = runtimeScene.rigidbodies.Create(floor);
            body.mass = 0;
            body.shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape::BOX;
            const auto light = runtimeScene.Entity_CreateLight("Importer proof sun");
            runtimeScene.lights.GetComponent(light)->SetType(wi::scene::LightComponent::DIRECTIONAL);
            runtimeScene.lights.GetComponent(light)->intensity = 3;
            runtimeScene.transforms.GetComponent(light)->RotateRollPitchYaw(XMFLOAT3(-0.7f, 0.6f, 0));
            auto& weather = runtimeScene.weathers.Create(wi::ecs::CreateEntity());
            weather.ambient = XMFLOAT3(0.3f, 0.3f, 0.3f);
            fs::create_directories(project / "Content/Scenes");
            const auto scenePath = project / "Content/Scenes/Main.wiscene";
            {
                wi::Archive archive(scenePath.generic_u8string(), false, false);
                runtimeScene.Serialize(archive);
                if (!Require(archive.SaveFile(scenePath.generic_u8string()), "runtime scene save failed")) return false;
            }
            write(project / "Importer Audit.renegade",
                "format = renegade-project\nversion = 1\n\n[project]\nproject_id = " + projectId +
                "\nname = Importer Audit\nstartup_scene = Content/Scenes/Main.wiscene\n");
            SceneService snapshotScene;
            snapshotScene.GetScene().Merge(runtimeScene);
            CommandService commands;
            TestLevelSnapshot snapshot;
            std::string snapshotError;
            const auto previousMode = wi::resourcemanager::GetMode();
            if (!Require(TestLevelSnapshotService(snapshotScene, commands).Create(
                project.generic_u8string(), snapshot, snapshotError), snapshotError)) return false;
            if (!Require(wi::resourcemanager::GetMode() == previousMode,
                "snapshot changed the caller's resource embedding preference")) return false;
            write(root / "runtime-snapshot-path.txt", snapshot.scenePath);
            std::cout << "STANDALONE RUNTIME SCENE READY: " << placed << " imported assets\n";
            return true;
        }

        fs::create_directories(project / "Content/Models");
        fs::create_directories(sources);
        // Simple authored cube with no dependency on licensed owner models.
        const std::string cube =
            "o Cube\nv -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\n"
            "v -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\n"
            "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
            "f 1/1 4/4 3/3 2/2\nf 5/1 6/2 7/3 8/4\nf 1/1 2/2 6/3 5/4\n"
            "f 2/1 3/2 7/3 6/4\nf 3/1 4/2 8/3 7/4\nf 4/1 1/2 5/3 8/4\n";
        write(sources / "plain.obj", cube);
        write(sources / "missing.obj", "mtllib absent.mtl\n" + cube);
        write(sources / "partial.obj",
            "o Partial\nv -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\nvn 0 0 1\n"
            "f 1//1 2//1 3//1\nf 1 3 4\n");
        write(sources / "bad.obj", "v 0 0 0\nf 1 999 9999\n");
        write(sources / "badquad.obj", "v 0 0 0\nf 1 999 9999 777\n");
        write(sources / "bad.fbx", "This is not an FBX file.");
        write(sources / "bad.png", "This is not an image.");
        const auto map = sources / "maps/checker.png";
        fs::create_directories(map.parent_path());
        fs::copy_file(fixtures / "textures/checker.png", map, fs::copy_options::overwrite_existing);
        write(sources / "materials/cube.mtl", "newmtl First\nKd 1 1 1\nmap_Kd ../maps/checker.png\n");
        write(sources / "textured.obj", "mtllib materials/cube.mtl\nusemtl First\n" + cube);
        write(sources / "outside/relative.mtl", "newmtl First\nKd 1 1 1\nmap_Kd ../maps/checker.png\n");
        write(sources / "outside/relative.obj", "mtllib relative.mtl\nusemtl First\n" + cube);
        auto multi = cube;
        multi.replace(multi.find("f 1/1"), 0, "usemtl First\n");
        multi.replace(multi.find("f 5/1"), 0, "usemtl Second\n");
        write(sources / "multi.obj", "mtllib multi.mtl\n" + multi);
        write(sources / "multi.mtl", "newmtl First\nKd 1 1 1\nmap_Kd maps/checker.png\nnewmtl Second\nKd 1 1 1\n");
        write(sources / "corrupt.mtl", "newmtl First\nKd 1 1 1\nmap_Kd bad.png\n");
        write(sources / "corrupt.obj", "mtllib corrupt.mtl\nusemtl First\n" + cube);
        fs::create_directories(sources / "missing-fbx");
        fs::copy_file(fixtures / "static_textured_cube.fbx", sources / "missing-fbx/static.fbx",
            fs::copy_options::overwrite_existing);
        fs::copy_file(fixtures / "static_triangle.glb", sources / "triangle.glb",
            fs::copy_options::overwrite_existing);
        for (const auto& filename : {"bad.obj", "badquad.obj", "bad.fbx"})
        {
            auto invalid = ModelImportCandidateService().PrepareModel((sources / filename).generic_u8string());
            if (!Require(!invalid.IsReady() && !invalid.Error().empty(), "malformed geometry was accepted")) return false;
        }

        nlohmann::json records = nlohmann::json::array();
        auto exercise = [&](const fs::path& source, const std::string& name, bool warnings, bool assign) {
            ModelImportCandidateService service;
            auto candidate = service.PrepareModel(source.generic_u8string());
            if (!Require(candidate.IsReady(), name + ": " + candidate.Error()) ||
                !Require(!warnings || !candidate.Warnings().empty(), name + ": expected map warning absent")) return false;
            const auto evidence = candidate.Evidence();
            if (assign)
            {
                std::string error;
                if (!Require(service.AssignTexture(candidate, {0, 0, map.generic_u8string()}, error), error))
                    return false;
                const auto previous = candidate.PeekScene()->materials[0].textures[0].name;
                const auto count = candidate.Dependencies().size();
                if (!Require(!service.AssignTexture(candidate, {0, 0, (sources / "bad.png").generic_u8string()}, error) &&
                    candidate.IsReady() && candidate.PeekScene()->materials[0].textures[0].name == previous &&
                    candidate.Dependencies().size() == count, "bad replacement changed the candidate")) return false;
                if (!Require(!service.AssignTexture(candidate, {99999, 0, map.generic_u8string()}, error) &&
                    !service.AssignTexture(candidate, {0, 99999, map.generic_u8string()}, error),
                    "out-of-range material or map accepted")) return false;
                if (!Require(service.AssignTexture(candidate, {0, 0, map.generic_u8string()}, error) &&
                    candidate.Dependencies().size() == count && candidate.Evidence() == evidence,
                    "replacement duplicated dependencies or changed geometry")) return false;
            }
            if (candidate.PeekScene()->materials.GetCount() > 1)
            {
                std::string error;
                const auto first = candidate.PeekScene()->materials[0].textures[0].name;
                if (!Require(service.AssignTexture(candidate, {1, 0, map.generic_u8string()}, error) &&
                    candidate.PeekScene()->materials[0].textures[0].name == first,
                    "assignment changed a different material")) return false;
            }
            size_t maps = 0;
            for (size_t m = 0; m < candidate.PeekScene()->materials.GetCount(); ++m)
                for (const auto& texture : candidate.PeekScene()->materials[m].textures)
                    if (!texture.name.empty())
                    {
                        if (!Require(texture.resource.IsValid() && texture.resource.GetTexture().IsValid(),
                            "candidate contains unresolved named map")) return false;
                        ++maps;
                    }
            renegade::studio::ModelImportPreview preview;
            std::string error;
            if (!Require(preview.Prepare(*candidate.PeekMutableScene(), error), error) || !RenderPreview(preview))
                return false;
            preview.RefreshMaterials(*candidate.PeekScene());
            if (!Require(!preview.IsReady(), "material update retained stale preview readiness") ||
                !RenderPreview(preview)) return false;
            ModelImportCommitRequest request;
            request.projectRoot = project.generic_u8string();
            request.projectId = projectId;
            request.assetName = name;
            if (!Require(preview.CapturePng(request.thumbnailPng, error), error) ||
                !VerifyThumbnailPixels(request.thumbnailPng)) return false;
            std::ofstream thumbnail(root / (name + ".png"), std::ios::binary);
            thumbnail.write(reinterpret_cast<const char*>(request.thumbnailPng.data()), request.thumbnailPng.size());
            const auto result = ModelImportCommitService().CommitModel(request, candidate);
            if (!Require(result.succeeded && result.committed, result.error) ||
                !VerifyPlacement(project, projectId, result.assetId)) return false;
            records.push_back({{"id", result.assetId}, {"asset", result.assetProjectRelativePath},
                {"source", result.sourceProjectRelativePath}, {"maps", maps}});
            std::cout << "IMPORT / PREVIEW / COMMIT / PLACEMENT / SAVE REOPEN PASS: " << name <<
                " maps=" << maps << " warnings=" << candidate.Warnings().size() << '\n';
            return true;
        };
        if (!exercise(sources / "plain.obj", "OBJ Plain", false, false) ||
            !exercise(sources / "missing.obj", "OBJ Missing MTL", true, false) ||
            !exercise(sources / "missing.obj", "OBJ Manual Map", true, true) ||
            !exercise(sources / "partial.obj", "OBJ Partial Normals", false, false) ||
            !exercise(sources / "textured.obj", "OBJ Textured", false, false) ||
            !exercise(sources / "outside/relative.obj", "OBJ Outside Texture", false, false) ||
            !exercise(sources / "multi.obj", "OBJ Multiple Materials", false, false) ||
            !exercise(sources / "corrupt.obj", "OBJ Corrupt Map", true, false) ||
            !exercise(sources / "missing-fbx/static.fbx", "FBX Missing Map", true, false) ||
            !exercise(sources / "missing-fbx/static.fbx", "FBX Manual Map", true, true) ||
            !exercise(sources / "triangle.glb", "GLB Manual Map", false, true)) return false;
        write(root / "audit-records.json", records.dump(2));
        fs::remove_all(sources);
        std::cout << "ORIGINAL DISPOSABLE SOURCES REMOVED; COLD PROCESS REQUIRED\n";
        return true;
    }
}
