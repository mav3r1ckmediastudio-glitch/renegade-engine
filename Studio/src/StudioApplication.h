#pragma once

#include <array>
#include <cstddef>
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
        };

        // Bottom dock tab identity. Matches the four drawers named in
        // Renegade_Studio_UI_Design_Tokens_v1.0.json (workspace.bottom_drawers)
        // and Renegade_Studio_Workspace_Prototype_v1.0_Standalone.html.
        enum class DockTab
        {
            Assets,
            Console,
            Output,
            Diagnostics,
        };

        enum class Menu
        {
            None,
            File,
            Edit,
            View,
            Window,
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

        // Workspace shell: resizable/collapsible panels, scene tab, bottom
        // dock tab-switching, menu bar, Asset Browser, Console, unsaved-
        // changes modal. See docs/PHASE3_STUDIO_SHELL_REBUILD.md.
        void CreatePanelChrome();
        void CreateMenuBar();
        void CreateSceneTabStrip();
        void CreateBottomDock();
        void CreateAssetBrowser();
        void CreateConsolePanel();
        void CreateModalAndToast();
        void LayoutMenuBar(float width);
        void LayoutSceneTabStrip(float leftWidth, float rightWidth, float top);
        void LayoutBottomDock(
            float leftWidth,
            float rightWidth,
            float width,
            float height);
        void LayoutAssetBrowser(float contentWidth, float contentHeight);
        void LayoutConsolePanel(float contentWidth, float contentHeight);
        void LayoutModalAndToast(float width, float height);
        void RefreshAssetBrowser();
        void RefreshConsole();
        void SetDockTab(DockTab tab, bool forceOpen);
        void ToggleDockTab(DockTab tab);
        void SetLeftPanelVisible(bool visible);
        void SetRightPanelVisible(bool visible);
        void ToggleMenu(Menu menu);
        void CloseAllMenus();
        [[nodiscard]] bool IsSceneDirty() const;
        void MarkSavedForDirtyTracking();
        void RequestCloseScene();
        void ShowUnsavedChangesModal();
        void HideUnsavedChangesModal();
        void ShowToast(const std::string& title, const std::string& detail);
        void ResetWorkspaceLayout();
        void PersistPanelGeometry();
        void LoadPanelGeometry();

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
        // Menu bar (File/Edit/View/Window). Build is intentionally absent:
        // there is no build/package capability yet, and a menu that does
        // nothing would misrepresent what Studio can do.
        wi::gui::Button menuButtonFile_;
        wi::gui::Button menuButtonEdit_;
        wi::gui::Button menuButtonView_;
        wi::gui::Button menuButtonWindow_;
        wi::gui::Window menuDropdownFile_;
        wi::gui::Window menuDropdownEdit_;
        wi::gui::Window menuDropdownView_;
        wi::gui::Window menuDropdownWindow_;
        wi::gui::Button menuFileSave_;
        wi::gui::Button menuFileSaveAs_;
        wi::gui::Button menuFileCloseScene_;
        wi::gui::Button menuEditUndo_;
        wi::gui::Button menuEditRedo_;
        wi::gui::Button menuViewHierarchy_;
        wi::gui::Button menuViewInspector_;
        wi::gui::Button menuViewAssets_;
        wi::gui::Button menuViewConsole_;
        wi::gui::Button menuViewOutput_;
        wi::gui::Button menuViewDiagnostics_;
        wi::gui::Button menuWindowResetLayout_;
        Menu openMenu_ = Menu::None;

        // Scene tab strip (single tab: Renegade does not support multiple
        // open scenes yet, but the tab still carries the real dirty state
        // and is the real close/unsaved-changes affordance).
        wi::gui::Window sceneTabsPanel_;
        wi::gui::Button sceneTabButton_;
        wi::gui::Button sceneTabCloseButton_;

        // Bottom dock: contentPanel_ is the dockable/collapsible drawer
        // itself; the four tab buttons switch which of the panels below is
        // visible inside it.
        wi::gui::Window contentPanel_;
        wi::gui::Button dockTabAssetsButton_;
        wi::gui::Button dockTabConsoleButton_;
        wi::gui::Button dockTabOutputButton_;
        wi::gui::Button dockTabDiagnosticsButton_;
        wi::gui::Button dockCollapseButton_;
        DockTab activeDockTab_ = DockTab::Assets;
        bool bottomDockOpen_ = false;

        // Asset Browser: a real (read-only) view of the open project's
        // Content directory via ContentBrowserService, not placeholder data.
        static constexpr std::size_t MaximumVisibleAssetFolders = 10;
        static constexpr std::size_t MaximumVisibleAssetCards = 16;
        wi::gui::TextInputField assetSearchField_;
        std::string assetSearchFilter_;
        wi::gui::Label assetBreadcrumbLabel_;
        std::array<wi::gui::Button, MaximumVisibleAssetFolders> assetFolderButtons_;
        std::array<wi::gui::Button, MaximumVisibleAssetCards> assetCardButtons_;
        wi::gui::Label assetEmptyStateLabel_;

        // Console and Output share one live view of wi::backlog -
        // Renegade's and Wicked's real engine log. Wicked does not separate
        // an "output" stream from the log, so inventing a second, different
        // feed for Output would be fake; both tabs show the same real text.
        // Diagnostics renders live renderer/session stats instead.
        wi::gui::Button consoleClearButton_;
        wi::gui::Label consoleLogLabel_;
        wi::gui::Label diagnosticsLabel_;

        // Unsaved-changes modal and save toast.
        wi::gui::Window modalWindow_;
        wi::gui::Label modalTitleLabel_;
        wi::gui::Label modalBodyLabel_;
        wi::gui::Button modalCancelButton_;
        wi::gui::Button modalDiscardButton_;
        wi::gui::Button modalSaveButton_;
        bool modalVisible_ = false;
        wi::gui::Window toastPanel_;
        wi::gui::Label toastTitleLabel_;
        wi::gui::Label toastDetailLabel_;
        float toastTimer_ = 0.0f;

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
        bool gizmoDragActive_ = false;
        bool flyCameraActive_ = false;
        bool gridVisible_ = true;
        bool projectHubVisible_ = true;
        float leftPanelWidth_ = 320.0f;
        float rightPanelWidth_ = 360.0f;
        float bottomDockHeight_ = 300.0f;
        bool hierarchyVisible_ = true;
        bool inspectorVisible_ = true;
        std::size_t lastSavedUndoCount_ = 0;
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
