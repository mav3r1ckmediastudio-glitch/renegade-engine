Warning: truncated output (original token count: 140661)
Total output lines: 13435

#include "DiagnosticInputFrame.h"
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
#include "renegade/bridge/HumanoidRetargetService.h"
#include "renegade/bridge/AnimationService.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cfloat>
#include <filesystem>
#include <fstream>
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

    // External files must be identical at preview and governed commit.
    // Hashing is read-only; the committed RAsset contains baked native keys.
    bool HashCreatorExternalAnimationSource(
        const std::string& sourcePath,
        std::uint64_t& hash)
    {
        std::ifstream file(fs::u8path(sourcePath), std::ios::binary);
        if (!file)
            return false;
        hash = 14695981039346656037ull;
        char buffer[64 * 1024];
        while (file)
        {
            file.read(buffer, sizeof(buffer));
            for (std::streamsize index = 0; index < file.gcount(); ++index)
            {
                hash ^= static_cast<unsigned char>(buffer[index]);
                hash *= 1099511628211ull;
            }
        }
        return file.eof() && !file.bad();
    }

    void ApplyCreatorImportPreviewLighting();
    struct CreatorModelImportWorkspaceState
    {
        bool active = false;
        bool committing = false;
        std::string sourcePath;
        std::size_t undoBaseline = 0;
        wi::allocator::shared_ptr<wi::scene::Scene> previewScene;
        wi::ecs::Entity previewRoot = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity previewLight = wi::ecs::INVALID_ENTITY;
        wi::scene::TransformComponent cameraBefore;
        float cameraFovBefore = XM_PIDIV4;
        bool cameraCaptured = false;
        float automaticScale = 1.0f;
        renegade::bridge::ModelBounds sourceBounds;
        renegade::bridge::ImportedSceneSummary summary;
        renegade::bridge::ImportedModelEvidence evidence;
        std::string rigDiagnostic;
        std::vector<wi::ecs::Entity> materialEntities;
        std::vector<wi::ecs::Entity> animationEntities;
        std::vector<XMFLOAT2> animationSourceRanges;
        std::vector<renegade::bridge::CreatorMaterialSourceOverride> materialOverrides;
        std::vector<renegade::bridge::CreatorAnimationImportRecipe> animationRecipe;
        struct ExternalAnimation
        {
            std::string sourcePath;
            std::uint64_t sourceHash = 0;
            std::size_t clipCount = 0;
            std::unique_ptr<renegade::bridge::RetargetHumanoidAnimationsCommand> command;
        };
        std::vector<ExternalAnimation> externalAnimations;
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
        bool importAsCharacter = false;
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

    // All importer edits and presentation remain in a transient scene.
    // The authored document is never used as the preview render target.
    wi::scene::Scene& CreatorImportActiveScene()
    {
        if (creatorModelImporter.previewScene.IsValid())
            return *creatorModelImporter.previewScene;
        return renegade::bridge::StudioSession::Current()->Scenes().GetScene();
    }


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

        auto& scene = CreatorImportActiveScene();
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
            auto& scene = CreatorImportActiveScene();
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
    renegade::studio::RenegadeButton creatorImportAnimationPlay;
    renegade::studio::RenegadeButton creatorImportAnimationPause;
    renegade::studio::RenegadeButton creatorImportAnimationStop;
    renegade::studio::RenegadeButton creatorImportExternalAnimationAdd;
    renegade::studio::RenegadeButton creatorImportExternalAnimationRemove;
    wi::gui::Label creatorImportExternalAnimationStatus;
    wi::gui::Label creatorImportAnimationReadout;
    wi::gui::Label creatorImportTransformLabel;
    std::array<renegade::studio::RenegadeButton, 6> creatorImportStageButtons;
    renegade::studio::RenegadeButton creatorImportModelChoice;
    renegade::studio::RenegadeButton creatorImportCharacterChoice;
    wi::gui::Label creatorImportRigReadout;
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
        const auto& scene = CreatorImportActiveScene();
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
        auto& scene = CreatorImportActiveScene();
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
        auto& scene = CreatorImportActiveScene();
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
        auto& scene = CreatorImportActiveScene();
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
        auto& scene = CreatorImportActiveScene();
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
        auto& scene = CreatorImportActiveScene();
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

    // Preview transport acts only on the transient scene and never records
    // editor commands. A recipe entry may duplicate a source action; resolve
    // the actual native component by source index, not the visible row index.
    void StopCreatorImportPreviewAnimations()
    {
        if (!creatorModelImporter.previewScene.IsValid())
            return;
        auto& preview = *creatorModelImporter.previewScene;
        for (std::size_t index = 0; index < preview.animations.GetCount(); ++index)
            (void)renegade::bridge::StopAnimation(
                preview, preview.animations.GetEntity(index));
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
        if (creatorModelImporter.previewScene.IsValid() &&
            clip.sourceAnimationIndex < creatorModelImporter.animationEntities.size())
        {
            const auto entity = creatorModelImporter.animationEntities[clip.sourceAnimationIndex];
            const auto* animation = creatorModelImporter.previewScene->animations.GetComponent(entity);
            out << (animation == nullptr ? " // NATIVE CLIP MISSING" :
                (animation->IsPlaying() ? " // NATIVE PLAYBACK ACTIVE" : " // NATIVE PAUSED"));
        }
        else
            out << " // NATIVE CLIP MISSING";
        creatorImportAnimationReadout.SetText(out.str());
    }

    void PreviewSelectedCreatorImportAnimation(const bool play, const bool pause)
    {
        if (!creatorModelImporter.active || !creatorModelImporter.importAsCharacter ||
            !creatorModelImporter.previewScene.IsValid() ||
            creatorModelImporter.animationRecipe.empty())
            return;
        auto& preview = *creatorModelImporter.previewScene;
        const auto& clip = creatorModelImporter.animationRecipe[
            creatorModelImporter.selectedAnimation];
        if (!clip.enabled ||
            clip.sourceAnimationIndex >= creatorModelImporter.animationEntities.size())
        {
            creatorImportAnimationReadout.SetText(
                "Preview unavailable: select an included source animation.");
            return;
        }
        const auto entity = creatorModelImporter.animationEntities[clip.sourceAnimationIndex];
        auto* animation = preview.animations.GetComponent(entity);
        if (animation == nullptr || animation->channels.empty() ||
            clip.sourceAnimationIndex >= creatorModelImporter.animationSourceRanges.size())
        {
            creatorImportAnimationReadout.SetText(
                "Preview unavailable: no matching native animation channels.");
            return;
        }
        const auto sourceRange = creatorModelImporter.animationSourceRanges[
            clip.sourceAnimationIndex];
        if (!std::isfinite(clip.start) || !std::isfinite(clip.end) ||
            clip.start < sourceRange.x || clip.end > sourceRange.y || clip.start >= clip.end)
        {
            creatorImportAnimationReadout.SetText(
                "Preview unavailable: clip range is outside its native source.");
            return;
        }
        if (play)
        {
            StopCreatorImportPreviewAnimations();
            animation->start = clip.start;
            animation->end = clip.end;
            (void)renegade::bridge::PlayAnimation(preview, entity, true);
        }
        else if (pause)
            (void)renegade::bridge::PauseAnimation(preview, entity);
        else
            StopCreatorImportPreviewAnimations();
        RefreshCreatorImportAnimationEditor();
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

    void RefreshCreatorExternalAnimationQueue()
    {
        std::ostringstream status;
        status << "EXTERNAL SOURCES: " << creatorModelImporter.externalAnimations.size();
        for (const auto& source : creatorModelImporter.externalAnimations)
            status << "\n" << fs::u8path(source.sourcePath).filename().generic_u8string()
                << " // " << source.clipCount << " native clip(s)";
        status << "\nPREVIEW ONLY // CONFIRM ATTEMPTS GOVERNED COMMIT.";
        creatorImportExternalAnimationStatus.SetText(status.str());
        creatorImportExternalAnimationRemove.SetEnabled(
            !creatorModelImporter.externalAnimations.empty());
    }

    void QueueCreatorExternalAnimation(const std::string& path)
    {
        if (!creatorModelImporter.active || !creatorModelImporter.importAsCharacter ||
            !creatorModelImporter.previewScene.IsValid() ||
            creatorModelImporter.committing || path.empty())
            return;
        auto& preview = *creatorModelImporter.previewScene;
        std::string error;
        if (!renegade::bridge::EnsureHumanoidAnimationSourceMapping(preview, error))
        {
            creatorImportExternalAnimationStatus.SetText("DESTINATION RIG INVALID // " + error);
            return;
        }
        wi::ecs::Entity target = wi::ecs::INVALID_ENTITY;
        for (std::size_t index = 0; index < preview.armatures.GetCount(); ++index)
        {
            const auto candidate = preview.armatures.GetEntity(index);
            if (renegade::bridge::IsHumanoidMappingValid(
                    renegade::bridge::CaptureHumanoidMapping(preview, candidate)))
            {
                target = candidate;
                break;
            }
        }
        if (target == wi::ecs::INVALID_ENTITY)
        {
            creatorImportExternalAnimationStatus.SetText(
                "DESTINATION RIG INVALID // no complete native humanoid map.");
            return;
        }
        std::uint64_t sourceHash = 0;
        if (!HashCreatorExternalAnimationSource(path, sourceHash))
        {
            creatorImportExternalAnimationStatus.SetText(
                "SOURCE UNREADABLE // external clip could not be fingerprinted.");
            return;
        }
        StopCreatorImportPreviewAnimations();
        auto command = std::make_unique<
            renegade::bridge::RetargetHumanoidAnimationsCommand>(preview, target, path, true);
        if (!command->Execute())
        {
            creatorImportExternalAnimationStatus.SetText(
                "RETARGET FAILED // " + command->Result().error);
            return;
        }
        std::uint64_t sourceHashAfter = 0;
        if (!HashCreatorExternalAnimationSource(path, sourceHashAfter) ||
            sourceHashAfter != sourceHash)
        {
            command->Undo();
            creatorImportExternalAnimationStatus.SetText(
                "SOURCE CHANGED // retry external clip preview.");
            return;
        }
        const auto created = command->Result().createdAnimations;
        if (created.empty())
        {
            command->Undo();
            creatorImportExternalAnimationStatus.SetText(
                "RETARGET FAILED // no native clips were produced.");
            return;
        }
        for (const auto entity : created)
        {
            auto* animation = preview.animations.GetComponent(entity);
            if (animation == nullptr || animation->channels.empty())
            {
                command->Undo();
                creatorImportExternalAnimationStatus.SetText(
                    "RETARGET FAILED // a baked clip has no native channels.");
                return;
            }
        }
        for (const auto entity : created)
        {
            const auto* animation = preview.animations.GetComponent(entity);
            const auto* name = preview.names.GetComponent(entity);
            renegade::bridge::CreatorAnimationImportRecipe clip;
            clip.sourceAnimationIndex = static_cast<std::uint32_t>(
                creatorModelImporter.animationEntities.size());
            clip.name = name != nullptr ? name->name : "Retargeted clip";
            clip.start = animation->start;
            clip.end = animation->end;
            clip.enabled = true;
            creatorModelImporter.animationEntities.push_back(entity);
            creatorModelImporter.animationSourceRanges.push_back(
                XMFLOAT2(animation->start, animation->end));
            creatorModelImporter.animationRecipe.push_back(std::move(clip));
        }
        CreatorModelImportWorkspaceState::ExternalAnimation source;
        source.sourcePath = path;
        source.sourceHash = sourceHash;
        source.clipCount = created.size();
        source.command = std::move(command);
        creatorModelImporter.externalAnimations.push_back(std::move(source));
        StopCreatorImportPreviewAnimations();
        creatorModelImporter.selectedAnimation =
            creatorModelImporter.animationRecipe.size() - created.size();
        RebuildCreatorImportAnimationCombo();
        RefreshCreatorExternalAnimationQueue();
    }

    void RemoveLastCreatorExternalAnimation()
    {
        if (creatorModelImporter.externalAnimations.empty() ||
            !creatorModelImporter.previewScene.IsValid())
            return;
        StopCreatorImportPreviewAnimations();
        auto& last = creatorModelImporter.externalAnimations.back();
        const std::size_t count = last.clipCount;
        if (last.command)
            last.command->Undo();
        for (std::size_t i = 0; i < count; ++i)
        {
            creatorModelImporter.animationEntities.pop_back();
            creatorModelImporter.animationSourceRanges.pop_back();
            creatorModelImporter.animationRecipe.pop_back();
        }
        creatorModelImporter.externalAnimations.pop_back();
        creatorModelImporter.selectedAnimation = 0;
        RebuildCreatorImportAnimationCombo();
        RefreshCreatorExternalAnimationQueue();
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
        auto& scene = CreatorImportActiveScene();
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
        auto& scene = CreatorImportActiveScene();
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

    bool StudioRenderPath::DiagnosticImportActive() const
    {
        return creatorModelImporter.active;
    }

    void StudioRenderPath::BindDiagnostics(
        wi::Application::InfoDisplayer& diagnostics) noexcept
    {
        diagnostics_ = &diagnostics;
        InitializeLiveDiagnostics();
    }

    void StudioRenderPath::Load()
    {
        setSSREnabled(false);
        setReflectionsEnabled(true);

        // Ambient occlusion grounds the deck props against the terrain, and
        // volumetric lights are what make the scene's fog react to lighting
        // instead of reading as a flat screen-space overlay.
        setAO(AO::AO_MSAO);
        setAOPower(1.4f);
        setVolumeLightsEnabled(true);

        // Gate 5 owns image-quality state per Level. Apply the persisted
        // state before RenderPath3D::Load() so the first rendered frame cannot
        // inherit arbitrary settings from the previous editor scene.
        SyncRenderSettingsFromScene(false);

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
        // w is the grid plane height. Y=0 is Renegade's creator-facing
        // world reference plane; keep only the 2 cm reverse-Z/depth epsilon.
        // The isolated creator-import stage keeps its own elevated plane.
        constexpr float gridPlaneHeight = 0.02f;
        constants.cameraPosition = XMFLOAT4(
            camera->Eye.x,
            camera->Eye.y,
            camera->Eye.z,
            creatorModelImporter.active
                ? CreatorImportStageHeight + 0.02f
                : gridPlaneHeight);

        // The authored editor uses ice-blue interaction lines. Importer
        // preview keeps the same native grid pass, but deliberately reduces
        // it to a neutral studio floor so it cannot read as level-editing UI.
        const bool importerPreview = creatorModelImporter.active;
        constants.minorColor = importerPreview
            ? XMFLOAT4(0.34f, 0.46f, 0.52f, 0.10f)
            : XMFLOAT4(0.36f, 0.84f, 1.0f, 0.28f);
        constants.majorColor = importerPreview
            ? XMFLOAT4(0.42f, 0.57f, 0.63f, 0.18f)
            : XMFLOAT4(0.46f, 0.90f, 1.0f, 0.50f);
        constants.axisColorX = importerPreview
            ? XMFLOAT4(0.42f, 0.57f, 0.63f, 0.18f)
            : XMFLOAT4(1.00f, 0.42f, 0.06f, 0.70f);
        constants.axisColorZ = importerPreview
            ? XMFLOAT4(0.42f, 0.57f, 0.63f, 0.18f)
            : XMFLOAT4(0.30f, 0.78f, 1.00f, 0.70f);

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
        if (pathTracePreviewActive_)
        {
            selectionOutlineMask_ = {};
            selectionOutlineMaskMsaa_ = {};
            RenderPath3D_PathTracing::ResizeBuffers();
            return;
        }
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

    void StudioRenderPath::PreRender()
    {
        if (testLevelRuntime_.IsActive())
        {
            // Runtime owns the live 3D world during Test Level. Keep only the
            // lightweight 2D Studio surface alive so STOP/status remain usable.
            wi::RenderPath2D::PreRender();
            return;
        }
        wi::RenderPath3D::PreRender();
    }

    void StudioRenderPath::Render() const
    {
        if (testLevelRuntime_.IsActive())
        {
            wi::RenderPath2D::Render();
            return;
        }
        if (pathTracePreviewActive_)
        {
            RenderPath3D_PathTracing::Render();
            return;
        }
        RenderPath3D::Render();

        const auto* depthStencil = GetDepthStencil();
        if (projectHubVisible_ || creatorModelImporter.active ||
            creatorModelImporter.thumbnailCapturePending ||
            outlinedSelection_ == wi::ecs::INVALID_ENTITY ||
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
                RequestDiagnosticAction(action);
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
            RequestDiagnosticAction(EditorAction::ToggleGrid);
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
            SetRenderWorkspaceActive(false);
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
            sceneIdentityLabel_,
            "Scene Identity Section",
            "SCENE // IDENTITY");
        sceneNameInput_.Create("Scene Entity Name");
        sceneNameInput_.SetPlaceholder("ENTITY NAME");
        sceneNameInput_.SetTooltip(
            "Creator-facing name. Reusable imported assets rename their stable top-level root, not an internal glTF node.");
        sceneNameInput_.OnInputAccepted([this](const wi::gui::EventArgs& args)
        {
            CommitSelectedSceneName(args.sValue);
        });
        inspectorPanel_.AddWidget(&sceneNameInput_);

        createSectionLabel(
            sceneLayerLabel_,
            "Scene Layer Section",
            "LAYERS // 32-BIT MASK");
        sceneLayerAllButton_.Create("Enable All Scene Layers");
        sceneLayerAllButton_.SetText("ALL");
        sceneLayerAllButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ApplySelectedLayerMask(~0u);
        });
        inspectorPanel_.AddWidget(&sceneLayerAllButton_);
        sceneLayerNoneButton_.Create("Disable All Scene Layers");
        sceneLayerNoneButton_.SetText("NONE");
        sceneLayerNoneButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ApplySelectedLayerMask(0u);
        });
        inspectorPanel_.AddWidget(&sceneLayerNoneButton_);
        for (std::uint32_t bit = 0; bit < sceneLayerBits_.size(); ++bit)
        {
            auto& checkbox = sceneLayerBits_[bit];
            // The CheckBox Create() label is creator-visible. Keep it compact
            // so all 32 native layer bits remain readable in the 8-column grid.
            checkbox.Create(std::to_string(bit));
            checkbox.SetTooltip(
                "Wicked layer bit " + std::to_string(bit) +
                ". Reusable assets apply the bit to the stable root and descendant render objects.");
            checkbox.OnClick([this, bit](const wi::gui::EventArgs& args)
            {
                ApplySelectedLayerBit(bit, args.bValue);
            });
            inspectorPanel_.AddWidget(&checkbox);
        }

        createSectionLabel(
            sceneMetadataLabel_,
            "Scene Metadata Section",
            "METADATA // PRESET");
        sceneMetadataPreset_.Create("Metadata Preset");
        sceneMetadataPreset_.AddItem("CUSTOM", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::Custom));
        sceneMetadataPreset_.AddItem("WAYPOINT", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::Waypoint));
        sceneMetadataPreset_.AddItem("PLAYER", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::Player));
        sceneMetadataPreset_.AddItem("ENEMY", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::Enemy));
        sceneMetadataPreset_.AddItem("NPC", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::NPC));
        sceneMetadataPreset_.AddItem("PICKUP", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::Pickup));
        sceneMetadataPreset_.AddItem("VEHICLE", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::Vehicle));
        sceneMetadataPreset_.AddItem("POINT OF INTEREST", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::PointOfInterest));
        sceneMetadataPreset_.SetTooltip(
            "Native Wicked semantic preset. Existing typed metadata and Renegade asset identity are preserved.");
        sceneMetadataPreset_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            ApplySelectedMetadataPreset(
                static_cast<wi::scene::MetadataComponent::Preset>(args.userdata));
        });
        inspectorPanel_.AddWidget(&sceneMetadataPreset_);

        createSectionLabel(
            sceneObjectLabel_,
            "Scene Object Section",
            "OBJECT // RENDER PARTICIPATION");
        const auto createObjectToggle = [this](
            SceneInspectorCheckBox& checkbox,
            const char* name,
            const char* tooltip,
            const bridge::ObjectParticipationProperty property)
        {
            checkbox.Create(name);
            checkbox.SetTooltip(tooltip);
            checkbox.OnClick([this, property](const wi::gui::EventArgs& args)
            {
                ApplySelectedObjectParticipation(property, args.bValue);
            });
            inspectorPanel_.AddWidget(&checkbox);
        };
        createObjectToggle(sceneObjectRenderable_, "Renderable: ",
            "Participate in normal scene rendering.",
            bridge::ObjectParticipationProperty::Renderable);
        createObjectToggle(sceneObjectCastShadow_, "Cast shadow: ",
            "Allow this object to cast native Wicked shadows.",
            bridge::ObjectParticipationProperty::CastShadow);
        createObjectToggle(sceneObjectForeground_, "Foreground: ",
            "Render as foreground geometry.",
            bridge::ObjectParticipationProperty::Foreground);
        createObjectToggle(sceneObjectMainCamera_, "Main camera: ",
            "Visible to the main camera.",
            bridge::ObjectParticipationProperty::VisibleInMainCamera);
        createObjectToggle(sceneObjectReflections_, "Reflections: ",
            "Visible to reflection rendering.",
            bridge::ObjectParticipationProperty::VisibleInReflections);
        createObjectToggle(sceneObjectWetmap_, "Wetmap: ",
            "Enable native Wicked wetmap participation.",
            bridge::ObjectParticipationProperty::Wetmap);

        createSectionLabel(
            playerLabel_,
            "Player Start Section",
            "PLAYER START // FIRST PERSON");
        playerCameraMode_.Create("Player Camera Mode");
        playerCameraMode_.SetText(
            "CAMERA // FIRST PERSON // SPAWN HEADING FOLLOWS ROTATION Y");
        playerCameraMode_.font.params.size = 11;
        playerCameraMode_.font.params.color = HologramMuted;
        playerCameraMode_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        playerCameraMode_.SetColor(wi::Color::Transparent());
        inspectorPanel_.AddWidget(&playerCameraMode_);

        const auto createPlayerSlider = [this](
            SceneInspectorSlider& slider,
            const char* name,
            const char* label,
            const char* tooltip,
            const PlayerField field,
            const float minimum,
            const float maximum,
            const float steps)
        {
            slider.Create(minimum, maximum, minimum, steps, name, label);
            slider.SetTooltip(tooltip);
            slider.OnValueCommitted([this, field](const float value)
            {
                CommitSelectedPlayerField(field, value);
            });
            inspectorPanel_.AddWidget(&slider);
        };
        createPlayerSlider(playerCapsuleRadius_, "Player Capsule Radius",
            "CAPSULE RADIUS // M", "Wicked/Jolt character capsule radius.",
            PlayerField::CapsuleRadius, 0.1f, 1.5f, 1401.0f);
        createPlayerSlider(playerCapsuleHeight_, "Player Capsule Height",
            "CAPSULE TOTAL HEIGHT // M", "Total height including both rounded caps.",
            PlayerField::CapsuleTotalHeight, 0.4f, 4.0f, 3601.0f);
        createPlayerSlider(playerEyeHeight_, "Player Eye Height",
            "EYE HEIGHT // M", "First-person camera height above the marker feet.",
            PlayerField::EyeHeight, 0.1f, 3.5f, 3401.0f);
        createPlayerSlider(playerWalkSpeed_, "Player Walk Speed",
            "WALK SPEED // M/S", "Normal movement speed.",
            PlayerField::WalkSpeed, 0.0f, 20.0f, 2001.0f);
        createPlayerSlider(playerSprintSpeed_, "Player Sprint Speed",
            "SPRINT SPEED // M/S", "Sprint speed; never lower than walk speed.",
            PlayerField::SprintSpeed, 0.0f, 30.0f, 3001.0f);
        createPlayerSlider(playerJumpSpeed_, "Player Jump Speed",
            "JUMP SPEED // M/S", "Vertical impulse requested through Wicked character physics.",
            PlayerField::JumpSpeed, 0.0f, 20.0f, 2001.0f);
        createPlayerSlider(playerLookSensitivity_, "Player Look Sensitivity",
            "LOOK SENSITIVITY", "Multiplier for mouse and gamepad look actions.",
            PlayerField::LookSensitivity, 0.05f, 5.0f, 991.0f);
        createPlayerSlider(playerMaximumSlope_, "Player Maximum Slope",
            "MAXIMUM SLOPE // DEG", "Steepest surface accepted by Wicked/Jolt.",
            PlayerField::MaximumSlope, 0.0f, 89.0f, 891.0f);
        createPlayerSlider(playerGravityFactor_, "Player Gravity Factor",
            "GRAVITY FACTOR", "Multiplier for world gravity on the character.",
            PlayerField::GravityFactor, 0.0f, 4.0f, 801.0f);
        createPlayerSlider(playerMinimumPitch_, "Player Minimum Pitch",
            "LOOK DOWN LIMIT // DEG", "Lowest first-person camera pitch.",
            PlayerField::MinimumPitch, -89.0f, 0.0f, 891.0f);
        createPlayerSlider(playerMaximumPitch_, "Player Maximum Pitch",
            "LOOK UP LIMIT // DEG", "Highest first-person camera pitch.",
            PlayerField::MaximumPitch, 0.0f, 89.0f, 891.0f);

        CreateMaterialInspector();
        CreateS1BInspectorSections();
        CreateRenderWorkspace();

        createSectionLabel(
            cameraLabel_,
            "Camera Section",
            "CAMERA // NATIVE WICKED");
        cameraProjection_.Create("Camera Projection");
        cameraProjection_.AddItem("PERSPECTIVE", 0);
        cameraProjection_.AddItem("ORTHOGRAPHIC", 1);
        cameraProjection_.SetTooltip(
            "Choose the selected scene camera's native projection mode.");
        cameraProjection_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            ApplySelectedCameraProjection(args.userdata == 1);
        });
        inspectorPanel_.AddWidget(&cameraProjection_);

        const auto createCameraSlider = [this](
            SceneInspectorSlider& input,
            const char* name,
            const char* label,
            const char* tooltip,
            const CameraField field,
            const float minimum,
            const float maximum,
            const float steps)
        {
            input.Create(minimum, maximum, 0.0f, steps, name, label);
            input.SetTooltip(tooltip);
            input.OnDragStarted([this, field](const float)
            {
                BeginCameraSlider(field);
            });
            input.OnValuePreview([this, field](const float value)
            {
                PreviewCameraSlider(field, value);
            });
            input.OnValueCommitted([this, field](const float value)
            {
                CommitCameraSlider(field, value);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createCameraSlider(cameraFieldOfView_, "Camera FOV", "FIELD OF VIEW",
            "Perspective field of view in degrees.",
            CameraField::FieldOfView, 1.0f, 179.0f, 1780.0f);
        createCameraSlider(cameraNearPlane_, "Camera Near Plane", "NEAR CLIP",
            "Geometry nearer than this distance is clipped.",
            CameraField::NearPlane, 0.001f, 10.0f, 10000.0f);
        createCameraSlider(cameraFarPlane_, "Camera Far Plane", "FAR CLIP",
            "Geometry farther than this distance is clipped.",
            CameraField::FarPlane, 10.0f, 100000.0f, 100000.0f);
        createCameraSlider(cameraFocalLength_, "Camera Focal Length", "FOCAL DISTANCE",
            "Depth-of-field focus distance.",
            CameraField::FocalLength, 0.001f, 1000.0f, 10000.0f);
        createCameraSlider(cameraApertureSize_, "Camera Aperture", "APERTURE",
            "Depth-of-field aperture strength.",
            CameraField::ApertureSize, 0.0f, 1.0f, 1000.0f);
        createCameraSlider(cameraOrthoVerticalSize_, "Camera Ortho Size", "ORTHO // VERTICAL SIZE",
            "Vertical size of the orthographic camera volume.",
            CameraField::OrthoVerticalSize, 0.01f, 10000.0f, 100000.0f);

        cameraAlignToView_.Create("Align Camera To View");
        cameraAlignToView_.SetText("ALIGN CAMERA TO VIEW");
        cameraAlignToView_.SetTooltip(
            "Move the selected scene camera to the current editor viewpoint.");
        cameraAlignToView_.OnClick([this](const wi::gui::EventArgs&)
        {
            AlignSelectedCameraToView();
        });
        inspectorPanel_.AddWidget(&cameraAlignToView_);
        cameraViewFrom_.Create("View From Camera");
        cameraViewFrom_.SetText("VIEW FROM CAMERA");
        cameraViewFrom_.SetTooltip(
            "Move the transient editor view to the selected scene camera without changing the scene.");
        cameraViewFrom_.OnClick([this](const wi::gui::EventArgs&)
        {
            ViewFromSelectedCamera();
        });
        inspectorPanel_.AddWidget(&cameraViewFrom_);

        createSectionLabel(
            decalLabel_,
            "Decal Section",
            "DECAL // NATIVE WICKED");
        decalBaseColorOnlyAlpha_.Create("Base color alpha only: ");
        decalBaseColorOnlyAlpha_.SetTooltip(
            "Use only base-colour alpha while preserving normal/surface decal detail.");
        decalBaseColorOnlyAlpha_.OnClick([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* decal = session_->Scenes().GetScene().decals.GetComponent(entity);
            if (decal == nullptr)
                return;
            auto state = bridge::CaptureDecal(*decal);
            state.baseColorOnlyAlpha = args.bValue;
            CommitSelectedDecal(state);
        });
        inspectorPanel_.AddWidget(&decalBaseColorOnlyAlpha_);

        decalSlopeBlend_.Create(
            0.0f, 8.0f, 0.0f, 801.0f,
            "Decal Slope Blend", "SLOPE BLEND");
        decalSlopeBlend_.SetTooltip(
            "Blend decal projection by receiving-surface slope. Zero disables slope rejection.");
        decalSlopeBlend_.OnValueCommitted([this](float value)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* decal = session_->Scenes().GetScene().decals.GetComponent(entity);
            if (decal == nullptr)
                return;
            auto state = bridge::CaptureDecal(*decal);
            state.slopeBlendPower = value;
            CommitSelectedDecal(state);
        });
        inspectorPanel_.AddWidget(&decalSlopeBlend_);

        createSectionLabel(
            decalMaterialLabel_,
            "Decal Material Section",
            "MATERIAL // RENEGRADE CORE");
        const auto createDecalMaterialSlider = [this](
            SceneInspectorSlider& slider,
            const char* name,
            const char* label,
            const int component)
        {
            slider.Create(0.0f, 1.0f, 1.0f, 1001.0f, name, label);
            slider.OnValueCommitted([this, component](float value)
            {
                if (session_ == nullptr)
                    return;
                const auto selected = session_->Selection().SelectedEntity();
                auto& scene = session_->Scenes().GetScene();
                if (!scene.decals.Contains(selected))
                    return;
                const auto materialEntity =
                    bridge::ResolveEditableMaterialEntity(scene, selected);
                auto* material = scene.materials.GetComponent(materialEntity);
                if (material == nullptr)
                    return;
                auto state = bridge::CaptureMaterial(*material);
                if (component == 0)
                    state.baseColor.x = value;
                else if (component == 1)
                    state.baseColor.y = value;
                else if (component == 2)
                    state.baseColor.z = value;
                else
                    state.baseColor.w = value;
                (void)session_->Commands().Execute(
                    std::make_unique<bridge::SetMaterialCommand>(
                        scene, materialEntity, state));
                RefreshInspector();
                RefreshStatus();
            });
            inspectorPanel_.AddWidget(&slider);
        };
        createDecalMaterialSlider(
            decalBaseColorRed_, "Decal Material Red", "BASE COLOR // R", 0);
        createDecalMaterialSlider(
            decalBaseColorGreen_, "Decal Material Green", "BASE COLOR // G", 1);
        createDecalMaterialSlider(
            decalBaseColorBlue_, "Decal Material Blue", "BASE COLOR // B", 2);
        createDecalMaterialSlider(
            decalOpacity_, "Decal Material Opacity", "OPACITY", 3);

        decalBaseColorTexture_.Create("Decal Base Color Texture");
        decalBaseColorTexture_.SetText("SELECT DECAL TEXTURE...");
        decalBaseColorTexture_.SetTooltip(
            "Choose a local image, import it as a governed Renegade texture, and bind it to this projected decal's base-colour/alpha slot.");
        decalBaseColorTexture_.OnClick([this](const wi::gui::EventArgs&)
        {
            ChooseSelectedDecalTexture();
        });
        inspectorPanel_.AddWidget(&decalBaseColorTexture_);

        createSectionLabel(
            environmentProbeLabel_,
            "Environment Probe Section",
            "ENVIRONMENT PROBE // NATIVE WICKED");
        environmentProbeResolution_.Create("Probe Resolution");
        for (const std::uint64_t resolution :
            {32ull, 64ull, 128ull, 256ull, 512ull, 1024ull, 2048ull})
        {
            environmentProbeResolution_.AddItem(
                std::to_string(resolution), resolution);
        }
        environmentProbeResolution_.SetTooltip(
            "Cubemap face resolution. Higher values cost more GPU memory and capture time.");
        environmentProbeResolution_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* probe = session_->Scenes().GetScene().probes.GetComponent(entity);
            if (probe == nullptr)
                return;
            auto state = bridge::CaptureEnvironmentProbe(*probe);
            state.resolution = static_cast<std::uint32_t>(args.userdata);
            CommitSelectedEnvironmentProbe(state);
        });
        inspectorPanel_.AddWidget(&environmentProbeResolution_);

        environmentProbeRealtime_.Create("Real-time update: ");
        environmentProbeRealtime_.SetTooltip(
            "Continuously recapture this probe using the configured interval.");
        environmentProbeRealtime_.OnClick([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* probe = session_->Scenes().GetScene().probes.GetComponent(entity);
            if (probe == nullptr)
                return;
            auto state = bridge::CaptureEnvironmentProbe(*probe);
            state.realTime = args.bValue;
            CommitSelectedEnvironmentProbe(state);
        });
        inspectorPanel_.AddWidget(&environmentProbeRealtime_);

        environmentProbeInterval_.Create(
            0.0f, 60.0f, 0.0f, 601.0f,
            "Probe Update Interval", "UPDATE INTERVAL // S");
        environmentProbeInterval_.OnValueCommitted([this](float value)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* probe = session_->Scenes().GetScene().probes.GetComponent(entity);
            if (probe == nullptr)
                return;
            auto state = bridge::CaptureEnvironmentProbe(*probe);
            state.updateInterval = value;
            CommitSelectedEnvironmentProbe(state);
        });
        inspectorPanel_.AddWidget(&environmentProbeInterval_);

        environmentProbeMsaa_.Create("8x MSAA capture: ");
        environmentProbeMsaa_.SetTooltip(
            "Use Wicked's native 8-sample MSAA environment-probe capture path.");
        environmentProbeMsaa_.OnClick([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* probe = session_->Scenes().GetScene().probes.GetComponent(entity);
            if (probe == nullptr)
                return;
            auto state = bridge::CaptureEnvironmentProbe(*probe);
            state.msaa = args.bValue;
            CommitSelectedEnvironmentProbe(state);
        });
        inspectorPanel_.AddWidget(&environmentProbeMsaa_);

        environmentProbeViewDistance_.Create(
            -1.0f, 5000.0f, -1.0f, 5002.0f,
            "Probe View Distance", "VIEW DISTANCE // -1 = CAMERA");
        environmentProbeViewDistance_.OnValueCommitted([this](float value)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* probe = session_->Scenes().GetScene().probes.GetComponent(entity);
            if (probe == nullptr)
                return;
            auto state = bridge::CaptureEnvironmentProbe(*probe);
            state.viewDistance = value < 0.0f ? -1.0f : value;
            CommitSelectedEnvironmentProbe(state);
        });
        inspectorPanel_.AddWidget(&environmentProbeViewDistance_);

        environmentProbeRefresh_.Create("Refresh Environment Probe");
        environmentProbeRefresh_.SetText("REFRESH PROBE");
        environmentProbeRefresh_.SetTooltip(
            "Discard the generated cubemap and force Wicked to recapture this probe.");
        environmentProbeRefresh_.OnClick([this](const wi::gui::EventArgs&)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            if (bridge::RefreshEnvironmentProbe(
                    session_->Scenes().GetScene(), entity))
            {
                RefreshInspector();
                RefreshStatus();
            }
        });
        inspectorPanel_.AddWidget(&environmentProbeRefresh_);

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
            RequestDiagnosticAction(EditorAction::StartSunPreview);
        });
        inspectorPanel_.AddWidget(&sunPlayButton_);

        sunPauseButton_.Create("Pause Sun Preview");
        sunPauseButton_.SetText("PAUSE");
        sunPauseButton_.SetTooltip(
            "Pause the preview and commit its final time as one Undo step.");
        sunPauseButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::PauseSunPreview);
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
            RequestDiagnosticAction(EditorAction::SetOceanEnabled);
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
                RequestDiagnosticAction(EditorAction::ApplyOceanPreset);
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
            RequestDiagnosticAction(EditorAction::SetOceanResolution);
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
        createOceanSlider(oceanChoppyScale_, "Ocean Choppy Scale", "CHOPPINESS",
            "Longitudinal wave displacement.", OceanField::ChoppyScale,
            0.0f, 10.0f, 1000.0f);
        createOceanSlider(oceanTimeScale_, "Ocean Time Scale", "SIMULATION SPEED",
            "Speed of FFT wave evolution.", OceanField::TimeScale,
            0.0f, 4.0f, 4000.0f);
        createOceanSlider(oceanWindAzimuth_, "Ocean Wind Azimuth", "WIND DIRECTION",
            "Ocean-specific horizontal wind direction in degrees.",
            OceanField::WindAzimuth, -180.0f, 180.0f, 720.0f);
        createOceanSlider(oceanWindSpeed_, "Ocean Wind Speed", "WIND SPEED",
            "Ocean spectrum wind speed; changing it recreates the simulation.",
            OceanField::WindSpeed, 0.0f, 1200.0f, 1200.0f);
        createOceanSlider(oceanWindDependency_, "Ocean Wind Dependency", "WIND DEPENDENCY",
            "Smaller values strengthen alignment with wind direction.",
            OceanField::WindDependency, 0.0f, 1.0f, 1000.0f);
        createOceanSlider(oceanSurfaceDetail_, "Ocean Surface Detail", "SURFACE DETAIL",
            "Geometry detail from 1 to 10; high values cost GPU time.",
            OceanField::SurfaceDetail, 1.0f, 10.0f, 9.0f);
        createOceanSlider(oceanDisplacementTolerance_,
            "Ocean Displacement Tolerance", "EDGE TOLERANCE",
            "Reduces screen-edge glitches from large waves at a detail cost.",
            OceanField::DisplacementTolerance, 1.0f, 10.0f, 900.0f);
        createOceanSlider(oceanWaterRed_, "Ocean Water Red", "WATER RED",
            "Native water surface red channel.", OceanField::WaterRed,
            0.0f, 1.0f, 1000.0f);
        createOceanSlider(oceanWaterGreen_, "Ocean Water Green", "WATER GREEN",
            "Native water surface green channel.", OceanField::WaterGreen,
            0.0f, 1.0f, 1000.0f);
        createOceanSlider(oceanWaterBlue_, "Ocean Water Blue", "WATER BLUE",
            "Native water surface blue channel.", OceanField::WaterBlue,
            0.0f, 1.0f, 1000.0f);
        createOceanSlider(oceanWaterOpacity_, "Ocean Water Opacity", "WATER OPACITY",
            "Native water surface alpha.", OceanField::WaterOpacity,
            0.0f, 1.0f, 1000.0f);
        createOceanSlider(oceanExtinctionRed_, "Ocean Extinction Red", "DEPTH RED",
            "Native absorption/extinction red channel.",
            OceanField::ExtinctionRed, 0.0f, 1.0f, 1000.0f);
        createOceanSlider(oceanExtinctionGreen_, "Ocean Extinction Green", "DEPTH GREEN",
            "Native absorption/extinction green channel.",
            OceanField::ExtinctionGreen, 0.0f, 1.0f, 1000.0f);
        createOceanSlider(oceanExtinctionBlue_, "Ocean Extinction Blue", "DEPTH BLUE",
            "Native absorption/extinction blue channel.",
            OceanField::ExtinctionBlue, 0.0f, 1.0f, 1000.0f);

        createSectionLabel(
            terrainLabel_,
            "Terrain Section",
            "TERRAIN // GENERATION");
        createTerrainButton_.Create("Create Native Terrain");
        createTerrainButton_.SetText("CREATE TERRAIN");
        createTerrainButton_.SetTooltip(
            "Create and select one native streamed terrain component");
        createTerrainButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::CreateTerrain);
        });
        inspectorPanel_.AddWidget(&createTerrainButton_);

        terrainSizeReadout_.Create("Current Terrain Size");
        terrainSizeReadout_.SetText("CURRENT TERRAIN // 1.25 KM x 1.25 KM");
        terrainSizeReadout_.SetTooltip(
            "Authored finite terrain size. Expansion preserves every existing chunk.");
        inspectorPanel_.AddWidget(&terrainSizeReadout_);

        expandTerrainButton_.Create("Expand Terrain");
        expandTerrainButton_.SetText("EXPAND TERRAIN // +1 RING");
        expandTerrainButton_.SetTooltip(
            "Add one 66 m chunk ring on every side without restarting or erasing sculpting.");
        expandTerrainButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::ExpandTerrain);
        });
        inspectorPanel_.AddWidget(&expandTerrainButton_);

        const auto createTerrainSlider = [this](
            RenegadeSlider& input,
            const char* name,
            const char* label,
            const char* tooltip,
            const TerrainField field,
            const float minimum,
            const float maximum,
            const float steps)
        {
            input.Create(minimum, maximum, 0.0f, steps, name, label);
            input.SetTooltip(tooltip);
            input.OnDragStarted([this, field](const float)
            {
                BeginTerrainSlider(field);
            });
            input.OnValuePreview([this, field](const float value)
            {
                PreviewTerrainSlider(field, value);
            });
            input.OnValueCommitted([this, field](const float value)
            {
                CommitTerrainSlider(field, value);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createTerrainSlider(terrainChunkScale_, "Terrain Resolution",
            "VERTEX SPACING (M)",
            "Distance between sculptable terrain samples. 1 m is standard; "
            "larger spacing trades local detail for world coverage.",
            TerrainField::ChunkScale, 0.25f, 16.0f, 1575.0f);
        createTerrainSlider(terrainMinimumHeight_, "Terrain Minimum Height",
            "MIN HEIGHT", "Lowest generated terrain elevation.",
            TerrainField::MinimumHeight, -2000.0f, 1999.0f, 3999.0f);
        createTerrainSlider(terrainMaximumHeight_, "Terrain Maximum Height",
            "MAX HEIGHT", "Highest generated terrain elevation.",
            TerrainField::MaximumHeight, -1999.0f, 2000.0f, 3999.0f);
        createTerrainSlider(terrainLowAltitudeBlend_, "Terrain Rock Slope",
            "ROCK ON SLOPES", "Steepness required before rock appears; lower values put rock on gentler slopes.",
            TerrainField::LowAltitudeBlend, 0.0f, 1.0f, 1000.0f);
        createTerrainSlider(terrainBaseBlend_, "Terrain Low Height",
            "LOW-GROUND MATERIAL", "How far the low-ground material reaches up from minimum height.",
            TerrainField::BaseBlend, 0.0f, 1.0f, 1000.0f);
        createTerrainSlider(terrainSlopeBlend_, "Terrain High Height",
            "HIGH-GROUND MATERIAL", "How far the high-ground material reaches down from maximum height.",
            TerrainField::SlopeBlend, 0.0f, 1.0f, 1000.0f);
        createTerrainSlider(terrainLodBias_, "Terrain LOD Bias",
            "LOD BIAS", "Terrain detail bias; zero is the safe default.",
            TerrainField::LodBias, -4.0f, 4.0f, 800.0f);

        createSectionLabel(
            terrainMaterialLabel_,
            "Terrain Material Section",
            "MATERIAL // DEFAULT GRASS");
        terrainMaterialPreset_.Create("Terrain Material Preset");
        terrainMaterialPreset_.AddItem("CUSTOM", 0u);
        terrainMaterialPreset_.AddItem(
            "MEADOW // 8X",
            static_cast<std::uint64_t>(
                bridge::TerrainMaterialPreset::Meadow) + 1u);
        terrainMaterialPreset_.AddItem(
            "COARSE GRASS // 12X",
            static_cast<std::uint64_t>(
                bridge::TerrainMaterialPreset::CoarseGrass) + 1u);
        terrainMaterialPreset_.AddItem(
            "FINE GROUND COVER // 16X",
            static_cast<std::uint64_t>(
                bridge::TerrainMaterialPreset::FineGroundCover) + 1u);
        terrainMaterialPreset_.SetTooltip(
            "Change grass density live without regenerating texture files.");
        terrainMaterialPreset_.OnSelect(
            [this](const wi::gui::EventArgs& args)
        {
            if (args.userdata > 0u)
            {
                pendingTerrainMaterialPreset_ =
                    static_cast<bridge::TerrainMaterialPreset>(
                        args.userdata - 1u);
                RequestDiagnosticAction(EditorAction::ApplyTerrainMaterialPreset);
            }
        });
        inspectorPanel_.AddWidget(&terrainMaterialPreset_);

        terrainTextureScale_.Create(
            1.0f,
            bridge::DefaultGrassPackedTileCount,
            bridge::DefaultGrassTextureScale,
            31.0f,
            "Terrain Texture Scale",
            "TEXTURE SCALE");
        terrainTextureScale_.SetTooltip(
            "Visible grass repeats. Updates live and is stored in WISCENE.");
        terrainTextureScale_.OnDragStarted([this](const float)
        {
            BeginTerrainTextureScale();
        });
        terrainTextureScale_.OnValuePreview([this](const float value)
        {
            PreviewTerrainTextureScale(value);
        });
        terrainTextureScale_.OnValueCommitted([this](const float value)
        {
            CommitTerrainTextureScale(value);
        });
        inspectorPanel_.AddWidget(&terrainTextureScale_);

        terrainApplyDefaultGrassButton_.Create("Apply Default Grass");
        terrainApplyDefaultGrassButton_.SetText("APPLY DEFAULT");
        terrainApplyDefaultGrassButton_.SetTooltip(
            "Assign the bundled grass to all four terrain material regions.");
        terrainApplyDefaultGrassButton_.OnClick(
            [this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::ApplyDefaultGrass);
        });
        inspectorPanel_.AddWidget(&terrainApplyDefaultGrassButton_);

        terrainReloadMaterialButton_.Create("Reload Terrain Material");
        terrainReloadMaterialButton_.SetText("RELOAD FILES");
        terrainReloadMaterialButton_.SetTooltip(
            "Reload changed bundled texture files without rebuilding Studio.");
        terrainReloadMaterialButton_.OnClick(
            [this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::ReloadTerrainMaterial);
        });
        inspectorPanel_.AddWidget(&terrainReloadMaterialButton_);

        createSectionLabel(terrainSculptLabel_, "Terrain Sculpt Section", "SCULPT // VIEWPORT BRUSH");
        terrainSculptMode_.Create("Terrain Sculpt Mode");
        terrainSculptMode_.AddItem("RAISE", static_cast<std::uint64_t>(bridge::TerrainSculptMode::Raise));
        terrainSculptMode_.AddItem("LOWER", static_cast<std::uint64_t>(bridge::TerrainSculptMode::Lower));
        terrainSculptMode_.AddItem("SMOOTH", static_cast<std::uint64_t>(bridge::TerrainSculptMode::Smooth));
        terrainSculptMode_.AddItem("FLATTEN", static_cast<std::uint64_t>(bridge::TerrainSculptMode::Flatten));
        terrainSculptMode_.SetTooltip("Choose how dragging the left mouse button changes terrain.");
        terrainSculptMode_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            terrainSculptModeValue_ = static_cast<bridge::TerrainSculptMode>(args.userdata);
        });
        inspectorPanel_.AddWidget(&terrainSculptMode_);
        const auto createBrushSlider = [this](RenegadeSlider& slider, const char* name,
            const char* label, const char* tooltip, float minimum, float maximum,
            float value, float steps, float* target)
        {
            slider.Create(minimum, maximum, value, steps, name, label);
            slider.SetTooltip(tooltip);
            const auto update = [this, target](const float value)
            {
                *target = value;
                std::ostringstream brush;
                brush << "BRUSH // SIZE " << std::fixed
                      << std::setprecision(0) << terrainBrushRadiusValue_
                      << " // STRENGTH " << std::setprecision(2)
                      << terrainBrushStrengthValue_;
                terrainBrushReadout_.SetText(brush.str());
            };
            slider.OnValuePreview(update);
            slider.OnValueCommitted(update);
            inspectorPanel_.AddWidget(&slider);
        };
        createBrushSlider(terrainBrushRadius_, "Terrain Brush Radius", "BRUSH SIZE",
            "Radius of the brush in world units.", 1.0f, 100.0f, 12.0f, 990.0f, &terrainBrushRadiusValue_);
        createBrushSlider(terrainBrushStrength_, "Terrain Brush Strength", "STRENGTH",
            "Height change applied while dragging.", 0.05f, 5.0f, 1.0f, 990.0f, &terrainBrushStrengthValue_);
        createBrushSlider(terrainBrushFalloff_, "Terrain Brush Falloff", "FALLOFF",
            "Zero is soft; one concentrates the effect at the centre.", 0.0f, 1.0f, 0.55f, 1000.0f, &terrainBrushFalloffValue_);

        terrainBrushReadout_.Create("Terrain Brush Readout");
        terrainBrushReadout_.SetText("BRUSH // SIZE 12 // STRENGTH 1.00");
        inspectorPanel_.AddWidget(&terrainBrushReadout_);
        terrainStrokeDiagnostic_.Create("Terrain Stroke Diagnostic");
        terrainStrokeDiagnostic_.SetText("LAST STROKE // READY");
        inspectorPanel_.AddWidget(&terrainStrokeDiagnostic_);

        CreateWd01VegetationControls();

        focusButton_.Create("Focus Selected");
        focusButton_.SetText("FOCUS [F]");
        focusButton_.SetTooltip("Frame the selected entity in the viewport");
        focusButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::FocusSelection);
        });
        inspectorPanel_.AddWidget(&focusButton_);

        duplicateButton_.Create("Duplicate Selected");
        duplicateButton_.SetText("DUPLICATE");
        duplicateButton_.SetTooltip("Duplicate selected entity (Ctrl+D)");
        duplicateButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::DuplicateSelection);
        });
        inspectorPanel_.AddWidget(&duplicateButton_);

        deleteButton_.Create("Delete Selected");
        deleteButton_.SetText("DELETE");
        deleteButton_.SetTooltip("Delete selected entity (Delete)");
        deleteButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::DeleteSelection);
        });
        inspectorPanel_.AddWidget(&deleteButton_);

        undoButton_.Create("Undo Transform");
        undoButton_.SetText("UNDO");
        undoButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        undoButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::Undo);
        });
        inspectorPanel_.AddWidget(&undoButton_);

        redoButton_.Create("Redo Transform");
        redoButton_.SetText("REDO");
        redoButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        redoButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::Redo);
        });
        inspectorPanel_.AddWidget(&redoButton_);

        saveButton_.Create("Save Scene");
        saveButton_.SetText("SAVE");
        saveButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        saveButton_.SetTooltip("Save the current scene (Ctrl+S)");
        saveButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::SaveScene);
        });
        inspectorPanel_.AddWidget(&saveButton_);

        saveAsButton_.Create("Save Scene As");
        saveAsButton_.SetText("SAVE AS...");
        saveAsButton_.SetSize(XMFLOAT2(112.0f, 28.0f));
        saveAsButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::SaveSceneAs);
        });
        inspectorPanel_.AddWidget(&saveAsButton_);

        reopenButton_.Create("Reopen Scene");
        reopenButton_.SetText("REOPEN");
        reopenButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        reopenButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::ReopenScene);
        });
        inspectorPanel_.AddWidget(&reopenButton_);

        contentPanel_.Create(
            "Content Browser",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        contentPanel_.SetShadowRadius(8.0f);
        GetGUI().AddWidget(&contentPanel_);

        contentLabel_.Create("Content Browser Title");
        contentLabel_.SetText("CONTENT // PROJECT ASSETS");
        contentLabel_.font.params.size = 16;
        contentLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        contentPanel_.AddWidget(&contentLabel_);

        contentPlaceholder_.Create("Content Browser Placeholder");
        contentPlaceholder_.SetText(
            "No assets imported yet.\n\n"
            "Asset import and the project-aware browser are not built. Until "
            "they are, scenes are authored from the generated Proving Ground "
            "and edited in the viewport.");
        contentPlaceholder_.SetFitTextEnabled(true);
        contentPlaceholder_.font.params.color = HologramMuted;
        contentPlaceholder_.font.params.size = 14;
        contentPanel_.AddWidget(&contentPlaceholder_);

        // The proof slice is a Renegade-owned renderer. It is added after the
        // legacy widgets so it sits behind future interactive components in
        // wiGUI's back-to-front render order. The legacy workspace panels are
        // hidden by SetProjectHubVisible(); they are retained temporarily as
        // a behavioural reference, not used as the finished presentation.
        studioChrome_.Create();
        studioChrome_.OnHierarchySelected(
            [this](const std::uint64_t entity)
        {
            if (session_ == nullptr)
            {
                return;
            }
            session_->Selection().Select(
                static_cast<wi::ecs::Entity>(entity));
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            RefreshHierarchy();
            RefreshInspector();
            RefreshStatus();
        });
        studioChrome_.OnToolSelected([this](const int tool)
        {
            pendingAction_ = tool == 0
                ? EditorAction::SelectTool
                : tool == 1
                    ? EditorAction::TranslateTool
                    : tool == 2
                        ? EditorAction::RotateTool
                        : EditorAction::ScaleTool;
        });
        studioChrome_.OnAction(
            [this](const RenegadeStudioChrome::Action action)
        {
            if (action == RenegadeStudioChrome::Action::ProjectHub ||
                action == RenegadeStudioChrome::Action::SceneWorkspace ||
                action == RenegadeStudioChrome::Action::EnvironmentWorkspace ||
                action == RenegadeStudioChrome::Action::TerrainWorkspace ||
                action == RenegadeStudioChrome::Action::RenderWorkspace)
            {
                ResetS1BInspectorDisclosure();
            }
            switch (action)
            {
            case RenegadeStudioChrome::Action::ProjectHub:
                RequestDiagnosticAction(EditorAction::ProjectHub);
                break;
            case RenegadeStudioChrome::Action::OpenScene:
                RequestDiagnosticAction(EditorAction::OpenScene);
                break;
            case RenegadeStudioChrome::Action::Save:
                RequestDiagnosticAction(EditorAction::SaveScene);
                break;
            case RenegadeStudioChrome::Action::SaveAs:
                RequestDiagnosticAction(EditorAction::SaveSceneAs);
                break;
            case RenegadeStudioChrome::Action::Reopen:
                RequestDiagnosticAction(EditorAction::ReopenScene);
                break;
            case RenegadeStudioChrome::Action::Undo:
                RequestDiagnosticAction(EditorAction::Undo);
                break;
            case RenegadeStudioChrome::Action::Redo:
                RequestDiagnosticAction(EditorAction::Redo);
                break;
            case RenegadeStudioChrome::Action::Duplicate:
                RequestDiagnosticAction(EditorAction::DuplicateSelection);
                break;
            case RenegadeStudioChrome::Action::Delete:
                RequestDiagnosticAction(EditorAction::DeleteSelection);
                break;
            case RenegadeStudioChrome::Action::CreatePointLight:
                pendingLightType_ = wi::scene::LightComponent::POINT;
                RequestDiagnosticAction(EditorAction::CreateLight);
                break;
            case RenegadeStudioChrome::Action::CreateSpotLight:
                pendingLightType_ = wi::scene::LightComponent::SPOT;
                RequestDiagnosticAction(EditorAction::CreateLight);
                break;
            case RenegadeStudioChrome::Action::CreateDirectionalLight:
                pendingLightType_ = wi::scene::LightComponent::DIRECTIONAL;
                RequestDiagnosticAction(EditorAction::CreateLight);
                break;
            case RenegadeStudioChrome::Action::CreateRectangleLight:
                pendingLightType_ = wi::scene::LightComponent::RECTANGLE;
                RequestDiagnosticAction(EditorAction::CreateLight);
                break;
            case RenegadeStudioChrome::Action::CreatePlayerStart:
                RequestDiagnosticAction(EditorAction::CreatePlayerStart);
                break;
            case RenegadeStudioChrome::Action::CreateCamera:
                RequestDiagnosticAction(EditorAction::CreateCamera);
                break;
            case RenegadeStudioChrome::Action::CreateDecal:
                RequestDiagnosticAction(EditorAction::CreateDecal);
                break;
            case RenegadeStudioChrome::Action::CreateEnvironmentProbe:
                RequestDiagnosticAction(EditorAction::CreateEnvironmentProbe);
                break;
            case RenegadeStudioChrome::Action::Focus:
                RequestDiagnosticAction(EditorAction::FocusSelection);
                break;
            case RenegadeStudioChrome::Action::ToggleGrid:
                RequestDiagnosticAction(EditorAction::ToggleGrid);
                break;
            case RenegadeStudioChrome::Action::EnvironmentWorkspace:
                RequestDiagnosticAction(EditorAction::OpenEnvironmentWorkspace);
                break;
            case RenegadeStudioChrome::Action::TerrainWorkspace:
                RequestDiagnosticAction(EditorAction::OpenTerrainWorkspace);
                break;
            case RenegadeStudioChrome::Action::RenderWorkspace:
                RequestDiagnosticAction(EditorAction::OpenRenderWorkspace);
                break;
            case RenegadeStudioChrome::Action::SceneWorkspace:
                RequestDiagnosticAction(EditorAction::OpenSceneWorkspace);
                break;
            case RenegadeStudioChrome::Action::TestLevelPlay:
                RequestDiagnosticAction(EditorAction::StartTestLevel);
                break;
            case RenegadeStudioChrome::Action::TestLevelStop:
                RequestDiagnosticAction(EditorAction::StopTestLevel);
                break;
            case RenegadeStudioChrome::Action::BuildWindowsGame:
                RequestDiagnosticAction(EditorAction::BuildWindowsGame);
                break;
            case RenegadeStudioChrome::Action::ValidateModelImport:
                RequestDiagnosticAction(EditorAction::ValidateModelImport);
                break;
            case RenegadeStudioChrome::Action::ImportModel:
                RequestDiagnosticAction(EditorAction::ImportModel);
                break;
            }
        });
        studioChrome_.OnDrawerChanged([this](const int tab)
        {
            if (tab >= 0)
            {
                lastDrawerTab_ = tab;
            }
            if (tab == 0)
            {
                RefreshAssetBrowser();
            }
            if (session_ == nullptr)
            {
                return;
            }
            auto& projects = session_->Projects();
            projects.SetEditorPreference("drawer_open", tab >= 0);
            for (int index = 0; index < 4; ++index)
            {
                projects.SetEditorPreference(
                    "drawer_tab_" + std::to_string(index),
                    lastDrawerTab_ == index);
            }
        });
        studioChrome_.OnAssetBrowserFolderSelected(
            [this](const std::string& relativePath)
        {
            SelectAssetBrowserFolder(relativePath);
        });
        studioChrome_.OnAssetBrowserItemSelected(
            [this](const std::string& relativePath)
        {
            SelectAssetBrowserItem(relativePath);
        });
        studioChrome_.OnCreatorAssetPlaceRequested(
            [this](
                const bridge::StableId& assetId,
                const std::string& label)
        {
            BeginCreatorAssetPlacement(assetId, label);
        });
        studioChrome_.OnCreatorAssetDropped(
            [this](
                const bridge::StableId& assetId,
                const std::string& label,
                const float x,
                const float y)
        {
            DropCreatorAsset(assetId, label, x, y);
        });
        studioChrome_.OnLayoutChanged(
            [this](
                const float hierarchyWidth,
                const float inspectorWidth,
                const float drawerHeight,
                const bool finished)
        {
            workspaceLayoutDirty_ = true;
            if (!finished || session_ == nullptr)
            {
                return;
            }

            // ProjectService currently exposes durable boolean preferences.
            // Encode the three bounded pixel dimensions without bypassing the
            // service or leaking editor layout into project/scene data.
            auto& projects = session_->Projects();
            WriteLayoutPreference(
                projects,
                "hierarchy_width",
                static_cast<int>(std::round(hierarchyWidth)));
            WriteLayoutPreference(
                projects,
                "inspector_width",
                static_cast<int>(std::round(inspectorWidth)));
            WriteLayoutPreference(
                projects,
                "drawer_height",
                static_cast<int>(std::round(drawerHeight)));
            projects.SetEditorPreference("workspace_layout_saved", true);
        });
        // Audio is an independent top-level authoring surface. Keep the
        // accepted Inspector alive and untouched underneath it: Wicked's
        // Window::SetVisible(true) makes every child visible, so using parent
        // visibility as a z-order switch corrupts per-section Inspector state.
        // Register Audio ahead of the Inspector in Wicked's reverse render
        // order, then keep the chrome behind both authoring surfaces.
        GetGUI().RemoveWidget(&inspectorPanel_);
        GetGUI().AddWidget(&studioChrome_.AudioWorkspace());
        GetGUI().AddWidget(&inspectorPanel_);
        GetGUI().AddWidget(&studioChrome_);
    }

    void StudioRenderPath::CreateProjectHub()
    {
        projectHubPanel_.Create(
            "Renegade Project Hub",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        projectHubPanel_.SetShadowRadius(0.0f);
        GetGUI().AddWidget(&projectHubPanel_);

        hubBrandLabel_.Create("Renegade Hub Brand");
        hubBrandLabel_.SetText("RENEGADE");
        hubBrandLabel_.font.params.size = 15;
        hubBrandLabel_.font.params.bolden = 0.30f;
        hubBrandLabel_.font.params.color = HubOrange;
        hubBrandLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&hubBrandLabel_);

        hubTitleLabel_.Create("Renegade Project Hub Title");
        hubTitleLabel_.SetText("PROJECT HUB");
        hubTitleLabel_.font.params.size = 38;
        hubTitleLabel_.font.params.bolden = 0.28f;
        hubTitleLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&hubTitleLabel_);

        hubSubtitleLabel_.Create("Renegade Project Hub Subtitle");
        hubSubtitleLabel_.SetText(
            "PROJECT LIFECYCLE CONTROL // CREATE // OPEN // CONTINUE");
        hubSubtitleLabel_.font.params.size = 14;
        hubSubtitleLabel_.font.params.color = HubMuted;
        hubSubtitleLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&hubSubtitleLabel_);

        projectNameInput_.Create("New Project Name");
        projectNameInput_.SetDescription("NEW PROJECT // NAME: ");
        projectNameInput_.SetText("New Renegade Project");
        projectNameInput_.SetCancelInputEnabled(false);
        projectNameInput_.SetTooltip("Name the project, then choose its parent folder");
        projectHubPanel_.AddWidget(&projectNameInput_);

        createProjectButton_.Create("Create Renegade Project");
        createProjectButton_.SetText("CREATE NEW PROJECT");
        createProjectButton_.SetTooltip(
            "Choose a parent folder and create a project from the Proving Ground");
        createProjectButton_.SetAngularHighlightWidth(3.0f);
        createProjectButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            CreateProject();
        });
        projectHubPanel_.AddWidget(&createProjectButton_);

        openProjectButton_.Create("Open Renegade Project");
        openProjectButton_.SetText("OPEN PROJECT...");
        openProjectButton_.SetTooltip(
            "Open an existing Renegade project descriptor (.renegade)");
        openProjectButton_.SetAngularHighlightWidth(3.0f);
        openProjectButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            OpenProject();
        });
        projectHubPanel_.AddWidget(&openProjectButton_);

        // Project Hub is deliberately project-level. OPEN SCENE remains an
        // editor command in the Renegade Studio chrome, but is not presented
        // as a primary startup action here.
        openSceneButton_.Create("Open Renegade Scene");
        openSceneButton_.SetText("OPEN SCENE...");
        openSceneButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            OpenScene();
        });
        openSceneButton_.SetVisible(false);
        projectHubPanel_.AddWidget(&openSceneButton_);

        recentProjectsLabel_.Create("Recent Projects");
        recentProjectsLabel_.SetText("RECENT PROJECTS");
        recentProjectsLabel_.font.params.size = 17;
        recentProjectsLabel_.font.params.bolden = 0.22f;
        recentProjectsLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&recentProjectsLabel_);

        for (std::size_t index = 0; index < recentProjectButtons_.size(); ++index)
        {
            auto& button = recentProjectButtons_[index];
            button.Create("Recent Project " + std::to_string(index));
            button.SetText("");
            button.SetAngularHighlightWidth(2.0f);
            button.SetShadowRadius(1.0f);
            button.font.params.size = 14;
            button.font.params.h_align = wi::font::WIFALIGN_LEFT;
            button.font.params.v_align = wi::font::WIFALIGN_CENTER;
            button.OnClick([this, index](const wi::gui::EventArgs&)
            {
                SelectRecentProject(index);
            });
            projectHubPanel_.AddWidget(&button);
        }

        selectedProjectLabel_.Create("Selected Project");
        selectedProjectLabel_.SetText("PROJECT DETAILS");
        selectedProjectLabel_.SetFitTextEnabled(true);
        selectedProjectLabel_.font.params.size = 15;
        selectedProjectLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        selectedProjectLabel_.font.params.v_align = wi::font::WIFALIGN_TOP;
        projectHubPanel_.AddWidget(&selectedProjectLabel_);

        launchProjectButton_.Create("Launch Selected Project");
        launchProjectButton_.SetText("OPEN PROJECT");
        launchProjectButton_.SetAngularHighlightWidth(3.0f);
        launchProjectButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            OpenSelectedRecentProject();
        });
        projectHubPanel_.AddWidget(&launchProjectButton_);

        continueProjectButton_.Create("Continue Current Project");
        continueProjectButton_.SetText("BACK TO EDITOR");
        continueProjectButton_.SetTooltip(
            "Close Project Hub and return to the currently active project");
        continueProjectButton_.SetAngularHighlightWidth(2.0f);
        continueProjectButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            SetProjectHubVisible(false);
        });
        projectHubPanel_.AddWidget(&continueProjectButton_);

        hubMessageLabel_.Create("Project Hub Message");
        hubMessageLabel_.SetText(
            "PROJECT SERVICES // ONLINE     FORMAT // RENEGADE PROJECT V1");
        hubMessageLabel_.SetFitTextEnabled(true);
        hubMessageLabel_.font.params.size = 13;
        hubMessageLabel_.font.params.color = HubMuted;
        hubMessageLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&hubMessageLabel_);
        projectHubPanel_.SetVisible(false);

        projectLoadingOverlay_.Create();
        projectLoadingOverlay_.OnReturnToHub([this]()
        {
            projectLoadingOverlay_.SetVisible(false);
            RefreshProjectHub();
            SetProjectHubVisible(true);
        });
        GetGUI().AddWidget(&projectLoadingOverlay_);

        projectHubChrome_.Create();
        const auto savedIdentity = StudioUserPreferences::LoadDeveloperIdentity();
        projectHubChrome_.SetDeveloperIdentity(savedIdentity.has_value()
            ? fs::path(*savedIdentity).u8string() : std::string("DEVELOPER"));
        projectHubChrome_.SetStatusProvider([this]() { return hubMessageLabel_.GetText(); });
        projectHubChrome_.OnRecentProjectSelected([this](std::size_t index) { SelectRecentProject(index); });
        projectHubChrome_.OnAction([this](RenegadeProjectHub::Action action)
        {
            switch (action)
            {
            case RenegadeProjectHub::Action::NewProject:
                hubNewProjectMode_ = true;
                projectHubChrome_.SetNewProjectMode(true);
                hubNewProjectNameInput_.SetText("New Renegade Project");
                hubNewProjectNameInput_.SetVisible(true);
                hubNewProjectConfirmButton_.SetVisible(true);
                hubNewProjectCancelButton_.SetVisible(true);
                break;
            case RenegadeProjectHub::Action::OpenProject:
                // Defer the native browser until the next thread-safe point.
                // Calling it directly from the custom Hub input pass can race
                // the GUI/input update and was observed by the owner as a
                // dead OPEN PROJECT control.
                hubMessageLabel_.font.params.color = HologramMuted;
                hubMessageLabel_.SetText("PROJECT BROWSER // OPENING");
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this](uint64_t) { OpenProject(); });
                break;
            case RenegadeProjectHub::Action::OpenSelectedProject: OpenSelectedRecentProject(); break;
            case RenegadeProjectHub::Action::BackToEditor:
                if (session_ && session_->Projects().HasProject()) SetProjectHubVisible(false);
                break;
            case RenegadeProjectHub::Action::ExitRenegade:
                RequestExit();
                break;
            case RenegadeProjectHub::Action::CancelNewProject:
                hubNewProjectMode_ = false;
                projectHubChrome_.SetNewProjectMode(false);
                hubNewProjectNameInput_.SetVisible(false);
                hubNewProjectConfirmButton_.SetVisible(false);
                hubNewProjectCancelButton_.SetVisible(false);
                break;
            }
        });
        projectHubChrome_.SetVisible(projectHubVisible_);

        hubNewProjectNameInput_.Create("Hub New Project Name");
        hubNewProjectNameInput_.SetPlaceholder("PROJECT NAME");
        hubNewProjectNameInput_.SetText("New Renegade Project");
        hubNewProjectNameInput_.SetCancelInputEnabled(false);
        hubNewProjectNameInput_.OnInputAccepted([this](const wi::gui::EventArgs&) { CreateProject(); });
        hubNewProjectNameInput_.SetVisible(false);
        GetGUI().AddWidget(&hubNewProjectNameInput_);
        hubNewProjectConfirmButton_.Create("Hub Create Project Confirm");
        hubNewProjectConfirmButton_.SetText("CREATE PROJECT");
        hubNewProjectConfirmButton_.OnClick([this](const wi::gui::EventArgs&) { CreateProject(); });
        hubNewProjectConfirmButton_.SetVisible(false);
        GetGUI().AddWidget(&hubNewProjectConfirmButton_);
        hubNewProjectCancelButton_.Create("Hub Create Project Cancel");
        hubNewProjectCancelButton_.SetText("CANCEL");
        hubNewProjectCancelButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            hubNewProjectMode_ = false;
            projectHubChrome_.SetNewProjectMode(false);
            hubNewProjectNameInput_.SetVisible(false);
            hubNewProjectConfirmButton_.SetVisible(false);
            hubNewProjectCancelButton_.SetVisible(false);
        });
        hubNewProjectCancelButton_.SetVisible(false);
        GetGUI().AddWidget(&hubNewProjectCancelButton_);

        // Wicked GUI renders top-level widgets back-to-front. Register the
        // authored Hub chrome after its native NEW PROJECT controls so the
        // input/CREATE/CANCEL controls render above the modal on their very
        // first visible frame instead of only being promoted after a click.
        GetGUI().AddWidget(&projectHubChrome_);
    }

    // A small, self-contained popup rather than a new row wedged into the
    // Inspector's Transform section: the Inspector's layout is a long chain
    // of hardcoded absolute pixel positions (see ResizeLayout), and
    // inserting a row there would mean renumbering every row below it with
    // no way to verify the result short of a packaged build. This window
    // owns its own position/size in ResizeLayout instead, independent of
    // that chain, and only appears right after ADD > IMPORT MODEL... places
    // a model.
    void StudioRenderPath::CreateImportScalePanel()
    {
        importScalePanel_.Create(
            "Model Import Workspace",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR |
            wi::gui::Window::WindowControls::RESIZE_LEFT);
        importScalePanel_.OnResize([this]()
        {
            if (importInspectorLayoutInProgress_)
                return;
            importInspectorWidth_ = std::clamp(
                importScalePanel_.GetSize().x, 310.0f, 680.0f);
            ResizeLayout();
        });
        // Registration is deferred until every importer page is attached.

        importScaleTitleLabel_.Create("MODEL IMPORTER // PREVIEW BEFORE COMMIT");
        importScaleReadoutLabel_.Create("");
        creatorImportHelpLabel.Create(
            "The model is temporary. The project is unchanged until CONFIRM IMPORT is pressed.");
        creatorImportHelpLabel.SetFitTextEnabled(true);

        constexpr const char* stageNames[] = {
            "ASSET SETUP", "TRANSFORM & SCALE", "MATERIALS & TEXTURES",
            "RIG & RETARGETING", "ANIMATIONS", "REVIEW & IMPORT"};
        for (std::size_t index = 0; index < creatorImportStageButtons.size(); ++index)
        {
            auto& heading = creatorImportStageButtons[index];
            heading.Create(std::string("Importer Stage ") + stageNames[index]);
            heading.SetText(stageNames[index]);
            heading.OnClick([this, index](const wi::gui::EventArgs&)
            {
                if (!creatorModelImporter.importAsCharacter &&
                    (index == 3 || index == 4))
                    return;
                creatorModelImporter.workspaceSection = index;
                importScalePanel_.scrollbar_vertical.SetOffset(0.0f);
                RefreshCreatorImportWorkspaceSection();
                ResizeLayout();
            });
        }
        creatorImportModelChoice.Create("Import as Model");
        creatorImportModelChoice.SetText("MODEL");
        creatorImportModelChoice.OnClick([this](const wi::gui::EventArgs&)
        {
            if (!creatorModelImporter.externalAnimations.empty())
            {
                creatorImportExternalAnimationStatus.SetText(
                    "Remove queued external animations before switching to Model.");
                return;
            }
            StopCreatorImportPreviewAnimations();
            creatorModelImporter.importAsCharacter = false;
            creatorModelImporter.destinationFolder = "Content/Models";
            creatorImportDestination.SetValue(creatorModelImporter.destinationFolder);
            importScaleTitleLabel_.SetText("MODEL IMPORTER // PREVIEW BEFORE COMMIT");
            RefreshCreatorImportWorkspaceSection();
            ResizeLayout();
        });
        creatorImportCharacterChoice.Create("Import as Character");
        creatorImportCharacterChoice.SetText("CHARACTER");
        creatorImportCharacterChoice.OnClick([this](const wi::gui::EventArgs&)
        {
            creatorModelImporter.importAsCharacter = true;
            creatorModelImporter.destinationFolder = "Content/Characters";
            creatorImportDestination.SetValue(creatorModelImporter.destinationFolder);
            importScaleTitleLabel_.SetText("CHARACTER IMPORTER // RIG REVIEW REQUIRED");
            RefreshCreatorImportWorkspaceSection();
            ResizeLayout();
        });
        creatorImportRigReadout.Create("");
        creatorImportRigReadout.SetFitTextEnabled(true);

        creatorImportAssetName.Create("Creator Asset Name");
        creatorImportAssetName.SetPlaceholder("ASSET NAME");
        creatorImportAssetName.OnInput([](const wi::gui::EventArgs& args)
        {
            creatorModelImporter.assetName = args.sValue;
        });
        creatorImportAssetName.OnInputAccepted([](const wi::gui::EventArgs& args)
        {
            creatorModelImporter.assetName = args.sValue;
        });
        creatorImportDestination.Create("Creator Asset Destination");
        creatorImportDestination.SetPlaceholder("Content/Models");
        creatorImportDestination.OnInput([](const wi::gui::EventArgs& args)
        {
            creatorModelImporter.destinationFolder = args.sValue;
        });
        creatorImportDestination.OnInputAccepted([](const wi::gui::EventArgs& args)
        {
            creatorModelImporter.destinationFolder = args.sValue;
        });

        creatorImportTransformLabel.Create("TRANSFORM // PREVIEW");
        const auto createImportTransformSlider = [](
            RenegadeSlider& field,
            const char* name,
            const char* label,
            const float minimum,
            const float maximum,
            const float initial,
            const bool rotation,
            const int axis)
        {
            field.Create(minimum, maximum, initial, 2000.0f, name, label);
            field.OnValuePreview([rotation, axis](const float value)
            {
                XMFLOAT3& target = rotation
                    ? creatorModelImporter.rotationDegrees
                    : creatorModelImporter.positionOffset;
                if (axis == 0) target.x = value;
                if (axis == 1) target.y = value;
                if (axis == 2) target.z = value;
                ApplyCreatorImportPreviewTransform();
            });
        };
        createImportTransformSlider(creatorImportPositionX, "Import Position X", "POSITION X", -100.0f, 100.0f, 0.0f, false, 0);
        createImportTransformSlider(creatorImportPositionY, "Import Position Y", "POSITION Y", -100.0f, 100.0f, 0.0f, false, 1);
        createImportTransformSlider(creatorImportPositionZ, "Import Position Z", "POSITION Z", -100.0f, 100.0f, 0.0f, false, 2);
        createImportTransformSlider(creatorImportRotationX, "Import Rotation X", "ROTATION X", -180.0f, 180.0f, 0.0f, true, 0);
        createImportTransformSlider(creatorImportRotationY, "Import Rotation Y", "ROTATION Y", -180.0f, 180.0f, 0.0f, true, 1);
        createImportTransformSlider(creatorImportRotationZ, "Import Rotation Z", "ROTATION Z", -180.0f, 180.0f, 0.0f, true, 2);

        const auto createScaleSlider = [](RenegadeSlider& field, const char* name, const char* label, const int axis)
        {
            field.Create(0.001f, 10.0f, 1.0f, 10000.0f, name, label);
            field.OnValuePreview([axis](const float value)
            {
                if (creatorModelImporter.scaleLinked)
                {
                    creatorModelImporter.scale = XMFLOAT3(value, value, value);
                    creatorImportScaleX.SetValue(value);
                    creatorImportScaleY.SetValue(value);
                    creatorImportScaleZ.SetValue(value);
                }
                else
                {
                    if (axis == 0) creatorModelImporter.scale.x = value;
                    if (axis == 1) creatorModelImporter.scale.y = value;
                    if (axis == 2) creatorModelImporter.scale.z = value;
                }
                ApplyCreatorImportPreviewTransform();
            });
        };
        createScaleSlider(creatorImportScaleX, "Import Scale X", "SCALE X", 0);
        createScaleSlider(creatorImportScaleY, "Import Scale Y", "SCALE Y", 1);
        createScaleSlider(creatorImportScaleZ, "Import Scale Z", "SCALE Z", 2);
        creatorImportScaleLinked.Create("Linked Import Scale");
        creatorImportScaleLinked.SetText("LINK XYZ SCALE");
        creatorImportScaleLinked.SetCheck(true);
        creatorImportScaleLinked.OnClick([](const wi::gui::EventArgs& args)
        {
            creatorModelImporter.scaleLinked = args.bValue;
        });

        creatorImportDimensionPreset.Create("Real World Size Preset");
        creatorImportDimensionPreset.AddItem("SOURCE SIZE");
        creatorImportDimensionPreset.AddItem("SMALL PROP // 0.50 M HIGH");
        creatorImportDimensionPreset.AddItem("HUMAN // 1.82 M HIGH");
        creatorImportDimensionPreset.AddItem("DOOR // 2.04 M HIGH");
        creatorImportDimensionPreset.AddItem("LARGE PROP // 3.00 M HIGH");
        creatorImportDimensionPreset.OnSelect([](const wi::gui::EventArgs& args)
        {
            const float sourceHeight = creatorModelImporter.sourceBounds.valid
                ? creatorModelImporter.sourceBounds.maximum.y - creatorModelImporter.sourceBounds.minimum.y
                : 0.0f;
            const float targets[] = {0.0f, 0.50f, 1.82f, 2.04f, 3.0f};
            const int selected = std::clamp(args.iValue, 0, 4);
            const float factor = selected == 0 || sourceHeight <= 0.0001f
                ? 1.0f : targets[selected] / sourceHeight;
            creatorModelImporter.scale = XMFLOAT3(factor, factor, factor);
            creatorImportScaleX.SetValue(factor);
            creatorImportScaleY.SetValue(factor);
            creatorImportScaleZ.SetValue(factor);
            ApplyCreatorImportPreviewTransform();
        });

        importScaleModeCombo_.Create("Scale Mode");
        importScaleModeCombo_.AddItem(
            "AUTOMATIC",
            static_cast<std::uint64_t>(bridge::ModelScaleMode::Automatic));
        importScaleModeCombo_.AddItem(
            "ORIGINAL / METRES",
            static_cast<std::uint64_t>(bridge::ModelScaleMode::Original));
        importScaleModeCombo_.AddItem(
            "CENTIMETRES",
            static_cast<std::uint64_t>(bridge::ModelScaleMode::Centimeters));
        importScaleModeCombo_.AddItem(
            "INCHES",
            static_cast<std::uint64_t>(bridge::ModelScaleMode::Inches));
        importScaleModeCombo_.SetSelectedWithoutCallback(0);
        importScaleModeCombo_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr || !creatorModelImporter.active)
                return;
            const auto mode = static_cast<bridge::ModelScaleMode>(args.userdata);
            pendingImportScaleMode_ = mode;
            const float factor = mode == bridge::ModelScaleMode::Automatic
                ? creatorModelImporter.automaticScale
                : mode == bridge::ModelScaleMode::Centimeters ? 0.01f
                : mode == bridge::ModelScaleMode::Inches ? 0.0254f
                : 1.0f;
            creatorModelImporter.scale = XMFLOAT3(factor, factor, factor);
            creatorImportScaleX.SetValue(factor);
            creatorImportScaleY.SetValue(factor);
            creatorImportScaleZ.SetValue(factor);
            ApplyCreatorImportPreviewTransform();
            importScaleAppliedFactor_ = factor;
        });

        creatorImportMaterialLabel.Create("MATERIALS // DETECTED MAPS");
        creatorImportMaterialCombo.Create("Material Slot");
        creatorImportMaterialCombo.OnSelect([](const wi::gui::EventArgs& args)
        {
            creatorModelImporter.selectedMaterial =
                static_cast<std::size_t>(args.userdata);
            RefreshCreatorImportMaterialReadout();
            RefreshCreatorImportTextureEditor();
            RefreshCreatorImportMaterialScalars();
        });
        creatorImportMaterialReadout.Create("");
        creatorImportMaterialReadout.SetFitTextEnabled(true);
        creatorImportTexturePreviews.SetName("Imported Material Map Previews");
        creatorImportTexturePreviews.SetShadowRadius(0.0f);
        creatorImportTexturePreviews.OnSlotSelected([](const std::size_t index)
        {
            creatorImportTextureSlot = index;
            creatorImportTextureSlotCombo.SetSelectedWithoutCallback(
                static_cast<int>(index));
            RefreshCreatorImportTextureEditor();
        });
        creatorImportTexturePreviews.OnBrowseRequested([](const std::size_t index)
        {
            creatorImportTextureSlot = index;
            creatorImportTextureSlotCombo.SetSelectedWithoutCallback(
                static_cast<int>(index));
            RefreshCreatorImportTextureEditor();
            OpenCreatorImportTextureBrowser();
        });
        creatorImportTextureHelp.Create("TEXTURE SLOT // AUTO-DETECT, REPLACE OR REMOVE");
        creatorImportTextureSlotCombo.Create("Texture Slot");
        for (const char* name : {"BASE COLOR", "NORMAL", "SURFACE (PACKED)", "ROUGHNESS", "METALNESS", "AO", "EMISSIVE"})
            creatorImportTextureSlotCombo.AddItem(name);
        creatorImportTextureSlotCombo.SetSelectedWithoutCallback(0);
        creatorImportTextureSlotCombo.OnSelect([](const wi::gui::EventArgs& args)
        {
            creatorImportTextureSlot = static_cast<std::size_t>(std::max(0, args.iValue));
            RefreshCreatorImportTextureEditor();
        });
        creatorImportTexturePath.Create("Texture Source Path");
        creatorImportTexturePath.SetPlaceholder("AUTO-DETECT");
        creatorImportTexturePath.OnInputAccepted([](const wi::gui::EventArgs& args)
        {
            if (!creatorModelImporter.active) return;
            auto& choice = SelectedCreatorTextureChoice();
            choice.overridden = true;
            choice.path = args.sValue;
            ApplyCreatorPreviewTextureChoice(choice.path);
            RefreshCreatorImportTextureEditor();
            RefreshCreatorImportMaterialReadout();
        });
        creatorImportTextureBrowse.Create("Browse Imported Material Texture");
        creatorImportTextureBrowse.SetText("BROWSE...");
        creatorImportTextureBrowse.OnClick([](const wi::gui::EventArgs&)
        {
            OpenCreatorImportTextureBrowser();
        });
        creatorImportTextureClear.Create("Clear Imported Material Texture");
        creatorImportTextureClear.SetText("REMOVE");
        creatorImportTextureClear.OnClick([](const wi::gui::EventArgs&)
        {
            if (!creatorModelImporter.active) return;
            auto& choice = SelectedCreatorTextureChoice();
            choice.overridden = true;
            choice.path.clear();
            ApplyCreatorPreviewTextureChoice({});
            RefreshCreatorImportTextureEditor();
            RefreshCreatorImportMaterialReadout();
        });

        creatorImportMaterialScalarLabel.Create("PBR VALUES // PREVIEW = COMMIT");
        const auto createMaterialScalar = [](
            RenegadeSlider& slider,
            const char* name,
            const char* label,
            const float minimum,
            const float maximum,
            float renegade::bridge::CreatorMaterialSourceOverride::* member)
        {
            slider.Create(minimum, maximum, 0.0f, 1000.0f, name, label);
            slider.OnValuePreview([member](const float value)
            {
                if (!creatorModelImporter.active ||
                    creatorModelImporter.materialEntities.empty())
                    return;
                EnsureCreatorMaterialOverride().*member = value;
                PreviewCreatorMaterialScalar(member, value);
            });
            slider.OnValueCommitted([member](const float value)
            {
                if (!creatorModelImporter.active ||
                    creatorModelImporter.materialEntities.empty())
                    return;
                EnsureCreatorMaterialOverride().*member = value;
                PreviewCreatorMaterialScalar(member, value);
            });
        };
        createMaterialScalar(creatorImportRoughness, "Import Roughness", "ROUGHNESS", 0.0f, 1.0f,
            &renegade::bridge::CreatorMaterialSourceOverride::roughnessValue);
        createMaterialScalar(creatorImportMetalness, "Import Metalness", "METALNESS", 0.0f, 1.0f,
            &renegade::bridge::CreatorMaterialSourceOverride::metalnessValue);
        createMaterialScalar(creatorImportReflectance, "Import Reflectance", "REFLECTANCE", 0.0f, 1.0f,
            &renegade::bridge::CreatorMaterialSourceOverride::reflectanceValue);
        createMaterialScalar(creatorImportNormalStrength, "Import Normal Strength", "NORMAL STRENGTH", 0.0f, 4.0f,
            &renegade::bridge::CreatorMaterialSourceOverride::normalStrengthValue);
        createMaterialScalar(creatorImportAoStrength, "Import AO Strength", "AO STRENGTH", 0.0f, 1.0f,
            &renegade::bridge::CreatorMaterialSourceOverride::aoStrengthValue);
        createMaterialScalar(creatorImportEmissiveStrength, "Import Emissive Strength", "EMISSIVE STRENGTH", 0.0f, 20.0f,
            &renegade::bridge::CreatorMaterialSourceOverride::emissiveStrengthValue);

        creatorImportLightingLabel.Create("PREVIEW LIGHTING // NEVER SAVED");
        const auto createLightingSlider = [](RenegadeSlider& slider,
            const char* name, const char* label, const float minimum,
            const float maximum, float* value)
        {
            slider.Create(minimum, maximum, *value, 1000.0f, name, label);
            slider.OnValuePreview([value](const float next)
            {
                *value = next;
                ApplyCreatorImportPreviewLighting();
            });
        };
        createLightingSlider(creatorImportLightIntensity, "Preview Light Intensity", "LIGHT INTENSITY", 0.0f, 20.0f, &creatorModelImporter.lightIntensity);
        createLightingSlider(creatorImportLightAzimuth, "Preview Light Azimuth", "HORIZONTAL DIRECTION", -180.0f, 180.0f, &creatorModelImporter.lightAzimuth);
        createLightingSlider(creatorImportLightElevation, "Preview Light Elevation", "ELEVATION", -10.0f, 90.0f, &creatorModelImporter.lightElevation);
        createLightingSlider(creatorImportAmbientBrightness, "Preview Ambient Brightness", "AMBIENT BRIGHTNESS", 0.0f, 2.0f, &creatorModelImporter.ambientBrightness);
        creatorImportLightingPreset.Create("Preview Lighting Preset");
        creatorImportLightingPreset.AddItem("NEUTRAL");
        creatorImportLightingPreset.AddItem("OUTDOOR");
        creatorImportLightingPreset.AddItem("DARK");
        creatorImportLightingReset.Create("Reset Neutral Preview Lighting");
        creatorImportLightingReset.SetText("RESET NEUTRAL LIGHTING");
        creatorImportMannequinVisible.Create("Human Scale Reference");
        creatorImportMannequinVisible.SetText("SHOW 1.82 M MALE REFERENCE");
        creatorImportMannequinVisible.SetCheck(true);
        creatorImportHumanReference = wi::resourcemanager::Load(
            "Content/ui/creator-human-reference.png");
        creatorImportMannequinVisible.OnClick([](const wi::gui::EventArgs& args)
        {
            creatorModelImporter.mannequinVisible = args.bValue;
        });

        const auto applyLightingPreset = [](const int preset)
        {
            if (preset == 1)
            {
                creatorModelImporter.lightIntensity = 7.0f;
                creatorModelImporter.lightAzimuth = -45.0f;
                creatorModelImporter.lightElevation = 50.0f;
                creatorModelImporter.ambientBrightness = 0.55f;
            }
            else if (preset == 2)
            {
                creatorModelImporter.lightIntensity = 1.5f;
                creatorModelImporter.lightAzimuth = 25.0f;
                creatorModelImporter.lightElevation = 20.0f;
                creatorModelImporter.ambientBrightness = 0.08f;
            }
            else
            {
                creatorModelImporter.lightIntensity = 4.0f;
                creatorModelImporter.lightAzimuth = -35.0f;
                creatorModelImporter.lightElevation = 35.0f;
                creatorModelImporter.ambientBrightness = 0.35f;
            }
            creatorImportLightIntensity.SetValue(creatorModelImporter.lightIntensity);
            creatorImportLightAzimuth.SetValue(creatorModelImporter.lightAzimuth);
            creatorImportLightElevation.SetValue(creatorModelImporter.lightElevation);
            creatorImportAmbientBrightness.SetValue(creatorModelImporter.ambientBrightness);
            ApplyCreatorImportPreviewLighting();
        };
        creatorImportLightingPreset.OnSelect([applyLightingPreset](const wi::gui::EventArgs& args)
        {
            applyLightingPreset(args.iValue);
        });
        creatorImportLightingReset.OnClick([applyLightingPreset](const wi::gui::EventArgs&)
        {
            creatorImportLightingPreset.SetSelectedWithoutCallback(0);
            applyLightingPreset(0);
        });

        creatorImportAnimationLabel.Create("ANIMATIONS // EDITABLE CLIPS");
        creatorImportAnimationCombo.Create("Animation Action");
        creatorImportAnimationCombo.OnSelect([](const wi::gui::EventArgs& args)
        {
            StopCreatorImportPreviewAnimations();
            creatorModelImporter.selectedAnimation =
                static_cast<std::size_t>(args.userdata);
            RefreshCreatorImportAnimationEditor();
        });
        creatorImportAnimationName.Create("Animation Clip Name");
        creatorImportAnimationName.SetPlaceholder("CLIP NAME");
        creatorImportAnimationName.OnInputAccepted([](const wi::gui::EventArgs& args)
        {
            if (creatorModelImporter.animationRecipe.empty()) return;
            creatorModelImporter.animationRecipe[creatorModelImporter.selectedAnimation].name = args.sValue;
            RebuildCreatorImportAnimationCombo();
        });
        creatorImportAnimationStart.Create("Animation Start");
        creatorImportAnimationStart.SetDescription("START: ");
        creatorImportAnimationStart.OnInputAccepted([](const wi::gui::EventArgs& args)
        {
            if (creatorModelImporter.animationRecipe.empty()) return;
            auto& clip = creatorModelImporter.animationRecipe[creatorModelImporter.selectedAnimation];
            clip.start = std::min(args.fValue, clip.end);
            RefreshCreatorImportAnimationEditor();
        });
        creatorImportAnimationEnd.Create("Animation End");
        creatorImportAnimationEnd.SetDescription("END: ");
        creatorImportAnimationEnd.OnInputAccepted([](const wi::gui::EventArgs& args)
        {
            if (creatorModelImporter.animationRecipe.empty()) return;
            auto& clip = creatorModelImporter.animationRecipe[creatorModelImporter.selectedAnimation];
            clip.end = std::max(args.fValue, clip.start);
            RefreshCreatorImportAnimationEditor();
        });
        creatorImportAnimationEnabled.Create("Animation Included");
        creatorImportAnimationEnabled.AddItem("INCLUDE");
        creatorImportAnimationEnabled.AddItem("EXCLUDE");
        creatorImportAnimationEnabled.OnSelect([](const wi::gui::EventArgs& args)
        {
            if (creatorModelImporter.animationRecipe.empty()) return;
            creatorModelImporter.animationRecipe[creatorModelImporter.selectedAnimation].enabled = args.iValue == 0;
            RefreshCreatorImportAnimationEditor();
        });
        creatorImportAnimationAdd.Create("Add Animation Clip");
        creatorImportAnimationAdd.SetText("ADD CLIP");
        creatorImportAnimationAdd.OnClick([](const wi::gui::EventArgs&)
        {
            if (creatorModelImporter.animationRecipe.empty()) return;
            auto clip = creatorModelImporter.animationRecipe[creatorModelImporter.selectedAnimation];
            clip.name = clip.name.empty()
                ? "Clip " + std::to_string(creatorModelImporter.animationRecipe.size() + 1)
                : clip.name + " Copy";
            creatorModelImporter.animationRecipe.push_back(std::move(clip));
            creatorModelImporter.selectedAnimation = creatorModelImporter.animationRecipe.size() - 1;
            RebuildCreatorImportAnimationCombo();
        });
        creatorImportAnimationDelete.Create("Delete Animation Clip");
        creatorImportAnimationDelete.SetText("DELETE CLIP");
        creatorImportAnimationDelete.OnClick([](const wi::gui::EventArgs&)
        {
            if (creatorModelImporter.animationRecipe.empty()) return;
            StopCreatorImportPreviewAnimations();
            creatorModelImporter.animationRecipe.erase(
                creatorModelImporter.animationRecipe.begin() +
                static_cast<std::ptrdiff_t>(creatorModelImporter.selectedAnimation));
            if (creatorModelImporter.selectedAnimation > 0)
                --creatorModelImporter.selectedAnimation;
            RebuildCreatorImportAnimationCombo();
        });
        creatorImportAnimationPlay.Create("Play Native Import Animation");
     …40661 tokens truncated…nst auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
            return;
        XMFLOAT2 center = {};
        if (!ProjectEditorPoint(transform->GetPosition(), center))
            return;
        const XMFLOAT4 color = probe
            ? XMFLOAT4(0.20f, 0.92f, 1.0f, 0.95f)
            : XMFLOAT4(1.0f, 0.48f, 0.10f, 0.95f);
        constexpr float radius = 8.0f;
        if (probe)
        {
            DrawEditorLine(
                XMFLOAT2(center.x - radius, center.y - radius),
                XMFLOAT2(center.x + radius, center.y - radius), color);
            DrawEditorLine(
                XMFLOAT2(center.x + radius, center.y - radius),
                XMFLOAT2(center.x + radius, center.y + radius), color);
            DrawEditorLine(
                XMFLOAT2(center.x + radius, center.y + radius),
                XMFLOAT2(center.x - radius, center.y + radius), color);
            DrawEditorLine(
                XMFLOAT2(center.x - radius, center.y + radius),
                XMFLOAT2(center.x - radius, center.y - radius), color);
            DrawEditorLine(
                XMFLOAT2(center.x - radius, center.y),
                XMFLOAT2(center.x + radius, center.y), color);
            DrawEditorLine(
                XMFLOAT2(center.x, center.y - radius),
                XMFLOAT2(center.x, center.y + radius), color);
        }
        else
        {
            DrawEditorLine(
                XMFLOAT2(center.x, center.y - radius - 3.0f),
                XMFLOAT2(center.x + radius + 3.0f, center.y), color);
            DrawEditorLine(
                XMFLOAT2(center.x + radius + 3.0f, center.y),
                XMFLOAT2(center.x, center.y + radius + 3.0f), color);
            DrawEditorLine(
                XMFLOAT2(center.x, center.y + radius + 3.0f),
                XMFLOAT2(center.x - radius - 3.0f, center.y), color);
            DrawEditorLine(
                XMFLOAT2(center.x - radius - 3.0f, center.y),
                XMFLOAT2(center.x, center.y - radius - 3.0f), color);
        }
        if (selected == entity)
        {
            drawVolume(
                *transform,
                XMFLOAT4(color.x, color.y, color.z, 0.72f));
        }
        if (canSelect)
        {
            const float dx = pointer.x - center.x;
            const float dy = pointer.y - center.y;
            const float distanceSquared = dx * dx + dy * dy;
            if (distanceSquared < bestDistanceSquared)
            {
                bestDistanceSquared = distanceSquared;
                best = entity;
            }
        }
    };

    for (std::size_t index = 0; index < scene.decals.GetCount(); ++index)
        drawEntity(scene.decals.GetEntity(index), false);
    for (std::size_t index = 0; index < scene.probes.GetCount(); ++index)
        drawEntity(scene.probes.GetEntity(index), true);

    if (canSelect && best != wi::ecs::INVALID_ENTITY)
    {
        session_->Selection().Select(best);
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        return true;
    }
    return false;
}

bool StudioRenderPath::HandlePlayerStartSceneIcon(
    const XMFLOAT4& pointer)
{
    if (session_ == nullptr || camera == nullptr || projectHubVisible_ ||
        creatorModelImporter.thumbnailCapturePending)
    {
        return false;
    }

    auto& scene = session_->Scenes().GetScene();
    const auto resolved = bridge::ResolvePlayerStart(scene);
    if (resolved.resolution != bridge::PlayerStartResolution::Success ||
        !session_->Scenes().IsHierarchyVisible(resolved.start.entity))
    {
        return false;
    }
    const auto* transform = scene.transforms.GetComponent(resolved.start.entity);
    if (transform == nullptr)
        return false;

    const XMFLOAT3 feet = transform->GetPosition();
    const XMFLOAT3 euler = wi::math::QuaternionToRollPitchYaw(
        resolved.start.transform.rotation);
    const XMVECTOR forwardVector = XMVectorSet(
        std::sin(euler.y), 0.0f, std::cos(euler.y), 0.0f);
    const XMVECTOR rightVector = XMVectorSet(
        std::cos(euler.y), 0.0f, -std::sin(euler.y), 0.0f);
    const XMVECTOR origin = XMLoadFloat3(&feet) + XMVectorSet(0, 0.035f, 0, 0);
    const auto worldPoint = [&](const float forward, const float right,
        const float up = 0.0f)
    {
        XMFLOAT3 point;
        XMStoreFloat3(&point,
            origin + forwardVector * forward + rightVector * right +
                XMVectorSet(0, up, 0, 0));
        return point;
    };

    // Ground-plane silhouette follows the supplied arrow asset proportions:
    // 2.4 m long, 0.72 m wide, with its tip aligned to Runtime +Z forward.
    constexpr XMFLOAT2 Arrow[7] = {
        XMFLOAT2(1.20f, 0.0f),
        XMFLOAT2(0.28f, 0.36f),
        XMFLOAT2(0.28f, 0.15f),
        XMFLOAT2(-1.20f, 0.15f),
        XMFLOAT2(-1.20f, -0.15f),
        XMFLOAT2(0.28f, -0.15f),
        XMFLOAT2(0.28f, -0.36f),
    };
    XMFLOAT2 projected[7] = {};
    bool visible[7] = {};
    for (int index = 0; index < 7; ++index)
        visible[index] = ProjectEditorPoint(
            worldPoint(Arrow[index].x, Arrow[index].y), projected[index]);

    XMFLOAT2 center = {};
    if (!ProjectEditorPoint(feet, center))
        return false;
    const float dx = pointer.x - center.x;
    const float dy = pointer.y - center.y;
    const bool hovered = dx * dx + dy * dy <= 28.0f * 28.0f;
    const bool selected = session_->Selection().SelectedEntity() ==
        resolved.start.entity;
    const XMFLOAT4 color = selected
        ? XMFLOAT4(1.0f, 0.55f, 0.15f, 1.0f)
        : hovered
            ? XMFLOAT4(0.58f, 0.95f, 1.0f, 1.0f)
            : XMFLOAT4(0.20f, 0.84f, 1.0f, 0.95f);
    for (int index = 0; index < 7; ++index)
    {
        const int next = (index + 1) % 7;
        if (visible[index] && visible[next])
            DrawEditorLine(projected[index], projected[next], color);
    }

    // The arrow is always present. Selecting it adds the real configured
    // capsule as a wire guide without creating a renderable Runtime mesh.
    if (selected)
    {
        const auto settings = resolved.start.settings;
        const float radius = settings.capsuleRadius;
        const float totalHeight = bridge::PlayerCapsuleTotalHeight(settings);
        constexpr int Segments = 20;
        for (int ring = 0; ring < 2; ++ring)
        {
            const float height = ring == 0 ? radius : totalHeight - radius;
            for (int segment = 0; segment < Segments; ++segment)
            {
                const float a0 = XM_2PI * static_cast<float>(segment) / Segments;
                const float a1 = XM_2PI * static_cast<float>(segment + 1) / Segments;
                XMFLOAT2 p0 = {}, p1 = {};
                if (ProjectEditorPoint(
                        worldPoint(std::cos(a0) * radius,
                            std::sin(a0) * radius, height), p0) &&
                    ProjectEditorPoint(
                        worldPoint(std::cos(a1) * radius,
                            std::sin(a1) * radius, height), p1))
                {
                    DrawEditorLine(p0, p1, XMFLOAT4(color.x, color.y, color.z, 0.72f));
                }
            }
        }
        for (const float side : {-radius, radius})
        {
            XMFLOAT2 bottom = {}, top = {};
            if (ProjectEditorPoint(worldPoint(0, side, radius), bottom) &&
                ProjectEditorPoint(worldPoint(0, side, totalHeight - radius), top))
            {
                DrawEditorLine(bottom, top, XMFLOAT4(color.x, color.y, color.z, 0.72f));
            }
        }
    }

    const bool selectRequested = hovered && !flyCameraActive_ &&
        !GetGUI().HasFocus() && !gizmo_.IsInteracting() &&
        IsPointerOverViewport(pointer) &&
        wi::input::Press(wi::input::MOUSE_BUTTON_LEFT);
    if (selectRequested)
    {
        session_->Selection().Select(resolved.start.entity);
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        SyncGizmoSelection();
        SyncSelectionOutline();
        return true;
    }
    return false;
}

bool StudioRenderPath::HandleCameraSceneIcons(
    const XMFLOAT4& pointer)
{
    if (session_ == nullptr || camera == nullptr || projectHubVisible_ ||
        creatorModelImporter.thumbnailCapturePending)
    {
        return false;
    }

    auto& scene = session_->Scenes().GetScene();
    const bool selectRequested =
        !flyCameraActive_ && !GetGUI().HasFocus() &&
        !gizmo_.IsInteracting() &&
        IsPointerOverViewport(pointer) &&
        wi::input::Press(wi::input::MOUSE_BUTTON_LEFT);

    for (std::size_t index = 0; index < scene.cameras.GetCount(); ++index)
    {
        const wi::ecs::Entity entity = scene.cameras.GetEntity(index);
        if (!session_->Scenes().IsHierarchyVisible(entity))
            continue;

        const auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
            continue;

        wi::scene::CameraComponent authoredCamera = scene.cameras[index];
        authoredCamera.TransformCamera(*transform);
        authoredCamera.UpdateCamera();

        XMFLOAT3 eye = {};
        XMFLOAT3 ahead = {};
        const XMVECTOR eyeVector = authoredCamera.GetEye();
        const XMVECTOR aheadVector = XMVectorAdd(
            eyeVector,
            XMVectorScale(authoredCamera.GetAt(), 1.0f));
        XMStoreFloat3(&eye, eyeVector);
        XMStoreFloat3(&ahead, aheadVector);

        XMFLOAT2 center = {};
        if (!ProjectEditorPoint(eye, center))
            continue;

        XMFLOAT2 direction = XMFLOAT2(1.0f, 0.0f);
        XMFLOAT2 aheadScreen = {};
        if (ProjectEditorPoint(ahead, aheadScreen))
        {
            const float dx = aheadScreen.x - center.x;
            const float dy = aheadScreen.y - center.y;
            const float length = std::sqrt(dx * dx + dy * dy);
            if (length > 0.001f)
            {
                direction.x = dx / length;
                direction.y = dy / length;
            }
        }
        const XMFLOAT2 perpendicular(-direction.y, direction.x);

        const bool selected =
            session_->Selection().SelectedEntity() == entity;
        const float pointerDx = pointer.x - center.x;
        const float pointerDy = pointer.y - center.y;
        const bool hovered =
            pointerDx * pointerDx + pointerDy * pointerDy <= 18.0f * 18.0f;
        const XMFLOAT4 color = selected
            ? XMFLOAT4(0.58f, 0.95f, 1.0f, 1.0f)
            : hovered
                ? XMFLOAT4(1.0f, 0.68f, 0.30f, 1.0f)
                : XMFLOAT4(0.20f, 0.84f, 1.0f, 0.92f);

        const auto point = [&](const float forward, const float side)
        {
            return XMFLOAT2(
                center.x + direction.x * forward + perpendicular.x * side,
                center.y + direction.y * forward + perpendicular.y * side);
        };

        const XMFLOAT2 backLeft = point(-7.0f, 5.0f);
        const XMFLOAT2 backRight = point(-7.0f, -5.0f);
        const XMFLOAT2 frontLeft = point(6.0f, 5.0f);
        const XMFLOAT2 frontRight = point(6.0f, -5.0f);
        const XMFLOAT2 lensLeft = point(12.0f, 7.0f);
        const XMFLOAT2 lensRight = point(12.0f, -7.0f);
        const XMFLOAT2 facingEnd = point(27.0f, 0.0f);

        DrawEditorLine(backLeft, frontLeft, color);
        DrawEditorLine(frontLeft, frontRight, color);
        DrawEditorLine(frontRight, backRight, color);
        DrawEditorLine(backRight, backLeft, color);
        DrawEditorLine(frontLeft, lensLeft, color);
        DrawEditorLine(frontRight, lensRight, color);
        DrawEditorLine(lensLeft, lensRight, color);
        DrawEditorLine(point(12.0f, 0.0f), facingEnd, color);
        DrawEditorLine(
            facingEnd,
            XMFLOAT2(
                facingEnd.x - direction.x * 6.0f + perpendicular.x * 4.0f,
                facingEnd.y - direction.y * 6.0f + perpendicular.y * 4.0f),
            color);
        DrawEditorLine(
            facingEnd,
            XMFLOAT2(
                facingEnd.x - direction.x * 6.0f - perpendicular.x * 4.0f,
                facingEnd.y - direction.y * 6.0f - perpendicular.y * 4.0f),
            color);

        if (hovered && selectRequested)
        {
            session_->Selection().Select(entity);
            gizmoSuppressedForCameraView_ = false;
            RefreshHierarchy();
            RefreshInspector();
            RefreshStatus();
            SyncGizmoSelection();
            SyncSelectionOutline();
            return true;
        }
    }
    return false;
}

    bool StudioRenderPath::HandleLightSceneIcons(
        const XMFLOAT4& pointer)
    {
        if (session_ == nullptr || camera == nullptr || projectHubVisible_ ||
            creatorModelImporter.thumbnailCapturePending)
        {
            return false;
        }

        auto& scene = session_->Scenes().GetScene();
        const auto selected = session_->Selection().SelectedEntity();
        wi::ecs::Entity hit = wi::ecs::INVALID_ENTITY;
        float bestDistanceSquared = 22.0f * 22.0f;
        const bool canSelect = !lightPlacementActive_ &&
            !flyCameraActive_ && !GetGUI().HasFocus() &&
            !gizmo_.IsInteracting() &&
            IsPointerOverViewport(pointer) &&
            wi::input::Press(wi::input::MOUSE_BUTTON_LEFT);

        for (std::size_t index = 0; index < scene.lights.GetCount(); ++index)
        {
            const auto entity = scene.lights.GetEntity(index);
            const auto& light = scene.lights[index];
            if (!session_->Scenes().IsHierarchyVisible(entity))
            {
                continue;
            }
            const auto* transform = scene.transforms.GetComponent(entity);
            if (transform == nullptr)
            {
                continue;
            }

            const XMFLOAT3 position = transform->GetPosition();
            XMFLOAT2 center;
            if (!ProjectEditorPoint(position, center))
            {
                continue;
            }
            const float dx = pointer.x - center.x;
            const float dy = pointer.y - center.y;
            const float distanceSquared = dx * dx + dy * dy;
            const bool hovered = distanceSquared <= 22.0f * 22.0f;
            const XMFLOAT4 color = entity == selected
                ? XMFLOAT4(1.0f, 0.55f, 0.15f, 1.0f)
                : hovered
                    ? XMFLOAT4(0.58f, 0.95f, 1.0f, 1.0f)
                    : XMFLOAT4(0.20f, 0.84f, 1.0f, 0.92f);

            const auto drawCircle = [&](const float radius)
            {
                constexpr int Segments = 16;
                for (int segment = 0; segment < Segments; ++segment)
                {
                    const float angle0 = static_cast<float>(segment) /
                        static_cast<float>(Segments) * XM_2PI;
                    const float angle1 = static_cast<float>(segment + 1) /
                        static_cast<float>(Segments) * XM_2PI;
                    DrawEditorLine(
                        XMFLOAT2(center.x + std::cos(angle0) * radius,
                            center.y + std::sin(angle0) * radius),
                        XMFLOAT2(center.x + std::cos(angle1) * radius,
                            center.y + std::sin(angle1) * radius),
                        color);
                }
            };

            XMFLOAT3 directionTarget = position;
            directionTarget.x += light.direction.x;
            directionTarget.y += light.direction.y;
            directionTarget.z += light.direction.z;
            XMFLOAT2 projectedDirection;
            XMFLOAT2 arrow = XMFLOAT2(0.0f, 1.0f);
            if (ProjectEditorPoint(directionTarget, projectedDirection))
            {
                arrow = XMFLOAT2(
                    projectedDirection.x - center.x,
                    projectedDirection.y - center.y);
                const float length = std::sqrt(
                    arrow.x * arrow.x + arrow.y * arrow.y);
                if (length > 0.001f)
                {
                    arrow.x /= length;
                    arrow.y /= length;
                }
            }
            const XMFLOAT2 perpendicular = XMFLOAT2(-arrow.y, arrow.x);

            switch (light.GetType())
            {
            case wi::scene::LightComponent::POINT:
                drawCircle(7.0f);
                for (int rayIndex = 0; rayIndex < 4; ++rayIndex)
                {
                    const float angle =
                        static_cast<float>(rayIndex) * XM_PIDIV2;
                    DrawEditorLine(
                        XMFLOAT2(center.x + std::cos(angle) * 10.0f,
                            center.y + std::sin(angle) * 10.0f),
                        XMFLOAT2(center.x + std::cos(angle) * 15.0f,
                            center.y + std::sin(angle) * 15.0f),
                        color);
                }
                break;
            case wi::scene::LightComponent::SPOT:
            {
                const XMFLOAT2 tip = XMFLOAT2(
                    center.x + arrow.x * 15.0f,
                    center.y + arrow.y * 15.0f);
                const XMFLOAT2 baseLeft = XMFLOAT2(
                    center.x - arrow.x * 7.0f + perpendicular.x * 8.0f,
                    center.y - arrow.y * 7.0f + perpendicular.y * 8.0f);
                const XMFLOAT2 baseRight = XMFLOAT2(
                    center.x - arrow.x * 7.0f - perpendicular.x * 8.0f,
                    center.y - arrow.y * 7.0f - perpendicular.y * 8.0f);
                DrawEditorLine(baseLeft, baseRight, color);
                DrawEditorLine(baseLeft, tip, color);
                DrawEditorLine(baseRight, tip, color);
                DrawEditorLine(center, tip, color);
                break;
            }
            case wi::scene::LightComponent::RECTANGLE:
            {
                constexpr float HalfWidth = 10.0f;
                constexpr float HalfHeight = 7.0f;
                const XMFLOAT2 topLeft(
                    center.x - HalfWidth,
                    center.y - HalfHeight);
                const XMFLOAT2 topRight(
                    center.x + HalfWidth,
                    center.y - HalfHeight);
                const XMFLOAT2 bottomLeft(
                    center.x - HalfWidth,
                    center.y + HalfHeight);
                const XMFLOAT2 bottomRight(
                    center.x + HalfWidth,
                    center.y + HalfHeight);
                DrawEditorLine(topLeft, topRight, color);
                DrawEditorLine(topRight, bottomRight, color);
                DrawEditorLine(bottomRight, bottomLeft, color);
                DrawEditorLine(bottomLeft, topLeft, color);
                DrawEditorLine(topLeft, bottomRight, color);
                DrawEditorLine(topRight, bottomLeft, color);
                break;
            }
            case wi::scene::LightComponent::DIRECTIONAL:
            default:
                drawCircle(8.0f);
                for (int rayIndex = 0; rayIndex < 8; ++rayIndex)
                {
                    const float angle =
                        static_cast<float>(rayIndex) / 8.0f * XM_2PI;
                    DrawEditorLine(
                        XMFLOAT2(center.x + std::cos(angle) * 11.0f,
                            center.y + std::sin(angle) * 11.0f),
                        XMFLOAT2(center.x + std::cos(angle) * 16.0f,
                            center.y + std::sin(angle) * 16.0f),
                        color);
                }
                break;
            }

            if (light.GetType() != wi::scene::LightComponent::POINT)
            {
                const XMFLOAT2 arrowEnd = XMFLOAT2(
                    center.x + arrow.x * 22.0f,
                    center.y + arrow.y * 22.0f);
                DrawEditorLine(center, arrowEnd, color);
                DrawEditorLine(
                    arrowEnd,
                    XMFLOAT2(
                        arrowEnd.x - arrow.x * 6.0f + perpendicular.x * 4.0f,
                        arrowEnd.y - arrow.y * 6.0f + perpendicular.y * 4.0f),
                    color);
                DrawEditorLine(
                    arrowEnd,
                    XMFLOAT2(
                        arrowEnd.x - arrow.x * 6.0f - perpendicular.x * 4.0f,
                        arrowEnd.y - arrow.y * 6.0f - perpendicular.y * 4.0f),
                    color);
            }

            if (canSelect && hovered && distanceSquared < bestDistanceSquared)
            {
                bestDistanceSquared = distanceSquared;
                hit = entity;
            }
        }

        if (hit == wi::ecs::INVALID_ENTITY)
        {
            return false;
        }
        SetEnvironmentWorkspaceActive(false);
        SetTerrainWorkspaceActive(false);
        session_->Selection().Select(hit);
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        return true;
    }

    bool StudioRenderPath::IsPointerOverViewport(
        const XMFLOAT4& pointer) const noexcept
    {
        return pointer.x >= viewportBounds_.x &&
            pointer.x < viewportBounds_.z &&
            pointer.y >= viewportBounds_.y &&
            pointer.y < viewportBounds_.w;
    }

    void StudioRenderPath::HandleViewportNavigation(
        const float dt,
        const XMFLOAT4& pointer)
    {
        const bool pointerOverViewport = IsPointerOverViewport(pointer);
        if (!flyCameraActive_ &&
            pointerOverViewport &&
            !GetGUI().HasFocus() &&
            wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT))
        {
            gizmoSuppressedForCameraView_ = false;
            flyCameraActive_ = true;
            cameraPointerAnchor_ = pointer;
        }

        if (flyCameraActive_ &&
            !wi::input::Down(wi::input::MOUSE_BUTTON_RIGHT))
        {
            flyCameraActive_ = false;
            wi::input::HidePointer(false);
            return;
        }

        if (pointerOverViewport && !GetGUI().HasFocus())
        {
            if (pointer.z > 0.1f)
            {
                cameraMoveSpeed_ = std::min(
                    100.0f,
                    cameraMoveSpeed_ * 1.25f);
            }
            else if (pointer.z < -0.1f)
            {
                cameraMoveSpeed_ = std::max(
                    0.1f,
                    cameraMoveSpeed_ / 1.25f);
            }
        }

        if (!flyCameraActive_)
        {
            return;
        }

        const auto& mouse = wi::input::GetMouseState();
        constexpr float lookSensitivity = 0.0017f;
        const float yaw = mouse.delta_position.x * lookSensitivity;
        const float pitch = mouse.delta_position.y * lookSensitivity;

        XMVECTOR movement = XMVectorZero();
        const auto key = [](const char value)
        {
            return static_cast<wi::input::BUTTON>(value);
        };
        if (wi::input::Down(key('W')))
        {
            movement += XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        }
        if (wi::input::Down(key('S')))
        {
            movement += XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
        }
        if (wi::input::Down(key('A')))
        {
            movement += XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f);
        }
        if (wi::input::Down(key('D')))
        {
            movement += XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        }
        if (wi::input::Down(key('Q')))
        {
            movement += XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
        }
        if (wi::input::Down(key('E')))
        {
            movement += XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        }

        const float movementLength =
            XMVectorGetX(XMVector3LengthSq(movement));
        if (movementLength > 0.0f)
        {
            movement = XMVector3Normalize(movement);
            const float speedMultiplier =
                wi::input::Down(wi::input::KEYBOARD_BUTTON_LSHIFT)
                ? 4.0f
                : 1.0f;
            movement *=
                cameraMoveSpeed_ *
                speedMultiplier *
                std::min(dt, 0.1f);
            const XMMATRIX cameraRotation = XMMatrixRotationQuaternion(
                XMLoadFloat4(&editorCameraTransform_.rotation_local));
            editorCameraTransform_.Translate(
                XMVector3TransformNormal(movement, cameraRotation));
        }

        if (yaw != 0.0f || pitch != 0.0f)
        {
            editorCameraTransform_.RotateRollPitchYaw(
                XMFLOAT3(pitch, yaw, 0.0f));
        }

        if (movementLength > 0.0f || yaw != 0.0f || pitch != 0.0f)
        {
            editorCameraTransform_.UpdateTransform();
            camera->TransformCamera(editorCameraTransform_);
            camera->UpdateCamera();
        }

        wi::input::SetPointer(cameraPointerAnchor_);
        wi::input::HidePointer(true);
    }

    bool StudioRenderPath::HandleTerrainSculpt(const XMFLOAT4& pointer)
    {
        if (!terrainWorkspaceActive_ || session_ == nullptr || flyCameraActive_ ||
            GetGUI().HasFocus() || !IsPointerOverViewport(pointer) ||
            session_->Scenes().GetScene().terrains.GetCount() == 0)
        {
            return false;
        }
        auto& scene = session_->Scenes().GetScene();
        const auto terrainEntity = scene.terrains.GetEntity(0);
        auto* terrain = scene.terrains.GetComponent(terrainEntity);
        if (terrain == nullptr) return false;
        const auto ray = wi::renderer::GetPickRay(static_cast<long>(pointer.x),
            static_cast<long>(pointer.y), *this, *camera);
        const auto picked = wi::scene::Pick(ray, wi::enums::FILTER_TERRAIN, ~0u, scene);
        if (picked.entity != wi::ecs::INVALID_ENTITY)
        {
            wi::renderer::DrawSphere(wi::primitive::Sphere(picked.position,
                terrainBrushRadiusValue_), XMFLOAT4(0.20f, 0.92f, 1.0f, 0.22f), true);
        }
        if (wi::input::Press(wi::input::MOUSE_BUTTON_LEFT) &&
            picked.entity != wi::ecs::INVALID_ENTITY)
        {
            terrainStrokeActive_ = true;
            terrainStrokeChanged_ = false;
            terrainStrokeEntity_ = terrainEntity;
            terrainStrokeBefore_ = bridge::CaptureTerrainSculpt(scene, *terrain);
            terrainFlattenHeight_ = picked.position.y;
        }
        if (terrainStrokeActive_ && wi::input::Down(wi::input::MOUSE_BUTTON_LEFT) &&
            picked.entity != wi::ecs::INVALID_ENTITY)
        {
            terrainStrokeChanged_ |= bridge::SculptTerrain(scene, *terrain,
                picked.position, terrainBrushRadiusValue_,
                terrainBrushStrengthValue_ * 0.12f, terrainBrushFalloffValue_,
                terrainSculptModeValue_, terrainFlattenHeight_);
            return true;
        }
        if (terrainStrokeActive_ && wi::input::Release(wi::input::MOUSE_BUTTON_LEFT))
        {
            terrainStrokeActive_ = false;
            if (terrainStrokeChanged_)
            {
                const auto finishStarted =
                    std::chrono::steady_clock::now();
                auto after = bridge::CaptureTerrainSculpt(scene, *terrain);
                const std::size_t affectedChunks =
                    bridge::RetainChangedTerrainSculpt(
                        terrainStrokeBefore_,
                        after);
                if (affectedChunks > 0)
                {
                    bridge::RefreshTerrainSculptPhysics(
                        scene,
                        *terrain,
                        after);
                    session_->Commands().RecordExecuted(
                        std::make_unique<bridge::SculptTerrainCommand>(
                            scene,
                            terrainStrokeEntity_,
                            std::move(terrainStrokeBefore_),
                            std::move(after)));
                }
                const auto elapsed = std::chrono::duration_cast<
                    std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - finishStarted);
                terrainStrokeDiagnostic_.SetText(
                    "LAST STROKE // " +
                    std::to_string(affectedChunks) +
                    " TILES // " +
                    std::to_string(elapsed.count()) +
                    " MS");
                RefreshStatus();
            }
            return true;
        }
        return false;
    }

    bool StudioRenderPath::HandleViewportSelection(
        const XMFLOAT4& pointer)
    {
        if (session_ == nullptr ||
            flyCameraActive_ ||
            GetGUI().HasFocus() ||
            !IsPointerOverViewport(pointer) ||
            !wi::input::Press(wi::input::MOUSE_BUTTON_LEFT) ||
            gizmo_.IsInteracting())
        {
            return false;
        }

        const auto pickRay = wi::renderer::GetPickRay(
            static_cast<long>(pointer.x),
            static_cast<long>(pointer.y),
            *this,
            *camera);
        auto& scene = session_->Scenes().GetScene();
        const auto picked = wi::scene::Pick(
            pickRay,
            wi::enums::FILTER_OBJECT_ALL,
            ~0u,
            scene);
        wi::ecs::Entity selection = picked.entity;
        if (selection != wi::ecs::INVALID_ENTITY)
        {
            const wi::ecs::Entity reusableRoot =
                ResolveReusableSelectionRoot(scene, selection);
            if (reusableRoot != wi::ecs::INVALID_ENTITY)
                selection = reusableRoot;
        }
        const auto current = session_->Selection().SelectedEntity();
        if (selection == current && !environmentWorkspaceActive_)
        {
            return false;
        }

        environmentWorkspaceActive_ = false;
        studioChrome_.SetEnvironmentWorkspaceActive(false);
        terrainWorkspaceActive_ = false;
        studioChrome_.SetTerrainWorkspaceActive(false);

        if (selection == wi::ecs::INVALID_ENTITY ||
            !session_->Scenes().IsHierarchyVisible(selection))
        {
            session_->Selection().Clear();
        }
        else
        {
            session_->Selection().Select(selection);
        }

        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        return true;
    }

    void StudioRenderPath::RefreshProjectHub()
    {
        if (!session_) return;
        const auto& projects = session_->Projects().RecentProjects();
        if (projects.empty()) selectedRecentProject_ = -1;
        else if (selectedRecentProject_ < 0 || static_cast<std::size_t>(selectedRecentProject_) >= projects.size())
            selectedRecentProject_ = 0;

        for (auto& button : recentProjectButtons_) button.SetVisible(false);
        launchProjectButton_.SetEnabled(selectedRecentProject_ >= 0);
        continueProjectButton_.SetVisible(false);

        std::vector<RenegadeProjectHub::ProjectEntry> entries;
        entries.reserve(projects.size());
        for (const auto& recent : projects)
        {
            RenegadeProjectHub::ProjectEntry entry;
            entry.name = recent.name;
            entry.descriptorPath = recent.descriptorPath;
            bridge::ProjectMetadata meta;
            std::string error;
            entry.descriptorValid = session_->Projects().InspectProject(recent.descriptorPath, meta, error);
            if (entry.descriptorValid)
            {
                if (!meta.name.empty()) entry.name = meta.name;
                entry.rootPath = meta.rootPath;
                entry.startupScene = meta.startupScene;
                entry.startupFlow = meta.startupFlow;
                entry.formatVersion = meta.formatVersion;
            }
            entries.push_back(std::move(entry));
        }
        projectHubChrome_.SetProjects(std::move(entries), selectedRecentProject_);
        if (session_->Projects().HasProject())
            projectHubChrome_.SetCurrentProject(session_->Projects().CurrentProject().name, true);
        else
            projectHubChrome_.SetCurrentProject({}, false);
    }

    void StudioRenderPath::ApplySelectedTransformValue(
        const TransformTool tool,
        const int axis,
        const float value)
    {
        if (environmentWorkspaceActive_ || session_ == nullptr ||
            !session_->Selection().HasSelection())
        {
            return;
        }

        const auto entity = session_->Selection().SelectedEntity();
        auto* transform =
            session_->Scenes().GetScene().transforms.GetComponent(entity);
        if (transform == nullptr)
        {
            return;
        }

        auto next = bridge::CaptureTransform(*transform);
        if (tool == TransformTool::Translate)
        {
            if (axis == 0)
            {
                next.translation.x = value;
            }
            else if (axis == 1)
            {
                next.translation.y = value;
            }
            else
            {
                next.translation.z = value;
            }
        }
        else if (tool == TransformTool::Rotate)
        {
            auto rotation =
                wi::math::QuaternionToRollPitchYaw(transform->rotation_local);
            const float radians = value / 180.0f * XM_PI;
            if (axis == 0)
            {
                rotation.x = radians;
            }
            else if (axis == 1)
            {
                rotation.y = radians;
            }
            else
            {
                rotation.z = radians;
            }
            XMStoreFloat4(
                &next.rotation,
                XMQuaternionNormalize(
                    XMQuaternionRotationRollPitchYaw(
                        rotation.x,
                        rotation.y,
                        rotation.z)));
        }
        else
        {
            if (axis == 0)
            {
                next.scale.x = value;
            }
            else if (axis == 1)
            {
                next.scale.y = value;
            }
            else
            {
                next.scale.z = value;
            }
        }

        session_->Commands().Execute(
            std::make_unique<bridge::SetTransformCommand>(
                session_->Scenes().GetScene(),
                entity,
                next));
        SetTransformTool(tool);
        RefreshInspector();
        RefreshStatus();
    }

    bool StudioRenderPath::CommitSelectedWeather(
        const bridge::WeatherState& weather)
    {
        if (session_ == nullptr)
        {
            return false;
        }

        const auto entity = EditableWeatherEntity();
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return false;
        }
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetWeatherCommand>(
                session_->Scenes().GetScene(),
                entity,
                weather));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::SetWeatherFieldValue(
        bridge::WeatherState& weather,
        const WeatherField field,
        const float value) noexcept
    {
        switch (field)
        {
        case WeatherField::SkyExposure:
            weather.skyExposure = std::clamp(value, 0.0f, 8.0f);
            break;
        case WeatherField::Stars:
            weather.stars = std::clamp(value, 0.0f, 1.0f);
            break;
        case WeatherField::AmbientIntensity:
            weather.ambientIntensity = std::clamp(value, 0.0f, 8.0f);
            break;
        case WeatherField::FogStart:
            weather.fogStart = std::clamp(value, 0.0f, 100000.0f);
            break;
        case WeatherField::FogDensity:
            weather.fogDensity = std::clamp(value, 0.0f, 1.0f);
            break;
        case WeatherField::FogHeightStart:
            weather.fogHeightStart =
                std::clamp(value, -100000.0f, 100000.0f);
            break;
        case WeatherField::FogHeightEnd:
            weather.fogHeightEnd =
                std::clamp(value, -100000.0f, 100000.0f);
            break;
        case WeatherField::CloudCoverage:
            weather.cloudCoverage = std::clamp(value, 0.0f, 1.0f);
            break;
        case WeatherField::CloudStartHeight:
            weather.cloudStartHeight =
                std::clamp(value, 0.0f, 50000.0f);
            break;
        case WeatherField::CloudThickness:
            weather.cloudThickness =
                std::clamp(value, 1.0f, 50000.0f);
            break;
        }
    }

    void StudioRenderPath::BeginWeatherSlider(const WeatherField field)
    {
        weatherSliderActive_ = false;
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        const auto* component =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (component == nullptr)
        {
            return;
        }
        weatherSliderActive_ = true;
        weatherSliderField_ = field;
        weatherSliderEntity_ = entity;
        weatherSliderBefore_ = bridge::CaptureWeather(*component);
        weatherSliderAfter_ = weatherSliderBefore_;
    }

    void StudioRenderPath::PreviewWeatherSlider(
        const WeatherField field,
        const float value)
    {
        if (!weatherSliderActive_ || weatherSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        auto* component = scene.weathers.GetComponent(weatherSliderEntity_);
        if (component == nullptr)
        {
            weatherSliderActive_ = false;
            return;
        }
        weatherSliderAfter_ = weatherSliderBefore_;
        SetWeatherFieldValue(weatherSliderAfter_, field, value);
        bridge::ApplyWeather(*component, weatherSliderAfter_);
        if (scene.weathers.GetCount() > 0 &&
            scene.weathers.GetEntity(0) == weatherSliderEntity_)
        {
            scene.weather = *component;
        }
    }

    void StudioRenderPath::CommitWeatherSlider(
        const WeatherField field,
        const float value)
    {
        if (!weatherSliderActive_ || weatherSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        auto* component = scene.weathers.GetComponent(weatherSliderEntity_);
        if (component == nullptr)
        {
            weatherSliderActive_ = false;
            return;
        }

        SetWeatherFieldValue(weatherSliderAfter_, field, value);
        bridge::ApplyWeather(*component, weatherSliderBefore_);
        if (scene.weathers.GetCount() > 0 &&
            scene.weathers.GetEntity(0) == weatherSliderEntity_)
        {
            scene.weather = *component;
        }
        session_->Commands().Execute(
            std::make_unique<bridge::SetWeatherCommand>(
                scene,
                weatherSliderEntity_,
                weatherSliderBefore_,
                weatherSliderAfter_));
        weatherSliderActive_ = false;
        weatherSliderEntity_ = wi::ecs::INVALID_ENTITY;
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ApplySelectedWeatherToggle(
        const WeatherToggle toggle,
        const bool value)
    {
        if (session_ == nullptr)
        {
            return;
        }

        const auto entity = EditableWeatherEntity();
        const auto* component =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (component == nullptr)
        {
            return;
        }

        auto next = bridge::CaptureWeather(*component);
        switch (toggle)
        {
        case WeatherToggle::AerialPerspective:
            next.aerialPerspective = value;
            break;
        case WeatherToggle::HeightFog:
            next.heightFog = value;
            break;
        case WeatherToggle::CloudsCastShadow:
            next.cloudsCastShadow = value;
            break;
        }
        CommitSelectedWeather(next);
    }

    void StudioRenderPath::ApplySelectedSkyMode(
        const bridge::WeatherState::SkyMode mode)
    {
        if (session_ == nullptr)
        {
            return;
        }

        const auto entity = EditableWeatherEntity();
        const auto* component =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (component == nullptr)
        {
            return;
        }

        auto next = bridge::CaptureWeather(*component);
        next.skyMode = mode;
        CommitSelectedWeather(next);
    }

    void StudioRenderPath::ApplyWeatherPreset(const int preset)
    {
        if (session_ == nullptr)
        {
            return;
        }

        const auto entity = EditableWeatherEntity();
        const auto* component =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (component == nullptr)
        {
            return;
        }

        const auto current = bridge::CaptureWeather(*component);
        bridge::WeatherPreset selectedPreset;
        switch (preset)
        {
        case 1:
            selectedPreset = bridge::WeatherPreset::Clear;
            break;
        case 2:
            selectedPreset = bridge::WeatherPreset::Scattered;
            break;
        case 3:
            selectedPreset = bridge::WeatherPreset::Overcast;
            break;
        case 4:
            selectedPreset = bridge::WeatherPreset::Storm;
            break;
        default:
            return;
        }
        CommitSelectedWeather(
            bridge::MakeWeatherPreset(current, selectedPreset));
    }

    bool StudioRenderPath::CommitPrecipitation(
        const bridge::PrecipitationState& precipitation)
    {
        if (session_ == nullptr)
        {
            return false;
        }
        const auto entity = EditableWeatherEntity();
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return false;
        }
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetPrecipitationCommand>(
                session_->Scenes().GetScene(),
                entity,
                precipitation));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::ApplyPrecipitationMode(
        const bridge::PrecipitationMode mode)
    {
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        const auto* component =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (component == nullptr)
        {
            return;
        }
        CommitPrecipitation(bridge::MakePrecipitationProfile(
            bridge::CapturePrecipitation(*component),
            mode));
    }

    void StudioRenderPath::SetPrecipitationFieldValue(
        bridge::PrecipitationState& precipitation,
        const PrecipitationField field,
        const float value) noexcept
    {
        switch (field)
        {
        case PrecipitationField::Intensity:
            precipitation.intensity = std::clamp(value, 0.0f, 1.0f);
            break;
        case PrecipitationField::FallSpeed:
            precipitation.fallSpeed = std::clamp(value, 0.01f, 2.0f);
            break;
        case PrecipitationField::ParticleScale:
            precipitation.particleScale =
                std::clamp(value, 0.005f, 0.1f);
            break;
        case PrecipitationField::WindAzimuth:
            precipitation.windAzimuthDegrees =
                std::clamp(value, -180.0f, 180.0f);
            break;
        case PrecipitationField::WindSpeed:
            precipitation.windSpeed = std::clamp(value, 0.0f, 50.0f);
            break;
        case PrecipitationField::Turbulence:
            precipitation.turbulence = std::clamp(value, 0.0f, 20.0f);
            break;
        }
    }

    void StudioRenderPath::BeginPrecipitationSlider(
        const PrecipitationField field)
    {
        precipitationSliderActive_ = false;
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        const auto* component =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (component == nullptr)
        {
            return;
        }
        precipitationSliderActive_ = true;
        precipitationSliderField_ = field;
        precipitationSliderEntity_ = entity;
        precipitationSliderBefore_ =
            bridge::CapturePrecipitation(*component);
        precipitationSliderAfter_ = precipitationSliderBefore_;
    }

    void StudioRenderPath::PreviewPrecipitationSlider(
        const PrecipitationField field,
        const float value)
    {
        if (!precipitationSliderActive_ ||
            precipitationSliderField_ != field || session_ == nullptr)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        auto* component =
            scene.weathers.GetComponent(precipitationSliderEntity_);
        if (component == nullptr)
        {
            precipitationSliderActive_ = false;
            return;
        }
        precipitationSliderAfter_ = precipitationSliderBefore_;
        SetPrecipitationFieldValue(precipitationSliderAfter_, field, value);
        bridge::ApplyPrecipitation(*component, precipitationSliderAfter_);
        if (scene.weathers.GetCount() > 0 &&
            scene.weathers.GetEntity(0) == precipitationSliderEntity_)
        {
            scene.weather = *component;
        }
    }

    void StudioRenderPath::CommitPrecipitationSlider(
        const PrecipitationField field,
        const float value)
    {
        if (!precipitationSliderActive_ ||
            precipitationSliderField_ != field || session_ == nullptr)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        auto* component =
            scene.weathers.GetComponent(precipitationSliderEntity_);
        if (component == nullptr)
        {
            precipitationSliderActive_ = false;
            return;
        }
        SetPrecipitationFieldValue(precipitationSliderAfter_, field, value);
        bridge::ApplyPrecipitation(*component, precipitationSliderBefore_);
        if (scene.weathers.GetCount() > 0 &&
            scene.weathers.GetEntity(0) == precipitationSliderEntity_)
        {
            scene.weather = *component;
        }
        session_->Commands().Execute(
            std::make_unique<bridge::SetPrecipitationCommand>(
                scene,
                precipitationSliderEntity_,
                precipitationSliderBefore_,
                precipitationSliderAfter_));
        precipitationSliderActive_ = false;
        precipitationSliderEntity_ = wi::ecs::INVALID_ENTITY;
        RefreshInspector();
        RefreshStatus();
    }

    bool StudioRenderPath::CommitSun(const bridge::SunState& sun)
    {
        if (session_ == nullptr)
        {
            return false;
        }
        const auto entity = EditableWeatherEntity();
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return false;
        }
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetSunCommand>(
                session_->Scenes().GetScene(),
                entity,
                sun));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::ApplySunPreset(const bridge::SunPreset preset)
    {
        StopSunPreview(true);
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        const auto current = bridge::CaptureSun(
            session_->Scenes().GetScene(),
            entity);
        CommitSun(bridge::MakeSunPreset(current, preset));
    }

    void StudioRenderPath::SetSunFieldValue(
        bridge::SunState& sun,
        const SunField field,
        const float value) noexcept
    {
        switch (field)
        {
        case SunField::Time:
            bridge::SetSunTime(sun, value);
            break;
        case SunField::Azimuth:
            bridge::SetSunAzimuth(sun, value);
            break;
        case SunField::Elevation:
            bridge::SetSunElevation(sun, value);
            break;
        }
    }

    void StudioRenderPath::BeginSunSlider(const SunField field)
    {
        StopSunPreview(true);
        sunSliderActive_ = false;
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return;
        }
        sunSliderActive_ = true;
        sunSliderField_ = field;
        sunSliderBefore_ = bridge::CaptureSun(
            session_->Scenes().GetScene(),
            entity);
        sunSliderAfter_ = sunSliderBefore_;
    }

    void StudioRenderPath::PreviewSunSlider(
        const SunField field,
        const float value)
    {
        if (!sunSliderActive_ || sunSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        sunSliderAfter_ = sunSliderBefore_;
        SetSunFieldValue(sunSliderAfter_, field, value);
        bridge::ApplySun(
            session_->Scenes().GetScene(),
            EditableWeatherEntity(),
            sunSliderAfter_);
    }

    void StudioRenderPath::CommitSunSlider(
        const SunField field,
        const float value)
    {
        if (!sunSliderActive_ || sunSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        SetSunFieldValue(sunSliderAfter_, field, value);
        auto& scene = session_->Scenes().GetScene();
        const auto entity = EditableWeatherEntity();
        bridge::ApplySun(scene, entity, sunSliderBefore_);
        session_->Commands().Execute(
            std::make_unique<bridge::SetSunCommand>(
                scene,
                entity,
                sunSliderBefore_,
                sunSliderAfter_));
        sunSliderActive_ = false;
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::StartSunPreview()
    {
        if (sunPreviewPlaying_ || session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return;
        }
        sunPreviewBefore_ = bridge::CaptureSun(
            session_->Scenes().GetScene(),
            entity);
        sunPreviewCurrent_ = sunPreviewBefore_;
        sunPreviewPlaying_ = true;
        sunPlayButton_.SetEnabled(false);
        sunPauseButton_.SetEnabled(true);
    }

    void StudioRenderPath::StopSunPreview(const bool commit)
    {
        if (!sunPreviewPlaying_ || session_ == nullptr)
        {
            return;
        }
        sunPreviewPlaying_ = false;
        auto& scene = session_->Scenes().GetScene();
        const auto entity = EditableWeatherEntity();
        bridge::ApplySun(scene, entity, sunPreviewBefore_);
        if (commit)
        {
            session_->Commands().Execute(
                std::make_unique<bridge::SetSunCommand>(
                    scene,
                    entity,
                    sunPreviewBefore_,
                    sunPreviewCurrent_));
        }
        sunPlayButton_.SetEnabled(true);
        sunPauseButton_.SetEnabled(false);
        RefreshInspector();
        RefreshStatus();
    }

    bool StudioRenderPath::CommitOcean(const bridge::OceanState& ocean)
    {
        if (session_ == nullptr)
        {
            return false;
        }
        const auto entity = EditableWeatherEntity();
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return false;
        }
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetOceanCommand>(
                session_->Scenes().GetScene(),
                entity,
                ocean));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::ApplyOceanEnabled(const bool enabled)
    {
        StopSunPreview(true);
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        const auto* weather =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (weather == nullptr)
        {
            return;
        }
        auto ocean = bridge::CaptureOcean(*weather);
        ocean.enabled = enabled;
        CommitOcean(ocean);
    }

    void StudioRenderPath::ApplyOceanResolution(const int dimension)
    {
        StopSunPreview(true);
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        const auto* weather =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (weather == nullptr)
        {
            return;
        }
        auto ocean = bridge::CaptureOcean(*weather);
        ocean.displacementMapDimension = dimension;
        CommitOcean(ocean);
    }

    void StudioRenderPath::ApplyOceanPreset(const bridge::OceanPreset preset)
    {
        StopSunPreview(true);
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        const auto* weather =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (weather == nullptr)
        {
            return;
        }
        CommitOcean(bridge::MakeOceanPreset(
            bridge::CaptureOcean(*weather),
            preset));
    }

    void StudioRenderPath::SetOceanFieldValue(
        bridge::OceanState& ocean,
        const OceanField field,
        const float value) noexcept
    {
        switch (field)
        {
        case OceanField::PatchLength:
            ocean.patchLength = std::clamp(value, 1.0f, 2000.0f);
            break;
        case OceanField::TimeScale:
            ocean.timeScale = std::clamp(value, 0.0f, 4.0f);
            break;
        case OceanField::WaveAmplitude:
            ocean.waveAmplitude = std::clamp(value, 0.0f, 2000.0f);
            break;
        case OceanField::WindAzimuth:
            ocean.windAzimuthDegrees =
                std::clamp(value, -180.0f, 180.0f);
            break;
        case OceanField::WindSpeed:
            ocean.windSpeed = std::clamp(value, 0.0f, 2000.0f);
            break;
        case OceanField::WindDependency:
            ocean.windDependency = std::clamp(value, 0.0f, 1.0f);
            break;
        case OceanField::ChoppyScale:
            ocean.choppyScale = std::clamp(value, 0.0f, 10.0f);
            break;
        case OceanField::WaterRed:
            ocean.waterColor.x = std::clamp(value, 0.0f, 4.0f);
            break;
        case OceanField::WaterGreen:
            ocean.waterColor.y = std::clamp(value, 0.0f, 4.0f);
            break;
        case OceanField::WaterBlue:
            ocean.waterColor.z = std::clamp(value, 0.0f, 4.0f);
            break;
        case OceanField::WaterOpacity:
            ocean.waterColor.w = std::clamp(value, 0.0f, 1.0f);
            break;
        case OceanField::ExtinctionRed:
            ocean.extinctionColor.x = std::clamp(value, 0.0f, 4.0f);
            break;
        case OceanField::ExtinctionGreen:
            ocean.extinctionColor.y = std::clamp(value, 0.0f, 4.0f);
            break;
        case OceanField::ExtinctionBlue:
            ocean.extinctionColor.z = std::clamp(value, 0.0f, 4.0f);
            break;
        case OceanField::WaterHeight:
            ocean.waterHeight = std::clamp(value, -1000.0f, 1000.0f);
            break;
        case OceanField::SurfaceDetail:
            ocean.surfaceDetail = static_cast<std::uint32_t>(
                std::clamp(std::lround(value), 1l, 10l));
            break;
        case OceanField::DisplacementTolerance:
            ocean.surfaceDisplacementTolerance =
                std::clamp(value, 1.0f, 10.0f);
            break;
        }
    }

    void StudioRenderPath::BeginOceanSlider(const OceanField field)
    {
        StopSunPreview(true);
        oceanSliderActive_ = false;
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        const auto* weather =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (weather == nullptr)
        {
            return;
        }
        oceanSliderActive_ = true;
        oceanSliderField_ = field;
        oceanSliderEntity_ = entity;
        oceanSliderBefore_ = bridge::CaptureOcean(*weather);
        oceanSliderAfter_ = oceanSliderBefore_;
    }

    void StudioRenderPath::PreviewOceanSlider(
        const OceanField field,
        const float value)
    {
        if (!oceanSliderActive_ || oceanSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        oceanSliderAfter_ = oceanSliderBefore_;
        SetOceanFieldValue(oceanSliderAfter_, field, value);
        bridge::ApplyOcean(
            session_->Scenes().GetScene(),
            oceanSliderEntity_,
            oceanSliderAfter_);
    }

    void StudioRenderPath::CommitOceanSlider(
        const OceanField field,
        const float value)
    {
        if (!oceanSliderActive_ || oceanSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        SetOceanFieldValue(oceanSliderAfter_, field, value);
        auto& scene = session_->Scenes().GetScene();
        bridge::ApplyOcean(scene, oceanSliderEntity_, oceanSliderBefore_);
        session_->Commands().Execute(
            std::make_unique<bridge::SetOceanCommand>(
                scene,
                oceanSliderEntity_,
                oceanSliderBefore_,
                oceanSliderAfter_));
        oceanSliderActive_ = false;
        oceanSliderEntity_ = wi::ecs::INVALID_ENTITY;
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::CommitSelectedSceneName(const std::string& name)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto selected = session_->Selection().SelectedEntity();
        const auto target = bridge::ResolveSceneComponentAuthoringRoot(scene, selected);
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetSceneNameCommand>(scene, selected, name));
        if (changed && target != wi::ecs::INVALID_ENTITY)
            session_->Selection().Select(target);
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ApplySelectedLayerBit(
        const std::uint32_t bit,
        const bool enabled)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto selected = session_->Selection().SelectedEntity();
        (void)session_->Commands().Execute(
            std::make_unique<bridge::SetSceneLayerBitCommand>(
                scene, selected, bit, enabled));
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ApplySelectedLayerMask(const std::uint32_t mask)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto selected = session_->Selection().SelectedEntity();
        (void)session_->Commands().Execute(
            std::make_unique<bridge::SetSceneLayerMaskCommand>(
                scene, selected, mask));
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ApplySelectedMetadataPreset(
        const wi::scene::MetadataComponent::Preset preset)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto selected = session_->Selection().SelectedEntity();
        (void)session_->Commands().Execute(
            std::make_unique<bridge::SetMetadataPresetCommand>(
                scene, selected, preset));
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ApplySelectedObjectParticipation(
        const bridge::ObjectParticipationProperty property,
        const bool value)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto selected = session_->Selection().SelectedEntity();
        (void)session_->Commands().Execute(
            std::make_unique<bridge::SetObjectParticipationCommand>(
                scene, selected, property, value));
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::CommitSelectedPlayerField(
        const PlayerField field,
        const float value)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto entity = session_->Selection().SelectedEntity();
        if (!bridge::IsPlayerStart(scene, entity))
            return;

        auto settings = bridge::CapturePlayerControllerSettings(scene, entity);
        switch (field)
        {
        case PlayerField::CapsuleRadius:
            settings.capsuleRadius = value;
            break;
        case PlayerField::CapsuleTotalHeight:
            settings.capsuleHeight = std::max(
                0.01f, (value - settings.capsuleRadius * 2.0f) * 0.5f);
            break;
        case PlayerField::EyeHeight:
            settings.eyeHeight = value;
            break;
        case PlayerField::WalkSpeed:
            settings.walkSpeed = value;
            break;
        case PlayerField::SprintSpeed:
            settings.sprintSpeed = value;
            break;
        case PlayerField::JumpSpeed:
            settings.jumpSpeed = value;
            break;
        case PlayerField::LookSensitivity:
            settings.lookSensitivity = value;
            break;
        case PlayerField::MaximumSlope:
            settings.maximumSlopeDegrees = value;
            break;
        case PlayerField::GravityFactor:
            settings.gravityFactor = value;
            break;
        case PlayerField::MinimumPitch:
            settings.minimumPitch = wi::math::DegreesToRadians(value);
            break;
        case PlayerField::MaximumPitch:
            settings.maximumPitch = wi::math::DegreesToRadians(value);
            break;
        }

        (void)session_->Commands().Execute(
            std::make_unique<bridge::SetPlayerControllerSettingsCommand>(
                scene, entity, settings));
        RefreshInspector();
        RefreshStatus();
    }


    bridge::TransformState StudioRenderPath::CaptureEditorCameraTransform() const
    {
        bridge::TransformState state;
        if (camera == nullptr)
            return state;
        wi::scene::TransformComponent transform;
        transform.MatrixTransform(camera->GetInvView());
        transform.UpdateTransform();
        return bridge::CaptureTransform(transform);
    }

    void StudioRenderPath::CreateCameraFromView()
    {
        if (session_ == nullptr || camera == nullptr)
            return;
        auto command = std::make_unique<bridge::CreateCameraCommand>(
            session_->Scenes().GetScene(),
            bridge::CaptureCamera(*camera),
            CaptureEditorCameraTransform(),
            camera->width,
            camera->height);
        auto* created = command.get();
        if (session_->Commands().Execute(std::move(command)))
        {
            session_->Selection().Select(created->CreatedEntity());
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            RefreshHierarchy();
            RefreshInspector();
            RefreshStatus();
        }
    }

    void StudioRenderPath::CreatePlayerStartFromView()
    {
        if (session_ == nullptr || camera == nullptr)
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto existing = bridge::ResolvePlayerStart(scene);
        if (existing.resolution != bridge::PlayerStartResolution::Missing)
        {
            studioChrome_.SetStatusText(
                "PLAYER START // LEVEL ALREADY HAS ONE");
            return;
        }

        auto playerStartTransform = CaptureEditorCameraTransform();
        // The editor camera represents eye position; Player Start represents
        // capsule feet. This makes Test Level begin from the view the creator
        // was composing instead of spawning the capsule 1.65 metres above it.
        playerStartTransform.translation.y -= 1.65f;
        const XMFLOAT3 cameraEuler = wi::math::QuaternionToRollPitchYaw(
            playerStartTransform.rotation);
        XMStoreFloat4(
            &playerStartTransform.rotation,
            XMQuaternionRotationRollPitchYaw(0.0f, cameraEuler.y, 0.0f));
        auto command = std::make_unique<bridge::CreatePlayerStartCommand>(
            scene,
            playerStartTransform);
        auto* created = command.get();
        if (session_->Commands().Execute(std::move(command)))
        {
            session_->Selection().Select(created->CreatedEntity());
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            SetRenderWorkspaceActive(false);
            RefreshHierarchy();
            RefreshInspector();
            RefreshStatus();
            studioChrome_.SetStatusText(
                "PLAYER START // CREATED // POSITION WITH GIZMO");
        }
    }

    bool StudioRenderPath::CommitSelectedCamera(
        const bridge::CameraState& cameraState)
    {
        if (session_ == nullptr)
            return false;
        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        if (!scene.cameras.Contains(entity))
            return false;
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetCameraCommand>(
                scene, entity, cameraState));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::ApplySelectedCameraProjection(
        const bool orthographic)
    {
        if (session_ == nullptr)
            return;
        const auto entity = session_->Selection().SelectedEntity();
        const auto* authoredCamera =
            session_->Scenes().GetScene().cameras.GetComponent(entity);
        if (authoredCamera == nullptr)
            return;
        auto state = bridge::CaptureCamera(*authoredCamera);
        state.orthographic = orthographic;
        CommitSelectedCamera(state);
    }

    void StudioRenderPath::SetCameraFieldValue(
        bridge::CameraState& cameraState,
        const CameraField field,
        const float value) noexcept
    {
        switch (field)
        {
        case CameraField::FieldOfView:
            cameraState.fieldOfViewDegrees = value;
            break;
        case CameraField::NearPlane:
            cameraState.nearPlane = value;
            break;
        case CameraField::FarPlane:
            cameraState.farPlane = value;
            break;
        case CameraField::FocalLength:
            cameraState.focalLength = value;
            break;
        case CameraField::ApertureSize:
            cameraState.apertureSize = value;
            break;
        case CameraField::OrthoVerticalSize:
            cameraState.orthoVerticalSize = value;
            break;
        }
        cameraState = bridge::SanitizeCameraState(cameraState);
    }

    void StudioRenderPath::BeginCameraSlider(const CameraField field)
    {
        cameraSliderActive_ = false;
        if (session_ == nullptr)
            return;
        const auto entity = session_->Selection().SelectedEntity();
        const auto* authoredCamera =
            session_->Scenes().GetScene().cameras.GetComponent(entity);
        if (authoredCamera == nullptr)
            return;
        cameraSliderActive_ = true;
        cameraSliderField_ = field;
        cameraSliderEntity_ = entity;
        cameraSliderBefore_ = bridge::CaptureCamera(*authoredCamera);
        cameraSliderAfter_ = cameraSliderBefore_;
    }

    void StudioRenderPath::PreviewCameraSlider(
        const CameraField field,
        const float value)
    {
        if (!cameraSliderActive_ || cameraSliderField_ != field ||
            session_ == nullptr)
            return;
        cameraSliderAfter_ = cameraSliderBefore_;
        SetCameraFieldValue(cameraSliderAfter_, field, value);
        auto* authoredCamera = session_->Scenes().GetScene().cameras.GetComponent(
            cameraSliderEntity_);
        if (authoredCamera != nullptr)
            bridge::ApplyCamera(*authoredCamera, cameraSliderAfter_);
    }

    void StudioRenderPath::CommitCameraSlider(
        const CameraField field,
        const float value)
    {
        if (!cameraSliderActive_ || cameraSliderField_ != field ||
            session_ == nullptr)
            return;
        SetCameraFieldValue(cameraSliderAfter_, field, value);
        auto& scene = session_->Scenes().GetScene();
        auto* authoredCamera = scene.cameras.GetComponent(cameraSliderEntity_);
        if (authoredCamera != nullptr)
            bridge::ApplyCamera(*authoredCamera, cameraSliderBefore_);
        (void)session_->Commands().Execute(
            std::make_unique<bridge::SetCameraCommand>(
                scene,
                cameraSliderEntity_,
                cameraSliderBefore_,
                cameraSliderAfter_));
        cameraSliderActive_ = false;
        cameraSliderEntity_ = wi::ecs::INVALID_ENTITY;
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::AlignSelectedCameraToView()
    {
        if (session_ == nullptr || camera == nullptr)
            return;
        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        auto* authoredCamera = scene.cameras.GetComponent(entity);
        if (authoredCamera == nullptr)
            return;
        if (session_->Commands().Execute(
                std::make_unique<bridge::SetTransformCommand>(
                    scene, entity, CaptureEditorCameraTransform())))
        {
            if (auto* transform = scene.transforms.GetComponent(entity))
            {
                authoredCamera->TransformCamera(*transform);
                authoredCamera->UpdateCamera();
            }
            gizmoSuppressedForCameraView_ = true;
            RefreshInspector();
            RefreshStatus();
        }
    }

    void StudioRenderPath::ViewFromSelectedCamera()
    {
        if (session_ == nullptr || camera == nullptr)
            return;
        const auto entity = session_->Selection().SelectedEntity();
        const auto& scene = session_->Scenes().GetScene();
        const auto* authoredCamera = scene.cameras.GetComponent(entity);
        const auto* transform = scene.transforms.GetComponent(entity);
        if (authoredCamera == nullptr || transform == nullptr)
            return;
        bridge::ApplyCamera(*camera, bridge::CaptureCamera(*authoredCamera));
        camera->TransformCamera(*transform);
        camera->UpdateCamera();
        gizmoSuppressedForCameraView_ = true;
        RefreshStatus();
    }

    bool StudioRenderPath::CommitSelectedLight(
        const bridge::LightState& light)
    {
        if (session_ == nullptr)
        {
            return false;
        }
        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        if (!scene.lights.Contains(entity))
        {
            return false;
        }
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetLightCommand>(
                scene,
                entity,
                light));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::ApplySelectedLightType(
        const wi::scene::LightComponent::LightType type)
    {
        StopSunPreview(true);
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = session_->Selection().SelectedEntity();
        const auto* light =
            session_->Scenes().GetScene().lights.GetComponent(entity);
        if (light == nullptr)
        {
            return;
        }
        auto state = bridge::CaptureLight(*light);
        state.type = type;
        CommitSelectedLight(state);
    }

    void StudioRenderPath::ApplySelectedLightToggle(
        const LightToggle toggle,
        const bool value)
    {
        StopSunPreview(true);
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = session_->Selection().SelectedEntity();
        const auto* light =
            session_->Scenes().GetScene().lights.GetComponent(entity);
        if (light == nullptr)
        {
            return;
        }
        auto state = bridge::CaptureLight(*light);
        switch (toggle)
        {
        case LightToggle::CastShadow:
            state.castShadow = value;
            break;
        case LightToggle::Volumetrics:
            state.volumetrics = value;
            break;
        }
        CommitSelectedLight(state);
    }

    void StudioRenderPath::SetLightFieldValue(
        bridge::LightState& light,
        const LightField field,
        const float value) noexcept
    {
        switch (field)
        {
        case LightField::ColorRed:
            light.color.x = std::clamp(value, 0.0f, 1.0f);
            break;
        case LightField::ColorGreen:
            light.color.y = std::clamp(value, 0.0f, 1.0f);
            break;
        case LightField::ColorBlue:
            light.color.z = std::clamp(value, 0.0f, 1.0f);
            break;
        case LightField::Intensity:
            light.intensity = std::clamp(value, 0.0f, 100000.0f);
            break;
        case LightField::Range:
            light.range = std::clamp(value, 0.0f, 100000.0f);
            break;
        case LightField::OuterCone:
            light.outerConeDegrees = std::clamp(value, 0.1f, 89.9f);
            light.innerConeDegrees = std::min(
                light.innerConeDegrees,
                light.outerConeDegrees);
            break;
        case LightField::InnerCone:
            light.innerConeDegrees = std::clamp(
                value,
                0.0f,
                light.outerConeDegrees);
            break;
        case LightField::Radius:
            light.radius = std::clamp(value, 0.0f, 100000.0f);
            break;
        case LightField::Length:
            light.length = std::clamp(value, 0.0f, 100000.0f);
            break;
        case LightField::Height:
            light.height = std::clamp(value, 0.0f, 100000.0f);
            break;
        case LightField::VolumetricBoost:
            light.volumetricBoost = std::clamp(value, 0.0f, 10.0f);
            break;
        }
    }

    void StudioRenderPath::BeginLightSlider(const LightField field)
    {
        StopSunPreview(true);
        lightSliderActive_ = false;
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = session_->Selection().SelectedEntity();
        const auto* light =
            session_->Scenes().GetScene().lights.GetComponent(entity);
        if (light == nullptr)
        {
            return;
        }
        lightSliderActive_ = true;
        lightSliderField_ = field;
        lightSliderEntity_ = entity;
        lightSliderBefore_ = bridge::CaptureLight(*light);
        lightSliderAfter_ = lightSliderBefore_;
    }

    void StudioRenderPath::PreviewLightSlider(
        const LightField field,
        const float value)
    {
        if (!lightSliderActive_ || lightSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        auto* light = session_->Scenes().GetScene().lights.GetComponent(
            lightSliderEntity_);
        if (light == nullptr)
        {
            return;
        }
        lightSliderAfter_ = lightSliderBefore_;
        SetLightFieldValue(lightSliderAfter_, field, value);
        bridge::ApplyLight(*light, lightSliderAfter_);
    }

    void StudioRenderPath::CommitLightSlider(
        const LightField field,
        const float value)
    {
        if (!lightSliderActive_ || lightSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        SetLightFieldValue(lightSliderAfter_, field, value);
        auto& scene = session_->Scenes().GetScene();
        auto* light = scene.lights.GetComponent(lightSliderEntity_);
        if (light != nullptr)
        {
            bridge::ApplyLight(*light, lightSliderBefore_);
            session_->Commands().Execute(
                std::make_unique<bridge::SetLightCommand>(
                    scene,
                    lightSliderEntity_,
                    lightSliderBefore_,
                    lightSliderAfter_));
        }
        lightSliderActive_ = false;
        lightSliderEntity_ = wi::ecs::INVALID_ENTITY;
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::CreateTerrain()
    {
        if (session_ == nullptr)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        // Wicked creates Weather on the Terrain entity when generation starts
        // in a blank scene. Establish Renegade's dedicated Environment carrier
        // first so the two Inspector workspaces never acquire the same owner.
        if (scene.weathers.GetCount() == 0)
        {
            const auto environmentState = bridge::CaptureWeather(
                scene.weather);
            if (!session_->Commands().Execute(
                    std::make_unique<bridge::CreateEnvironmentCommand>(
                        scene,
                        environmentState,
                        "Environment")))
            {
                return;
            }
        }
        if (bridge::FindPrimarySunLight(scene) == wi::ecs::INVALID_ENTITY)
        {
            if (!session_->Commands().Execute(
                    std::make_unique<bridge::CreateSunCommand>(
                        scene,
                        session_->Scenes().WeatherEntity())))
            {
                return;
            }
        }
        if (scene.terrains.GetCount() > 0)
        {
            session_->Selection().Select(scene.terrains.GetEntity(0));
            RefreshHierarchy();
            RefreshInspector();
            return;
        }
        auto command = std::make_unique<bridge::CreateTerrainCommand>(
            scene,
            bridge::TerrainState{},
            "Terrain");
        auto* createCommand = command.get();
        if (!session_->Commands().Execute(std::move(command)))
        {
            return;
        }
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
            ShowStudioMessageBox(
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
                            ShowStudioMessageBox(
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
        ShowStudioMessageBox(report.str(), "Model Import Gate 1");
    }

    void StudioRenderPath::ImportModel()
    {
        if (session_ == nullptr ||
            session_->Projects().CurrentProject().rootPath.empty())
        {
            ShowStudioMessageBox(
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
                            ShowStudioMessageBox(
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
                            ShowStudioMessageBox(error, "Import Model");
                            return;
                        }

                        // Wicked defaults imported humanoids to procedural head
                        // look-at toward world origin. No authored target means
                        // a supposedly stationary FBX turns its head on tick one.
                        // Normalize the retained in-memory asset before cloning
                        // so preview and committed Character share the same pose.
                        auto* isolated = state->prepared.PeekMutableScene();
                        (void)bridge::DisableDefaultHumanoidLookAt(*isolated);
                        creatorModelImporter = {};
                        creatorModelImporter.active = true;
                        creatorModelImporter.sourcePath = state->sourcePath;
                        creatorModelImporter.undoBaseline = session_->Commands().UndoCount();
                        creatorModelImporter.cameraBefore = editorCameraTransform_;
                        creatorModelImporter.cameraFovBefore = camera->fov;
                        creatorModelImporter.cameraCaptured = true;
                        creatorModelImporter.summary = bridge::ImportService::Summarize(*isolated);
                        creatorModelImporter.evidence = bridge::ImportService::SummarizeModelEvidence(*isolated);
                        {
                            std::ostringstream rigStatus;
                            rigStatus << "Source armatures: " << creatorModelImporter.summary.armatures
                                << " | bones: " << creatorModelImporter.evidence.armatureBones;
                            if (isolated->armatures.GetCount() == 0)
                            {
                                rigStatus << "\nNo armature; humanoid retargeting unavailable.";
                            }
                            else
                            {
                                const auto rig = isolated->armatures.GetEntity(0);
                                const auto mapping = bridge::BuildAutoHumanoidMapping(*isolated, rig);
                                rigStatus << "\nAuto-map candidates: " << mapping.mappedBones
                                    << " / " << bridge::HumanoidBoneCount
                                    << (mapping.valid ? " | required bones found" : " | incomplete");
                                if (!mapping.valid)
                                {
                                    constexpr std::array required = {
                                        bridge::HumanoidBone::Hips, bridge::HumanoidBone::Spine,
                                        bridge::HumanoidBone::Head,
                                        bridge::HumanoidBone::LeftUpperLeg,
                                        bridge::HumanoidBone::LeftLowerLeg,
                                        bridge::HumanoidBone::LeftFoot,
                                        bridge::HumanoidBone::RightUpperLeg,
                                        bridge::HumanoidBone::RightLowerLeg,
                                        bridge::HumanoidBone::RightFoot,
                                        bridge::HumanoidBone::LeftUpperArm,
                                        bridge::HumanoidBone::LeftLowerArm,
                                        bridge::HumanoidBone::LeftHand,
                                        bridge::HumanoidBone::RightUpperArm,
                                        bridge::HumanoidBone::RightLowerArm,
                                        bridge::HumanoidBone::RightHand};
                                    rigStatus << "\nMissing required: ";
                                    bool first = true;
                                    for (const auto bone : required)
                                    {
                                        if (mapping.mapping.bones[static_cast<std::size_t>(bone)] !=
                                            wi::ecs::INVALID_ENTITY)
                                            continue;
                                        if (!first)
                                            rigStatus << ", ";
                                        rigStatus << bridge::HumanoidBoneName(bone);
                                        first = false;
                                    }
                                }
                                if (!mapping.error.empty())
                                    rigStatus << "\n" << mapping.error;
                                if (isolated->armatures.GetCount() > 1)
                                    rigStatus << "\nMultiple armatures: inspected the first only.";
                            }
                            rigStatus << "\nMapping is diagnostic only; retarget preview is not ready.";
                            creatorModelImporter.rigDiagnostic = rigStatus.str();
                        }
                        creatorModelImporter.sourceBounds = bridge::ImportService::MeasureModelBounds(*isolated);
                        // V3 starts from the source's authored units. Any
                        // conversion is an explicit Transform stage choice.
                        creatorModelImporter.automaticScale = 1.0f;
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

                        creatorModelImporter.previewScene =
                            wi::allocator::make_shared_single<wi::scene::Scene>();
                        auto& preview = *creatorModelImporter.previewScene;
                        const std::size_t materialStart = preview.materials.GetCount();
                        const std::size_t animationStart = preview.animations.GetCount();

                        // Keep the one expensive conversion for governed commit.
                        // The live importer gets a Wicked prefab copy, so cancelling
                        // or editing the preview never consumes the retained source
                        // scene and Confirm never needs to invoke FBX/GLTF again.
                        std::string previewCloneError;
                        auto modelPreviewClone = CloneCreatorPreviewScene(
                            *state->prepared.PeekMutableScene(),
                            previewCloneError);
                        if (!modelPreviewClone.IsValid())
                        {
                            creatorModelImporter = {};
                            studioChrome_.SetStatusText(
                                "IMPORT MODEL // PREVIEW CLONE FAILED");
                            ShowStudioMessageBox(
                                previewCloneError,
                                "Import Model");
                            return;
                        }
                        creatorModelImporter.preparedForCommit =
                            std::move(state->prepared);

                        bridge::PlaceImportedModelCommand place(
                            preview,
                            std::move(modelPreviewClone),
                            XMFLOAT3(0.0f, CreatorImportStageHeight, 0.0f),
                            creatorModelImporter.automaticScale);
                        if (!place.Execute())
                        {
                            creatorModelImporter = {};
                            studioChrome_.SetStatusText("IMPORT MODEL // PREVIEW PLACE FAILED");
                            return;
                        }
                        creatorModelImporter.previewRoot = place.PlacedEntity();

                        for (std::size_t index = materialStart; index < preview.materials.GetCount(); ++index)
                            creatorModelImporter.materialEntities.push_back(preview.materials.GetEntity(index));
                        ApplyDetectedCreatorPreviewMaterials();
                        for (std::size_t index = animationStart; index < preview.animations.GetCount(); ++index)
                        {
                            const auto entity = preview.animations.GetEntity(index);
                            creatorModelImporter.animationEntities.push_back(entity);
                            const auto* animation = preview.animations.GetComponent(entity);
                            creatorModelImporter.animationSourceRanges.push_back(
                                animation == nullptr ? XMFLOAT2{} :
                                    XMFLOAT2(animation->start, animation->end));
                            if (animation != nullptr)
                            {
                                bridge::CreatorAnimationImportRecipe clip;
                                clip.sourceAnimationIndex = static_cast<std::uint32_t>(index - animationStart);
                                clip.name = CreatorImportEntityName(
                                    preview, entity, "Animation " + std::to_string(index - animationStart + 1));
                                clip.start = animation->start;
                                clip.end = animation->end;
                                clip.enabled = true;
                                creatorModelImporter.animationRecipe.push_back(std::move(clip));
                            }
                        }

                        // Wicked's placement command auto-plays imported clips;
                        // importer playback must instead be explicit and singular.
                        StopCreatorImportPreviewAnimations();
                        creatorModelImporter.weatherEntity = wi::ecs::INVALID_ENTITY;
                        creatorModelImporter.ambientBefore = preview.weather.ambient;
                        creatorModelImporter.ambientCaptured = true;
                        if (const auto* weather = preview.weathers.GetComponent(
                                creatorModelImporter.weatherEntity))
                        {
                            creatorModelImporter.ambientBefore = weather->ambient;
                        }

                        // Preview lighting must never enter the editor command stack.
                        const auto lightState = bridge::MakeNewLightState(
                            wi::scene::LightComponent::DIRECTIONAL);
                        creatorModelImporter.previewLight = preview.Entity_CreateLight(
                            "Importer preview light",
                            XMFLOAT3(0.0f, CreatorImportStageHeight + 4.0f, 0.0f),
                            lightState.color,
                            creatorModelImporter.lightIntensity,
                            lightState.range,
                            lightState.type,
                            lightState.outerConeDegrees * (XM_PI / 180.0f),
                            lightState.innerConeDegrees * (XM_PI / 180.0f));
                        ApplyCreatorImportPreviewLighting();

                        FrameCreatorImportPreviewCamera();

                        // Render exclusively from the transient importer scene.
                        scene = creatorModelImporter.previewScene.get();
                        importScalePanel_.SetPreviewScene(scene);
                        ClearSelectionOutline();
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
        creatorModelImporter.importAsCharacter = false;
        importScaleTitleLabel_.SetText("MODEL IMPORTER // PREVIEW BEFORE COMMIT");
        creatorImportAssetName.SetValue(creatorModelImporter.assetName);
        creatorImportDestination.SetValue(creatorModelImporter.destinationFolder);
        creatorImportRigReadout.SetText(creatorModelImporter.rigDiagnostic);
        creatorImportRigReadout.SetTooltip(creatorModelImporter.rigDiagnostic);
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
        RefreshCreatorExternalAnimationQueue();
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
        ResizeLayout();
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

    void StudioRenderPath::LayoutCreatorImportStageHeadings(
        const float inspectorWidth)
    {
        RefreshCreatorImportWorkspaceSection();
        constexpr std::array<float, 6> bodyHeights = {
            168.0f, 820.0f, 790.0f, 150.0f, 520.0f, 570.0f};
        float rowY = 146.0f;
        float contentOffset = 0.0f;
        for (std::size_t index = 0; index < creatorImportStageButtons.size(); ++index)
        {
            auto& heading = creatorImportStageButtons[index];
            if (!heading.IsVisible())
                continue;
            heading.SetPos(XMFLOAT2(12.0f, rowY));
            heading.SetSize(XMFLOAT2(inspectorWidth - 24.0f, 52.0f));
            rowY += 62.0f;
            if (index == creatorModelImporter.workspaceSection)
            {
                contentOffset = rowY - 184.0f;
                rowY += bodyHeights[index];
            }
        }
        importScalePanel_.OffsetVisibleStageContent(contentOffset);
    }

    void StudioRenderPath::RefreshCreatorImportWorkspaceSection()
    {
        if (!creatorModelImporter.importAsCharacter &&
            (creatorModelImporter.workspaceSection == 3 ||
                creatorModelImporter.workspaceSection == 4))
            creatorModelImporter.workspaceSection = 0;
        const std::size_t section = creatorModelImporter.workspaceSection;
        for (std::size_t index = 0; index < creatorImportStageButtons.size(); ++index)
        {
            auto& heading = creatorImportStageButtons[index];
            heading.SetVisible(creatorModelImporter.importAsCharacter ||
                (index != 3 && index != 4));
            heading.SetText(std::string(index == section ? "▸ " : "  ") +
                std::array<const char*, 6>{"ASSET SETUP", "TRANSFORM & SCALE",
                    "MATERIALS & TEXTURES", "RIG & RETARGETING", "ANIMATIONS",
                    "REVIEW & IMPORT"}[index]);
        }
        creatorImportModelChoice.SetVisible(section == 0);
        creatorImportCharacterChoice.SetVisible(section == 0);
        creatorImportModelChoice.SetText(creatorModelImporter.importAsCharacter
            ? "MODEL" : "✓ MODEL");
        creatorImportCharacterChoice.SetText(creatorModelImporter.importAsCharacter
            ? "✓ CHARACTER" : "CHARACTER");
        creatorImportAssetName.SetVisible(section == 0 || section == 5);
        creatorImportDestination.SetVisible(section == 0 || section == 5);
        creatorImportRigReadout.SetVisible(section == 3 &&
            creatorModelImporter.importAsCharacter);

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
            widget->SetVisible(section == 1);

        for (wi::gui::Widget* widget : {
            static_cast<wi::gui::Widget*>(&creatorImportAnimationLabel),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationCombo),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationName),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationStart),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationEnd),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationEnabled),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationAdd),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationDelete),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationPlay),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationPause),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationStop),
            static_cast<wi::gui::Widget*>(&creatorImportExternalAnimationAdd),
            static_cast<wi::gui::Widget*>(&creatorImportExternalAnimationRemove),
            static_cast<wi::gui::Widget*>(&creatorImportExternalAnimationStatus),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationReadout)})
            widget->SetVisible(section == 4 &&
                creatorModelImporter.importAsCharacter);

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
            ShowStudioMessageBox(
                "The importer lost the already-converted model scene. The project was not changed.",
                "Import Model");
            return;
        }

        auto& preview = CreatorImportActiveScene();
        if (preview.transforms.GetComponent(creatorModelImporter.previewRoot) == nullptr)
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
            // Keep the actual actionable reason visible after dismissing the dialog.
            creatorImportThumbnailStatus.SetText("IMPORT BLOCKED // " + destinationError);
            creatorImportThumbnailStatus.SetTooltip(destinationError);
            studioChrome_.SetStatusText(
                "IMPORT MODEL // DESTINATION PREFLIGHT FAILED // " + destinationError);
            ShowStudioMessageBox(destinationError.c_str(), "Import Model");
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
            struct ExternalSource
            {
                std::string path;
                std::uint64_t sourceHash = 0;
                std::size_t expectedClips = 0;
            };
            std::vector<ExternalSource> externalSources;
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
        for (const auto& source : creatorModelImporter.externalAnimations)
            state->externalSources.push_back(
                {source.sourcePath, source.sourceHash, source.clipCount});
        state->positionOffset = creatorModelImporter.positionOffset;
        state->rotationDegrees = creatorModelImporter.rotationDegrees;
        state->authoredScale = creatorModelImporter.scale;
        state->prepared = std::move(creatorModelImporter.preparedForCommit);

        const auto cameraBefore = creatorModelImporter.cameraBefore;
        const float cameraFovBefore = creatorModelImporter.cameraFovBefore;
        creatorModelImporter.committing = true;

        RestoreCreatorImportPreviewEnvironment();
        // No editor undo commands were created during isolated preview.
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

        // Keep the importer visibly open while the governed transaction runs.
        // The user sees explicit phases instead of an apparently idle Studio.
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        studioChrome_.SetStatusText(
            "IMPORT MODEL // PROCESSING // MATERIALS + GOVERNED TEXTURES");

        const auto importConfirmStarted = std::chrono::steady_clock::now();
        wi::jobsystem::Execute(modelImportWorkload_,
            [this, state, importConfirmStarted](wi::jobsystem::JobArgs)
            {
                if (!state->prepared.IsReady() || state->prepared.PeekScene() == nullptr)
                {
                    state->imported.error =
                        "The retained prepared model scene is unavailable.";
                }
                else
                {
                    // The external preview is disposable. Bake selected native
                    // retarget clips into the retained converted model only on
                    // CONFIRM, before the governed RAsset transaction begins.
                    // Never write preview entities or external file paths into
                    // the user's authored level.
                    auto* sourceScene = state->prepared.PeekMutableScene();
                    if (!state->externalSources.empty())
                    {
                        const auto retargetStarted = std::chrono::steady_clock::now();
                        std::string mappingError;
                        if (!bridge::EnsureHumanoidAnimationSourceMapping(
                                *sourceScene, mappingError))
                            state->imported.error = "Character destination rig: " + mappingError;
                        wi::ecs::Entity destination = wi::ecs::INVALID_ENTITY;
                        if (state->imported.error.empty())
                        {
                            for (std::size_t index = 0;
                                index < sourceScene->armatures.GetCount(); ++index)
                            {
                                const auto rig = sourceScene->armatures.GetEntity(index);
                                if (bridge::IsHumanoidMappingValid(
                                        bridge::CaptureHumanoidMapping(*sourceScene, rig)))
                                {
                                    destination = rig;
                                    break;
                                }
                            }
                            if (destination == wi::ecs::INVALID_ENTITY)
                                state->imported.error = "No valid source character rig remains.";
                        }
                        for (const auto& external : state->externalSources)
                        {
                            if (!state->imported.error.empty())
                                break;
                            std::uint64_t hashAtCommit = 0;
                            if (!HashCreatorExternalAnimationSource(
                                    external.path, hashAtCommit) ||
                                hashAtCommit != external.sourceHash)
                            {
                                state->imported.error =
                                    "External animation changed or disappeared after preview; retry import.";
                                break;
                            }
                            bridge::RetargetHumanoidAnimationsCommand retarget(
                                *sourceScene, destination, external.path, true);
                            if (!retarget.Execute())
                            {
                                state->imported.error = "External animation " +
                                    fs::u8path(external.path).filename().generic_u8string() +
                                    ": " + retarget.Result().error;
                                break;
                            }
                            std::uint64_t hashAfterRetarget = 0;
                            if (!HashCreatorExternalAnimationSource(
                                    external.path, hashAfterRetarget) ||
                                hashAfterRetarget != external.sourceHash)
                            {
                                retarget.Undo();
                                state->imported.error =
                                    "External animation changed during import; no asset was committed.";
                                break;
                            }
                            if (retarget.Result().createdAnimations.size() !=
                                external.expectedClips)
                            {
                                retarget.Undo();
                                state->imported.error =
                                    "External animation changed after preview; retry import to avoid saving different clips.";
                                break;
                            }
                        }
                        wi::backlog::post("[IMPORT-PERF] retarget ms=" + std::to_string(
                            std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - retargetStarted).count()));
                    }
                    if (state->imported.error.empty())
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
                    wi::backlog::post("[IMPORT-PERF] materials ms=" +
                        std::to_string(state->materialsSeconds * 1000.0) +
                        " package ms=" + std::to_string(state->packageSeconds * 1000.0));
}
const auto packageFinished = std::chrono::steady_clock::now();
wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, state, importConfirmStarted, packageFinished](std::uint64_t)
                    {
                        const auto handbackStarted = std::chrono::steady_clock::now();
                        wi::backlog::post("[IMPORT-PERF] post-package handback wait ms=" +
                            std::to_string(std::chrono::duration<double, std::milli>(
                                handbackStarted - packageFinished).count()));
                        importScalePanel_.SetEnabled(true);
                        importScalePanel_.SetVisible(false);
                        importScalePanel_.SetPreviewScene(nullptr);
                        scene = &session_->Scenes().GetScene();
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
                            const bool committed = state->imported.asset.transaction.committed;
                            const std::string destination = state->imported.assetProjectRelativePath.empty()
                                ? state->destinationFolder
                                : state->imported.assetProjectRelativePath;
                            const std::string stage = committed
                                ? "POST-COMMIT VERIFICATION"
                                : (state->imported.asset.import.succeeded
                                    ? "PACKAGE / TRANSACTION"
                                    : "PREPARATION / PACKAGE");
                            studioChrome_.SetStatusText(
                                "IMPORT MODEL // " + stage + " FAILED // " + state->imported.error);
                            ShowStudioMessageBox(
                                "Stage: " + stage + " // Reason: " + state->imported.error +
                                "\nDestination: " + destination +
                                "\nCommitted: " + (committed ? "YES - inspect before retry" : "NO") +
                                "\nAsset ID: " + state->imported.asset.assetId,
                                "Import Model");
                            return;
                        }
                        const auto hierarchyStarted = std::chrono::steady_clock::now();
                        RefreshHierarchy();
                        const auto hierarchyDone = std::chrono::steady_clock::now();
                        RefreshInspector();
                        const auto inspectorDone = std::chrono::steady_clock::now();
                        RefreshStatus();
                        wi::backlog::post("[IMPORT-PERF] handback hierarchy_ms=" +
                            std::to_string(std::chrono::duration<double, std::milli>(
                                hierarchyDone - hierarchyStarted).count()) +
                            " inspector_ms=" + std::to_string(std::chrono::duration<double, std::milli>(
                                inspectorDone - hierarchyDone).count()));
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
                        const auto browserStarted = std::chrono::steady_clock::now();
                        if (!studioChrome_.RevealCreatorAsset(
                                state->imported.asset.assetId,
                                state->imported.assetProjectRelativePath,
                                state->projectId,
                                std::move(state->imported.verifiedCatalogue),
                                browserError))
                        {
                            studioChrome_.SetStatusText(
                                "IMPORT MODEL // ASSET COMMITTED // BROWSER FAILED");
                            ShowStudioMessageBox(
                                "The governed asset was committed, but Studio could not verify it in the Asset Browser. Do not import it again.\n\nAsset: " +
                                    state->imported.assetProjectRelativePath +
                                    "\n\nReason: " + browserError,
                                "Import Model");
                            return;
                        }
                        wi::backlog::post("[IMPORT-PERF] handback browser-reveal_ms=" +
                            std::to_string(std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - browserStarted).count()));
                        wi::backlog::post("[IMPORT-PERF] confirm-to-handback ms=" +
                            std::to_string(std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - importConfirmStarted).count()) +
                            " handback-main-thread ms=" +
                            std::to_string(std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - handbackStarted).count()));
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
            // The preview was never merged into the authored scene.
            importScalePanel_.SetPreviewScene(nullptr);
            scene = &session_->Scenes().GetScene();
            if (creatorModelImporter.cameraCaptured)
            {
                editorCameraTransform_ = creatorModelImporter.cameraBefore;
                editorCameraTransform_.UpdateTransform();
                camera->fov = creatorModelImporter.cameraFovBefore;
                camera->TransformCamera(editorCameraTransform_);
                camera->UpdateCamera();
            }
            ClearSelectionOutline();
            RefreshHierarchy();
            RefreshInspector();
            SyncSelectionOutline();
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

    void StudioRenderPath::ShowStudioMessageBox(
        const std::string& message,
        const std::string& caption)
    {
        // Native modal dialogs stop Wicked's input update loop. Reporting
        // failures through the backlog keeps the Studio window interactive,
        // while the status line still identifies the failed operation.
        wi::backlog::post(
            caption + " // " + message,
            wi::backlog::LogLevel::Error);
        const std::size_t firstLine = message.find('\n');
        studioChrome_.SetStatusText(
            caption + " // " +
            message.substr(0, firstLine == std::string::npos
                ? message.size()
                : firstLine));
        wi::input::ClearForNextFrame();
        wi::input::HidePointer(false);
        wi::input::ResetCursors();
        studioChrome_.ResetTransientPointerState();
    }

    void StudioRenderPath::StartTestLevel()
    {
        projectPreviewActive_ = false;
        if (session_ == nullptr || !session_->Projects().HasProject())
        {
            ShowStudioMessageBox(
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
            session_->Commands(),
            &session_->Scripts());
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
            ShowStudioMessageBox(
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
            ShowStudioMessageBox(message, "Test Level");
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
            ShowStudioMessageBox(
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
            ShowStudioMessageBox(
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
            ShowStudioMessageBox(
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
            ShowStudioMessageBox(
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
        ShowStudioMessageBox(message,
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
            ShowStudioMessageBox(result.warning,
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
        RequestDiagnosticAction(EditorAction::ProjectHub);
    }

    void StudioRenderPath::RequestAssetBrowserFromStoryFlow()
    {
        RefreshAssetBrowser();
    }

    void StudioRenderPath::RequestProjectPlayFromStoryFlow()
    {
        RequestDiagnosticAction(EditorAction::StartProjectPlay);
    }

    void StudioRenderPath::RequestWindowsGameBuild()
    {
        if (session_ == nullptr || !session_->Projects().HasProject())
        {
            SetWindowsGameBuildStatus(
                "BUILD FAILED // AN ACTIVE RENEGADE PROJECT IS REQUIRED");
            return;
        }
        if (windowsGameBuildPreparationActive_ || windowsGameBuildRequested_)
        {
            SetWindowsGameBuildStatus(
                "BUILD WINDOWS GAME // ALREADY QUEUED");
            return;
        }

        windowsGameBuildPreparationActive_ = true;
        SetWindowsGameBuildStatus(
            "BUILD WINDOWS GAME // SAVING DIRTY PROJECT DOCUMENTS");
        StopSunPreview(true);

        const auto finishScenePreparation = [this](const bool saved)
        {
            windowsGameBuildPreparationActive_ = false;
            if (!saved)
            {
                const std::string detail = session_ == nullptr
                    ? std::string{}
                    : session_->Scenes().LastError();
                SetWindowsGameBuildStatus(
                    detail.empty()
                        ? "BUILD FAILED // SCENE SAVE WAS CANCELLED OR FAILED"
                        : "BUILD FAILED // SCENE SAVE FAILED // " + detail);
                return;
            }

            windowsGameBuildRequested_ = true;
            SetWindowsGameBuildStatus(
                "BUILD WINDOWS GAME // QUEUED // SCENE SAVED");
        };

        if (!session_->Commands().IsDirty())
        {
            finishScenePreparation(true);
            return;
        }

        const std::string scenePath = session_->Scenes().CurrentPath();
        if (scenePath.empty())
        {
            SaveSceneAs(finishScenePreparation);
            return;
        }
        SaveSceneAfterTransientCleanup(scenePath, finishScenePreparation);
    }

    bool StudioRenderPath::ConsumeWindowsGameBuildRequest() noexcept
    {
        return std::exchange(windowsGameBuildRequested_, false);
    }

    void StudioRenderPath::SetWindowsGameBuildStatus(std::string message)
    {
        studioChrome_.SetStatusText(std::move(message));
        studioChrome_.SetActiveBottomTab(2, true);
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
        gizmoSuppressedForCameraView_ = false;
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
        if (session_ != nullptr)
        {
            auto& scene = session_->Scenes().GetScene();
            const std::size_t count = std::min(
                outlinedEntities_.size(), outlinedEntityPreviousStencils_.size());
            for (std::size_t index = 0; index < count; ++index)
            {
                auto* object = scene.objects.GetComponent(outlinedEntities_[index]);
                if (object != nullptr)
                    object->SetUserStencilRef(outlinedEntityPreviousStencils_[index]);
            }
        }
        outlinedSelection_ = wi::ecs::INVALID_ENTITY;
        outlinedEntities_.clear();
        outlinedEntityPreviousStencils_.clear();
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
        if (selected == outlinedSelection_)
            return;

        ClearSelectionOutline();
        if (session_ == nullptr || selected == wi::ecs::INVALID_ENTITY)
            return;

        auto& scene = session_->Scenes().GetScene();
        const wi::ecs::Entity reusableRoot =
            ResolveReusableSelectionRoot(scene, selected);
        if (reusableRoot == selected)
        {
            std::vector<wi::ecs::Entity> renderObjects;
            CollectReusableSelectionObjects(scene, reusableRoot, renderObjects);
            for (const wi::ecs::Entity entity : renderObjects)
            {
                auto* object = scene.objects.GetComponent(entity);
                if (object == nullptr)
                    continue;
                outlinedEntities_.push_back(entity);
                outlinedEntityPreviousStencils_.push_back(object->userStencilRef);
                object->SetUserStencilRef(SelectionStencilReference);
            }
            if (!outlinedEntities_.empty())
                outlinedSelection_ = selected;
            return;
        }

        auto* object = scene.objects.GetComponent(selected);
        if (object == nullptr)
            return;
        outlinedSelection_ = selected;
        outlinedEntities_.push_back(selected);
        outlinedEntityPreviousStencils_.push_back(object->userStencilRef);
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
