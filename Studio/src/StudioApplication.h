#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <WickedEngine.h>
#include <Translator.h>

#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/AssetBrowserService.h"
#include "renegade/bridge/LightService.h"
#include "renegade/bridge/ImportService.h"
#include "renegade/bridge/OceanService.h"
#include "renegade/bridge/PrecipitationService.h"
#include "renegade/bridge/SunService.h"
#include "renegade/bridge/TerrainService.h"
#include "CreatorImportPreviewWindow.h"
#include "RenegadeStudioChrome.h"
#include "RenegadeProjectHub.h"
#include "RenegadeProjectLoadingOverlay.h"
#include "RenegadeStoryFlowRenderPath.h"
#include "RenegadeScreenEditorRenderPath.h"
#include "StoryFlowStudioIntegration.h"
#include "TestLevelRuntimeProcess.h"

// StudioApplication.cpp defines this helper in its global unnamed namespace.
// Declare it here so earlier creator-import callbacks can call it before the
// definition appears later in the translation unit.
namespace
{
    void RefreshCreatorImportMaterialReadout();
}

namespace renegade::studio
{
    class StudioRenderPath final : public wi::RenderPath3D
    {
    public:
        void BindSession(bridge::StudioSession& session) noexcept;
        void SetExitRequestHandler(std::function<void()> handler);
        void RequestExit();
        void BindDiagnostics(
            wi::Application::InfoDisplayer& diagnostics) noexcept;
        void DeleteGPUResources() override;
        void Load() override;
        void Render() const override;
        void ResizeBuffers() override;
        void Update(float dt) override;
        void Compose(wi::graphics::CommandList cmd) const override;
        void RenderTransparents(wi::graphics::CommandList cmd) const override;
        void ResizeLayout() override;
        void RefreshStatus();
        void RefreshHierarchy();
        void RefreshInspector();
        void RefreshProjectHub();
        void RefreshAssetBrowser();
        void RestoreGovernedMaterialTextures();

        void RequestProjectHubFromStoryFlow();
        void RequestAssetBrowserFromStoryFlow();
        void RequestProjectPlayFromStoryFlow();

        // Story Flow is now the project home. Project-level runtime lifecycle
        // must therefore be callable and pollable without asking the inactive
        // Level Editor render path to consume one of its private action queues.
        void StartProjectPlayFromStoryFlowNow()
        {
            StartProjectPlay();
        }

        void PollProjectPlayFromStoryFlow()
        {
            if (projectPreviewActive_ && testLevelRuntime_.IsActive())
                PollTestLevel();
        }

        void StopProjectPlayFromStoryFlowNow()
        {
            if (projectPreviewActive_)
                StopTestLevel();
        }

        [[nodiscard]] bool IsProjectPlayFromStoryFlowActive() const noexcept
        {
            return projectPreviewActive_ && testLevelRuntime_.IsActive();
        }

        [[nodiscard]] bool IsProjectPlayFromStoryFlowRunning() const noexcept
        {
            return IsProjectPlayFromStoryFlowActive() &&
                testLevelRuntime_.LastResult().state ==
                    TestLevelProcessState::Running;
        }

        [[nodiscard]] const TestLevelProcessResult&
        ProjectPlayFromStoryFlowResult() const noexcept
        {
            return testLevelRuntime_.LastResult();
        }

        // Gate 1 exposes only the presentation/lifecycle seams required by the
        // Story Flow adapter. Semantic Flow state remains in EngineBridge.
        [[nodiscard]] bool IsProjectHubVisible() const noexcept
        {
            return projectHubVisible_;
        }

        [[nodiscard]] bool IsProjectLoadBlocking() const noexcept
        {
            return projectLoadingOverlay_.IsBlocking();
        }

        [[nodiscard]] XMFLOAT4 StoryFlowWorkspaceBounds() const noexcept
        {
            return studioChrome_.ViewportBounds();
        }

        wi::gui::GUI& StoryFlowGui() noexcept
        {
            return GetGUI();
        }

        // Wicked renders top-level wiGUI widgets in reverse registration order.
        // Story Flow lifecycle controls are attached after Studio creation, so
        // a normal AddWidget() would put them behind the full Scene chrome.
        // Register the control, then deliberately move the chrome to the end:
        // reverse rendering paints the chrome first and the lifecycle control
        // afterwards, keeping the control visible and interactive above it.
        void RegisterStoryFlowLifecycleControl(wi::gui::Widget& control)
        {
            auto& gui = GetGUI();
            gui.AddWidget(&control);
            gui.RemoveWidget(&studioChrome_);
            gui.AddWidget(&studioChrome_);
        }

    private:
        enum class EditorAction
        {
            None,
            Undo,
            Redo,
            FocusSelection,
            DuplicateSelection,
            DeleteSelection,
            CreateLight,
            OpenScene,
            SaveScene,
            SaveSceneAs,
            ReopenScene,
            ProjectHub,
            SelectTool,
            TranslateTool,
            RotateTool,
            ScaleTool,
            ToggleGrid,
            OpenEnvironmentWorkspace,
            OpenTerrainWorkspace,
            OpenSceneWorkspace,
            StartTestLevel,
            StartProjectPlay,
            StopTestLevel,
            StartSunPreview,
            PauseSunPreview,
            SetOceanEnabled,
            SetOceanResolution,
            ApplyOceanPreset,
            CreateTerrain,
            ApplyTerrainMaterialPreset,
            ApplyDefaultGrass,
            ReloadTerrainMaterial,
            ValidateModelImport,
            ImportModel,
            ApplyImportScale,
            DismissImportScale,
        };

        struct GridConstants
        {
            XMFLOAT4X4 viewProjection;
            XMFLOAT4X4 inverseViewProjection;
            XMFLOAT4 cameraPosition;
            XMFLOAT4 minorColor;
            XMFLOAT4 majorColor;
            XMFLOAT4 axisColorX;
            XMFLOAT4 axisColorZ;
            XMFLOAT4 params;
        };

        enum class TransformTool
        {
            Select,
            Translate,
            Rotate,
            Scale,
        };

        enum class WeatherField
        {
            SkyExposure,
            Stars,
            AmbientIntensity,
            FogStart,
            FogDensity,
            FogHeightStart,
            FogHeightEnd,
            CloudCoverage,
            CloudStartHeight,
            CloudThickness,
        };

        enum class WeatherToggle
        {
            AerialPerspective,
            HeightFog,
            CloudsCastShadow,
        };

        enum class PrecipitationField
        {
            Intensity,
            FallSpeed,
            ParticleScale,
            WindAzimuth,
            WindSpeed,
            Turbulence,
        };

        enum class SunField
        {
            Time,
            Azimuth,
            Elevation,
        };

        enum class OceanField
        {
            PatchLength,
            TimeScale,
            WaveAmplitude,
            WindAzimuth,
            WindSpeed,
            WindDependency,
            ChoppyScale,
            WaterRed,
            WaterGreen,
            WaterBlue,
            WaterOpacity,
            ExtinctionRed,
            ExtinctionGreen,
            ExtinctionBlue,
            WaterHeight,
            SurfaceDetail,
            DisplacementTolerance,
        };

        enum class TerrainField
        {
            VisibleChunkRadius,
            ChunkScale,
            MinimumHeight,
            MaximumHeight,
            LowAltitudeBlend,
            BaseBlend,
            SlopeBlend,
            LodBias,
        };

        enum class LightField
        {
            ColorRed,
            ColorGreen,
            ColorBlue,
            Intensity,
            Range,
            OuterCone,
            InnerCone,
            Radius,
            Length,
            Height,
            VolumetricBoost,
        };

        enum class LightToggle
        {
            CastShadow,
            Volumetrics,
        };

        void ApplySelectedTransformValue(
            TransformTool tool,
            int axis,
            float value);
        void ApplySelectedWeatherToggle(WeatherToggle toggle, bool value);
        void ApplySelectedSkyMode(bridge::WeatherState::SkyMode mode);
        void ApplyWeatherPreset(int preset);
        void BeginWeatherSlider(WeatherField field);
        void PreviewWeatherSlider(WeatherField field, float value);
        void CommitWeatherSlider(WeatherField field, float value);
        static void SetWeatherFieldValue(
            bridge::WeatherState& weather,
            WeatherField field,
            float value) noexcept;
        bool CommitSelectedWeather(const bridge::WeatherState& weather);
        void ApplyPrecipitationMode(bridge::PrecipitationMode mode);
        void BeginPrecipitationSlider(PrecipitationField field);
        void PreviewPrecipitationSlider(
            PrecipitationField field,
            float value);
        void CommitPrecipitationSlider(
            PrecipitationField field,
            float value);
        static void SetPrecipitationFieldValue(
            bridge::PrecipitationState& precipitation,
            PrecipitationField field,
            float value) noexcept;
        bool CommitPrecipitation(
            const bridge::PrecipitationState& precipitation);
        void ApplySunPreset(bridge::SunPreset preset);
        void BeginSunSlider(SunField field);
        void PreviewSunSlider(SunField field, float value);
        void CommitSunSlider(SunField field, float value);
        static void SetSunFieldValue(
            bridge::SunState& sun,
            SunField field,
            float value) noexcept;
        bool CommitSun(const bridge::SunState& sun);
        void StartSunPreview();
        void StopSunPreview(bool commit);
        void ApplyOceanEnabled(bool enabled);
        void ApplyOceanResolution(int dimension);
        void ApplyOceanPreset(bridge::OceanPreset preset);
        void BeginOceanSlider(OceanField field);
        void PreviewOceanSlider(OceanField field, float value);
        void CommitOceanSlider(OceanField field, float value);
        static void SetOceanFieldValue(
            bridge::OceanState& ocean,
            OceanField field,
            float value) noexcept;
        bool CommitOcean(const bridge::OceanState& ocean);
        void CreateTerrain();
        void BeginTerrainSlider(TerrainField field);
        void PreviewTerrainSlider(TerrainField field, float value);
        void CommitTerrainSlider(TerrainField field, float value);
        void ApplyTerrainMaterialPreset(bridge::TerrainMaterialPreset preset);
        void BeginTerrainTextureScale();
        void PreviewTerrainTextureScale(float value);
        void CommitTerrainTextureScale(float value);
        void ApplyDefaultGrass();
        void ReloadTerrainMaterial();
        void ValidateModelImport();
        void RunModelImportProof(const std::string& sourcePath);
        void PresentModelImportProof(
            const bridge::ImportResult& result);
        void ImportModel();
        void RunModelImportPlacement(const std::string& sourcePath);
        void CreateImportScalePanel();
        void ShowImportScalePanel(
            wi::ecs::Entity entity,
            float appliedScaleFactor,
            const std::string& sourceFileName);
        void FrameCreatorImportPreviewCamera();
        void CaptureCreatorImportThumbnail();
        void ApplyImportScaleMode(bridge::ModelScaleMode mode);
        void DismissImportScalePanel();
        void RefreshCreatorImportWorkspaceSection();
        static void SetTerrainFieldValue(
            bridge::TerrainState& terrain,
            TerrainField field,
            float value) noexcept;
        bool CommitTerrain(const bridge::TerrainState& terrain);
        void ApplySelectedLightType(
            wi::scene::LightComponent::LightType type);
        void ApplySelectedLightToggle(LightToggle toggle, bool value);
        void BeginLightSlider(LightField field);
        void PreviewLightSlider(LightField field, float value);
        void CommitLightSlider(LightField field, float value);
        static void SetLightFieldValue(
            bridge::LightState& light,
            LightField field,
            float value) noexcept;
        bool CommitSelectedLight(const bridge::LightState& light);
        void CreateLight(
            wi::scene::LightComponent::LightType type);
        void PlaceLight(
            wi::scene::LightComponent::LightType type,
            const XMFLOAT3& position,
            const XMFLOAT4& rotation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
        bool HandleLightPlacement(const XMFLOAT4& pointer);
        void CancelLightPlacement();
        void BeginCreatorAssetPlacement(
            const bridge::StableId& assetId,
            const std::string& label);
        void DropCreatorAsset(
            const bridge::StableId& assetId,
            const std::string& label,
            float screenX,
            float screenY);
        bool HandleCreatorAssetPlacement(const XMFLOAT4& pointer);
        void CancelCreatorAssetPlacement();
        bool HandleLightSceneIcons(const XMFLOAT4& pointer);
        [[nodiscard]] bool ProjectEditorPoint(
            const XMFLOAT3& world,
            XMFLOAT2& screen) const noexcept;
        bool HandleTerrainSculpt(const XMFLOAT4& pointer);
        [[nodiscard]] wi::ecs::Entity EditableWeatherEntity() const noexcept;
        void SetEnvironmentWorkspaceActive(bool active);
        void SetTerrainWorkspaceActive(bool active);
        void ApplyRenegadeTheme();
        void LoadGridResources();
        void LayoutInspectorActions(
            bool environment,
            bool terrain = false,
            bool light = false);
        void DrawEditorGrid(wi::graphics::CommandList cmd) const;
        void SetGridVisible(bool visible);
        void CreateProjectHub();
        void CreateWorkspaceShell();
        void SelectAssetBrowserFolder(const std::string& relativePath);
        void SelectAssetBrowserItem(const std::string& relativePath);
        void CreateProject();
        void ClearSelectionOutline() noexcept;
        void DeleteSelection();
        void DuplicateSelection();
        void FocusSelection();
        void HandleEditorShortcuts();
        bool HandleViewportSelection(const XMFLOAT4& pointer);
        void HandleViewportNavigation(float dt, const XMFLOAT4& pointer);
        [[nodiscard]] bool IsPointerOverViewport(
            const XMFLOAT4& pointer) const noexcept;
        [[nodiscard]] bool IsSelectedEntityValid() const;
        void OpenProject();
        void OpenScene();
        void BeginOpenScene(const std::string& scenePath);
        void CompleteOpenScene(bridge::PreparedSceneOpen prepared);
        void AdoptOpenedSceneCamera();
        void AdoptOpenedSceneTerrainFallbackCamera(
            const wi::scene::Scene& openedScene);
        struct ProjectLoadOperation;
        void BeginProjectLoad(const std::string& descriptorPath);
        void CompleteProjectLoad(std::shared_ptr<ProjectLoadOperation> operation);
        void OpenProjectDescriptor(const std::string& descriptorPath);
        void OpenSelectedRecentProject();
        void ProcessPendingAction();
        void StartTestLevel();
        void StartProjectPlay();
        void PollTestLevel();
        void StopTestLevel();
        [[nodiscard]] std::string ResolveTestLevelRuntimePath() const;
        [[nodiscard]] std::string TestLevelBackendArgument() const;
        void ReturnToProjectHub();
        void SelectRecentProject(std::size_t index);
        void SetTransformTool(TransformTool tool);
        void SetProjectHubVisible(bool visible);
        void SyncGizmoSelection();
        void SyncSelectionOutline();
        void SaveSceneAfterTransientCleanup(
            const std::string& scenePath,
            std::function<void(bool)> completion = {});
        void SaveScene();
        void SaveSceneAs(
            std::function<void(bool)> completion = {});
        void ReopenScene();
        void RequestSceneReplacement(
            std::function<void()> continuation);

        bridge::StudioSession* session_ = nullptr;
        wi::Application::InfoDisplayer* diagnostics_ = nullptr;
        wi::gui::Window toolbarPanel_;
        wi::gui::Label workspaceTitle_;
        wi::gui::Button translateToolButton_;
        wi::gui::Button rotateToolButton_;
        wi::gui::Button scaleToolButton_;
        wi::gui::Button projectHubButton_;
        wi::gui::Window hierarchyPanel_;
        wi::gui::Label statusLabel_;
        wi::gui::Label hierarchyLabel_;
        wi::gui::TreeList hierarchyTree_;
        SceneInspectorTextInputField hierarchySearch_;
        wi::gui::Window inspectorPanel_;
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
        SceneInspectorSlider terrainVisibleRadius_;
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
        CreatorAssetStudioChrome studioChrome_;
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
        wi::ecs::Entity outlinedEntity_ = wi::ecs::INVALID_ENTITY;
        std::uint8_t outlinedEntityPreviousStencil_ = 0;
        bridge::TransformState gizmoTransformBefore_;
        XMFLOAT4 viewportBounds_ = {};
        XMFLOAT4 cameraPointerAnchor_ = {};
        float cameraMoveSpeed_ = 5.0f;
        bool gizmoDragActive_ = false;
        bool flyCameraActive_ = false;
        bool gridVisible_ = true;
        int lastDrawerTab_ = 0;
        bool workspaceLayoutDirty_ = false;
        bool weatherSliderActive_ = false;
        WeatherField weatherSliderField_ = WeatherField::SkyExposure;
        wi::ecs::Entity weatherSliderEntity_ = wi::ecs::INVALID_ENTITY;
        bridge::WeatherState weatherSliderBefore_;
        bridge::WeatherState weatherSliderAfter_;
        bool environmentWorkspaceActive_ = false;
        bool terrainWorkspaceActive_ = false;
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
        TerrainField terrainSliderField_ = TerrainField::VisibleChunkRadius;
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
        wi::jobsystem::context sceneOpenWorkload_;
        wi::jobsystem::context projectLoadWorkload_;
        wi::jobsystem::context modelImportWorkload_;
        std::string openingScenePath_;
        bool sceneOpenInProgress_ = false;
        wi::ecs::Entity importScaleTargetEntity_ = wi::ecs::INVALID_ENTITY;
        float importScaleAppliedFactor_ = 1.0f;
        bridge::ModelScaleMode pendingImportScaleMode_ =
            bridge::ModelScaleMode::Original;
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
        StudioRenderPath renderer_;
        RenegadeStoryFlowRenderPath storyFlowRenderer_;
        RenegadeScreenEditorRenderPath screenEditorRenderer_;
        StoryFlowStudioIntegration storyFlowIntegration_;
        std::string startupScene_ = "Content/ProvingGround.wiscene";
    };
}
