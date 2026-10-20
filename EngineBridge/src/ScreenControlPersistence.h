#pragma once
#include "renegade/bridge/ScreenService.h"
#include "ScreenBindingPersistence.h"
#include "ScreenAnimationPersistence.h"
namespace renegade::bridge::detail
{
    inline void WriteScreenControl(wi::config::Section& s,const ScreenWidget& w)
    {
        const auto kind=ScreenControlKindFor(w.kind);const auto& c=w.control;
        if(kind==ScreenControlKind::TextInput)s.Set("control_value",EncodeBindingText(std::get<std::string>(c.value)));
        else if(kind==ScreenControlKind::Toggle)s.Set("control_value",std::get<bool>(c.value)?"true":"false");
        else s.Set("control_value",ScreenControlNumber(std::get<double>(c.value)));
        s.Set("control_minimum",ScreenControlNumber(c.minimum));s.Set("control_maximum",ScreenControlNumber(c.maximum));
        s.Set("control_step",ScreenControlNumber(c.step));s.Set("control_text_limit",static_cast<int>(c.textLimit));
        s.Set("control_option_count",static_cast<int>(c.options.size()));
        for(std::size_t i=0;i<c.options.size();++i)s.Set(("control_option_"+std::to_string(i)).c_str(),EncodeBindingText(c.options[i]));
    }
    inline bool ReadScreenControl(const wi::config::Section& s,ScreenWidget& w,std::string& error)
    {
        auto& c=w.control;const auto kind=ScreenControlKindFor(w.kind);int limit=0,count=-1;
        if(!s.Has("control_value")||!ReadAnimationScalar(s,"control_minimum",c.minimum)||
           !ReadAnimationScalar(s,"control_maximum",c.maximum)||!ReadAnimationScalar(s,"control_step",c.step)||
           !ReadAnimationScalar(s,"control_text_limit",limit)||limit<1||limit>4096||
           !ReadAnimationScalar(s,"control_option_count",count)||count<0||count>64)
        {error="Malformed UI control data.";return false;}
        c.textLimit=static_cast<unsigned>(limit);
        if(kind==ScreenControlKind::TextInput)
        {
            std::string text;if(!DecodeBindingText(s.GetText("control_value"),c.textLimit,text))
            {error="Malformed text input encoding.";return false;}c.value=std::move(text);
        }
        else if(kind==ScreenControlKind::Toggle)
        {
            const auto v=s.GetText("control_value");if(v!="true"&&v!="false")
            {error="Malformed Toggle value.";return false;}c.value=v=="true";
        }
        else
        {
            double value=0;if(!ReadAnimationScalar(s,"control_value",value))
            {error="Malformed numeric control value.";return false;}c.value=value;
        }
        for(int i=0;i<count;++i)
        {
            const auto key="control_option_"+std::to_string(i);std::string value;
            if(!s.Has(key.c_str())||!DecodeBindingText(s.GetText(key.c_str()),128,value))
            {error="Malformed Selector choice.";return false;}c.options.push_back(std::move(value));
        }
        return ValidateScreenControl(kind,c,error);
    }
}
