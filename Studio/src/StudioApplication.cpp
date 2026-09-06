#include "StudioApplication.h"
#include "StudioUserPreferences.h"

#include "renegade/bridge/TestLevelSnapshotService.h"
#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "renegade/bridge/CreatorModelMaterialPreparationService.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/FlowService.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cfloat>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <utility>

namespace
{
    namespace fs = std::filesystem;
    constexpr std::uint8_t SelectionStencilReference = 0x0F;
    constexpr wi::Color HologramIdle = wi::Color(12, 16, 19, 255);
    constexpr wi::Color HologramFocus = wi::Color(26, 31, 35, 255);
    constexpr wi::Color HologramActive = wi::Color(38, 43, 47, 255);
    constexpr wi::Color HologramText = wi::Color(244, 244, 244, 255);
    constexpr wi::Color HologramMuted = wi::Color(178, 178, 176, 255);
    constexpr wi::Color HologramBorder = wi::Color(38, 52, 61, 255);
    constexpr wi::Color HologramPanel = wi::Color(8, 12, 16, 255);
    constexpr wi::Color HologramSelected = wi::Color(44, 35, 29, 255);
    constexpr wi::Color HubBackground = wi::Color(4, 7, 10, 255);
    constexpr wi::Color HubSurface = wi::Color(8, 14, 18, 255);
    constexpr wi::Color HubSurfaceRaised = wi::Color(11, 20, 25, 255);
    constexpr wi::Color HubBorder = wi::Color(28, 68, 82, 255);
    constexpr wi::Color HubCyan = wi::Color(92, 208, 236, 255);
    constexpr wi::Color HubOrange = wi::Color(222, 91, 29, 255);
    constexpr wi::Color HubMuted = wi::Color(139, 158, 166, 255);
    constexpr wi::Color HubSelected = wi::Color(13, 35, 43, 255);
    constexpr wi::Color WarningAmber = wi::Color(255, 150, 40, 255);
    constexpr int LayoutPreferenceBits = 10;

    XMFLOAT4 RotationFromTo(
        const XMFLOAT3& source,
        const XMFLOAT3& target) noexcept
    {
        const XMVECTOR sourceVector = XMLoadFloat3(&source);
        const XMVECTOR targetVector = XMLoadFloat3(&target);
        if (XMVectorGetX(XMVector3LengthSq(sourceVector)) < 0.0001f ||
            XMVectorGetX(XMVector3LengthSq(targetVector)) < 0.0001f)
        {
            return XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        }
        const XMVECTOR from = XMVector3Normalize(sourceVector);
        const XMVECTOR to = XMVector3Normalize(targetVector);
        const float dot = XMVectorGetX(XMVector3Dot(from, to));
        if (dot >= 0.9999f)
        {
            return XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        }
        if (dot <= -0.9999f)
        {
            XMVECTOR axis = XMVector3Cross(from, XMVectorSet(1, 0, 0, 0));
            if (XMVectorGetX(XMVector3LengthSq(axis)) < 0.0001f)
            {
                axis = XMVector3Cross(from, XMVectorSet(0, 0, 1, 0));
            }
            XMFLOAT4 rotation;
            XMStoreFloat4(
                &rotation,
                XMQuaternionRotationAxis(XMVector3Normalize(axis), XM_PI));
            return rotation;
        }

        const XMVECTOR axis = XMVector3Cross(from, to);
        XMFLOAT4 rotation;
        XMStoreFloat4(
            &rotation,
            XMQuaternionNormalize(XMVectorSet(
                XMVectorGetX(axis),
                XMVectorGetY(axis),
                XMVectorGetZ(axis),
                1.0f + dot)));
        return rotation;
    }

    wi::ecs::Entity ResolveReusableSelectionRoot(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity selected) noexcept
    {
        if (selected == wi::ecs::INVALID_ENTITY)
            return wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity current = selected;
        const std::size_t maximumDepth = scene.hierarchy.GetCount() + 1;
        for (std::size_t depth = 0;
            current != wi::ecs::INVALID_ENTITY && depth <= maximumDepth; ++depth)
        {
            const auto* metadata = scene.metadatas.GetComponent(current);
            if (metadata != nullptr && metadata->string_values.has(
                    renegade::bridge::ReusableAssetInstanceIdMetadataKey))
            {
                return current;
            }
            const auto* hierarchy = scene.hierarchy.GetComponent(current);
            if (hierarchy == nullptr || hierarchy->parentID == wi::ecs::INVALID_ENTITY ||
                hierarchy->parentID == current)
            {
                break;
            }
            current = hierarchy->parentID;
        }
        return wi::ecs::INVALID_ENTITY;
    }

    void CollectReusableSelectionObjects(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity root,
        std::vector<wi::ecs::Entity>& objects)
    {
        objects.clear();
        if (root == wi::ecs::INVALID_ENTITY)
            return;
        for (std::size_t index = 0; index < scene.objects.GetCount(); ++index)
        {
            const wi::ecs::Entity entity = scene.objects.GetEntity(index);
            if (entity == root || scene.Entity_IsDescendant(entity, root))
                objects.push_back(entity);
        }
    }

    const char* PlacementLightName(
        const wi::scene::LightComponent::LightType type) noexcept
    {
        switch (type)
        {
        case wi::scene::LightComponent::SPOT:
            return "SPOT";
        case wi::scene::LightComponent::RECTANGLE:
            return "RECTANGLE";
        case wi::scene::LightComponent::DIRECTIONAL:
            return "DIRECTIONAL";
        case wi::scene::LightComponent::POINT:
        default:
            return "POINT";
        }
    }

    renegade::studio::RenegadeStudioChrome::HierarchyCategory
    ToHierarchyCategory(
        const renegade::bridge::SceneEntityCategory category) noexcept
    {
        using BridgeCategory = renegade::bridge::SceneEntityCategory;
        using ChromeCategory =
            renegade::studio::RenegadeStudioChrome::HierarchyCategory;
        switch (category)
        {
        case BridgeCategory::Lights:
            return ChromeCategory::Lights;
        case BridgeCategory::Models:
            return ChromeCategory::Models;
        case BridgeCategory::Characters:
            return ChromeCategory::Characters;
        case BridgeCategory::Cameras:
            return ChromeCategory::Cameras;
        case BridgeCategory::Terrain:
            return ChromeCategory::Terrain;
        case BridgeCategory::Effects:
            return ChromeCategory::Effects;
        case BridgeCategory::Audio:
            return ChromeCategory::Audio;
        case BridgeCategory::Other:
        default:
            return ChromeCategory::Other;
        }
    }

    void DrawEditorLine(
        const XMFLOAT2& start,
        const XMFLOAT2& end,
        const XMFLOAT4& color)
    {
        wi::renderer::RenderableLine2D line;
        line.start = start;
        line.end = end;
        line.color_start = color;
        line.color_end = color;
        wi::renderer::DrawLine(line);
    }

    int ReadLayoutPreference(
        const renegade::bridge::ProjectService& projects,
        const std::string& key,
        const int fallback)
    {
        int value = 0;
        for (int bit = 0; bit < LayoutPreferenceBits; ++bit)
        {
            if (projects.GetEditorPreference(
                    key + "_bit_" + std::to_string(bit),
                    false))
            {
                value |= 1 << bit;
            }
        }
        return value > 0 ? value : fallback;
    }

    void WriteLayoutPreference(
        renegade::bridge::ProjectService& projects,
        const std::string& key,
        const int value)
    {
        for (int bit = 0; bit < LayoutPreferenceBits; ++bit)
        {
            projects.SetEditorPreference(
                key + "_bit_" + std::to_string(bit),
                (value & (1 << bit)) != 0);
        }
    }

    // Keep the temporary preview beyond the normal camera's 1000 m far plane
    // so the authored level remains invisible, but do not push it to 100 km.
    // At Y=100000 a 32-bit transform has roughly 7.8 mm granularity, which
    // visibly quantizes detailed character geometry and normals even though
    // the source mesh is intact. Y=2048 keeps sub-millimetre precision while
    // retaining render isolation from the authored scene at the origin.
    constexpr float CreatorImportStageHeight = 2048.0f;
    constexpr float CreatorImportPreviewFov = 32.0f * XM_PI / 180.0f;


    struct CreatorThumbnailWeatherSnapshot
    {
        bool valid = false;
        std::uint32_t flags = 0;
        XMFLOAT3 horizon = {};
        XMFLOAT3 zenith = {};
        float skyExposure = 1.0f;
        float fogDensity = 0.0f;
        float stars = 0.0f;
        std::string skyMapName;
        wi::Resource skyMap;
    };

    CreatorThumbnailWeatherSnapshot CaptureThumbnailWeather(
        const wi::scene::WeatherComponent& weather)
    {
        CreatorThumbnailWeatherSnapshot snapshot;
        snapshot.valid = true;
        snapshot.flags = weather._flags;
        snapshot.horizon = weather.horizon;
        snapshot.zenith = weather.zenith;
        snapshot.skyExposure = weather.skyExposure;
        snapshot.fogDensity = weather.fogDensity;
        snapshot.stars = weather.stars;
        snapshot.skyMapName = weather.skyMapName;
        snapshot.skyMap = weather.skyMap;
        return snapshot;
    }

    void ApplyNeutralThumbnailWeather(
        wi::scene::WeatherComponent& weather)
    {
        weather.SetRealisticSky(false);
        weather.SetVolumetricClouds(false);
        weather.SetHeightFog(false);
        weather.SetOverrideFogColor(false);
        weather.horizon = XMFLOAT3(0.065f, 0.07f, 0.075f);
        weather.zenith = XMFLOAT3(0.065f, 0.07f, 0.075f);
        weather.skyExposure = 1.0f;
        weather.fogDensity = 0.0f;
        weather.stars = 0.0f;
        weather.skyMapName.clear();
        weather.skyMap = {};
    }

    void RestoreThumbnailWeather(
        wi::scene::WeatherComponent& weather,
        const CreatorThumbnailWeatherSnapshot& snapshot)
    {
        if (!snapshot.valid)
            return;
        weather._flags = snapshot.flags;
        weather.horizon = snapshot.horizon;
        weather.zenith = snapshot.zenith;
        weather.skyExposure = snapshot.skyExposure;
        weather.fogDensity = snapshot.fogDensity;
        weather.stars = snapshot.stars;
        weather.skyMapName = snapshot.skyMapName;
        weather.skyMap = snapshot.skyMap;
    }

    bool SaveCreatorSquareThumbnail(
        const wi::graphics::Texture& texture,
        const std::string& path)
    {
        if (!texture.IsValid())
            return false;

        auto desc = texture.GetDesc();
        if (desc.width == 0 || desc.height == 0 ||
            desc.depth != 1 || desc.array_size != 1 ||
            wi::graphics::GetFormatPlaneCount(desc.format) != 1 ||
            wi::graphics::GetFormatBlockSize(desc.format) != 1)
        {
            return false;
        }

        const std::uint32_t stride =
            wi::graphics::GetFormatStride(desc.format);
        if (stride == 0)
            return false;

        wi::vector<std::uint8_t> pixels;
        if (!wi::helper::saveTextureToMemory(texture, pixels))
            return false;

        const std::uint32_t side = std::min(desc.width, desc.height);
        const std::uint32_t offsetX = (desc.width - side) / 2;
        const std::uint32_t offsetY = (desc.height - side) / 2;
        const std::size_t sourceRow =
            static_cast<std::size_t>(desc.width) * stride;
        const std::size_t squareRow =
            static_cast<std::size_t>(side) * stride;
        const std::size_t required =
            sourceRow * static_cast<std::size_t>(desc.height);
        if (pixels.size() < required)
            return false;

        wi::vector<std::uint8_t> square(
            squareRow * static_cast<std::size_t>(side));
        for (std::uint32_t row = 0; row < side; ++row)
        {
            const std::uint8_t* source =
                pixels.data() +
                (static_cast<std::size_t>(row + offsetY) * sourceRow) +
                (static_cast<std::size_t>(offsetX) * stride);
            std::uint8_t* destination =
                square.data() +
                static_cast<std::size_t>(row) * squareRow;
            std::memcpy(destination, source, squareRow);
        }

        desc.width = side;
        desc.height = side;
        desc.depth = 1;
        desc.array_size = 1;
        desc.mip_levels = 1;
        return wi::helper::saveTextureToFile(square, desc, path);
    }

    void ApplyCreatorImportPreviewLighting();
    struct CreatorModelImportWorkspaceState
    {
        bool active = false;
        bool committing = false;
        std::string sourcePath;
        std::size_t undoBaseline = 0;
        wi::ecs::Entity previewRoot = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity previewLight = wi::ecs::INVALID_ENTITY;
        wi::scene::TransformComponent cameraBefore;
        float cameraFovBefore = XM_PIDIV4;
        bool cameraCaptured = false;
        float automaticScale = 1.0f;
        renegade::bridge::ModelBounds sourceBounds;
        renegade::bridge::ImportedSceneSummary summary;
        renegade::bridge::ImportedModelEvidence evidence;
        std::vector<wi::ecs::Entity> materialEntities;
        std::vector<wi::ecs::Entity> animationEntities;
        std::vector<renegade::bridge::CreatorMaterialSourceOverride> materialOverrides;
        std::vector<renegade::bridge::CreatorAnimationImportRecipe> animationRecipe;
        std::size_t selectedMaterial = 0;
        std::size_t selectedAnimation = 0;
        XMFLOAT3 positionOffset = XMFLOAT3(0.0f, 0.0f, 0.0f);
        XMFLOAT3 rotationDegrees = XMFLOAT3(0.0f, 0.0f, 0.0f);
        XMFLOAT3 scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
        bool scaleLinked = true;
        std::string assetName;
        std::string destinationFolder = "Content/Models";
        float lightIntensity = 4.0f;
        float lightAzimuth = -35.0f;
        float lightElevation = 35.0f;
        float ambientBrightness = 0.35f;
        XMFLOAT3 ambientBefore = {};
        wi::ecs::Entity weatherEntity = wi::ecs::INVALID_ENTITY;
        bool ambientCaptured = false;
        bool mannequinVisible = true;
        std::string thumbnailCapturePath;
        std::uint32_t thumbnailCaptureRevision = 0;
        bool thumbnailCapturePending = false;
        bool thumbnailPresentationOverridden = false;
        float thumbnailRestoreLightIntensity = 4.0f;
        float thumbnailRestoreLightAzimuth = -35.0f;
        float thumbnailRestoreLightElevation = 35.0f;
        float thumbnailRestoreAmbientBrightness = 0.35f;
        CreatorThumbnailWeatherSnapshot thumbnailSceneWeatherBefore;
        CreatorThumbnailWeatherSnapshot thumbnailEntityWeatherBefore;
        renegade::bridge::PreparedModelImport preparedForCommit;
        std::size_t workspaceSection = 0;
    };

    wi::allocator::shared_ptr<wi::scene::Scene> CloneCreatorPreviewScene(
        wi::scene::Scene& source,
        std::string& error)
    {
        auto clone = wi::allocator::make_shared_single<wi::scene::Scene>();
        wi::Archive archive;
        archive.SetReadModeAndResetPos(false);
        wi::ecs::EntitySerializer serializer;
        source.componentLibrary.Serialize(archive, serializer);
        wi::jobsystem::Wait(serializer.ctx);
        archive.SetReadModeAndResetPos(true);
        clone->componentLibrary.Serialize(archive, serializer);
        wi::jobsystem::Wait(serializer.ctx);
        if (clone->transforms.GetCount() == 0 ||
            clone->objects.GetCount() == 0 ||
            clone->meshes.GetCount() == 0)
        {
            error = "The already-converted model could not be cloned for importer preview.";
            return {};
        }
        error.clear();
        return clone;
    }

    CreatorModelImportWorkspaceState creatorModelImporter;


    void BeginCreatorThumbnailPresentation()
    {
        auto* session = renegade::bridge::StudioSession::Current();
        if (session == nullptr || !creatorModelImporter.active)
            return;

        creatorModelImporter.thumbnailRestoreLightIntensity =
            creatorModelImporter.lightIntensity;
        creatorModelImporter.thumbnailRestoreLightAzimuth =
            creatorModelImporter.lightAzimuth;
        creatorModelImporter.thumbnailRestoreLightElevation =
            creatorModelImporter.lightElevation;
        creatorModelImporter.thumbnailRestoreAmbientBrightness =
            creatorModelImporter.ambientBrightness;

        auto& scene = session->Scenes().GetScene();
        creatorModelImporter.thumbnailSceneWeatherBefore =
            CaptureThumbnailWeather(scene.weather);
        creatorModelImporter.thumbnailEntityWeatherBefore = {};
        ApplyNeutralThumbnailWeather(scene.weather);
        if (auto* weather =
                scene.weathers.GetComponent(creatorModelImporter.weatherEntity))
        {
            creatorModelImporter.thumbnailEntityWeatherBefore =
                CaptureThumbnailWeather(*weather);
            ApplyNeutralThumbnailWeather(*weather);
        }

        creatorModelImporter.lightIntensity = 4.0f;
        creatorModelImporter.lightAzimuth = -35.0f;
        creatorModelImporter.lightElevation = 35.0f;
        creatorModelImporter.ambientBrightness = 0.35f;
        creatorModelImporter.thumbnailPresentationOverridden = true;
        ApplyCreatorImportPreviewLighting();
    }

    void RestoreCreatorThumbnailPresentation()
    {
        if (!creatorModelImporter.thumbnailPresentationOverridden)
            return;

        auto* session = renegade::bridge::StudioSession::Current();
        if (session != nullptr)
        {
            auto& scene = session->Scenes().GetScene();
            RestoreThumbnailWeather(
                scene.weather,
                creatorModelImporter.thumbnailSceneWeatherBefore);
            if (auto* weather =
                    scene.weathers.GetComponent(
                        creatorModelImporter.weatherEntity))
            {
                RestoreThumbnailWeather(
                    *weather,
                    creatorModelImporter.thumbnailEntityWeatherBefore);
            }
        }

        creatorModelImporter.lightIntensity =
            creatorModelImporter.thumbnailRestoreLightIntensity;
        creatorModelImporter.lightAzimuth =
            creatorModelImporter.thumbnailRestoreLightAzimuth;
        creatorModelImporter.lightElevation =
            creatorModelImporter.thumbnailRestoreLightElevation;
        creatorModelImporter.ambientBrightness =
            creatorModelImporter.thumbnailRestoreAmbientBrightness;
        creatorModelImporter.thumbnailPresentationOverridden = false;
        ApplyCreatorImportPreviewLighting();
    }
    renegade::studio::RenegadeComboBox creatorImportMaterialCombo;
    renegade::studio::RenegadeComboBox creatorImportAnimationCombo;
    wi::gui::Label creatorImportMaterialLabel;
    wi::gui::Label creatorImportMaterialReadout;
    wi::gui::Label creatorImportTextureHelp;
    renegade::studio::RenegadeTextureMapList creatorImportTexturePreviews;
    renegade::studio::RenegadeComboBox creatorImportTextureSlotCombo;
    renegade::studio::RenegadeTextInputField creatorImportTexturePath;
    renegade::studio::RenegadeButton creatorImportTextureBrowse;
    renegade::studio::RenegadeButton creatorImportTextureClear;
    std::size_t creatorImportTextureSlot = 0;
    wi::gui::Label creatorImportAnimationLabel;
    renegade::studio::RenegadeTextInputField creatorImportAnimationName;
    renegade::studio::RenegadeTextInputField creatorImportAnimationStart;
    renegade::studio::RenegadeTextInputField creatorImportAnimationEnd;
    renegade::studio::RenegadeComboBox creatorImportAnimationEnabled;
    renegade::studio::RenegadeButton creatorImportAnimationAdd;
    renegade::studio::RenegadeButton creatorImportAnimationDelete;
    wi::gui::Label creatorImportAnimationReadout;
    wi::gui::Label creatorImportTransformLabel;
    renegade::studio::RenegadeComboBox creatorImportSectionCombo;
    renegade::studio::RenegadeTextInputField creatorImportAssetName;
    renegade::studio::RenegadeTextInputField creatorImportDestination;
    renegade::studio::RenegadeSlider creatorImportPositionX;
    renegade::studio::RenegadeSlider creatorImportPositionY;
    renegade::studio::RenegadeSlider creatorImportPositionZ;
    renegade::studio::RenegadeSlider creatorImportRotationX;
    renegade::studio::RenegadeSlider creatorImportRotationY;
    renegade::studio::RenegadeSlider creatorImportRotationZ;
    renegade::studio::RenegadeSlider creatorImportScaleX;
    renegade::studio::RenegadeSlider creatorImportScaleY;
    renegade::studio::RenegadeSlider creatorImportScaleZ;
    renegade::studio::RenegadeCheckBox creatorImportScaleLinked;
    renegade::studio::RenegadeComboBox creatorImportDimensionPreset;
    wi::gui::Label creatorImportMaterialScalarLabel;
    renegade::studio::RenegadeSlider creatorImportRoughness;
    renegade::studio::RenegadeSlider creatorImportMetalness;
    renegade::studio::RenegadeSlider creatorImportReflectance;
    renegade::studio::RenegadeSlider creatorImportNormalStrength;
    renegade::studio::RenegadeSlider creatorImportAoStrength;
    renegade::studio::RenegadeSlider creatorImportEmissiveStrength;
    wi::gui::Label creatorImportLightingLabel;
    renegade::studio::RenegadeSlider creatorImportLightIntensity;
    renegade::studio::RenegadeSlider creatorImportLightAzimuth;
    renegade::studio::RenegadeSlider creatorImportLightElevation;
    renegade::studio::RenegadeSlider creatorImportAmbientBrightness;
    renegade::studio::RenegadeComboBox creatorImportLightingPreset;
    renegade::studio::RenegadeButton creatorImportLightingReset;
    renegade::studio::RenegadeCheckBox creatorImportMannequinVisible;
    wi::gui::Label creatorImportHelpLabel;
    wi::gui::Label creatorImportActionBar;
    wi::gui::Image creatorImportThumbnailPreview;
    wi::Resource creatorImportThumbnailPreviewResource;
    renegade::studio::RenegadeButton creatorImportThumbnailCapture;
    wi::gui::Label creatorImportThumbnailStatus;
    wi::Resource creatorImportHumanReference;

    std::string CreatorImportEntityName(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const std::string& fallback)
    {
        const auto* name = scene.names.GetComponent(entity);
        return name != nullptr && !name->name.empty() ? name->name : fallback;
    }

    std::vector<std::string> CreatorImportMaterialUsages(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity materialEntity)
    {
        std::vector<std::string> usages;
        for (std::size_t meshIndex = 0;
            meshIndex < scene.meshes.GetCount();
            ++meshIndex)
        {
            const auto meshEntity = scene.meshes.GetEntity(meshIndex);
            const auto& mesh = scene.meshes[meshIndex];
            for (const auto& subset : mesh.subsets)
            {
                if (subset.materialID != materialEntity)
                    continue;
                std::string usage = CreatorImportEntityName(
                    scene,
                    meshEntity,
                    "Mesh " + std::to_string(meshIndex + 1));
                if (!subset.surfaceName.empty() && subset.surfaceName != usage)
                    usage += " / " + subset.surfaceName;
                if (std::find(usages.begin(), usages.end(), usage) == usages.end())
                    usages.push_back(std::move(usage));
            }
        }
        return usages;
    }

    std::string CreatorImportMaterialDisplayName(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity materialEntity,
        const std::size_t index)
    {
        std::string result = CreatorImportEntityName(
            scene,
            materialEntity,
            "Material " + std::to_string(index + 1));
        const auto usages = CreatorImportMaterialUsages(scene, materialEntity);
        if (!usages.empty())
            result += " // " + usages.front();
        if (const auto* material = scene.materials.GetComponent(materialEntity))
        {
            const auto& base = material->textures[
                wi::scene::MaterialComponent::BASECOLORMAP].name;
            if (!base.empty())
                result += " // " + fs::u8path(base).filename().generic_u8string();
        }
        return result;
    }

    void UpdateCreatorImportScaleReferenceLabel()
    {
        std::ostringstream label;
        label << "SHOW MALE + 0.00 M TO 1.82 M RULER";
        if (creatorModelImporter.sourceBounds.valid)
        {
            const float modelHeight = std::abs(
                creatorModelImporter.sourceBounds.maximum.y -
                creatorModelImporter.sourceBounds.minimum.y) *
                std::abs(creatorModelImporter.scale.y);
            label << "   //   MODEL HEIGHT " << std::fixed
                << std::setprecision(2) << modelHeight << " M";
        }
        creatorImportMannequinVisible.SetText(label.str());
    }

    bool CreatorImportWorldBounds(XMFLOAT3& minimum, XMFLOAT3& maximum)
    {
        auto* session = renegade::bridge::StudioSession::Current();
        if (session == nullptr || !creatorModelImporter.sourceBounds.valid)
            return false;
        const auto& scene = session->Scenes().GetScene();
        const auto* root = scene.transforms.GetComponent(
            creatorModelImporter.previewRoot);
        if (root == nullptr)
            return false;

        minimum = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
        maximum = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        const auto& bounds = creatorModelImporter.sourceBounds;
        const XMMATRIX world = XMLoadFloat4x4(&root->world);
        for (const float x : {bounds.minimum.x, bounds.maximum.x})
        for (const float y : {bounds.minimum.y, bounds.maximum.y})
        for (const float z : {bounds.minimum.z, bounds.maximum.z})
        {
            XMFLOAT3 corner;
            XMStoreFloat3(
                &corner,
                XMVector3TransformCoord(XMVectorSet(x, y, z, 1.0f), world));
            minimum.x = std::min(minimum.x, corner.x);
            minimum.y = std::min(minimum.y, corner.y);
            minimum.z = std::min(minimum.z, corner.z);
            maximum.x = std::max(maximum.x, corner.x);
            maximum.y = std::max(maximum.y, corner.y);
            maximum.z = std::max(maximum.z, corner.z);
        }
        return true;
    }

    void QueueCreatorImportScaleRuler()
    {
        if (!creatorModelImporter.active ||
            creatorModelImporter.thumbnailCapturePending ||
            !creatorModelImporter.mannequinVisible)
            return;
        XMFLOAT3 minimum;
        XMFLOAT3 maximum;
        if (!CreatorImportWorldBounds(minimum, maximum))
            return;

        constexpr float ReferenceHeight = 1.82f;
        constexpr float ReferenceHalfWidth = 0.23f;
        constexpr float Clearance = 0.45f;
        constexpr float RulerGap = 0.36f;
        constexpr float CapHalfWidth = 0.10f;
        const float mannequinX = minimum.x - Clearance - ReferenceHalfWidth;
        const float rulerX = mannequinX - ReferenceHalfWidth - RulerGap;
        const float z = (minimum.z + maximum.z) * 0.5f;
        const XMFLOAT4 color(0.94f, 0.94f, 0.92f, 0.95f);
        const auto line = [color](const XMFLOAT3& start, const XMFLOAT3& end)
        {
            wi::renderer::RenderableLine ruler;
            ruler.start = start;
            ruler.end = end;
            ruler.color_start = color;
            ruler.color_end = color;
            wi::renderer::DrawLine(ruler, true);
        };
        line(
            XMFLOAT3(rulerX, CreatorImportStageHeight, z),
            XMFLOAT3(rulerX, CreatorImportStageHeight + ReferenceHeight, z));
        line(
            XMFLOAT3(rulerX - CapHalfWidth, CreatorImportStageHeight, z),
            XMFLOAT3(rulerX + CapHalfWidth, CreatorImportStageHeight, z));
        line(
            XMFLOAT3(rulerX - CapHalfWidth, CreatorImportStageHeight + ReferenceHeight, z),
            XMFLOAT3(rulerX + CapHalfWidth, CreatorImportStageHeight + ReferenceHeight, z));
    }

    void ApplyCreatorImportPreviewTransform()
    {
        auto* session = renegade::bridge::StudioSession::Current();
        if (session == nullptr || !creatorModelImporter.active ||
            creatorModelImporter.previewRoot == wi::ecs::INVALID_ENTITY)
            return;
        auto& scene = session->Scenes().GetScene();
        auto* transform = scene.transforms.GetComponent(creatorModelImporter.previewRoot);
        if (transform == nullptr)
            return;
        transform->translation_local = XMFLOAT3(
            creatorModelImporter.positionOffset.x,
            CreatorImportStageHeight + creatorModelImporter.positionOffset.y,
            creatorModelImporter.positionOffset.z);
        XMVECTOR q = XMQuaternionRotationRollPitchYaw(
            XMConvertToRadians(creatorModelImporter.rotationDegrees.x),
            XMConvertToRadians(creatorModelImporter×½ý÷{h‘éì¶»§q«^tectorSectionPreferenceStore> inspectorSectionPreferences_;
        InspectorSectionRegistry inspectorSectionRegistry_;
        std::uint64_t inspectorDisclosureSelectionRevision_ =
            static_cast<std::uint64_t>(-1);
        int inspectorDisclosureViewToken_ = -1;
        SceneInspectorButton transformSectionHeader_;
        SceneInspectorButton renderingSectionHeader_;
        SceneInspectorButton materialsSectionHeader_;
        wi::gui::Label inspectorLabel_;
        wi::gui::Label positionLabel_;
        wi::gui::Label rotationLabel_;
        wi::gui::Label scaleLabel_;
        SceneInspectorTextInputField translationX_;
        SceneInspectorTextInputField translationY_;
        SceneInspectorTextInputField translationZ_;
        SceneInspectorTextInputField rotationX_;
        SceneInspectorTextInputField rotationY_;
        SceneInspectorTextInputField rotationZ_;
        SceneInspectorTextInputField scaleX_;
        SceneInspectorTextInputField scaleY_;
        SceneInspectorTextInputField scaleZ_;
        wi::gui::Label sceneIdentityLabel_;
        SceneInspectorTextInputField sceneNameInput_;
        wi::gui::Label sceneLayerLabel_;
        SceneInspectorButton sceneLayerAllButton_;
        SceneInspectorButton sceneLayerNoneButton_;
        std::array<SceneInspectorCheckBox, 32> sceneLayerBits_;
        wi::gui::Label sceneMetadataLabel_;
        SceneInspectorComboBox sceneMetadataPreset_;
        wi::gui::Label sceneObjectLabel_;
        SceneInspectorCheckBox sceneObjectRenderable_;
        SceneInspectorCheckBox sceneObjectCastShadow_;
        SceneInspectorCheckBox sceneObjectForeground_;
        SceneInspectorCheckBox sceneObjectMainCamera_;
        SceneInspectorCheckBox sceneObjectReflections_;
        SceneInspectorCheckBox sceneObjectWetmap_;
        wi::gui::Label playerLabel_;
        wi::gui::Label playerCameraMode_;
        SceneInspectorSlider playerCapsuleRadius_;
        SceneInspectorSlider playerCapsuleHeight_;
        SceneInspectorSlider playerEyeHeight_;
        SceneInspectorSlider playerWalkSpeed_;
        SceneInspectorSlider playerSprintSpeed_;
        SceneInspectorSlider playerJumpSpeed_;
        SceneInspectorSlider playerLookSensitivity_;
        SceneInspectorSlider playerMaximumSlope_;
        SceneInspectorSlider playerGravityFactor_;
        SceneInspectorSlider playerMinimumPitch_;
        SceneInspectorSlider playerMaximumPitch_;
        wi::gui::Label materialLabel_;
        SceneInspectorComboBox materialSelector_;
        SceneInspectorComboBox materialShaderType_;
        SceneInspectorComboBox materialBlendMode_;
        wi::gui::Label materialCoreLabel_;
        SceneInspectorSlider materialBaseColorRed_;
        SceneInspectorSlider materialBaseColorGreen_;
        SceneInspectorSlider materialBaseColorBlue_;
        SceneInspectorSlider materialOpacity_;
        SceneInspectorSlider materialMetalness_;
        SceneInspectorSlider materialRoughness_;
        SceneInspectorSlider materialReflectance_;
        SceneInspectorSlider materialNormalStrength_;
        SceneInspectorSlider materialAlphaRef_;
        SceneInspectorSlider materialEmissiveRed_;
        SceneInspectorSlider materialEmissiveGreen_;
        SceneInspectorSlider materialEmissiveBlue_;
        SceneInspectorSlider materialEmissiveStrength_;
        SceneInspectorCheckBox materialReceiveShadow_;
        SceneInspectorCheckBox materialCastShadow_;
        SceneInspectorCheckBox materialUseVertexColors_;
        SceneInspectorCheckBox materialDoubleSided_;
        wi::gui::Label materialUvLabel_;
        SceneInspectorSlider materialTilingU_;
        SceneInspectorSlider materialTilingV_;
        SceneInspectorSlider materialOffsetU_;
        SceneInspectorSlider materialOffsetV_;
        wi::gui::Label materialTexturesLabel_;
        std::array<SceneInspectorButton, 5> materialTextureAssign_;
        std::array<SceneInspectorButton, 5> materialTextureClear_;
        wi::gui::Label materialShaderSpecificLabel_;
        SceneInspectorSlider materialPomStrength_;
        SceneInspectorSlider materialAnisotropyStrength_;
        SceneInspectorSlider materialAnisotropyRotation_;
        SceneInspectorSlider materialSheenRed_;
        SceneInspectorSlider materialSheenGreen_;
        SceneInspectorSlider materialSheenBlue_;
        SceneInspectorSlider materialSheenRoughness_;
        SceneInspectorSlider materialClearcoat_;
        SceneInspectorSlider materialClearcoatRoughness_;
        SceneInspectorSlider materialTransmission_;
        SceneInspectorSlider materialRefraction_;
        SceneInspectorSlider materialTerrainBlendHeight_;
        SceneInspectorSlider materialMeshBlend_;
        SceneInspectorSlider materialInteriorScaleX_;
        SceneInspectorSlider materialInteriorScaleY_;
        SceneInspectorSlider materialInteriorScaleZ_;
        SceneInspectorSlider materialInteriorOffsetX_;
        SceneInspectorSlider materialInteriorOffsetY_;
        SceneInspectorSlider materialInteriorOffsetZ_;
        SceneInspectorSlider materialInteriorRotation_;
        wi::gui::Label cameraLabel_;
        SceneInspectorComboBox cameraProjection_;
        SceneInspectorSlider cameraFieldOfView_;
        SceneInspectorSlider cameraNearPlane_;
        SceneInspectorSlider cameraFarPlane_;
        SceneInspectorSlider cameraFocalLength_;
        SceneInspectorSlider cameraApertureSize_;
        SceneInspectorSlider cameraOrthoVerticalSize_;
        SceneInspectorButton cameraAlignToView_;
        SceneInspectorButton cameraViewFrom_;
        wi::gui::Label decalLabel_;
        SceneInspectorCheckBox decalBaseColorOnlyAlpha_;
        SceneInspectorSlider decalSlopeBlend_;
        wi::gui::Label decalMaterialLabel_;
        SceneInspectorSlider decalBaseColorRed_;
        SceneInspectorSlider decalBaseColorGreen_;
        SceneInspectorSlider decalBaseColorBlue_;
        SceneInspectorSlider decalOpacity_;
        SceneInspectorButton decalBaseColorTexture_;
        wi::gui::Label environmentProbeLabel_;
        SceneInspectorComboBox environmentProbeResolution_;
        SceneInspectorCheckBox environmentProbeRealtime_;
        SceneInspectorSlider environmentProbeInterval_;
        SceneInspectorCheckBox environmentProbeMsaa_;
        SceneInspectorSlider environmentProbeViewDistance_;
        SceneInspectorButton environmentProbeRefresh_;
        wi::gui::Label lightLabel_;
        SceneInspectorComboBox lightType_;
        SceneInspectorSlider lightColorRed_;
        SceneInspectorSlider lightColorGreen_;
        SceneInspectorSlider lightColorBlue_;
        SceneInspectorSlider lightIntensity_;
        SceneInspectorSlider lightRange_;
        SceneInspectorSlider lightOuterCone_;
        SceneInspectorSlider lightInnerCone_;
        SceneInspectorSlider lightRadius_;
        SceneInspectorSlider lightLength_;
        SceneInspectorSlider lightHeight_;
        SceneInspectorCheckBox lightCastShadow_;
        SceneInspectorCheckBox lightVolumetrics_;
        SceneInspectorSlider lightVolumetricBoost_;
        wi::gui::Label environmentSkyLabel_;
        SceneInspectorComboBox environmentPreset_;
        SceneInspectorComboBox skyMode_;
        SceneInspectorCheckBox aerialPerspective_;
        SceneInspectorSlider skyExposure_;
        SceneInspectorSlider stars_;
        SceneInspectorSlider ambientIntensity_;
        wi::gui::Label environmentFogLabel_;
        SceneInspectorSlider fogStart_;
        SceneInspectorSlider fogDensity_;
        SceneInspectorCheckBox heightFog_;
        SceneInspectorSlider fogHeightStart_;
        SceneInspectorSlider fogHeightEnd_;
        wi::gui::Label environmentCloudLabel_;
        SceneInspectorSlider cloudCoverage_;
        SceneInspectorSlider cloudStartHeight_;
        SceneInspectorSlider cloudThickness_;
        SceneInspectorCheckBox cloudsCastShadow_;
        wi::gui::Label precipitationLabel_;
        SceneInspectorComboBox precipitationMode_;
        SceneInspectorSlider precipitationIntensity_;
        SceneInspectorSlider precipitationFallSpeed_;
        SceneInspectorSlider precipitationParticleScale_;
        SceneInspectorSlider precipitationWindAzimuth_;
        SceneInspectorSlider precipitationWindSpeed_;
        SceneInspectorSlider precipitationTurbulence_;
        wi::gui::Label sunLabel_;
        SceneInspectorComboBox sunPreset_;
        SceneInspectorSlider sunTime_;
        SceneInspectorSlider sunAzimuth_;
        SceneInspectorSlider sunElevation_;
        SceneInspectorSlider sunPreviewSpeed_;
        SceneInspectorButton sunPlayButton_;
        SceneInspectorButton sunPauseButton_;
        wi::gui::Label oceanLabel_;
        SceneInspectorCheckBox oceanEnabled_;
        SceneInspectorComboBox oceanPreset_;
        SceneInspectorComboBox oceanResolution_;
        SceneInspectorSlider oceanWaterHeight_;
        SceneInspectorSlider oceanPatchLength_;
        SceneInspectorSlider oceanWaveAmplitude_;
        SceneInspectorSlider oceanChoppyScale_;
        SceneInspectorSlider oceanTimeScale_;
        SceneInspectorSlider oceanWindAzimuth_;
        SceneInspectorSlider oceanWindSpeed_;
        SceneInspectorSlider oceanWindDependency_;
        SceneInspectorSlider oceanSurfaceDetail_;
        SceneInspectorSlider oceanDisplacementTolerance_;
        SceneInspectorSlider oceanWaterRed_;
        SceneInspectorSlider oceanWaterGreen_;
        SceneInspectorSlider oceanWaterBlue_;
        SceneInspectorSlider oceanWaterOpacity_;
        SceneInspectorSlider oceanExtinctionRed_;
        SceneInspectorSlider oceanExtinctionGreen_;
        SceneInspectorSlider oceanExtinctionBlue_;
        wi::gui::Label terrainLabel_;
        SceneInspectorButton createTerrainButton_;
        wi::gui::Label terrainSizeReadout_;
        SceneInspectorButton expandTerrainButton_;
        SceneInspectorSlider terrainChunkScale_;
        SceneInspectorSlider terrainMinimumHeight_;
        SceneInspectorSlider terrainMaximumHeight_;
        SceneInspectorSlider terrainLowAltitudeBlend_;
        SceneInspectorSlider terrainBaseBlend_;
        SceneInspectorSlider terrainSlopeBlend_;
        SceneInspectorSlider terrainLodBias_;
        wi::gui::Label terrainMaterialLabel_;
        SceneInspectorComboBox terrainMaterialPreset_;
        SceneInspectorSlider terrainTextureScale_;
        SceneInspectorButton terrainApplyDefaultGrassButton_;
        SceneInspectorButton terrainReloadMaterialButton_;
        wi::gui::Label terrainSculptLabel_;
        SceneInspectorComboBox terrainSculptMode_;
        SceneInspectorSlider terrainBrushRadius_;
        SceneInspectorSlider terrainBrushStrength_;
        SceneInspectorSlider terrainBrushFalloff_;
        wi::gui::Label terrainBrushReadout_;
        wi::gui::Label terrainStrokeDiagnostic_;
        wi::gui::Window renderWorkspacePanel_;
        wi::gui::Label renderWorkspaceTitle_;
        wi::gui::Label renderImageLabel_;
        SceneInspectorComboBox renderTonemap_;
        SceneInspectorSlider renderExposure_;
        SceneInspectorSlider renderBrightness_;
        SceneInspectorSlider renderContrast_;
        SceneInspectorSlider renderSaturation_;
        SceneInspectorSlider renderHdrCalibration_;
        wi::gui::Label renderColorGradingLabel_;
        SceneInspectorCheckBox renderColorGradingEnabled_;
        SceneInspectorComboBox renderLutLibrary_;
        SceneInspectorButton renderLutImport_;
        SceneInspectorButton renderLutClear_;
        std::vector<bridge::ColorGradingLutEntry> renderLutChoices_;
        wi::gui::Label renderBloomLabel_;
        SceneInspectorCheckBox renderBloomEnabled_;
        SceneInspectorSlider renderBloomThreshold_;
        SceneInspectorCheckBox renderEyeAdaptationEnabled_;
        SceneInspectorSlider renderEyeAdaptationKey_;
        SceneInspectorSlider renderEyeAdaptationRate_;
        wi::gui::Label renderAntiAliasingLabel_;
        SceneInspectorComboBox renderAntiAliasing_;
        wi::gui::Label renderPostFxLabel_;
        SceneInspectorCheckBox renderDepthOfFieldEnabled_;
        SceneInspectorSlider renderDepthOfFieldStrength_;
        SceneInspectorCheckBox renderMotionBlurEnabled_;
        SceneInspectorSlider renderMotionBlurStrength_;
        SceneInspectorCheckBox renderSharpenEnabled_;
        SceneInspectorSlider renderSharpenAmount_;
        SceneInspectorCheckBox renderChromaticAberrationEnabled_;
        SceneInspectorSlider renderChromaticAberrationAmount_;
        SceneInspectorCheckBox renderDitherEnabled_;
        wi::gui::Label renderAmbientOcclusionLabel_;
        SceneInspectorComboBox renderAmbientOcclusion_;
        SceneInspectorSlider renderAmbientOcclusionPower_;
        SceneInspectorSlider renderAmbientOcclusionRange_;
        SceneInspectorSlider renderAmbientOcclusionSampleCount_;
        wi::gui::Label renderGlobalIlluminationLabel_;
        SceneInspectorCheckBox renderSsgiEnabled_;
        SceneInspectorSlider renderSsgiDepthRejection_;
        SceneInspectorSlider renderGiBoost_;
        wi::gui::Label renderReflectionsLabel_;
        SceneInspectorCheckBox renderPlanarReflectionsEnabled_;
        SceneInspectorSlider renderPlanarReflectionResolutionScale_;
        SceneInspectorComboBox renderPlanarReflectionMsaa_;
        SceneInspectorCheckBox renderSsrEnabled_;
        SceneInspectorComboBox renderSsrQuality_;
        SceneInspectorSlider renderReflectionRoughnessCutoff_;
        wi::gui::Label renderRayTracingLabel_;
        wi::gui::Label renderRayTracingCapability_;
        SceneInspectorCheckBox renderRaytracedShadowsEnabled_;
        SceneInspectorCheckBox renderRaytracedReflectionsEnabled_;
        SceneInspectorSlider renderRaytracedReflectionsRange_;
        SceneInspectorComboBox renderRaytracedReflectionsQuality_;
        SceneInspectorCheckBox renderRaytracedDiffuseEnabled_;
        SceneInspectorSlider renderRaytracedDiffuseRange_;
        SceneInspectorComboBox renderRaytracedDiffuseQuality_;
        SceneInspectorCheckBox renderSurfelGiEnabled_;
        wi::gui::Label renderPathTraceLabel_;
        wi::gui::Label renderPathTraceStatus_;
        SceneInspectorButton renderPathTraceToggle_;
        SceneInspectorSlider renderPathTraceSamples_;
        SceneInspectorSlider renderPathTraceBounces_;
        SceneInspectorButton renderPathTraceRestart_;
        wi::gui::Label renderLightmapBakeLabel_;
        wi::gui::Label renderLightmapBakeStatus_;
        SceneInspectorComboBox renderLightmapResolution_;
        SceneInspectorComboBox renderLightmapUvSource_;
        SceneInspectorCheckBox renderLightmapBlockCompression_;
        SceneInspectorButton renderLightmapStart_;
        SceneInspectorButton renderLightmapStop_;
        SceneInspectorButton renderLightmapClear_;
        SceneInspectorButton renderLightmapPreview_;
        wi::gui::Label renderVertexAoBakeLabel_;
        wi::gui::Label renderVertexAoBakeStatus_;
        SceneInspectorSlider renderVertexAoRayCount_;
        SceneInspectorSlider renderVertexAoRayLength_;
        SceneInspectorButton renderVertexAoCompute_;
        SceneInspectorButton renderVertexAoClear_;
        bridge::LightmapBakeSettings lightmapBakeSettings_;
        bridge::LightmapBakeSession lightmapBakeSession_;
        bridge::VertexAoBakeSettings vertexAoBakeSettings_;
        std::string renderBakeLastMessage_;
        SceneInspectorButton focusButton_;
        SceneInspectorButton duplicateButton_;
        SceneInspectorButton deleteButton_;
        SceneInspectorButton undoButton_;
        SceneInspectorButton redoButton_;
        SceneInspectorButton saveButton_;
        SceneInspectorButton saveAsButton_;
        SceneInspectorButton reopenButton_;
        wi::gui::Window contentPanel_;
        wi::gui::Label contentLabel_;
        wi::gui::Label contentPlaceholder_;
        bridge::AssetBrowserService assetBrowserService_;
        std::string assetBrowserCurrentFolder_ = "Content";
        wi::gui::Window projectHubPanel_;
        RenegadeProjectHub projectHubChrome_;
        RenegadeProjectLoadingOverlay projectLoadingOverlay_;
        wi::gui::Label hubBrandLabel_;
        wi::gui::Label hubTitleLabel_;
        wi::gui::Label hubSubtitleLabel_;
        wi::gui::TextInputField projectNameInput_;
        wi::gui::Button createProjectButton_;
        wi::gui::Button openProjectButton_;
        wi::gui::Button openSceneButton_;
        wi::gui::Label recentProjectsLabel_;
        std::array<
            wi::gui::Button,
            bridge::ProjectService::MaximumRecentProjects> recentProjectButtons_;
        wi::gui::Label selectedProjectLabel_;
        wi::gui::Button launchProjectButton_;
        wi::gui::Button continueProjectButton_;
        wi::gui::Label hubMessageLabel_;
        RenegadeTextInputField hubNewProjectNameInput_;
        RenegadeButton hubNewProjectConfirmButton_;
        RenegadeButton hubNewProjectCancelButton_;
        wi::gui::Button gridToggleButton_;
        CreatorImportPreviewWindow importScalePanel_;
        wi::gui::Label importScaleTitleLabel_;
        wi::gui::Label importScaleReadoutLabel_;
        RenegadeComboBox importScaleModeCombo_;
        RenegadeButton importScaleApplyButton_;
        RenegadeButton importScaleDismissButton_;
        RenegadePhysicsLabStudioChrome studioChrome_;
        TestLevelRuntimeProcess testLevelRuntime_;
        bool projectPreviewActive_ = false;
        Translator gizmo_;
        wi::graphics::Shader gridVertexShader_;
        wi::graphics::Shader gridPixelShader_;
        wi::graphics::PipelineState gridPipeline_;
        wi::graphics::Texture selectionOutlineMask_;
        wi::graphics::Texture selectionOutlineMaskMsaa_;
        wi::scene::TransformComponent editorCameraTransform_;
        wi::ecs::Entity gizmoEntity_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity outlinedSelection_ = wi::ecs::INVALID_ENTITY;
        std::vector<wi::ecs::Entity> outlinedEntities_;
        std::vector<std::uint8_t> outlinedEntityPreviousStencils_;
        bridge::TransformState gizmoTransformBefore_;
        XMFLOAT4 viewportBounds_ = {};
        XMFLOAT4 cameraPointerAnchor_ = {};
        float cameraMoveSpeed_ = 5.0f;
        bool gizmoDragActive_ = false;
        bool flyCameraActive_ = false;
        bool gizmoSuppressedForCameraView_ = false;
        bool gridVisible_ = true;
        int lastDrawerTab_ = 0;
        bool workspaceLayoutDirty_ = false;
        bool inspectorRefreshPending_ = false;
        bool weatherSliderActive_ = false;
        WeatherField weatherSliderField_ = WeatherField::SkyExposure;
        wi::ecs::Entity weatherSliderEntity_ = wi::ecs::INVALID_ENTITY;
        bridge::WeatherState weatherSliderBefore_;
        bridge::WeatherState weatherSliderAfter_;
        bool environmentWorkspaceActive_ = false;
        bool terrainWorkspaceActive_ = false;
        bool renderWorkspaceActive_ = false;
        bool renderSliderActive_ = false;
        bool renderSliderHadCarrierBefore_ = false;
        RenderField renderSliderField_ = RenderField::Exposure;
        bridge::RenderSettingsState renderSliderBefore_;
        bridge::RenderSettingsState renderSliderAfter_;
        bridge::RenderSettingsState appliedRenderSettings_;
        bool appliedRenderSettingsInitialized_ = false;
        std::uint64_t appliedRenderSettingsSceneRevision_ = 0;
        wi::ecs::Entity wd01SynchronizedTerrainEntity_ =
            wi::ecs::INVALID_ENTITY;
        std::size_t wd01SynchronizedChunkCount_ = 0;
        bool pathTracePreviewActive_ = false;
        int pathTraceTargetSamples_ = 1024;
        std::uint32_t pathTraceBounceCount_ = 1;
        std::uint32_t pathTracePreviousBounceCount_ = 1;
        bool precipitationSliderActive_ = false;
        PrecipitationField precipitationSliderField_ =
            PrecipitationField::Intensity;
        wi::ecs::Entity precipitationSliderEntity_ =
            wi::ecs::INVALID_ENTITY;
        bridge::PrecipitationState precipitationSliderBefore_;
        bridge::PrecipitationState precipitationSliderAfter_;
        bool sunSliderActive_ = false;
        SunField sunSliderField_ = SunField::Time;
        bridge::SunState sunSliderBefore_;
        bridge::SunState sunSliderAfter_;
        bool sunPreviewPlaying_ = false;
        float sunPreviewSpeedHoursPerSecond_ = 0.100f;
        bridge::SunState sunPreviewBefore_;
        bridge::SunState sunPreviewCurrent_;
        bool oceanSliderActive_ = false;
        OceanField oceanSliderField_ = OceanField::WaterHeight;
        wi::ecs::Entity oceanSliderEntity_ = wi::ecs::INVALID_ENTITY;
        bridge::OceanState oceanSliderBefore_;
        bridge::OceanState oceanSliderAfter_;
        bool pendingOceanEnabled_ = false;
        int pendingOceanResolution_ = 512;
        bridge::OceanPreset pendingOceanPreset_ = bridge::OceanPreset::Calm;
        bool terrainSliderActive_ = false;
        TerrainField terrainSliderField_ = TerrainField::ChunkScale;
        wi::ecs::Entity terrainSliderEntity_ = wi::ecs::INVALID_ENTITY;
        bridge::TerrainState terrainSliderBefore_;
        bridge::TerrainState terrainSliderAfter_;
        bool terrainTextureScaleActive_ = false;
        wi::ecs::Entity terrainMaterialEntity_ = wi::ecs::INVALID_ENTITY;
        bridge::TerrainMaterialState terrainMaterialBefore_;
        bridge::TerrainMaterialState terrainMaterialAfter_;
        bridge::TerrainMaterialPreset pendingTerrainMaterialPreset_ =
            bridge::TerrainMaterialPreset::Meadow;
        bridge::TerrainSculptMode terrainSculptModeValue_ =
            bridge::TerrainSculptMode::Raise;
        float terrainBrushRadiusValue_ = 12.0f;
        float terrainBrushStrengthValue_ = 1.0f;
        float terrainBrushFalloffValue_ = 0.55f;
        float terrainFlattenHeight_ = 0.0f;
        bool terrainStrokeActive_ = false;
        bool terrainStrokeChanged_ = false;
        wi::ecs::Entity terrainStrokeEntity_ = wi::ecs::INVALID_ENTITY;
        bridge::TerrainSculptState terrainStrokeBefore_;
        bool lightSliderActive_ = false;
        LightField lightSliderField_ = LightField::Intensity;
        wi::ecs::Entity lightSliderEntity_ = wi::ecs::INVALID_ENTITY;
        bridge::LightState lightSliderBefore_;
        bridge::LightState lightSliderAfter_;
        bool cameraSliderActive_ = false;
        CameraField cameraSliderField_ = CameraField::FieldOfView;
        wi::ecs::Entity cameraSliderEntity_ = wi::ecs::INVALID_ENTITY;
        bridge::CameraState cameraSliderBefore_;
        bridge::CameraState cameraSliderAfter_;
        bool materialInspectorVisible_ = false;
        float materialInspectorBottom_ = 630.0f;
        std::size_t materialInspectorMaterialCount_ = 0;
        wi::ecs::Entity materialInspectorEntity_ = wi::ecs::INVALID_ENTITY;
        wi::scene::MaterialComponent::SHADERTYPE materialInspectorShaderType_ =
            wi::scene::MaterialComponent::SHADERTYPE_PBR;
        bool materialSliderActive_ = false;
        MaterialField materialSliderField_ = MaterialField::Roughness;
        wi::ecs::Entity materialSliderEntity_ = wi::ecs::INVALID_ENTITY;
        bridge::MaterialState materialSliderBefore_;
        bridge::MaterialState materialSliderAfter_;
        wi::scene::LightComponent::LightType pendingLightType_ =
            wi::scene::LightComponent::POINT;
        wi::scene::LightComponent::LightType lightPlacementType_ =
            wi::scene::LightComponent::POINT;
        bool lightPlacementActive_ = false;
        bool creatorAssetPlacementActive_ = false;
        bridge::StableId creatorAssetPlacementId_;
        std::string creatorAssetPlacementLabel_;
        XMFLOAT2 creatorAssetDropPoint_ = {};
        bool creatorAssetDropPending_ = false;
        bool projectHubVisible_ = true;
        bool hubNewProjectMode_ = false;
        std::function<void()> exitRequestHandler_;
        int selectedRecentProject_ = -1;
        EditorAction pendingAction_ = EditorAction::None;
        bool windowsGameBuildPreparationActive_ = false;
        bool windowsGameBuildRequested_ = false;
        wi::jobsystem::context sceneOpenWorkload_;
        wi::jobsystem::context projectLoadWorkload_;
        wi::jobsystem::context modelImportWorkload_;
        wi::jobsystem::context decalTextureImportWorkload_;
        wi::jobsystem::context materialTextureImportWorkload_;
        std::string openingScenePath_;
        bool sceneOpenInProgress_ = false;
        wi::ecs::Entity importScaleTargetEntity_ = wi::ecs::INVALID_ENTITY;
        float importScaleAppliedFactor_ = 1.0f;
        bridge::ModelScaleMode pendingImportScaleMode_ =
            bridge::ModelScaleMode::Original;
    };

    // The Physics Lab is a GUI workspace over the authoritative Scene render
    // path. When it owns the viewport, bypass only Studio's post-3D editor
    // overlays (selection outline and transform gizmo). The 3D scene itself,
    // shared hierarchy/selection state and Wicked GUI continue to run normally.
    class PhysicsLabStudioRenderPath final : public StudioRenderPath
    {
    public:
        void Update(float dt) override
        {
            StudioRenderPath::Update(dt);
            SyncAudioInspectorPresentation();
        }

        void Render() const override
        {
            // Test Level owns the foreground Runtime. Its resource handoff
            // takes precedence over specialist Studio viewport routing.
            if (IsTestLevelRuntimeActive())
            {
                StudioRenderPath::Render();
                return;
            }
            if (IsPhysicsLabActive())
            {
                wi::RenderPath3D::Render();
                return;
            }
            StudioRenderPath::Render();
        }

        void Compose(wi::graphics::CommandList cmd) const override
        {
            if (IsTestLevelRuntimeActive())
            {
                StudioRenderPath::Compose(cmd);
                return;
            }
            if (IsPhysicsLabActive())
            {
                wi::RenderPath3D::Compose(cmd);
                return;
            }
            StudioRenderPath::Compose(cmd);
        }
    };

    class StudioApplication final : public wi::Application
    {
    public:
        void SetStartupScene(std::string filePath);
        void SetExitRequestHandler(std::function<void()> handler);
        void RequestExit();
        void Initialize() override;

        // Workspace activation is resolved after Wicked completes the current
        // frame. ActivatePath() then owns the following frame, so the inactive
        // Level Editor cannot continue ticking/rendering behind Story Flow.
        void Run()
        {
            wi::Application::Run();
            storyFlowIntegration_.Tick(
                *this,
                renderer_,
                storyFlowRenderer_,
                screenEditorRenderer_,
                session_);
        }

    private:
        void PrepareProvingGround();

        bridge::StudioSession session_;
        PhysicsLabStudioRenderPath renderer_;
        RenegadeStoryFlowRenderPath storyFlowRenderer_;
        RenegadeScreenEditorRenderPath screenEditorRenderer_;
        StoryFlowStudioIntegration storyFlowIntegration_;
        std::string startupScene_ = "Content/ProvingGround.wiscene";
    };
}
