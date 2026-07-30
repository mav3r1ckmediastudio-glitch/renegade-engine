#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <WickedEngine.h>
#include <Translator.h>

#include "renegade/bridge/StudioSession.h"

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
            TranslateTool,
            RotateTool,
            ScaleTool,
            ToggleGrid,
            ToggleWorkspaceMenu,
            ToggleHierarchy,
            ToggleInspector,
            ResetWorkspace,
            ShowAssets,
            ShowConsole,
            ShowOutput,
            ShowDiagnostics,
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
            Translate,
            Rotate,
            Scale,
        };

        enum class BottomDrawerPage
        {
            None,
            Assets,
            Console,
            Output,
            Diagnostics,
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
        void SetBottomDrawerPage(BottomDrawerPage page);
        void RefreshWorkspaceControls();
        void PersistWorkspaceLayout();
        void ResetWorkspaceLayout();
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
        wi::gui::Button windowMenuButton_;
        wi::gui::Button projectHubButton_;
        wi::gui::Window hierarchyPanel_;
        wi::gui::Label statusLabel_;
        wi::gui::Label hierarchyLabel_;
        wi::gui::TreeList hierarchyTree_;
        wi::gui::Window inspectorPanel_;
        wi::gui::Label inspectorLabel_;
        wi::gui::Label positionLabel_;
        wi::gui::Label rotationLabel_;
        wi::gui::Label scaleLabel_;
        wi::gui::TextInputField translationX_;
        wi::gui::TextInputField translationY_;
        wi::gui::TextInputField translationZ_;
        wi::gui::TextInputField rotationX_;
        wi::gui::TextInputField rotationY_;
        wi::gui::TextInputField rotationZ_;
        wi::gui::TextInputField scaleX_;
        wi::gui::TextInputField scaleY_;
        wi::gui::TextInputField scaleZ_;
        wi::gui::Label environmentSkyLabel_;
        wi::gui::ComboBox environmentPreset_;
        wi::gui::ComboBox skyMode_;
        wi::gui::CheckBox aerialPerspective_;
        wi::gui::TextInputField skyExposure_;
        wi::gui::TextInputField ambientIntensity_;
        wi::gui::Label environmentFogLabel_;
        wi::gui::TextInputField fogStart_;
        wi::gui::TextInputField fogDensity_;
        wi::gui::CheckBox heightFog_;
        wi::gui::TextInputField fogHeightStart_;
        wi::gui::TextInputField fogHeightEnd_;
        wi::gui::Label environmentCloudLabel_;
        wi::gui::TextInputField cloudCoverage_;
        wi::gui::TextInputField cloudStartHeight_;
        wi::gui::TextInputField cloudThickness_;
        wi::gui::CheckBox cloudsCastShadow_;
        wi::gui::Button focusButton_;
        wi::gui::Button duplicateButton_;
        wi::gui::Button deleteButton_;
        wi::gui::Button undoButton_;
        wi::gui::Button redoButton_;
        wi::gui::Button saveButton_;
        wi::gui::Button saveAsButton_;
        wi::gui::Button reopenButton_;
        wi::gui::Window bottomTabBar_;
        wi::gui::Button assetsTabButton_;
        wi::gui::Button consoleTabButton_;
        wi::gui::Button outputTabButton_;
        wi::gui::Button diagnosticsTabButton_;
        wi::gui::Window contentPanel_;
        wi::gui::Label contentLabel_;
        wi::gui::Label contentPlaceholder_;
        wi::gui::Window workspaceMenuPanel_;
        wi::gui::Label workspaceMenuLabel_;
        wi::gui::Button hierarchyToggleButton_;
        wi::gui::Button inspectorToggleButton_;
        wi::gui::Button resetWorkspaceButton_;
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
        float hierarchyWidth_ = 286.0f;
        float inspectorWidth_ = 330.0f;
        float bottomDrawerHeight_ = 210.0f;
        bool gizmoDragActive_ = false;
        bool flyCameraActive_ = false;
        bool gridVisible_ = true;
        bool hierarchyVisible_ = true;
        bool inspectorVisible_ = true;
        bool workspaceMenuOpen_ = false;
        bool projectHubVisible_ = true;
        BottomDrawerPage bottomDrawerPage_ = BottomDrawerPage::None;
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
