#pragma once

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace renegade::studio
{
    struct InspectorMessageLayout
    {
        float validationY = 0.0f;
        float statusY = 0.0f;
        float validationBottom = 0.0f;
        float statusBottom = 0.0f;
    };

    [[nodiscard]] inline std::size_t InspectorWrapCharacterLimit(
        const float availableWidth,
        const int fontSize) noexcept
    {
        const float approximateCharacterWidth =
            static_cast<float>(std::max(1, fontSize)) * 0.62f;
        return std::max<std::size_t>(18,
            static_cast<std::size_t>(
                std::max(1.0f, availableWidth) /
                approximateCharacterWidth));
    }

    // Preserve the complete creator-facing message. Ordinary text wraps at
    // word boundaries; a single long identifier/path is split rather than
    // clipped or replaced with an ellipsis.
    [[nodiscard]] inline std::vector<std::string> WrapInspectorText(
        const std::string& value,
        const std::size_t requestedLimit)
    {
        const std::size_t limit = std::max<std::size_t>(1, requestedLimit);
        std::vector<std::string> lines;
        std::istringstream words(value);
        std::string line;
        std::string word;

        const auto flushLine = [&]()
        {
            if (!line.empty())
            {
                lines.push_back(line);
                line.clear();
            }
        };

        while (words >> word)
        {
            if (!line.empty() && line.size() + 1 + word.size() <= limit)
            {
                line += ' ';
                line += word;
                continue;
            }

            flushLine();
            while (word.size() > limit)
            {
                lines.push_back(word.substr(0, limit));
                word.erase(0, limit);
            }
            line = std::move(word);
        }
        flushLine();

        if (lines.empty())
            lines.emplace_back();
        return lines;
    }

    [[nodiscard]] inline InspectorMessageLayout ComputeInspectorMessageLayout(
        const bool graphView,
        const float requestedValidationY,
        const float validationBlockHeight,
        const float inspectorBottom,
        const std::size_t statusLineCount) noexcept
    {
        constexpr float lineHeight = 17.0f;
        constexpr float blockGap = 14.0f;
        const float safeStatusLineCount = static_cast<float>(
            std::max<std::size_t>(1, statusLineCount));
        const float statusBlockHeight =
            17.0f + safeStatusLineCount * lineHeight;

        InspectorMessageLayout layout;
        layout.validationY = requestedValidationY;
        layout.statusY = inspectorBottom - 90.0f -
            (safeStatusLineCount - 1.0f) * lineHeight;
        if (graphView)
        {
            layout.statusY = layout.validationY +
                validationBlockHeight + 24.0f;
        }
        else if (layout.validationY + validationBlockHeight >
            layout.statusY - blockGap)
        {
            layout.validationY = layout.statusY -
                blockGap - validationBlockHeight;
        }

        layout.validationBottom =
            layout.validationY + validationBlockHeight;
        layout.statusBottom = layout.statusY + statusBlockHeight;
        return layout;
    }
}
