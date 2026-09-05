#include "RenegadeProjectLoadingOverlay.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>

namespace
{
    constexpr float DesignWidth = 1672.0f;
    constexpr float DesignHeight = 941.0f;

    constexpr wi::Color Background = wi::Color(3, 7, 10, 255);
    constexpr wi::Color Panel = wi::Color(7, 14, 20, 250);
    constexpr wi::Color Border = wi::Color(38, 67, 82, 240);
    constexpr wi::Color Cyan = wi::Color(95, 216, 255, 255);
    constexpr wi::Color CyanDim = wi::Color(44, 102, 128, 255);
    constexpr wi::Color Orange = wi::Color(238, 117, 26, 255);
    constexpr wi::Color Text = wi::Color(230, 239, 245, 255);
    constexpr wi::Color Muted = wi::Color(132, 153, 168, 255);
    constexpr wi::Color Warning = wi::Color(255, 174, 72, 255);

    void DrawRect(float x, float y, float width, float height, wi::Color color,
        wi::graphics::CommandList cmd)
    {
        if (width <= 0.0f || height <= 0.0f)
            return;
        wi::image::Params params(x, y, width, height, color);
        params.blendFlag = wi::enums::BLENDMODE_ALPHA;
        wi::image::Draw(nullptr, params, cmd);
    }

    void DrawText(const std::string& text, float x, float y, int size,
        wi::Color color, wi::graphics::CommandList cmd,
        wi::font::Alignment horizontal = wi::font::WIFALIGN_LEFT,
        float tracking = 0.8f, float bolden = 0.10f)
    {
        wi::font::Params params(
            x, y, size, horizontal, wi::font::WIFALIGN_TOP,
            color, wi::Color::Transparent());
        params.spacingX = tracking;
        params.bolden = bolden;
        wi::font::Draw(text, params, cmd);
    }
}

namespace renegade::studio
{
    void RenegadeProjectLoadingOverlay::Create()
    {
        SetName("Renegade project loading overlay");
        SetShadowRadius(0.0f);
        SetLayout(width_, height_);
        SetVisible(false);
    }

    void RenegadeProjectLoadingOverlay::SetLayout(const float width, const float height)
    {
        width_ = std::max(1.0f, width);
        height_ = std::max(1.0f, height);
        scale_ = std::max(0.01f, std::min(width_ / DesignWidth, height_ / DesignHeight));
        offsetX_ = (width_ - DesignWidth * scale_) * 0.5f;
        offsetY_ = (height_ - DesignHeight * scale_) * 0.5f;
        SetPos(XMFLOAT2(0.0f, 0.0f));
        SetSize(XMFLOAT2(width_, height_));
    }

    void RenegadeProjectLoadingOverlay::Begin(std::string projectLabel)
    {
        projectLabel_ = std::move(projectLabel);
        error_.clear();
        elapsed_ = 0.0f;
        readyElapsed_ = 0.0f;
        completed_.store(0, std::memory_order_relaxed);
        total_.store(0, std::memory_order_relaxed);
        phase_.store(static_cast<int>(Phase::ValidatingProject), std::memory_order_release);
        SetVisible(true);
    }

    void RenegadeProjectLoadingOverlay::SetPhase(
        const Phase phase, const std::size_t completed, const std::size_t total) noexcept
    {
        completed_.store(completed, std::memory_order_relaxed);
        total_.store(total, std::memory_order_relaxed);
        phase_.store(static_cast<int>(phase), std::memory_order_release);
    }

    void RenegadeProjectLoadingOverlay::Fail(std::string error)
    {
        error_ = std::move(error);
        completed_.store(0, std::memory_order_relaxed);
        total_.store(0, std::memory_order_relaxed);
        phase_.store(static_cast<int>(Phase::Failed), std::memory_order_release);
        SetVisible(true);
    }

    void RenegadeProjectLoadingOverlay::OnReturnToHub(std::function<void()> callback)
    {
        returnToHub_ = std::move(callback);
    }

    RenegadeProjectLoadingOverlay::Phase RenegadeProjectLoadingOverlay::CurrentPhase() const noexcept
    {
        return static_cast<Phase>(phase_.load(std::memory_order_acquire));
    }

    bool RenegadeProjectLoadingOverlay::IsBlocking() const noexcept
    {
        return IsVisible() && CurrentPhase() != Phase::Idle;
    }

    XMFLOAT4 RenegadeProjectLoadingOverlay::ReturnButtonBounds() const noexcept
    {
        return XMFLOAT4(
            offsetX_ + 650.0f * scale_,
            offsetY_ + 650.0f * scale_,
            offsetX_ + 1022.0f * scale_,
            offsetY_ + 712.0f * scale_);
    }

    float RenegadeProjectLoadingOverlay::PhaseProgress(
        const Phase phase, const std::size_t completed, const std::size_t total) const noexcept
    {
        switch (phase)
        {
        case Phase::ValidatingProject: return 0.08f;
        case Phase::PreparingScene: return 0.24f;
        case Phase::RestoringAssets:
            if (total == 0)
                return 0.35f;
            return 0.35f + 0.52f * std::clamp(
                static_cast<float>(completed) / static_cast<float>(total), 0.0f, 1.0f);
        case Phase::Finalising: return 0.94f;
        case Phase::Ready: return 1.0f;
        case Phase::Failed: return 0.0f;
        case Phase::Idle:
        default: return 0.0f;
        }
    }

    std::string RenegadeProjectLoadingOverlay::PhaseMessage(const Phase phase) const
    {
        switch (phase)
        {
        case Phase::ValidatingProject:
            return "READING THE FINE PRINT...";
        case Phase::PreparingScene:
            return "ASSEMBLING REALITY...";
        case Phase::RestoringAssets:
        {
            static constexpr std::array<const char*, 4> Messages = {{
                "PUTTING THE PAINT BACK ON...",
                "ROUNDING UP YOUR ASSETS...",
                "CONVINCING THE MATERIALS TO COOPERATE...",
                "LOCATING THOSE TEXTURES YOU SWEAR YOU PACKAGED...",
            }};
            const std::size_t completed = completed_.load(std::memory_order_relaxed);
            return Messages[(completed / 3u) % Messages.size()];
        }
        case Phase::Finalising:
            return "ARGUING WITH THE LAST FEW BYTES...";
        case Phase::Ready:
            return "READY. GO BREAK SOMETHING.";
        case Phase::Failed:
            return "WELL. THAT WASN'T SUPPOSED TO HAPPEN.";
        case Phase::Idle:
        default:
            return {};
        }
    }

    std::string RenegadeProjectLoadingOverlay::PhaseDetail(
        const Phase phase, const std::size_t completed, const std::size_t total) const
    {
        switch (phase)
        {
        case Phase::ValidatingProject:
            return "Checking the project descriptor before we touch your current world.";
        case Phase::PreparingScene:
            return "Waking up the world. This bit is Wicked doing serious paperwork.";
        case Phase::RestoringAssets:
            if (total > 0)
            {
                std::ostringstream out;
                out << completed << " / " << total
                    << " governed resources prepared from project metadata.";
                return out.str();
            }
            return "Checking behind the sofa for governed resources.";
        case Phase::Finalising:
            return "Putting everything back where you left it.";
        case Phase::Ready:
            return "Project online. Editor handoff imminent.";
        case Phase::Failed:
            return error_.empty()
                ? "The project could not be loaded. Your previous project is still safe."
                : error_;
        case Phase::Idle:
        default:
            return {};
        }
    }

    void RenegadeProjectLoadingOverlay::Update(const wi::Canvas& canvas, const float dt)
    {
        Widget::Update(canvas, dt);
        if (!IsVisible())
            return;

        // READY relinquishes blocking ownership only after the current Level
        // Editor frame has already been covered. The Story Flow integration
        // runs after Application::Run(), sees Idle, and switches render paths
        // for the following frame. If the Level Editor is deliberately entered
        // later, this stale visual cover removes itself before that frame renders.
        const Phase phase = CurrentPhase();
        if (phase == Phase::Idle)
        {
            SetVisible(false);
            return;
        }

        elapsed_ += dt;
        if (phase == Phase::Ready)
        {
            readyElapsed_ += dt;
            if (readyElapsed_ >= 0.85f)
            {
                phase_.store(static_cast<int>(Phase::Idle), std::memory_order_release);
            }
            return;
        }
        readyElapsed_ = 0.0f;

        if (phase != Phase::Failed ||
            !wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
            return;

        const XMFLOAT4 pointer = wi::input::GetPointer();
        const XMFLOAT4 button = ReturnButtonBounds();
        if (pointer.x >= button.x && pointer.x <= button.z &&
            pointer.y >= button.y && pointer.y <= button.w && returnToHub_)
        {
            returnToHub_();
        }
    }

    void RenegadeProjectLoadingOverlay::Render(
        const wi::Canvas& canvas, const wi::graphics::CommandList cmd) const
    {
        if (!IsVisible())
            return;

        Widget::Render(canvas, cmd);
        const auto sx = [this](const float x) { return offsetX_ + x * scale_; };
        const auto sy = [this](const float y) { return offsetY_ + y * scale_; };
        const auto ss = [this](const float value) { return value * scale_; };

        DrawRect(0.0f, 0.0f, width_, height_, Background, cmd);

        // Deliberately sparse: this is a Renegade machine room, not another Hub.
        DrawText("RENEGADE // PROJECT LOADER", sx(78.0f), sy(66.0f),
            static_cast<int>(18.0f * scale_), Cyan, cmd);
        DrawText("BUILD WITHOUT PERMISSION", sx(1594.0f), sy(68.0f),
            static_cast<int>(13.0f * scale_), Muted, cmd, wi::font::WIFALIGN_RIGHT);
        DrawRect(sx(78.0f), sy(104.0f), ss(1516.0f), ss(1.0f), Border, cmd);

        const float panelX = sx(238.0f);
        const float panelY = sy(260.0f);
        const float panelW = ss(1196.0f);
        const float panelH = ss(428.0f);
        DrawRect(panelX, panelY, panelW, panelH, Border, cmd);
        DrawRect(panelX + ss(1.0f), panelY + ss(1.0f),
            panelW - ss(2.0f), panelH - ss(2.0f), Panel, cmd);

        // Idle while still visible is the one-frame project-home handoff state.
        // Keep presenting the completed loader rather than exposing the 3D
        // editor or flashing an empty overlay while Story Flow takes ownership.
        const Phase storedPhase = CurrentPhase();
        const Phase phase = storedPhase == Phase::Idle ? Phase::Ready : storedPhase;
        const std::size_t completed = completed_.load(std::memory_order_relaxed);
        const std::size_t total = total_.load(std::memory_order_relaxed);
        const float progress = PhaseProgress(phase, completed, total);

        const std::string label = projectLabel_.empty() ? "PROJECT" : projectLabel_;
        DrawText(label, sx(296.0f), sy(306.0f), static_cast<int>(15.0f * scale_),
            Muted, cmd);
        DrawText(PhaseMessage(phase), sx(296.0f), sy(354.0f),
            static_cast<int>(31.0f * scale_),
            phase == Phase::Failed ? Warning : Text, cmd);
        DrawText(PhaseDetail(phase, completed, total), sx(296.0f), sy(416.0f),
            static_cast<int>(15.0f * scale_), Muted, cmd);

        const float barX = sx(296.0f);
        const float barY = sy(506.0f);
        const float barW = ss(1080.0f);
        const float barH = ss(18.0f);
        DrawRect(barX, barY, barW, barH, CyanDim, cmd);
        if (phase != Phase::Failed)
            DrawRect(barX, barY, barW * progress, barH, Orange, cmd);

        const float scanner = std::fmod(elapsed_ * 190.0f, 1080.0f);
        if (phase != Phase::Ready && phase != Phase::Failed)
            DrawRect(barX + ss(scanner), barY - ss(4.0f), ss(2.0f), barH + ss(8.0f), Cyan, cmd);

        std::ostringstream percent;
        percent << static_cast<int>(std::round(progress * 100.0f)) << "%";
        DrawText(percent.str(), sx(1376.0f), sy(542.0f),
            static_cast<int>(14.0f * scale_), Text, cmd, wi::font::WIFALIGN_RIGHT);
        DrawText("REAL PIPELINE TELEMETRY // NO FAKE TIMER", sx(296.0f), sy(548.0f),
            static_cast<int>(12.0f * scale_), CyanDim, cmd);

        if (phase == Phase::Failed)
        {
            const XMFLOAT4 button = ReturnButtonBounds();
            DrawRect(button.x, button.y, button.z - button.x, button.w - button.y, Orange, cmd);
            DrawRect(button.x + ss(2.0f), button.y + ss(2.0f),
                button.z - button.x - ss(4.0f), button.w - button.y - ss(4.0f), Panel, cmd);
            DrawText("RETURN TO HUB", (button.x + button.z) * 0.5f, button.y + ss(18.0f),
                static_cast<int>(16.0f * scale_), Text, cmd, wi::font::WIFALIGN_CENTER);
        }

        DrawText("REN//LOAD", sx(78.0f), sy(872.0f), static_cast<int>(12.0f * scale_),
            CyanDim, cmd);
    }
}
