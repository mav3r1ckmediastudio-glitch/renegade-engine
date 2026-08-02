#pragma once

#include <array>
#include <string>
#include <functional>
#include <cstdint>
#include <vector>

#include <WickedEngine.h>

namespace renegade::studio
{
    class RenegadeTextInputField final : public wi::gui::TextInputField
    {
    public:
        void SetPlaceholder(std::string placeholder);
        void Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override { return "RenegadeTextInputField"; }

    private:
        std::string placeholder_;
    };

    class RenegadeButton final : public wi::gui::Button
    {
    public:
        void Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override { return "RenegadeButton"; }
    };

    class RenegadeCheckBox final : public wi::gui::CheckBox
    {
    public:
        void Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override { return "RenegadeCheckBox"; }
    };

    class RenegadeComboBox final : public wi::gui::ComboBox
    {
    public:
        void Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override { return "RenegadeComboBox"; }
    };

    class RenegadeSlider final : public wi::gui::Slider
    {
    public:
        void Create(
            float minimum,
            float maximum,
            float defaultValue,
            float steps,
            const std::string& name,
            std::string label);
        void OnDragStarted(std::function<void(float)> callback);
        void OnValuePreview(std::function<void(float)> callback);
        void OnValueCommitted(std::function<void(float)> callback);
        void Update(const wi::Canvas& canvas, float dt) override;
        void Render(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override
        {
            return "RenegadeSlider";
        }

    private:
        std::string label_;
        float valueBeforeUpdate_ = 0.0f;
        bool dragging_ = false;
        std::function<void(float)> dragStarted_;
        std::function<void(float)> valuePreview_;
        std::function<void(float)> valueCommitted_;
    };

    // Renegade-owned rendering for the Studio shell. Wicked supplies the
    // canvas and command list, but no stock wiGUI control draws this chrome.
    // Interactive components can be migrated onto this visual foundation one
    // bounded slice at a time after the product owner accepts the proof.
    class RenegadeStudioChrome final : public wi::gui::Widget
    {
    public:
        enum class HierarchyCategory : std::uint8_t
        {
            Lights,
            Models,
            Characters,
            Cameras,
            Terrain,
            Effects,
            Audio,
            Other,
            Count,
        };

        enum class Action
        {
            ProjectHub,
            OpenScene,
            Save,
            SaveAs,
            Reopen,
            Undo,
            Redo,
            Duplicate,
            Delete,
            CreatePointLight,
            CreateSpotLight,
            CreateDirectionalLight,
            CreateRectangleLight,
            Focus,
            ToggleGrid,
            EnvironmentWorkspace,
            TerrainWorkspace,
            SceneWorkspace,
        };

        struct HierarchyRow
        {
            std::string name;
            int depth = 0;
            bool selected = false;
            std::uint64_t entity = 0;
            HierarchyCategory category = HierarchyCategory::Other;
        };

        void Create();
        void SetLayout(float width, float height);
        void SetHierarchyRows(std::vector<HierarchyRow> rows);
        void SetSceneName(std::string sceneName);
        void SetSceneDirty(bool dirty) noexcept;
        void SetStatusText(std::string statusText);
        void SetSelectionName(std::string selectionName);
        void SetActiveTool(int toolIndex) noexcept;
        void SetHierarchyFilter(std::string filter);
        void SetGridVisible(bool visible) noexcept;
        void SetEnvironmentWorkspaceActive(bool active) noexcept;
        void SetTerrainWorkspaceActive(bool active) noexcept;
        void SetPanelSizes(
            float hierarchyWidth,
            float inspectorWidth,
            float drawerHeight) noexcept;
        void SetActiveBottomTab(int tab, bool notify = false);
        void OnHierarchySelected(std::function<void(std::uint64_t)> callback);
        void OnToolSelected(std::function<void(int)> callback);
        void OnAction(std::function<void(Action)> callback);
        void OnDrawerChanged(std::function<void(int)> callback);
        void OnLayoutChanged(
            std::function<void(float, float, float, bool)> callback);

        [[nodiscard]] XMFLOAT4 ViewportBounds() const noexcept;
        [[nodiscard]] float HierarchyWidth() const noexcept;
        [[nodiscard]] float InspectorWidth() const noexcept;
        [[nodiscard]] float DrawerHeight() const noexcept;
        [[nodiscard]] bool ConsumedPointerThisFrame() const noexcept;

        void Update(const wi::Canvas& canvas, float dt) override;
        void Render(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStudioChrome";
        }

    private:
        struct VisibleHierarchyItem
        {
            bool header = false;
            HierarchyCategory category = HierarchyCategory::Other;
            std::size_t rowIndex = 0;
        };

        float width_ = 1920.0f;
        float height_ = 1080.0f;
        float hierarchyWidth_ = 320.0f;
        float inspectorWidth_ = 360.0f;
        float drawerHeight_ = 238.0f;
        bool layoutInitialized_ = false;
        int resizingPanel_ = 0;
        int hoveredSplitter_ = 0;
        int activeTool_ = 1;
        int activeBottomTab_ = -1;
        int activeMenu_ = -1;
        int activeViewportMenu_ = -1;
        bool gridVisible_ = true;
        bool environmentWorkspaceActive_ = false;
        bool terrainWorkspaceActive_ = false;
        bool pointerConsumed_ = false;
        wi::Resource brandLockup_;
        std::vector<HierarchyRow> hierarchyRows_;
        std::vector<VisibleHierarchyItem> visibleHierarchyRows_;
        std::array<bool,
            static_cast<std::size_t>(HierarchyCategory::Count)>
            collapsedHierarchyCategories_ = {};
        std::size_t hierarchyScrollRow_ = 0;
        std::uint64_t lastHierarchySelection_ = 0;
        std::string sceneName_ = "PROVING GROUND";
        bool sceneDirty_ = false;
        std::string statusText_ = "STUDIO READY";
        std::string selectionName_;
        std::string hierarchyFilter_;
        float fpsSampleTime_ = 0.0f;
        float displayedFps_ = 0.0f;
        std::uint32_t fpsSampleFrames_ = 0;
        std::function<void(std::uint64_t)> hierarchySelected_;
        std::function<void(int)> toolSelected_;
        std::function<void(Action)> action_;
        std::function<void(int)> drawerChanged_;
        std::function<void(float, float, float, bool)> layoutChanged_;
    };
}
