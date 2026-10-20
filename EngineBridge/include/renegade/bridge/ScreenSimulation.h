#pragma once
#include "renegade/bridge/ScreenBindingFrame.h"
#include <algorithm>
namespace renegade::bridge
{
    // Explicit isolated samples. This class has no world, command or transport access.
    class ScreenSimulation final
    {
    public:
        void Reset() noexcept {samples_.clear();++revision_;}
        bool SetSample(ScreenBindingSource source,GameplayAttributeValue value,std::string& error)
        {
            if(!ValidateScreenBindingSource(source,error))return false;
            if((std::holds_alternative<double>(value)&&!std::isfinite(std::get<double>(value)))||
               (std::holds_alternative<std::string>(value)&&std::get<std::string>(value).size()>4096))
            {error="Simulation sample value is invalid.";return false;}
            const auto it=std::find_if(samples_.begin(),samples_.end(),[&](const auto& s){return s.source==source;});
            if(it!=samples_.end())it->value=std::move(value);
            else
            {
                if(samples_.size()>=4096){error="Simulation source budget exceeded.";return false;}
                samples_.push_back({std::move(source),std::move(value),1});
            }
            ++revision_;error.clear();return true;
        }
        void ClearSample(const ScreenBindingSource& source)
        {
            samples_.erase(std::remove_if(samples_.begin(),samples_.end(),
                [&](const auto& s){return s.source==source;}),samples_.end());++revision_;
        }
        const ScreenBindingSample* Sample(const ScreenBindingSource& source) const noexcept
        {
            const auto it=std::find_if(samples_.begin(),samples_.end(),[&](const auto& s){return s.source==source;});
            return it==samples_.end()?nullptr:&*it;
        }
        std::uint64_t Revision() const noexcept{return revision_;}
        bool Evaluate(const ScreenDocument& document,ScreenBindingFrame& frame,std::string& error) const
        {
            ScreenBindingSnapshot snapshot;ScreenBindingContext context;
            snapshot.session=context.session=1;snapshot.revision=revision_;snapshot.values=samples_;
            for(const auto& s:samples_)
                if(s.source.scope!=ScreenBindingScope::ProjectAttribute)
                    context.generations.emplace(ScreenBindingContext::Target{s.source.scope,s.source.targetId},1);
            return BuildScreenBindingFrame(document,snapshot,context,frame,error);
        }
        static std::vector<ScreenBindingSource> Sources(const ScreenDocument& document)
        {
            std::vector<ScreenBindingSource> sources;
            const auto add=[&](const auto& s)
            {if(std::find(sources.begin(),sources.end(),s)==sources.end())sources.push_back(s);};
            for(const auto& widget:document.widgets)for(const auto& binding:widget.bindings)
            {add(binding.source);if(binding.operation==ScreenBindingOperation::Ratio)add(binding.denominator);}
            return sources;
        }
    private:
        std::vector<ScreenBindingSample> samples_;std::uint64_t revision_=0;
    };
}
