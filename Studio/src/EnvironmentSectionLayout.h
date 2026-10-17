#pragma once
#include <cstddef>

namespace renegade::studio
{
    struct EnvironmentSectionBounds
    {
        float headerTop;
        float contentTop;
        float nextTop;
    };

    // Shared by the native widget layout and its exhaustive disclosure test.
    constexpr EnvironmentSectionBounds MeasureEnvironmentSection(
        float top, bool expanded, std::size_t childCount) noexcept
    {
        const float content = top + 34.0f;
        return {top, content, content + (expanded ? childCount * 34.0f : 0.0f)};
    }
}
