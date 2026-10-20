#pragma once
#include "renegade/bridge/ScreenService.h"
#include <tuple>
namespace renegade::bridge
{
    inline auto ScreenColorTuple(const ScreenColor& c){return std::make_tuple(c.red,c.green,c.blue,c.alpha);}
    inline std::uint32_t ScreenStyleChangedGroups(const ScreenWidgetStyle& a,const ScreenWidgetStyle& b)
    {
        std::uint32_t mask=0;
        const ScreenVisualState* x[]={&a.normal,&a.hover,&a.pressed,&a.focused,&a.disabled};
        const ScreenVisualState* y[]={&b.normal,&b.hover,&b.pressed,&b.focused,&b.disabled};
        for(int i=0;i<5;++i)
        {
            if(ScreenColorTuple(x[i]->background)!=ScreenColorTuple(y[i]->background)||
               ScreenColorTuple(x[i]->foreground)!=ScreenColorTuple(y[i]->foreground)||
               ScreenColorTuple(x[i]->imageTint)!=ScreenColorTuple(y[i]->imageTint))mask|=1;
            if(x[i]->imageResourcePath!=y[i]->imageResourcePath)mask|=8;
        }
        if(ScreenColorTuple(a.borderColor)!=ScreenColorTuple(b.borderColor)||
           a.borderWidth!=b.borderWidth||a.cornerRadius!=b.cornerRadius)mask|=2;
        const auto text=[](const ScreenTextStyle& t){return std::make_tuple(t.fontResource,t.fontSize,
            t.characterSpacing,t.lineSpacing,t.softness,t.bolden,ScreenColorTuple(t.shadowColor),
            t.shadowOffsetX,t.shadowOffsetY,t.shadowSoftness,t.shadowBolden,t.horizontalAlignment,
            t.verticalAlignment,t.wrap);};
        if(text(a.text)!=text(b.text))mask|=4;
        if(a.opacity!=b.opacity)mask|=16;
        return mask;
    }
}
