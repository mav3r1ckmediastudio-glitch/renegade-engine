#pragma once

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
        void Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override { return "RenegadeTextInputField"; }
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

    // Renegade-owned rendering for the Studio shell. Wicked supplies the
    // canvas and command list, but no stock wiGUI control draws this chrome.
    // Interactive components can be migrated onto this visual foundation one
    // bounded slice at a time after the product owner accepts the proof.
    class RenegadeStudioChrome final : public wi::gui::Widget
    {
    public:
        struct HierarchyRow
        {
            std::string name;
            int depth = 0;
            bool selected = false;
            std::uint64_t entity = 0;
        };

        void Create();
        void SetLayout(float width, float height);
        void SetHierarchyRows(std::vector<HierarchyRow> rows);
        void SetSceneName(std::string sceneName);
        void SetStatusText(std::string statusText);
        void SetSelectionName(std::string selectionName);
        void SetActiveTool(int toolIndex) noexcept;
        void SetHierarchyFilter(std::string filter);
        void OnHierarchySelected(std::function<void(std::uint64_t)> callback);
        void OnToolSelected(std::function<void(int)> callback);

        [[nodiscard]] XMFLOAT4 ViewportBounds() const noexcept;
        [[nodiscard]] float InspectorWidth() const noexcept;

        void Update(const wi::Canvas& canvas, float dt) override;
        void Render(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStudioChrome";
        }

    private:
        float width_ = 1920.0f;
        float height_ = 1080.0f;
        float hierarchyWidth_ = 320.0f;
        float inspectorWidth_ = 360.0f;
        int activeTool_ = 1;
        int activeBottomTab_ = -1;
        std::vector<HierarchyRow> hierarchyRows_;
        std::vector<std::size_t> visibleHierarchyRows_;
        std::string sceneName_ = "PROVING GROUND";
        std::string statusText_ = "STUDIO READY";
        std::string selectionName_;
        std::string hierarchyFilter_;
        std::function<void(std::uint64_t)> hierarchySelected_;
        std::function<void(int)> toolSelected_;
    };
}
