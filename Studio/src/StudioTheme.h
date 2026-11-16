#pragma once
#include <wiColor.h>

namespace renegade::studio::theme
{
// Studio's existing workspace palette and native font hierarchy. All editor
// surfaces are opaque; authored in-game presentation is a separate setting.
inline constexpr wi::Color Canvas{17, 19, 21, 255};
inline constexpr wi::Color Panel{35, 38, 41, 255};
inline constexpr wi::Color Idle{25, 28, 31, 255};
inline constexpr wi::Color Control{48, 52, 55, 255};
inline constexpr wi::Color Focus{58, 66, 72, 255};
inline constexpr wi::Color Active{133, 52, 16, 255};
inline constexpr wi::Color Text{244, 244, 244, 255};
inline constexpr wi::Color Muted{178, 178, 176, 255};
inline constexpr wi::Color Border{74, 81, 86, 255};
inline constexpr wi::Color Selected{56, 40, 31, 255};
inline constexpr wi::Color Accent{222, 91, 29, 255};
inline constexpr wi::Color AccentHover{246, 117, 48, 255};
inline constexpr wi::Color Disabled{41, 45, 48, 255};
inline constexpr wi::Color DisabledText{129, 138, 144, 255};
inline constexpr int HeadingSize = 16;
inline constexpr int ControlSize = 14;
inline constexpr int ScriptSize = 18;
inline constexpr int CaptionSize = 13;
} // namespace renegade::studio::theme
