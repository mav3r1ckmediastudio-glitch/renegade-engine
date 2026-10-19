#pragma once

#include "RenegadeStudioChrome.h"

#include <array>
#include <cstddef>
#include <chrono>
#include <functional>
#include <future>
#include <string>
#include <string_view>
#include <vector>

namespace renegade::studio
{
    // Original low-cost digital-face placeholder for the in-editor companion.
    // A later visual gate can replace this with authored art and animation.
    class RenegadeAssistantPortrait final : public wi::gui::Widget
    {
    public:
        void Update(const wi::Canvas& canvas, float dt) override;
        void Render(const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override
        {
            return "RenegadeAssistantPortrait";
        }
    private:
        float phase_ = 0.0f;
    };

    // The first Renegade Assistant milestone is intentionally read-only:
    // a native docked chat, a compact digital manifestation, and a stepwise
    // guide based on existing Studio inspector controls. There is no editor
    // command execution or unverified local-LLM transport in this milestone.
    class RenegadeAssistantDock final
    {
    public:
        struct ChatEntry
        {
            bool fromUser = false;
            std::string text;
        };

        void Create();
        void SetLayout(float x, float y, float width, float height);
        void SetOpen(bool open);
        [[nodiscard]] bool IsOpen() const noexcept { return open_; }
        [[nodiscard]] wi::gui::Window& Panel() noexcept { return panel_; }
        void OnOpenChanged(std::function<void(bool)> changed);
        void SubmitText(std::string text);
        void Tick();
        void StartRetargetGuide();
        void NextGuideStep();
        void PreviousGuideStep();
        [[nodiscard]] int GuideStep() const noexcept { return guideStep_; }
        [[nodiscard]] const std::vector<ChatEntry>& History() const noexcept
        {
            return history_;
        }

    private:
        struct GuideInfo
        {
            const char* heading;
            const char* text;
            const char* evidence;
        };

        static constexpr std::array<GuideInfo, 4> RetargetSteps = {{
            {
                "Find the source and target rigs",
                "Select the target humanoid in the Level Editor. In its "
                "Inspector, expand HUMANOID / RETARGET. Confirm the rig "
                "information and identify the source animation file.",
                "Phase7Gate7BHumanoidRetargetInspector.cpp"
            },
            {
                "Map the target skeleton",
                "Use AUTO-MAP to propose humanoid bone mappings. Inspect "
                "the Bone Slot and Bone Target controls. Correct any "
                "wrong or missing mapping before importing animation.",
                "HumanoidRetargetInspector::Register"
            },
            {
                "Import and retarget a clip",
                "Choose IMPORT + RETARGET and select the source animation "
                "file. Supported source labels include WISCENE, FBX, "
                "GLTF, GLB, VRM and VRMA. Check the reported outcome.",
                "HumanoidRetargetInspector::BrowseAnimationSource"
            },
            {
                "Review and test the result",
                "Inspect the clips associated with the rig. Open the "
                "ANIMATION section to play or scrub a retargeted clip. "
                "Check wrist positions and the visible pose yourself.",
                "Phase7Gate7AAnimationInspector.cpp"
            },
        }};

        void SubmitInput();
        void RequestLocalQwenReply();
        static std::string AskLocalQwen(std::vector<ChatEntry> history);
        void AddEntry(bool fromUser, std::string text);
        void RefreshVisibleHistory();
        void RefreshGuide();
        void RefreshBubble();
        void SaveLocalState() const;
        void LoadLocalState();
        [[nodiscard]] static std::string Wrap(
            std::string_view text, std::size_t maximumColumns, std::size_t maximumLines);
        [[nodiscard]] static std::string Lower(std::string text);

        wi::gui::Window panel_;
        RenegadeAssistantPortrait portrait_;
        wi::gui::Label title_;
        wi::gui::Label subtitle_;
        wi::gui::Label bubble_;
        wi::gui::Label guideTitle_;
        wi::gui::Label guideBody_;
        wi::gui::Label guideEvidence_;
        wi::gui::Label historyTitle_;
        std::array<wi::gui::Label, 3> historyCards_;
        RenegadeTextInputField input_;
        RenegadeButton send_;
        RenegadeButton close_;
        RenegadeButton guideStart_;
        RenegadeButton guidePrevious_;
        RenegadeButton guideNext_;
        RenegadeButton older_;
        RenegadeButton newer_;
        bool open_ = false;
        int guideStep_ = -1;
        std::size_t historyFirst_ = 0;
        float layoutWidth_ = 320.0f;
        float layoutHeight_ = 680.0f;
        std::string bubbleText_;
        std::vector<ChatEntry> history_;
        std::function<void(bool)> openChanged_;
        std::future<std::string> pendingReply_;
    };
}
