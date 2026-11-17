#include "StudioModalWindow.h"

namespace renegade::studio
{
void StudioModalWindow::Create(const std::string& name, WindowControls controls)
{
        wi::gui::Window::Create(name, controls);
        wi::gui::Window::RemoveWidget(&scrollbar_vertical);
        wi::gui::Window::RemoveWidget(&scrollbar_horizontal);
        scrollbar_vertical.Detach(); scrollbar_horizontal.Detach();
        body_.Create(name + " content", WindowControls::DISABLE_TITLE_BAR);
        body_.SetControlSize(14);
        body_.SetPos({0,28});
        wi::gui::Window::AddWidget(&body_, AttachmentOptions::NONE);
    }

void StudioModalWindow::SetSize(const XMFLOAT2& value)
{ desired_=value; wi::gui::Window::SetSize(value); }

void StudioModalWindow::AddWidget(wi::gui::Widget* w)
{ body_.AddWidget(w); }

void StudioModalWindow::RemoveWidgets()
{ body_.RemoveWidgets(); wi::gui::Window::RemoveWidgets(); }

wi::gui::Window& StudioModalWindow::ContentWindow()
{ return body_; }

XMFLOAT2 StudioModalWindow::ContentOrigin() const
{ return body_.Origin(); }

void StudioModalWindow::PinFooter(wi::gui::Widget& w,float x,float y,bool right,bool stretch)
{
        const auto size=w.GetSize(); const bool visible=w.IsVisible(), enabled=w.IsEnabled();
        body_.RemoveWidget(&w); w.Detach();
        wi::gui::Window::AddWidget(&w,AttachmentOptions::NONE);
        w.SetVisible(visible); w.SetEnabled(enabled);
        footer_.push_back({&w,x,y,size.x,right,stretch});
    }

void StudioModalWindow::Update(const wi::Canvas& canvas,float dt)
{
        if(!IsVisible()) return;
        // One physical Escape belongs to one modal, even if closing it enables
        // another Window later in the same native GUI update.
        static bool escapeClaimed=false;
        const bool escape=wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE);
        if(!escape && !wi::input::Down(wi::input::KEYBOARD_BUTTON_ESCAPE))escapeClaimed=false;
        if(IsEnabled() && escape && !escapeClaimed)
        {
            escapeClaimed=true;SetVisible(false);
            if(onClose)onClose(wi::gui::EventArgs{});
            return;
        }
        const XMFLOAT2 available={float(canvas.GetLogicalWidth()),float(canvas.GetLogicalHeight())};
        const XMFLOAT2 size={std::min(desired_.x,std::max(1.f,available.x-16)),
                             std::min(desired_.y,std::max(1.f,available.y-16))};
        wi::gui::Window::SetSize(size);
        if(canvas_.x!=available.x || canvas_.y!=available.y)
        {
            SetPos({std::max(8.f,(available.x-size.x)*.5f),std::max(8.f,(available.y-size.y)*.5f)});
            canvas_=available;
        }
        else
        {
            auto p=GetPos(); SetPos({std::clamp(p.x,0.f,std::max(0.f,available.x-size.x)),
                                    std::clamp(p.y,0.f,std::max(0.f,available.y-size.y))});
        }
        const auto origin=GetPos();
        body_.Detach();
        body_.SetPos({origin.x,origin.y+GetControlSize()});
        body_.SetSize({size.x,std::max(1.f,size.y-GetControlSize()-FooterHeight)});
        body_.AttachTo(this);
        for(auto& f:footer_)
        {
            f.widget->Detach();
            f.widget->SetPos({origin.x+(f.right?size.x-f.x:f.x),origin.y+size.y-FooterHeight+f.y});
            // Re-establish pixel size before attaching to the scaled Window transform.
            const float height=dynamic_cast<wi::gui::Button*>(f.widget)?32.f:
                dynamic_cast<wi::gui::CheckBox*>(f.widget)?20.f:24.f;
            f.widget->SetSize({f.stretch?std::max(1.f,size.x-f.x*2):f.width,height});
            f.widget->AttachTo(this);
        }
        wi::gui::Window::Update(canvas,dt);
    }
}
