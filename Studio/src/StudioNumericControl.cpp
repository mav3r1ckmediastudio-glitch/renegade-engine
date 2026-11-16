#include "StudioNumericControl.h"

namespace renegade::studio
{
void StudioNumericSlider::RenderTooltip(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const
{
        if(!IsVisible() || !IsEnabled() || valueInputField.GetState()==wi::gui::IDLE)
        { helpFrames_=0; return; }
        if(++helpFrames_<25) return;
        auto text=valueInputField.GetTooltip();
        if(text.empty() || text.find("FLT_MAX")!=std::string::npos)
            text="Type an exact value and press Enter.\nExpressions: 10 * 2 or 1 + 0.5.\nType reset to restore the initial value.\nAdvanced float constants are also accepted.";
        const float width=std::max(1.f,std::min(340.f,float(canvas.GetLogicalWidth())-24));
        wi::font::Params fp(0,0,theme::CaptionSize);fp.h_wrap=width-20;fp.color=theme::Text;
        const float height=wi::font::TextHeight(text,fp)+20;
        auto p=wi::input::GetPointer();
        const float x=std::clamp(p.x,8.f,std::max(8.f,float(canvas.GetLogicalWidth())-width-8));
        const float y=std::clamp(p.y+24,8.f,std::max(8.f,float(canvas.GetLogicalHeight())-height-8));
        ApplyScissor(canvas,{0,0,int32_t(canvas.GetLogicalWidth()),int32_t(canvas.GetLogicalHeight())},cmd,false);
        wi::image::Draw(nullptr,wi::image::Params(x,y,width,height,theme::Border),cmd);
        wi::image::Draw(nullptr,wi::image::Params(x+1,y+1,width-2,height-2,theme::Panel),cmd);
        fp.posX=x+10;fp.posY=y+10;wi::font::Draw(text,fp,cmd);
    }

void StudioPickerSlider::Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const
{
        if(!IsVisible())return;
        ApplyScissor(canvas,parent?parent->scissorRect:scissorRect,cmd);
        const auto p=valueInputField.GetPos(),s=valueInputField.GetSize();
        const float length=std::max(1.f,p.x-translation.x-6);
        const float t=end>start?std::clamp((value-start)/(end-start),0.f,1.f):0.f;
        const float y=translation.y+scale.y*.5f;
        wi::image::Draw(nullptr,wi::image::Params(translation.x,y-2,length,4,theme::Border),cmd);
        wi::image::Draw(nullptr,wi::image::Params(translation.x,y-2,length*t,4,IsEnabled()?theme::Accent:theme::DisabledText),cmd);
        wi::image::Draw(nullptr,wi::image::Params(translation.x+length*t-3,y-5,6,10,IsEnabled()?theme::Text:theme::DisabledText),cmd);
        wi::font::Params label(translation.x-8,y,theme::CaptionSize,wi::font::WIFALIGN_RIGHT,wi::font::WIFALIGN_CENTER,IsEnabled()?theme::Text:theme::DisabledText);
        wi::font::Draw(GetText(),label,cmd);
        wi::image::Draw(nullptr,wi::image::Params(p.x,p.y,s.x,s.y,theme::Idle),cmd);
        valueInputField.Render(canvas,cmd);
    }
}
