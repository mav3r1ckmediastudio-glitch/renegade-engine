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
        void DeleteGPUResources() override;
        void Load() override;
        void Render() const override;
        void ResizeBuffers() override;
        void Update(float dt) override;
        void Compose(wi::graphics::CommandList cmd) const override;
        void ResizeLayout() override;
        void RefreshStatus();
        void RefreshHierarchy();
        void RefreshInspector();
        void RefreshProjectHub();

    private:
        enum class HistoryAction
        {
            None,
            Undo,
            Redo,
        };

        void ApplySelectedTranslation(int axis, float value);
        void ApplyRenegadeTheme();
        void CreateProjectHub();
        void CreateWorkspaceShell();
        void CreateProject();
        void ClearSelectionOutline() noexcept;
        bool HandleViewportSelection(const XMFLOAT4& pointer);
        void HandleViewportNavigation(float dt, const XMFLOAT4& pointer);
        [[nodiscard]] bool IsPointerOverViewport(
            const XMFLOAT4& pointer) const noexcept;
        void OpenProject();
        void OpenProjectDescriptor(const std::string& descriptorPath);
        void OpenSelectedRecentProject();
        void ReturnToProjectHub();
        void SelectRecentProject(std::size_t index);
        void SetProjectHubVisible(bool visible);
        void SyncGizmoSelection();
        void SyncSelectionOutline();
        void SaveSceneAs();
        void ReopenScene();

        bridge::StudioSession* session_ = nullptr;
        wi::gui::Window toolbarPanel_;
        wi::gui::Label workspaceTitle_;
        wi::gui::Button projectHubButton_;
        wi::gui::Window hierarchyPanel_;
        wi::gui::Label statusLabel_;
        wi::gui::Label hierarchyLabel_;
        wi::gui::TreeList hierarchyTree_;
        wi::gui::Window inspectorPanel_;
        wi::gui::Label inspectorLabel_;
        wi::gui::TextInputField translationX_;
        wi::gui::TextInputField translationY_;
        wi::gui::TextInputField translationZ_;
        wi::gui::Button undoButton_;
        wi::gui::Button redoButton_;
        wi::gui::Button saveAsButton_;
        wi::gui::Button reopenButton_;
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
        Translator gizmo_;
        wi::graphics::Texture selectionOutlineMask_;
        wi::graphics::Texture selectionOutlineMaskMsaa_;
        wi::scene::TransformComponent editorCameraTransform_;
        wi::ecs::Entity gizmoEntity_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity outlinedEntity_ = wi::ecs::INVALID_ENTITY;
        std::uint8_t outlinedEntityPreviousStencil_ = 0;
        XMFLOAT3 gizmoTranslationBefore_ = {};
        XMFLOAT4 viewportBounds_ = {};
        XMFLOAT4 cameraPointerAnchor_ = {};
        float cameraMoveSpeed_ = 5.0f;
        bool gizmoDragActive_ = false;
        bool flyCameraActive_ = false;
        bool projectHubVisible_ = true;
        int selectedRecentProject_ = -1;
        HistoryAction pendingHistoryAction_ = HistoryAction::None;
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
