#pragma once
#include "renegade/bridge/ScreenService.h"
namespace renegade::bridge
{
    enum class ScreenWidgetPreset
    { Panel, Heading, Icon, Divider, Spacer, Counter, PlayerHealthBar, PlayerHealthText, PlayerAmmoText, Group, HorizontalStack, VerticalStack, LayoutGrid, Toggle, Slider, Selector, TextInput };
    // Presets are ordinary editable widgets using existing persistence/rendering.
    // Player presets explicitly bind real possessed-player state; no simulated values.
    inline ScreenWidget MakeScreenWidgetPreset(ScreenWidgetPreset preset,float canvasWidth,float canvasHeight)
    {
        ScreenWidget w;
        const bool healthBar=preset==ScreenWidgetPreset::PlayerHealthBar;
        const bool text=preset==ScreenWidgetPreset::Heading||preset==ScreenWidgetPreset::Counter||
            preset==ScreenWidgetPreset::PlayerHealthText||preset==ScreenWidgetPreset::PlayerAmmoText;
        w.kind=healthBar?ScreenWidgetKind::Progress:(text?ScreenWidgetKind::Text:ScreenWidgetKind::Image);
        w.enabled=false;w.visible=true;
        w.rect={canvasWidth*.15f,canvasHeight*.15f,320,48};
        switch(preset)
        {
        case ScreenWidgetPreset::Panel:w.name="Panel";w.rect.width=canvasWidth*.5f;w.rect.height=canvasHeight*.5f;break;
        case ScreenWidgetPreset::Heading:w.name="Heading";w.text="HEADING";w.rect.width=canvasWidth*.7f;w.rect.height=88;break;
        case ScreenWidgetPreset::Icon:w.name="Icon";w.rect.width=w.rect.height=64;break;
        case ScreenWidgetPreset::Divider:w.name="Divider";w.rect.height=2;break;
        case ScreenWidgetPreset::Spacer:w.name="Spacer";w.rect.width=w.rect.height=32;break;
        case ScreenWidgetPreset::Counter:w.name="Counter";w.text="0";break;
        case ScreenWidgetPreset::PlayerHealthBar:w.name="Player Health Bar";w.rect.height=24;w.fill=0;break;
        case ScreenWidgetPreset::PlayerHealthText:w.name="Player Health";w.text="HEALTH: unavailable";break;
        case ScreenWidgetPreset::PlayerAmmoText:w.name="Player Loaded Ammo";w.text="AMMO: unavailable";break;
        case ScreenWidgetPreset::Group:w.name="Group";w.rect.width=360;w.rect.height=240;break;
        case ScreenWidgetPreset::HorizontalStack:w.name="Horizontal Stack";w.rect.width=480;w.rect.height=160;w.container.mode=ScreenChildLayout::Horizontal;break;
        case ScreenWidgetPreset::VerticalStack:w.name="Vertical Stack";w.rect.width=360;w.rect.height=320;w.container.mode=ScreenChildLayout::Vertical;break;
        case ScreenWidgetPreset::Toggle:w.name="Toggle";w.text="Toggle";w.kind=ScreenWidgetKind::Toggle;w.control.value=false;w.enabled=true;break;
        case ScreenWidgetPreset::Slider:w.name="Slider";w.text="Slider";w.kind=ScreenWidgetKind::Slider;w.control.value=.5;w.rect.height=64;w.enabled=true;break;
        case ScreenWidgetPreset::Selector:w.name="Selector";w.text="Choice";w.kind=ScreenWidgetKind::Selector;w.control.options={"First","Second"};w.enabled=true;break;
        case ScreenWidgetPreset::TextInput:w.name="Text Input";w.text="Enter text";w.kind=ScreenWidgetKind::TextInput;w.control.value=std::string();w.enabled=true;break;
        case ScreenWidgetPreset::LayoutGrid:w.name="Grid";w.rect.width=480;w.rect.height=320;w.container.mode=ScreenChildLayout::Grid;break;
        }
        w.style=MakeScreenWidgetStyleTemplate(w.kind,w.rect.height);
        if(preset==ScreenWidgetPreset::Heading)
        {w.style.text.fontSize=48;w.style.text.horizontalAlignment=ScreenHorizontalAlignment::Center;}
        if(w.kind==ScreenWidgetKind::Image)
        {
            w.style.normal.background=preset==ScreenWidgetPreset::Panel?ScreenColor{24,30,36,230}:
                (preset==ScreenWidgetPreset::Divider?ScreenColor{160,180,190,255}:ScreenColor{0,0,0,0});
            w.style.hover=w.style.normal;w.style.pressed=w.style.normal;
            w.style.focused=w.style.normal;w.style.disabled=w.style.normal;
        }
        if(healthBar||preset==ScreenWidgetPreset::PlayerHealthText||preset==ScreenWidgetPreset::PlayerAmmoText)
        {
            ScreenBinding b;b.source.scope=ScreenBindingScope::PossessedPlayer;
            b.source.attributeId=preset==ScreenWidgetPreset::PlayerAmmoText?"ammo.loaded":"health.current";
            b.property=healthBar?ScreenBindingProperty::Fill:ScreenBindingProperty::Text;
            if(healthBar){b.operation=ScreenBindingOperation::Ratio;b.denominator.scope=ScreenBindingScope::PossessedPlayer;b.denominator.attributeId="health.maximum";}
            else b.prefix=preset==ScreenWidgetPreset::PlayerAmmoText?"AMMO: ":"HEALTH: ";
            b.required=true;w.bindings.push_back(std::move(b));
        }
        return w;
    }
}
