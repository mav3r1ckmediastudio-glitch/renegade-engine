#include "StudioApplication.h"

#include "renegade/bridge/TestLevelSnapshotService.h"
#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "renegade/bridge/CreatorModelMaterialPreparationService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include <algorithm>
#include <chrono>
#include <cmath>
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
        std::size_t workspaceSection = 0;
    };

    CreatorModelImportWorkspaceState creatorModelImporter;
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
    void StudioRenderPath::BindSession(bridge::StudioSession& session) noexcept
    {
        session_ = &session;
        scene = &session.Scenes().GetScene();
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
            pendingAction_ = EditorAction::CreateTerrain;
        });
        inspectorPanel_.AddWidget(&createTerrainButton_);

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
        createTerrainSlider(terrainVisibleRadius_, "Terrain Visible Radius",
            "VISIBLE CHUNKS", "Generated chunk radius around the terrain origin.",
            TerrainField::VisibleChunkRadius, 1.0f, 16.0f, 15.0f);
        createTerrainSlider(terrainChunkScale_, "Terrain Chunk Scale",
            "CHUNK SCALE", "World scale of each streamed terrain chunk.",
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
                pendingAction_ = EditorAction::ApplyTerrainMaterialPreset;
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
            pendingAction_ = EditorAction::ApplyDefaultGrass;
        });
        inspectorPanel_.AddWidget(&terrainApplyDefaultGrassButton_);

        terrainReloadMaterialButton_.Create("Reload Terrain Material");
        terrainReloadMaterialButton_.SetText("RELOAD FILES");
        terrainReloadMaterialButton_.SetTooltip(
            "Reload changed bundled texture files without rebuilding Studio.");
        terrainReloadMaterialButton_.OnClick(
            [this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::ReloadTerrainMaterial;
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

        focusButton_.Create("Focus Selected");
        focusButton_.SetText("FOCUS [F]");
        focusButton_.SetTooltip("Frame the selected entity in the viewport");
        focusButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::FocusSelection;
        });
        inspectorPanel_.AddWidget(&focusButton_);

        duplicateButton_.Create("Duplicate Selected");
        duplicateButton_.SetText("DUPLICATE");
        duplicateButton_.SetTooltip("Duplicate selected entity (Ctrl+D)");
        duplicateButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::DuplicateSelection;
        });
        inspectorPanel_.AddWidget(&duplicateButton_);

        deleteButton_.Create("Delete Selected");
        deleteButton_.SetText("DELETE");
        deleteButton_.SetTooltip("Delete selected entity (Delete)");
        deleteButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::DeleteSelection;
        });
        inspectorPanel_.AddWidget(&deleteButton_);

        undoButton_.Create("Undo Transform");
        undoButton_.SetText("UNDO");
        undoButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        undoButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::Undo;
        });
        inspectorPanel_.AddWidget(&undoButton_);

        redoButton_.Create("Redo Transform");
        redoButton_.SetText("REDO");
        redoButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        redoButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::Redo;
        });
        inspectorPanel_.AddWidget(&redoButton_);

        saveButton_.Create("Save Scene");
        saveButton_.SetText("SAVE");
        saveButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        saveButton_.SetTooltip("Save the current scene (Ctrl+S)");
        saveButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::SaveScene;
        });
        inspectorPanel_.AddWidget(&saveButton_);

        saveAsButton_.Create("Save Scene As");
        saveAsButton_.SetText("SAVE AS...");
        saveAsButton_.SetSize(XMFLOAT2(112.0f, 28.0f));
        saveAsButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::SaveSceneAs;
        });
        inspectorPanel_.AddWidget(&saveAsButton_);

        reopenButton_.Create("Reopen Scene");
        reopenButton_.SetText("REOPEN");
        reopenButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        reopenButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::ReopenScene;
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
            switch (action)
            {
            case RenegadeStudioChrome::Action::ProjectHub:
                pendingAction_ = EditorAction::ProjectHub;
                break;
            case RenegadeStudioChrome::Action::OpenScene:
                pendingAction_ = EditorAction::OpenScene;
                break;
            case RenegadeStudioChrome::Action::Save:
                pendingAction_ = EditorAction::SaveScene;
                break;
            case RenegadeStudioChrome::Action::SaveAs:
                pendingAction_ = EditorAction::SaveSceneAs;
                break;
            case RenegadeStudioChrome::Action::Reopen:
                pendingAction_ = EditorAction::ReopenScene;
                break;
            case RenegadeStudioChrome::Action::Undo:
                pendingAction_ = EditorAction::Undo;
                break;
            case RenegadeStudioChrome::Action::Redo:
                pendingAction_ = EditorAction::Redo;
                break;
            case RenegadeStudioChrome::Action::Duplicate:
                pendingAction_ = EditorAction::DuplicateSelection;
                break;
            case RenegadeStudioChrome::Action::Delete:
                pendingAction_ = EditorAction::DeleteSelection;
                break;
            case RenegadeStudioChrome::Action::CreatePointLight:
                pendingLightType_ = wi::scene::LightComponent::POINT;
                pendingAction_ = EditorAction::CreateLight;
                break;
            case RenegadeStudioChrome::Action::CreateSpotLight:
                pendingLightType_ = wi::scene::LightComponent::SPOT;
                pendingAction_ = EditorAction::CreateLight;
                break;
            case RenegadeStudioChrome::Action::CreateDirectionalLight:
                pendingLightType_ = wi::scene::LightComponent::DIRECTIONAL;
                pendingAction_ = EditorAction::CreateLight;
                break;
            case RenegadeStudioChrome::Action::CreateRectangleLight:
                pendingLightType_ = wi::scene::LightComponent::RECTANGLE;
                pendingAction_ = EditorAction::CreateLight;
                break;
            case RenegadeStudioChrome::Action::Focus:
                pendingAction_ = EditorAction::FocusSelection;
                break;
            case RenegadeStudioChrome::Action::ToggleGrid:
                pendingAction_ = EditorAction::ToggleGrid;
                break;
            case RenegadeStudioChrome::Action::EnvironmentWorkspace:
                pendingAction_ = EditorAction::OpenEnvironmentWorkspace;
                break;
            case RenegadeStudioChrome::Action::TerrainWorkspace:
                pendingAction_ = EditorAction::OpenTerrainWorkspace;
                break;
            case RenegadeStudioChrome::Action::SceneWorkspace:
                pendingAction_ = EditorAction::OpenSceneWorkspace;
                break;
            case RenegadeStudioChrome::Action::TestLevelPlay:
                pendingAction_ = EditorAction::StartTestLevel;
                break;
            case RenegadeStudioChrome::Action::TestLevelStop:
                pendingAction_ = EditorAction::StopTestLevel;
                break;
            case RenegadeStudioChrome::Action::ValidateModelImport:
                pendingAction_ = EditorAction::ValidateModelImport;
                break;
            case RenegadeStudioChrome::Action::ImportModel:
                pendingAction_ = EditorAction::ImportModel;
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
        GetGUI().AddWidget(&studioChrome_);
    }

    void StudioRenderPath::CreateProjectHub()
    {
        projectHubPanel_.Create(
            "Renegade Project Hub",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        projectHubPanel_.SetShadowRadius(12.0f);
        GetGUI().AddWidget(&projectHubPanel_);

        hubBrandLabel_.Create("Renegade Hub Brand");
        hubBrandLabel_.SetText("RENEGADE // IDENTITY ACCEPTED");
        hubBrandLabel_.font.params.size = 14;
        hubBrandLabel_.font.params.color = HologramMuted;
        hubBrandLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&hubBrandLabel_);

        hubTitleLabel_.Create("Renegade Project Hub Title");
        hubTitleLabel_.SetText("PROJECT HUB");
        hubTitleLabel_.font.params.size = 34;
        hubTitleLabel_.font.params.bolden = 0.25f;
        hubTitleLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&hubTitleLabel_);

        hubSubtitleLabel_.Create("Renegade Project Hub Subtitle");
        hubSubtitleLabel_.SetText(
            "Create a new world, open an existing .renegade project, "
            "or resume a recent operation.");
        hubSubtitleLabel_.font.params.size = 16;
        hubSubtitleLabel_.font.params.color = HologramMuted;
        hubSubtitleLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&hubSubtitleLabel_);

        projectNameInput_.Create("New Project Name");
        projectNameInput_.SetDescription("PROJECT NAME: ");
        projectNameInput_.SetText("New Renegade Project");
        projectNameInput_.SetCancelInputEnabled(false);
        projectHubPanel_.AddWidget(&projectNameInput_);

        createProjectButton_.Create("Create Renegade Project");
        createProjectButton_.SetText("CREATE PROJECT");
        createProjectButton_.SetTooltip(
            "Choose a parent folder and create a project from the Proving Ground");
        createProjectButton_.SetAngularHighlightWidth(5.0f);
        createProjectButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            CreateProject();
        });
        projectHubPanel_.AddWidget(&createProjectButton_);

        openProjectButton_.Create("Open Renegade Project");
        openProjectButton_.SetText("OPEN PROJECT...");
        openProjectButton_.SetAngularHighlightWidth(5.0f);
        openProjectButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            OpenProject();
        });
        projectHubPanel_.AddWidget(&openProjectButton_);

        openSceneButton_.Create("Open Renegade Scene");
        openSceneButton_.SetText("OPEN SCENE...");
        openSceneButton_.SetTooltip(
            "Open an existing Wicked scene (.wiscene) in Renegade Studio");
        openSceneButton_.SetAngularHighlightWidth(5.0f);
        openSceneButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            OpenScene();
        });
        projectHubPanel_.AddWidget(&openSceneButton_);

        recentProjectsLabel_.Create("Recent Projects");
        recentProjectsLabel_.SetText("RECENT OPERATIONS");
        recentProjectsLabel_.font.params.size = 17;
        recentProjectsLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&recentProjectsLabel_);

        for (std::size_t index = 0; index < recentProjectButtons_.size(); ++index)
        {
            auto& button = recentProjectButtons_[index];
            button.Create("Recent Project " + std::to_string(index));
            button.SetText("");
            button.SetAngularHighlightWidth(4.0f);
            button.font.params.h_align = wi::font::WIFALIGN_LEFT;
            button.font.params.v_align = wi::font::WIFALIGN_TOP;
            button.OnClick([this, index](const wi::gui::EventArgs&)
            {
                SelectRecentProject(index);
            });
            projectHubPanel_.AddWidget(&button);
        }

        selectedProjectLabel_.Create("Selected Project");
        selectedProjectLabel_.SetText("SELECT A PROJECT CARD");
        selectedProjectLabel_.SetFitTextEnabled(true);
        selectedProjectLabel_.font.params.size = 17;
        selectedProjectLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&selectedProjectLabel_);

        launchProjectButton_.Create("Launch Selected Project");
        launchProjectButton_.SetText("LAUNCH PROJECT");
        launchProjectButton_.SetAngularHighlightWidth(5.0f);
        launchProjectButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            OpenSelectedRecentProject();
        });
        projectHubPanel_.AddWidget(&launchProjectButton_);

        continueProjectButton_.Create("Continue Current Project");
        continueProjectButton_.SetText("RETURN TO CURRENT PROJECT");
        continueProjectButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            SetProjectHubVisible(false);
        });
        projectHubPanel_.AddWidget(&continueProjectButton_);

        hubMessageLabel_.Create("Project Hub Message");
        hubMessageLabel_.SetText("SYSTEM READY // PROJECT SERVICES ONLINE");
        hubMessageLabel_.SetFitTextEnabled(true);
        hubMessageLabel_.font.params.size = 14;
        hubMessageLabel_.font.params.color = HologramMuted;
        hubMessageLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&hubMessageLabel_);
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
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        // Registration is deferred until every importer page is attached.

        importScaleTitleLabel_.Create("MODEL IMPORTER // PREVIEW BEFORE COMMIT");
        importScaleReadoutLabel_.Create("");
        creatorImportHelpLabel.Create(
            "The model is temporary. The project is unchanged until CONFIRM IMPORT is pressed.");
        creatorImportHelpLabel.SetFitTextEnabled(true);

        creatorImportSectionCombo.Create("Importer Section");
        for (const char* section : {"ASSET", "TRANSFORM & SCALE", "MATERIALS // MAPS + PBR", "LIGHTING & SCALE REFERENCE", "ANIMATION", "IMPORT"})
            creatorImportSectionCombo.AddItem(section);
        creatorImportSectionCombo.SetSelectedWithoutCallback(0);
        creatorImportSectionCombo.OnSelect([this](const wi::gui::EventArgs& args)
        {
            creatorModelImporter.workspaceSection = static_cast<std::size_t>(
                std::max(0, args.iValue));
            importScalePanel_.scrollbar_vertical.SetOffset(0.0f);
            RefreshCreatorImportWorkspaceSection();
        });

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
            field.SetCreatorStyle(true);
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
            field.SetCreatorStyle(true);
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
            slider.SetCreatorStyle(true);
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
            slider.SetCreatorStyle(true);
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
            creatorModelImporter.animationRecipe.erase(
                creatorModelImporter.animationRecipe.begin() +
                static_cast<std::ptrdiff_t>(creatorModelImporter.selectedAnimation));
            if (creatorModelImporter.selectedAnimation > 0)
                --creatorModelImporter.selectedAnimation;
            RebuildCreatorImportAnimationCombo();
        });
        creatorImportAnimationReadout.Create("");
        creatorImportAnimationReadout.SetFitTextEnabled(true);

        creatorImportThumbnailCapture.Create("Capture Asset Thumbnail");
        creatorImportThumbnailCapture.SetText("CAPTURE THUMBNAIL");
        creatorImportThumbnailCapture.SetTooltip(
            "Capture the currently framed importer preview for the Asset Browser. You can adjust the camera and retake it before confirming.");
        creatorImportThumbnailCapture.OnClick([this](const wi::gui::EventArgs&)
        {
            CaptureCreatorImportThumbnail();
        });
        creatorImportThumbnailStatus.Create("THUMBNAIL NOT CAPTURED");
        creatorImportThumbnailStatus.SetText("THUMBNAIL NOT CAPTURED");

        importScaleApplyButton_.Create("Import Model Commit");
        importScaleApplyButton_.SetText("CONFIRM IMPORT");
        importScaleApplyButton_.SetTooltip(
            "Commit the governed reusable asset to the Asset Browser without placing an instance in the level.");
        importScaleApplyButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::ApplyImportScale;
        });

        importScaleDismissButton_.Create("Cancel Model Import");
        importScaleDismissButton_.SetText("CANCEL");
        importScaleDismissButton_.SetTooltip(
            "Discard the temporary preview and return to the level without importing anything.");
        importScaleDismissButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::DismissImportScale;
        });

        creatorImportActionBar.Create("THUMBNAIL & IMPORT");
        creatorImportActionBar.SetText("THUMBNAIL & IMPORT");
        creatorImportActionBar.SetShadowRadius(0.0f);

        // Commit is the final workflow page, matching the other sections and
        // leaving this page available for a future batch-import queue.
        importScaleApplyButton_.SetShadowRadius(0.0f);
        importScaleDismissButton_.SetShadowRadius(0.0f);

        for (wi::gui::Widget* widget : {
            static_cast<wi::gui::Widget*>(&importScaleTitleLabel_),
            static_cast<wi::gui::Widget*>(&importScaleReadoutLabel_),
            static_cast<wi::gui::Widget*>(&creatorImportHelpLabel),
            static_cast<wi::gui::Widget*>(&creatorImportSectionCombo),
            static_cast<wi::gui::Widget*>(&creatorImportAssetName),
            static_cast<wi::gui::Widget*>(&creatorImportDestination),
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
            static_cast<wi::gui::Widget*>(&importScaleModeCombo_),
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
            static_cast<wi::gui::Widget*>(&creatorImportEmissiveStrength),
            static_cast<wi::gui::Widget*>(&creatorImportLightingLabel),
            static_cast<wi::gui::Widget*>(&creatorImportLightIntensity),
            static_cast<wi::gui::Widget*>(&creatorImportLightAzimuth),
            static_cast<wi::gui::Widget*>(&creatorImportLightElevation),
            static_cast<wi::gui::Widget*>(&creatorImportAmbientBrightness),
            static_cast<wi::gui::Widget*>(&creatorImportLightingPreset),
            static_cast<wi::gui::Widget*>(&creatorImportLightingReset),
            static_cast<wi::gui::Widget*>(&creatorImportMannequinVisible),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationLabel),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationCombo),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationName),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationStart),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationEnd),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationEnabled),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationAdd),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationDelete),
            static_cast<wi::gui::Widget*>(&creatorImportAnimationReadout),
            static_cast<wi::gui::Widget*>(&creatorImportActionBar),
            static_cast<wi::gui::Widget*>(&creatorImportThumbnailCapture),
            static_cast<wi::gui::Widget*>(&creatorImportThumbnailStatus),
            static_cast<wi::gui::Widget*>(&importScaleApplyButton_),
            static_cast<wi::gui::Widget*>(&importScaleDismissButton_)})
        {
            widget->SetShadowRadius(0.0f);
            importScalePanel_.AddWidget(widget);
        }
        // Hide only after all child controls have inherited an enabled parent.
        importScalePanel_.SetVisible(false);
        GetGUI().AddWidget(&importScalePanel_);
    }

    void StudioRenderPath::ApplyRenegadeTheme()
    {
        wi::gui::Theme theme;
        theme.image.background = true;
        theme.image.blendFlag = wi::enums::BLENDMODE_ALPHA;
        theme.image.corner_rounding = true;
        for (auto& corner : theme.image.corners_rounding)
        {
            corner.radius = 7.0f;
        }
        theme.font.color = HologramText;
        theme.font.shadow_color = wi::Color(0, 0, 0, 220);
        theme.shadow = 3.0f;
        theme.shadow_color = HologramBorder;
        theme.shadow_highlight = true;
        theme.shadow_highlight_color = XMFLOAT3(0.28f, 0.30f, 0.32f);
        theme.shadow_highlight_spread = 0.18f;
        theme.tooltipImage = theme.image;
        theme.tooltipImage.color = HologramIdle;
        theme.tooltipFont = theme.font;
        theme.tooltip_shadow_color = HologramBorder;

        auto& gui = GetGUI();
        gui.SetTheme(theme);
        gui.SetColor(HologramIdle, wi::gui::IDLE);
        gui.SetColor(HologramFocus, wi::gui::FOCUS);
        gui.SetColor(HologramActive, wi::gui::ACTIVE);
        gui.SetColor(HologramFocus, wi::gui::DEACTIVATING);
        gui.SetColor(HologramPanel, wi::gui::WIDGET_ID_WINDOW_BASE);
        gui.SetColor(
            wi::Color(7, 10, 12, 255),
            wi::gui::WIDGET_ID_TEXTINPUTFIELD_IDLE);
        gui.SetColor(
            HologramFocus,
            wi::gui::WIDGET_ID_TEXTINPUTFIELD_FOCUS);
        gui.SetColor(
            HologramActive,
            wi::gui::WIDGET_ID_TEXTINPUTFIELD_ACTIVE);
        gui.SetColor(
            wi::Color(2, 12, 20, 245),
            wi::gui::WIDGET_ID_SCROLLBAR_BASE_IDLE);
        gui.SetColor(
            HologramFocus,
            wi::gui::WIDGET_ID_SCROLLBAR_KNOB_HOVER);
        gui.SetColor(
            HologramActive,
            wi::gui::WIDGET_ID_SCROLLBAR_KNOB_GRABBED);

        projectHubPanel_.SetColor(
            wi::Color(8, 12, 16, 255),
            wi::gui::WIDGET_ID_WINDOW_BASE);

        // The global Project Hub theme is intentionally not the workspace
        // theme. Reassert the owned Inspector host after the global pass so
        // Wicked cannot repaint its rounded cyan window or section pills.
        inspectorPanel_.SetColor(wi::Color::Transparent());
        inspectorPanel_.SetColor(
            wi::Color(8, 11, 13, 255),
            wi::gui::WIDGET_ID_WINDOW_BASE);
        inspectorPanel_.SetShadowRadius(0.0f);

        // Same reassertion as inspectorPanel_ above -- the Import Scale
        // popup is a separate wi::gui::Window and does not inherit
        // inspectorPanel_'s per-instance override, only the global theme.
        importScalePanel_.SetColor(wi::Color::Transparent());
        importScalePanel_.SetColor(
            HologramPanel,
            wi::gui::WIDGET_ID_WINDOW_BASE);

        const auto ownLabel = [](wi::gui::Label& label)
        {
            label.SetColor(HologramIdle);
            label.SetShadowRadius(0.0f);
            label.font.params.color = HologramText;
            label.font.params.bolden = 0.18f;
            label.font.params.shadowColor = wi::Color::Transparent();
        };
        ownLabel(inspectorLabel_);
        ownLabel(positionLabel_);
        ownLabel(rotationLabel_);
        ownLabel(scaleLabel_);
        ownLabel(lightLabel_);
        ownLabel(environmentSkyLabel_);
        ownLabel(environmentFogLabel_);
        ownLabel(environmentCloudLabel_);
        ownLabel(precipitationLabel_);
        ownLabel(sunLabel_);
        ownLabel(oceanLabel_);
        ownLabel(importScaleTitleLabel_);
        ownLabel(importScaleReadoutLabel_);
        ownLabel(creatorImportActionBar);
        ownLabel(creatorImportThumbnailStatus);
        ownLabel(workspaceTitle_);
        ownLabel(statusLabel_);
        ownLabel(hierarchyLabel_);
        ownLabel(terrainLabel_);
        ownLabel(terrainMaterialLabel_);
        ownLabel(terrainSculptLabel_);
        ownLabel(terrainBrushReadout_);
        ownLabel(terrainStrokeDiagnostic_);
        ownLabel(contentLabel_);
        ownLabel(contentPlaceholder_);
        ownLabel(hubBrandLabel_);
        ownLabel(hubTitleLabel_);
        ownLabel(hubSubtitleLabel_);
        ownLabel(recentProjectsLabel_);
        ownLabel(selectedProjectLabel_);
        ownLabel(hubMessageLabel_);
        ownLabel(creatorImportMaterialLabel);
        ownLabel(creatorImportMaterialReadout);
        ownLabel(creatorImportTextureHelp);
        ownLabel(creatorImportAnimationLabel);
        ownLabel(creatorImportAnimationReadout);
        ownLabel(creatorImportTransformLabel);
        ownLabel(creatorImportMaterialScalarLabel);
        ownLabel(creatorImportLightingLabel);
        ownLabel(creatorImportHelpLabel);

        wi::gui::Theme scrollbarTheme = theme;
        scrollbarTheme.image.corner_rounding = false;
        for (auto& corner : scrollbarTheme.image.corners_rounding)
        {
            corner.radius = 0.0f;
        }
        scrollbarTheme.shadow = 0.0f;
        inspectorPanel_.scrollbar_vertical.SetTheme(scrollbarTheme);
        inspectorPanel_.scrollbar_vertical.SetColor(
            wi::Color(12, 18, 22, 255),
            wi::gui::WIDGET_ID_SCROLLBAR_BASE_IDLE);
        inspectorPanel_.scrollbar_vertical.SetColor(
            wi::Color(38, 52, 61, 255),
            wi::gui::WIDGET_ID_SCROLLBAR_KNOB_INACTIVE);
        inspectorPanel_.scrollbar_vertical.SetColor(
            wi::Color(210, 91, 29, 255),
            wi::gui::WIDGET_ID_SCROLLBAR_KNOB_HOVER);
        inspectorPanel_.scrollbar_vertical.SetColor(
            wi::Color(210, 91, 29, 255),
            wi::gui::WIDGET_ID_SCROLLBAR_KNOB_GRABBED);
        importScalePanel_.scrollbar_vertical.SetTheme(scrollbarTheme);
        importScalePanel_.scrollbar_vertical.SetColor(
            wi::Color(12, 18, 22, 255),
            wi::gui::WIDGET_ID_SCROLLBAR_BASE_IDLE);
        importScalePanel_.scrollbar_vertical.SetColor(
            wi::Color(38, 52, 61, 255),
            wi::gui::WIDGET_ID_SCROLLBAR_KNOB_INACTIVE);
        importScalePanel_.scrollbar_vertical.SetColor(
            wi::Color(210, 91, 29, 255),
            wi::gui::WIDGET_ID_SCROLLBAR_KNOB_HOVER);
        importScalePanel_.scrollbar_vertical.SetColor(
            wi::Color(210, 91, 29, 255),
            wi::gui::WIDGET_ID_SCROLLBAR_KNOB_GRABBED);
    }

    void StudioRenderPath::Update(const float dt)
    {
        PollTestLevel();

        // Scene deserialization runs on Wicked's job system. Keep the current
        // document visible but immutable until its prepared replacement is
        // committed at EVENT_THREAD_SAFE_POINT. This check intentionally
        // precedes RenderPath3D::Update(), because wiGUI callbacks can author
        // scene changes from inside the base update.
        if (sceneOpenInProgress_)
        {
            pendingAction_ = EditorAction::None;
            return;
        }

        RenderPath3D::Update(dt);

        if (session_ == nullptr || projectHubVisible_)
        {
            return;
        }

        QueueCreatorImportScaleRuler();

        if (testLevelRuntime_.IsActive())
        {
            // StopTestLevel is the only editor action that must still take
            // effect while Runtime is active - without this, clicking STOP
            // queues pendingAction_ but this function returns below before
            // ever reaching the pendingAction_ dispatch further down, so the
            // click would have no effect until Runtime happened to exit on
            // its own. Any other action requested during an active session
            // is discarded rather than silently deferred until Runtime
            // exits and then firing unexpectedly.
            if (pendingAction_ == EditorAction::StopTestLevel)
            {
                ProcessPendingAction();
                return;
            }
            pendingAction_ = EditorAction::None;

            const auto state = testLevelRuntime_.LastResult().state;
            statusLabel_.SetText(
                state == TestLevelProcessState::Running
                    ? "TEST LEVEL // RUNNING // UNSAVED SNAPSHOT"
                    : "TEST LEVEL // STARTING // UNSAVED SNAPSHOT");
            studioChrome_.SetSceneDirty(session_->Commands().IsDirty());
            studioChrome_.SetStatusText(statusLabel_.GetText());
            return;
        }

        if (workspaceLayoutDirty_)
        {
            workspaceLayoutDirty_ = false;
            ResizeLayout();
        }

        viewportBounds_ = studioChrome_.ViewportBounds();
        const XMFLOAT4 pointer = wi::input::GetPointer();
        const bool lightIconConsumed = HandleLightSceneIcons(pointer);

        if (sunPreviewPlaying_)
        {
            bridge::SetSunTime(
                sunPreviewCurrent_,
                sunPreviewCurrent_.timeHours +
                    dt * sunPreviewSpeedHoursPerSecond_);
            bridge::ApplySun(
                session_->Scenes().GetScene(),
                EditableWeatherEntity(),
                sunPreviewCurrent_);
            sunTime_.SetValue(sunPreviewCurrent_.timeHours);
            sunAzimuth_.SetValue(sunPreviewCurrent_.azimuthDegrees);
            sunElevation_.SetValue(sunPreviewCurrent_.elevationDegrees);
        }

        HandleEditorShortcuts();

        // wiGUI invokes OnClick while Button::Update is still active. Apply
        // editor actions only after the complete GUI update has returned.
        if (pendingAction_ != EditorAction::None)
        {
            ProcessPendingAction();
            return;
        }

        // The Renegade-owned shell uses deliberate hit regions rather than
        // stock Wicked widgets. Never let a chrome click fall through into
        // scene selection, gizmo manipulation, or camera navigation.
        if (studioChrome_.ConsumedPointerThisFrame())
        {
            return;
        }

        if (lightIconConsumed)
        {
            return;
        }

        if (gizmoEntity_ != session_->Selection().SelectedEntity())
        {
            SyncGizmoSelection();
            SyncSelectionOutline();
        }

        if (HandleCreatorAssetPlacement(pointer))
        {
            return;
        }

        if (HandleLightPlacement(pointer))
        {
            return;
        }

        HandleViewportNavigation(dt, pointer);

        if (GetGUI().HasFocus() && !gizmoDragActive_)
        {
            return;
        }

        if (gizmoEntity_ != wi::ecs::INVALID_ENTITY &&
            !flyCameraActive_)
        {
            gizmo_.Update(*camera, pointer, *this);
        }

        if (HandleTerrainSculpt(pointer))
        {
            return;
        }
        if (HandleViewportSelection(pointer))
        {
            return;
        }

        if (gizmoEntity_ == wi::ecs::INVALID_ENTITY ||
            flyCameraActive_)
        {
            return;
        }

        if (gizmo_.IsDragStarted())
        {
            gizmoDragActive_ = true;
        }

        if (gizmo_.IsDragEnded())
        {
            gizmoDragActive_ = false;
            auto* transform =
                session_->Scenes().GetScene().transforms.GetComponent(gizmoEntity_);
            if (transform == nullptr)
            {
                return;
            }

            const auto transformAfter = bridge::CaptureTransform(*transform);
            transform->translation_local =
                gizmoTransformBefore_.translation;
            transform->rotation_local =
                gizmoTransformBefore_.rotation;
            transform->scale_local =
                gizmoTransformBefore_.scale;
            transform->SetDirty();
            transform->UpdateTransform();

            session_->Commands().Execute(
                std::make_unique<bridge::SetTransformCommand>(
                    session_->Scenes().GetScene(),
                    gizmoEntity_,
                    gizmoTransformBefore_,
                    transformAfter));
            gizmoTransformBefore_ = transformAfter;
            RefreshInspector();
            RefreshStatus();
        }
    }

    void StudioRenderPath::Compose(const wi::graphics::CommandList cmd) const
    {
        RenderPath3D::Compose(cmd);

        auto* device = wi::graphics::GetDevice();
        const wi::graphics::Rect viewportScissor = {
            static_cast<std::int32_t>(
                LogicalToPhysical(viewportBounds_.x)),
            static_cast<std::int32_t>(
                LogicalToPhysical(viewportBounds_.y)),
            static_cast<std::int32_t>(
                LogicalToPhysical(viewportBounds_.z)),
            static_cast<std::int32_t>(
                LogicalToPhysical(viewportBounds_.w)),
        };
        device->BindScissorRects(1, &viewportScissor, cmd);

        if (!projectHubVisible_ &&
            outlinedEntity_ != wi::ecs::INVALID_ENTITY &&
            selectionOutlineMask_.IsValid())
        {
            wi::renderer::BindCommonResources(cmd);
            // Thickness was 2.0, double Wicked's default, which read as a
            // heavy halo rather than a projected edge. 1.0 is one pixel.
            wi::renderer::Postprocess_Outline(
                selectionOutlineMask_,
                cmd,
                0.1f,
                1.0f,
                XMFLOAT4(0.30f, 0.86f, 1.0f, 0.90f));
        }

        if (!projectHubVisible_ && gizmoEntity_ != wi::ecs::INVALID_ENTITY)
        {
            gizmo_.Draw(*camera, wi::input::GetPointer(), cmd);
        }

        const wi::graphics::Rect fullScissor = {
            0,
            0,
            static_cast<std::int32_t>(GetPhysicalWidth()),
            static_cast<std::int32_t>(GetPhysicalHeight()),
        };
        device->BindScissorRects(1, &fullScissor, cmd);
    }

    void StudioRenderPath::ResizeLayout()
    {
        RenderPath3D::ResizeLayout();

        const float width = GetLogicalWidth();
        const float height = GetLogicalHeight();
        studioChrome_.SetLayout(width, height);
        const float toolbarHeight = 54.0f;
        const float leftWidth = projectHubVisible_
            ? std::clamp(width * 0.2f, 250.0f, 310.0f)
            : studioChrome_.HierarchyWidth();
        const float rightWidth = projectHubVisible_
            ? std::clamp(width * 0.22f, 290.0f, 350.0f)
            : studioChrome_.InspectorWidth();
        const float bottomHeight = projectHubVisible_
            ? std::clamp(height * 0.22f, 160.0f, 220.0f)
            : studioChrome_.DrawerHeight();

        viewportBounds_ = projectHubVisible_
            ? XMFLOAT4(
                leftWidth + 16.0f,
                toolbarHeight + 16.0f,
                width - rightWidth - 16.0f,
                height - bottomHeight - 16.0f)
            : studioChrome_.ViewportBounds();

        toolbarPanel_.SetPos(XMFLOAT2(8.0f, 8.0f));
        toolbarPanel_.SetSize(XMFLOAT2(width - 16.0f, toolbarHeight));
        workspaceTitle_.SetPos(XMFLOAT2(14.0f, 12.0f));
        workspaceTitle_.SetSize(XMFLOAT2(300.0f, 30.0f));
        translateToolButton_.SetPos(XMFLOAT2(314.0f, 9.0f));
        translateToolButton_.SetSize(XMFLOAT2(92.0f, 30.0f));
        rotateToolButton_.SetPos(XMFLOAT2(414.0f, 9.0f));
        rotateToolButton_.SetSize(XMFLOAT2(100.0f, 30.0f));
        scaleToolButton_.SetPos(XMFLOAT2(522.0f, 9.0f));
        scaleToolButton_.SetSize(XMFLOAT2(92.0f, 30.0f));
        gridToggleButton_.SetPos(XMFLOAT2(630.0f, 9.0f));
        gridToggleButton_.SetSize(XMFLOAT2(92.0f, 30.0f));
        projectHubButton_.SetPos(XMFLOAT2(width - 132.0f, 9.0f));
        projectHubButton_.SetSize(XMFLOAT2(108.0f, 30.0f));
        statusLabel_.SetPos(XMFLOAT2(736.0f, 14.0f));
        statusLabel_.SetSize(XMFLOAT2(
            std::max(120.0f, width - 890.0f),
            24.0f));

        hierarchyPanel_.SetPos(XMFLOAT2(8.0f, toolbarHeight + 16.0f));
        hierarchyPanel_.SetSize(XMFLOAT2(
            leftWidth,
            height - toolbarHeight - 24.0f));
        hierarchyLabel_.SetPos(XMFLOAT2(12.0f, 10.0f));
        hierarchyLabel_.SetSize(XMFLOAT2(leftWidth - 24.0f, 28.0f));
        hierarchyTree_.SetPos(XMFLOAT2(10.0f, 44.0f));
        hierarchyTree_.SetSize(XMFLOAT2(
            leftWidth - 20.0f,
            height - toolbarHeight - 82.0f));
        hierarchySearch_.SetPos(XMFLOAT2(12.0f, 117.0f));
        hierarchySearch_.SetSize(XMFLOAT2(leftWidth - 24.0f, 31.0f));

        inspectorPanel_.SetPos(XMFLOAT2(
            projectHubVisible_ ? width - rightWidth - 8.0f : width - rightWidth,
            projectHubVisible_ ? toolbarHeight + 16.0f : 64.0f));
        inspectorPanel_.SetSize(XMFLOAT2(
            rightWidth,
            projectHubVisible_
                ? height - toolbarHeight - 24.0f
                : height - 64.0f - 28.0f));
        inspectorLabel_.SetPos(XMFLOAT2(12.0f, 10.0f));
        inspectorLabel_.SetSize(XMFLOAT2(rightWidth - 24.0f, 28.0f));
        const float fieldGap = 8.0f;
        const float fieldWidth = (rightWidth - 40.0f) / 3.0f;
        const auto positionInputRow = [&](
            wi::gui::TextInputField& x,
            wi::gui::TextInputField& y,
            wi::gui::TextInputField& z,
            const float rowY)
        {
            x.SetPos(XMFLOAT2(12.0f, rowY));
            y.SetPos(XMFLOAT2(12.0f + fieldWidth + fieldGap, rowY));
            z.SetPos(XMFLOAT2(
                12.0f + (fieldWidth + fieldGap) * 2.0f,
                rowY));
            x.SetSize(XMFLOAT2(fieldWidth, 28.0f));
            y.SetSize(XMFLOAT2(fieldWidth, 28.0f));
            z.SetSize(XMFLOAT2(fieldWidth, 28.0f));
        };
        positionLabel_.SetPos(XMFLOAT2(12.0f, 44.0f));
        positionLabel_.SetSize(XMFLOAT2(rightWidth - 24.0f, 20.0f));
        positionInputRow(
            translationX_,
            translationY_,
            translationZ_,
            64.0f);
        rotationLabel_.SetPos(XMFLOAT2(12.0f, 104.0f));
        rotationLabel_.SetSize(XMFLOAT2(rightWidth - 24.0f, 20.0f));
        positionInputRow(rotationX_, rotationY_, rotationZ_, 124.0f);
        scaleLabel_.SetPos(XMFLOAT2(12.0f, 164.0f));
        scaleLabel_.SetSize(XMFLOAT2(rightWidth - 24.0f, 20.0f));
        positionInputRow(scaleX_, scaleY_, scaleZ_, 184.0f);

        const float environmentFieldWidth = rightWidth - 24.0f;
        const auto positionEnvironmentWidget =
            [environmentFieldWidth](
                wi::gui::Widget& widget,
                const float rowY,
                const float height = 28.0f)
        {
            widget.SetPos(XMFLOAT2(12.0f, rowY));
            widget.SetSize(XMFLOAT2(environmentFieldWidth, height));
        };
        positionEnvironmentWidget(lightLabel_, 224.0f, 20.0f);
        positionEnvironmentWidget(lightType_, 244.0f);
        positionEnvironmentWidget(lightColorRed_, 278.0f);
        positionEnvironmentWidget(lightColorGreen_, 312.0f);
        positionEnvironmentWidget(lightColorBlue_, 346.0f);
        positionEnvironmentWidget(lightIntensity_, 380.0f);
        positionEnvironmentWidget(lightRange_, 414.0f);
        positionEnvironmentWidget(lightOuterCone_, 448.0f);
        positionEnvironmentWidget(lightInnerCone_, 482.0f);
        positionEnvironmentWidget(lightRadius_, 516.0f);
        positionEnvironmentWidget(lightLength_, 550.0f);
        positionEnvironmentWidget(lightHeight_, 584.0f);
        positionEnvironmentWidget(lightCastShadow_, 618.0f);
        positionEnvironmentWidget(lightVolumetrics_, 650.0f);
        positionEnvironmentWidget(lightVolumetricBoost_, 682.0f);
        positionEnvironmentWidget(environmentSkyLabel_, 44.0f, 20.0f);
        positionEnvironmentWidget(environmentPreset_, 64.0f);
        positionEnvironmentWidget(skyMode_, 98.0f);
        positionEnvironmentWidget(aerialPerspective_, 132.0f);
        positionEnvironmentWidget(skyExposure_, 164.0f);
        positionEnvironmentWidget(ambientIntensity_, 198.0f);
        positionEnvironmentWidget(environmentFogLabel_, 232.0f, 20.0f);
        positionEnvironmentWidget(fogStart_, 252.0f);
        positionEnvironmentWidget(fogDensity_, 286.0f);
        positionEnvironmentWidget(heightFog_, 320.0f);
        positionEnvironmentWidget(fogHeightStart_, 352.0f);
        positionEnvironmentWidget(fogHeightEnd_, 386.0f);
        positionEnvironmentWidget(environmentCloudLabel_, 420.0f, 20.0f);
        positionEnvironmentWidget(cloudCoverage_, 440.0f);
        positionEnvironmentWidget(cloudStartHeight_, 474.0f);
        positionEnvironmentWidget(cloudThickness_, 508.0f);
        positionEnvironmentWidget(cloudsCastShadow_, 542.0f);
        positionEnvironmentWidget(precipitationLabel_, 576.0f, 20.0f);
        positionEnvironmentWidget(precipitationMode_, 596.0f);
        positionEnvironmentWidget(precipitationIntensity_, 630.0f);
        positionEnvironmentWidget(precipitationFallSpeed_, 664.0f);
        positionEnvironmentWidget(precipitationParticleScale_, 698.0f);
        positionEnvironmentWidget(precipitationWindAzimuth_, 732.0f);
        positionEnvironmentWidget(precipitationWindSpeed_, 766.0f);
        positionEnvironmentWidget(precipitationTurbulence_, 800.0f);
        positionEnvironmentWidget(sunLabel_, 834.0f, 20.0f);
        positionEnvironmentWidget(sunPreset_, 854.0f);
        positionEnvironmentWidget(sunTime_, 888.0f);
        positionEnvironmentWidget(sunAzimuth_, 922.0f);
        positionEnvironmentWidget(sunElevation_, 956.0f);
        positionEnvironmentWidget(sunPreviewSpeed_, 990.0f);
        sunPlayButton_.SetPos(XMFLOAT2(12.0f, 1024.0f));
        sunPauseButton_.SetPos(XMFLOAT2(
            20.0f + (environmentFieldWidth - 8.0f) * 0.5f,
            1024.0f));
        sunPlayButton_.SetSize(XMFLOAT2(
            (environmentFieldWidth - 8.0f) * 0.5f,
            28.0f));
        sunPauseButton_.SetSize(XMFLOAT2(
            (environmentFieldWidth - 8.0f) * 0.5f,
            28.0f));
        positionEnvironmentWidget(oceanLabel_, 1060.0f, 20.0f);
        positionEnvironmentWidget(oceanEnabled_, 1080.0f);
        positionEnvironmentWidget(oceanPreset_, 1114.0f);
        positionEnvironmentWidget(oceanResolution_, 1148.0f);
        positionEnvironmentWidget(oceanWaterHeight_, 1182.0f);
        positionEnvironmentWidget(oceanPatchLength_, 1216.0f);
        positionEnvironmentWidget(oceanWaveAmplitude_, 1250.0f);
        positionEnvironmentWidget(oceanChoppyScale_, 1284.0f);
        positionEnvironmentWidget(oceanTimeScale_, 1318.0f);
        positionEnvironmentWidget(oceanWindAzimuth_, 1352.0f);
        positionEnvironmentWidget(oceanWindSpeed_, 1386.0f);
        positionEnvironmentWidget(oceanWindDependency_, 1420.0f);
        positionEnvironmentWidget(oceanSurfaceDetail_, 1454.0f);
        positionEnvironmentWidget(oceanDisplacementTolerance_, 1488.0f);
        positionEnvironmentWidget(oceanWaterRed_, 1522.0f);
        positionEnvironmentWidget(oceanWaterGreen_, 1556.0f);
        positionEnvironmentWidget(oceanWaterBlue_, 1590.0f);
        positionEnvironmentWidget(oceanWaterOpacity_, 1624.0f);
        positionEnvironmentWidget(oceanExtinctionRed_, 1658.0f);
        positionEnvironmentWidget(oceanExtinctionGreen_, 1692.0f);
        positionEnvironmentWidget(oceanExtinctionBlue_, 1726.0f);

        positionEnvironmentWidget(terrainLabel_, 44.0f, 20.0f);
        positionEnvironmentWidget(createTerrainButton_, 64.0f);
        positionEnvironmentWidget(terrainVisibleRadius_, 64.0f);
        positionEnvironmentWidget(terrainChunkScale_, 98.0f);
        positionEnvironmentWidget(terrainMinimumHeight_, 132.0f);
        positionEnvironmentWidget(terrainMaximumHeight_, 166.0f);
        positionEnvironmentWidget(terrainLowAltitudeBlend_, 200.0f);
        positionEnvironmentWidget(terrainBaseBlend_, 234.0f);
        positionEnvironmentWidget(terrainSlopeBlend_, 268.0f);
        positionEnvironmentWidget(terrainLodBias_, 302.0f);
        positionEnvironmentWidget(terrainMaterialLabel_, 346.0f, 20.0f);
        positionEnvironmentWidget(terrainMaterialPreset_, 366.0f);
        positionEnvironmentWidget(terrainTextureScale_, 400.0f);
        terrainApplyDefaultGrassButton_.SetPos(XMFLOAT2(12.0f, 434.0f));
        terrainReloadMaterialButton_.SetPos(XMFLOAT2(
            20.0f + (environmentFieldWidth - 8.0f) * 0.5f,
            434.0f));
        terrainApplyDefaultGrassButton_.SetSize(XMFLOAT2(
            (environmentFieldWidth - 8.0f) * 0.5f,
            28.0f));
        terrainReloadMaterialButton_.SetSize(XMFLOAT2(
            (environmentFieldWidth - 8.0f) * 0.5f,
            28.0f));
        positionEnvironmentWidget(terrainSculptLabel_, 476.0f, 20.0f);
        positionEnvironmentWidget(terrainSculptMode_, 496.0f);
        positionEnvironmentWidget(terrainBrushRadius_, 530.0f);
        positionEnvironmentWidget(terrainBrushStrength_, 564.0f);
        positionEnvironmentWidget(terrainBrushFalloff_, 598.0f);
        positionEnvironmentWidget(terrainBrushReadout_, 632.0f, 20.0f);
        positionEnvironmentWidget(terrainStrokeDiagnostic_, 652.0f, 20.0f);

        const bool environmentSelected =
            environmentWorkspaceActive_;
        const bool terrainSelected = terrainWorkspaceActive_;
        const bool lightSelected =
            session_ != nullptr && !environmentSelected && !terrainSelected &&
            session_->Scenes().GetScene().lights.Contains(
                session_->Selection().SelectedEntity());
        LayoutInspectorActions(
            environmentSelected,
            terrainSelected,
            lightSelected);

        contentPanel_.SetPos(XMFLOAT2(
            leftWidth + 16.0f,
            height - bottomHeight - 8.0f));
        contentPanel_.SetSize(XMFLOAT2(
            width - leftWidth - rightWidth - 32.0f,
            bottomHeight));
        contentLabel_.SetPos(XMFLOAT2(12.0f, 10.0f));
        contentLabel_.SetSize(XMFLOAT2(
            contentPanel_.GetSize().x - 24.0f,
            28.0f));
        contentPlaceholder_.SetPos(XMFLOAT2(12.0f, 50.0f));
        contentPlaceholder_.SetSize(XMFLOAT2(
            contentPanel_.GetSize().x - 24.0f,
            bottomHeight - 62.0f));

        // Positioned independently of the Inspector's hardcoded column
        // (see CreateImportScalePanel). Import mode owns the screen, so this
        // is a right-docked task panel rather than a popup floating inside the
        // editor viewport.
        //
        // wi::gui::Window::Render scissor-clips every child widget to the
        // window's own rectangle (widget->parent->scissorRect), including a
        // ComboBox's dropdown list when it opens -- Wicked's auto-flip
        // logic in ComboBox::GetDropOffset only checks against the full
        // canvas height, not the parent window's bounds, so it never
        // triggers here. The panel must itself be tall enough to contain
        // the combo's fully open dropdown (its own 28px row plus three
        // 28px items, ~112px) or the options render clipped to invisible,
        // unselectable, even though the combo logic itself is fine. This
        // is why the panel is taller than its visible idle content and the
        // buttons sit well below the combo rather than immediately under
        // it.
        const float importScalePanelWidth = std::min(500.0f, std::max(440.0f, width * 0.32f));
        const float importScalePanelTop = 8.0f;
        const float importScalePanelHeight = std::max(320.0f, height - 16.0f);
        // GGMAX-style task workspace: keep the preview unobstructed and dock
        // the importer controls down the right side of the preview viewport.
        const float importScalePanelX =
            std::max(8.0f, width - importScalePanelWidth - 8.0f);
        importScalePanel_.SetPos(XMFLOAT2(
            importScalePanelX,
            importScalePanelTop));
        importScalePanel_.SetSize(XMFLOAT2(
            importScalePanelWidth,
            importScalePanelHeight));
        importScaleTitleLabel_.SetPos(XMFLOAT2(12.0f, 8.0f));
        importScaleTitleLabel_.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 24.0f));
        importScaleReadoutLabel_.SetPos(XMFLOAT2(12.0f, 36.0f));
        importScaleReadoutLabel_.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 64.0f));
        creatorImportHelpLabel.SetPos(XMFLOAT2(12.0f, 100.0f));
        creatorImportHelpLabel.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 42.0f));
        creatorImportSectionCombo.SetPos(XMFLOAT2(12.0f, 146.0f));
        creatorImportSectionCombo.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));

        creatorImportAssetName.SetPos(XMFLOAT2(12.0f, 190.0f));
        creatorImportAssetName.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 32.0f));
        creatorImportDestination.SetPos(XMFLOAT2(12.0f, 230.0f));
        creatorImportDestination.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 32.0f));

        creatorImportTransformLabel.SetPos(XMFLOAT2(12.0f, 184.0f));
        creatorImportTransformLabel.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 22.0f));
        const auto layoutFullRow = [importScalePanelWidth](
            wi::gui::Widget& widget, const float rowY)
        {
            widget.SetPos(XMFLOAT2(12.0f, rowY));
            widget.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));
        };
        const auto layoutSliderRow = [importScalePanelWidth](
            wi::gui::Widget& widget, const float rowY)
        {
            widget.SetPos(XMFLOAT2(12.0f, rowY));
            widget.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 34.0f));
        };
        layoutSliderRow(creatorImportPositionX, 210.0f);
        layoutSliderRow(creatorImportPositionY, 248.0f);
        layoutSliderRow(creatorImportPositionZ, 286.0f);
        layoutSliderRow(creatorImportRotationX, 330.0f);
        layoutSliderRow(creatorImportRotationY, 368.0f);
        layoutSliderRow(creatorImportRotationZ, 406.0f);
        layoutSliderRow(creatorImportScaleX, 450.0f);
        layoutSliderRow(creatorImportScaleY, 488.0f);
        layoutSliderRow(creatorImportScaleZ, 526.0f);
        layoutFullRow(creatorImportScaleLinked, 566.0f);
        layoutFullRow(creatorImportDimensionPreset, 602.0f);
        layoutFullRow(importScaleModeCombo_, 638.0f);

        creatorImportMaterialLabel.SetPos(XMFLOAT2(12.0f, 184.0f));
        creatorImportMaterialLabel.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 22.0f));
        creatorImportMaterialCombo.SetPos(XMFLOAT2(12.0f, 210.0f));
        creatorImportMaterialCombo.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));
        creatorImportMaterialReadout.SetPos(XMFLOAT2(12.0f, 242.0f));
        creatorImportMaterialReadout.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 56.0f));
        creatorImportTexturePreviews.SetPos(XMFLOAT2(12.0f, 302.0f));
        creatorImportTexturePreviews.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 280.0f));
        creatorImportTextureHelp.SetPos(XMFLOAT2(12.0f, 586.0f));
        creatorImportTextureHelp.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 20.0f));
        creatorImportTextureSlotCombo.SetPos(XMFLOAT2(12.0f, 610.0f));
        creatorImportTextureSlotCombo.SetSize(XMFLOAT2(150.0f, 28.0f));
        creatorImportTexturePath.SetPos(XMFLOAT2(166.0f, 610.0f));
        creatorImportTexturePath.SetSize(XMFLOAT2(importScalePanelWidth - 178.0f, 28.0f));
        creatorImportTextureBrowse.SetPos(XMFLOAT2(12.0f, 642.0f));
        creatorImportTextureBrowse.SetSize(XMFLOAT2(120.0f, 28.0f));
        creatorImportTextureClear.SetPos(XMFLOAT2(136.0f, 642.0f));
        creatorImportTextureClear.SetSize(XMFLOAT2(100.0f, 28.0f));
        creatorImportMaterialScalarLabel.SetPos(XMFLOAT2(12.0f, 682.0f));
        creatorImportMaterialScalarLabel.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 20.0f));
        layoutSliderRow(creatorImportRoughness, 710.0f);
        layoutSliderRow(creatorImportMetalness, 748.0f);
        layoutSliderRow(creatorImportReflectance, 786.0f);
        layoutSliderRow(creatorImportNormalStrength, 824.0f);
        layoutSliderRow(creatorImportAoStrength, 862.0f);
        layoutSliderRow(creatorImportEmissiveStrength, 900.0f);

        creatorImportLightingLabel.SetPos(XMFLOAT2(12.0f, 184.0f));
        creatorImportLightingLabel.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 22.0f));
        layoutSliderRow(creatorImportLightIntensity, 214.0f);
        layoutSliderRow(creatorImportLightAzimuth, 254.0f);
        layoutSliderRow(creatorImportLightElevation, 294.0f);
        layoutSliderRow(creatorImportAmbientBrightness, 334.0f);
        layoutFullRow(creatorImportLightingPreset, 366.0f);
        layoutFullRow(creatorImportLightingReset, 402.0f);
        layoutFullRow(creatorImportMannequinVisible, 446.0f);

        creatorImportAnimationLabel.SetPos(XMFLOAT2(12.0f, 184.0f));
        creatorImportAnimationLabel.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 22.0f));
        creatorImportAnimationCombo.SetPos(XMFLOAT2(12.0f, 210.0f));
        creatorImportAnimationCombo.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));
        creatorImportAnimationName.SetPos(XMFLOAT2(12.0f, 246.0f));
        creatorImportAnimationName.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));
        creatorImportAnimationStart.SetPos(XMFLOAT2(12.0f, 282.0f));
        creatorImportAnimationStart.SetSize(XMFLOAT2(120.0f, 28.0f));
        creatorImportAnimationEnd.SetPos(XMFLOAT2(136.0f, 282.0f));
        creatorImportAnimationEnd.SetSize(XMFLOAT2(120.0f, 28.0f));
        creatorImportAnimationEnabled.SetPos(XMFLOAT2(260.0f, 282.0f));
        creatorImportAnimationEnabled.SetSize(XMFLOAT2(importScalePanelWidth - 272.0f, 28.0f));
        creatorImportAnimationAdd.SetPos(XMFLOAT2(12.0f, 318.0f));
        creatorImportAnimationAdd.SetSize(XMFLOAT2(110.0f, 28.0f));
        creatorImportAnimationDelete.SetPos(XMFLOAT2(126.0f, 318.0f));
        creatorImportAnimationDelete.SetSize(XMFLOAT2(120.0f, 28.0f));
        creatorImportAnimationReadout.SetPos(XMFLOAT2(12.0f, 354.0f));
        creatorImportAnimationReadout.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 42.0f));
        creatorImportActionBar.SetPos(XMFLOAT2(12.0f, 278.0f));
        creatorImportActionBar.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));
        creatorImportThumbnailCapture.SetPos(XMFLOAT2(12.0f, 318.0f));
        creatorImportThumbnailCapture.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 40.0f));
        creatorImportThumbnailStatus.SetPos(XMFLOAT2(12.0f, 368.0f));
        creatorImportThumbnailStatus.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));
        importScaleApplyButton_.SetPos(XMFLOAT2(12.0f, 410.0f));
        importScaleApplyButton_.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 44.0f));
        importScaleDismissButton_.SetPos(XMFLOAT2(12.0f, 464.0f));
        importScaleDismissButton_.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 34.0f));

        projectHubPanel_.SetPos(XMFLOAT2(12.0f, 12.0f));
        projectHubPanel_.SetSize(XMFLOAT2(width - 24.0f, height - 24.0f));
        hubBrandLabel_.SetPos(XMFLOAT2(38.0f, 24.0f));
        hubBrandLabel_.SetSize(XMFLOAT2(500.0f, 24.0f));
        hubTitleLabel_.SetPos(XMFLOAT2(38.0f, 58.0f));
        hubTitleLabel_.SetSize(XMFLOAT2(500.0f, 48.0f));
        hubSubtitleLabel_.SetPos(XMFLOAT2(40.0f, 112.0f));
        hubSubtitleLabel_.SetSize(XMFLOAT2(width - 100.0f, 30.0f));

        projectNameInput_.SetPos(XMFLOAT2(40.0f, 160.0f));
        projectNameInput_.SetSize(XMFLOAT2(330.0f, 32.0f));
        createProjectButton_.SetPos(XMFLOAT2(388.0f, 160.0f));
        createProjectButton_.SetSize(XMFLOAT2(170.0f, 32.0f));
        openProjectButton_.SetPos(XMFLOAT2(570.0f, 160.0f));
        openProjectButton_.SetSize(XMFLOAT2(170.0f, 32.0f));
        openSceneButton_.SetPos(XMFLOAT2(752.0f, 160.0f));
        openSceneButton_.SetSize(XMFLOAT2(170.0f, 32.0f));

        recentProjectsLabel_.SetPos(XMFLOAT2(40.0f, 218.0f));
        recentProjectsLabel_.SetSize(XMFLOAT2(500.0f, 28.0f));

        const float previewX = std::max(820.0f, width * 0.64f);
        const float cardsWidth = std::max(620.0f, previewX - 70.0f);
        const float cardWidth = (cardsWidth - 18.0f) * 0.5f;
        constexpr float cardHeight = 76.0f;
        for (std::size_t index = 0; index < recentProjectButtons_.size(); ++index)
        {
            const float x =
                40.0f + static_cast<float>(index % 2) * (cardWidth + 18.0f);
            const float y =
                254.0f + static_cast<float>(index / 2) * (cardHeight + 14.0f);
            recentProjectButtons_[index].SetPos(XMFLOAT2(x, y));
            recentProjectButtons_[index].SetSize(
                XMFLOAT2(cardWidth, cardHeight));
            recentProjectButtons_[index].font.params.h_wrap =
                cardWidth - 20.0f;
        }

        selectedProjectLabel_.SetPos(XMFLOAT2(previewX, 254.0f));
        selectedProjectLabel_.SetSize(XMFLOAT2(
            std::max(260.0f, width - previewX - 50.0f),
            140.0f));
        launchProjectButton_.SetPos(XMFLOAT2(previewX, 410.0f));
        launchProjectButton_.SetSize(XMFLOAT2(240.0f, 42.0f));
        continueProjectButton_.SetPos(XMFLOAT2(previewX, 466.0f));
        continueProjectButton_.SetSize(XMFLOAT2(240.0f, 36.0f));

        hubMessageLabel_.SetPos(XMFLOAT2(40.0f, height - 74.0f));
        hubMessageLabel_.SetSize(XMFLOAT2(width - 90.0f, 38.0f));

        if (diagnostics_ != nullptr)
        {
            diagnostics_->rect.left = static_cast<std::int32_t>(
                LogicalToPhysical(viewportBounds_.x + 10.0f));
            diagnostics_->rect.top = static_cast<std::int32_t>(
                LogicalToPhysical(viewportBounds_.y + 10.0f));
            diagnostics_->rect.right = static_cast<std::int32_t>(
                LogicalToPhysical(viewportBounds_.z));
            diagnostics_->rect.bottom = static_cast<std::int32_t>(
                LogicalToPhysical(viewportBounds_.w));
        }
    }

    void StudioRenderPath::RefreshStatus()
    {
        if (session_ == nullptr)
        {
            statusLabel_.SetText("ENGINEBRIDGE SESSION UNAVAILABLE");
            return;
        }

        const auto& scenes = session_->Scenes();
        if (sceneOpenInProgress_)
        {
            const std::string openingName = wi::helper::GetFileNameFromPath(
                openingScenePath_);
            statusLabel_.SetText("OPENING SCENE // " + openingName);
            studioChrome_.SetStatusText(statusLabel_.GetText());
            return;
        }
        if (!scenes.LastError().empty())
        {
            statusLabel_.SetText("SCENE ERROR // " + scenes.LastError());
            studioChrome_.SetSceneDirty(session_->Commands().IsDirty());
            studioChrome_.SetStatusText(statusLabel_.GetText());
            return;
        }

        if (!session_->Documents().LastWarning().empty())
        {
            statusLabel_.SetText(
                "SCENE WARNING // " + session_->Documents().LastWarning());
            studioChrome_.SetSceneDirty(session_->Commands().IsDirty());
            studioChrome_.SetStatusText(statusLabel_.GetText());
            return;
        }

        if (lightPlacementActive_)
        {
            statusLabel_.SetText(
                std::string("PLACE ") +
                PlacementLightName(lightPlacementType_) +
                " LIGHT // LEFT CLICK SURFACE // ESC OR RIGHT CLICK CANCEL");
            studioChrome_.SetSceneDirty(session_->Commands().IsDirty());
            studioChrome_.SetStatusText(statusLabel_.GetText());
            return;
        }

        const std::string projectName = session_->Projects().HasProject()
            ? session_->Projects().CurrentProject().name
            : "PROVING GROUND";
        const char* transformTool = gizmo_.isRotator
            ? "ROTATE"
            : gizmo_.isScalator
                ? "SCALE"
                : "MOVE";
        statusLabel_.SetText(
            projectName + " // " +
            std::to_string(scenes.ListEntities().size()) +
            " ITEMS // " +
            transformTool +
            " // UNDO " +
            std::to_string(session_->Commands().UndoCount()) +
            " // REDO " +
            std::to_string(session_->Commands().RedoCount()));
        std::string sceneName = projectName;
        if (!scenes.CurrentPath().empty())
        {
            sceneName = wi::helper::RemoveExtension(
                wi::helper::GetFileNameFromPath(scenes.CurrentPath()));
        }
        studioChrome_.SetSceneName(sceneName);
        studioChrome_.SetSceneDirty(session_->Commands().IsDirty());
        studioChrome_.SetStatusText(statusLabel_.GetText());
    }

    void StudioRenderPath::RefreshHierarchy()
    {
        hierarchyTree_.ClearItems();
        std::vector<RenegadeStudioChrome::HierarchyRow> chromeRows;
        if (session_ == nullptr)
        {
            studioChrome_.SetHierarchyRows({});
            return;
        }

        const auto selected = session_->Selection().SelectedEntity();
        const auto weatherEntity = session_->Scenes().WeatherEntity();
        for (const auto& entity : session_->Scenes().ListEntities())
        {
            if (entity.entity == weatherEntity)
            {
                continue;
            }
            wi::gui::TreeList::Item item;
            item.name = entity.name;
            item.level = entity.depth;
            item.userdata = entity.entity;
            item.open = true;
            item.selected = entity.entity == selected;
            hierarchyTree_.AddItem(item);
            chromeRows.push_back({
                entity.name,
                entity.depth,
                entity.entity == selected,
                static_cast<std::uint64_t>(entity.entity),
                ToHierarchyCategory(entity.category),
            });
        }
        studioChrome_.SetHierarchyRows(std::move(chromeRows));
    }

    void StudioRenderPath::LayoutInspectorActions(
        const bool environment,
        const bool terrain,
        const bool light)
    {
        const float width = inspectorPanel_.GetSize().x;
        constexpr float gap = 8.0f;
        const float threeButtonWidth = (width - 40.0f) / 3.0f;
        const float twoButtonWidth = (width - 32.0f) / 2.0f;
        const float actionStart = environment
            ? std::max(1772.0f, inspectorPanel_.GetSize().y - 82.0f)
            : terrain
                ? 726.0f
                : light
                    ? 726.0f
                    : 230.0f;
        const float historyRow = environment
            ? actionStart
            : actionStart + 40.0f;
        const float saveRow = historyRow + 40.0f;

        focusButton_.SetPos(XMFLOAT2(12.0f, actionStart));
        duplicateButton_.SetPos(XMFLOAT2(
            12.0f + threeButtonWidth + gap,
            actionStart));
        deleteButton_.SetPos(XMFLOAT2(
            12.0f + (threeButtonWidth + gap) * 2.0f,
            actionStart));
        focusButton_.SetSize(XMFLOAT2(threeButtonWidth, 28.0f));
        duplicateButton_.SetSize(XMFLOAT2(threeButtonWidth, 28.0f));
        deleteButton_.SetSize(XMFLOAT2(threeButtonWidth, 28.0f));

        undoButton_.SetPos(XMFLOAT2(12.0f, historyRow));
        redoButton_.SetPos(XMFLOAT2(
            20.0f + twoButtonWidth,
            historyRow));
        undoButton_.SetSize(XMFLOAT2(twoButtonWidth, 28.0f));
        redoButton_.SetSize(XMFLOAT2(twoButtonWidth, 28.0f));

        saveButton_.SetPos(XMFLOAT2(12.0f, saveRow));
        saveAsButton_.SetPos(XMFLOAT2(
            12.0f + threeButtonWidth + gap,
            saveRow));
        reopenButton_.SetPos(XMFLOAT2(
            12.0f + (threeButtonWidth + gap) * 2.0f,
            saveRow));
        saveButton_.SetSize(XMFLOAT2(threeButtonWidth, 28.0f));
        saveAsButton_.SetSize(XMFLOAT2(threeButtonWidth, 28.0f));
        reopenButton_.SetSize(XMFLOAT2(threeButtonWidth, 28.0f));
    }

    void StudioRenderPath::RefreshInspector()
    {
        const bool hasSession = session_ != nullptr;
        const auto selectedEntity = hasSession
            ? session_->Selection().SelectedEntity()
            : wi::ecs::INVALID_ENTITY;
        const auto terrainWorkspaceEntity =
            hasSession && terrainWorkspaceActive_ &&
            session_->Scenes().GetScene().terrains.GetCount() > 0
            ? session_->Scenes().GetScene().terrains.GetEntity(0)
            : wi::ecs::INVALID_ENTITY;
        const auto entity = environmentWorkspaceActive_
            ? EditableWeatherEntity()
            : terrainWorkspaceActive_
                ? terrainWorkspaceEntity
                : selectedEntity;
        auto* transform = hasSession
            ? session_->Scenes().GetScene().transforms.GetComponent(
                environmentWorkspaceActive_ || terrainWorkspaceActive_
                    ? wi::ecs::INVALID_ENTITY
                    : selectedEntity)
            : nullptr;
        auto* weather = hasSession
            ? session_->Scenes().GetScene().weathers.GetComponent(entity)
            : nullptr;
        auto* terrain = hasSession && !environmentWorkspaceActive_
            ? session_->Scenes().GetScene().terrains.GetComponent(entity)
            : nullptr;
        auto* light = hasSession && !environmentWorkspaceActive_ &&
            !terrainWorkspaceActive_
            ? session_->Scenes().GetScene().lights.GetComponent(entity)
            : nullptr;
        SyncSelectionOutline();

        const bool hasTransform = transform != nullptr;
        const bool hasWeather = weather != nullptr;
        const bool hasTerrain = terrain != nullptr;
        const bool hasLight = light != nullptr;
        if (hasSession && selectedEntity != wi::ecs::INVALID_ENTITY &&
            !environmentWorkspaceActive_ && !terrainWorkspaceActive_)
        {
            const auto* selectedName =
                session_->Scenes().GetScene().names.GetComponent(
                    selectedEntity);
            studioChrome_.SetSelectionName(
                selectedName != nullptr ? selectedName->name : std::string{});
        }
        else
        {
            studioChrome_.SetSelectionName({});
        }
        LayoutInspectorActions(hasWeather, hasTerrain, hasLight);
        const auto setTransformVisible = [this, hasWeather, hasTerrain](wi::gui::Widget& widget)
        {
            widget.SetVisible(!environmentWorkspaceActive_ && !terrainWorkspaceActive_ && !hasWeather && !hasTerrain);
        };
        setTransformVisible(positionLabel_);
        setTransformVisible(rotationLabel_);
        setTransformVisible(scaleLabel_);
        setTransformVisible(translationX_);
        setTransformVisible(translationY_);
        setTransformVisible(translationZ_);
        setTransformVisible(rotationX_);
        setTransformVisible(rotationY_);
        setTransformVisible(rotationZ_);
        setTransformVisible(scaleX_);
        setTransformVisible(scaleY_);
        setTransformVisible(scaleZ_);

        const auto setLightVisible = [hasLight](wi::gui::Widget& widget)
        {
            widget.SetVisible(hasLight);
        };
        setLightVisible(lightLabel_);
        setLightVisible(lightType_);
        setLightVisible(lightColorRed_);
        setLightVisible(lightColorGreen_);
        setLightVisible(lightColorBlue_);
        setLightVisible(lightIntensity_);
        setLightVisible(lightRange_);
        setLightVisible(lightOuterCone_);
        setLightVisible(lightInnerCone_);
        setLightVisible(lightRadius_);
        setLightVisible(lightLength_);
        setLightVisible(lightHeight_);
        setLightVisible(lightCastShadow_);
        setLightVisible(lightVolumetrics_);
        setLightVisible(lightVolumetricBoost_);

        const auto setEnvironmentVisible =
            [hasWeather](wi::gui::Widget& widget)
        {
            widget.SetVisible(hasWeather);
        };
        setEnvironmentVisible(environmentSkyLabel_);
        setEnvironmentVisible(environmentPreset_);
        setEnvironmentVisible(skyMode_);
        setEnvironmentVisible(aerialPerspective_);
        setEnvironmentVisible(skyExposure_);
        setEnvironmentVisible(ambientIntensity_);
        setEnvironmentVisible(environmentFogLabel_);
        setEnvironmentVisible(fogStart_);
        setEnvironmentVisible(fogDensity_);
        setEnvironmentVisible(heightFog_);
        setEnvironmentVisible(fogHeightStart_);
        setEnvironmentVisible(fogHeightEnd_);
        setEnvironmentVisible(environmentCloudLabel_);
        setEnvironmentVisible(cloudCoverage_);
        setEnvironmentVisible(cloudStartHeight_);
        setEnvironmentVisible(cloudThickness_);
        setEnvironmentVisible(cloudsCastShadow_);
        setEnvironmentVisible(precipitationLabel_);
        setEnvironmentVisible(precipitationMode_);
        setEnvironmentVisible(precipitationIntensity_);
        setEnvironmentVisible(precipitationFallSpeed_);
        setEnvironmentVisible(precipitationParticleScale_);
        setEnvironmentVisible(precipitationWindAzimuth_);
        setEnvironmentVisible(precipitationWindSpeed_);
        setEnvironmentVisible(precipitationTurbulence_);
        setEnvironmentVisible(sunLabel_);
        setEnvironmentVisible(sunPreset_);
        setEnvironmentVisible(sunTime_);
        setEnvironmentVisible(sunAzimuth_);
        setEnvironmentVisible(sunElevation_);
        setEnvironmentVisible(sunPreviewSpeed_);
        setEnvironmentVisible(sunPlayButton_);
        setEnvironmentVisible(sunPauseButton_);
        setEnvironmentVisible(oceanLabel_);
        setEnvironmentVisible(oceanEnabled_);
        setEnvironmentVisible(oceanPreset_);
        setEnvironmentVisible(oceanResolution_);
        setEnvironmentVisible(oceanWaterHeight_);
        setEnvironmentVisible(oceanPatchLength_);
        setEnvironmentVisible(oceanWaveAmplitude_);
        setEnvironmentVisible(oceanChoppyScale_);
        setEnvironmentVisible(oceanTimeScale_);
        setEnvironmentVisible(oceanWindAzimuth_);
        setEnvironmentVisible(oceanWindSpeed_);
        setEnvironmentVisible(oceanWindDependency_);
        setEnvironmentVisible(oceanSurfaceDetail_);
        setEnvironmentVisible(oceanDisplacementTolerance_);
        setEnvironmentVisible(oceanWaterRed_);
        setEnvironmentVisible(oceanWaterGreen_);
        setEnvironmentVisible(oceanWaterBlue_);
        setEnvironmentVisible(oceanWaterOpacity_);
        setEnvironmentVisible(oceanExtinctionRed_);
        setEnvironmentVisible(oceanExtinctionGreen_);
        setEnvironmentVisible(oceanExtinctionBlue_);

        const auto setTerrainVisible = [this, hasTerrain](wi::gui::Widget& widget)
        {
            widget.SetVisible(terrainWorkspaceActive_ && hasTerrain);
        };
        terrainLabel_.SetVisible(
            terrainWorkspaceActive_);
        createTerrainButton_.SetVisible(
            hasSession && terrainWorkspaceActive_ && !hasTerrain);
        setTerrainVisible(terrainVisibleRadius_);
        setTerrainVisible(terrainChunkScale_);
        setTerrainVisible(terrainMinimumHeight_);
        setTerrainVisible(terrainMaximumHeight_);
        setTerrainVisible(terrainLowAltitudeBlend_);
        setTerrainVisible(terrainBaseBlend_);
        setTerrainVisible(terrainSlopeBlend_);
        setTerrainVisible(terrainLodBias_);
        setTerrainVisible(terrainMaterialLabel_);
        setTerrainVisible(terrainMaterialPreset_);
        setTerrainVisible(terrainTextureScale_);
        setTerrainVisible(terrainApplyDefaultGrassButton_);
        setTerrainVisible(terrainReloadMaterialButton_);
        setTerrainVisible(terrainSculptLabel_);
        setTerrainVisible(terrainSculptMode_);
        setTerrainVisible(terrainBrushRadius_);
        setTerrainVisible(terrainBrushStrength_);
        setTerrainVisible(terrainBrushFalloff_);
        setTerrainVisible(terrainBrushReadout_);
        setTerrainVisible(terrainStrokeDiagnostic_);

        translationX_.SetEnabled(hasTransform);
        translationY_.SetEnabled(hasTransform);
        translationZ_.SetEnabled(hasTransform);
        rotationX_.SetEnabled(hasTransform);
        rotationY_.SetEnabled(hasTransform);
        rotationZ_.SetEnabled(hasTransform);
        scaleX_.SetEnabled(hasTransform);
        scaleY_.SetEnabled(hasTransform);
        scaleZ_.SetEnabled(hasTransform);
        focusButton_.SetEnabled(hasTransform);
        duplicateButton_.SetEnabled(hasTransform);
        deleteButton_.SetEnabled(hasTransform);
        focusButton_.SetVisible(!hasWeather);
        duplicateButton_.SetVisible(!hasWeather);
        deleteButton_.SetVisible(!hasWeather);
        duplicateButton_.SetEnabled(hasTransform && !hasTerrain);
        undoButton_.SetEnabled(hasSession && session_->Commands().CanUndo());
        redoButton_.SetEnabled(hasSession && session_->Commands().CanRedo());
        saveButton_.SetEnabled(
            hasSession && !session_->Scenes().CurrentPath().empty());
        saveAsButton_.SetEnabled(hasSession);
        reopenButton_.SetEnabled(
            hasSession && !session_->Scenes().CurrentPath().empty());

        if (hasWeather)
        {
            const auto* name =
                session_->Scenes().GetScene().names.GetComponent(entity);
            inspectorLabel_.SetText(
                environmentWorkspaceActive_
                    ? "ENVIRONMENT // SCENE WEATHER"
                    : "ENVIRONMENT // " +
                        (name != nullptr && !name->name.empty()
                            ? name->name
                            : "ENTITY " + std::to_string(entity)));

            const auto state = bridge::CaptureWeather(*weather);
            environmentPreset_.SetSelectedWithoutCallback(0);
            skyMode_.SetSelectedByUserdataWithoutCallback(
                static_cast<std::uint64_t>(state.skyMode));
            aerialPerspective_.SetCheck(state.aerialPerspective);
            skyExposure_.SetValue(state.skyExposure);
            ambientIntensity_.SetValue(state.ambientIntensity);
            fogStart_.SetValue(state.fogStart);
            fogDensity_.SetValue(state.fogDensity);
            heightFog_.SetCheck(state.heightFog);
            fogHeightStart_.SetValue(state.fogHeightStart);
            fogHeightEnd_.SetValue(state.fogHeightEnd);
            cloudCoverage_.SetValue(state.cloudCoverage);
            cloudStartHeight_.SetValue(state.cloudStartHeight);
            cloudThickness_.SetValue(state.cloudThickness);
            cloudsCastShadow_.SetCheck(state.cloudsCastShadow);

            const auto precipitation =
                bridge::CapturePrecipitation(*weather);
            precipitationMode_.SetSelectedByUserdataWithoutCallback(
                static_cast<std::uint64_t>(precipitation.mode));
            precipitationIntensity_.SetValue(precipitation.intensity);
            precipitationFallSpeed_.SetValue(precipitation.fallSpeed);
            precipitationParticleScale_.SetValue(
                precipitation.particleScale);
            precipitationWindAzimuth_.SetValue(
                precipitation.windAzimuthDegrees);
            precipitationWindSpeed_.SetValue(precipitation.windSpeed);
            precipitationTurbulence_.SetValue(precipitation.turbulence);

            const auto sun = bridge::CaptureSun(
                session_->Scenes().GetScene(),
                entity);
            sunPreset_.SetSelectedWithoutCallback(0);
            sunTime_.SetValue(sun.timeHours);
            sunAzimuth_.SetValue(sun.azimuthDegrees);
            sunElevation_.SetValue(sun.elevationDegrees);
            sunPreviewSpeed_.SetValue(sunPreviewSpeedHoursPerSecond_);
            sunPlayButton_.SetEnabled(!sunPreviewPlaying_);
            sunPauseButton_.SetEnabled(sunPreviewPlaying_);

            const auto ocean = bridge::CaptureOcean(*weather);
            oceanEnabled_.SetCheck(ocean.enabled);
            oceanPreset_.SetSelectedWithoutCallback(0);
            oceanResolution_.SetSelectedByUserdataWithoutCallback(
                static_cast<std::uint64_t>(
                    ocean.displacementMapDimension));
            oceanWaterHeight_.SetValue(ocean.waterHeight);
            oceanPatchLength_.SetValue(ocean.patchLength);
            oceanWaveAmplitude_.SetValue(ocean.waveAmplitude);
            oceanChoppyScale_.SetValue(ocean.choppyScale);
            oceanTimeScale_.SetValue(ocean.timeScale);
            oceanWindAzimuth_.SetValue(ocean.windAzimuthDegrees);
            oceanWindSpeed_.SetValue(ocean.windSpeed);
            oceanWindDependency_.SetValue(ocean.windDependency);
            oceanSurfaceDetail_.SetValue(
                static_cast<float>(ocean.surfaceDetail));
            oceanDisplacementTolerance_.SetValue(
                ocean.surfaceDisplacementTolerance);
            oceanWaterRed_.SetValue(ocean.waterColor.x);
            oceanWaterGreen_.SetValue(ocean.waterColor.y);
            oceanWaterBlue_.SetValue(ocean.waterColor.z);
            oceanWaterOpacity_.SetValue(ocean.waterColor.w);
            oceanExtinctionRed_.SetValue(ocean.extinctionColor.x);
            oceanExtinctionGreen_.SetValue(ocean.extinctionColor.y);
            oceanExtinctionBlue_.SetValue(ocean.extinctionColor.z);

            oceanResolution_.SetEnabled(ocean.enabled);
            oceanWaterHeight_.SetEnabled(ocean.enabled);
            oceanPatchLength_.SetEnabled(ocean.enabled);
            oceanWaveAmplitude_.SetEnabled(ocean.enabled);
            oceanChoppyScale_.SetEnabled(ocean.enabled);
            oceanTimeScale_.SetEnabled(ocean.enabled);
            oceanWindAzimuth_.SetEnabled(ocean.enabled);
            oceanWindSpeed_.SetEnabled(ocean.enabled);
            oceanWindDependency_.SetEnabled(ocean.enabled);
            oceanSurfaceDetail_.SetEnabled(ocean.enabled);
            oceanDisplacementTolerance_.SetEnabled(ocean.enabled);
            oceanWaterRed_.SetEnabled(ocean.enabled);
            oceanWaterGreen_.SetEnabled(ocean.enabled);
            oceanWaterBlue_.SetEnabled(ocean.enabled);
            oceanWaterOpacity_.SetEnabled(ocean.enabled);
            oceanExtinctionRed_.SetEnabled(ocean.enabled);
            oceanExtinctionGreen_.SetEnabled(ocean.enabled);
            oceanExtinctionBlue_.SetEnabled(ocean.enabled);

            const bool physicalSky =
                state.skyMode != bridge::WeatherState::SkyMode::Skybox;
            const bool volumetricClouds =
                state.skyMode ==
                bridge::WeatherState::SkyMode::RealisticWithClouds;
            aerialPerspective_.SetEnabled(physicalSky);
            cloudCoverage_.SetEnabled(volumetricClouds);
            cloudStartHeight_.SetEnabled(volumetricClouds);
            cloudThickness_.SetEnabled(volumetricClouds);
            cloudsCastShadow_.SetEnabled(volumetricClouds);
            fogHeightStart_.SetEnabled(state.heightFog);
            fogHeightEnd_.SetEnabled(state.heightFog);
            const bool precipitationEnabled = precipitation.mode !=
                bridge::PrecipitationMode::None;
            precipitationIntensity_.SetEnabled(precipitationEnabled);
            precipitationFallSpeed_.SetEnabled(precipitationEnabled);
            precipitationParticleScale_.SetEnabled(precipitationEnabled);
            precipitationWindAzimuth_.SetEnabled(precipitationEnabled);
            precipitationWindSpeed_.SetEnabled(precipitationEnabled);
            precipitationTurbulence_.SetEnabled(precipitationEnabled);
            SyncGizmoSelection();
            return;
        }

        if (hasTerrain)
        {
            const auto* name =
                session_->Scenes().GetScene().names.GetComponent(entity);
            inspectorLabel_.SetText(
                "TERRAIN // " +
                (name != nullptr && !name->name.empty()
                    ? name->name
                    : "ENTITY " + std::to_string(entity)));
            const auto state = bridge::CaptureTerrain(*terrain);
            terrainVisibleRadius_.SetValue(
                static_cast<float>(state.visibleChunkRadius));
            terrainChunkScale_.SetValue(state.chunkScale);
            terrainMinimumHeight_.SetValue(state.minimumHeight);
            terrainMaximumHeight_.SetValue(state.maximumHeight);
            terrainLowAltitudeBlend_.SetValue(state.lowAltitudeBlend);
            terrainBaseBlend_.SetValue(state.baseBlend);
            terrainSlopeBlend_.SetValue(state.slopeBlend);
            terrainLodBias_.SetValue(state.lodBias);
            const float textureScale = bridge::CaptureTerrainTextureScale(
                session_->Scenes().GetScene(),
                *terrain);
            terrainTextureScale_.SetValue(textureScale);
            int materialPreset = 0;
            for (int presetIndex = 0; presetIndex < 3; ++presetIndex)
            {
                const auto preset = static_cast<bridge::TerrainMaterialPreset>(
                    presetIndex);
                if (std::abs(
                        textureScale -
                        bridge::MakeTerrainMaterialPreset(preset)) < 0.01f)
                {
                    materialPreset = presetIndex + 1;
                    break;
                }
            }
            terrainMaterialPreset_.SetSelectedWithoutCallback(materialPreset);
            std::ostringstream brush;
            brush << "BRUSH // SIZE " << std::fixed << std::setprecision(0)
                  << terrainBrushRadiusValue_ << " // STRENGTH "
                  << std::setprecision(2) << terrainBrushStrengthValue_;
            terrainBrushReadout_.SetText(brush.str());
            SyncGizmoSelection();
            return;
        }

        if (hasLight)
        {
            const auto* name =
                session_->Scenes().GetScene().names.GetComponent(entity);
            inspectorLabel_.SetText(
                "LIGHT // " +
                (name != nullptr && !name->name.empty()
                    ? name->name
                    : "ENTITY " + std::to_string(entity)));
            const auto state = bridge::CaptureLight(*light);
            lightType_.SetSelectedByUserdataWithoutCallback(
                static_cast<std::uint64_t>(state.type));
            lightColorRed_.SetValue(state.color.x);
            lightColorGreen_.SetValue(state.color.y);
            lightColorBlue_.SetValue(state.color.z);
            lightIntensity_.SetValue(state.intensity);
            lightRange_.SetValue(state.range);
            lightOuterCone_.SetValue(state.outerConeDegrees);
            lightInnerCone_.SetValue(state.innerConeDegrees);
            lightRadius_.SetValue(state.radius);
            lightLength_.SetValue(state.length);
            lightHeight_.SetValue(state.height);
            lightCastShadow_.SetCheck(state.castShadow);
            lightVolumetrics_.SetCheck(state.volumetrics);
            lightVolumetricBoost_.SetValue(state.volumetricBoost);

            const bool directional = state.type ==
                wi::scene::LightComponent::DIRECTIONAL;
            const bool point = state.type ==
                wi::scene::LightComponent::POINT;
            const bool spot = state.type ==
                wi::scene::LightComponent::SPOT;
            const bool rectangle = state.type ==
                wi::scene::LightComponent::RECTANGLE;
            lightRange_.SetEnabled(!directional);
            lightOuterCone_.SetVisible(spot);
            lightInnerCone_.SetVisible(spot);
            lightRadius_.SetVisible(directional || point || spot);
            lightLength_.SetVisible(point || rectangle);
            lightHeight_.SetVisible(rectangle);
            lightVolumetricBoost_.SetEnabled(state.volumetrics);
        }

        if (!hasTransform)
        {
            if (!hasLight)
            {
                inspectorLabel_.SetText(terrainWorkspaceActive_
                    ? "TERRAIN // CREATE OR EDIT LANDSCAPE"
                    : "TRANSFORM // SELECT AN ENTITY");
            }
            translationX_.SetValue(0.0f);
            translationY_.SetValue(0.0f);
            translationZ_.SetValue(0.0f);
            rotationX_.SetValue(0.0f);
            rotationY_.SetValue(0.0f);
            rotationZ_.SetValue(0.0f);
            scaleX_.SetValue(1.0f);
            scaleY_.SetValue(1.0f);
            scaleZ_.SetValue(1.0f);
            return;
        }

        const auto* name =
            session_->Scenes().GetScene().names.GetComponent(entity);
        if (!hasLight)
        {
            inspectorLabel_.SetText(
                "TRANSFORM // " +
                (name != nullptr && !name->name.empty()
                    ? name->name
                    : "ENTITY " + std::to_string(entity)));
        }
        translationX_.SetValue(transform->translation_local.x);
        translationY_.SetValue(transform->translation_local.y);
        translationZ_.SetValue(transform->translation_local.z);
        const auto rotation =
            wi::math::QuaternionToRollPitchYaw(transform->rotation_local);
        rotationX_.SetValue(rotation.x / XM_PI * 180.0f);
        rotationY_.SetValue(rotation.y / XM_PI * 180.0f);
        rotationZ_.SetValue(rotation.z / XM_PI * 180.0f);
        scaleX_.SetValue(transform->scale_local.x);
        scaleY_.SetValue(transform->scale_local.y);
        scaleZ_.SetValue(transform->scale_local.z);
        SyncGizmoSelection();
    }

    void StudioRenderPath::HandleEditorShortcuts()
    {
        if (projectHubVisible_ ||
            flyCameraActive_ ||
            GetGUI().IsTyping() ||
            pendingAction_ != EditorAction::None ||
            gizmoDragActive_ ||
            weatherSliderActive_ ||
            precipitationSliderActive_ ||
            sunSliderActive_ ||
            oceanSliderActive_ ||
            lightSliderActive_ ||
            lightPlacementActive_ ||
            terrainSliderActive_ ||
            terrainTextureScaleActive_ ||
            terrainStrokeActive_)
        {
            return;
        }

        const auto key = [](const char value)
        {
            return static_cast<wi::input::BUTTON>(value);
        };
        const bool control =
            wi::input::Down(wi::input::KEYBOARD_BUTTON_LCONTROL) ||
            wi::input::Down(wi::input::KEYBOARD_BUTTON_RCONTROL);
        const bool shift =
            wi::input::Down(wi::input::KEYBOARD_BUTTON_LSHIFT) ||
            wi::input::Down(wi::input::KEYBOARD_BUTTON_RSHIFT);

        if (control && wi::input::Press(key('Z')))
        {
            pendingAction_ = EditorAction::Undo;
        }
        else if (control && wi::input::Press(key('Y')))
        {
            pendingAction_ = EditorAction::Redo;
        }
        else if (control && wi::input::Press(key('D')))
        {
            pendingAction_ = EditorAction::DuplicateSelection;
        }
        else if (control && wi::input::Press(key('S')))
        {
            pendingAction_ = shift
                ? EditorAction::SaveSceneAs
                : EditorAction::SaveScene;
        }
        else if (wi::input::Press(wi::input::KEYBOARD_BUTTON_DELETE))
        {
            pendingAction_ = EditorAction::DeleteSelection;
        }
        else if (wi::input::Press(key('F')))
        {
            pendingAction_ = EditorAction::FocusSelection;
        }
        else if (wi::input::Press(key('W')))
        {
            pendingAction_ = EditorAction::TranslateTool;
        }
        else if (wi::input::Press(key('E')))
        {
            pendingAction_ = EditorAction::RotateTool;
        }
        else if (wi::input::Press(key('R')))
        {
            pendingAction_ = EditorAction::ScaleTool;
        }
        else if (wi::input::Press(key('G')))
        {
            pendingAction_ = EditorAction::ToggleGrid;
        }
    }

    bool StudioRenderPath::IsSelectedEntityValid() const
    {
        return session_ != nullptr &&
            session_->Selection().HasSelection() &&
            session_->Scenes().ContainsEntity(
                session_->Selection().SelectedEntity());
    }

    void StudioRenderPath::ProcessPendingAction()
    {
        if (session_ == nullptr)
        {
            pendingAction_ = EditorAction::None;
            return;
        }

        const EditorAction action = pendingAction_;
        pendingAction_ = EditorAction::None;
        if (lightPlacementActive_ && action != EditorAction::CreateLight)
        {
            CancelLightPlacement();
        }
        switch (action)
        {
        case EditorAction::Undo:
        case EditorAction::Redo:
        {
            StopSunPreview(true);
            ClearSelectionOutline();
            const bool changed = action == EditorAction::Undo
                ? session_->Commands().Undo()
                : session_->Commands().Redo();
            if (changed)
            {
                if (session_->Selection().HasSelection() &&
                    !IsSelectedEntityValid())
                {
                    session_->Selection().Clear();
                }
                RefreshHierarchy();
                RefreshInspector();
                RefreshStatus();
            }
            else
            {
                SyncSelectionOutline();
            }
            break;
        }
        case EditorAction::FocusSelection:
            FocusSelection();
            break;
        case EditorAction::DuplicateSelection:
            DuplicateSelection();
            break;
        case EditorAction::DeleteSelection:
            DeleteSelection();
            break;
        case EditorAction::CreateLight:
            CreateLight(pendingLightType_);
            break;
        case EditorAction::OpenScene:
            OpenScene();
            break;
        case EditorAction::SaveScene:
            SaveScene();
            break;
        case EditorAction::SaveSceneAs:
            SaveSceneAs();
            break;
        case EditorAction::ReopenScene:
            ReopenScene();
            break;
        case EditorAction::ProjectHub:
            ReturnToProjectHub();
            break;
        case EditorAction::SelectTool:
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            SetTransformTool(TransformTool::Select);
            break;
        case EditorAction::TranslateTool:
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            SetTransformTool(TransformTool::Translate);
            break;
        case EditorAction::RotateTool:
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            SetTransformTool(TransformTool::Rotate);
            break;
        case EditorAction::ScaleTool:
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            SetTransformTool(TransformTool::Scale);
            break;
        case EditorAction::ToggleGrid:
            SetGridVisible(!gridVisible_);
            break;
        case EditorAction::OpenEnvironmentWorkspace:
            SetEnvironmentWorkspaceActive(true);
            break;
        case EditorAction::OpenTerrainWorkspace:
            SetTerrainWorkspaceActive(true);
            break;
        case EditorAction::OpenSceneWorkspace:
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            break;
        case EditorAction::StartTestLevel:
            StartTestLevel();
            break;
        case EditorAction::StopTestLevel:
            StopTestLevel();
            break;
        case EditorAction::StartSunPreview:
            StartSunPreview();
            break;
        case EditorAction::PauseSunPreview:
            StopSunPreview(true);
            break;
        case EditorAction::SetOceanEnabled:
            ApplyOceanEnabled(pendingOceanEnabled_);
            break;
        case EditorAction::SetOceanResolution:
            ApplyOceanResolution(pendingOceanResolution_);
            break;
        case EditorAction::ApplyOceanPreset:
            ApplyOceanPreset(pendingOceanPreset_);
            break;
        case EditorAction::CreateTerrain:
            CreateTerrain();
            break;
        case EditorAction::ApplyTerrainMaterialPreset:
            ApplyTerrainMaterialPreset(pendingTerrainMaterialPreset_);
            break;
        case EditorAction::ApplyDefaultGrass:
            ApplyDefaultGrass();
            break;
        case EditorAction::ReloadTerrainMaterial:
            ReloadTerrainMaterial();
            break;
        case EditorAction::ValidateModelImport:
            ValidateModelImport();
            break;
        case EditorAction::ImportModel:
            ImportModel();
            break;
        case EditorAction::ApplyImportScale:
            ApplyImportScaleMode(pendingImportScaleMode_);
            break;
        case EditorAction::DismissImportScale:
            DismissImportScalePanel();
            break;
        case EditorAction::None:
        default:
            break;
        }
    }

    void StudioRenderPath::SetTransformTool(const TransformTool tool)
    {
        gizmo_.isTranslator = tool == TransformTool::Translate;
        gizmo_.isRotator = tool == TransformTool::Rotate;
        gizmo_.isScalator = tool == TransformTool::Scale;

        translateToolButton_.SetColor(
            gizmo_.isTranslator ? HologramSelected : HologramIdle,
            wi::gui::IDLE);
        rotateToolButton_.SetColor(
            gizmo_.isRotator ? HologramSelected : HologramIdle,
            wi::gui::IDLE);
        scaleToolButton_.SetColor(
            gizmo_.isScalator ? HologramSelected : HologramIdle,
            wi::gui::IDLE);
        studioChrome_.SetActiveTool(
            tool == TransformTool::Select
                ? 0
                : tool == TransformTool::Translate
                ? 1
                : tool == TransformTool::Rotate
                    ? 2
                    : 3);
        SyncGizmoSelection();
        RefreshStatus();
    }

    wi::ecs::Entity StudioRenderPath::EditableWeatherEntity() const noexcept
    {
        if (session_ == nullptr)
        {
            return wi::ecs::INVALID_ENTITY;
        }
        if (environmentWorkspaceActive_)
        {
            return session_->Scenes().WeatherEntity();
        }
        const auto selected = session_->Selection().SelectedEntity();
        return session_->Scenes().GetScene().weathers.Contains(selected)
            ? selected
            : wi::ecs::INVALID_ENTITY;
    }

    void StudioRenderPath::SetEnvironmentWorkspaceActive(const bool active)
    {
        if (!active && sunPreviewPlaying_)
        {
            StopSunPreview(true);
        }
        environmentWorkspaceActive_ = active && session_ != nullptr &&
            session_->Scenes().WeatherEntity() != wi::ecs::INVALID_ENTITY;
        if (environmentWorkspaceActive_)
        {
            terrainWorkspaceActive_ = false;
        }
        studioChrome_.SetEnvironmentWorkspaceActive(
            environmentWorkspaceActive_);
        studioChrome_.SetTerrainWorkspaceActive(terrainWorkspaceActive_);
        ClearSelectionOutline();
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::SetTerrainWorkspaceActive(const bool active)
    {
        if (sunPreviewPlaying_)
        {
            StopSunPreview(true);
        }
        terrainWorkspaceActive_ = active && session_ != nullptr;
        if (terrainWorkspaceActive_)
        {
            environmentWorkspaceActive_ = false;
        }
        studioChrome_.SetEnvironmentWorkspaceActive(environmentWorkspaceActive_);
        studioChrome_.SetTerrainWorkspaceActive(terrainWorkspaceActive_);
        if (terrainWorkspaceActive_ &&
            session_->Scenes().GetScene().terrains.GetCount() > 0)
        {
            session_->Selection().Select(
                session_->Scenes().GetScene().terrains.GetEntity(0));
        }
        ClearSelectionOutline();
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::FocusSelection()
    {
        if (!IsSelectedEntityValid())
        {
            return;
        }

        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        scene.Update(0.0f);
        const auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
        {
            return;
        }

        XMVECTOR center = transform->GetPositionV();
        float distance = 5.0f;
        if (scene.objects.Contains(entity))
        {
            const auto index = scene.objects.GetIndex(entity);
            if (index < scene.aabb_objects.size())
            {
                const auto& bounds = scene.aabb_objects[index];
                const XMFLOAT3 boundsCenter = bounds.getCenter();
                center = XMLoadFloat3(&boundsCenter);
                distance = std::max(2.5f, bounds.getRadius() * 2.5f);
            }
        }

        const XMVECTOR forward = XMVector3Normalize(camera->GetAt());
        const XMVECTOR eye = center - forward * distance;
        const XMMATRIX view = XMMatrixLookAtLH(
            eye,
            center,
            camera->GetUp());
        editorCameraTransform_.ClearTransform();
        editorCameraTransform_.MatrixTransform(
            XMMatrixInverse(nullptr, view));
        editorCameraTransform_.UpdateTransform();
        camera->TransformCamera(editorCameraTransform_);
        camera->UpdateCamera();
    }

    void StudioRenderPath::DuplicateSelection()
    {
        if (!IsSelectedEntityValid())
        {
            return;
        }

        const auto entity = session_->Selection().SelectedEntity();
        ClearSelectionOutline();
        auto command = std::make_unique<bridge::DuplicateEntityCommand>(
            session_->Scenes().GetScene(),
            entity);
        auto* duplicateCommand = command.get();
        if (!session_->Commands().Execute(std::move(command)))
        {
            SyncSelectionOutline();
            return;
        }

        session_->Selection().Select(
            duplicateCommand->DuplicatedEntity());
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::DeleteSelection()
    {
        if (!IsSelectedEntityValid())
        {
            return;
        }

        const auto entity = session_->Selection().SelectedEntity();
        ClearSelectionOutline();
        if (!session_->Commands().Execute(
                std::make_unique<bridge::DeleteEntityCommand>(
                    session_->Scenes().GetScene(),
                    entity)))
        {
            SyncSelectionOutline();
            return;
        }

        session_->Selection().Clear();
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::CreateLight(
        const wi::scene::LightComponent::LightType type)
    {
        if (session_ == nullptr || camera == nullptr)
        {
            return;
        }

        StopSunPreview(true);
        SetEnvironmentWorkspaceActive(false);
        SetTerrainWorkspaceActive(false);

        lightPlacementActive_ = false;
        if (type != wi::scene::LightComponent::DIRECTIONAL)
        {
            lightPlacementType_ = type;
            lightPlacementActive_ = true;
            ClearSelectionOutline();
            RefreshStatus();
            return;
        }

        XMFLOAT3 position = camera->Eye;
        position.x += camera->At.x * 5.0f;
        position.y += camera->At.y * 5.0f;
        position.z += camera->At.z * 5.0f;

        PlaceLight(type, position);
    }

    void StudioRenderPath::PlaceLight(
        const wi::scene::LightComponent::LightType type,
        const XMFLOAT3& position,
        const XMFLOAT4& rotation)
    {
        if (session_ == nullptr)
        {
            return;
        }

        ClearSelectionOutline();
        auto command = std::make_unique<bridge::CreateLightCommand>(
            session_->Scenes().GetScene(),
            type,
            position,
            rotation);
        auto* createCommand = command.get();
        if (!session_->Commands().Execute(std::move(command)))
        {
            SyncSelectionOutline();
            return;
        }

        lightPlacementActive_ = false;
        wi::input::SetCursor(wi::input::CURSOR_DEFAULT);
        session_->Selection().Select(createCommand->CreatedEntity());
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::CancelLightPlacement()
    {
        if (!lightPlacementActive_)
        {
            return;
        }
        lightPlacementActive_ = false;
        wi::input::SetCursor(wi::input::CURSOR_DEFAULT);
        RefreshStatus();
    }

    void StudioRenderPath::BeginCreatorAssetPlacement(
        const bridge::StableId& assetId,
        const std::string& label)
    {
        if (session_ == nullptr || !bridge::IsValidStableId(assetId))
            return;
        CancelLightPlacement();
        creatorAssetPlacementActive_ = true;
        creatorAssetPlacementId_ = assetId;
        creatorAssetPlacementLabel_ = label;
        creatorAssetDropPending_ = false;
        studioChrome_.SetActiveBottomTab(-1, true);
        studioChrome_.SetStatusText(
            "PLACE ASSET // CLICK A SURFACE // ESC OR RMB TO CANCEL");
    }

    void StudioRenderPath::DropCreatorAsset(
        const bridge::StableId& assetId,
        const std::string& label,
        const float screenX,
        const float screenY)
    {
        BeginCreatorAssetPlacement(assetId, label);
        if (!creatorAssetPlacementActive_)
            return;
        creatorAssetDropPoint_ = XMFLOAT2(screenX, screenY);
        creatorAssetDropPending_ = true;
        studioChrome_.SetStatusText("PLACE ASSET // DROPPING ON SURFACE");
    }

    void StudioRenderPath::CancelCreatorAssetPlacement()
    {
        if (!creatorAssetPlacementActive_)
            return;
        creatorAssetPlacementActive_ = false;
        creatorAssetPlacementId_.clear();
        creatorAssetPlacementLabel_.clear();
        creatorAssetDropPending_ = false;
        wi::input::SetCursor(wi::input::CURSOR_DEFAULT);
        studioChrome_.SetStatusText("PLACE ASSET // CANCELLED");
    }

    bool StudioRenderPath::HandleCreatorAssetPlacement(
        const XMFLOAT4& pointer)
    {
        if (!creatorAssetPlacementActive_ || session_ == nullptr)
            return false;

        XMFLOAT4 placementPointer = pointer;
        if (creatorAssetDropPending_)
        {
            placementPointer.x = creatorAssetDropPoint_.x;
            placementPointer.y = creatorAssetDropPoint_.y;
        }

        if (wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE) ||
            wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT))
        {
            CancelCreatorAssetPlacement();
            return true;
        }

        if (flyCameraActive_ ||
            (!creatorAssetDropPending_ && GetGUI().HasFocus()) ||
            !IsPointerOverViewport(placementPointer))
        {
            wi::input::SetCursor(wi::input::CURSOR_NOTALLOWED);
            if (creatorAssetDropPending_)
            {
                CancelCreatorAssetPlacement();
                studioChrome_.SetStatusText(
                    "PLACE ASSET // DROP INSIDE THE SCENE VIEWPORT");
            }
            return true;
        }

        auto& scene = session_->Scenes().GetScene();
        const auto ray = wi::renderer::GetPickRay(
            static_cast<long>(placementPointer.x),
            static_cast<long>(placementPointer.y),
            *this,
            *camera);
        const auto picked = wi::scene::Pick(
            ray,
            wi::enums::FILTER_OBJECT_ALL | wi::enums::FILTER_TERRAIN,
            ~0u,
            scene);
        XMFLOAT3 surfacePosition = picked.position;
        bool hasSurface = picked.entity != wi::ecs::INVALID_ENTITY;
        if (!hasSurface && std::abs(ray.direction.y) > 0.0001f)
        {
            const float distance = -ray.origin.y / ray.direction.y;
            if (distance >= ray.TMin && distance <= ray.TMax)
            {
                surfacePosition = XMFLOAT3(
                    ray.origin.x + ray.direction.x * distance,
                    0.0f,
                    ray.origin.z + ray.direction.z * distance);
                hasSurface = true;
            }
        }
        if (!hasSurface)
        {
            wi::input::SetCursor(wi::input::CURSOR_NOTALLOWED);
            if (creatorAssetDropPending_)
            {
                CancelCreatorAssetPlacement();
                studioChrome_.SetStatusText(
                    "PLACE ASSET // NO SURFACE UNDER DROP POINT");
            }
            return true;
        }

        wi::input::SetCursor(wi::input::CURSOR_CROSS);
        wi::renderer::DrawSphere(
            wi::primitive::Sphere(surfacePosition, 0.16f),
            XMFLOAT4(1.0f, 0.36f, 0.06f, 0.9f),
            false);
        if (!creatorAssetDropPending_ &&
            !wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
            return true;
        creatorAssetDropPending_ = false;

        const auto& project = session_->Projects().CurrentProject();
        bridge::CreatorAssetWorkflowService workflow;
        auto prepared = workflow.PrepareModelPlacement(
            project.rootPath,
            project.projectId,
            creatorAssetPlacementId_);
        if (!prepared.IsReady())
        {
            studioChrome_.SetStatusText(
                "PLACE ASSET // PREPARE FAILED // " +
                prepared.Result().error);
            return true;
        }

        const wi::scene::Scene* preparedScene = prepared.PeekScene();
        const float scale = bridge::ImportService::ResolveScaleFactor(
            bridge::ModelScaleMode::Automatic,
            *preparedScene);
        const bridge::ModelBounds bounds =
            bridge::ImportService::MeasureModelBounds(*preparedScene);
        XMFLOAT3 position = surfacePosition;
        if (bounds.valid)
        {
            // PlaceReusableModelCommand translates the prepared root to the
            // requested point. Offset its local minimum upward so the actual
            // bottom of the scaled asset rests on the picked surface.
            position.y = bridge::ImportService::ResolveGroundedPlacementY(
                surfacePosition.y,
                bounds,
                scale);
        }

        const bridge::StableId assetId = creatorAssetPlacementId_;
        auto command = std::make_unique<bridge::PlaceReusableModelCommand>(
            scene,
            prepared.ReleaseScene(),
            assetId,
            position,
            scale);
        auto* placed = command.get();
        if (!session_->Commands().Execute(std::move(command)))
        {
            studioChrome_.SetStatusText("PLACE ASSET // FAILED");
            return true;
        }

        const std::string label = creatorAssetPlacementLabel_;
        creatorAssetPlacementActive_ = false;
        creatorAssetPlacementId_.clear();
        creatorAssetPlacementLabel_.clear();
        creatorAssetDropPending_ = false;
        wi::input::SetCursor(wi::input::CURSOR_DEFAULT);
        session_->Selection().Select(placed->PlacedEntity());
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        studioChrome_.SetStatusText(
            "PLACE ASSET // " + label +
            " // SURFACE GROUNDED // STABLE RASSET INSTANCE");
        return true;
    }

    bool StudioRenderPath::HandleLightPlacement(const XMFLOAT4& pointer)
    {
        if (!lightPlacementActive_ || session_ == nullptr)
        {
            return false;
        }

        if (wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE) ||
            wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT))
        {
            CancelLightPlacement();
            return true;
        }

        if (flyCameraActive_ || GetGUI().HasFocus() ||
            !IsPointerOverViewport(pointer))
        {
            wi::input::SetCursor(wi::input::CURSOR_DEFAULT);
            return false;
        }

        const auto ray = wi::renderer::GetPickRay(
            static_cast<long>(pointer.x),
            static_cast<long>(pointer.y),
            *this,
            *camera);
        const auto picked = wi::scene::Pick(
            ray,
            wi::enums::FILTER_OBJECT_ALL,
            ~0u,
            session_->Scenes().GetScene());
        if (picked.entity == wi::ecs::INVALID_ENTITY)
        {
            wi::input::SetCursor(wi::input::CURSOR_NOTALLOWED);
            return true;
        }

        wi::input::SetCursor(wi::input::CURSOR_CROSS);
        wi::renderer::DrawSphere(
            wi::primitive::Sphere(picked.position, 0.18f),
            XMFLOAT4(0.20f, 0.92f, 1.0f, 0.90f),
            false);
        wi::renderer::RenderableLine normal;
        normal.start = picked.position;
        XMStoreFloat3(
            &normal.end,
            XMLoadFloat3(&picked.position) +
                XMLoadFloat3(&picked.normal) * 0.8f);
        normal.color_start = XMFLOAT4(0.20f, 0.92f, 1.0f, 0.95f);
        normal.color_end = XMFLOAT4(1.0f, 0.55f, 0.15f, 0.95f);
        wi::renderer::DrawLine(normal, false);

        if (!wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
        {
            return true;
        }

        XMFLOAT4 rotation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        if (lightPlacementType_ == wi::scene::LightComponent::SPOT)
        {
            rotation = RotationFromTo(XMFLOAT3(0, 1, 0), picked.normal);
        }
        else if (lightPlacementType_ == wi::scene::LightComponent::RECTANGLE)
        {
            rotation = RotationFromTo(XMFLOAT3(0, 0, -1), picked.normal);
        }
        PlaceLight(lightPlacementType_, picked.position, rotation);
        return true;
    }

    bool StudioRenderPath::ProjectEditorPoint(
        const XMFLOAT3& world,
        XMFLOAT2& screen) const noexcept
    {
        if (camera == nullptr)
        {
            return false;
        }
        const XMVECTOR clip = XMVector4Transform(
            XMVectorSet(world.x, world.y, world.z, 1.0f),
            camera->GetViewProjection());
        const float w = XMVectorGetW(clip);
        if (w <= 0.001f)
        {
            return false;
        }
        const XMVECTOR ndc = clip / w;
        const float z = XMVectorGetZ(ndc);
        if (z < 0.0f || z > 1.0f)
        {
            return false;
        }
        screen.x = (XMVectorGetX(ndc) * 0.5f + 0.5f) * GetLogicalWidth();
        screen.y = (-XMVectorGetY(ndc) * 0.5f + 0.5f) * GetLogicalHeight();
        return IsPointerOverViewport(XMFLOAT4(screen.x, screen.y, 0, 0));
    }

    bool StudioRenderPath::HandleLightSceneIcons(
        const XMFLOAT4& pointer)
    {
        if (session_ == nullptr || camera == nullptr || projectHubVisible_)
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
        const auto picked = wi::scene::Pick(
            pickRay,
            wi::enums::FILTER_OBJECT_ALL,
            ~0u,
            session_->Scenes().GetScene());
        const auto current = session_->Selection().SelectedEntity();
        if (picked.entity == current && !environmentWorkspaceActive_)
        {
            return false;
        }

        environmentWorkspaceActive_ = false;
        studioChrome_.SetEnvironmentWorkspaceActive(false);
        terrainWorkspaceActive_ = false;
        studioChrome_.SetTerrainWorkspaceActive(false);

        if (picked.entity == wi::ecs::INVALID_ENTITY ||
            !session_->Scenes().IsHierarchyVisible(picked.entity))
        {
            session_->Selection().Clear();
        }
        else
        {
            session_->Selection().Select(picked.entity);
        }

        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        return true;
    }

    void StudioRenderPath::RefreshProjectHub()
    {
        if (session_ == nullptr)
        {
            return;
        }

        const auto& projects = session_->Projects().RecentProjects();
        for (std::size_t index = 0; index < recentProjectButtons_.size(); ++index)
        {
            auto& button = recentProjectButtons_[index];
            const bool hasProject = index < projects.size();
            button.SetVisible(hasProject);
            if (!hasProject)
            {
                continue;
            }

            button.SetText(
                projects[index].name + "\n" +
                projects[index].descriptorPath);
            button.SetColor(
                static_cast<int>(index) == selectedRecentProject_
                    ? HologramSelected
                    : HologramIdle,
                wi::gui::IDLE);
        }

        if (selectedRecentProject_ < 0 ||
            static_cast<std::size_t>(selectedRecentProject_) >= projects.size())
        {
            selectedRecentProject_ = -1;
            selectedProjectLabel_.SetText(
                projects.empty()
                    ? "NO RECENT PROJECTS\n\nCreate a project to initialise "
                      "the Renegade workspace."
                    : "SELECT A PROJECT CARD\n\nThe selected project details "
                      "and launch control appear here.");
        }
        else
        {
            const auto& selected =
                projects[static_cast<std::size_t>(selectedRecentProject_)];
            selectedProjectLabel_.SetText(
                selected.name + "\n\n" +
                selected.descriptorPath +
                "\n\nFORMAT // RENEGADE PROJECT V1");
        }

        launchProjectButton_.SetEnabled(selectedRecentProject_ >= 0);
        continueProjectButton_.SetVisible(session_->Projects().HasProject());
        if (session_->Projects().HasProject())
        {
            continueProjectButton_.SetText(
                "RETURN TO " +
                session_->Projects().CurrentProject().name);
        }
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
        case TerrainField::VisibleChunkRadius:
            terrain.visibleChunkRadius = static_cast<int>(
                std::clamp(std::lround(value), 1l, 16l));
            break;
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

                        auto place = std::make_unique<bridge::PlaceImportedModelCommand>(
                            liveScene,
                            state->prepared.ReleaseScene(),
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
        creatorImportThumbnailCapture.SetText("CAPTURE THUMBNAIL");
        creatorImportThumbnailStatus.SetText("THUMBNAIL NOT CAPTURED");
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
            !session_->Projects().HasProject())
            return;

        const fs::path directory =
            fs::u8path(session_->Projects().CurrentProject().rootPath) /
            "Intermediate" / "Imports";
        std::error_code ec;
        fs::create_directories(directory, ec);
        if (ec)
        {
            creatorImportThumbnailStatus.SetText(
                "THUMBNAIL FAILED // CANNOT CREATE IMPORT CACHE");
            return;
        }

        const fs::path capturePath =
            directory / ".creator-asset-thumbnail.png";
        if (!wi::helper::saveTextureToFile(
                GetRenderResult3D(), capturePath.generic_u8string()))
        {
            creatorImportThumbnailStatus.SetText(
                "THUMBNAIL FAILED // RETAKE");
            return;
        }

        creatorModelImporter.thumbnailCapturePath =
            capturePath.generic_u8string();
        creatorImportThumbnailCapture.SetText("RETAKE THUMBNAIL");
        creatorImportThumbnailStatus.SetText(
            "THUMBNAIL CAPTURED // READY TO CONFIRM");
        importScaleApplyButton_.SetEnabled(true);
    }

    void StudioRenderPath::ApplyImportScaleMode(
        const bridge::ModelScaleMode)
    {
        if (session_ == nullptr || !creatorModelImporter.active || creatorModelImporter.committing)
            return;
        if (creatorModelImporter.thumbnailCapturePath.empty() ||
            !fs::exists(fs::u8path(creatorModelImporter.thumbnailCapturePath)))
        {
            creatorImportThumbnailStatus.SetText(
                "CAPTURE A THUMBNAIL BEFORE CONFIRMING");
            importScaleApplyButton_.SetEnabled(false);
            return;
        }

        auto& liveScene = session_->Scenes().GetScene();
        auto* previewTransform = liveScene.transforms.GetComponent(creatorModelImporter.previewRoot);
        if (previewTransform == nullptr)
        {
            DismissImportScalePanel();
            return;
        }

        const std::string sourcePath = creatorModelImporter.sourcePath;
        const auto cameraBefore = creatorModelImporter.cameraBefore;
        const auto materialOverrides = creatorModelImporter.materialOverrides;
        const auto animationRecipe = creatorModelImporter.animationRecipe;
        const std::string assetName = creatorModelImporter.assetName;
        const std::string destinationFolder = creatorModelImporter.destinationFolder;
        creatorModelImporter.committing = true;

        RestoreCreatorImportPreviewEnvironment();
        while (session_->Commands().UndoCount() > creatorModelImporter.undoBaseline)
        {
            if (!session_->Commands().Undo())
                break;
        }
        importScalePanel_.SetVisible(false);
        studioChrome_.SetVisible(true);
        inspectorPanel_.SetVisible(true);
        hierarchySearch_.SetVisible(true);
        editorCameraTransform_ = cameraBefore;
        editorCameraTransform_.UpdateTransform();
        camera->fov = creatorModelImporter.cameraFovBefore;
        camera->TransformCamera(editorCameraTransform_);
        camera->UpdateCamera();
        ClearSelectionOutline();
        session_->Selection().Clear();
        RefreshHierarchy();
        RefreshInspector();

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
            std::string settingsJson = "{}";
            bridge::CreatorModelImportResult imported;
        };
        auto state = std::make_shared<GovernedCommitState>();
        const auto& project = session_->Projects().CurrentProject();
        state->projectRoot = project.rootPath;
        state->projectId = project.projectId;
        state->sourcePath = sourcePath;
        state->assetName = assetName;
        state->destinationFolder = destinationFolder;
        state->thumbnailCapturePath = creatorModelImporter.thumbnailCapturePath;
        state->materialOverrides = materialOverrides;
        state->animationRecipe = animationRecipe;
        studioChrome_.SetStatusText("IMPORT MODEL // COMMITTING GOVERNED ASSET");

        wi::jobsystem::Execute(modelImportWorkload_,
            [this, state](wi::jobsystem::JobArgs)
            {
                const fs::path previewDirectory =
                    fs::u8path(state->projectRoot) / "Intermediate" / "Imports";
                std::error_code ec;
                fs::create_directories(previewDirectory, ec);
                bridge::ModelImportRequest modelRequest;
                modelRequest.sourcePath = state->sourcePath;
                modelRequest.assetPath = (previewDirectory / ".creator-commit.wiscene").generic_u8string();
                modelRequest.expectedFormat = bridge::ImportService::ClassifyModelSourceFormat(state->sourcePath);
                auto converted = bridge::ImportService().PrepareModelAsset(modelRequest);
                if (converted.IsReady())
                {
                    bridge::CreatorModelMaterialPreparationRequest materialRequest;
                    materialRequest.preparedScene = converted.PeekScene();
                    materialRequest.projectRoot = state->projectRoot;
                    materialRequest.projectId = state->projectId;
                    materialRequest.modelSourcePath = state->sourcePath;
                    materialRequest.overrides = state->materialOverrides;
                    auto materials = bridge::PrepareCreatorModelMaterials(materialRequest);
                    if (materials.succeeded)
                    {
                        materials.recipe.animations = state->animationRecipe;
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
                else
                {
                    state->imported.error = converted.Result().error;
                }

                bridge::CreatorAssetWorkflowService workflow;
                if (state->imported.error.empty())
                    state->imported = workflow.ImportModel(
                        state->projectRoot, state->projectId, state->sourcePath,
                        state->settingsJson, state->assetName,
                        state->destinationFolder);
                if (state->imported.succeeded)
                {
                    fs::path thumbnailPath =
                        fs::u8path(state->projectRoot) /
                        fs::u8path(state->imported.assetProjectRelativePath);
                    thumbnailPath.replace_extension(".thumbnail.png");
                    std::error_code thumbnailEc;
                    fs::copy_file(
                        fs::u8path(state->thumbnailCapturePath),
                        thumbnailPath,
                        fs::copy_options::overwrite_existing,
                        thumbnailEc);
                    if (thumbnailEc)
                        state->thumbnailError = thumbnailEc.message();
                }
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, state](std::uint64_t)
                    {
                        creatorModelImporter = {};
                        if (!state->imported.succeeded)
                        {
                            studioChrome_.SetStatusText("IMPORT MODEL // COMMIT FAILED");
                            wi::helper::messageBox(
                                "The preview was discarded safely, but the governed asset could not be committed.\n\nReason: " + state->imported.error,
                                "Import Model");
                            return;
                        }
                        RefreshHierarchy();
                        RefreshInspector();
                        RefreshStatus();
                        assetBrowserCurrentFolder_ = fs::u8path(
                            state->imported.assetProjectRelativePath)
                            .parent_path().lexically_normal().generic_u8string();
                        studioChrome_.SetActiveBottomTab(0, true);
                        RefreshAssetBrowser();
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
                        studioChrome_.SetStatusText(
                            "IMPORT MODEL // GOVERNED ASSET VISIBLE + SELECTED" +
                            (state->thumbnailError.empty()
                                ? std::string(" // THUMBNAIL SAVED // ")
                                : std::string(" // THUMBNAIL WARNING // ")) +
                            fs::u8path(state->imported.assetProjectRelativePath).filename().generic_u8string());
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
            studioChrome_.SetStatusText(
                "TEST LEVEL // RUNNING // UNSAVED SNAPSHOT");
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
            studioChrome_.SetStatusText("TEST LEVEL // COMPLETED");
            return;
        }

        studioChrome_.SetStatusText("TEST LEVEL // FAILED");
        std::string message = result.message.empty()
            ? "The Test Level Runtime stopped before it became ready."
            : result.message;
        if (!result.warning.empty())
        {
            message += "\n\nWarning: " + result.warning;
        }
        wi::helper::messageBox(message, "Test Level");
    }

    void StudioRenderPath::StopTestLevel()
    {
        if (!testLevelRuntime_.IsActive())
        {
            studioChrome_.SetTestLevelState(
                RenegadeStudioChrome::TestLevelState::Idle);
            return;
        }

        const TestLevelProcessResult result = testLevelRuntime_.Stop();
        studioChrome_.SetTestLevelState(
            RenegadeStudioChrome::TestLevelState::Idle);
        studioChrome_.SetStatusText(
            result.cleanupSucceeded
                ? "TEST LEVEL // STOPPED // SNAPSHOT CLEAN"
                : "TEST LEVEL // STOPPED // CLEANUP WARNING");
        if (!result.warning.empty())
        {
            wi::helper::messageBox(result.warning, "Test Level");
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

        const std::string projectName = projectNameInput_.GetText();
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
                if (!session_->Projects().CreateProject(
                        parentDirectory,
                        projectName,
                        "Content/ProvingGround.wiscene"))
                {
                    hubMessageLabel_.font.params.color = WarningAmber;
                    hubMessageLabel_.SetText(
                        "PROJECT CREATE FAILED // " +
                        session_->Projects().LastError());
                    return;
                }

                ClearSelectionOutline();
                if (!session_->LoadScene(session_->Projects().StartupScenePath()))
                {
                    hubMessageLabel_.font.params.color = WarningAmber;
                    hubMessageLabel_.SetText(
                        "PROJECT SCENE FAILED // " +
                        session_->Scenes().LastError());
                    return;
                }

                RestoreGovernedMaterialTextures();
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
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, descriptorPath](uint64_t)
                    {
                        OpenProjectDescriptor(descriptorPath);
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

        ClearSelectionOutline();
        const bool saved = session_->SaveScene(currentPath);
        SyncSelectionOutline();
        RefreshStatus();
        RefreshInspector();
        if (saved)
        {
            continuation();
        }
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

    void StudioRenderPath::OpenProjectDescriptor(
        const std::string& descriptorPath)
    {
        if (session_ == nullptr)
        {
            return;
        }

        if (!session_->Projects().OpenProject(descriptorPath))
        {
            hubMessageLabel_.font.params.color = WarningAmber;
            hubMessageLabel_.SetText(
                "PROJECT OPEN FAILED // " +
                session_->Projects().LastError());
            return;
        }
        ClearSelectionOutline();
        if (!session_->LoadScene(session_->Projects().StartupScenePath()))
        {
            hubMessageLabel_.font.params.color = WarningAmber;
            hubMessageLabel_.SetText(
                "PROJECT SCENE FAILED // " +
                session_->Scenes().LastError());
            return;
        }

        RestoreGovernedMaterialTextures();
        workspaceTitle_.SetText(
            "RENEGADE STUDIO // " +
            session_->Projects().CurrentProject().name);
        hubMessageLabel_.font.params.color = HologramMuted;
        hubMessageLabel_.SetText(
            "PROJECT ONLINE // " +
            session_->Projects().CurrentProject().descriptorPath);
        selectedRecentProject_ = -1;
        RefreshProjectHub();
        SetProjectHubVisible(false);
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

        OpenProjectDescriptor(recent[index].descriptorPath);
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
        projectHubPanel_.SetVisible(visible);
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

    void StudioRenderPath::SaveScene()
    {
        if (session_ == nullptr)
        {
            return;
        }

        const std::string scenePath = session_->Scenes().CurrentPath();
        if (scenePath.empty())
        {
            SaveSceneAs();
            return;
        }

        StopSunPreview(true);
        ClearSelectionOutline();
        session_->SaveScene(scenePath);
        SyncSelectionOutline();
        RefreshStatus();
        RefreshInspector();
    }

    void StudioRenderPath::SaveSceneAs(
        std::function<void(bool)> completion)
    {
        if (session_ == nullptr)
        {
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
                    [this, scenePath, completion](uint64_t)
                    {
                        ClearSelectionOutline();
                        const bool saved = session_->SaveScene(scenePath);
                        SyncSelectionOutline();
                        RefreshStatus();
                        RefreshInspector();
                        if (completion)
                        {
                            completion(saved);
                        }
                    });
            },
            [completion]()
            {
                if (completion)
                {
                    completion(false);
                }
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
        ActivatePath(&renderer_);
    }
}
