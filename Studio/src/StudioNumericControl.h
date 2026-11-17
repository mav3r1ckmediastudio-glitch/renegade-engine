#pragma once
#include "StudioTheme.h"
#include <WickedEngine.h>
#include <algorithm>

namespace renegade::studio
{
class StudioNumericSlider : public wi::gui::Slider
{
    mutable unsigned helpFrames_=0;
public:
void RenderTooltip(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const override;
};
// Pickers retain their native one-line geometry and callbacks. Only the rail,
// numeric face and text roles change; all range/quantization semantics remain.
class StudioPickerSlider : public StudioNumericSlider
{
public:
void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const override;
};
}
