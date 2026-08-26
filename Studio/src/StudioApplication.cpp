Warning: truncated output (original token count: 104308)
Total output lines: 10314

#include "StudioApplication.h"
#include "StudioUserPreferences.h"

#include "renegade/bridge/TestLevelSnapshotService.h"
#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "renegade/bridge/CreatorModelMaterialPreparationService.h"
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
            XMConvertToRadians(creatorModelImporter.rotationDegrees.y),
            XMConvertToRadians(creatorModelImporter.rotationDegrees.z));
        XMStoreFloat4(&transform->rotation_local, q);
        transform->scale_local = creatorModelImporter.scale;
        transform->SetDirty();
        transform->UpdateTransform();
        UpdateCreatorImportScaleReferenceLabel();
    }

    void ApplyCreatorImportPreviewLighting()
    {
        auto* session = renegade::bridge::StudioSession::Current();
        if (session == nullptr || !creatorModelImporter.active)
            return;
        auto& scene = session->Scenes().GetScene();
        if (auto* light = scene.lights.GetComponent(creatorModelImporter.previewLight))
        {
            light->intensity = creatorModelImporter.lightIntensity;
        }
        if (auto* transform = scene.transforms.GetComponent(creatorModelImporter.previewLight))
        {
            const XMVECTOR rotation = XMQuaternionRotationRollPitchYaw(
                XMConvertToRadians(-creatorModelImporter.lightElevation),
                XMConvertToRadians(creatorModelImporter.lightAzimuth),
                0.0f);
            XMStoreFloat4(&transform->rotation_local, rotation);
            transform->SetDirty();
            transform->UpdateTransform();
        }
        const XMFLOAT3 ambient(
            creatorModelImporter.ambientBrightness,
            creatorModelImporter.ambientBrightness,
            creatorModelImporter.ambientBrightness);
        scene.weather.ambient = ambient;
        if (auto* weather = scene.weathers.GetComponent(creatorModelImporter.weatherEntity))
            weather->ambient = ambient;
    }

    void RestoreCreatorImportPreviewEnvironment()
    {
        auto* session = renegade::bridge::StudioSession::Current();
        if (session == nullptr || !creatorModelImporter.ambientCaptured)
            return;
        auto& scene = session->Scenes().GetScene();
        scene.weather.ambient = creatorModelImporter.ambientBefore;
        if (auto* weather = scene.weathers.GetComponent(creatorModelImporter.weatherEntity))
            weather->ambient = creatorModelImporter.ambientBefore;
    }

    renegade::bridge::CreatorMaterialSourceOverride& EnsureCreatorMaterialOverride()
    {
        const std::uint32_t materialIndex = static_cast<std::uint32_t>(
            creatorModelImporter.selectedMaterial);
        auto found = std::find_if(
            creatorModelImporter.materialOverrides.begin(),
            creatorModelImporter.materialOverrides.end(),
            [materialIndex](const renegade::bridge::CreatorMaterialSourceOverride& value)
            { return value.materialIndex == materialIndex; });
        if (found == creatorModelImporter.materialOverrides.end())
        {
            renegade::bridge::CreatorMaterialSourceOverride created;
            created.materialIndex = materialIndex;
            creatorModelImporter.materialOverrides.push_back(std::move(created));
            return creatorModelImporter.materialOverrides.back();
        }
        return *found;
    }

    renegade::bridge::CreatorTextureSourceChoice& SelectedCreatorTextureChoice()
    {
        auto& material = EnsureCreatorMaterialOverride();
        switch (creatorImportTextureSlot)
        {
        case 0: return material.baseColor;
        case 1: return material.normal;
        case 2: return material.surface;
        case 3: return material.roughness;
        case 4: return material.metalness;
        case 5: return material.occlusion;
        default: return material.emissive;
        }
    }

    void ApplyDetectedCreatorPreviewMaterials()
    {
        auto* session = renegade::bridge::StudioSession::Current();
        if (session == nullptr || !session->Projects().HasProject())
            return;
        auto& scene = session->Scenes().GetScene();
        std::string error;
        const fs::path output = fs::u8path(
            session->Projects().CurrentProject().rootPath) /
            "Intermediate" / "PreviewMaterials";
        if (!renegade::bridge::ApplyCreatorModelMaterialPreview(
                scene,
                creatorModelImporter.sourcePath,
                output.generic_u8string(),
                creatorModelImporter.materialOverrides,
                creatorModelImporter.materialEntities,
                error))
        {
            creatorImportTextureHelp.SetText(
                "TEXTURE PREVIEW ERROR // " + error);
            wi::backlog::post(
                "Renegade creator material preview: " + error,
                wi::backlog::LogLevel::Warning);
            return;
        }
        creatorImportTextureHelp.SetText(
            "TEXTURE SLOT // AUTO-DETECT, REPLACE OR REMOVE");
    }

    void PreviewCreatorMaterialScalar(
        float renegade::bridge::CreatorMaterialSourceOverride::* member,
        const float value)
    {
        auto* session = renegade::bridge::StudioSession::Current();
        if (session == nullptr || creatorModelImporter.materialEntities.empty())
            return;
        auto& scene = session->Scenes().GetScene();
        const auto entity = creatorModelImporter.materialEntities[
            std::min(
                creatorModelImporter.selectedMaterial,
                creatorModelImporter.materialEntities.size() - 1)];
        auto* material = scene.materials.GetComponent(entity);
        if (material == nullptr)
            return;

        using Override = renegade::bridge::CreatorMaterialSourceOverride;
        if (member == &Override::roughnessValue)
            material->SetRoughness(value);
        else if (member == &Override::metalnessValue)
            material->SetMetalness(value);
        else if (member == &Override::reflectanceValue)
            material->SetReflectance(value);
        else if (member == &Override::normalStrengthValue)
            material->SetNormalMapStrength(value);
        else if (member == &Override::emissiveStrengthValue)
            material->SetEmissiveStrength(value);

        // AO strength changes packed Surface pixels, so its override is saved
        // here and applied by the authoritative import preparation. Repacking
        // and reloading PNG resources on the Studio render thread previously
        // stalled it for seconds and made all PBR controls unusable.
    }

    void RefreshCreatorImportTextureEditor()
    {
        creatorImportTexturePreviews.SetSelectedSlot(creatorImportTextureSlot);
        if (creatorModelImporter.materialEntities.empty())
        {
            creatorImportTexturePath.SetValue("");
            creatorImportTexturePath.SetTooltip("No texture source selected");
            return;
        }
        const std::uint32_t materialIndex = static_cast<std::uint32_t>(
            creatorModelImporter.selectedMaterial);
        const auto found = std::find_if(
            creatorModelImporter.materialOverrides.begin(),
            creatorModelImporter.materialOverrides.end(),
            [materialIndex](const renegade::bridge::CreatorMaterialSourceOverride& value)
            { return value.materialIndex == materialIndex; });
        if (found == creatorModelImporter.materialOverrides.end())
        {
            creatorImportTexturePath.SetValue("<AUTO // imported binding or filename suffix>");
            creatorImportTexturePath.SetTooltip(
                "Automatic: use the imported binding or a detected filename suffix");
            return;
        }
        const renegade::bridge::CreatorTextureSourceChoice* choice = nullptr;
        switch (creatorImportTextureSlot)
        {
        case 0: choice = &found->baseColor; break;
        case 1: choice = &found->normal; break;
        case 2: choice = &found->surface; break;
        case 3: choice = &found->roughness; break;
        case 4: choice = &found->metalness; break;
        case 5: choice = &found->occlusion; break;
        default: choice = &found->emissive; break;
        }
        if (!choice->overridden)
        {
            creatorImportTexturePath.SetValue("<AUTO // imported binding or filename suffix>");
            creatorImportTexturePath.SetTooltip(
                "Automatic: use the imported binding or a detected filename suffix");
        }
        else if (choice->path.empty())
        {
            creatorImportTexturePath.SetValue("<REMOVED>");
            creatorImportTexturePath.SetTooltip("Texture slot explicitly removed");
        }
        else
        {
            creatorImportTexturePath.SetValue(choice->path);
            creatorImportTexturePath.SetTooltip(choice->path);
        }
    }

    void ApplyCreatorPreviewTextureChoice(const std::string& path)
    {
        auto* session = renegade::bridge::StudioSession::Current();
        if (session == nullptr || creatorModelImporter.materialEntities.empty())
            return;
        (void)path;
        ApplyDetectedCreatorPreviewMaterials();
    }

    void OpenCreatorImportTextureBrowser()
    {
        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::OPEN;
        params.description = "Texture source for the selected imported material slot";
        for (const char* extension : {"png", "jpg", "jpeg", "tga", "bmp", "dds", "hdr"})
            params.extensions.push_back(extension);
        wi::helper::FileDialog(params, [](const std::string& path)
        {
            wi::eventhandler::Subscribe_Once(
                wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                [path](std::uint64_t)
                {
                    if (path.empty() || !creatorModelImporter.active)
                        return;
                    auto& choice = SelectedCreatorTextureChoice();
                    choice.overridden = true;
                    choice.path = path;
                    ApplyCreatorPreviewTextureChoice(path);
                    RefreshCreatorImportTextureEditor();
                    RefreshCreatorImportMaterialReadout();
                });
        });
    }

    void RefreshCreatorImportAnimationEditor()
    {
        if (creatorModelImporter.animationRecipe.empty())
        {
            creatorImportAnimationName.SetValue("");
            creatorImportAnimationStart.SetValue(0.0f);
            creatorImportAnimationEnd.SetValue(0.0f);
            creatorImportAnimationEnabled.SetSelectedWithoutCallback(-1);
            creatorImportAnimationReadout.SetText("No animation actions detected.");
            return;
        }
        creatorModelImporter.selectedAnimation = std::min(
            creatorModelImporter.selectedAnimation,
            creatorModelImporter.animationRecipe.size() - 1);
        const auto& clip = creatorModelImporter.animationRecipe[
            creatorModelImporter.selectedAnimation];
        creatorImportAnimationName.SetValue(clip.name);
        creatorImportAnimationStart.SetValue(clip.start);
        creatorImportAnimationEnd.SetValue(clip.end);
        creatorImportAnimationEnabled.SetSelectedWithoutCallback(clip.enabled ? 0 : 1);
        std::ostringstream out;
        out.precision(3);
        out << std::fixed << "Source action " << clip.sourceAnimationIndex + 1
            << " // " << clip.start << " - " << clip.end
            << " // " << (clip.enabled ? "INCLUDED" : "EXCLUDED");
        creatorImportAnimationReadout.SetText(out.str());
    }

    void RebuildCreatorImportAnimationCombo()
    {
        creatorImportAnimationCombo.ClearItems();
        for (std::size_t index = 0; index < creatorModelImporter.animationRecipe.size(); ++index)
        {
            const auto& clip = creatorModelImporter.animationRecipe[index];
            creatorImportAnimationCombo.AddItem(
                clip.name.empty() ? "Animation " + std::to_string(index + 1) : clip.name,
                static_cast<std::uint64_t>(index));
        }
        if (!creatorModelImporter.animationRecipe.empty())
        {
            creatorModelImporter.selectedAnimation = std::min(
                creatorModelImporter.selectedAnimation,
                creatorModelImporter.animationRecipe.size() - 1);
            creatorImportAnimationCombo.SetSelectedWithoutCallback(
                static_cast<int>(creatorModelImporter.selectedAnimation));
        }
        RefreshCreatorImportAnimationEditor();
    }

    void RefreshCreatorImportMaterialReadout()
    {
        auto* session = renegade::bridge::StudioSession::Current();
        if (session == nullptr || creatorModelImporter.materialEntities.empty())
        {
            creatorImportMaterialReadout.SetText("No imported materials detected.");
            creatorImportTexturePreviews.ClearSlots();
            return;
        }
        creatorModelImporter.selectedMaterial = std::min(
            creatorModelImporter.selectedMaterial,
            creatorModelImporter.materialEntities.size() - 1);
        auto& scene = session->Scenes().GetScene();
        const auto entity = creatorModelImporter.materialEntities[
            creatorModelImporter.selectedMaterial];
        const auto* material = scene.materials.GetComponent(entity);
        if (material == nullptr)
        {
            creatorImportMaterialReadout.SetText("Selected material is unavailable.");
            creatorImportTexturePreviews.ClearSlots();
            return;
        }
        creatorImportTexturePreviews.ClearSlots();
        creatorImportTexturePreviews.SetSlot(
            0,
            material->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource,
            material->textures[wi::scene::MaterialComponent::BASECOLORMAP].name);
        creatorImportTexturePreviews.SetSlot(
            1,
            material->textures[wi::scene::MaterialComponent::NORMALMAP].resource,
            material->textures[wi::scene::MaterialComponent::NORMALMAP].name);
        creatorImportTexturePreviews.SetSlot(
            2,
            material->textures[wi::scene::MaterialComponent::SURFACEMAP].resource,
            material->textures[wi::scene::MaterialComponent::SURFACEMAP].name);
        creatorImportTexturePreviews.SetSlot(
            6,
            material->textures[wi::scene::MaterialComponent::EMISSIVEMAP].resource,
            material->textures[wi::scene::MaterialComponent::EMISSIVEMAP].name);

        const std::uint32_t materialIndex = static_cast<std::uint32_t>(
            creatorModelImporter.selectedMaterial);
        const auto source = std::find_if(
            creatorModelImporter.materialOverrides.begin(),
            creatorModelImporter.materialOverrides.end(),
            [materialIndex](const renegade::bridge::CreatorMaterialSourceOverride& value)
            { return value.materialIndex == materialIndex; });
        if (source != creatorModelImporter.materialOverrides.end())
        {
            const auto loadSource = [](const renegade::bridge::CreatorTextureSourceChoice& choice)
            {
                if (!choice.overridden || choice.path.empty())
                    return wi::Resource{};
                return wi::resourcemanager::Load(choice.path);
            };
            creatorImportTexturePreviews.SetSlot(
                3, loadSource(source->roughness), source->roughness.path);
            creatorImportTexturePreviews.SetSlot(
                4, loadSource(source->metalness), source->metalness.path);
            wi::Resource ao = loadSource(source->occlusion);
            std::string aoPath = source->occlusion.path;
            if (!ao.IsValid())
            {
                ao = material->textures[
                    wi::scene::MaterialComponent::OCCLUSIONMAP].resource;
                aoPath = material->textures[
                    wi::scene::MaterialComponent::OCCLUSIONMAP].name;
            }
            creatorImportTexturePreviews.SetSlot(5, ao, std::move(aoPath));
        }
        creatorImportTexturePreviews.SetSelectedSlot(creatorImportTextureSlot);

        std::ostringstream out;
        const auto usages = CreatorImportMaterialUsages(scene, entity);
        out << CreatorImportEntityName(
                scene,
                entity,
                "Material " + std::to_string(creatorModelImporter.selectedMaterial + 1));
        if (!usages.empty())
        {
            out << "\nUSED BY: " << usages.front();
            if (usages.size() > 1)
                out << "  (+" << (usages.size() - 1) << " MORE)";
        }
        out << "\nR " << material->roughness
            << "  M " << material->metalness
            << "  Refl " << material->reflectance;
        creatorImportMaterialReadout.SetText(out.str());
    }

    void RefreshCreatorImportMaterialScalars()
    {
        if (creatorModelImporter.materialOverrides.empty())
            return;
        auto& material = EnsureCreatorMaterialOverride();
        creatorImportRoughness.SetValue(material.roughnessValue);
        creatorImportMetalness.SetValue(material.metalnessValue);
        creatorImportReflectance.SetValue(material.reflectanceValue);
        creatorImportNormalStrength.SetValue(material.normalStrengthValue);
        creatorImportAoStrength.SetValue(material.aoStrengthValue);
        creatorImportEmissiveStrength.SetValue(material.emissiveStrengthValue);
    }

    void RefreshCreatorImportAnimationReadout()
    {
        auto* session = renegade::bridge::StudioSession::Current();
        if (session == nullptr || creatorModelImporter.animationEntities.empty())
        {
            creatorImportAnimationReadout.SetText("No animation actions detected.");
            return;
        }
        creatorModelImporter.selectedAnimation = std::min(
            creatorModelImporter.selectedAnimation,
            creatorModelImporter.animationEntities.size() - 1);
        auto& scene = session->Scenes().GetScene();
        const auto entity = creatorModelImporter.animationEntities[
            creatorModelImporter.selectedAnimation];
        const auto* animation = scene.animations.GetComponent(entity);
        if (animation == nullptr)
        {
            creatorImportAnimationReadout.SetText("Selected animation is unavailable.");
            return;
        }
        std::ostringstream out;
        out.precision(3);
        out << std::fixed
            << "Start: " << animation->start
            << "   End: " << animation->end
            << "   Duration: " << std::max(0.0f, animation->end - animation->start)
            << "\nChannels: " << animation->channels.size()
            << "   Samplers: " << animation->samplers.size();
        creatorImportAnimationReadout.SetText(out.str());
    }

}

namespace renegade::studio
{
    struct StudioRenderPath::ProjectLoadOperation
    {
        std::string descriptorPath;
        std::string startupScenePath;
        bridge::ProjectMetadata project;
        bridge::PreparedSceneOpen preparedScene;
        bridge::MaterialTextureRestoreResult textureRestore;
        bool storyFlowNative = false;
        std::string error;
    };

    void StudioRenderPath::BindSession(bridge::StudioSession& session) noexcept
    {
        session_ = &session;
        scene = &session.Scenes().GetScene();
    }

    void StudioRenderPath::SetExitRequestHandler(std::function<void()> handler)
    {
        exitRequestHandler_ = std::move(handler);
    }

    void StudioRenderPath::RequestExit()
    {
        if (projectLoadingOverlay_.IsBlocking() &&
            projectLoadingOverlay_.CurrentPhase() != RenegadeProjectLoadingOverlay::Phase::Failed)
        {
            return;
        }
        if (session_ == nullptr)
        {
            if (exitRequestHandler_)
                exitRequestHandler_();
            return;
        }

        RequestSceneReplacement(
            [this]()
            {
                if (exitRequestHandler_)
                    exitRequestHandler_();
            });
    }

    void StudioRenderPath::BindDiagnostics(
        wi::Application::InfoDisplayer& diagnostics) noexcept
    {
        diagnostics_ = &diagnostics;
    }

    void StudioRenderPath::Load()
    {
        setSSREnabled(false);
        setReflectionsEnabled(true);
        setFXAAEnabled(false);
        setBloomEnabled(true);
        setBloomThreshold(1.35f);

        // Ambient occlusion grounds the deck props against the terrain, and
        // volumetric lights are what make the scene's fog react to lighting
        // instead of reading as a flat screen-space overlay.
        setAO(AO::AO_MSAO);
        setAOPower(1.4f);
        setVolumeLightsEnabled(true);

        // Fixed exposure. Eye adaption would make the viewport brightness
        // depend on where the creator points the camera, which is not
        // acceptable for a colour-accurate authoring viewport.
        setEyeAdaptionEnabled(false);
        setExposure(1.0f);

        // Renegade draws its own grid. Wicked's stock helper is a fixed 20x20
        // unit line list whose adaptive path is gated behind gridHelper2D -
        // which rotates the grid into the vertical plane - and whose axis line
        // colours are hardcoded. It stays off permanently.
        wi::renderer::SetToDrawGridHelper(false);
        LoadGridResources();

        // The generated Proving Ground is composed around the world origin so
        // that it shares the grid helper's footprint.
        const XMVECTOR eye = XMVectorSet(13.0f, 7.6f, -16.5f, 1.0f);
        const XMVECTOR at = XMVectorSet(0.0f, 1.8f, 0.0f, 1.0f);
        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
        editorCameraTransform_.ClearTransform();
        editorCameraTransform_.MatrixTransform(
            XMMatrixInverse(nullptr, view));
        editorCameraTransform_.UpdateTransform();
        camera->TransformCamera(editorCameraTransform_);
        camera->UpdateCamera();

        CreateWorkspaceShell();
        CreateProjectHub();
        CreateImportScalePanel();
        ApplyRenegadeTheme();

        gizmo_.translate_snap = 0.1f;
        gizmo_.rotate_snap = 15.0f / 180.0f * XM_PI;
        gizmo_.scale_snap = 0.1f;

        // Translator sizes itself as distance-to-camera * 0.05 * tool_scale,
        // so tool_scale is a direct screen-space multiplier. The default of
        // 1.0 dominated the viewport. Thinner arms and slightly reduced
        // opacity match the restrained-glow direction; negative axes are
        // darkened hard so the gizmo reads as a projected instrument rather
        // than a solid object.
        gizmo_.tool_scale = 0.60f;
        gizmo_.tool_thickness = 0.70f;
        gizmo_.tool_opacity = 0.85f;
        gizmo_.tool_darken_negative_axes = 0.35f;

        SetTransformTool(TransformTool::Translate);

        // Restore the creator's saved grid preference. This is Renegade's
        // first persisted editor preference; camera speed and layout should
        // follow the same route rather than inventing a second one.
        if (session_ != nullptr)
        {
            gridVisible_ =
                session_->Projects().GetEditorPreference("grid_visible", true);
        }
        gridToggleButton_.SetText(gridVisible_ ? "GRID ON" : "GRID OFF");
        studioChrome_.SetGridVisible(gridVisible_);

        if (session_ != nullptr)
        {
            for (int index = 0; index < 4; ++index)
            {
                if (session_->Projects().GetEditorPreference(
                        "drawer_tab_" + std::to_string(index),
                        index == 0))
                {
                    lastDrawerTab_ = index;
                    break;
                }
            }
            const bool drawerOpen =
                session_->Projects().GetEditorPreference(
                    "drawer_open",
                    false);
            studioChrome_.SetActiveBottomTab(
                drawerOpen ? lastDrawerTab_ : -1);

            auto& projects = session_->Projects();
            if (projects.GetEditorPreference(
                    "workspace_layout_saved",
                    false))
            {
                studioChrome_.SetPanelSizes(
                    static_cast<float>(ReadLayoutPreference(
                        projects,
                        "hierarchy_width",
                        static_cast<int>(studioChrome_.HierarchyWidth()))),
                    static_cast<float>(ReadLayoutPreference(
                        projects,
                        "inspector_width",
                        static_cast<int>(studioChrome_.InspectorWidth()))),
                    static_cast<float>(ReadLayoutPreference(
                        projects,
                        "drawer_height",
                        static_cast<int>(studioChrome_.DrawerHeight()))));
            }
        }

        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        RefreshProjectHub();
        RefreshAssetBrowser();
        SetProjectHubVisible(true);

        RenderPath3D::Load();
    }

    void StudioRenderPath::LoadGridResources()
    {
        auto* device = wi::graphics::GetDevice();
        if (device == nullptr)
        {
            return;
        }

        // Renegade owns these shaders, so they are not in Wicked's shader dump
        // and must be compiled from source shipped beside the executable. This
        // is the same approach Wicked's own Example_ImGui sample uses: point
        // the shader source path at the working directory just long enough to
        // resolve them, then restore it so Wicked's own shaders are unaffected.
        const std::string previousSourcePath =
            wi::renderer::GetShaderSourcePath();
        wi::renderer::SetShaderSourcePath(
            wi::helper::GetCurrentPath() + "/Content/shaders/");

        const bool vertexLoaded = wi::renderer::LoadShader(
            wi::graphics::ShaderStage::VS,
            gridVertexShader_,
            "RenegadeGridVS.cso");
        const bool pixelLoaded = wi::renderer::LoadShader(
            wi::graphics::ShaderStage::PS,
            gridPixelShader_,
            "RenegadeGridPS.cso");

        wi::renderer::SetShaderSourcePath(previousSourcePath);

        if (!vertexLoaded || !pixelLoaded ||
            !gridVertexShader_.IsValid() || !gridPixelShader_.IsValid())
        {
            // A missing grid is a visual downgrade, not a failure worth
            // taking the editor down for. Everything downstream checks
            // gridPipeline_ before drawing.
            wi::backlog::post(
                "Renegade: the editor grid shaders could not be loaded. "
                "The viewport will render without a grid.",
                wi::backlog::LogLevel::Warning);
            return;
        }

        wi::graphics::PipelineStateDesc description;
        description.vs = &gridVertexShader_;
        description.ps = &gridPixelShader_;
        description.rs = wi::renderer::GetRasterizerState(
            wi::enums::RSTYPE_DOUBLESIDED);
        // Depth read with no write. The pixel shader writes SV_Depth from the
        // ground intersection, so the hardware test occludes the grid behind
        // scene geometry without this pass ever sampling the depth buffer.
        description.dss = wi::renderer::GetDepthStencilState(
            wi::enums::DSSTYPE_DEPTHREAD);
        description.bs = wi::renderer::GetBlendState(
            wi::enums::BSTYPE_PREMULTIPLIED);
        description.pt = wi::graphics::PrimitiveTopology::TRIANGLELIST;

        if (!device->CreatePipelineState(&description, &gridPipeline_))
        {
            wi::backlog::post(
                "Renegade: the editor grid pipeline could not be created. "
                "The viewport will render without a grid.",
                wi::backlog::LogLevel::Warning);
        }
    }

    void StudioRenderPath::DrawEditorGrid(
        const wi::graphics::CommandList cmd) const
    {
        if (!gridVisible_ || projectHubVisible_ ||
            creatorModelImporter.thumbnailCapturePending ||
            !gridPipeline_.IsValid() || camera == nullptr)
        {
            return;
        }

        auto* device = wi::graphics::GetDevice();
        device->EventBegin("Renegade Editor Grid", cmd);

        const XMMATRIX viewProjection = camera->GetViewProjection();

        GridConstants constants = {};
        XMStoreFloat4x4(&constants.viewProjection, viewProjection);
        XMStoreFloat4x4(
            &constants.inverseViewProjection,
            XMMatrixInverse(nullptr, viewProjection));
        // w is the grid plane height. The generated deck's top surface is at
        // exactly y = 0, so a grid drawn at y = 0 is coplanar with it and
        // loses the GREATER depth test. 2 cm is invisible at any working
        // camera distance and is the same trick Wicked's own helper uses.
        constants.cameraPosition = XMFLOAT4(
            camera->Eye.x,
            camera->Eye.y,
            camera->Eye.z,
            creatorModelImporter.active ? CreatorImportStageHeight + 0.02f : 0.02f);

        // Ice-blue is the approved interaction colour. Unlike Wicked's helper,
        // every line including the two axes is Renegade's to choose.
        constants.minorColor = XMFLOAT4(0.36f, 0.84f, 1.0f, 0.28f);
        constants.majorColor = XMFLOAT4(0.46f, 0.90f, 1.0f, 0.50f);
        constants.axisColorX = XMFLOAT4(1.00f, 0.42f, 0.06f, 0.70f);
        constants.axisColorZ = XMFLOAT4(0.30f, 0.78f, 1.00f, 0.70f);

        // Fade start/end, base spacing, master opacity. The fade window keeps
        // the horizon from turning into an aliased smear.
        constants.params = XMFLOAT4(60.0f, 320.0f, 1.0f, 1.0f);

        device->BindPipelineState(&gridPipeline_, cmd);
        device->BindDynamicConstantBuffer(constants, 0, cmd);
        device->Draw(3, 0, cmd);

        device->EventEnd(cmd);
    }

    void StudioRenderPath::RenderTransparents(
        const wi::graphics::CommandList cmd) const
    {
        RenderPath3D::RenderTransparents(cmd);

        // The base 3D scene (including the model's own transparent materials)
        // remains visible. Everything below is Renegade editor/importer overlay
        // content and must not contaminate the Asset Browser thumbnail.
        if (creatorModelImporter.thumbnailCapturePending)
            return;

        const bool drawMannequin = creatorModelImporter.active &&
            creatorModelImporter.mannequinVisible && session_ != nullptr;
        if ((!gridVisible_ || !gridPipeline_.IsValid()) && !drawMannequin)
        {
            return;
        }
        if (projectHubVisible_ || camera == nullptr)
        {
            return;
        }

        // Wicked ends every render pass before RenderTransparents() returns.
        // Open an explicit pass over the main colour and depth attachments so
        // the grid is valid on both DX12 and Vulkan. Match Wicked's own
        // transparent-pass attachment setup, including an MSAA resolve.
        auto* device = wi::graphics::GetDevice();
        wi::graphics::RenderPassImage attachments[3] = {};
        std::uint32_t attachmentCount = 0;
        attachments[attachmentCount++] =
            wi::graphics::RenderPassImage::RenderTarget(
                &rtMain_render,
                wi::graphics::RenderPassImage::LoadOp::LOAD);
        if (getMSAASampleCount() > 1)
        {
            attachments[attachmentCount++] =
                wi::graphics::RenderPassImage::Resolve(&rtMain);
        }
        attachments[attachmentCount++] =
            wi::graphics::RenderPassImage::DepthStencil(
                &depthBuffer_Main,
                wi::graphics::RenderPassImage::LoadOp::LOAD,
                wi::graphics::RenderPassImage::StoreOp::STORE,
                wi::graphics::ResourceState::DEPTHSTENCIL,
                wi::graphics::ResourceState::DEPTHSTENCIL,
                wi::graphics::ResourceState::DEPTHSTENCIL);

        device->RenderPassBegin(attachments, attachmentCount, cmd);

        wi::graphics::Viewport viewport;
        viewport.width =
            static_cast<float>(depthBuffer_Main.GetDesc().width);
        viewport.height =
            static_cast<float>(depthBuffer_Main.GetDesc().height);
        viewport.min_depth = 0.0f;
        viewport.max_depth = 1.0f;
        device->BindViewports(1, &viewport, cmd);

        const wi::graphics::Rect scissor = GetScissorInternalResolution();
        device->BindScissorRects(1, &scissor, cmd);

        DrawEditorGrid(cmd);
        if (drawMannequin)
        {
            XMFLOAT3 minimum;
            XMFLOAT3 maximum;
            if (CreatorImportWorldBounds(minimum, maximum) &&
                creatorImportHumanReference.IsValid())
            {
                constexpr float ReferenceHeight = 1.82f;
                constexpr float ReferenceWidth =
                    ReferenceHeight * (200.0f / 574.0f);
                constexpr float ReferenceHalfWidth = ReferenceWidth * 0.5f;
                constexpr float Clearance = 0.45f;
                const float x = minimum.x - Clearance - ReferenceHalfWidth;
                const float z = (minimum.z + maximum.z) * 0.5f;
                const XMMATRIX rotation =
                    XMLoadFloat3x3(&camera->rotationMatrix);
                const XMMATRIX projection = camera->GetViewProjection();
                wi::image::Params image;
                image.pos = XMFLOAT3(x, CreatorImportStageHeight, z);
                image.siz = XMFLOAT2(ReferenceWidth, ReferenceHeight);
                image.pivot = XMFLOAT2(0.5f, 1.0f);
                image.color = XMFLOAT4(0.82f, 0.84f, 0.85f, 0.92f);
                image.enableDrawRect(XMFLOAT4(0.0f, 3.0f, 200.0f, 574.0f));
                image.sampleFlag = wi::image::SAMPLEMODE_CLAMP;
                image.blendFlag = wi::enums::BLENDMODE_ALPHA;
                image.customRotation = &rotation;
                image.customProjection = &projection;
                image.enableDepthTest();
                wi::image::Draw(
                    &creatorImportHumanReference.GetTexture(),
                    image,
                    cmd);
            }
        }
        device->RenderPassEnd(cmd);
    }

    void StudioRenderPath::SetGridVisible(const bool visible)
    {
        gridVisible_ = visible;
        gridToggleButton_.SetText(visible ? "GRID ON" : "GRID OFF");
        studioChrome_.SetGridVisible(visible);

        if (session_ != nullptr)
        {
            session_->Projects().SetEditorPreference("grid_visible", visible);
        }
    }

    void StudioRenderPath::DeleteGPUResources()
    {
        selectionOutlineMask_ = {};
        selectionOutlineMaskMsaa_ = {};
        RenderPath3D::DeleteGPUResources();
    }

    void StudioRenderPath::ResizeBuffers()
    {
        RenderPath3D::ResizeBuffers();

        const auto* depthStencil = GetDepthStencil();
        if (depthStencil == nullptr)
        {
            return;
        }

        auto* device = wi::graphics::GetDevice();
        const XMUINT2 resolution = GetInternalResolution();
        wi::graphics::TextureDesc description;
        description.width = resolution.x;
        description.height = resolution.y;
        description.format = wi::graphics::Format::R8_UNORM;
        description.bind_flags =
            wi::graphics::BindFlag::RENDER_TARGET |
            wi::graphics::BindFlag::SHADER_RESOURCE;

        if (getMSAASampleCount() > 1)
        {
            description.sample_count = getMSAASampleCount();
            description.bind_flags = wi::graphics::BindFlag::RENDER_TARGET;
            if (device->CreateTexture(
                    &description,
                    nullptr,
                    &selectionOutlineMaskMsaa_))
            {
                device->SetName(
                    &selectionOutlineMaskMsaa_,
                    "renegade.selectionOutlineMaskMsaa");
            }
            description.sample_count = 1;
            description.bind_flags =
                wi::graphics::BindFlag::RENDER_TARGET |
                wi::graphics::BindFlag::SHADER_RESOURCE;
        }

        if (device->CreateTexture(
                &description,
                nullptr,
                &selectionOutlineMask_))
        {
            device->SetName(
                &selectionOutlineMask_,
                "renegade.selectionOutlineMask");
        }
    }

    void StudioRenderPath::Render() const
    {
        RenderPath3D::Render();

        const auto* depthStencil = GetDepthStencil();
        if (projectHubVisible_ ||
            creatorModelImporter.thumbnailCapturePending ||
            outlinedEntity_ == wi::ecs::INVALID_ENTITY ||
            depthStencil == nullptr ||
            !selectionOutlineMask_.IsValid())
        {
            return;
        }

        auto* device = wi::graphics::GetDevice();
        const auto commandList = device->BeginCommandList();
        device->EventBegin("Renegade Selection Outline Mask", commandList);

        if (selectionOutlineMaskMsaa_.IsValid())
        {
            const wi::graphics::RenderPassImage renderPass[] = {
                wi::graphics::RenderPassImage::RenderTarget(
                    &selectionOutlineMaskMsaa_,
                    wi::graphics::RenderPassImage::LoadOp::CLEAR,
                    wi::graphics::RenderPassImage::StoreOp::DONTCARE),
                wi::graphics::RenderPassImage::Resolve(
                    &selectionOutlineMask_),
                wi::graphics::RenderPassImage::DepthStencil(
                    depthStencil,
                    wi::graphics::RenderPassImage::LoadOp::LOAD,
                    wi::graphics::RenderPassImage::StoreOp::STORE),
            };
            device->RenderPassBegin(
                renderPass,
                arraysize(renderPass),
                commandList);
        }
        else
        {
            const wi::graphics::RenderPassImage renderPass[] = {
                wi::graphics::RenderPassImage::RenderTarget(
                    &selectionOutlineMask_,
                    wi::graphics::RenderPassImage::LoadOp::CLEAR),
                wi::graphics::RenderPassImage::DepthStencil(
                    depthStencil,
                    wi::graphics::RenderPassImage::LoadOp::LOAD,
                    wi::graphics::RenderPassImage::StoreOp::STORE),
            };
            device->RenderPassBegin(
                renderPass,
                arraysize(renderPass),
                commandList);
        }

        wi::graphics::Viewport viewport;
        viewport.width =
            static_cast<float>(selectionOutlineMask_.GetDesc().width);
        viewport.height =
            static_cast<float>(selectionOutlineMask_.GetDesc().height);
        device->BindViewports(1, &viewport, commandList);

        wi::image::Params mask;
        mask.enableFullScreen();
        mask.stencilComp = wi::image::STENCILMODE::STENCILMODE_EQUAL;
        mask.stencilRefMode = wi::image::STENCILREFMODE_USER;
        mask.stencilRef = SelectionStencilReference;
        wi::image::Draw(nullptr, mask, commandList);

        device->RenderPassEnd(commandList);
        device->EventEnd(commandList);
    }

    void StudioRenderPath::CreateWorkspaceShell()
    {
        toolbarPanel_.Create(
            "Renegade Command Bar",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        toolbarPanel_.SetShadowRadius(8.0f);
        GetGUI().AddWidget(&toolbarPanel_);

        workspaceTitle_.Create("Renegade Workspace Title");
        workspaceTitle_.SetText("RENEGADE STUDIO // PROVING GROUND");
        // Size 19 overflowed the 300px slot and clipped mid-word. Fit-text
        // also keeps long project names inside the label rather than running
        // them under the tool buttons.
        workspaceTitle_.font.params.size = 16;
        workspaceTitle_.SetFitTextEnabled(true);
        workspaceTitle_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        toolbarPanel_.AddWidget(&workspaceTitle_);

        const auto createToolButton = [this](
            wi::gui::Button& button,
            const char* name,
            const char* text,
            const char* tooltip,
            const EditorAction action)
        {
            button.Create(name);
            button.SetText(text);
            button.SetTooltip(tooltip);
            button.SetAngularHighlightWidth(4.0f);
            button.OnClick([this, action](const wi::gui::EventArgs&)
            {
                pendingAction_ = action;
            });
            toolbarPanel_.AddWidget(&button);
        };
        createToolButton(
            translateToolButton_,
            "Translate Tool",
            "MOVE [W]",
            "Translate the selected entity",
            EditorAction::TranslateTool);
        createToolButton(
            rotateToolButton_,
            "Rotate Tool",
            "ROTATE [E]",
            "Rotate the selected entity",
            EditorAction::RotateTool);
        createToolButton(
            scaleToolButton_,
            "Scale Tool",
            "SCALE [R]",
            "Scale the selected entity",
            EditorAction::ScaleTool);

        projectHubButton_.Create("Open Project Hub");
        gridToggleButton_.Create("Grid Toggle");
        gridToggleButton_.SetText("GRID ON");
        gridToggleButton_.SetTooltip(
            "Show or hide the editor grid [G]. The grid is never saved into a "
            "scene.");
        gridToggleButton_.OnClick([this](wi::gui::EventArgs)
        {
            pendingAction_ = EditorAction::ToggleGrid;
        });
        toolbarPanel_.AddWidget(&gridToggleButton_);

        projectHubButton_.SetText("PROJECTS");
        projectHubButton_.SetTooltip("Return to the Renegade Project Hub");
        projectHubButton_.SetAngularHighlightWidth(4.0f);
        projectHubButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ReturnToProjectHub();
        });
        toolbarPanel_.AddWidget(&projectHubButton_);

        statusLabel_.Create("Renegade Studio Status");
        statusLabel_.SetSize(XMFLOAT2(720.0f, 22.0f));
        statusLabel_.font.params.size = 14;
        statusLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        toolbarPanel_.AddWidget(&statusLabel_);

        hierarchyPanel_.Create(
            "World Outliner",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        hierarchyPanel_.SetShadowRadius(8.0f);
        GetGUI().AddWidget(&hierarchyPanel_);

        hierarchyLabel_.Create("Scene Hierarchy");
        hierarchyLabel_.SetText("WORLD // HIERARCHY");
        hierarchyLabel_.font.params.size = 16;
        hierarchyLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        hierarchyPanel_.AddWidget(&hierarchyLabel_);

        hierarchyTree_.Create("Renegade Hierarchy");
        hierarchyTree_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
            {
                return;
            }
            session_->Selection().Select(
                static_cast<wi::ecs::Entity>(args.userdata));
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            RefreshInspector();
            RefreshStatus();
        });
        hierarchyPanel_.AddWidget(&hierarchyTree_);

        hierarchySearch_.Create("Hierarchy Search");
        hierarchySearch_.SetDescription("⌕  ");
        hierarchySearch_.SetValue("");
        hierarchySearch_.SetPlaceholder("SEARCH SCENE...");
        hierarchySearch_.SetTooltip("Filter the visible scene hierarchy");
        hierarchySearch_.SetCancelInputEnabled(false);
        hierarchySearch_.OnInput([this](const wi::gui::EventArgs& args)
        {
            studioChrome_.SetHierarchyFilter(args.sValue);
        });
        hierarchySearch_.OnInputAccepted(
            [this](const wi::gui::EventArgs& args)
        {
            studioChrome_.SetHierarchyFilter(args.sValue);
        });
        GetGUI().AddWidget(&hierarchySearch_);

        inspectorPanel_.Create(
            "Inspector",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        inspectorPanel_.SetShadowRadius(0.0f);
        inspectorPanel_.SetColor(wi::Color::Transparent());
        inspectorPanel_.SetColor(
            HologramPanel,
            wi::gui::WIDGET_ID_WINDOW_BASE);
        GetGUI().AddWidget(&inspectorPanel_);

        inspectorLabel_.Create("Transform Inspector");
        inspectorLabel_.SetText("TRANSFORM // SELECT AN ENTITY");
        inspectorLabel_.font.params.size = 16;
        inspectorLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        inspectorLabel_.SetColor(wi::Color::Transparent());
        inspectorPanel_.AddWidget(&inspectorLabel_);

        const auto createSectionLabel = [this](
            wi::gui::Label& label,
            const char* name,
            const char* text)
        {
            label.Create(name);
            label.SetText(text);
            label.font.params.size = 13;
            label.font.params.color = HologramMuted;
            label.font.params.h_align = wi::font::WIFALIGN_LEFT;
            label.SetColor(wi::Color::Transparent());
            inspectorPanel_.AddWidget(&label);
        };
        createSectionLabel(
            positionLabel_,
            "Position Section",
            "POSITION");
        createSectionLabel(
            rotationLabel_,
            "Rotation Section",
            "ROTATION // DEGREES");
        createSectionLabel(
            scaleLabel_,
            "Scale Section",
            "SCALE");

        const auto createTransformInput = [this](
            wi::gui::TextInputField& input,
            const char* name,
            const char* description,
            const TransformTool tool,
            const int axis)
        {
            input.Create(name);
            input.SetDescription(description);
            input.SetValue(0.0f);
            input.SetSize(XMFLOAT2(90.0f, 28.0f));
            input.OnInputAccepted(
                [this, tool, axis](const wi::gui::EventArgs& args)
            {
                ApplySelectedTransformValue(tool, axis, args.fValue);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createTransformInput(
            translationX_,
            "Translation X",
            "X: ",
            TransformTool::Translate,
            0);
        createTransformInput(
            translationY_,
            "Translation Y",
            "Y: ",
            TransformTool::Translate,
            1);
        createTransformInput(
            translationZ_,
            "Translation Z",
            "Z: ",
            TransformTool::Translate,
            2);
        createTransformInput(
            rotationX_,
            "Rotation X",
            "X: ",
            TransformTool::Rotate,
            0);
        createTransformInput(
            rotationY_,
            "Rotation Y",
            "Y: ",
            TransformTool::Rotate,
            1);
        createTransformInput(
            rotationZ_,
            "Rotation Z",
            "Z: ",
            TransformTool::Rotate,
            2);
        createTransformInput(
            scaleX_,
            "Scale X",
            "X: ",
            TransformTool::Scale,
            0);
        createTransformInput(
            scaleY_,
            "Scale Y",
            "Y: ",
            TransformTool::Scale,
            1);
        createTransformInput(
            scaleZ_,
            "Scale Z",
            "Z: ",
            TransformTool::Scale,
            2);

        createSectionLabel(
            lightLabel_,
            "Light Section",
            "LIGHT // NATIVE WICKED");
        lightType_.Create("Light Type");
        lightType_.AddItem(
            "DIRECTIONAL",
            static_cast<std::uint64_t>(
                wi::scene::LightComponent::DIRECTIONAL));
        lightType_.AddItem(
            "POINT",
            static_cast<std::uint64_t>(wi::scene::LightComponent::POINT));
        lightType_.AddItem(
            "SPOT",
            static_cast<std::uint64_t>(wi::scene::LightComponent::SPOT));
        lightType_.AddItem(
            "RECTANGLE",
            static_cast<std::uint64_t>(wi::scene::LightComponent::RECTANGLE));
        lightType_.SetTooltip(
            "Wicked's four native light types. Type-specific shape controls "
            "appear below.");
        lightType_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            ApplySelectedLightType(
                static_cast<wi::scene::LightComponent::LightType>(
                    args.userdata));
        });
        inspectorPanel_.AddWidget(&lightType_);

        const auto createLightSlider = [this](
            RenegadeSlider& input,
            const char* name,
            const char* label,
            const char* tooltip,
            const LightField field,
            const float minimum,
            const float maximum,
            const float steps)
        {
            input.Create(minimum, maximum, 0.0f, steps, name, label);
            input.SetTooltip(tooltip);
            input.OnDragStarted([this, field](const float)
            {
                BeginLightSlider(field);
            });
            input.OnValuePreview([this, field](const float value)
            {
                PreviewLightSlider(field, value);
            });
            input.OnValueCommitted([this, field](const float value)
            {
                CommitLightSlider(field, value);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createLightSlider(
            lightColorRed_,
            "Light Color Red",
            "COLOUR // RED",
            "Red channel of the native light colour.",
            LightField::ColorRed,
            0.0f,
            1.0f,
            255.0f);
        createLightSlider(
            lightColorGreen_,
            "Light Color Green",
            "COLOUR // GREEN",
            "Green channel of the native light colour.",
            LightField::ColorGreen,
            0.0f,
            1.0f,
            255.0f);
        createLightSlider(
            lightColorBlue_,
            "Light Color Blue",
            "COLOUR // BLUE",
            "Blue channel of the native light colour.",
            LightField::ColorBlue,
            0.0f,
            1.0f,
            255.0f);
        createLightSlider(
            lightIntensity_,
            "Light Intensity",
            "INTENSITY",
            "Brightness in Wicked's native physical units for this type.",
            LightField::Intensity,
            0.0f,
            2000.0f,
            20000.0f);
        createLightSlider(
            lightRange_,
            "Light Range",
            "RANGE",
            "Maximum influence distance. Directional lights are scene-wide.",
            LightField::Range,
            0.0f,
            1000.0f,
            10000.0f);
        createLightSlider(
            lightOuterCone_,
            "Light Outer Cone",
            "SPOT // OUTER CONE",
            "Outer spotlight cone angle in degrees.",
            LightField::OuterCone,
            0.1f,
            89.9f,
            898.0f);
        createLightSlider(
            lightInnerCone_,
            "Light Inner Cone",
            "SPOT // INNER CONE",
            "Inner spotlight cone angle; it cannot exceed the outer cone.",
            LightField::InnerCone,
            0.0f,
            89.9f,
            899.0f);
        createLightSlider(
            lightRadius_,
            "Light Radius",
            "SOURCE // RADIUS",
            "Physical source radius; also controls directional shadow softness.",
            LightField::Radius,
            0.0f,
            10.0f,
            1000.0f);
        createLightSlider(
            lightLength_,
            "Light Length Or Width",
            "SOURCE // LENGTH / WIDTH",
            "Point capsule length, or rectangle width.",
            LightField::Length,
            0.0f,
            100.0f,
            2000.0f);
        createLightSlider(
            lightHeight_,
            "Light Height",
            "SOURCE // HEIGHT",
            "Rectangle light height.",
            LightField::Height,
            0.0f,
            100.0f,
            2000.0f);

        lightCastShadow_.Create("Cast shadows: ");
        lightCastShadow_.SetTooltip(
            "Render native Wicked shadows from this light.");
        lightCastShadow_.OnClick([this](const wi::gui::EventArgs& args)
        {
            ApplySelectedLightToggle(LightToggle::CastShadow, args.bValue);
        });
        inspectorPanel_.AddWidget(&lightCastShadow_);

        lightVolumetrics_.Create("Volumetric beam: ");
        lightVolumetrics_.SetTooltip(
            "Enable Wicked's real shadow-aware volumetric light scattering.");
        lightVolumetrics_.OnClick([this](const wi::gui::EventArgs& args)
        {
            ApplySelectedLightToggle(LightToggle::Volumetrics, args.bValue);
        });
        inspectorPanel_.AddWidget(&lightVolumetrics_);
        createLightSlider(
            lightVolumetricBoost_,
            "Light Volumetric Boost",
            "VOLUMETRIC // BOOST",
            "Increase this light's contribution to volumetric fog.",
            LightField::VolumetricBoost,
            0.0f,
            10.0f,
            1000.0f);

        createSectionLabel(
            environmentSkyLabel_,
            "Environment Sky Section",
            "SKY // ATMOSPHERE");
        environmentPreset_.Create("Environment Preset");
        environmentPreset_.AddItem("CUSTOM", 0);
        environmentPreset_.AddItem("CLEAR", 1);
        environmentPreset_.AddItem("SCATTERED", 2);
        environmentPreset_.AddItem("OVERCAST", 3);
        environmentPreset_.AddItem("STORM", 4);
        environmentPreset_.SetTooltip(
            "Apply a curated starting point. Every preset is a single "
            "Undo/Redo command.");
        environmentPreset_.OnSelect(
            [this](const wi::gui::EventArgs& args)
        {
            if (args.userdata != 0)
            {
                ApplyWeatherPreset(static_cast<int>(args.userdata));
            }
        });
        inspectorPanel_.AddWidget(&environmentPreset_);

        skyMode_.Create("Sky Mode");
        skyMode_.AddItem(
            "REALISTIC SKY",
            static_cast<std::uint64_t>(
                bridge::WeatherState::SkyMode::Realistic));
        skyMode_.AddItem(
            "REALISTIC + CLOUDS",
            static_cast<std::uint64_t>(
                bridge::WeatherState::SkyMode::RealisticWithClouds));
        skyMode_.AddItem(
            "SKYBOX TEXTURE",
            static_cast<std::uint64_t>(
                bridge::WeatherState::SkyMode::Skybox));
        skyMode_.SetTooltip(
            "Choose Wicked's physical atmosphere, volumetric clouds, or the "
            "weather component's existing skybox texture.");
        skyMode_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            ApplySelectedSkyMode(
                static_cast<bridge::WeatherState::SkyMode>(args.userdata));
        });
        inspectorPanel_.AddWidget(&skyMode_);

        const auto createWeatherToggle = [this](
            wi::gui::CheckBox& input,
            const char* name,
            const char* tooltip,
            const WeatherToggle toggle)
        {
            input.Create(name);
            input.SetTooltip(tooltip);
            input.OnClick([this, toggle](const wi::gui::EventArgs& args)
            {
                ApplySelectedWeatherToggle(toggle, args.bValue);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createWeatherToggle(
            aerialPerspective_,
            "Aerial perspective: ",
            "Apply atmospheric scattering to scene geometry.",
            WeatherToggle::AerialPerspective);

        const auto createWeatherSlider = [this](
            RenegadeSlider& input,
            const char* name,
            const char* label,
            const char* tooltip,
            const WeatherField field,
            const float minimum,
            const float maximum,
            const float steps)
        {
            input.Create(
                minimum,
                maximum,
                0.0f,
                steps,
                name,
                label);
            input.SetTooltip(tooltip);
            input.OnDragStarted([this, field](const float)
            {
                BeginWeatherSlider(field);
            });
            input.OnValuePreview([this, field](const float value)
            {
                PreviewWeatherSlider(field, value);
            });
            input.OnValueCommitted([this, field](const float value)
            {
                CommitWeatherSlider(field, value);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createWeatherSlider(
            skyExposure_,
            "Sky Exposure",
            "EXPOSURE",
            "Brightness of the physical sky.",
            WeatherField::SkyExposure,
            0.0f,
            4.0f,
            400.0f);
        createWeatherSlider(
            stars_,
            "Stars",
            "STARS",
            "Procedural star visibility in the native realistic sky.",
            WeatherField::Stars,
            0.0f,
            1.0f,
            100.0f);
        createWeatherSlider(
            ambientIntensity_,
            "Ambient Intensity",
            "AMBIENT",
            "Neutral intensity applied while preserving the authored hue.",
            WeatherField::AmbientIntensity,
            0.0f,
            2.0f,
            400.0f);

        createSectionLabel(
            environmentFogLabel_,
            "Environment Fog Section",
            "FOG // HEIGHT LAYER");
        createWeatherSlider(
            fogStart_,
            "Fog Start",
            "START",
            "Distance from the camera before fog begins.",
            WeatherField::FogStart,
            0.0f,
            500.0f,
            500.0f);
        createWeatherSlider(
            fogDensity_,
            "Fog Density",
            "DENSITY",
            "Overall atmospheric fog density.",
            WeatherField::FogDensity,
            0.0f,
            0.1f,
            1000.0f);
        createWeatherToggle(
            heightFog_,
            "Height fog: ",
            "Restrict fog vertically between the authored heights.",
            WeatherToggle::HeightFog);
        createWeatherSlider(
            fogHeightStart_,
            "Fog Height Start",
            "BASE",
            "Lower height of the fog layer.",
            WeatherField::FogHeightStart,
            -100.0f,
            100.0f,
            400.0f);
        createWeatherSlider(
            fogHeightEnd_,
            "Fog Height End",
            "TOP",
            "Upper height of the fog layer.",
            WeatherField::FogHeightEnd,
            -100.0f,
            200.0f,
            600.0f);

        createSectionLabel(
            environmentCloudLabel_,
            "Environment Cloud Section",
            "VOLUMETRIC CLOUDS");
        createWeatherSlider(
            cloudCoverage_,
            "Cloud Coverage",
            "COVERAGE",
            "Primary cloud-layer coverage amount.",
            WeatherField::CloudCoverage,
            0.0f,
            1.0f,
            100.0f);
        createWeatherSlider(
            cloudStartHeight_,
            "Cloud Start Height",
            "BASE",
            "Altitude where the volumetric cloud volume begins.",
            WeatherField::CloudStartHeight,
            100.0f,
            10000.0f,
            990.0f);
        createWeatherSlider(
            cloudThickness_,
            "Cloud Thickness",
            "DEPTH",
            "Vertical depth of the volumetric cloud volume.",
            WeatherField::CloudThickness,
            100.0f,
            10000.0f,
            990.0f);
        createWeatherToggle(
            cloudsCastShadow_,
            "Cloud shadows: ",
            "Allow volumetric clouds to cast moving shadows on the world.",
            WeatherToggle::CloudsCastShadow);

        createSectionLabel(
            precipitationLabel_,
            "Environment Precipitation Section",
            "PRECIPITATION // NATIVE PARTICLES");

        precipitationMode_.Create("Precipitation Mode");
        precipitationMode_.AddItem(
            "OFF",
            static_cast<std::uint64_t>(bridge::PrecipitationMode::None));
        precipitationMode_.AddItem(
            "RAIN",
            static_cast<std::uint64_t>(bridge::PrecipitationMode::Rain));
        precipitationMode_.AddItem(
            "SNOW",
            static_cast<std::uint64_t>(bridge::PrecipitationMode::Snow));
        precipitationMode_.SetTooltip(
            "Rain uses Wicked's native precipitation renderer. Snow uses a "
            "Renegade-authored slow flake profile over the same GPU emitter.");
        precipitationMode_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            ApplyPrecipitationMode(
                static_cast<bridge::PrecipitationMode>(args.userdata));
        });
        inspectorPanel_.AddWidget(&precipitationMode_);

        const auto createPrecipitationSlider = [this](
            RenegadeSlider& input,
            const char* name,
            const char* label,
            const char* tooltip,
            const PrecipitationField field,
            const float minimum,
            const float maximum,
            const float steps)
        {
            input.Create(minimum, maximum, 0.0f, steps, name, label);
            input.SetTooltip(tooltip);
            input.OnDragStarted([this, field](const float)
            {
                BeginPrecipitationSlider(field);
            });
            input.OnValuePreview([this, field](const float value)
            {
                PreviewPrecipitationSlider(field, value);
            });
            input.OnValueCommitted([this, field](const float value)
            {
                CommitPrecipitationSlider(field, value);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createPrecipitationSlider(
            precipitationIntensity_,
            "Precipitation Intensity",
            "INTENSITY",
            "Particle density. Zero disables precipitation.",
            PrecipitationField::Intensity,
            0.0f,
            1.0f,
            200.0f);
        createPrecipitationSlider(
            precipitationFallSpeed_,
            "Precipitation Fall Speed",
            "FALL SPEED",
            "Downward particle speed; snow profiles start much slower.",
            PrecipitationField::FallSpeed,
            0.01f,
            2.0f,
            400.0f);
        createPrecipitationSlider(
            precipitationParticleScale_,
            "Precipitation Particle Scale",
            "PARTICLE SIZE",
            "Rendered particle size.",
            PrecipitationField::ParticleScale,
            0.005f,
            0.1f,
            400.0f);
        createPrecipitationSlider(
            precipitationWindAzimuth_,
            "Precipitation Wind Azimuth",
            "WIND DIRECTION",
            "Horizontal wind direction in degrees.",
            PrecipitationField::WindAzimuth,
            -180.0f,
            180.0f,
            360.0f);
        createPrecipitationSlider(
            precipitationWindSpeed_,
            "Precipitation Wind Speed",
            "WIND SPEED",
            "Horizontal wind strength applied to precipitation.",
            PrecipitationField::WindSpeed,
            0.0f,
            20.0f,
            400.0f);
        createPrecipitationSlider(
            precipitationTurbulence_,
            "Precipitation Turbulence",
            "TURBULENCE",
            "Random particle drift; higher values create snow flurries.",
            PrecipitationField::Turbulence,
            0.0f,
            20.0f,
            400.0f);

        createSectionLabel(
            sunLabel_,
            "Environment Sun Section",
            "SUN // TIME OF DAY");
        sunPreset_.Create("Sun Preset");
        sunPreset_.AddItem("CUSTOM", 0);
        sunPreset_.AddItem(
            "DAWN",
            static_cast<std::uint64_t>(bridge::SunPreset::Dawn) + 1u);
        sunPreset_.AddItem(
            "MIDDAY",
            static_cast<std::uint64_t>(bridge::SunPreset::Midday) + 1u);
        sunPreset_.AddItem(
            "GOLDEN HOUR",
            static_cast<std::uint64_t>(bridge::SunPreset::GoldenHour) + 1u);
        sunPreset_.AddItem(
            "DUSK",
            static_cast<std::uint64_t>(bridge::SunPreset::Dusk) + 1u);
        sunPreset_.AddItem(
            "MIDNIGHT",
            static_cast<std::uint64_t>(bridge::SunPreset::Midnight) + 1u);
        sunPreset_.SetTooltip(
            "Move the serialized scene sun to a curated time of day.");
        sunPreset_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if (args.userdata > 0)
            {
                ApplySunPreset(static_cast<bridge::SunPreset>(
                    args.userdata - 1u));
            }
        });
        inspectorPanel_.AddWidget(&sunPreset_);

        const auto createSunSlider = [this](
            RenegadeSlider& input,
            const char* name,
            const char* label,
            const char* tooltip,
            const SunField field,
            const float minimum,
            const float maximum,
            const float steps)
        {
            input.Create(minimum, maximum, 0.0f, steps, name, label);
            input.SetTooltip(tooltip);
            input.OnDragStarted([this, field](const float)
            {
                BeginSunSlider(field);
            });
            input.OnValuePreview([this, field](const float value)
            {
                PreviewSunSlider(field, value);
            });
            input.OnValueCommitted([this, field](const float value)
            {
                CommitSunSlider(field, value);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createSunSlider(
            sunTime_,
            "Sun Time",
            "TIME // HOURS",
            "Time from 00:00 to 24:00. The value box accepts direct input.",
            SunField::Time,
            0.0f,
            24.0f,
            288.0f);
        createSunSlider(
            sunAzimuth_,
            "Sun Azimuth",
            "AZIMUTH",
            "Horizontal sun direction in degrees.",
            SunField::Azimuth,
            -180.0f,
            180.0f,
            360.0f);
        createSunSlider(
            sunElevation_,
            "Sun Elevation",
            "ELEVATION",
            "Sun height above or below the horizon in degrees.",
            SunField::Elevation,
            -90.0f,
            90.0f,
            360.0f);

        sunPreviewSpeed_.Create(
            0.001f,
            24.0f,
            0.100f,
            23999.0f,
            "Sun Preview Speed",
            "PREVIEW HOURS / SEC");
        sunPreviewSpeed_.SetTooltip(
            "Editor-only preview speed from 0.001 to 24.000 hours per "
            "second. It is not written to the scene.");
        sunPreviewSpeed_.OnValuePreview([this](const float value)
        {
            sunPreviewSpeedHoursPerSecond_ = value;
        });
        sunPreviewSpeed_.OnValueCommitted([this](const float value)
        {
            sunPreviewSpeedHoursPerSecond_ = value;
        });
        inspectorPanel_.AddWidget(&sunPreviewSpeed_);

        sunPlayButton_.Create("Play Sun Preview");
        sunPlayButton_.SetText("PLAY DAY");
        sunPlayButton_.SetTooltip(
            "Preview the 24-hour path. Pausing commits one Undo step.");
        sunPlayButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::StartSunPreview;
        });
        inspectorPanel_.AddWidget(&sunPlayButton_);

        sunPauseButton_.Create("Pause Sun Preview");
        sunPauseButton_.SetText("PAUSE");
        sunPauseButton_.SetTooltip(
            "Pause the preview and commit its final time as one Undo step.");
        sunPauseButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::PauseSunPreview;
        });
        inspectorPanel_.AddWidget(&sunPauseButton_);

        createSectionLabel(
            oceanLabel_,
            "Environment Ocean Section",
            "OCEAN // NATIVE FFT");
        oceanEnabled_.Create("Ocean enabled: ");
        oceanEnabled_.SetTooltip(
            "Enable Wicked's infinite camera-relative FFT ocean surface.");
        oceanEnabled_.OnClick([this](const wi::gui::EventArgs& args)
        {
            pendingOceanEnabled_ = args.bValue;
            pendingAction_ = EditorAction::SetOceanEnabled;
        });
        inspectorPanel_.AddWidget(&oceanEnabled_);

        oceanPreset_.Create("Ocean Preset");
        oceanPreset_.AddItem("CUSTOM", 0);
        oceanPreset_.AddItem(
            "CALM",
            static_cast<std::uint64_t>(bridge::OceanPreset::Calm) + 1u);
        oceanPreset_.AddItem(
            "COASTAL",
            static_cast<std::uint64_t>(bridge::OceanPreset::Coastal) + 1u);
        oceanPreset_.AddItem(
            "STORM",
            static_cast<std::uint64_t>(bridge::OceanPreset::Storm) + 1u);
        oceanPreset_.AddItem(
            "ALIEN",
            static_cast<std::uint64_t>(bridge::OceanPreset::Alien) + 1u);
        oceanPreset_.SetTooltip(
            "Apply a complete native-ocean starting point as one Undo step.");
        oceanPreset_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if (args.userdata > 0)
            {
                pendingOceanPreset_ = static_cast<bridge::OceanPreset>(
                    args.userdata - 1u);
                pendingAction_ = EditorAction::ApplyOceanPreset;
            }
        });
        inspectorPanel_.AddWidget(&oceanPreset_);

        oceanResolution_.Create("Ocean FFT Resolution");
        oceanResolution_.AddItem("64 // LOW", 64);
        oceanResolution_.AddItem("128", 128);
        oceanResolution_.AddItem("256", 256);
        oceanResolution_.AddItem("512 // DEFAULT", 512);
        oceanResolution_.AddItem("1024 // EXPENSIVE", 1024);
        oceanResolution_.SetTooltip(
            "FFT displacement-map dimension. 1024 can be expensive and "
            "recreates the native simulation resources.");
        oceanResolution_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            pendingOceanResolution_ = static_cast<int>(args.userdata);
            pendingAction_ = EditorAction::SetOceanResolution;
        });
        inspectorPanel_.AddWidget(&oceanResolution_);

        const auto createOceanSlider = [this](
            RenegadeSlider& input,
            const char* name,
            const char* label,
            const char* tooltip,
            const OceanField field,
            const float minimum,
            const float maximum,
            const float steps)
        {
            input.Create(minimum, maximum, 0.0f, steps, name, label);
            input.SetTooltip(tooltip);
            input.OnDragStarted([this, field](const float)
            {
                BeginOceanSlider(field);
            });
            input.OnValuePreview([this, field](const float value)
            {
                PreviewOceanSlider(field, value);
            });
            input.OnValueCommitted([this, field](const float value)
            {
                CommitOceanSlider(field, value);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createOceanSlider(oceanWaterHeight_, "Ocean Water Height", "LEVEL",
            "World-space ocean height.", OceanField::WaterHeight,
            -100.0f, 100.0f, 800.0f);
        createOceanSlider(oceanPatchLength_, "Ocean Patch Length", "PATCH SIZE",
            "FFT tiling scale; changing it recreates the simulation.",
            OceanField::PatchLength, 1.0f, 1000.0f, 999.0f);
        createOceanSlider(oceanWaveAmplitude_, "Ocean Wave Amplitude", "WAVE AMPLITUDE",
            "Transverse wave energy; changing it recreates the simulation.",
            OceanField::WaveAmplitude, 0.0f, 1000.0f, 1000.0f);
        createOceanSli…54308 tokens truncated… }
        session_->Selection().Select(createCommand->CreatedEntity());
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ExpandTerrain()
    {
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        if (!scene.terrains.Contains(entity))
        {
            return;
        }
        session_->Commands().Execute(
            std::make_unique<bridge::ExpandTerrainCommand>(scene, entity));
        RefreshInspector();
        RefreshStatus();
    }

    bool StudioRenderPath::CommitTerrain(const bridge::TerrainState& terrain)
    {
        if (session_ == nullptr)
        {
            return false;
        }
        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        if (!scene.terrains.Contains(entity))
        {
            return false;
        }
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetTerrainCommand>(
                scene,
                entity,
                terrain));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::SetTerrainFieldValue(
        bridge::TerrainState& terrain,
        const TerrainField field,
        const float value) noexcept
    {
        switch (field)
        {
        case TerrainField::ChunkScale:
            terrain.chunkScale = std::clamp(value, 0.25f, 16.0f);
            break;
        case TerrainField::MinimumHeight:
            terrain.minimumHeight = std::clamp(value, -2000.0f, 1999.0f);
            break;
        case TerrainField::MaximumHeight:
            terrain.maximumHeight = std::clamp(value, -1999.0f, 2000.0f);
            break;
        case TerrainField::LowAltitudeBlend:
            terrain.lowAltitudeBlend = std::clamp(value, 0.0f, 1.0f);
            break;
        case TerrainField::BaseBlend:
            terrain.baseBlend = std::clamp(value, 0.0f, 1.0f);
            break;
        case TerrainField::SlopeBlend:
            terrain.slopeBlend = std::clamp(value, 0.0f, 1.0f);
            break;
        case TerrainField::LodBias:
            terrain.lodBias = std::clamp(value, -4.0f, 4.0f);
            break;
        }
    }

    void StudioRenderPath::BeginTerrainSlider(const TerrainField field)
    {
        terrainSliderActive_ = false;
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = session_->Selection().SelectedEntity();
        const auto* terrain =
            session_->Scenes().GetScene().terrains.GetComponent(entity);
        if (terrain == nullptr)
        {
            return;
        }
        terrainSliderActive_ = true;
        terrainSliderField_ = field;
        terrainSliderEntity_ = entity;
        terrainSliderBefore_ = bridge::CaptureTerrain(*terrain);
        terrainSliderAfter_ = terrainSliderBefore_;
    }

    void StudioRenderPath::PreviewTerrainSlider(
        const TerrainField field,
        const float value)
    {
        if (!terrainSliderActive_ || terrainSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        auto* terrain = session_->Scenes().GetScene().terrains.GetComponent(
            terrainSliderEntity_);
        if (terrain == nullptr)
        {
            return;
        }
        terrainSliderAfter_ = terrainSliderBefore_;
        SetTerrainFieldValue(terrainSliderAfter_, field, value);
        bridge::ApplyTerrain(*terrain, terrainSliderAfter_, false);
    }

    void StudioRenderPath::CommitTerrainSlider(
        const TerrainField field,
        const float value)
    {
        if (!terrainSliderActive_ || terrainSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        SetTerrainFieldValue(terrainSliderAfter_, field, value);
        auto& scene = session_->Scenes().GetScene();
        auto* terrain = scene.terrains.GetComponent(terrainSliderEntity_);
        if (terrain != nullptr)
        {
            bridge::ApplyTerrain(*terrain, terrainSliderBefore_, false);
            session_->Commands().Execute(
                std::make_unique<bridge::SetTerrainCommand>(
                    scene,
                    terrainSliderEntity_,
                    terrainSliderBefore_,
                    terrainSliderAfter_));
        }
        terrainSliderActive_ = false;
        terrainSliderEntity_ = wi::ecs::INVALID_ENTITY;
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ApplyTerrainMaterialPreset(
        const bridge::TerrainMaterialPreset preset)
    {
        if (session_ == nullptr ||
            session_->Scenes().GetScene().terrains.GetCount() == 0)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        const auto entity = scene.terrains.GetEntity(0);
        const auto* terrain = scene.terrains.GetComponent(entity);
        if (terrain == nullptr)
        {
            return;
        }
        auto before = bridge::CaptureTerrainMaterial(scene, *terrain);
        auto after = before;
        bridge::SetTerrainTextureScale(
            after,
            bridge::MakeTerrainMaterialPreset(preset));
        session_->Commands().Execute(
            std::make_unique<bridge::SetTerrainMaterialCommand>(
                scene,
                entity,
                std::move(before),
                std::move(after)));
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::BeginTerrainTextureScale()
    {
        terrainTextureScaleActive_ = false;
        if (session_ == nullptr ||
            session_->Scenes().GetScene().terrains.GetCount() == 0)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        terrainMaterialEntity_ = scene.terrains.GetEntity(0);
        const auto* terrain = scene.terrains.GetComponent(
            terrainMaterialEntity_);
        if (terrain == nullptr)
        {
            terrainMaterialEntity_ = wi::ecs::INVALID_ENTITY;
            return;
        }
        terrainMaterialBefore_ = bridge::CaptureTerrainMaterial(scene, *terrain);
        terrainMaterialAfter_ = terrainMaterialBefore_;
        terrainTextureScaleActive_ = true;
    }

    void StudioRenderPath::PreviewTerrainTextureScale(const float value)
    {
        if (!terrainTextureScaleActive_ || session_ == nullptr)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        auto* terrain = scene.terrains.GetComponent(terrainMaterialEntity_);
        if (terrain == nullptr)
        {
            return;
        }
        terrainMaterialAfter_ = terrainMaterialBefore_;
        bridge::SetTerrainTextureScale(terrainMaterialAfter_, value);
        bridge::ApplyTerrainMaterial(
            scene,
            *terrain,
            terrainMaterialAfter_,
            true);
    }

    void StudioRenderPath::CommitTerrainTextureScale(const float value)
    {
        if (!terrainTextureScaleActive_ || session_ == nullptr)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        auto* terrain = scene.terrains.GetComponent(terrainMaterialEntity_);
        if (terrain != nullptr)
        {
            bridge::SetTerrainTextureScale(terrainMaterialAfter_, value);
            const float before = terrainMaterialBefore_.slots[0].texMulAdd.x;
            const float after = terrainMaterialAfter_.slots[0].texMulAdd.x;
            if (std::abs(before - after) > 0.00001f)
            {
                bridge::ApplyTerrainMaterial(
                    scene,
                    *terrain,
                    terrainMaterialAfter_,
                    true);
                session_->Commands().RecordExecuted(
                    std::make_unique<bridge::SetTerrainMaterialCommand>(
                        scene,
                        terrainMaterialEntity_,
                        std::move(terrainMaterialBefore_),
                        std::move(terrainMaterialAfter_)));
            }
        }
        terrainTextureScaleActive_ = false;
        terrainMaterialEntity_ = wi::ecs::INVALID_ENTITY;
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ApplyDefaultGrass()
    {
        if (session_ == nullptr ||
            session_->Scenes().GetScene().terrains.GetCount() == 0)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        const auto entity = scene.terrains.GetEntity(0);
        const auto* terrain = scene.terrains.GetComponent(entity);
        if (terrain == nullptr)
        {
            return;
        }
        auto before = bridge::CaptureTerrainMaterial(scene, *terrain);
        auto after = bridge::MakeDefaultGrassMaterial(
            bridge::DefaultGrassTextureScale);
        session_->Commands().Execute(
            std::make_unique<bridge::SetTerrainMaterialCommand>(
                scene,
                entity,
                std::move(before),
                std::move(after)));
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ReloadTerrainMaterial()
    {
        if (session_ == nullptr ||
            session_->Scenes().GetScene().terrains.GetCount() == 0)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        auto* terrain = scene.terrains.GetComponent(
            scene.terrains.GetEntity(0));
        if (terrain != nullptr)
        {
            bridge::ReloadDefaultTerrainMaterial(scene, *terrain);
            terrainStrokeDiagnostic_.SetText("MATERIAL // FILES RELOADED");
        }
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ValidateModelImport()
    {
        if (session_ == nullptr ||
            session_->Projects().CurrentProject().rootPath.empty())
        {
            wi::helper::messageBox(
                "Open or create a Renegade project before running the model import proof.",
                "Model Import Gate 1");
            return;
        }

        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::OPEN;
        params.description = "GLB/GLTF model for Gate 1 validation";
        params.extensions.push_back("glb");
        params.extensions.push_back("gltf");
        wi::helper::FileDialog(
            params,
            [this](const std::string& sourcePath)
            {
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, sourcePath](uint64_t)
                    {
                        if (sourcePath.empty())
                        {
                            return;
                        }
                        if (wi::jobsystem::IsBusy(modelImportWorkload_))
                        {
                            wi::helper::messageBox(
                                "A model import validation is already running.",
                                "Model Import Gate 1");
                            return;
                        }

                        const fs::path source = fs::u8path(sourcePath);
                        studioChrome_.SetStatusText(
                            "MODEL IMPORT PROOF // RUNNING // " +
                            source.filename().u8string());
                        RunModelImportProof(sourcePath);
                    });
            });
    }

    void StudioRenderPath::RunModelImportProof(const std::string& sourcePath)
    {
        if (session_ == nullptr || sourcePath.empty())
        {
            return;
        }

        const fs::path outputDirectory =
            fs::u8path(session_->Projects().CurrentProject().rootPath) /
            "Saved" / "Validation" / "ModelImport";
        const fs::path source = fs::u8path(sourcePath);
        const fs::path assetPath =
            outputDirectory / fs::u8path(source.stem().u8string() + ".wiscene");

        const std::string destinationPath = assetPath.generic_u8string();
        wi::jobsystem::Execute(
            modelImportWorkload_,
            [this, sourcePath, destinationPath](wi::jobsystem::JobArgs)
            {
                auto prepared = std::make_shared<bridge::PreparedModelImport>(
                    bridge::ImportService().PrepareGltfAsset(
                        sourcePath,
                        destinationPath));
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, prepared](uint64_t)
                    {
                        auto result = bridge::ImportService().CompleteGltfAsset(
                            std::move(*prepared));
                        PresentModelImportProof(result);
                    });
            });
    }

    void StudioRenderPath::PresentModelImportProof(
        const bridge::ImportResult& result)
    {
        const auto* device = wi::graphics::GetDevice();
        const std::string renderer = device != nullptr &&
                device->GetShaderFormat() == wi::graphics::ShaderFormat::SPIRV
            ? "VULKAN"
            : "DX12";
        std::ostringstream report;
        report << (result.succeeded ? "PASS" : "FAIL")
            << " // MODEL IMPORT V1 GATE 1\n\n"
            << "Renderer: " << renderer << '\n'
            << "Source: " << result.sourcePath << '\n'
            << "WISCENE: " << result.assetPath << "\n\n";
        if (result.succeeded)
        {
            report << "Objects: " << result.reloaded.objects << '\n'
                << "Meshes: " << result.reloaded.meshes << '\n'
                << "Materials: " << result.reloaded.materials << '\n'
                << "Texture references: "
                << result.reloaded.textureReferences << '\n'
                << "Transforms: " << result.reloaded.transforms << '\n'
                << "Hierarchy links: " << result.reloaded.hierarchy << '\n'
                << "Armatures: " << result.reloaded.armatures << '\n'
                << "Animations: " << result.reloaded.animations << "\n\n"
                << "The isolated imported scene survived WISCENE save and reload unchanged.";
        }
        else
        {
            report << "Reason: " << result.error;
        }

        studioChrome_.SetStatusText(
            std::string("MODEL IMPORT PROOF // ") +
            (result.succeeded ? "PASS // " : "FAIL // ") + renderer);
        wi::helper::messageBox(report.str(), "Model Import Gate 1");
    }

    void StudioRenderPath::ImportModel()
    {
        if (session_ == nullptr ||
            session_->Projects().CurrentProject().rootPath.empty())
        {
            wi::helper::messageBox(
                "Open or create a Renegade project before importing a model.",
                "Import Model");
            return;
        }

        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::OPEN;
        params.description = "FBX/GLTF/GLB model to prepare in the Import Model workspace";
        params.extensions.push_back("fbx");
        params.extensions.push_back("glb");
        params.extensions.push_back("gltf");
        wi::helper::FileDialog(
            params,
            [this](const std::string& sourcePath)
            {
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, sourcePath](uint64_t)
                    {
                        if (sourcePath.empty())
                        {
                            return;
                        }
                        if (wi::jobsystem::IsBusy(modelImportWorkload_))
                        {
                            wi::helper::messageBox(
                                "A model import is already running.",
                                "Import Model");
                            return;
                        }

                        const fs::path source = fs::u8path(sourcePath);
                        studioChrome_.SetStatusText(
                            "IMPORT MODEL // CONVERTING // " +
                            source.filename().u8string());
                        RunModelImportPlacement(sourcePath);
                    });
            });
    }

    void StudioRenderPath::RunModelImportPlacement(
        const std::string& sourcePath)
    {
        if (session_ == nullptr || sourcePath.empty() ||
            !session_->Projects().HasProject() || creatorModelImporter.active ||
            creatorModelImporter.committing)
        {
            return;
        }

        struct PreviewPrepareState
        {
            std::string sourcePath;
            std::string projectRoot;
            bridge::PreparedModelImport prepared;
        };
        auto state = std::make_shared<PreviewPrepareState>();
        state->sourcePath = sourcePath;
        state->projectRoot = session_->Projects().CurrentProject().rootPath;

        wi::jobsystem::Execute(modelImportWorkload_,
            [this, state](wi::jobsystem::JobArgs)
            {
                const fs::path previewDirectory =
                    fs::u8path(state->projectRoot) / "Intermediate" / "Imports";
                std::error_code ec;
                fs::create_directories(previewDirectory, ec);
                bridge::ModelImportRequest request;
                request.sourcePath = state->sourcePath;
                request.assetPath = (previewDirectory / ".creator-preview.wiscene")
                    .generic_u8string();
                request.expectedFormat = bridge::ImportService::ClassifyModelSourceFormat(
                    state->sourcePath);
                state->prepared = bridge::ImportService().PrepareModelAsset(request);

                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, state](std::uint64_t)
                    {
                        if (session_ == nullptr || !state->prepared.IsReady())
                        {
                            const std::string error = state->prepared.Result().error.empty()
                                ? "Renegade could not prepare a visible preview."
                                : state->prepared.Result().error;
                            studioChrome_.SetStatusText("IMPORT MODEL // PREVIEW FAILED");
                            wi::helper::messageBox(error, "Import Model");
                            return;
                        }

                        auto& liveScene = session_->Scenes().GetScene();
                        const auto* isolated = state->prepared.PeekScene();
                        creatorModelImporter = {};
                        creatorModelImporter.active = true;
                        creatorModelImporter.sourcePath = state->sourcePath;
                        creatorModelImporter.undoBaseline = session_->Commands().UndoCount();
                        creatorModelImporter.cameraBefore = editorCameraTransform_;
                        creatorModelImporter.cameraFovBefore = camera->fov;
                        creatorModelImporter.cameraCaptured = true;
                        creatorModelImporter.summary = bridge::ImportService::Summarize(*isolated);
                        creatorModelImporter.evidence = bridge::ImportService::SummarizeModelEvidence(*isolated);
                        creatorModelImporter.sourceBounds = bridge::ImportService::MeasureModelBounds(*isolated);
                        creatorModelImporter.automaticScale = bridge::ImportService::ResolveScaleFactor(
                            bridge::ModelScaleMode::Automatic, *isolated);
                        creatorModelImporter.scale = XMFLOAT3(
                            creatorModelImporter.automaticScale,
                            creatorModelImporter.automaticScale,
                            creatorModelImporter.automaticScale);
                        creatorModelImporter.assetName = fs::u8path(state->sourcePath)
                            .stem().generic_u8string();
                        creatorModelImporter.destinationFolder = "Content/Models";
                        const auto detectedMaterials = bridge::DetectCreatorModelMaterials(
                            *isolated, state->sourcePath);
                        if (detectedMaterials.succeeded)
                            creatorModelImporter.materialOverrides = detectedMaterials.materials;

                        const std::size_t materialStart = liveScene.materials.GetCount();
                        const std::size_t animationStart = liveScene.animations.GetCount();

                        // Keep the one expensive conversion for governed commit.
                        // The live importer gets a Wicked prefab copy, so cancelling
                        // or editing the preview never consumes the retained source
                        // scene and Confirm never needs to invoke FBX/GLTF again.
                        std::string previewCloneError;
                        auto previewScene = CloneCreatorPreviewScene(
                            *state->prepared.PeekMutableScene(),
                            previewCloneError);
                        if (!previewScene.IsValid())
                        {
                            creatorModelImporter = {};
                            studioChrome_.SetStatusText(
                                "IMPORT MODEL // PREVIEW CLONE FAILED");
                            wi::helper::messageBox(
                                previewCloneError,
                                "Import Model");
                            return;
                        }
                        creatorModelImporter.preparedForCommit =
                            std::move(state->prepared);

                        auto place = std::make_unique<bridge::PlaceImportedModelCommand>(
                            liveScene,
                            std::move(previewScene),
                            XMFLOAT3(0.0f, CreatorImportStageHeight, 0.0f),
                            creatorModelImporter.automaticScale);
                        auto* placed = place.get();
                        if (!session_->Commands().Execute(std::move(place)))
                        {
                            creatorModelImporter = {};
                            studioChrome_.SetStatusText("IMPORT MODEL // PREVIEW PLACE FAILED");
                            return;
                        }
                        creatorModelImporter.previewRoot = placed->PlacedEntity();

                        for (std::size_t index = materialStart; index < liveScene.materials.GetCount(); ++index)
                            creatorModelImporter.materialEntities.push_back(liveScene.materials.GetEntity(index));
                        ApplyDetectedCreatorPreviewMaterials();
                        for (std::size_t index = animationStart; index < liveScene.animations.GetCount(); ++index)
                        {
                            const auto entity = liveScene.animations.GetEntity(index);
                            creatorModelImporter.animationEntities.push_back(entity);
                            const auto* animation = liveScene.animations.GetComponent(entity);
                            if (animation != nullptr)
                            {
                                bridge::CreatorAnimationImportRecipe clip;
                                clip.sourceAnimationIndex = static_cast<std::uint32_t>(index - animationStart);
                                clip.name = CreatorImportEntityName(
                                    liveScene, entity, "Animation " + std::to_string(index - animationStart + 1));
                                clip.start = animation->start;
                                clip.end = animation->end;
                                clip.enabled = true;
                                creatorModelImporter.animationRecipe.push_back(std::move(clip));
                            }
                        }

                        creatorModelImporter.weatherEntity = session_->Scenes().WeatherEntity();
                        creatorModelImporter.ambientBefore = liveScene.weather.ambient;
                        creatorModelImporter.ambientCaptured = true;
                        if (const auto* weather = liveScene.weathers.GetComponent(
                                creatorModelImporter.weatherEntity))
                        {
                            creatorModelImporter.ambientBefore = weather->ambient;
                        }

                        auto light = std::make_unique<bridge::CreateLightCommand>(
                            liveScene,
                            wi::scene::LightComponent::DIRECTIONAL,
                            XMFLOAT3(0.0f, CreatorImportStageHeight + 4.0f, 0.0f));
                        auto* lightRaw = light.get();
                        if (session_->Commands().Execute(std::move(light)))
                        {
                            creatorModelImporter.previewLight = lightRaw->CreatedEntity();
                            auto lightState = bridge::MakeNewLightState(wi::scene::LightComponent::DIRECTIONAL);
                            lightState.intensity = creatorModelImporter.lightIntensity;
                            lightState.castShadow = false;
                            session_->Commands().Execute(
                                std::make_unique<bridge::SetLightCommand>(
                                    liveScene, creatorModelImporter.previewLight, lightState));
                        }
                        ApplyCreatorImportPreviewLighting();

                        FrameCreatorImportPreviewCamera();

                        session_->Selection().Select(creatorModelImporter.previewRoot);
                        RefreshHierarchy();
                        RefreshInspector();
                        studioChrome_.SetVisible(false);
                        inspectorPanel_.SetVisible(false);
                        hierarchySearch_.SetVisible(false);
                        ShowImportScalePanel(
                            creatorModelImporter.previewRoot,
                            creatorModelImporter.automaticScale,
                            fs::u8path(state->sourcePath).filename().generic_u8string());
                    });
            });
    }

    void StudioRenderPath::ShowImportScalePanel(
        const wi::ecs::Entity entity,
        const float appliedScaleFactor,
        const std::string& sourceFileName)
    {
        importScaleTargetEntity_ = entity;
        importScaleAppliedFactor_ = appliedScaleFactor;
        pendingImportScaleMode_ = bridge::ModelScaleMode::Automatic;

        std::ostringstream readout;
        readout << sourceFileName
            << "\nMeshes: " << creatorModelImporter.summary.meshes
            << "   Materials: " << creatorModelImporter.summary.materials
            << "   Textures: " << creatorModelImporter.summary.textureReferences
            << "\nAnimations: " << creatorModelImporter.summary.animations
            << "   Bones: " << creatorModelImporter.evidence.armatureBones;
        importScaleReadoutLabel_.SetText(readout.str());
        importScaleModeCombo_.SetSelectedWithoutCallback(0);
        creatorModelImporter.workspaceSection = 0;
        creatorImportSectionCombo.SetSelectedWithoutCallback(0);
        creatorImportAssetName.SetValue(creatorModelImporter.assetName);
        creatorImportDestination.SetValue(creatorModelImporter.destinationFolder);
        creatorModelImporter.positionOffset = XMFLOAT3(0.0f, 0.0f, 0.0f);
        creatorModelImporter.rotationDegrees = XMFLOAT3(0.0f, 0.0f, 0.0f);
        creatorImportPositionX.SetValue(0.0f);
        creatorImportPositionY.SetValue(0.0f);
        creatorImportPositionZ.SetValue(0.0f);
        creatorImportRotationX.SetValue(0.0f);
        creatorImportRotationY.SetValue(0.0f);
        creatorImportRotationZ.SetValue(0.0f);
        creatorImportScaleX.SetValue(creatorModelImporter.scale.x);
        creatorImportScaleY.SetValue(creatorModelImporter.scale.y);
        creatorImportScaleZ.SetValue(creatorModelImporter.scale.z);
        creatorImportScaleLinked.SetCheck(creatorModelImporter.scaleLinked);
        creatorImportDimensionPreset.SetSelectedWithoutCallback(-1);

        creatorImportMaterialCombo.ClearItems();
        auto& scene = session_->Scenes().GetScene();
        for (std::size_t index = 0; index < creatorModelImporter.materialEntities.size(); ++index)
        {
            const auto entityId = creatorModelImporter.materialEntities[index];
            creatorImportMaterialCombo.AddItem(
                CreatorImportMaterialDisplayName(scene, entityId, index),
                static_cast<std::uint64_t>(index));
        }
        if (!creatorModelImporter.materialEntities.empty())
        {
            creatorImportMaterialCombo.SetSelectedWithoutCallback(0);
            creatorModelImporter.selectedMaterial = 0;
        }
        RefreshCreatorImportMaterialReadout();
        RefreshCreatorImportMaterialScalars();
        creatorImportTextureSlot = 0;
        creatorImportTextureSlotCombo.SetSelectedWithoutCallback(0);
        RefreshCreatorImportTextureEditor();

        creatorModelImporter.selectedAnimation = 0;
        RebuildCreatorImportAnimationCombo();
        creatorImportLightIntensity.SetValue(creatorModelImporter.lightIntensity);
        creatorImportLightAzimuth.SetValue(creatorModelImporter.lightAzimuth);
        creatorImportLightElevation.SetValue(creatorModelImporter.lightElevation);
        creatorImportAmbientBrightness.SetValue(creatorModelImporter.ambientBrightness);
        creatorImportLightingPreset.SetSelectedWithoutCallback(0);
        creatorImportMannequinVisible.SetCheck(creatorModelImporter.mannequinVisible);
        creatorModelImporter.thumbnailCapturePath.clear();
        creatorModelImporter.thumbnailCaptureRevision = 0;
        creatorImportThumbnailPreviewResource = {};
        creatorImportThumbnailPreview.SetImage(wi::Resource{});
        creatorImportThumbnailCapture.SetText("CAPTURE THUMBNAIL");
        creatorImportThumbnailStatus.SetText(
            "THUMBNAIL // CAPTURE, REVIEW SQUARE PREVIEW, RETAKE IF NEEDED");
        importScaleApplyButton_.SetEnabled(false);
        UpdateCreatorImportScaleReferenceLabel();
        importScalePanel_.SetVisible(true);
        importScalePanel_.scrollbar_vertical.SetOffset(0.0f);
        RefreshCreatorImportWorkspaceSection();
    }

    void StudioRenderPath::FrameCreatorImportPreviewCamera()
    {
        if (!creatorModelImporter.active || !creatorModelImporter.sourceBounds.valid)
            return;

        const auto& bounds = creatorModelImporter.sourceBounds;
        const XMFLOAT3 scale = creatorModelImporter.scale;
        const XMFLOAT3 center(
            (bounds.minimum.x + bounds.maximum.x) * 0.5f * scale.x,
            CreatorImportStageHeight +
                (bounds.minimum.y + bounds.maximum.y) * 0.5f * scale.y,
            (bounds.minimum.z + bounds.maximum.z) * 0.5f * scale.z);
        const XMFLOAT3 extents(
            std::abs(bounds.maximum.x - bounds.minimum.x) * scale.x,
            std::abs(bounds.maximum.y - bounds.minimum.y) * scale.y,
            std::abs(bounds.maximum.z - bounds.minimum.z) * scale.z);
        const float radius = std::max(
            0.25f,
            0.5f * std::sqrt(
                extents.x * extents.x +
                extents.y * extents.y +
                extents.z * extents.z));

        // A longer, neutral preview lens avoids the exaggerated near/far
        // proportions produced by the editor camera when it is placed close
        // to a character. Distance follows the measured, scaled bounds so a
        // boot, head or large prop cannot accidentally fill the near plane.
        camera->fov = CreatorImportPreviewFov;
        const float distance = std::max(
            2.5f,
            radius / std::sin(CreatorImportPreviewFov * 0.5f) * 1.2f);
        const XMVECTOR target = XMLoadFloat3(&center);
        const XMVECTOR viewDirection = XMVector3Normalize(
            XMVectorSet(0.32f, 0.12f, -1.0f, 0.0f));
        const XMVECTOR eye = target + viewDirection * distance;
        const XMMATRIX view = XMMatrixLookAtLH(
            eye, target, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
        editorCameraTransform_.ClearTransform();
        editorCameraTransform_.MatrixTransform(XMMatrixInverse(nullptr, view));
        editorCameraTransform_.UpdateTransform();
        camera->TransformCamera(editorCameraTransform_);
        camera->UpdateCamera();
    }

    void StudioRenderPath::RefreshCreatorImportWorkspaceSection()
    {
        const std::size_t section = creatorModelImporter.workspaceSection;
        creatorImportAssetName.SetVisible(section == 0 || section == 5);
        creatorImportDestination.SetVisible(section == 0 || section == 5);

        for (wi::gui::Widget* widget : {
            static_cast<wi::gui::Widget*>(&creatorImportTransformLabel),
            static_cast<wi::gui::Widget*>(&creatorImportPositionX),
            static_cast<wi::gui::Widget*>(&creatorImportPositionY),
            static_cast<wi::gui::Widget*>(&creatorImportPositionZ),
            static_cast<wi::gui::Widget*>(&creatorImportRotationX),
            static_cast<wi::gui::Widget*>(&creatorImportRotationY),
            static_cast<wi::gui::Widget*>(&creatorImportRotationZ),
            static_cast<wi::gui::Widget*>(&creatorImportScaleX),
            static_cast<wi::gui::Widget*>(&creatorImportScaleY),
            static_cast<wi::gui::Widget*>(&creatorImportScaleZ),
            static_cast<wi::gui::Widget*>(&creatorImportScaleLinked),
            static_cast<wi::gui::Widget*>(&creatorImportDimensionPreset),
            static_cast<wi::gui::Widget*>(&importScaleModeCombo_)})
            widget->SetVisible(section == 1);

        for (wi::gui::Widget* widget : {
            static_cast<wi::gui::Widget*>(&creatorImportMaterialLabel),
            static_cast<wi::gui::Widget*>(&creatorImportMaterialCombo),
            static_cast<wi::gui::Widget*>(&creatorImportMaterialReadout),
            static_cast<wi::gui::Widget*>(&creatorImportTexturePreviews),
            static_cast<wi::gui::Widget*>(&creatorImportTextureHelp),
            static_cast<wi::gui::Widget*>(&creatorImportTextureSlotCombo),
            static_cast<wi::gui::Widget*>(&creatorImportTexturePath),
            static_cast<wi::gui::Widget*>(&creatorImportTextureBrowse),
            static_cast<wi::gui::Widget*>(&creatorImportTextureClear),
            static_cast<wi::gui::Widget*>(&creatorImportMaterialScalarLabel),
            static_cast<wi::gui::Widget*>(&creatorImportRoughness),
            static_cast<wi::gui::Widget*>(&creatorImportMetalness),
            static_cast<wi::gui::Widget*>(&creatorImportReflectance),
            static_cast<wi::gui::Widget*>(&creatorImportNormalStrength),
            static_cast<wi::gui::Widget*>(&creatorImportAoStrength),
            static_cast<wi::gui::Widget*>(&creatorImportEmissiveStrength)})
            widget->SetVisible(section == 2);

        for (wi::gui::Widget* widget : {
            static_cast<wi::gui::Widget*>(&creatorImportLightingLabel),
            static_cast<wi::gui::Widget*>(&creatorImportLightIntensity),
            static_cast<wi::gui::Widget*>(&creatorImportLightAzimuth),
            static_cast<wi::gui::Widget*>(&creatorImportLightElevation),
            static_cast<wi::gui::Widget*>(&creatorImportAmbientBrightness),
            static_cast<wi::gui::Widget*>(&creatorImportLightingPreset),
            static_cast<wi::gui::Widget*>(&creatorImportLightingReset),
            static_cast<wi::gui::Widget*>(&creatorImportMannequinVisible)})
            widget->SetVisible(section == 3);

        for (wi::gui::Widget* widget : {
            static_cast<wi::gui::Widget*>(&creatorImportAnimationLabel),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationCombo),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationName),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationStart),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationEnd),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationEnabled),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationAdd),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationDelete),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationReadout)})
            widget->SetVisible(section == 4);

        for (wi::gui::Widget* widget : {
            static_cast<wi::gui::Widget*>(&creatorImportActionBar),
            static_cast<wi::gui::Widget*>(&creatorImportThumbnailPreview),
            static_cast<wi::gui::Widget*>(&creatorImportThumbnailCapture),
            static_cast<wi::gui::Widget*>(&creatorImportThumbnailStatus),
            static_cast<wi::gui::Widget*>(&importScaleApplyButton_),
            static_cast<wi::gui::Widget*>(&importScaleDismissButton_)})
            widget->SetVisible(section == 5);
    }

    void StudioRenderPath::CaptureCreatorImportThumbnail()
    {
        if (session_ == nullptr || !creatorModelImporter.active ||
            creatorModelImporter.committing ||
            creatorModelImporter.thumbnailCapturePending ||
            !session_->Projects().HasProject())
            return;

        const bool firstThumbnailCapture =
            creatorModelImporter.thumbnailCapturePath.empty();
        BeginCreatorThumbnailPresentation();
        if (firstThumbnailCapture)
            FrameCreatorImportPreviewCamera();

        const fs::path directory =
            fs::u8path(session_->Projects().CurrentProject().rootPath) /
            "Intermediate" / "Imports";
        std::error_code ec;
        fs::create_directories(directory, ec);
        if (ec)
        {
            RestoreCreatorThumbnailPresentation();
            creatorImportThumbnailStatus.SetText(
                "THUMBNAIL FAILED // CANNOT CREATE IMPORT CACHE");
            return;
        }

        ++creatorModelImporter.thumbnailCaptureRevision;
        const fs::path capturePath = directory /
            fs::u8path(".creator-asset-thumbnail-" +
                std::to_string(creatorModelImporter.thumbnailCaptureRevision) +
                ".png");
        fs::remove(capturePath, ec);
        creatorModelImporter.thumbnailCapturePath =
            capturePath.generic_u8string();
        creatorModelImporter.thumbnailCapturePending = true;
        creatorImportThumbnailStatus.SetText(
            "CAPTURING SQUARE AUTO-FRAMED ASSET...");
        importScaleApplyButton_.SetEnabled(false);
    }

    void StudioRenderPath::ApplyImportScaleMode(
        const bridge::ModelScaleMode)
    {
        if (session_ == nullptr || !creatorModelImporter.active ||
            creatorModelImporter.committing)
            return;
        if (creatorModelImporter.thumbnailCapturePending ||
            creatorModelImporter.thumbnailCapturePath.empty() ||
            !fs::exists(fs::u8path(creatorModelImporter.thumbnailCapturePath)))
        {
            creatorImportThumbnailStatus.SetText(
                "CAPTURE A THUMBNAIL BEFORE CONFIRMING");
            importScaleApplyButton_.SetEnabled(false);
            return;
        }
        if (!creatorModelImporter.preparedForCommit.IsReady())
        {
            studioChrome_.SetStatusText("IMPORT MODEL // RETAINED PREVIEW LOST");
            wi::helper::messageBox(
                "The importer lost the already-converted model scene. The project was not changed.",
                "Import Model");
            return;
        }

        auto& liveScene = session_->Scenes().GetScene();
        if (liveScene.transforms.GetComponent(creatorModelImporter.previewRoot) == nullptr)
        {
            DismissImportScalePanel();
            return;
        }

        const auto& project = session_->Projects().CurrentProject();
        bridge::CreatorAssetWorkflowService workflow;
        std::string destinationError;
        if (!workflow.ValidateModelImportDestination(
                project.rootPath,
                creatorModelImporter.sourcePath,
                creatorModelImporter.assetName,
                creatorModelImporter.destinationFolder,
                destinationError))
        {
            creatorImportThumbnailStatus.SetText(
                "IMPORT BLOCKED // PREFLIGHT FAILED");
            studioChrome_.SetStatusText(
                "IMPORT MODEL // DESTINATION PREFLIGHT FAILED");
            wi::helper::messageBox(destinationError.c_str(), "Import Model");
            return;
        }

        struct GovernedCommitState
        {
            std::string projectRoot;
            bridge::StableId projectId;
            std::string sourcePath;
            std::string assetName;
            std::string destinationFolder;
            std::string thumbnailCapturePath;
            std::string thumbnailError;
            std::vector<bridge::CreatorMaterialSourceOverride> materialOverrides;
            std::vector<bridge::CreatorAnimationImportRecipe> animationRecipe;
            XMFLOAT3 positionOffset = XMFLOAT3(0.0f, 0.0f, 0.0f);
            XMFLOAT3 rotationDegrees = XMFLOAT3(0.0f, 0.0f, 0.0f);
            XMFLOAT3 authoredScale = XMFLOAT3(1.0f, 1.0f, 1.0f);
            std::string settingsJson = "{}";
            bridge::PreparedModelImport prepared;
            bridge::CreatorModelImportResult imported;
            bridge::PreparedReusableModelPlacement warmedPlacement;
            double materialsSeconds = 0.0;
            double packageSeconds = 0.0;
        };

        auto state = std::make_shared<GovernedCommitState>();
        state->projectRoot = project.rootPath;
        state->projectId = project.projectId;
        state->sourcePath = creatorModelImporter.sourcePath;
        state->assetName = creatorModelImporter.assetName;
        state->destinationFolder = creatorModelImporter.destinationFolder;
        state->thumbnailCapturePath = creatorModelImporter.thumbnailCapturePath;
        state->materialOverrides = creatorModelImporter.materialOverrides;
        state->animationRecipe = creatorModelImporter.animationRecipe;
        state->positionOffset = creatorModelImporter.positionOffset;
        state->rotationDegrees = creatorModelImporter.rotationDegrees;
        state->authoredScale = creatorModelImporter.scale;
        state->prepared = std::move(creatorModelImporter.preparedForCommit);

        const auto cameraBefore = creatorModelImporter.cameraBefore;
        const float cameraFovBefore = creatorModelImporter.cameraFovBefore;
        const std::size_t undoBaseline = creatorModelImporter.undoBaseline;
        creatorModelImporter.committing = true;

        RestoreCreatorImportPreviewEnvironment();
        while (session_->Commands().UndoCount() > undoBaseline)
        {
            if (!session_->Commands().Undo())
                break;
        }
        importScalePanel_.SetEnabled(false);
        importScaleApplyButton_.SetText("PROCESSING...");
        importScaleApplyButton_.SetEnabled(false);
        importScaleDismissButton_.SetEnabled(false);
        creatorImportThumbnailCapture.SetEnabled(false);
        creatorImportThumbnailStatus.SetText(
            "PROCESSING // MATERIALS + GOVERNED TEXTURES");
        editorCameraTransform_ = cameraBefore;
        editorCameraTransform_.UpdateTransform();
        camera->fov = cameraFovBefore;
        camera->TransformCamera(editorCameraTransform_);
        camera->UpdateCamera();
        ClearSelectionOutline();
        session_->Selection().Clear();

        // Keep the importer visibly open while the governed transaction runs.
        // The user sees explicit phases instead of an apparently idle Studio.
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        studioChrome_.SetStatusText(
            "IMPORT MODEL // PROCESSING // MATERIALS + GOVERNED TEXTURES");

        wi::jobsystem::Execute(modelImportWorkload_,
            [this, state](wi::jobsystem::JobArgs)
            {
                if (!state->prepared.IsReady() || state->prepared.PeekScene() == nullptr)
                {
                    state->imported.error =
                        "The retained prepared model scene is unavailable.";
                }
                else
                {
                    bridge::CreatorModelMaterialPreparationRequest materialRequest;
                    materialRequest.preparedScene = state->prepared.PeekScene();
                    materialRequest.projectRoot = state->projectRoot;
                    materialRequest.projectId = state->projectId;
                    materialRequest.modelSourcePath = state->sourcePath;
                    materialRequest.overrides = state->materialOverrides;
                    const auto materialsStarted = std::chrono::steady_clock::now();
                    auto materials = bridge::PrepareCreatorModelMaterials(materialRequest);
                    state->materialsSeconds = std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - materialsStarted).count();
                    if (materials.succeeded)
                    {
                        materials.recipe.animations = state->animationRecipe;
                        materials.recipe.transform.authored = true;
                        materials.recipe.transform.positionX = state->positionOffset.x;
                        materials.recipe.transform.positionY = state->positionOffset.y;
                        materials.recipe.transform.positionZ = state->positionOffset.z;
                        materials.recipe.transform.rotationXDegrees = state->rotationDegrees.x;
                        materials.recipe.transform.rotationYDegrees = state->rotationDegrees.y;
                        materials.recipe.transform.rotationZDegrees = state->rotationDegrees.z;
                        materials.recipe.transform.scaleX = state->authoredScale.x;
                        materials.recipe.transform.scaleY = state->authoredScale.y;
                        materials.recipe.transform.scaleZ = state->authoredScale.z;
                        std::string recipeError;
                        if (!bridge::SerializeCreatorModelImportOptions(
                                materials.recipe, state->settingsJson, recipeError))
                        {
                            state->imported.error = recipeError;
                        }
                    }
                    else
                    {
                        state->imported.error = materials.error;
                    }
                }

                bridge::CreatorAssetWorkflowService workflow;
                if (state->imported.error.empty())
                {
                    wi::eventhandler::Subscribe_Once(
                        wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                        [this](std::uint64_t)
                        {
                            if (creatorModelImporter.committing)
                            {
                                creatorImportThumbnailStatus.SetText(
                                    "PROCESSING // WRITING RASSET PACKAGE");
                                studioChrome_.SetStatusText(
                                    "IMPORT MODEL // PROCESSING // WRITING RASSET PACKAGE");
                            }
                        });
                    const auto packageStarted = std::chrono::steady_clock::now();
                    state->imported = workflow.ImportModel(
                        state->projectRoot,
                        state->projectId,
                        state->sourcePath,
                        state->settingsJson,
                        state->assetName,
                state->destinationFolder,
                std::move(state->prepared),
                state->thumbnailCapturePath,
                &state->warmedPlacement);
                    state->packageSeconds = std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - packageStarted).count();
}
wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, state](std::uint64_t)
                    {
                        importScalePanel_.SetEnabled(true);
                        importScalePanel_.SetVisible(false);
                        studioChrome_.SetVisible(true);
                        inspectorPanel_.SetVisible(true);
                        hierarchySearch_.SetVisible(true);
                        importScaleApplyButton_.SetText("CONFIRM IMPORT");
                        importScaleApplyButton_.SetEnabled(true);
                        importScaleDismissButton_.SetEnabled(true);
                        creatorImportThumbnailCapture.SetEnabled(true);
                        creatorModelImporter = {};
                        importScaleTargetEntity_ = wi::ecs::INVALID_ENTITY;

                        if (!state->imported.succeeded)
                        {
                            studioChrome_.SetStatusText("IMPORT MODEL // COMMIT FAILED");
                            wi::helper::messageBox(
                                "The preview was discarded safely, but the governed asset could not be committed.\n\nReason: " +
                                    state->imported.error,
                                "Import Model");
                            return;
                        }
                        RefreshHierarchy();
                        RefreshInspector();
                        RefreshStatus();
                        assetBrowserCurrentFolder_ = fs::u8path(
                            state->imported.assetProjectRelativePath)
                            .parent_path().lexically_normal().generic_u8string();
                        const bool instantPlacementReady =
                            state->warmedPlacement.IsReady();
                        if (instantPlacementReady)
                        {
                            detail::PrimeCreatorAssetDragPreparation(
                                state->imported.asset.assetId,
                                state->imported.assetProjectRelativePath,
                                std::move(state->warmedPlacement));
                        }
                        studioChrome_.SetActiveBottomTab(0, true);
                        std::string browserError;
                        if (!studioChrome_.RevealCreatorAsset(
                                state->imported.asset.assetId,
                                state->imported.assetProjectRelativePath,
                                browserError))
                        {
                            studioChrome_.SetStatusText(
                                "IMPORT MODEL // ASSET COMMITTED // BROWSER FAILED");
                            wi::helper::messageBox(
                                "The governed asset was committed, but Studio could not verify it in the Asset Browser. Do not import it again.\n\nAsset: " +
                                    state->imported.assetProjectRelativePath +
                                    "\n\nReason: " + browserError,
                                "Import Model");
                            return;
                        }
                        std::ostringstream completed;
                        completed << std::fixed << std::setprecision(1)
                            << "IMPORT MODEL // READY // MATERIALS "
                            << state->materialsSeconds << "s // PACKAGE "
                            << state->packageSeconds << "s // "
                            << fs::u8path(state->imported.assetProjectRelativePath)
                                .filename().generic_u8string();
                        completed << (instantPlacementReady
                            ? " // INSTANT PLACEMENT READY"
                            : " // PLACEMENT CACHE WARNING");
                        studioChrome_.SetStatusText(completed.str());
                    });
            });
    }

    void StudioRenderPath::DismissImportScalePanel()
    {
        importScalePanel_.SetVisible(false);
        if (session_ != nullptr && creatorModelImporter.active)
        {
            RestoreCreatorImportPreviewEnvironment();
            while (session_->Commands().UndoCount() > creatorModelImporter.undoBaseline)
            {
                if (!session_->Commands().Undo())
                    break;
            }
            if (creatorModelImporter.cameraCaptured)
            {
                editorCameraTransform_ = creatorModelImporter.cameraBefore;
                editorCameraTransform_.UpdateTransform();
                camera->fov = creatorModelImporter.cameraFovBefore;
                camera->TransformCamera(editorCameraTransform_);
                camera->UpdateCamera();
            }
            session_->Selection().Clear();
            ClearSelectionOutline();
            RefreshHierarchy();
            RefreshInspector();
            RefreshStatus();
        }
        creatorModelImporter = {};
        importScaleTargetEntity_ = wi::ecs::INVALID_ENTITY;
        studioChrome_.SetVisible(true);
        inspectorPanel_.SetVisible(true);
        hierarchySearch_.SetVisible(true);
        studioChrome_.SetStatusText("IMPORT MODEL // CANCELLED // PROJECT UNCHANGED");
    }

    std::string StudioRenderPath::ResolveTestLevelRuntimePath() const
    {
        // fs::current_path() is not reliable here: common Windows file-open
        // dialogs (Open Project, Open Scene, etc.) are documented to change
        // the calling process's working directory as a side effect, and
        // once that happens every candidate below silently resolves against
        // the wrong root - RenegadeRuntime.exe still exists exactly where
        // it always did, but this lookup would no longer find it. Anchor to
        // this process's own executable path instead, which cannot drift.
        wchar_t modulePath[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0)
        {
            return {};
        }
        const fs::path workingDirectory = fs::path(modulePath).parent_path();

        std::vector<fs::path> candidates = {
            workingDirectory / "Runtime" / "RenegadeRuntime.exe",
            workingDirectory / "RenegadeRuntime.exe",
        };

        const fs::path configuration = workingDirectory.filename();
        const fs::path buildRoot = workingDirectory.parent_path().parent_path();
        if (!configuration.empty() && !buildRoot.empty())
        {
            candidates.push_back(
                buildRoot / "Runtime" / configuration / "RenegadeRuntime.exe");
        }

        for (const auto& candidate : candidates)
        {
            std::error_code pathError;
            if (fs::is_regular_file(candidate, pathError) && !pathError)
            {
                return candidate.lexically_normal().generic_u8string();
            }
        }
        return {};
    }

    std::string StudioRenderPath::TestLevelBackendArgument() const
    {
        const auto* device = wi::graphics::GetDevice();
        if (device != nullptr && std::string(device->GetTag()) == "[Vulkan]")
        {
            return "vulkan";
        }
        return "dx12";
    }

    void StudioRenderPath::StartTestLevel()
    {
        projectPreviewActive_ = false;
        if (session_ == nullptr || !session_->Projects().HasProject())
        {
            wi::helper::messageBox(
                "Open or create a Renegade project before starting Test Level.",
                "Test Level");
            return;
        }
        if (testLevelRuntime_.IsActive())
        {
            return;
        }

        bridge::TestLevelSnapshotService snapshotService(
            session_->Scenes(),
            session_->Commands());
        bridge::TestLevelSnapshot snapshot;
        std::string error;
        ClearSelectionOutline();
        const bool snapshotCreated = snapshotService.Create(
            session_->Projects().CurrentProject(),
            snapshot,
            error);
        SyncSelectionOutline();
        if (!snapshotCreated)
        {
            studioChrome_.SetTestLevelState(
                RenegadeStudioChrome::TestLevelState::Idle);
            studioChrome_.SetStatusText("TEST LEVEL // SNAPSHOT FAILED");
            wi::helper::messageBox(
                "Renegade could not create the Test Level snapshot.\n\n" +
                    error,
                "Test Level");
            return;
        }

        const std::string runtimePath = ResolveTestLevelRuntimePath();
        if (runtimePath.empty())
        {
            std::string cleanupError;
            snapshotService.Cleanup(snapshot, cleanupError);
            studioChrome_.SetTestLevelState(
                RenegadeStudioChrome::TestLevelState::Idle);
            studioChrome_.SetStatusText("TEST LEVEL // RUNTIME NOT FOUND");

            std::string message =
                "RenegadeRuntime.exe was not found beside this Studio build.";
            if (!cleanupError.empty())
            {
                message += "\n\nSnapshot cleanup warning: " + cleanupError;
            }
            wi::helper::messageBox(message, "Test Level");
            return;
        }

        TestLevelLaunchOptions options;
        options.executablePath = runtimePath;
        options.workingDirectory =
            fs::u8path(runtimePath).parent_path().generic_u8string();
        options.arguments = {
            TestLevelBackendArgument(),
            "--project",
            snapshot.descriptorPath,
        };
        options.startupTimeout = std::chrono::milliseconds(60000);

        if (!testLevelRuntime_.Launch(
                std::move(options),
                std::move(snapshot),
                error))
        {
            studioChrome_.SetTestLevelState(
                RenegadeStudioChrome::TestLevelState::Idle);
            studioChrome_.SetStatusText("TEST LEVEL // LAUNCH FAILED");
            wi::helper::messageBox(
                "Renegade could not launch Test Level.\n\n" + error,
                "Test Level");
            return;
        }

        studioChrome_.SetTestLevelState(
            RenegadeStudioChrome::TestLevelState::Starting);
        studioChrome_.SetStatusText(
            "TEST LEVEL // STARTING // UNSAVED SNAPSHOT");
    }

    void StudioRenderPath::StartProjectPlay()
    {
        projectPreviewActive_ = false;
        if (session_ == nullptr || !session_->Projects().HasProject())
        {
            wi::helper::messageBox(
                "Open or create a Renegade project before previewing Story Flow.",
                "Story Flow Preview");
            return;
        }
        if (testLevelRuntime_.IsActive())
            return;

        const std::string runtimePath = ResolveTestLevelRuntimePath();
        if (runtimePath.empty())
        {
            studioChrome_.SetStatusText("STORY FLOW PREVIEW // RUNTIME NOT FOUND");
            wi::helper::messageBox(
                "RenegadeRuntime.exe was not found beside this Studio build.",
                "Story Flow Preview");
            return;
        }

        const auto& project = session_->Projects().CurrentProject();
        TestLevelLaunchOptions options;
        options.executablePath = runtimePath;
        options.workingDirectory =
            fs::u8path(runtimePath).parent_path().generic_u8string();
        options.arguments = {
            TestLevelBackendArgument(),
            "--project",
            project.descriptorPath,
        };
        options.startupTimeout = std::chrono::milliseconds(60000);
        options.ownsSnapshot = false;
        projectPreviewActive_ = true;

        bridge::TestLevelSnapshot noSnapshot;
        std::string error;
        if (!testLevelRuntime_.Launch(
                std::move(options), std::move(noSnapshot), error))
        {
            projectPreviewActive_ = false;
            studioChrome_.SetStatusText("STORY FLOW PREVIEW // LAUNCH FAILED");
            wi::helper::messageBox(
                "Renegade could not launch Story Flow Preview.\n\n" + error,
                "Story Flow Preview");
            return;
        }

        studioChrome_.SetTestLevelState(
            RenegadeStudioChrome::TestLevelState::Starting);
        studioChrome_.SetStatusText("STORY FLOW PREVIEW // STARTING");
    }

    void StudioRenderPath::PollTestLevel()
    {
        if (!testLevelRuntime_.IsActive())
        {
            return;
        }

        const TestLevelProcessResult result = testLevelRuntime_.Poll();
        if (result.state == TestLevelProcessState::Running)
        {
            studioChrome_.SetTestLevelState(
                RenegadeStudioChrome::TestLevelState::Running);
            studioChrome_.SetStatusText(projectPreviewActive_
                ? "STORY FLOW PREVIEW // RUNNING"
                : "TEST LEVEL // RUNNING // UNSAVED SNAPSHOT");
            return;
        }
        if (!result.finished)
        {
            studioChrome_.SetTestLevelState(
                RenegadeStudioChrome::TestLevelState::Starting);
            return;
        }

        studioChrome_.SetTestLevelState(
            RenegadeStudioChrome::TestLevelState::Idle);
        if (result.succeeded)
        {
            studioChrome_.SetStatusText(projectPreviewActive_
                ? "STORY FLOW PREVIEW // COMPLETED"
                : "TEST LEVEL // COMPLETED");
            projectPreviewActive_ = false;
            return;
        }

        const bool wasProjectPreview = projectPreviewActive_;
        studioChrome_.SetStatusText(wasProjectPreview
            ? "STORY FLOW PREVIEW // FAILED"
            : "TEST LEVEL // FAILED");
        projectPreviewActive_ = false;
        std::string message = result.message.empty()
            ? (wasProjectPreview
                ? "The Story Flow Preview Runtime stopped before it became ready."
                : "The Test Level Runtime stopped before it became ready.")
            : result.message;
        if (!result.warning.empty())
        {
            message += "\n\nWarning: " + result.warning;
        }
        wi::helper::messageBox(message,
            wasProjectPreview ? "Story Flow Preview" : "Test Level");
    }

    void StudioRenderPath::StopTestLevel()
    {
        if (!testLevelRuntime_.IsActive())
        {
            studioChrome_.SetTestLevelState(
                RenegadeStudioChrome::TestLevelState::Idle);
            return;
        }

        const bool wasProjectPreview = projectPreviewActive_;
        const TestLevelProcessResult result = testLevelRuntime_.Stop();
        projectPreviewActive_ = false;
        studioChrome_.SetTestLevelState(
            RenegadeStudioChrome::TestLevelState::Idle);
        studioChrome_.SetStatusText(wasProjectPreview
            ? "STORY FLOW PREVIEW // STOPPED"
            : (result.cleanupSucceeded
                ? "TEST LEVEL // STOPPED // SNAPSHOT CLEAN"
                : "TEST LEVEL // STOPPED // CLEANUP WARNING"));
        if (!result.warning.empty())
        {
            wi::helper::messageBox(result.warning,
                wasProjectPreview ? "Story Flow Preview" : "Test Level");
        }
    }

    void StudioRenderPath::RestoreGovernedMaterialTextures()
    {
        if (session_ == nullptr || !session_->Projects().HasProject())
            return;

        const auto& project = session_->Projects().CurrentProject();
        const auto restored = bridge::RestoreMaterialTextureBindings(
            session_->Scenes().GetScene(), project.rootPath, project.projectId);
        if (!restored.succeeded)
        {
            studioChrome_.SetStatusText(
                "TEXTURE BINDING // RESTORE WARNING // " + restored.error);
        }
        else if (restored.restored > 0)
        {
            studioChrome_.SetStatusText(
                "TEXTURE BINDING // RESTORED " +
                std::to_string(restored.restored) +
                " GOVERNED MATERIAL TEXTURE");
        }
    }

    void StudioRenderPath::RequestProjectHubFromStoryFlow()
    {
        pendingAction_ = EditorAction::ProjectHub;
    }

    void StudioRenderPath::RequestAssetBrowserFromStoryFlow()
    {
        RefreshAssetBrowser();
    }

    void StudioRenderPath::RequestProjectPlayFromStoryFlow()
    {
        pendingAction_ = EditorAction::StartProjectPlay;
    }

    void StudioRenderPath::RefreshAssetBrowser()
    {
        std::vector<RenegadeStudioChrome::AssetFolderRow> folders;
        std::vector<RenegadeStudioChrome::AssetCard> assets;

        if (session_ == nullptr || !session_->Projects().HasProject())
        {
            studioChrome_.SetAssetBrowserData(
                std::move(folders),
                std::move(assets),
                "NO PROJECT");
            return;
        }

        const auto snapshot = assetBrowserService_.Scan(
            session_->Projects().CurrentProject().rootPath,
            assetBrowserCurrentFolder_);
        if (!snapshot.succeeded)
        {
            studioChrome_.SetAssetBrowserData(
                std::move(folders),
                std::move(assets),
                "CONTENT UNAVAILABLE");
            studioChrome_.SetStatusText(
                "ASSET BROWSER // " + snapshot.error);
            return;
        }

        assetBrowserCurrentFolder_ = snapshot.currentFolder;
        folders.reserve(snapshot.folders.size());
        for (const auto& folder : snapshot.folders)
        {
            RenegadeStudioChrome::AssetFolderRow row;
            row.name = folder.name;
            row.relativePath = folder.projectRelativePath;
            row.depth = static_cast<int>(folder.depth);
            row.selected = folder.selected;
            folders.push_back(std::move(row));
        }

        assets.reserve(snapshot.assets.size());
        for (const auto& asset : snapshot.assets)
        {
            RenegadeStudioChrome::AssetCard card;
            card.name = asset.name;
            card.relativePath = asset.projectRelativePath;
            card.typeLabel =
                bridge::AssetBrowserService::TypeLabel(asset.type);
            card.directory = asset.directory;
            if (!asset.directory)
            {
                fs::path thumbnailPath =
                    fs::u8path(session_->Projects().CurrentProject().rootPath) /
                    fs::u8path(asset.projectRelativePath);
                thumbnailPath.replace_extension(".thumbnail.png");
                if (fs::exists(thumbnailPath))
                    card.thumbnail = wi::resourcemanager::Load(
                        thumbnailPath.generic_u8string());
            }
            assets.push_back(std::move(card));
        }

        studioChrome_.SetAssetBrowserData(
            std::move(folders),
            std::move(assets),
            snapshot.currentFolder);
        studioChrome_.SetStatusText(
            "ASSET BROWSER // " + snapshot.currentFolder);
    }

    void StudioRenderPath::SelectAssetBrowserFolder(
        const std::string& relativePath)
    {
        if (relativePath.empty())
        {
            return;
        }
        assetBrowserCurrentFolder_ = relativePath;
        RefreshAssetBrowser();
    }

    void StudioRenderPath::SelectAssetBrowserItem(
        const std::string& relativePath)
    {
        if (session_ == nullptr || relativePath.empty())
        {
            return;
        }

        const fs::path absolute =
            fs::u8path(session_->Projects().CurrentProject().rootPath) /
            fs::u8path(relativePath);
        if (fs::is_directory(absolute))
        {
            SelectAssetBrowserFolder(relativePath);
            return;
        }

        // V1 deliberately stops at real project browsing and selection.
        // Type-specific open/place/apply and drag payloads are the next slice.
        studioChrome_.SetStatusText(
            "ASSET SELECTED // " + relativePath);
    }

    void StudioRenderPath::CreateProject()
    {
        if (session_ == nullptr)
        {
            return;
        }

        const std::string projectName = hubNewProjectNameInput_.GetText();
        const std::string parentDirectory = wi::helper::FolderDialog(
            "Select the folder that will contain the new Renegade project.");
        if (parentDirectory.empty())
        {
            return;
        }

        wi::eventhandler::Subscribe_Once(
            wi::eventhandler::EVENT_THREAD_SAFE_POINT,
            [this, parentDirectory, projectName](uint64_t)
            {
                RequestSceneReplacement(
                    [this, parentDirectory, projectName]()
                    {
                        if (!session_->Projects().CreateStoryFlowProject(
                                parentDirectory,
                                projectName))
                        {
                            hubMessageLabel_.font.params.color = WarningAmber;
                            hubMessageLabel_.SetText(
                                "PROJECT CREATE FAILED // " +
                                session_->Projects().LastError());
                            return;
                        }

                        ClearSelectionOutline();
                        if (!session_->CommitPendingProjectWithoutScene())
                        {
                            hubMessageLabel_.font.params.color = WarningAmber;
                            hubMessageLabel_.SetText(
                                "PROJECT HOME FAILED // " +
                                session_->Scenes().LastError());
                            return;
                        }

                        workspaceTitle_.SetText(
                            "RENEGADE STUDIO // " +
                            session_->Projects().CurrentProject().name);
                        hubMessageLabel_.font.params.color = HologramMuted;
                        hubMessageLabel_.SetText(
                            "PROJECT CREATED // " +
                            session_->Projects().CurrentProject().descriptorPath);
                        selectedRecentProject_ = -1;
                        RefreshProjectHub();
                        SetProjectHubVisible(false);
                    });
            });
    }

    void StudioRenderPath::OpenProject()
    {
        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::OPEN;
        params.description = "Renegade Project (.renegade)";
        params.extensions.push_back("renegade");
        wi::helper::FileDialog(
            params,
            [this](const std::string& descriptorPath)
            {
                if (descriptorPath.empty())
                    return;
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, descriptorPath](uint64_t)
                    {
                        RequestSceneReplacement(
                            [this, descriptorPath]()
                            {
                                hubMessageLabel_.font.params.color = HologramMuted;
                                hubMessageLabel_.SetText(
                                    "PROJECT OPENING // " +
                                    wi::helper::GetFileNameFromPath(descriptorPath));
                                OpenProjectDescriptor(descriptorPath);
                            });
                    });
            });
    }

    void StudioRenderPath::RequestSceneReplacement(
        std::function<void()> continuation)
    {
        if (session_ == nullptr || !continuation)
        {
            return;
        }

        // A running preview is a real pending scene edit. Commit it first so
        // the dirty-state question includes what the creator can currently
        // see in the viewport.
        StopSunPreview(true);
        if (!session_->Commands().IsDirty())
        {
            continuation();
            return;
        }

        const std::string currentPath = session_->Scenes().CurrentPath();
        const std::string sceneName = currentPath.empty()
            ? "the current scene"
            : "\"" + wi::helper::GetFileNameFromPath(currentPath) + "\"";
        const auto result = wi::helper::messageBoxCustom(
            "Do you want to save changes to " + sceneName + "?",
            "Unsaved changes",
            "YesNoCancel");
        if (result == wi::helper::MessageBoxResult::No ||
            result == wi::helper::MessageBoxResult::OK)
        {
            continuation();
            return;
        }
        if (result != wi::helper::MessageBoxResult::Yes)
        {
            return;
        }
        if (currentPath.empty())
        {
            SaveSceneAs(
                [continuation = std::move(continuation)](const bool saved)
                {
                    if (saved)
                    {
                        continuation();
                    }
                });
            return;
        }

        SaveSceneAfterTransientCleanup(
            currentPath,
            [continuation = std::move(continuation)](const bool saved)
            {
                if (saved)
                {
                    continuation();
                }
            });
    }

    void StudioRenderPath::OpenScene()
    {
        if (session_ == nullptr || sceneOpenInProgress_)
        {
            return;
        }

        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::OPEN;
        params.description = "Renegade Scene (.wiscene)";
        params.extensions.push_back("wiscene");
        wi::helper::FileDialog(
            params,
            [this](const std::string& scenePath)
            {
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, scenePath](uint64_t)
                    {
                        RequestSceneReplacement(
                            [this, scenePath]()
                            {
                                BeginOpenScene(scenePath);
                            });
                    });
            });
    }

    void StudioRenderPath::BeginOpenScene(const std::string& scenePath)
    {
        if (session_ == nullptr || sceneOpenInProgress_)
        {
            return;
        }

        sceneOpenInProgress_ = true;
        openingScenePath_ = scenePath;
        sceneOpenWorkload_.priority = wi::jobsystem::Priority::Low;
        if (projectHubVisible_)
        {
            hubMessageLabel_.font.params.color = HologramMuted;
            hubMessageLabel_.SetText(
                "SCENE OPENING // " +
                wi::helper::GetFileNameFromPath(scenePath));
        }
        RefreshStatus();

        auto prepared = std::make_shared<bridge::PreparedSceneOpen>();
        wi::jobsystem::Execute(
            sceneOpenWorkload_,
            [this, scenePath, prepared](wi::jobsystem::JobArgs)
            {
                *prepared = session_->Documents().PrepareOpen(scenePath);
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, prepared](uint64_t)
                    {
                        CompleteOpenScene(std::move(*prepared));
                    });
            });
    }

    void StudioRenderPath::CompleteOpenScene(
        bridge::PreparedSceneOpen prepared)
    {
        sceneOpenInProgress_ = false;
        openingScenePath_.clear();

        if (session_ == nullptr)
        {
            return;
        }

        ClearSelectionOutline();
        if (!session_->Documents().CommitPreparedOpen(std::move(prepared)))
        {
            SyncSelectionOutline();
            if (projectHubVisible_)
            {
                hubMessageLabel_.font.params.color = WarningAmber;
                hubMessageLabel_.SetText(
                    "SCENE OPEN FAILED // " +
                    session_->Scenes().LastError());
            }
            RefreshStatus();
            RefreshInspector();
            return;
        }

        AdoptOpenedSceneCamera();
        RestoreGovernedMaterialTextures();
        SetEnvironmentWorkspaceActive(false);
        SetTerrainWorkspaceActive(false);
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        SetProjectHubVisible(false);
    }

    void StudioRenderPath::AdoptOpenedSceneCamera()
    {
        if (session_ == nullptr || camera == nullptr)
        {
            return;
        }

        const auto cameraEntity = session_->Documents().LastOpenedCamera();
        const auto& openedScene = session_->Scenes().GetScene();
        const auto* openedCamera =
            openedScene.cameras.GetComponent(cameraEntity);
        if (openedCamera == nullptr)
        {
            // No authored camera in the opened document (common for
            // terrain-only scenes that were never given an explicit
            // Camera entity). Wicked's terrain chunk streaming
            // (wi::terrain::Terrain::Generation_Update, run every real
            // frame from RenderPath3D::Update) evicts and permanently
            // discards any chunk whose distance from the *current*
            // editor camera exceeds its removal radius. Leaving the
            // camera wherever it happened to be from the previous
            // document means a freshly opened terrain scene has its
            // just-loaded, correctly-deserialized chunks evicted and
            // silently replaced with fresh procedural generation before
            // the user ever sees them - which reads as "the terrain
            // didn't save" even though the archive round-trip was
            // correct. Recenter over the terrain's own saved chunk
            // position instead of leaving the stale camera in place.
            AdoptOpenedSceneTerrainFallbackCamera(openedScene);
            return;
        }

        camera->Eye = openedCamera->Eye;
        camera->At = openedCamera->At;
        camera->Up = openedCamera->Up;
        camera->fov = openedCamera->fov;
        camera->zNearP = openedCamera->zNearP;
        camera->zFarP = openedCamera->zFarP;
        camera->focal_length = openedCamera->focal_length;
        camera->aperture_size = openedCamera->aperture_size;
        camera->aperture_shape = openedCamera->aperture_shape;
        camera->width = static_cast<float>(GetInternalResolution().x);
        camera->height = static_cast<float>(GetInternalResolution().y);

        const auto* openedTransform =
            openedScene.transforms.GetComponent(cameraEntity);
        if (openedTransform != nullptr)
        {
            editorCameraTransform_ = *openedTransform;
            camera->TransformCamera(editorCameraTransform_);
        }
        camera->UpdateCamera();
    }

    void StudioRenderPath::AdoptOpenedSceneTerrainFallbackCamera(
        const wi::scene::Scene& openedScene)
    {
        if (camera == nullptr || openedScene.terrains.GetCount() == 0)
        {
            return;
        }

        const wi::terrain::Terrain& terrain = openedScene.terrains[0];
        if (terrain.chunks.empty())
        {
            return;
        }

        // Prefer the chunk at the terrain's own saved center; fall back
        // to whichever loaded chunk is closest to it if that exact
        // coordinate was not generated/saved.
        auto best = terrain.chunks.find(terrain.center_chunk);
        if (best == terrain.chunks.end())
        {
            best = terrain.chunks.begin();
            int bestDist =
                std::max(
                    std::abs(terrain.center_chunk.x - best->first.x),
                    std::abs(terrain.center_chunk.z - best->first.z));
            for (auto it = terrain.chunks.begin();
                 it != terrain.chunks.end();
                 ++it)
            {
                const int dist = std::max(
                    std::abs(terrain.center_chunk.x - it->first.x),
                    std::abs(terrain.center_chunk.z - it->first.z));
                if (dist < bestDist)
                {
                    bestDist = dist;
                    best = it;
                }
            }
        }
        if (best == terrain.chunks.end())
        {
            return;
        }

        const XMFLOAT3 targetPosition = best->second.sphere.center;
        const float radius = std::max(best->second.sphere.radius, 1.0f);
        const XMVECTOR at = XMLoadFloat3(&targetPosition);
        const XMVECTOR eye =
            at +
            XMVectorSet(0.0f, radius * 1.5f, -radius * 2.5f, 0.0f);
        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
        editorCameraTransform_.ClearTransform();
        editorCameraTransform_.MatrixTransform(
            XMMatrixInverse(nullptr, view));
        editorCameraTransform_.UpdateTransform();
        camera->TransformCamera(editorCameraTransform_);
        camera->width = static_cast<float>(GetInternalResolution().x);
        camera->height = static_cast<float>(GetInternalResolution().y);
        camera->UpdateCamera();
    }

    void StudioRenderPath::BeginProjectLoad(
        const std::string& descriptorPath)
    {
        if (session_ == nullptr || descriptorPath.empty() ||
            projectLoadingOverlay_.IsBlocking() ||
            wi::jobsystem::IsBusy(projectLoadWorkload_))
        {
            return;
        }

        projectLoadingOverlay_.Begin(
            wi::helper::GetFileNameFromPath(descriptorPath));
        projectHubChrome_.SetVisible(false);
        hubNewProjectNameInput_.SetVisible(false);
        hubNewProjectConfirmButton_.SetVisible(false);
        hubNewProjectCancelButton_.SetVisible(false);

        projectLoadingOverlay_.SetPhase(
            RenegadeProjectLoadingOverlay::Phase::ValidatingProject);
        if (!session_->Projects().OpenProject(descriptorPath))
        {
            projectLoadingOverlay_.Fail(
                "Project validation failed: " + session_->Projects().LastError());
            return;
        }

        auto operation = std::make_shared<ProjectLoadOperation>();
        operation->descriptorPath = descriptorPath;
        operation->startupScenePath = session_->Projects().StartupScenePath();
        operation->project = session_->Projects().PendingProject();
        operation->storyFlowNative = operation->startupScenePath.empty();
        projectLoadingOverlay_.SetPhase(
            RenegadeProjectLoadingOverlay::Phase::PreparingScene);

        projectLoadWorkload_.priority = wi::jobsystem::Priority::Low;
        wi::jobsystem::Execute(
            projectLoadWorkload_,
            [this, operation](wi::jobsystem::JobArgs)
            {
                if (operation->storyFlowNative)
                {
                    std::string resolvedFlow;
                    bridge::FlowDocument flow;
                    if (!bridge::ResolveStoryFlowDocumentPath(
                            operation->project.rootPath,
                            operation->project.projectId,
                            operation->project.startupFlowId,
                            operation->project.startupFlow,
                            resolvedFlow,
                            operation->error) ||
                        !bridge::ReadFlowDocument(
                            resolvedFlow,
                            operation->project.projectId,
                            flow,
                            operation->error))
                    {
                        operation->error =
                            "The Story Flow project home could not be prepared: " +
                            operation->error;
                    }
                }
                else
                {
                    operation->preparedScene = session_->Documents().PrepareOpen(
                        operation->startupScenePath);
                    if (!operation->preparedScene.IsReady())
                    {
                        operation->error = operation->preparedScene.Error().empty()
                            ? "The startup scene could not be prepared."
                            : operation->preparedScene.Error();
                    }
                    else
                    {
                        auto* candidate =
                            operation->preparedScene.MutablePreparedScene();
                        if (candidate != nullptr)
                        {
                            projectLoadingOverlay_.SetPhase(
                                RenegadeProjectLoadingOverlay::Phase::RestoringAssets,
                                0, 0);
                            operation->textureRestore =
                                bridge::RestoreMaterialTextureBindings(
                                    *candidate,
                                    operation->project.rootPath,
                                    operation->project.projectId,
                                    {},
                                    [this](const std::size_t completed,
                                        const std::size_t total)
                                    {
                                        projectLoadingOverlay_.SetPhase(
                                            RenegadeProjectLoadingOverlay::Phase::RestoringAssets,
                                            completed, total);
                                    });
                        }
                    }
                }

                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, operation](std::uint64_t)
                    {
                        CompleteProjectLoad(operation);
                    });
            });
    }

    void StudioRenderPath::CompleteProjectLoad(
        std::shared_ptr<ProjectLoadOperation> operation)
    {
        if (session_ == nullptr || !operation)
            return;

        if (!operation->error.empty())
        {
            session_->Projects().DiscardPendingProject();
            projectLoadingOverlay_.Fail(operation->error);
            return;
        }

        projectLoadingOverlay_.SetPhase(
            RenegadeProjectLoadingOverlay::Phase::Finalising);
        ClearSelectionOutline();
        const bool adopted = operation->storyFlowNative
            ? session_->CommitPendingProjectWithoutScene()
            : session_->CommitPendingProjectScene(
                std::move(operation->preparedScene));
        if (!adopted)
        {
            SyncSelectionOutline();
            projectLoadingOverlay_.Fail(
                session_->Scenes().LastError().empty()
                    ? "The prepared project could not be adopted."
                    : session_->Scenes().LastError());
            return;
        }

        if (!operation->storyFlowNative)
        {
            AdoptOpenedSceneCamera();
        }
        SetEnvironmentWorkspaceActive(false);
        SetTerrainWorkspaceActive(false);
        workspaceTitle_.SetText(
            "RENEGADE STUDIO // " +
            session_->Projects().CurrentProject().name);
        hubMessageLabel_.font.params.color =
            operation->storyFlowNative || operation->textureRestore.succeeded
            ? HologramMuted : WarningAmber;
        hubMessageLabel_.SetText(
            operation->storyFlowNative || operation->textureRestore.succeeded
            ? "PROJECT ONLINE // " + session_->Projects().CurrentProject().descriptorPath
            : "PROJECT ONLINE // GOVERNED RESOURCE WARNING // " +
                operation->textureRestore.error);
        selectedRecentProject_ = -1;
        RefreshProjectHub();
        RefreshAssetBrowser();
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        SetProjectHubVisible(false);
        projectLoadingOverlay_.SetPhase(
            RenegadeProjectLoadingOverlay::Phase::Ready);
    }

    void StudioRenderPath::OpenProjectDescriptor(
        const std::string& descriptorPath)
    {
        BeginProjectLoad(descriptorPath);
    }

    void StudioRenderPath::OpenSelectedRecentProject()
    {
        if (session_ == nullptr || selectedRecentProject_ < 0)
        {
            return;
        }

        const auto& recent = session_->Projects().RecentProjects();
        const auto index = static_cast<std::size_t>(selectedRecentProject_);
        if (index >= recent.size())
        {
            return;
        }

        const std::string descriptorPath = recent[index].descriptorPath;
        RequestSceneReplacement(
            [this, descriptorPath]()
            {
                OpenProjectDescriptor(descriptorPath);
            });
    }

    void StudioRenderPath::ReturnToProjectHub()
    {
        if (session_ == nullptr)
        {
            return;
        }

        RequestSceneReplacement(
            [this]()
            {
                selectedRecentProject_ = -1;
                hubMessageLabel_.font.params.color = HologramMuted;
                hubMessageLabel_.SetText(
                    "PROJECT HUB ONLINE // SELECT AN OPERATION");
                RefreshProjectHub();
                SetProjectHubVisible(true);
            });
    }

    void StudioRenderPath::SelectRecentProject(const std::size_t index)
    {
        if (session_ == nullptr ||
            index >= session_->Projects().RecentProjects().size())
        {
            return;
        }

        selectedRecentProject_ = static_cast<int>(index);
        RefreshProjectHub();
    }

    void StudioRenderPath::SetProjectHubVisible(const bool visible)
    {
        if (visible && flyCameraActive_)
        {
            flyCameraActive_ = false;
            wi::input::HidePointer(false);
        }

        projectHubVisible_ = visible;
        projectHubPanel_.SetVisible(false);
        projectHubChrome_.SetVisible(visible);
        if (!visible)
        {
            hubNewProjectMode_ = false;
            projectHubChrome_.SetNewProjectMode(false);
        }
        hubNewProjectNameInput_.SetVisible(visible && hubNewProjectMode_);
        hubNewProjectConfirmButton_.SetVisible(visible && hubNewProjectMode_);
        hubNewProjectCancelButton_.SetVisible(visible && hubNewProjectMode_);
        // Stock workspace surfaces stay hidden. RenegadeStudioChrome owns the
        // shell, while the opaque Inspector host schedules the functional
        // controls rendered by Renegade subclasses.
        toolbarPanel_.SetVisible(false);
        hierarchyPanel_.SetVisible(false);
        inspectorPanel_.SetVisible(!visible);
        hierarchySearch_.SetVisible(!visible);
        contentPanel_.SetVisible(false);
        studioChrome_.SetVisible(!visible);

        // The stock overlay collides with Renegade's owned shell. Live FPS is
        // rendered by RenegadeStudioChrome's status bar instead and therefore
        // hides automatically with the rest of the workspace on Project Hub.
        if (diagnostics_ != nullptr)
        {
            diagnostics_->active = false;
        }

        if (!visible)
        {
            RefreshHierarchy();
            RefreshInspector();
            RefreshStatus();
        }
    }

    void StudioRenderPath::SyncGizmoSelection()
    {
        gizmo_.selected.clear();
        gizmo_.selectedEntitiesNonRecursive.clear();
        gizmoEntity_ = wi::ecs::INVALID_ENTITY;
        gizmoDragActive_ = false;

        if (environmentWorkspaceActive_ || session_ == nullptr ||
            !session_->Selection().HasSelection())
        {
            return;
        }

        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        const auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
        {
            return;
        }

        wi::scene::PickResult selected;
        selected.entity = entity;
        gizmo_.scene = &scene;
        gizmo_.selected.push_back(selected);
        gizmo_.selectedEntitiesNonRecursive.push_back(entity);
        gizmo_.PreTranslate();
        gizmoEntity_ = entity;
        gizmoTransformBefore_ = bridge::CaptureTransform(*transform);
    }

    void StudioRenderPath::ClearSelectionOutline() noexcept
    {
        if (session_ != nullptr &&
            outlinedEntity_ != wi::ecs::INVALID_ENTITY)
        {
            auto* object = session_->Scenes()
                .GetScene()
                .objects
                .GetComponent(outlinedEntity_);
            if (object != nullptr)
            {
                object->SetUserStencilRef(
                    outlinedEntityPreviousStencil_);
            }
        }

        outlinedEntity_ = wi::ecs::INVALID_ENTITY;
        outlinedEntityPreviousStencil_ = 0;
    }

    void StudioRenderPath::SyncSelectionOutline()
    {
        if (environmentWorkspaceActive_)
        {
            ClearSelectionOutline();
            return;
        }
        const auto selected = session_ != nullptr
            ? session_->Selection().SelectedEntity()
            : wi::ecs::INVALID_ENTITY;
        if (selected == outlinedEntity_)
        {
            return;
        }

        ClearSelectionOutline();
        if (session_ == nullptr ||
            selected == wi::ecs::INVALID_ENTITY)
        {
            return;
        }

        auto* object =
            session_->Scenes().GetScene().objects.GetComponent(selected);
        if (object == nullptr)
        {
            return;
        }

        outlinedEntity_ = selected;
        outlinedEntityPreviousStencil_ = object->userStencilRef;
        object->SetUserStencilRef(SelectionStencilReference);
    }

    void StudioRenderPath::SaveSceneAfterTransientCleanup(
        const std::string& scenePath,
        std::function<void(bool)> completion)
    {
        if (session_ == nullptr)
        {
            if (completion)
                completion(false);
            return;
        }

        if (detail::CreatorAssetDragPreviewBlocksSave())
        {
            detail::ClearCreatorAssetDragPreview();
            studioChrome_.SetStatusText(
                "SAVE // WAITING FOR TRANSIENT ASSET PREVIEW CLEANUP");
            wi::eventhandler::Subscribe_Once(
                wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                [this, scenePath, completion](std::uint64_t)
                {
                    SaveSceneAfterTransientCleanup(scenePath, completion);
                });
            return;
        }

        ClearSelectionOutline();
        const bool saved = session_->SaveScene(scenePath);
        SyncSelectionOutline();
        RefreshStatus();
        RefreshInspector();
        if (completion)
            completion(saved);
    }

    void StudioRenderPath::SaveScene()
    {
        if (session_ == nullptr)
            return;

        const std::string scenePath = session_->Scenes().CurrentPath();
        if (scenePath.empty())
        {
            SaveSceneAs();
            return;
        }

        StopSunPreview(true);
        SaveSceneAfterTransientCleanup(scenePath);
    }

    void StudioRenderPath::SaveSceneAs(
        std::function<void(bool)> completion)
    {
        if (session_ == nullptr)
        {
            if (completion)
                completion(false);
            return;
        }

        StopSunPreview(true);

        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::SAVE;
        params.description = "Renegade Scene (.wiscene)";
        params.extensions.push_back("wiscene");
        wi::helper::FileDialog(
            params,
            [this, completion](const std::string& selectedPath)
            {
                const std::string scenePath =
                    wi::helper::ForceExtension(selectedPath, "wiscene");
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, scenePath, completion](std::uint64_t)
                    {
                        SaveSceneAfterTransientCleanup(
                            scenePath,
                            completion);
                    });
            },
            [completion]()
            {
                if (completion)
                    completion(false);
            });
    }

    void StudioRenderPath::ReopenScene()
    {
        if (session_ == nullptr)
        {
            return;
        }

        const std::string scenePath = session_->Scenes().CurrentPath();
        if (scenePath.empty())
        {
            session_->ReloadScene();
            RefreshStatus();
            return;
        }

        RequestSceneReplacement(
            [this, scenePath]()
            {
                BeginOpenScene(scenePath);
            });
    }

    void StudioApplication::SetStartupScene(std::string filePath)
    {
        if (!filePath.empty())
        {
            startupScene_ = std::move(filePath);
        }
    }

    void StudioApplication::PrepareProvingGround()
    {
        if (wi::helper::FileExists(startupScene_) &&
            session_.LoadScene(startupScene_))
        {
            return;
        }

        session_.Scenes().CreateProvingGround();
        session_.SaveScene(startupScene_);
    }

    void StudioApplication::SetExitRequestHandler(std::function<void()> handler)
    {
        renderer_.SetExitRequestHandler(std::move(handler));
    }

    void StudioApplication::RequestExit()
    {
        renderer_.RequestExit();
    }

    void StudioApplication::Initialize()
    {
        wi::Application::Initialize();

        infoDisplay.active = true;
        infoDisplay.watermark = false;
        infoDisplay.device_name = false;
        infoDisplay.resolution = false;
        infoDisplay.logical_size = false;
        infoDisplay.colorspace = false;
        infoDisplay.fpsinfo = false;
        infoDisplay.size = 14;

        session_.Projects().Initialize("Saved/RenegadeStudio.ini");
        PrepareProvingGround();

        renderer_.BindSession(session_);
        renderer_.BindDiagnostics(infoDisplay);
        renderer_.init(canvas);
        renderer_.Load();

        storyFlowIntegration_.OnScreenEditorOpen(
            [this](const StoryFlowScreenEditorHandoff& handoff)
            {
                if (!session_.Projects().HasProject()) return;
                const auto& project = session_.Projects().CurrentProject();
                std::string error;
                if (!screenEditorRenderer_.OpenScreen(
                        handoff, project.rootPath, project.projectId, error))
                {
                    wi::backlog::post(
                        "Renegade Screen Editor: " + error,
                        wi::backlog::LogLevel::Error);
                    return;
                }
                storyFlowIntegration_.RequestScreenEditor();
            });
        screenEditorRenderer_.OnReturnRequested([this]()
        {
            storyFlowIntegration_.RequestStoryFlow();
        });
        ActivatePath(&renderer_);
    }
}
