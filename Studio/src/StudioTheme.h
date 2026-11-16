#pragma once
#include <wiColor.h>

namespace renegade::studio::theme
{
// Studio's existing workspace palette and native font hierarchy. All editor
// surfaces are opaque; authored in-game presentation is a separate setting.
inline constexpr wi::Color Panel{8, 12, 16, 255};
inline constexpr wi::Color Idle{12, 16, 19, 255};
inline constexpr wi::Color Focus{26, 31, 35, 255};
inline constexpr wi::Color Active{38, 43, 47, 255};
inline constexpr wi::Color Text{244, 244, 244, 255};
inline constexpr wi::Color Muted{178, 178, 176, 255};
inline constexpr wi::Color Border{38, 52, 61, 255};
inline constexpr wi::Color Selected{44, 35, 29, 255};
inline constexpr wi::Color Accent{222, 91, 29, 255};
inline constexpr int HeadingSize = 16;
inline constexpr int ControlSize = 14;
inline constexpr int ScriptSize = 18;
inline constexpr int CaptionSize = 13;
} // namespace renegade::studio::theme
