#pragma once

#include <string>
#include <vector>

#include <WickedEngine.h>

namespace renegade::studio
{
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
        };

        void Create();
        void SetLayout(float width, float height);
        void SetHierarchyRows(std::vector<HierarchyRow> rows);
        void SetSceneName(std::string sceneName);
        void SetStatusText(std::string statusText);
        void SetSelectionName(std::string selectionName);
        void SetActiveTool(int toolIndex) noexcept;

        [[nodiscard]] XMFLOAT4 ViewportBounds() const noexcept;

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
        int activeTool_ = 1;
        std::vector<HierarchyRow> hierarchyRows_;
        std::string sceneName_ = "PROVING GROUND";
        std::string statusText_ = "STUDIO READY";
        std::string selectionName_;
    };
}
