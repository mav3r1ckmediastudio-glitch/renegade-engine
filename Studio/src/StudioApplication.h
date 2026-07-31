#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <WickedEngine.h>
#include <Translator.h>

#include "renegade/bridge/StudioSession.h"
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
            SaveScene,
            SaveSceneAs,
            ReopenScene,
            ProjectHub,
            SelectTool,
            TranslateTool,
            RotateTool,
            ScaleTool,
            ToggleGrid,
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

        void ApplySelectedTransformValue(
            TransformTool tool,
            int axis,
            float value);
        void ApplySelectedWeatherValue(WeatherField field, float value);
        void ApplySelectedWeatherToggle(WeatherToggle toggle, bool value);
        void ApplySelectedSkyMode(bridge::WeatherState::SkyMode mode);
        void ApplyWeatherPreset(int preset);
        bool CommitSelectedWeather(const bridge::WeatherState& weather);
        void ApplyRenegadeTheme();
        void LoadGridResources();
        void LayoutInspectorActions(bool environment);
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
        void SaveSceneAs();
        void ReopenScene();

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
        wi::gui::Label environmentSkyLabel_;
        RenegadeComboBox environmentPreset_;
        RenegadeComboBox skyMode_;
        RenegadeCheckBox aerialPerspective_;
        RenegadeTextInputField skyExposure_;
        RenegadeTextInputField ambientIntensity_;
        wi::gui::Label environmentFogLabel_;
        RenegadeTextInputField fogStart_;
        RenegadeTextInputField fogDensity_;
        RenegadeCheckBox heightFog_;
        RenegadeTextInputField fogHeightStart_;
        RenegadeTextInputField fogHeightEnd_;
        wi::gui::Label environmentCloudLabel_;
        RenegadeTextInputField cloudCoverage_;
        RenegadeTextInputField cloudStartHeight_;
        RenegadeTextInputField cloudThickness_;
        RenegadeCheckBox cloudsCastShadow_;
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
        bool projectHubVisible_ = true;
        int selectedRecentProject_ = -1;
        EditorAction pendingAction_ = EditorAction::None;
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
