#pragma once
#include "StudioTheme.h"
#include <WickedEngine.h>
#include <algorithm>
#include <vector>

namespace renegade::studio
{
// Readable content scrolls independently of the always-reachable action row.
// Native Window owns drawing, pointer clipping, focus and both scrollbars.
class StudioModalWindow : public wi::gui::Window
{
    class Body : public wi::gui::Window
    {
    public:
        XMFLOAT2 Origin() const { return {scrollable_area.translation.x, scrollable_area.translation.y}; }
    } body_;
    struct Footer { wi::gui::Widget* widget; float x, y, width; bool right, stretch; };
    std::vector<Footer> footer_;
    XMFLOAT2 desired_{1020,840}, canvas_{};
    static constexpr float FooterHeight = 76;
public:
    void Create(const std::string& name, WindowControls controls);
    void SetSize(const XMFLOAT2& value);
    void AddWidget(wi::gui::Widget* w);
    void RemoveWidgets();
    wi::gui::Window& ContentWindow();
    XMFLOAT2 ContentOrigin() const;
    void PinFooter(wi::gui::Widget& w,float x,float y,bool right=false,bool stretch=false);
    void Update(const wi::Canvas& canvas,float dt) override;
};
}
