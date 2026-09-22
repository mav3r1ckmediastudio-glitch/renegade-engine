#pragma once

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>
#include <WickedEngine.h>

namespace renegade::studio
{
    // Native presentation surface for the approved five-card animation workflow.
    // All actual inputs are ordinary Wicked widgets owned by StudioApplication.
    class CreatorImportAnimationDashboard final : public wi::gui::Widget
    {
    public:
        CreatorImportAnimationDashboard()
        {
            SetName("Importer Animation Dashboard");
            SetShadowRadius(0.0f);
            SetEnabled(false);
        }
        void SetLayout(float sourceHeight, float clipsTop, float clipsHeight,
            float previewTop, float assignmentsTop, float validationTop) noexcept
        {
            sourceHeight_ = sourceHeight;
            clipsTop_ = clipsTop;
            clipsHeight_ = clipsHeight;
            previewTop_ = previewTop;
            assignmentsTop_ = assignmentsTop;
            validationTop_ = validationTop;
        }
        void SetSources(std::vector<std::string> names, std::vector<std::size_t> counts)
        {
            sources_ = std::move(names);
            sourceClipCounts_ = std::move(counts);
        }
        void SetSelectedClip(std::string name, std::string source, float duration,
            float rangeStart = 0.0f, float rangeEnd = 1.0f)
        {
            clipName_ = std::move(name);
            clipSource_ = std::move(source);
            clipDuration_ = duration;
            trimStart_ = std::clamp(rangeStart, 0.0f, 1.0f);
            trimEnd_ = std::clamp(rangeEnd, trimStart_, 1.0f);
        }
        void SetAssignments(std::array<std::string, 6> assignments,
            std::array<int, 6> counts, std::string validation,
            std::string validationDetail)
        {
            assignments_ = std::move(assignments);
            assignmentCounts_ = counts;
            validation_ = std::move(validation);
            validationDetail_ = std::move(validationDetail);
        }
        void Render(const wi::Canvas& canvas, const wi::graphics::CommandList cmd) const override
        {
            if (!IsVisible()) return;
            ApplyScissor(canvas, scissorRect, cmd);
            const float x = translation.x, y = translation.y, w = scale.x;
            const auto rect = [cmd](float left, float top, float width, float height, wi::Color color)
            {
                if (width <= 0.0f || height <= 0.0f) return;
                wi::image::Params p(left, top, width, height, color);
                p.blendFlag = wi::enums::BLENDMODE_ALPHA;
                wi::image::Draw(nullptr, p, cmd);
            };
            const auto rounded = [cmd](float left, float top,
                float width, float height, wi::Color color, float radius)
            {
                if (width <= 0.0f || height <= 0.0f) return;
                wi::image::Params p(left, top, width, height, color);
                p.blendFlag = wi::enums::BLENDMODE_ALPHA;
                p.enableCornerRounding();
                for (auto& corner : p.corners_rounding)
                {
                    corner.radius = radius;
                    corner.segments = 6;
                }
                wi::image::Draw(nullptr, p, cmd);
            };
            const auto text = [cmd](const std::string& value, float tx, float ty,
                int size, wi::Color color, float weight = 0.1f)
            {
                wi::font::Params p(tx, ty, size, wi::font::WIFALIGN_LEFT,
                    wi::font::WIFALIGN_TOP, color, wi::Color::Transparent());
                p.bolden = weight;
                wi::font::Draw(value, p, cmd);
            };
            const wi::Color border(53, 67, 77, 255);
            const wi::Color strong(237, 240, 243, 255);
            const wi::Color muted(162, 176, 187, 255);
            const wi::Color accent(255, 140, 77, 255);
            const auto panel = [&](float top, float height, const char* heading)
            {
                // Back shadow, recessed outer lip, highlight, inset and raised header.
                rounded(x + 4, y + top + 5, w - 7, height,
                    wi::Color(3, 7, 11, 220), 9.0f);
                rounded(x, y + top, w, height, border, 8.0f);
                rounded(x + 1, y + top + 1, w - 2, height - 2,
                    wi::Color(17, 25, 31, 255), 7.0f);
                rect(x + 2, y + top + 2, w - 4, 34, wi::Color(32, 42, 50, 255));
                rect(x + 2, y + top + 35, w - 4, 1, wi::Color(8, 13, 17, 255));
                rect(x + 2, y + top + 2, w - 4, 1, wi::Color(65, 79, 88, 255));
                text(heading, x + 13, y + top + 11, 14, strong, 0.22f);
            };
            panel(0, sourceHeight_, "1.  ANIMATION SOURCES");
            for (std::size_t i = 0; i < std::min<std::size_t>(2, sources_.size()); ++i)
            {
                const float top = 42.0f + static_cast<float>(i) * 47.0f;
                rounded(x + 10, y + top + 2, w - 20, 42,
                    wi::Color(8, 13, 18, 255), 6.0f);
                rounded(x + 10, y + top, w - 20, 42,
                    wi::Color(47, 60, 69, 255), 6.0f);
                rounded(x + 11, y + top + 1, w - 22, 40,
                    wi::Color(25, 35, 43, 255), 5.0f);
                rect(x + 18, y + top + 10, 18, 22, wi::Color(115, 135, 149, 255));
                rect(x + 30, y + top + 10, 6, 6, wi::Color(39, 52, 61, 255));
                rect(x + 22, y + top + 20, 10, 1, wi::Color(42, 56, 67, 255));
                rect(x + 22, y + top + 24, 10, 1, wi::Color(42, 56, 67, 255));
                const std::size_t limit = std::max<std::size_t>(12, static_cast<std::size_t>((w - 92) / 8));
                const std::string name = sources_[i].size() > limit
                    ? sources_[i].substr(0, limit - 3) + "..." : sources_[i];
                text(name, x + 45, y + top + 6, 13, strong);
                const std::size_t count = i < sourceClipCounts_.size() ? sourceClipCounts_[i] : 0;
                text((i == 0 ? "Embedded" : "External") + std::string(" source  |  ") +
                    std::to_string(count) + " clips", x + 45, y + top + 23, 11, muted);
            }
            if (sources_.size() > 2)
                text("+ " + std::to_string(sources_.size() - 2) + " more sources (see clip list)",
                    x + 13, y + 147, 11, muted);
            panel(clipsTop_, clipsHeight_, "2.  AVAILABLE CLIPS");
            panel(previewTop_, 330, "3.  CLIP PREVIEW & PROPERTIES");
            text("Selected clip", x + 13, y + previewTop_ + 44, 11, muted);
            text("Source: " + clipSource_, x + 13, y + previewTop_ + 95, 11, muted);
            char duration[48];
            std::snprintf(duration, sizeof(duration), "%.2fs", clipDuration_);
            text(std::string("Duration: ") + duration, x + w - 120, y + previewTop_ + 95, 11, muted);
            rect(x + 13, y + previewTop_ + 111, w - 26, 11, wi::Color(8, 13, 18, 255));
            rect(x + 16, y + previewTop_ + 114, w - 32, 5, wi::Color(95, 60, 44, 255));
            rect(x + 16 + (w - 32) * trimStart_, y + previewTop_ + 114,
                std::max(2.0f, (w - 32) * (trimEnd_ - trimStart_)), 5, accent);
            text("TRIM START / END", x + 13, y + previewTop_ + 133, 10, muted);
            panel(assignmentsTop_, 329, "4.  CHARACTER ACTION ASSIGNMENTS");
            text("Action", x + 14, y + assignmentsTop_ + 43, 11, muted);
            text("Assigned clip", x + w * 0.41f, y + assignmentsTop_ + 43, 11, muted);
            constexpr std::array<const char*, 6> names = {
                "Idle", "Walk", "Run", "Melee Attack", "Hit Reaction", "Death"};
            for (std::size_t i = 0; i < names.size(); ++i)
            {
                const float top = assignmentsTop_ + 65.0f + i * 35.0f;
                if (i % 2 == 0) rect(x + 9, y + top - 2, w - 18, 35,
                    wi::Color(25, 35, 43, 255));
                rect(x + 10, y + top + 32, w - 20, 1, wi::Color(43, 55, 64, 255));
                text(std::string(names[i]) + (assignmentCounts_[i] > 1
                    ? " (" + std::to_string(assignmentCounts_[i]) + ")" : ""),
                    x + 17, y + top + 7, 12, strong);
            }
            panel(validationTop_, 93, "5.  VALIDATION");
            const bool needsAttention = validation_.find("No gameplay") != std::string::npos ||
                validation_.find("Duplicate") != std::string::npos;
            rect(x + 14, y + validationTop_ + 43, 18, 18, needsAttention
                ? wi::Color(145, 92, 39, 255) : wi::Color(42, 119, 78, 255));
            text(needsAttention ? "!" : "OK", x + 16, y + validationTop_ + 44, 11, strong);
            text(validation_, x + 42, y + validationTop_ + 44, 11, strong);
            text(validationDetail_, x + 42, y + validationTop_ + 66, 10, muted);
        }
    private:
        float sourceHeight_ = 173, clipsTop_ = 183, clipsHeight_ = 302;
        float previewTop_ = 495, assignmentsTop_ = 835, validationTop_ = 1176;
        std::vector<std::string> sources_;
        std::vector<std::size_t> sourceClipCounts_;
        std::array<std::string, 6> assignments_{};
        std::array<int, 6> assignmentCounts_{};
        std::string clipName_, clipSource_, validation_, validationDetail_;
        float clipDuration_ = 0.0f;
        float trimStart_ = 0.0f, trimEnd_ = 1.0f;
    };
}
