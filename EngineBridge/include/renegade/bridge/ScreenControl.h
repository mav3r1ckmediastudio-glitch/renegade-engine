#pragma once
#include "renegade/bridge/GameplayAttributeProfile.h"
#include <algorithm>
#include <cmath>
#include <charconv>
namespace renegade::bridge
{
    enum class ScreenControlKind { Toggle, Slider, Selector, TextInput };
    struct ScreenControlData
    {
        GameplayAttributeValue value=0.0;
        double minimum=0,maximum=1,step=.1;
        std::vector<std::string> options;
        unsigned textLimit=256;
    };
    inline std::string ScreenControlNumber(double value)
    {char b[64];const auto result=std::to_chars(b,b+sizeof(b),value);return std::string(b,result.ptr);}
    inline bool ValidateScreenControl(ScreenControlKind kind,const ScreenControlData& c,std::string& error)
    {
        if(c.options.size()>64||c.textLimit<1||c.textLimit>4096||!std::isfinite(c.minimum)||!std::isfinite(c.maximum)||!std::isfinite(c.step))
        {error="UI control options/text limit is invalid.";return false;}
        if(kind!=ScreenControlKind::Selector&&!c.options.empty())
        {error="Only Selector controls may contain choices.";return false;}
        if(kind==ScreenControlKind::Toggle)
        {
            if(!std::holds_alternative<bool>(c.value)){error="Toggle value must be boolean.";return false;}
        }
        else if(kind==ScreenControlKind::TextInput)
        {
            const auto* value=std::get_if<std::string>(&c.value);
            if(!value||value->size()>c.textLimit)
            {error="Text input value exceeds its declared byte limit.";return false;}
        }
        else
        {
            const auto* value=std::get_if<double>(&c.value);
            if(!value||!std::isfinite(*value)){error="UI control value must be a finite number.";return false;}
            if(kind==ScreenControlKind::Slider)
            {
                if(!std::isfinite(c.minimum)||!std::isfinite(c.maximum)||!std::isfinite(c.step)||
                   c.minimum>=c.maximum||std::abs(c.minimum)>1e9||std::abs(c.maximum)>1e9||
                   c.step<=0||c.step>c.maximum-c.minimum||(c.maximum-c.minimum)/c.step>1000000||*value<c.minimum||*value>c.maximum)
                {error="Slider range/step/value is invalid.";return false;}
            }
            else if(kind==ScreenControlKind::Selector)
            {
                if(c.options.empty()||*value<0||std::floor(*value)!=*value||*value>=c.options.size())
                {error="Selector requires choices and a valid initial index.";return false;}
                for(const auto& option:c.options)if(option.empty()||option.size()>128||option.find('|')!=std::string::npos)
                {error="Selector labels must contain 1..128 bytes.";return false;}
            }
            else{error="Unknown UI control kind.";return false;}
        }
        error.clear();return true;
    }
    inline double QuantizeScreenSlider(const ScreenControlData& c,double value) noexcept
    {
        if(value<=c.minimum)return c.minimum;if(value>=c.maximum)return c.maximum;
        value=std::clamp(value,c.minimum,c.maximum);
        return std::clamp(c.minimum+std::round((value-c.minimum)/c.step)*c.step,c.minimum,c.maximum);
    }
    inline std::string ScreenControlDisplay(ScreenControlKind kind,const ScreenControlData& c)
    {
        if(kind==ScreenControlKind::Toggle)return std::get<bool>(c.value)?"ON":"OFF";
        if(kind==ScreenControlKind::TextInput)return std::get<std::string>(c.value);
        if(kind==ScreenControlKind::Selector)return c.options[static_cast<std::size_t>(std::get<double>(c.value))];
        return ScreenControlNumber(std::get<double>(c.value));
    }
}
