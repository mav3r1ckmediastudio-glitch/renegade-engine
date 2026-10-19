#pragma once
#include "RenegadeStudioChrome.h"
#include "RenegadeScreenLiteralTextInput.h"
#include "renegade/bridge/GameplayAttributeDocumentService.h"
#include "RenegadeScreenChoicePicker.h"
#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/bridge/ScreenBindingTargetService.h"
#include <algorithm>
#include <functional>
#include <sstream>

namespace renegade::studio
{
    class RenegadeScreenDataBindingInspector final : public wi::gui::Widget
    {
        using Property = bridge::ScreenBindingProperty;
        using Operation = bridge::ScreenBindingOperation;
        using Scope = bridge::ScreenBindingScope;
        struct Attribute { std::string id; std::string label; bool boolean; bool characterOnly; bool text=false; };
        static const std::vector<Attribute>& Attributes()
        {
            static const std::vector<Attribute> values = {
                {"health.current","Health / current",false,false},
                {"health.maximum","Health / maximum",false,false},
                {"alive","Alive",true,false},
                {"ammo.loaded","Ammo / loaded",false,false},
                {"ammo.capacity","Ammo / capacity",false,false},
                {"ammo.reserve","Ammo / reserve",false,true},
                {"ammo.reloading","Ammo / reloading",true,true},
                {"equipment.equipped","Equipment / equipped",true,false}};
            return values;
        }
        std::vector<Attribute> AvailableAttributes()
        {
            std::vector<Attribute> result;
            if(draft_.source.scope!=Scope::ProjectAttribute)result=Attributes();
            if(!profileDocument_)return result;
            const auto wanted=draft_.source.scope==Scope::ProjectAttribute?bridge::GameplayAttributeScope::Project:
                draft_.source.scope==Scope::Character?bridge::GameplayAttributeScope::Character:bridge::GameplayAttributeScope::Player;
            const auto target=wanted==bridge::GameplayAttributeScope::Character?actor_.GetValue():std::string{};
            for(const auto& attachment:profileDocument_->attachments)
            {
                if(attachment.scope!=wanted||attachment.targetId!=target)continue;
                const auto profile=std::find_if(profileDocument_->profiles.begin(),profileDocument_->profiles.end(),
                    [&](const auto& value){return value.id==attachment.profileId;});
                if(profile==profileDocument_->profiles.end())continue;
                for(const auto& value:profile->attributes)
                    result.push_back({value.id,value.name,std::holds_alternative<bool>(value.initial),false,
                        std::holds_alternative<std::string>(value.initial)});
            }
            return result;
        }
        int AttributeType(const std::string& id)
        {
            for(const auto& attribute:AvailableAttributes())if(attribute.id==id)return attribute.text?2:attribute.boolean?1:0;
            return 0;
        }
    public:
        void SetGameplayAttributeDocument(std::optional<bridge::GameplayAttributeDocument> document)
        {profileDocument_=std::move(document);UpdateLabels();}
        void Create()
        {
            SetName("Screen Gameplay Binding Inspector");
            const auto button = [](RenegadeButton& control,const char* name) { control.Create(name); };
            button(property_,"UI binding target property");
            button(scope_,"UI binding source role");
            button(actorChoice_,"UI binding Character picker");
            actorChoice_.OnClick([this](const wi::gui::EventArgs&) { OpenActor(); });
            button(source_,"UI binding source attribute");
            button(operation_,"UI binding transform");
            button(denominator_,"UI binding ratio denominator");
            button(required_,"UI binding readiness requirement");
            button(missing_,"UI binding missing source policy");
            button(apply_,"UI apply gameplay binding");
            button(remove_,"UI remove gameplay binding");
            const auto input = [](RenegadeTextInputField& control,const char* name,const char* description)
            {
                control.Create(name);control.SetValue("");
                control.SetDescription(description);control.SetCancelInputEnabled(false);
                control.SetShadowRadius(0);
            };
            input(actor_,"UI binding Character identity","CHARACTER ID  ");
            input(threshold_,"UI binding condition threshold","COMPARE  ");
            input(prefix_,"UI binding text prefix","PREFIX  ");
            input(suffix_,"UI binding text suffix","SUFFIX  ");
            input(precision_,"UI binding decimal places","DECIMALS  ");
            actor_.SetLiteral(true);prefix_.SetLiteral(true);suffix_.SetLiteral(true);
            property_.OnClick([this](const wi::gui::EventArgs&) { OpenProperty(); });
            scope_.OnClick([this](const wi::gui::EventArgs&)
            {
                draft_.source.scope=draft_.source.scope==Scope::PossessedPlayer?Scope::Character:
                    draft_.source.scope==Scope::Character?Scope::ProjectAttribute:Scope::PossessedPlayer;
                draft_.source.targetId.clear();actor_.SetValue("");
                draft_.source.attributeId.clear();draft_.denominator={};sameDenominatorTarget_=true;
                UpdateLabels();
            });
            source_.OnClick([this](const wi::gui::EventArgs&) { OpenSource(false); });
            denominator_.OnClick([this](const wi::gui::EventArgs&) { OpenSource(true); });
            operation_.OnClick([this](const wi::gui::EventArgs&) { OpenOperation(); });
            required_.OnClick([this](const wi::gui::EventArgs&) { draft_.required=!draft_.required;UpdateLabels(); });
            missing_.OnClick([this](const wi::gui::EventArgs&)
            {
                draft_.missingPolicy=draft_.missingPolicy==bridge::ScreenBindingMissingPolicy::Hide?
                    bridge::ScreenBindingMissingPolicy::AuthoredFallback:bridge::ScreenBindingMissingPolicy::Hide;
                UpdateLabels();
            });
            apply_.SetText("APPLY BINDING");
            remove_.SetText("REMOVE BINDING");
            apply_.OnClick([this](const wi::gui::EventArgs&) { Apply(false); });
            remove_.OnClick([this](const wi::gui::EventArgs&) { Apply(true); });
        }
        void SetActorCatalogue(bridge::ScreenBindingActorCatalogue catalogue)
        { actors_=std::move(catalogue);UpdateLabels(); }
        void Bind(bridge::ScreenAuthoringSession* session,const bridge::StableId& selected,
            std::function<void()> changed)
        {
            const bool selectionChanged=session_!=session || selected_!=selected;
            session_=session;selected_=selected;changed_=std::move(changed);
            if(selectionChanged)
            {
                const auto* widget=Selected();
                propertyValue_=widget && widget->kind==bridge::ScreenWidgetKind::Progress?Property::Fill:
                    widget && (widget->kind==bridge::ScreenWidgetKind::Text ||
                        widget->kind==bridge::ScreenWidgetKind::Button)?Property::Text:Property::Visible;
            }
            Load();
        }
        void SetLayout(float x,float y,float width,float height)
        {
            x_=x;y_=y;width_=width;height_=std::max(1.0f,height);
            SetPos(XMFLOAT2(x,y));SetSize(XMFLOAT2(width,height_));
            picker_.SetLayout(x,y+40,width,std::min(280.0f,std::max(160.0f,height_-48)));
        }
        void SetActive(bool active)
        {
            SetVisible(active);
            if(!active)
            {
                for(auto* control:Controls())control->SetVisible(false);
                picker_.Close();
            }
        }
        bool HasOpenPopup() const noexcept { return picker_.IsOpen(); }
        void ClosePopup() { picker_.Close(); }
        void Update(const wi::Canvas& canvas,float dt) override
        {
            Widget::Update(canvas,dt);
            if(!IsVisible() || !Selected())
            {
                for(auto* control:Controls())control->SetVisible(false);
                picker_.Close();return;
            }
            if(observedRevision_!=session_->Revision())Load();
            Layout();
            if(picker_.IsOpen()){picker_.Update();return;}
            for(auto* control:Controls())if(control->IsVisible())control->Update(canvas,dt);
        }
        void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const override
        {
            if(!IsVisible() || !Selected())return;
            for(const auto* control:ControlsConst())
            {
                if(!control->IsVisible())continue;
                ApplyScissor(canvas,scissorRect,cmd);control->Render(canvas,cmd);
            }
            ApplyScissor(canvas,scissorRect,cmd);
            wi::font::Params text;
            text.posX=x_;text.posY=y_+height_-32;text.size=9;
            text.color=wi::Color(145,170,181,255);text.h_wrap=width_;
            wi::font::Draw(status_,text,cmd);
            picker_.Render(cmd);
        }
    private:
        const bridge::ScreenWidget* Selected() const
        { return session_ && session_->IsLoaded()?session_->FindWidget(selected_):nullptr; }
        static bool BooleanTarget(Property value)
        { return value==Property::Visible || value==Property::Enabled; }
        bool BooleanAttribute(const std::string& id) {return AttributeType(id)==1;}
        static const char* PropertyName(Property value)
        {
            switch(value){case Property::Text:return "TEXT";case Property::Fill:return "FILL";
                case Property::Visible:return "VISIBLE";case Property::Enabled:return "ENABLED";}
            return "UNKNOWN";
        }
        static const char* OperationName(Operation value)
        {
            switch(value){case Operation::Direct:return "DIRECT";case Operation::Ratio:return "RATIO";
                case Operation::AtLeast:return "AT LEAST";case Operation::AtMost:return "AT MOST";
                case Operation::Equals:return "EQUALS";}
            return "UNKNOWN";
        }
        static std::string Trim(std::string text)
        {
            const auto first=text.find_first_not_of(" \t\r\n");
            if(first==std::string::npos)return {};
            return text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);
        }
        void Load()
        {
            picker_.Close();
            const auto* widget=Selected();if(!widget)return;
            observedRevision_=session_->Revision();
            draft_={};draft_.property=propertyValue_;
            for(const auto& binding:widget->bindings)
                if(binding.property==propertyValue_){draft_=binding;break;}
            actor_.SetValue(draft_.source.targetId);
            prefix_.SetValue(draft_.prefix);suffix_.SetValue(draft_.suffix);
            precision_.SetValue(std::to_string(draft_.decimalPlaces));
            sameDenominatorTarget_=draft_.denominator.scope==draft_.source.scope &&
                draft_.denominator.targetId==draft_.source.targetId;
            if(const auto* n=std::get_if<double>(&draft_.comparison))
            { std::ostringstream value;value.imbue(std::locale::classic());value<<std::setprecision(15)<<*n;threshold_.SetValue(value.str()); }
            else if(const auto* b=std::get_if<bool>(&draft_.comparison))threshold_.SetValue(*b?"true":"false");
            else threshold_.SetValue(std::get<std::string>(draft_.comparison));
            UpdateLabels();
        }
        void OpenActor()
        {
            std::vector<RenegadeScreenChoicePicker::Choice> choices;
            for(const auto& actor:actors_.characters) choices.push_back({actor.name+" // "+actor.id.substr(0,8),actor.id});
            picker_.Open("CURRENT LEVEL CHARACTERS",std::move(choices),draft_.source.targetId,
                [this](const std::string& id) { draft_.source.targetId=id;actor_.SetValue(id);
                    if(sameDenominatorTarget_)draft_.denominator.targetId=id;UpdateLabels(); });
            status_=actors_.characters.empty()?"No Characters in this Level. Use Possessed Player or open a Level with Characters.":
                actors_.diagnostics.empty()?"Choose a Character from the currently loaded Level.":
                "Some Characters have invalid/duplicate identities and cannot be selected.";
        }
        void UpdateLabels()
        {
            std::string actorLabel=draft_.source.targetId.empty()?"CHOOSE...":"UNRESOLVED ID";
            for(const auto& actor:actors_.characters)if(actor.id==draft_.source.targetId){actorLabel=actor.name;break;}
            actorChoice_.SetText("CHARACTER: "+actorLabel);
            property_.SetText(std::string("PROPERTY: ")+PropertyName(propertyValue_));
            scope_.SetText(draft_.source.scope==Scope::Character?"SOURCE ROLE: CHARACTER":
                draft_.source.scope==Scope::ProjectAttribute?"SOURCE ROLE: PROJECT ATTRIBUTE":"SOURCE ROLE: POSSESSED PLAYER");
            source_.SetText("ATTRIBUTE: "+(draft_.source.attributeId.empty()?std::string("CHOOSE..."):draft_.source.attributeId));
            operation_.SetText(std::string("TRANSFORM: ")+OperationName(draft_.operation));
            denominator_.SetText("DIVIDE BY: "+(draft_.denominator.attributeId.empty()?std::string("CHOOSE..."):draft_.denominator.attributeId));
            required_.SetText(draft_.required?"REQUIRED: YES":"REQUIRED: NO");
            threshold_.SetLiteral(AttributeType(draft_.source.attributeId)!=0||!std::holds_alternative<double>(draft_.comparison));
            missing_.SetText(draft_.missingPolicy==bridge::ScreenBindingMissingPolicy::Hide?"MISSING: HIDE":"MISSING: FALLBACK");
        }
        void OpenProperty()
        {
            std::vector<RenegadeScreenChoicePicker::Choice> choices;
            const auto* widget=Selected();if(!widget)return;
            if(widget->kind==bridge::ScreenWidgetKind::Text || widget->kind==bridge::ScreenWidgetKind::Button)
                choices.push_back({"Text","0"});
            if(widget->kind==bridge::ScreenWidgetKind::Progress)choices.push_back({"Fill","1"});
            choices.push_back({"Visible","2"});choices.push_back({"Enabled","3"});
            picker_.Open("BINDING TARGET",std::move(choices),std::to_string(static_cast<int>(propertyValue_)),
                [this](const std::string& value){propertyValue_=static_cast<Property>(std::stoi(value));Load();});
        }
        void OpenOperation()
        {
            std::vector<RenegadeScreenChoicePicker::Choice> choices={{"Direct","0"}};
            if(BooleanTarget(propertyValue_))
            { choices.push_back({"At least","2"});choices.push_back({"At most","3"});choices.push_back({"Equals","4"}); }
            else choices.push_back({"Ratio","1"});
            picker_.Open("TYPED TRANSFORM",std::move(choices),std::to_string(static_cast<int>(draft_.operation)),
                [this](const std::string& value)
                {
                    draft_.operation=static_cast<Operation>(std::stoi(value));
                    draft_.source.attributeId.clear();draft_.denominator={};
                    threshold_.SetValue("0");UpdateLabels();
                });
        }
        void OpenSource(bool denominator)
        {
            std::vector<RenegadeScreenChoicePicker::Choice> choices;
            for(const auto& attribute:AvailableAttributes())
            {
                if(attribute.characterOnly && draft_.source.scope!=Scope::Character)continue;
                if(draft_.source.scope==Scope::Character && std::string(attribute.id)=="equipment.equipped")continue;
                const bool numeric=denominator || propertyValue_==Property::Fill ||
                    draft_.operation==Operation::Ratio || draft_.operation==Operation::AtLeast ||
                    draft_.operation==Operation::AtMost;
                const bool boolean=BooleanTarget(propertyValue_) && draft_.operation==Operation::Direct;
                if((numeric && (attribute.boolean||attribute.text)) || (boolean && !attribute.boolean))continue;
                choices.push_back({std::string(attribute.label)+(attribute.text?" // Text":attribute.boolean?" // Boolean":" // Number"),attribute.id});
            }
            picker_.Open(denominator?"RATIO MAXIMUM SOURCE":"COMPATIBLE GAMEPLAY SOURCE",std::move(choices),
                denominator?draft_.denominator.attributeId:draft_.source.attributeId,
                [this,denominator](const std::string& id)
                {
                    if(denominator)
                    {
                        draft_.denominator={draft_.source.scope,Trim(actor_.GetValue()),id};
                        if(draft_.source.scope!=Scope::Character)draft_.denominator.targetId.clear();
                        sameDenominatorTarget_=true;
                    }
                    else
                    {
                        draft_.source.attributeId=id;
                        const auto type=AttributeType(id);
                        draft_.comparison=type==2?bridge::GameplayAttributeValue{std::string{}}:
                            type==1?bridge::GameplayAttributeValue{true}:bridge::GameplayAttributeValue{0.0};
                        threshold_.SetValue(type==2?"":type==1?"true":"0");
                    }
                    UpdateLabels();
                });
        }
        void Apply(bool remove)
        {
            const auto* widget=Selected();if(!widget)return;
            auto bindings=widget->bindings;
            const auto existing=std::find_if(bindings.begin(),bindings.end(),
                [this](const bridge::ScreenBinding& value){return value.property==propertyValue_;});
            if(remove)
            {
                if(existing==bindings.end()){status_="No binding on this property.";return;}
                bindings.erase(existing);
            }
            else
            {
                auto candidate=draft_;
                candidate.source.targetId=candidate.source.scope==Scope::Character?Trim(actor_.GetValue()):bridge::StableId{};
                if(candidate.operation==Operation::Ratio && sameDenominatorTarget_)
                { candidate.denominator.scope=candidate.source.scope;candidate.denominator.targetId=candidate.source.targetId; }
                candidate.prefix=prefix_.GetValue();candidate.suffix=suffix_.GetValue();
                std::istringstream precision(Trim(precision_.GetValue()));
                precision>>std::noskipws>>candidate.decimalPlaces;
                if(precision.fail() || precision.peek()!=std::char_traits<char>::eof())
                {status_="Decimals must be an integer from 0 to 6.";return;}
                if(candidate.operation==Operation::AtLeast || candidate.operation==Operation::AtMost ||
                    candidate.operation==Operation::Equals)
                {
                    const auto comparison=Trim(threshold_.GetValue());
                    if(BooleanAttribute(candidate.source.attributeId))
                    {
                        if(comparison!="true" && comparison!="false"){status_="Boolean comparison must be true or false.";return;}
                        candidate.comparison=comparison=="true";
                    }
                    else if(AttributeType(candidate.source.attributeId)==2||std::holds_alternative<std::string>(draft_.comparison))
                        candidate.comparison=threshold_.GetValue();
                    else
                    {
                        double number=0;std::istringstream input(comparison);input.imbue(std::locale::classic());
                        input>>std::noskipws>>number;
                        if(input.fail() || input.peek()!=std::char_traits<char>::eof() || !std::isfinite(number))
                        {status_="Comparison must be a finite number.";return;}
                        candidate.comparison=number;
                    }
                }
                std::string error;
                if(!bridge::ValidateScreenBinding(candidate,error)){status_=error;return;}
                if(existing==bindings.end())bindings.push_back(candidate);else *existing=candidate;
            }
            std::string error;
            if(!session_->UpdateWidgetBindings(selected_,std::move(bindings),error)){status_=error;return;}
            Load();status_=remove?"Binding removed.":"Binding applied. Gameplay remains the value owner.";
            if(changed_)changed_();
        }
        void Layout()
        {
            std::vector<wi::gui::Widget*> visible={&property_,&scope_};
            if(draft_.source.scope==Scope::Character)
            {visible.push_back(&actorChoice_);visible.push_back(&actor_);}
            visible.push_back(&source_);visible.push_back(&operation_);
            if(draft_.operation==Operation::Ratio)visible.push_back(&denominator_);
            if(draft_.operation==Operation::AtLeast || draft_.operation==Operation::AtMost ||
                draft_.operation==Operation::Equals)visible.push_back(&threshold_);
            if(propertyValue_==Property::Text)
            { visible.push_back(&prefix_);visible.push_back(&suffix_);visible.push_back(&precision_); }
            visible.push_back(&required_);visible.push_back(&missing_);
            visible.push_back(&apply_);visible.push_back(&remove_);
            float y=y_;
            for(auto* control:Controls())control->SetVisible(std::find(visible.begin(),visible.end(),control)!=visible.end());
            for(auto* control:visible)
            {control->SetPos(XMFLOAT2(x_,y));control->SetSize(XMFLOAT2(width_,28));y+=34;}
        }
        std::vector<wi::gui::Widget*> Controls()
        {return {&property_,&scope_,&actorChoice_,&actor_,&source_,&operation_,&denominator_,&threshold_,&prefix_,&suffix_,&precision_,&required_,&missing_,&apply_,&remove_};}
        std::vector<const wi::gui::Widget*> ControlsConst() const
        {return {&property_,&scope_,&actorChoice_,&actor_,&source_,&operation_,&denominator_,&threshold_,&prefix_,&suffix_,&precision_,&required_,&missing_,&apply_,&remove_};}
        bridge::ScreenAuthoringSession* session_=nullptr;
        bridge::StableId selected_;
        std::function<void()> changed_;
        bridge::ScreenBinding draft_;
        Property propertyValue_=Property::Text;
        std::uint64_t observedRevision_=0;
        bool sameDenominatorTarget_=true;
        float x_=0,y_=0,width_=320,height_=500;
        std::string status_="Choose an explicit gameplay source. Design shows authored fallback values.";
        bridge::ScreenBindingActorCatalogue actors_;
        RenegadeButton actorChoice_;
        RenegadeButton property_,scope_,source_,operation_,denominator_,required_,missing_,apply_,remove_;
        RenegadeTextInputField precision_;
        RenegadeScreenLiteralTextInput actor_,threshold_,prefix_,suffix_;
        std::optional<bridge::GameplayAttributeDocument> profileDocument_;
        RenegadeScreenChoicePicker picker_;
    };
}
