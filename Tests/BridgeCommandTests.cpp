#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/LightService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SelectionService.h"
#include "renegade/bridge/StudioSession.h"

namespace
{
    bool NearlyEqual(const float left, const float right)
    {
        return std::abs(left - right) < 0.0001f;
    }

    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    bool ContainsEntity(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
    {
        wi::unordered_set<wi::ecs::Entity> entities;
        scene.FindAllEntities(entities);
        return entities.count(entity) != 0;
    }

    struct TemporaryDirectory
    {
        std::filesystem::path path;

        ~TemporaryDirectory()
        {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    };
}

int main()
{
    renegade::bridge::SceneService scenes;
    renegade::bridge::SelectionService selection;
    renegade::bridge::CommandService commands;

    const auto parent = wi::ecs::CreateEntity();
    scenes.GetScene().names.Create(parent) = "Parent";
    scenes.GetScene().transforms.Create(parent);

    const auto child = wi::ecs::CreateEntity();
    scenes.GetScene().names.Create(child) = "Child";
    auto& childTransform = scenes.GetScene().transforms.Create(child);
    scenes.GetScene().Component_Attach(child, parent);

    const auto hierarchy = scenes.ListEntities();
    if (hierarchy.size() != 2 || hierarchy[0].entity != parent ||
        hierarchy[1].entity != child || hierarchy[1].depth != 1)
    {
        return Fail("hierarchy listing did not preserve parent/child order");
    }

    selection.Select(child);
    if (!selection.HasSelection() || selection.SelectedEntity() != child)
    {
        return Fail("selection service did not retain the selected entity");
    }
    selection.Clear();
    if (selection.HasSelection() ||
        selection.SelectedEntity() != wi::ecs::INVALID_ENTITY)
    {
        return Fail("selection service did not clear the selected entity");
    }
    selection.Select(child);

    if (!commands.Execute(std::make_unique<renegade::bridge::SetTranslationCommand>(
            scenes.GetScene(),
            child,
            XMFLOAT3(4.0f, 5.0f, 6.0f))))
    {
        return Fail("transform command did not execute");
    }

    if (!NearlyEqual(childTransform.translation_local.x, 4.0f) ||
        !commands.CanUndo() || commands.CanRedo() || !commands.IsDirty())
    {
        return Fail("execute state is incorrect");
    }

    commands.MarkSaved();
    if (commands.IsDirty())
    {
        return Fail("mark saved did not establish a clean scene state");
    }

    if (!commands.Undo() ||
        !NearlyEqual(childTransform.translation_local.x, 0.0f) ||
        !commands.CanRedo() || !commands.IsDirty())
    {
        return Fail("undo did not restore the original transform");
    }

    if (!commands.Redo() ||
        !NearlyEqual(childTransform.translation_local.x, 4.0f) ||
        commands.CanRedo() || commands.IsDirty())
    {
        return Fail("redo did not restore the edited transform");
    }

    if (!commands.Undo() ||
        !commands.Execute(
            std::make_unique<renegade::bridge::SetTranslationCommand>(
                scenes.GetScene(),
                child,
                XMFLOAT3(2.0f, 0.0f, 0.0f))) ||
        !commands.IsDirty() || commands.CanRedo())
    {
        return Fail("a new command branch did not invalidate the saved state");
    }

    commands.Clear();
    if (commands.IsDirty())
    {
        return Fail("clearing command history did not reset dirty state");
    }
    childTransform.translation_local = XMFLOAT3{};
    childTransform.SetDirty();
    childTransform.UpdateTransform();

    for (int step = 1; step <= 10; ++step)
    {
        if (!commands.Execute(
                std::make_unique<renegade::bridge::SetTranslationCommand>(
                    scenes.GetScene(),
                    child,
                    XMFLOAT3(static_cast<float>(step), 0.0f, 0.0f))))
        {
            return Fail("repeated transform command did not execute");
        }
    }

    if (commands.UndoCount() != 10 || commands.RedoCount() != 0)
    {
        return Fail("repeated transform history counts are incorrect");
    }

    for (int expected = 9; expected >= 0; --expected)
    {
        if (!commands.Undo() ||
            !NearlyEqual(
                childTransform.translation_local.x,
                static_cast<float>(expected)))
        {
            return Fail("repeated undo did not restore the expected transform");
        }
    }

    if (commands.CanUndo() || commands.UndoCount() != 0 ||
        commands.RedoCount() != 10)
    {
        return Fail("repeated undo history counts are incorrect");
    }

    for (int expected = 1; expected <= 10; ++expected)
    {
        if (!commands.Redo() ||
            !NearlyEqual(
                childTransform.translation_local.x,
                static_cast<float>(expected)))
        {
            return Fail("repeated redo did not restore the expected transform");
        }
    }

    if (commands.UndoCount() != 10 || commands.CanRedo())
    {
        return Fail("repeated redo history counts are incorrect");
    }

    const auto undoCountBeforeNoOp = commands.UndoCount();
    if (commands.Execute(
            std::make_unique<renegade::bridge::SetTranslationCommand>(
                scenes.GetScene(),
                child,
                XMFLOAT3(10.0f, 0.0f, 0.000001f))) ||
        commands.UndoCount() != undoCountBeforeNoOp)
    {
        return Fail("microscopic transform polluted the undo history");
    }

    commands.Clear();
    const auto transformBefore =
        renegade::bridge::CaptureTransform(childTransform);
    auto transformAfter = transformBefore;
    transformAfter.translation = XMFLOAT3(3.0f, 2.0f, 1.0f);
    transformAfter.rotation =
        XMFLOAT4(0.0f, 0.7071067f, 0.0f, 0.7071067f);
    transformAfter.scale = XMFLOAT3(2.0f, 3.0f, 4.0f);
    if (!commands.Execute(
            std::make_unique<renegade::bridge::SetTransformCommand>(
                scenes.GetScene(),
                child,
                transformAfter)) ||
        !NearlyEqual(childTransform.scale_local.y, 3.0f) ||
        !NearlyEqual(childTransform.rotation_local.y, 0.7071067f))
    {
        return Fail("full transform command did not execute");
    }
    if (!commands.Undo() ||
        !NearlyEqual(childTransform.scale_local.y, transformBefore.scale.y) ||
        !NearlyEqual(
            childTransform.rotation_local.w,
            transformBefore.rotation.w))
    {
        return Fail("full transform undo did not restore rotation and scale");
    }
    if (!commands.Redo() ||
        !NearlyEqual(childTransform.translation_local.x, 3.0f) ||
        !NearlyEqual(childTransform.scale_local.z, 4.0f))
    {
        return Fail("full transform redo did not restore the edited state");
    }

    // Curated environment edits must be renderer-independent, preserve
    // unsurfaced Wicked values, and participate in the same Undo/Redo history
    // as transforms.
    {
        renegade::bridge::SceneService weatherScenes;
        if (weatherScenes.WeatherEntity() != wi::ecs::INVALID_ENTITY)
        {
            return Fail("an empty scene reported a weather entity");
        }

        const auto environment = wi::ecs::CreateEntity();
        weatherScenes.GetScene().names.Create(environment) = "Environment";
        auto& weather =
            weatherScenes.GetScene().weathers.Create(environment);
        weather.SetRealisticSky(true);
        weather.SetRealisticSkyAerialPerspective(true);
        weather.SetVolumetricCloudsReceiveShadow(true);
        weather.skyExposure = 0.8f;
        weather.ambient = XMFLOAT3(0.02f, 0.04f, 0.08f);
        weather.horizon = XMFLOAT3(0.11f, 0.12f, 0.13f);
        weather.windSpeed = 3.5f;
        weather.volumetricCloudParameters.layerFirst.coverageAmount = 0.2f;
        weather.volumetricCloudParameters.layerSecond.coverageAmount = 0.73f;
        weatherScenes.GetScene().weather = weather;

        if (weatherScenes.WeatherEntity() != environment)
        {
            return Fail("scene service did not resolve the primary weather");
        }

        const auto before = renegade::bridge::CaptureWeather(weather);
        auto after = before;
        after.skyMode =
            renegade::bridge::WeatherState::SkyMode::RealisticWithClouds;
        after.skyExposure = 1.25f;
        after.ambientIntensity = 0.16f;
        after.fogStart = 24.0f;
        after.fogDensity = 0.031f;
        after.heightFog = true;
        after.fogHeightStart = -1.0f;
        after.fogHeightEnd = 5.0f;
        after.cloudCoverage = 0.62f;
        after.cloudStartHeight = 1800.0f;
        after.cloudThickness = 4200.0f;
        after.cloudsCastShadow = true;

        renegade::bridge::CommandService weatherCommands;
        if (!weatherCommands.Execute(
                std::make_unique<renegade::bridge::SetWeatherCommand>(
                    weatherScenes.GetScene(),
                    environment,
                    after)))
        {
            return Fail("weather command did not execute");
        }

        if (!weather.IsVolumetricClouds() ||
            !weather.IsVolumetricCloudsCastShadow() ||
            !NearlyEqual(weather.skyExposure, 1.25f) ||
            !NearlyEqual(weather.ambient.z, 0.16f) ||
            !NearlyEqual(weather.ambient.x, 0.04f) ||
            !NearlyEqual(weather.fogDensity, 0.031f) ||
            !NearlyEqual(
                weather.volumetricCloudParameters.layerFirst.coverageAmount,
                0.62f))
        {
            return Fail("weather command did not apply the curated state");
        }

        if (!NearlyEqual(weather.horizon.x, 0.11f) ||
            !NearlyEqual(weather.windSpeed, 3.5f) ||
            !NearlyEqual(
                weather.volumetricCloudParameters.layerSecond.coverageAmount,
                0.73f) ||
            !weather.IsVolumetricCloudsReceiveShadow())
        {
            return Fail("weather command overwrote an unsurfaced value");
        }

        if (!NearlyEqual(
                weatherScenes.GetScene().weather.skyExposure,
                weather.skyExposure))
        {
            return Fail("weather command did not refresh the runtime weather");
        }

        if (!weatherCommands.Undo() ||
            weather.IsVolumetricClouds() ||
            !NearlyEqual(weather.skyExposure, 0.8f) ||
            !NearlyEqual(weather.ambient.z, 0.08f) ||
            !NearlyEqual(
                weather.volumetricCloudParameters.layerFirst.coverageAmount,
                0.2f))
        {
            return Fail("weather undo did not restore the authored state");
        }

        if (!weatherCommands.Redo() ||
            !weather.IsVolumetricClouds() ||
            !NearlyEqual(weather.skyExposure, 1.25f))
        {
            return Fail("weather redo did not restore the edited state");
        }

        const auto undoCountBeforeWeatherNoOp =
            weatherCommands.UndoCount();
        if (weatherCommands.Execute(
                std::make_unique<renegade::bridge::SetWeatherCommand>(
                    weatherScenes.GetScene(),
                    environment,
                    renegade::bridge::CaptureWeather(weather))) ||
            weatherCommands.UndoCount() != undoCountBeforeWeatherNoOp)
        {
            return Fail("unchanged weather polluted the undo history");
        }

        if (weatherCommands.Execute(
                std::make_unique<renegade::bridge::SetWeatherCommand>(
                    weatherScenes.GetScene(),
                    wi::ecs::INVALID_ENTITY,
                    before,
                    after)))
        {
            return Fail("weather command accepted a missing component");
        }

        const auto storm = renegade::bridge::MakeWeatherPreset(
            before,
            renegade::bridge::WeatherPreset::Storm);
        if (storm.skyMode !=
                renegade::bridge::WeatherState::SkyMode::
                    RealisticWithClouds ||
            !storm.cloudsCastShadow ||
            !storm.heightFog ||
            !NearlyEqual(storm.cloudCoverage, 0.95f) ||
            !NearlyEqual(storm.cloudStartHeight, 750.0f))
        {
            return Fail("the Storm weather preset is incomplete");
        }

        const auto clear = renegade::bridge::MakeWeatherPreset(
            storm,
            renegade::bridge::WeatherPreset::Clear);
        if (clear.skyMode !=
                renegade::bridge::WeatherState::SkyMode::Realistic ||
            clear.cloudsCastShadow ||
            clear.heightFog ||
            !NearlyEqual(clear.cloudCoverage, 0.05f))
        {
            return Fail("the Clear weather preset is incomplete");
        }
    }

    commands.Clear();
    auto duplicateCommand =
        std::make_unique<renegade::bridge::DuplicateEntityCommand>(
            scenes.GetScene(),
            child);
    auto* duplicateCommandView = duplicateCommand.get();
    if (!commands.Execute(std::move(duplicateCommand)))
    {
        return Fail("duplicate entity command did not execute");
    }
    const auto duplicate = duplicateCommandView->DuplicatedEntity();
    const auto* duplicateName =
        scenes.GetScene().names.GetComponent(duplicate);
    if (!ContainsEntity(scenes.GetScene(), duplicate) ||
        duplicateName == nullptr ||
        duplicateName->name != "Child Copy")
    {
        return Fail("duplicate entity was not created and named");
    }
    if (!commands.Undo() || ContainsEntity(scenes.GetScene(), duplicate))
    {
        return Fail("duplicate undo did not remove the duplicate");
    }
    if (!commands.Redo() || !ContainsEntity(scenes.GetScene(), duplicate))
    {
        return Fail("duplicate redo did not restore the same entity");
    }

    commands.Clear();
    if (!commands.Execute(
            std::make_unique<renegade::bridge::DeleteEntityCommand>(
                scenes.GetScene(),
                duplicate)) ||
        ContainsEntity(scenes.GetScene(), duplicate))
    {
        return Fail("delete entity command did not remove the entity");
    }
    if (!commands.Undo() || !ContainsEntity(scenes.GetScene(), duplicate))
    {
        return Fail("delete undo did not restore the entity");
    }
    if (!commands.Redo() || ContainsEntity(scenes.GetScene(), duplicate))
    {
        return Fail("delete redo did not remove the entity again");
    }

    const auto internalGridEntity = wi::ecs::CreateEntity();
    scenes.GetScene().names.Create(internalGridEntity) =
        "__renegade_internal_grid_test";
    scenes.GetScene().transforms.Create(internalGridEntity);

    const auto creatorEntities = scenes.ListEntities();
    if (creatorEntities.size() >= scenes.EntityCount())
    {
        return Fail("generated grid internals leaked into the hierarchy");
    }
    for (const auto& entity : creatorEntities)
    {
        if (entity.name.rfind("__renegade_internal_", 0) == 0)
        {
            return Fail("internal entity name leaked into the hierarchy");
        }
    }

    // Generated Proving Ground structure.
    //
    // SceneService::CreateProvingGround() cannot run here: Wicked's primitive
    // factories call MeshComponent::CreateRenderData(), which dereferences the
    // global graphics device, and this test process has none. The composition
    // is therefore asserted through its renderer-independent blueprint, which
    // CreateProvingGround() instantiates verbatim.
    const auto blueprint = renegade::bridge::ProvingGroundBlueprint();
    if (blueprint.empty())
    {
        return Fail("the Proving Ground blueprint is empty");
    }

    bool hasTerrain = false;
    bool hasEnvironment = false;
    bool hasShadowCastingSun = false;
    bool hasVolumetricLight = false;
    int deckEdgeCount = 0;
    std::vector<std::string> blueprintNames;
    for (const auto& prop : blueprint)
    {
        blueprintNames.push_back(prop.name);

        if (prop.name.rfind("__renegade_internal_", 0) == 0)
        {
            return Fail(
                "the Proving Ground blueprint still serializes internal "
                "helper entities");
        }
        if (prop.name.find("grid") != std::string::npos ||
            prop.name.find("Grid") != std::string::npos)
        {
            return Fail(
                "the Proving Ground blueprint still builds grid geometry; "
                "the editor grid must come from the renderer-owned helper");
        }

        switch (prop.kind)
        {
        case renegade::bridge::ProvingGroundProp::Kind::Terrain:
            hasTerrain = true;
            if (prop.terrainResolution < 2 ||
                prop.halfExtents.x <= renegade::bridge::GridHelperHalfExtent)
            {
                return Fail(
                    "generated ground relief is smaller than the editor grid");
            }
            break;
        case renegade::bridge::ProvingGroundProp::Kind::Environment:
            hasEnvironment = true;
            break;
        case renegade::bridge::ProvingGroundProp::Kind::Light:
            if (prop.lightType == wi::scene::LightComponent::DIRECTIONAL &&
                prop.castShadow)
            {
                hasShadowCastingSun = true;
            }
            if (prop.volumetrics && prop.volumetricBoost > 0.0f)
            {
                hasVolumetricLight = true;
            }
            break;
        default:
            break;
        }

        if (prop.name.rfind("Deck Edge", 0) == 0)
        {
            ++deckEdgeCount;
        }
        if (prop.name == "Proving Ground Deck" &&
            prop.halfExtents.x < renegade::bridge::GridHelperHalfExtent)
        {
            return Fail(
                "the generated deck is narrower than the editor grid helper");
        }
    }

    if (!hasTerrain)
    {
        return Fail("the Proving Ground blueprint has no ground relief");
    }
    if (!hasEnvironment)
    {
        return Fail(
            "the Proving Ground blueprint has no weather entity, so sky and "
            "fog would not survive save and reopen");
    }
    if (!hasShadowCastingSun)
    {
        return Fail(
            "the Proving Ground blueprint has no shadow-casting sun");
    }
    if (!hasVolumetricLight)
    {
        return Fail(
            "the Proving Ground blueprint has no volumetric light, so the "
            "atmosphere would not react to lighting");
    }
    if (deckEdgeCount != 4)
    {
        return Fail("the generated deck is not fully edged");
    }

    // Every generated entity must be distinguishable in the hierarchy. Eight
    // rows all reading "Perimeter Pylon" are not selectable by name and give
    // the creator no way to tell which is which.
    std::sort(blueprintNames.begin(), blueprintNames.end());
    if (std::adjacent_find(blueprintNames.begin(), blueprintNames.end()) !=
        blueprintNames.end())
    {
        return Fail(
            "the Proving Ground blueprint generates duplicate entity names");
    }

    // Headless save and reload of a mesh-free scene. This covers the
    // SceneService archive round trip - which no longer routes through the
    // renderer-dependent wi::scene::LoadModel - including the serialized
    // weather that carries the Proving Ground's sky and fog.
    {
        TemporaryDirectory sceneFixture;
        sceneFixture.path =
            std::filesystem::temp_directory_path() /
            (
                "renegade-scene-roundtrip-" +
                std::to_string(
                    std::chrono::steady_clock::now()
                        .time_since_epoch()
                        .count()));
        std::filesystem::create_directories(sceneFixture.path);
        const auto scenePath = sceneFixture.path / "RoundTrip.wiscene";

        renegade::bridge::StudioSession authored;
        const auto landmark = wi::ecs::CreateEntity();
        authored.Scenes().GetScene().names.Create(landmark) = "Landmark";
        authored.Scenes().GetScene().transforms.Create(landmark);
        {
            auto* landmarkTransform = authored.Scenes()
                .GetScene()
                .transforms
                .GetComponent(landmark);
            if (landmarkTransform == nullptr)
            {
                return Fail("landmark transform fixture was not created");
            }
            landmarkTransform->translation_local = XMFLOAT3(2.0f, 3.0f, 4.0f);
            landmarkTransform->SetDirty();
            landmarkTransform->UpdateTransform();
        }

        const auto environment = wi::ecs::CreateEntity();
        authored.Scenes().GetScene().names.Create(environment) = "Environment";
        auto& authoredWeather =
            authored.Scenes().GetScene().weathers.Create(environment);
        authoredWeather.SetRealisticSky(true);
        authoredWeather.SetRealisticSkyAerialPerspective(true);
        authoredWeather.SetVolumetricClouds(true);
        authoredWeather.SetVolumetricCloudsCastShadow(true);
        authoredWeather.SetHeightFog(true);
        authoredWeather.skyExposure = 1.15f;
        authoredWeather.fogDensity = 0.018f;
        authoredWeather.volumetricCloudParameters.layerFirst.coverageAmount =
            0.57f;
        authoredWeather.volumetricCloudParameters.cloudStartHeight = 1650.0f;
        authoredWeather.volumetricCloudParameters.cloudThickness = 3600.0f;

        // Use a small native terrain fixture, including authored height and
        // blend pixels, so the document round trip covers terrain state rather
        // than only ordinary entity components.
        const auto terrainEntity = wi::ecs::CreateEntity();
        authored.Scenes().GetScene().names.Create(terrainEntity) = "Terrain";
        authored.Scenes().GetScene().transforms.Create(terrainEntity);
        auto& authoredTerrain =
            authored.Scenes().GetScene().terrains.Create(terrainEntity);
        authoredTerrain.terrainEntity = terrainEntity;
        authoredTerrain.scene = &authored.Scenes().GetScene();
        authoredTerrain.seed = 77123;
        authoredTerrain.chunk_scale = 3.25f;
        authoredTerrain.bottomLevel = -44.0f;
        authoredTerrain.topLevel = 188.0f;
        wi::terrain::Chunk authoredChunk;
        authoredChunk.x = 4;
        authoredChunk.z = -2;
        auto& authoredChunkData = authoredTerrain.chunks[authoredChunk];
        authoredChunkData.heightmap_data = {12, 1400, 32000, 65535};
        authoredChunkData.blendmap_layers.resize(1);
        authoredChunkData.blendmap_layers[0].pixels = {3, 17, 128, 244};

        const XMFLOAT4 placedLightRotation = XMFLOAT4(
            std::sin(XM_PIDIV4 * 0.5f),
            0.0f,
            0.0f,
            std::cos(XM_PIDIV4 * 0.5f));
        auto createLight =
            std::make_unique<renegade::bridge::CreateLightCommand>(
                authored.Scenes().GetScene(),
                wi::scene::LightComponent::SPOT,
                XMFLOAT3(7.0f, 8.0f, 9.0f),
                placedLightRotation);
        auto* createLightCommand = createLight.get();
        if (!authored.Commands().Execute(std::move(createLight)))
        {
            return Fail("round-trip Add Light command did not execute");
        }
        const auto authoredLightEntity = createLightCommand->CreatedEntity();
        auto* authoredLight = authored.Scenes().GetScene().lights.GetComponent(
            authoredLightEntity);
        if (authoredLight == nullptr)
        {
            return Fail("round-trip Add Light component was not created");
        }
        auto authoredLightState = renegade::bridge::CaptureLight(*authoredLight);
        authoredLightState.color = XMFLOAT3(0.2f, 0.4f, 0.8f);
        authoredLightState.intensity = 321.0f;
        authoredLightState.range = 47.0f;
        authoredLightState.castShadow = true;
        authoredLightState.volumetrics = true;
        authoredLightState.volumetricBoost = 1.75f;
        if (!authored.Commands().Execute(
                std::make_unique<renegade::bridge::SetLightCommand>(
                    authored.Scenes().GetScene(),
                    authoredLightEntity,
                    authoredLightState)))
        {
            return Fail("round-trip authored light state did not apply");
        }

        if (!authored.SaveScene(scenePath.generic_string()) ||
            !std::filesystem::is_regular_file(scenePath) ||
            authored.Scenes().CurrentPath() != scenePath.generic_string())
        {
            return Fail("scene service did not write the scene archive");
        }

        renegade::bridge::StudioSession reopened;
        if (!reopened.LoadScene(scenePath.generic_string()))
        {
            return Fail("scene service did not reload the scene archive");
        }
        if (reopened.Scenes().EntityCount() != authored.Scenes().EntityCount())
        {
            return Fail("the reloaded scene lost or gained entities");
        }
        const auto reopenedWeatherEntity = reopened.Scenes().WeatherEntity();
        const auto* reopenedWeather =
            reopened.Scenes().GetScene().weathers.GetComponent(
                reopenedWeatherEntity);
        if (reopened.Scenes().GetScene().weathers.GetCount() != 1 ||
            reopenedWeatherEntity == wi::ecs::INVALID_ENTITY ||
            reopenedWeather == nullptr ||
            !reopenedWeather->IsHeightFog() ||
            !reopenedWeather->IsVolumetricClouds() ||
            !reopenedWeather->IsVolumetricCloudsCastShadow() ||
            !NearlyEqual(reopenedWeather->skyExposure, 1.15f) ||
            !NearlyEqual(
                reopenedWeather->volumetricCloudParameters
                    .layerFirst.coverageAmount,
                0.57f) ||
            !NearlyEqual(
                reopenedWeather->volumetricCloudParameters.cloudStartHeight,
                1650.0f) ||
            !NearlyEqual(
                reopenedWeather->volumetricCloudParameters.cloudThickness,
                3600.0f))
        {
            return Fail("the reloaded scene lost its serialized weather");
        }

        bool reloadedLandmark = false;
        for (const auto& entity : reopened.Scenes().ListEntities())
        {
            if (entity.name.rfind("__renegade_internal_", 0) == 0)
            {
                return Fail("the reloaded scene contains internal helpers");
            }
            if (entity.name != "Landmark")
            {
                continue;
            }
            const auto* transform =
                reopened.Scenes().GetScene().transforms.GetComponent(
                    entity.entity);
            if (transform == nullptr ||
                !NearlyEqual(transform->translation_local.x, 2.0f) ||
                !NearlyEqual(transform->translation_local.z, 4.0f))
            {
                return Fail("the reloaded scene lost a saved transform");
            }
            reloadedLandmark = true;
        }
        if (!reloadedLandmark)
        {
            return Fail("the reloaded scene lost a named entity");
        }
        bool reloadedAuthoredLight = false;
        for (const auto& entity : reopened.Scenes().ListEntities())
        {
            if (entity.name != "Spot Light")
            {
                continue;
            }
            const auto* light =
                reopened.Scenes().GetScene().lights.GetComponent(entity.entity);
            const auto* transform = reopened.Scenes()
                .GetScene()
                .transforms.GetComponent(entity.entity);
            if (light == nullptr || transform == nullptr ||
                light->GetType() != wi::scene::LightComponent::SPOT ||
                !NearlyEqual(light->color.x, 0.2f) ||
                !NearlyEqual(light->color.z, 0.8f) ||
                !NearlyEqual(light->intensity, 321.0f) ||
                !NearlyEqual(light->range, 47.0f) ||
                !light->IsCastingShadow() ||
                !light->IsVolumetricsEnabled() ||
                !NearlyEqual(light->volumetric_boost, 1.75f) ||
                !NearlyEqual(transform->translation_local.x, 7.0f) ||
                !NearlyEqual(transform->translation_local.z, 9.0f) ||
                !NearlyEqual(
                    transform->rotation_local.x,
                    placedLightRotation.x) ||
                !NearlyEqual(
                    transform->rotation_local.w,
                    placedLightRotation.w))
            {
                return Fail("the reloaded scene changed the authored light");
            }
            reloadedAuthoredLight = true;
        }
        if (!reloadedAuthoredLight)
        {
            return Fail("the reloaded scene lost the added light");
        }
        if (reopened.Scenes().GetScene().terrains.GetCount() != 1)
        {
            return Fail("the reloaded scene lost its terrain component");
        }
        const auto& reopenedTerrain =
            reopened.Scenes().GetScene().terrains[0];
        const auto reopenedChunk = reopenedTerrain.chunks.find(authoredChunk);
        if (reopenedTerrain.scene != &reopened.Scenes().GetScene() ||
            reopenedTerrain.terrainEntity !=
                reopened.Scenes().GetScene().terrains.GetEntity(0) ||
            reopenedTerrain.seed != authoredTerrain.seed ||
            !NearlyEqual(
                reopenedTerrain.chunk_scale,
                authoredTerrain.chunk_scale) ||
            !NearlyEqual(
                reopenedTerrain.bottomLevel,
                authoredTerrain.bottomLevel) ||
            !NearlyEqual(reopenedTerrain.topLevel, authoredTerrain.topLevel) ||
            reopenedChunk == reopenedTerrain.chunks.end() ||
            reopenedChunk->second.heightmap_data.size() != 4 ||
            reopenedChunk->second.heightmap_data[2] != 32000 ||
            reopenedChunk->second.blendmap_layers.size() != 1 ||
            reopenedChunk->second.blendmap_layers[0].pixels.size() != 4 ||
            reopenedChunk->second.blendmap_layers[0].pixels[3] != 244)
        {
            return Fail("the reloaded scene lost authored terrain data");
        }

        // A second save protects the previously valid document as .bak, and
        // successful saves maintain a bounded rolling backup history.
        if (!authored.Commands().Execute(
                std::make_unique<renegade::bridge::SetTranslationCommand>(
                    authored.Scenes().GetScene(),
                    landmark,
                    XMFLOAT3(8.0f, 3.0f, 4.0f))) ||
            !authored.SaveScene(scenePath.generic_string()) ||
            authored.Commands().IsDirty())
        {
            return Fail("transactional overwrite did not complete cleanly");
        }

        const auto previousPath =
            scenePath.parent_path() / "RoundTrip.bak.wiscene";
        renegade::bridge::StudioSession previous;
        if (!std::filesystem::is_regular_file(previousPath) ||
            !previous.LoadScene(previousPath.generic_string()))
        {
            return Fail("transactional overwrite did not preserve a .bak");
        }
        bool previousLandmarkRestored = false;
        for (const auto& entity : previous.Scenes().ListEntities())
        {
            if (entity.name != "Landmark")
            {
                continue;
            }
            const auto* transform = previous.Scenes()
                .GetScene()
                .transforms
                .GetComponent(entity.entity);
            previousLandmarkRestored = transform != nullptr &&
                NearlyEqual(transform->translation_local.x, 2.0f);
        }
        if (!previousLandmarkRestored)
        {
            return Fail("the .bak did not contain the previous scene state");
        }

        for (int version = 0; version < 12; ++version)
        {
            auto* landmarkTransform = authored.Scenes()
                .GetScene()
                .transforms
                .GetComponent(landmark);
            if (landmarkTransform == nullptr)
            {
                return Fail("rolling backup fixture lost its transform");
            }
            landmarkTransform->translation_local.x =
                10.0f + static_cast<float>(version);
            landmarkTransform->SetDirty();
            landmarkTransform->UpdateTransform();
            if (!authored.SaveScene(scenePath.generic_string()))
            {
                return Fail("rolling automatic backup save failed");
            }
        }

        const auto backupDirectory = std::filesystem::u8path(
            authored.Documents().AutomaticBackupDirectory(
                scenePath.generic_string()));
        std::size_t backupCount = 0;
        for (const auto& backup :
            std::filesystem::directory_iterator(backupDirectory))
        {
            if (backup.is_regular_file() &&
                backup.path().extension() == ".wiscene")
            {
                ++backupCount;
            }
        }
        if (backupCount !=
            renegade::bridge::SceneDocumentService::MaximumAutomaticBackups)
        {
            return Fail("automatic scene backup retention is not bounded");
        }

        // Save As writes the current document to a new path without changing
        // the already saved source archive.
        const auto* sourceTransform = authored.Scenes()
            .GetScene()
            .transforms
            .GetComponent(landmark);
        if (sourceTransform == nullptr)
        {
            return Fail("Save As fixture lost its source transform");
        }
        const float sourceVersion = sourceTransform->translation_local.x;
        if (!authored.Commands().Execute(
                std::make_unique<renegade::bridge::SetTranslationCommand>(
                    authored.Scenes().GetScene(),
                    landmark,
                    XMFLOAT3(44.0f, 3.0f, 4.0f))))
        {
            return Fail("Save As fixture command did not execute");
        }
        const auto saveAsPath = sceneFixture.path / "RoundTripCopy.wiscene";
        if (!authored.SaveScene(saveAsPath.generic_string()) ||
            authored.Scenes().CurrentPath() != saveAsPath.generic_string() ||
            authored.Commands().IsDirty())
        {
            return Fail("Save As did not establish the new saved document");
        }

        renegade::bridge::StudioSession originalAfterSaveAs;
        renegade::bridge::StudioSession copyAfterSaveAs;
        if (!originalAfterSaveAs.LoadScene(scenePath.generic_string()) ||
            !copyAfterSaveAs.LoadScene(saveAsPath.generic_string()))
        {
            return Fail("Save As archives could not be reopened");
        }
        float originalX = 0.0f;
        float copyX = 0.0f;
        for (const auto& entity : originalAfterSaveAs.Scenes().ListEntities())
        {
            if (entity.name == "Landmark")
            {
                originalX = originalAfterSaveAs.Scenes()
                    .GetScene()
                    .transforms
                    .GetComponent(entity.entity)
                    ->translation_local.x;
            }
        }
        for (const auto& entity : copyAfterSaveAs.Scenes().ListEntities())
        {
            if (entity.name == "Landmark")
            {
                copyX = copyAfterSaveAs.Scenes()
                    .GetScene()
                    .transforms
                    .GetComponent(entity.entity)
                    ->translation_local.x;
            }
        }
        if (!NearlyEqual(originalX, sourceVersion) ||
            !NearlyEqual(copyX, 44.0f))
        {
            return Fail("Save As changed the source or lost the current state");
        }

        // A failed save must not change document identity or clear dirty state.
        const auto invalidDestination =
            sceneFixture.path / "NotAFile.wiscene";
        std::filesystem::create_directory(invalidDestination);
        if (!authored.Commands().Execute(
                std::make_unique<renegade::bridge::SetTranslationCommand>(
                    authored.Scenes().GetScene(),
                    landmark,
                    XMFLOAT3(55.0f, 3.0f, 4.0f))) ||
            authored.SaveScene(invalidDestination.generic_string()) ||
            authored.Scenes().CurrentPath() != saveAsPath.generic_string() ||
            !authored.Commands().IsDirty())
        {
            return Fail("failed save changed document identity or dirty state");
        }
    }

    // A failed Open Scene attempt is transactional: the current scene,
    // selection, command history and dirty state remain available.
    {
        renegade::bridge::StudioSession session;
        const auto survivor = wi::ecs::CreateEntity();
        session.Scenes().GetScene().names.Create(survivor) = "Survivor";
        session.Scenes().GetScene().transforms.Create(survivor);
        session.Selection().Select(survivor);
        if (!session.Commands().Execute(
                std::make_unique<renegade::bridge::SetTranslationCommand>(
                    session.Scenes().GetScene(),
                    survivor,
                    XMFLOAT3(3.0f, 0.0f, 0.0f))))
        {
            return Fail("transactional open fixture command did not execute");
        }
        const auto missing = std::filesystem::temp_directory_path() /
            ("renegade-missing-scene-" +
                std::to_string(
                    std::chrono::steady_clock::now()
                        .time_since_epoch()
                        .count()) +
                ".wiscene");
        if (session.LoadScene(missing.generic_string()) ||
            !session.Scenes().ContainsEntity(survivor) ||
            session.Selection().SelectedEntity() != survivor ||
            !session.Commands().CanUndo() ||
            !session.Commands().IsDirty())
        {
            return Fail("failed scene open discarded the active editor state");
        }
    }

    // Open Scene has a distinct prepare/commit boundary. Preparing a valid
    // replacement on a worker must not touch the active document; committing
    // it replaces scene, path, selection and history together. An invalid
    // archive must never reach that commit point.
    {
        TemporaryDirectory openFixture;
        openFixture.path =
            std::filesystem::temp_directory_path() /
            (
                "renegade-document-open-" +
                std::to_string(
                    std::chrono::steady_clock::now()
                        .time_since_epoch()
                        .count()));
        std::filesystem::create_directories(openFixture.path);
        const auto sourcePath = openFixture.path / "Prepared.wiscene";
        const auto corruptPath = openFixture.path / "Corrupt.wiscene";
        const auto trailingPath = openFixture.path / "Trailing.wiscene";

        renegade::bridge::StudioSession source;
        const auto loadedEntity = wi::ecs::CreateEntity();
        source.Scenes().GetScene().names.Create(loadedEntity) =
            "Prepared Landmark";
        source.Scenes().GetScene().transforms.Create(loadedEntity);
        const auto loadedCamera = wi::ecs::CreateEntity();
        source.Scenes().GetScene().names.Create(loadedCamera) =
            "Authored Camera";
        source.Scenes().GetScene().transforms.Create(loadedCamera);
        source.Scenes().GetScene().cameras.Create(loadedCamera);
        if (!source.SaveScene(sourcePath.generic_string()))
        {
            return Fail("document-open fixture scene was not written");
        }
        {
            std::ofstream corrupt(corruptPath, std::ios::binary);
            corrupt << "this is not a Wicked archive";
        }
        std::filesystem::copy_file(sourcePath, trailingPath);
        {
            std::ofstream trailing(
                trailingPath,
                std::ios::binary | std::ios::app);
            trailing << "unexpected trailing bytes";
        }

        renegade::bridge::StudioSession session;
        const auto survivor = wi::ecs::CreateEntity();
        session.Scenes().GetScene().names.Create(survivor) = "Survivor";
        session.Scenes().GetScene().transforms.Create(survivor);
        session.Selection().Select(survivor);
        if (!session.Commands().Execute(
                std::make_unique<renegade::bridge::SetTranslationCommand>(
                    session.Scenes().GetScene(),
                    survivor,
                    XMFLOAT3(7.0f, 0.0f, 0.0f))))
        {
            return Fail("document-open history fixture did not execute");
        }

        auto corrupt = session.Documents().PrepareOpen(
            corruptPath.generic_string());
        if (corrupt.IsReady() ||
            session.Documents().CommitPreparedOpen(std::move(corrupt)) ||
            !session.Scenes().ContainsEntity(survivor) ||
            session.Selection().SelectedEntity() != survivor ||
            !session.Commands().CanUndo() ||
            !session.Commands().IsDirty())
        {
            return Fail("a corrupt prepared open damaged the active document");
        }

        // Wicked compressed archives decode the valid compressed stream and
        // tolerate bytes appended after it. That remains a valid scene, but
        // the prepare phase must still leave the active document untouched.
        auto trailing = session.Documents().PrepareOpen(
            trailingPath.generic_string());
        if (!trailing.IsReady() ||
            !session.Scenes().ContainsEntity(survivor) ||
            session.Selection().SelectedEntity() != survivor ||
            !session.Commands().CanUndo() ||
            !session.Commands().IsDirty())
        {
            return Fail(
                "preparing a WISCENE with trailing data damaged the active "
                "document");
        }

        auto prepared = session.Documents().PrepareOpen(
            sourcePath.generic_string());
        if (!prepared.IsReady() ||
            !session.Scenes().ContainsEntity(survivor) ||
            session.Selection().SelectedEntity() != survivor ||
            !session.Commands().CanUndo())
        {
            return Fail("preparing a valid scene mutated the active document");
        }
        // Wicked's EntitySerializer remaps entity handles on every
        // deserialize (wiScene_Serializers.cpp comments this as intentional,
        // to keep serialized entities "unique and persistent across the
        // scene"), even when reading into a brand-new scene. loadedEntity and
        // loadedCamera are therefore only valid identities in the *source*
        // session, before it was saved; the reopened entities in `session`
        // will carry different runtime IDs. Every postcondition below is
        // checked independently so a future regression reports exactly what
        // broke instead of one aggregate message, and the reopened entities
        // are located by stable semantic identity (name plus required
        // component) rather than by the pre-save runtime ID.
        if (!session.Documents().CommitPreparedOpen(std::move(prepared)))
        {
            return Fail("prepared scene commit returned false");
        }
        if (session.Scenes().ContainsEntity(survivor))
        {
            return Fail(
                "prepared scene commit did not clear the previous document");
        }

        const auto findEntityByName = [](
            const renegade::bridge::StudioSession& target,
            const std::string& name) -> wi::ecs::Entity
        {
            for (const auto& candidate : target.Scenes().ListEntities())
            {
                if (candidate.name == name)
                {
                    return candidate.entity;
                }
            }
            return wi::ecs::INVALID_ENTITY;
        };

        const auto reopenedLandmark =
            findEntityByName(session, "Prepared Landmark");
        if (reopenedLandmark == wi::ecs::INVALID_ENTITY ||
            session.Scenes().GetScene().transforms.GetComponent(
                reopenedLandmark) == nullptr)
        {
            return Fail(
                "reopened scene is missing the prepared landmark entity");
        }
        if (session.Scenes().CurrentPath() != sourcePath.generic_string())
        {
            return Fail(
                "prepared scene commit did not update the current path");
        }
        if (session.Selection().HasSelection())
        {
            return Fail("prepared scene commit left a stale selection");
        }
        if (session.Commands().CanUndo())
        {
            return Fail("prepared scene commit left stale Undo history");
        }
        if (session.Commands().IsDirty())
        {
            return Fail("prepared scene commit left the document dirty");
        }

        const auto reopenedCamera =
            findEntityByName(session, "Authored Camera");
        if (reopenedCamera == wi::ecs::INVALID_ENTITY ||
            session.Scenes().GetScene().cameras.GetComponent(
                reopenedCamera) == nullptr)
        {
            return Fail(
                "reopened scene is missing the authored camera entity");
        }
        if (session.Documents().LastOpenedCamera() != reopenedCamera)
        {
            return Fail(
                "prepared scene commit did not carry the authored camera "
                "identity through remapping");
        }
    }

    TemporaryDirectory projectFixture;
    projectFixture.path =
        std::filesystem::temp_directory_path() /
        (
            "renegade-project-service-" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto projectParent = projectFixture.path / "Projects";
    const auto templateScene = projectFixture.path / "ProvingGround.wiscene";
    std::filesystem::create_directories(projectParent);
    {
        std::ofstream templateFile(templateScene, std::ios::binary);
        templateFile << "Renegade project service scene fixture";
    }

    renegade::bridge::ProjectService projects;
    const auto stateFile = projectFixture.path / "State" / "Studio.ini";
    projects.Initialize(stateFile.generic_string());
    if (!projects.CreateProject(
            projectParent.generic_string(),
            "Test Project",
            templateScene.generic_string()))
    {
        return Fail("project service did not create a project");
    }

    const auto descriptor =
        projectParent / "Test Project" / "Test Project.renegade";
    const auto startupScene =
        projectParent / "Test Project" / "Content" / "Scenes" / "Main.wiscene";
    if (!projects.HasProject() ||
        projects.CurrentProject().name != "Test Project" ||
        projects.RecentProjects().size() != 1 ||
        !std::filesystem::is_regular_file(descriptor) ||
        !std::filesystem::is_regular_file(startupScene))
    {
        return Fail("created project structure or metadata is incorrect");
    }

    renegade::bridge::ProjectService reopenedProjects;
    reopenedProjects.Initialize(stateFile.generic_string());
    if (reopenedProjects.RecentProjects().size() != 1 ||
        !reopenedProjects.OpenProject(descriptor.generic_string()) ||
        reopenedProjects.StartupScenePath() !=
            std::filesystem::absolute(startupScene)
                .lexically_normal()
                .generic_string())
    {
        return Fail("project descriptor or recent-project state did not reopen");
    }

    // Editor preferences. These describe how the creator likes Studio to
    // behave, so they live beside the recent-project registry and must survive
    // a restart without ever reaching a scene or the .renegade descriptor.
    if (!reopenedProjects.GetEditorPreference("grid_visible", true) ||
        reopenedProjects.GetEditorPreference("never_set_key", false))
    {
        return Fail("an unset editor preference did not fall back");
    }

    reopenedProjects.SetEditorPreference("grid_visible", false);

    renegade::bridge::ProjectService restartedProjects;
    restartedProjects.Initialize(stateFile.generic_string());
    if (restartedProjects.GetEditorPreference("grid_visible", true))
    {
        return Fail("an editor preference did not survive a restart");
    }

    std::cout
        << "PASS: hierarchy filtering, selection, full transform, "
           "weather and duplicate/delete undo-redo, repeated history, "
           "no-op filtering, Proving Ground blueprint structure, unique "
           "generated names, headless scene save/reload, project lifecycle, "
           "transactional prepared scene open, corrupt-open protection, "
           "dirty-state tracking, and "
           "persisted editor preferences\n";
    return 0;
}
