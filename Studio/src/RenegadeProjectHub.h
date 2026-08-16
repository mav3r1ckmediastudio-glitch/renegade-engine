#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <WickedEngine.h>

namespace renegade::studio
{
    class RenegadeProjectHub final : public wi::gui::Widget
    {
    public:
        struct ProjectEntry
        {
            std::string name;
            std::string descriptorPath;
            std::string rootPath;
            std::string startupScene;
            std::uint32_t formatVersion = 1;
            bool descriptorValid = true;
        };

        enum class Action
        {
            NewProject,
            OpenProject,
            OpenSelectedProject,
            BackToEditor,
            ExitRenegade,
            CancelNewProject,
        };

        void Create();
        void SetLayout(float width, float height);
        void SetDeveloperIdentity(std::string identity);
        void SetProjects(std::vector<ProjectEntry> projects, int selectedIndex);
        void SetSelectedIndex(int selectedIndex) noexcept;
        void SetCurrentProject(std::string name, bool active);
        void SetStatusText(std::string text);
        void SetStatusProvider(std::function<std::string()> provider);
        void SetNewProjectMode(bool active) noexcept;
        void OnAction(std::function<void(Action)> callback);
        void OnRecentProjectSelected(std::function<void(std::size_t)> callback);

        [[nodiscard]] bool ConsumedPointerThisFrame() const noexcept;
        [[nodiscard]] XMFLOAT4 NewProjectInputBounds() const noexcept;
        [[nodiscard]] XMFLOAT4 NewProjectConfirmBounds() const noexcept;
        [[nodiscard]] XMFLOAT4 NewProjectCancelBounds() const noexcept;

        void Update(const wi::Canvas& canvas, float dt) override;
        void Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override
        {
            return "RenegadeProjectHub";
        }

    private:
        enum class HoverTarget
        {
            None,
            NewProject,
            OpenProject,
            BackToEditor,
            ExitRenegade,
            FeaturedProject,
            OpenSelected,
            LowerNewProject,
            Recent0,
            Recent1,
            Recent2,
        };

        [[nodiscard]] XMFLOAT2 ToBase(float x, float y) const noexcept;
        [[nodiscard]] XMFLOAT4 ToScreen(const XMFLOAT4& baseRect) const noexcept;
        [[nodiscard]] bool ContainsBase(
            const XMFLOAT2& point,
            const XMFLOAT4& rect) const noexcept;
        [[nodiscard]] HoverTarget ResolveHover(const XMFLOAT2& basePointer) const noexcept;
        void Invoke(Action action);

        float width_ = 1920.0f;
        float height_ = 1080.0f;
        float scale_ = 1.0f;
        float offsetX_ = 0.0f;
        float offsetY_ = 0.0f;
        bool pointerConsumed_ = false;
        bool currentProjectActive_ = false;
        bool newProjectMode_ = false;
        int selectedIndex_ = -1;
        HoverTarget hovered_ = HoverTarget::None;
        wi::Resource conceptPlate_;
        std::string developerIdentity_ = "DEVELOPER";
        std::string currentProjectName_;
        std::string statusText_ = "PROJECT SERVICES // ONLINE";
        std::vector<ProjectEntry> projects_;
        std::function<std::string()> statusProvider_;
        std::function<void(Action)> action_;
        std::function<void(std::size_t)> recentProjectSelected_;
    };
}
