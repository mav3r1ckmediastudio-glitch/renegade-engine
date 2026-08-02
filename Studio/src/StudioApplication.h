#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <WickedEngine.h>
#include <Translator.h>

#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/LightService.h"
#include "renegade/bridge/ImportService.h"
#include "renegade/bridge/OceanService.h"
#include "renegade/bridge/PrecipitationService.h"
#include "renegade/bridge/SunService.h"
#include "renegade/bridge/TerrainService.h"
#include "RenegadeStudioChrome.h"

namespace renegade::studio
{
    class StudioRenderPath final : public wi::RenderPath3D
    {
    public:
        void BindSession(bridge::StudioSession& session) noexcept;
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
        };

        // Mirrors RenegadeGridCB in Studio/shaders/RenegadeGridPS.hlsl.
        // Every member is 16-byte aligned, so HLSL constant buffer packing
        // matches this layout member for member.
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
        // Fallback used by AdoptOpenedSceneCamera() when the opened
        // document has no authored camera. Recenters the editor camera
        // over the terrain's own saved chunk position so Wicked's
        // camera-distance chunk streaming does not evict the
        // just-loaded, correctly-deserialized terrain chunks on the
        // next frame. No-ops if the scene has no terrain.
        void AdoptOpenedSceneTerrainFallbackCamera(
            const wi::scene::Scene& openedScene);
        void OpenProjectDescriptor(const std::string& descriptorPath);
        void OpenSelectedRecentProject();
        void ProcessPendingAction();
        void ReturnToProjectHub();
        void SelectRecentProject(std::size_t index);
        void SetTransformTool(TransformTool tool);
        void SetProjectHubVisible(bool visible);
        void SyncGizmoSelection();
        void SyncSelectionOutline();
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
        RenegadeTextInputField hierarchySearch_;
        wi::gui::Window inspectorPanel_;
        wi::gui::Label inspectorLabel_;
        wi::gui::Label positionLabel_;
        wi::gui::Label rotationLabel_;
        wi::gui::Label scaleLabel_;
        RenegadeTextInputField translationX_;
        RenegadeTextInputField translationY_;
        RenegadeTextInputField translationZ_;
        RenegadeTextInputField rotationX_;
        RenegadeTextInputField rotationY_;
        RenegadeTextInputField rotationZ_;
        RenegadeTextInputField scaleX_;
        RenegadeTextInputField scaleY_;
        RenegadeTextInputField scaleZ_;
        wi::gui::Label lightLabel_;
        RenegadeComboBox lightType_;
        RenegadeSlider lightColorRed_;
        RenegadeSlider lightColorGreen_;
        RenegadeSlider lightColorBlue_;
        RenegadeSlider lightIntensity_;
        RenegadeSlider lightRange_;
        RenegadeSlider lightOuterCone_;
        RenegadeSlider lightInnerCone_;
        RenegadeSlider lightRadius_;
        RenegadeSlider lightLength_;
        RenegadeSlider lightHeight_;
        RenegadeCheckBox lightCastShadow_;
        RenegadeCheckBox lightVolumetrics_;
        RenegadeSlider lightVolumetricBoost_;
        wi::gui::Label environmentSkyLabel_;
        RenegadeComboBox environmentPreset_;
        RenegadeComboBox skyMode_;
        RenegadeCheckBox aerialPerspective_;
        RenegadeSlider skyExposure_;
        RenegadeSlider ambientIntensity_;
        wi::gui::Label environmentFogLabel_;
        RenegadeSlider fogStart_;
        RenegadeSlider fogDensity_;
        RenegadeCheckBox heightFog_;
        RenegadeSlider fogHeightStart_;
        RenegadeSlider fogHeightEnd_;
        wi::gui::Label environmentCloudLabel_;
        RenegadeSlider cloudCoverage_;
        RenegadeSlider cloudStartHeight_;
        RenegadeSlider cloudThickness_;
        RenegadeCheckBox cloudsCastShadow_;
        wi::gui::Label precipitationLabel_;
        RenegadeComboBox precipitationMode_;
        RenegadeSlider precipitationIntensity_;
        RenegadeSlider precipitationFallSpeed_;
        RenegadeSlider precipitationParticleScale_;
        RenegadeSlider precipitationWindAzimuth_;
        RenegadeSlider precipitationWindSpeed_;
        RenegadeSlider precipitationTurbulence_;
        wi::gui::Label sunLabel_;
        RenegadeComboBox sunPreset_;
        RenegadeSlider sunTime_;
        RenegadeSlider sunAzimuth_;
        RenegadeSlider sunElevation_;
        RenegadeSlider sunPreviewSpeed_;
        RenegadeButton sunPlayButton_;
        RenegadeButton sunPauseButton_;
        wi::gui::Label oceanLabel_;
        RenegadeCheckBox oceanEnabled_;
        RenegadeComboBox oceanPreset_;
        RenegadeComboBox oceanResolution_;
        RenegadeSlider oceanWaterHeight_;
        RenegadeSlider oceanPatchLength_;
        RenegadeSlider oceanWaveAmplitude_;
        RenegadeSlider oceanChoppyScale_;
        RenegadeSlider oceanTimeScale_;
        RenegadeSlider oceanWindAzimuth_;
        RenegadeSlider oceanWindSpeed_;
        RenegadeSlider oceanWindDependency_;
        RenegadeSlider oceanSurfaceDetail_;
        RenegadeSlider oceanDisplacementTolerance_;
        RenegadeSlider oceanWaterRed_;
        RenegadeSlider oceanWaterGreen_;
        RenegadeSlider oceanWaterBlue_;
        RenegadeSlider oceanWaterOpacity_;
        RenegadeSlider oceanExtinctionRed_;
        RenegadeSlider oceanExtinctionGreen_;
        RenegadeSlider oceanExtinctionBlue_;
        wi::gui::Label terrainLabel_;
        RenegadeButton createTerrainButton_;
        RenegadeSlider terrainVisibleRadius_;
        RenegadeSlider terrainChunkScale_;
        RenegadeSlider terrainMinimumHeight_;
        RenegadeSlider terrainMaximumHeight_;
        RenegadeSlider terrainLowAltitudeBlend_;
        RenegadeSlider terrainBaseBlend_;
        RenegadeSlider terrainSlopeBlend_;
        RenegadeSlider terrainLodBias_;
        wi::gui::Label terrainMaterialLabel_;
        RenegadeComboBox terrainMaterialPreset_;
        RenegadeSlider terrainTextureScale_;
        RenegadeButton terrainApplyDefaultGrassButton_;
        RenegadeButton terrainReloadMaterialButton_;
        wi::gui::Label terrainSculptLabel_;
        RenegadeComboBox terrainSculptMode_;
        RenegadeSlider terrainBrushRadius_;
        RenegadeSlider terrainBrushStrength_;
        RenegadeSlider terrainBrushFalloff_;
        wi::gui::Label terrainBrushReadout_;
        wi::gui::Label terrainStrokeDiagnostic_;
        RenegadeButton focusButton_;
        RenegadeButton duplicateButton_;
        RenegadeButton deleteButton_;
        RenegadeButton undoButton_;
        RenegadeButton redoButton_;
        RenegadeButton saveButton_;
        RenegadeButton saveAsButton_;
        RenegadeButton reopenButton_;
        wi::gui::Window contentPanel_;
        wi::gui::Label contentLabel_;
        wi::gui::Label contentPlaceholder_;
        wi::gui::Window projectHubPanel_;
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
        wi::gui::Button gridToggleButton_;
        RenegadeStudioChrome studioChrome_;
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
        bool projectHubVisible_ = true;
        int selectedRecentProject_ = -1;
        EditorAction pendingAction_ = EditorAction::None;
        wi::jobsystem::context sceneOpenWorkload_;
        std::string openingScenePath_;
        bool sceneOpenInProgress_ = false;
    };

    class StudioApplication final : public wi::Application
    {
    public:
        void SetStartupScene(std::string filePath);
        void Initialize() override;

    private:
        void PrepareProvingGround();

        bridge::StudioSession session_;
        StudioRenderPath renderer_;
        std::string startupScene_ = "Content/ProvingGround.wiscene";
    };
}
