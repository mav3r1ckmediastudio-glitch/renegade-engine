#pragma once
#include "RenegadeStudioChrome.h"
#include "RenegadeScreenLiteralTextInput.h"
#include "RenegadeScreenChoicePicker.h"
#include "renegade/bridge/GameplayAttributeAuthoringSession.h"
#include "renegade/bridge/ScreenBindingTargetService.h"
#include <sstream>
#include <iomanip>
namespace renegade::studio
{
    class RenegadeGameplayAttributeInspector final : public wi::gui::Widget
    {
    public:
        using AttachCallback=std::function<bool(const std::string&,const std::string&,std::string&)>;
        using SavedCallback=std::function<void(const bridge::GameplayAttributeDocument&)>;
        void Create()
        {
            SetName("Gameplay attribute profile editor");
            const auto button=[](RenegadeButton& c,const char* name,const char* text){c.Create(name);c.SetText(text);};
            button(close_,"Close gameplay profiles","CLOSE");
            button(profile_,"Choose shared gameplay profile","CHOOSE PROFILE");
            button(addProfile_,"Add shared gameplay profile","+ PROFILE");
            button(renameProfile_,"Rename gameplay profile","RENAME");
            button(removeProfile_,"Delete gameplay profile","DELETE PROFILE");
            button(scope_,"Gameplay profile attachment scope","SCOPE: PROJECT");
            button(actor_,"Gameplay profile character identity","CHOOSE CHARACTER");
            button(attach_,"Attach shared gameplay profile","ATTACH SELECTED PROFILE");
            button(detach_,"Detach shared gameplay profile","DETACH FROM SCOPE");
            button(undo_,"Undo gameplay profile edit","UNDO PROFILE");
            button(redo_,"Redo gameplay profile edit","REDO PROFILE");
            button(save_,"Save gameplay attribute module","SAVE GAMEPLAY MODULE");
            button(reload_,"Reload gameplay attribute module","RELOAD // DISCARD DRAFT");
            button(attribute_,"Choose gameplay attribute","CHOOSE ATTRIBUTE");
            button(type_,"Gameplay attribute type","TYPE: NUMBER");
            button(mode_,"Gameplay default or instance override","EDIT: SHARED DEFAULT");
            button(apply_,"Apply gameplay attribute","APPLY ATTRIBUTE");
            button(addAttribute_,"Add gameplay attribute","+ ATTRIBUTE");
            button(removeAttribute_,"Remove gameplay attribute","DELETE ATTRIBUTE");
            const auto input=[](RenegadeScreenLiteralTextInput& c,const char* name,const char* description,bool literal){
                c.Create(name);c.SetValue("");c.SetDescription(description);c.SetCancelInputEnabled(false);c.SetLiteral(literal);
            };
            input(profileName_,"Gameplay profile name","PROFILE NAME  ",true);
            input(id_,"Gameplay attribute ID","ATTRIBUTE ID  ",true);
            input(name_,"Gameplay attribute label","LABEL  ",true);
            input(initial_,"Gameplay initial value","INITIAL  ",false);
            input(minimum_,"Gameplay minimum","MINIMUM  ",false);
            input(maximum_,"Gameplay maximum","MAXIMUM  ",false);
            input(rate_,"Gameplay rule rate","RATE / SECOND  ",false);
            for(int i=0;i<4;++i)
            {
                masks_[i].Create("Gameplay per-property override "+std::to_string(i));
                masks_[i].OnClick([this,i](const wi::gui::EventArgs&){mask_^=1u<<i;Labels();});
            }
            close_.OnClick([this](const wi::gui::EventArgs&){open_=false;SetVisible(false);picker_.Close();});
            profile_.OnClick([this](const wi::gui::EventArgs&){PickProfile();});
            addProfile_.OnClick([this](const wi::gui::EventArgs&){AddProfile();});
            renameProfile_.OnClick([this](const wi::gui::EventArgs&){RenameProfile();});
            removeProfile_.OnClick([this](const wi::gui::EventArgs&){RemoveProfile();});
            scope_.OnClick([this](const wi::gui::EventArgs&){scopeIndex_=(scopeIndex_+1)%3;LoadFields();});
            actor_.OnClick([this](const wi::gui::EventArgs&){PickActor();});
            attach_.OnClick([this](const wi::gui::EventArgs&){Attach(false);});
            detach_.OnClick([this](const wi::gui::EventArgs&){Attach(true);});
            undo_.OnClick([this](const wi::gui::EventArgs&){std::string error;if(!session_.Undo(error)){status_=error;return;}LoadFields();});
            redo_.OnClick([this](const wi::gui::EventArgs&){std::string error;if(!session_.Redo(error)){status_=error;return;}LoadFields();});
            save_.OnClick([this](const wi::gui::EventArgs&){Save();});
            reload_.OnClick([this](const wi::gui::EventArgs&){Reload();});
            attribute_.OnClick([this](const wi::gui::EventArgs&){PickAttribute();});
            type_.OnClick([this](const wi::gui::EventArgs&){
                typeIndex_=(typeIndex_+1)%3;initial_.SetValue(typeIndex_==0?"0":typeIndex_==1?"false":"");
                if(typeIndex_!=0){minimum_.SetValue("");maximum_.SetValue("");rate_.SetValue("0");}Labels();
            });
            mode_.OnClick([this](const wi::gui::EventArgs&){overrideMode_=!overrideMode_;LoadFields();});
            apply_.OnClick([this](const wi::gui::EventArgs&){Apply();});
            addAttribute_.OnClick([this](const wi::gui::EventArgs&){AddAttribute();});
            removeAttribute_.OnClick([this](const wi::gui::EventArgs&){RemoveAttribute();});
        }
        void SetActorCatalogue(bridge::ScreenBindingActorCatalogue actors){actors_=std::move(actors);Labels();}
        bool Open(std::string root,std::string projectId,std::string documentId,std::string hint,
            AttachCallback attach,SavedCallback saved)
        {
            attachCallback_=std::move(attach);savedCallback_=std::move(saved);picker_.Close();
            const bool changedReference=root_==root&&projectId_==projectId&&session_.IsLoaded()&&
                (documentId_!=documentId||hint_!=hint);
            if(changedReference&&session_.Dirty())
            {
                status_="Project gameplay module reference changed. Current unsaved draft is retained; save or discard it before opening the new module.";
                open_=true;SetVisible(true);return false;
            }
            if(root_!=root||projectId_!=projectId||!session_.IsLoaded()||changedReference)
            {
                session_.Clear();root_=std::move(root);projectId_=std::move(projectId);
                documentId_=std::move(documentId);hint_=std::move(hint);selectedProfile_.clear();selectedAttribute_.clear();
                std::string error;
                if(!documentId_.empty())
                {
                    std::string path;
                    if(!bridge::ResolveGameplayAttributeDocumentPath(root_,projectId_,documentId_,hint_,path,error)||
                        !session_.Open(path,projectId_,error)){status_=error;open_=true;SetVisible(true);return false;}
                }
                else
                {
                    bridge::GameplayAttributeDocument doc;const auto id=bridge::GenerateStableId();
                    const auto path="Content/Gameplay/AttributeProfiles-"+id.substr(0,8)+".renegade-attributes";
                    doc.envelope=bridge::CreateDocumentEnvelope(projectId_,bridge::GameplayAttributeDocumentType,path,"Native gameplay profile editor");
                    if(!session_.CreateDraft((std::filesystem::u8path(root_)/path).generic_u8string(),std::move(doc),error))
                    {status_=error;open_=true;SetVisible(true);return false;}
                }
            }
            open_=true;SetVisible(true);LoadFields();return true;
        }
        void Clear(){session_.Clear();root_.clear();projectId_.clear();documentId_.clear();hint_.clear();open_=false;picker_.Close();SetVisible(false);}
        bool IsOpen() const noexcept{return open_;}
        void SetLayout(float width,float height)
        {
            width_=std::min(1120.0f,std::max(680.0f,width-32));height_=std::min(600.0f,std::max(440.0f,height-64));
            x_=(width-width_)*.5f;y_=(height-height_)*.5f;SetPos(XMFLOAT2(x_,y_));SetSize(XMFLOAT2(width_,height_));
            picker_.SetLayout(x_+300,y_+48,width_-316,std::max(180.0f,height_-64));
        }
        void Update(const wi::Canvas& canvas,float dt) override
        {
            Widget::Update(canvas,dt);if(!open_)return;
            Layout();
            if(picker_.IsOpen()){picker_.Update();return;}
            const auto* profile=Profile();const auto* attribute=Attribute();
            const auto* attachment=Attachment();const bool sameAttachment=profile&&attachment&&attachment->profileId==profile->id;
            for(auto* c:Controls())c->SetEnabled(session_.IsLoaded());
            close_.SetEnabled(true);reload_.SetEnabled(!documentId_.empty());undo_.SetEnabled(session_.CanUndo());redo_.SetEnabled(session_.CanRedo());
            actor_.SetEnabled(scopeIndex_==2);detach_.SetEnabled(attachment!=nullptr);attach_.SetEnabled(profile!=nullptr);
            renameProfile_.SetEnabled(profile!=nullptr);removeProfile_.SetEnabled(profile!=nullptr);
            type_.SetEnabled(attribute&&!overrideMode_);id_.SetEnabled(attribute&&!overrideMode_);name_.SetEnabled(attribute&&!overrideMode_);
            apply_.SetEnabled(attribute&&(!overrideMode_||sameAttachment));addAttribute_.SetEnabled(profile&&!overrideMode_);removeAttribute_.SetEnabled(attribute&&!overrideMode_);
            initial_.SetEnabled(attribute&&(!overrideMode_||(sameAttachment&&(mask_&1))));
            minimum_.SetEnabled(attribute&&typeIndex_==0&&(!overrideMode_||(sameAttachment&&(mask_&2))));
            maximum_.SetEnabled(attribute&&typeIndex_==0&&(!overrideMode_||(sameAttachment&&(mask_&4))));
            rate_.SetEnabled(attribute&&typeIndex_==0&&(!overrideMode_||(sameAttachment&&(mask_&8))));
            for(int i=0;i<4;++i)masks_[i].SetEnabled(attribute&&overrideMode_&&sameAttachment&&(i==0||typeIndex_==0));
            for(auto* c:Controls())c->Update(canvas,dt);
        }
        void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const override
        {
            if(!open_)return;
            wi::image::Params background(x_,y_,width_,height_,wi::Color(8,13,19,254));
            background.blendFlag=wi::enums::BLENDMODE_ALPHA;wi::image::Draw(nullptr,background,cmd);
            ApplyScissor(canvas,scissorRect,cmd);
            wi::font::Params title;title.posX=x_+16;title.posY=y_+14;title.size=17;title.color=wi::Color(230,240,248,255);
            wi::font::Draw("GAMEPLAY ATTRIBUTE PROFILES",title,cmd);
            for(auto* c:const_cast<RenegadeGameplayAttributeInspector*>(this)->Controls())
            {ApplyScissor(canvas,scissorRect,cmd);c->Render(canvas,cmd);}
            ApplyScissor(canvas,scissorRect,cmd);
            wi::font::Params info;info.posX=x_+300;info.posY=y_+330;info.size=12;info.h_wrap=width_-320;info.color=wi::Color(160,185,198,255);
            wi::font::Draw("Shared defaults create independent gameplay instances. Instance overrides apply only to the selected scope/Character. Initial values apply at spawn/reset; they are not current Runtime values. Native health and ammunition retain their existing gameplay owners.",info,cmd);
            info.posX=x_+16;info.posY=y_+470;info.size=11;info.h_wrap=width_-32;info.color=wi::Color(238,170,88,255);
            wi::font::Draw(status_,info,cmd);picker_.Render(cmd);
        }
    private:
        static std::string NumberText(double n){std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(17)<<n;return s.str();}
        static bool Number(const std::string& text,double& n)
        {std::istringstream s(text);s.imbue(std::locale::classic());s>>std::noskipws>>n;return !s.fail()&&s.peek()==std::char_traits<char>::eof()&&std::isfinite(n);}
        static bool Optional(const std::string& text,std::optional<double>& n)
        {if(text.empty()){n.reset();return true;}double value=0;if(!Number(text,value))return false;n=value;return true;}
        bool Value(bridge::GameplayAttributeValue& value)
        {
            const auto text=initial_.GetValue();
            if(typeIndex_==0){double n=0;if(!Number(text,n))return false;value=n;}
            else if(typeIndex_==1){if(text!="true"&&text!="false")return false;value=text=="true";}
            else value=text;return true;
        }
        const bridge::GameplayAttributeProfile* Profile() const
        {
            if(!session_.IsLoaded())return nullptr;
            const auto& values=session_.Document().profiles;
            const auto found=std::find_if(values.begin(),values.end(),[&](const auto& p){return p.id==selectedProfile_;});
            return found==values.end()?nullptr:&*found;
        }
        const bridge::GameplayAttributeDefinition* Attribute() const
        {
            const auto* p=Profile();if(!p)return nullptr;
            const auto found=std::find_if(p->attributes.begin(),p->attributes.end(),[&](const auto& a){return a.id==selectedAttribute_;});
            return found==p->attributes.end()?nullptr:&*found;
        }
        const bridge::GameplayAttributeAttachment* Attachment() const
        {
            if(!session_.IsLoaded())return nullptr;const auto& values=session_.Document().attachments;
            const auto found=std::find_if(values.begin(),values.end(),[&](const auto& a){return int(a.scope)==scopeIndex_&&a.targetId==(scopeIndex_==2?actorId_:"");});
            return found==values.end()?nullptr:&*found;
        }
        void Labels()
        {
            const auto* p=Profile();profile_.SetText(p?"PROFILE: "+p->name:"CHOOSE / ADD SHARED PROFILE");
            const auto* a=Attribute();attribute_.SetText(a?"ATTRIBUTE: "+a->id:"NO ATTRIBUTE SELECTED");
            const char* scopes[]={"PROJECT","POSSESSED PLAYER","CHARACTER"};scope_.SetText(std::string("SCOPE: ")+scopes[scopeIndex_]);
            std::string actorName=actorId_.empty()?"CHOOSE...":actorId_.substr(0,8);
            for(const auto& value:actors_.characters)if(value.id==actorId_)actorName=value.name;
            actor_.SetText("CHARACTER: "+actorName);
            const char* types[]={"NUMBER","BOOLEAN","TEXT"};type_.SetText(std::string("TYPE: ")+types[typeIndex_]);
            mode_.SetText(overrideMode_?"EDIT: INSTANCE OVERRIDES":"EDIT: SHARED PROFILE DEFAULTS");
            const char* groups[]={"INITIAL","MINIMUM","MAXIMUM","RATE"};
            for(unsigned i=0;i<4;++i)masks_[i].SetText(std::string(groups[i])+((mask_&(1u<<i))?" // OVERRIDE":" // INHERIT"));
            initial_.SetLiteral(typeIndex_!=0);
            save_.SetText(session_.Dirty()?"SAVE GAMEPLAY MODULE *":"SAVE GAMEPLAY MODULE");
        }
        void LoadFields()
        {
            if(!session_.IsLoaded()){Labels();return;}
            const auto& doc=session_.Document();
            if(!Profile()&&!doc.profiles.empty())selectedProfile_=doc.profiles.front().id;
            const auto* p=Profile();if(p&&!Attribute()&&!p->attributes.empty())selectedAttribute_=p->attributes.front().id;
            profileName_.SetValue(p?p->name:"");const auto* a=Attribute();mask_=0;
            if(a)
            {
                auto effective=*a;
                const auto* attachment=Attachment();
                if(overrideMode_&&attachment&&attachment->profileId==selectedProfile_)
                {
                    std::map<std::string,bridge::GameplayAttributeDefinition> values;std::string error;
                    if(bridge::ResolveGameplayAttributeProfile(*p,attachment->overrides,values,error))effective=values.at(a->id);
                    for(const auto& value:attachment->overrides)if(value.attributeId==a->id)
                        mask_=(value.initial?1u:0u)|(value.minimum?2u:0u)|(value.maximum?4u:0u)|(value.regeneratePerSecond?8u:0u);
                }
                id_.SetValue(a->id);name_.SetValue(a->name);typeIndex_=int(effective.initial.index());
                if(typeIndex_==0)initial_.SetValue(NumberText(std::get<double>(effective.initial)));
                else if(typeIndex_==1)initial_.SetValue(std::get<bool>(effective.initial)?"true":"false");
                else initial_.SetValue(std::get<std::string>(effective.initial));
                minimum_.SetValue(effective.minimum?NumberText(*effective.minimum):"");maximum_.SetValue(effective.maximum?NumberText(*effective.maximum):"");
                rate_.SetValue(NumberText(effective.regeneratePerSecond));
            }
            else{id_.SetValue("");name_.SetValue("");initial_.SetValue("");minimum_.SetValue("");maximum_.SetValue("");rate_.SetValue("0");}
            status_=session_.Dirty()?"GAMEPLAY DRAFT // save explicitly. Screen history is independent.":
                "Gameplay module ready. Attach a profile to activate it for the selected scope.";
            Labels();
        }
        void Commit(bridge::GameplayAttributeDocument doc)
        {std::string error;if(!session_.Replace(std::move(doc),error)){status_=error;return;}LoadFields();}
        void AddProfile()
        {
            if(!session_.IsLoaded())return;auto doc=session_.Document();
            bridge::GameplayAttributeProfile p{bridge::GenerateStableId(),"Resource Profile",{{"energy","Energy",50.0,0.0,100.0,0.0}}};
            if(!profileName_.GetValue().empty())p.name=profileName_.GetValue();
            selectedProfile_=p.id;selectedAttribute_="energy";doc.profiles.push_back(std::move(p));overrideMode_=false;Commit(std::move(doc));
        }
        void RenameProfile()
        {
            if(!Profile())return;auto doc=session_.Document();
            for(auto& p:doc.profiles)if(p.id==selectedProfile_)p.name=profileName_.GetValue();Commit(std::move(doc));
        }
        void RemoveProfile()
        {
            if(!Profile())return;auto doc=session_.Document();
            doc.profiles.erase(std::remove_if(doc.profiles.begin(),doc.profiles.end(),[&](const auto& p){return p.id==selectedProfile_;}),doc.profiles.end());
            // Removing a shared source must remove its attachments in the same history entry.
            doc.attachments.erase(std::remove_if(doc.attachments.begin(),doc.attachments.end(),
                [&](const auto& attachment){return attachment.profileId==selectedProfile_;}),doc.attachments.end());
            Commit(std::move(doc));
        }
        void AddAttribute()
        {
            if(!Profile())return;auto doc=session_.Document();
            for(auto& p:doc.profiles)if(p.id==selectedProfile_)
            {
                unsigned n=1;std::string id;
                do{id="attribute_"+std::to_string(n++);}while(std::any_of(p.attributes.begin(),p.attributes.end(),[&](const auto& a){return a.id==id;}));
                p.attributes.push_back({id,"Attribute",0.0,{},{},0.0});selectedAttribute_=id;
            }
            Commit(std::move(doc));
        }
        void RemoveAttribute()
        {
            if(!Attribute())return;auto doc=session_.Document();
            for(auto& p:doc.profiles)if(p.id==selectedProfile_)p.attributes.erase(
                std::remove_if(p.attributes.begin(),p.attributes.end(),[&](const auto& a){return a.id==selectedAttribute_;}),p.attributes.end());
            // Drop orphan per-instance overrides with the deleted definition.
            for(auto& attachment:doc.attachments)if(attachment.profileId==selectedProfile_)
                attachment.overrides.erase(std::remove_if(attachment.overrides.begin(),attachment.overrides.end(),
                    [&](const auto& value){return value.attributeId==selectedAttribute_;}),attachment.overrides.end());
            Commit(std::move(doc));
        }
        void Attach(bool remove)
        {
            if(!session_.IsLoaded()||(!remove&&!Profile()))return;
            if(scopeIndex_==2&&!bridge::IsValidStableId(actorId_)){status_="Choose a real Character identity first.";return;}
            auto doc=session_.Document();const auto target=scopeIndex_==2?actorId_:std::string{};
            doc.attachments.erase(std::remove_if(doc.attachments.begin(),doc.attachments.end(),[&](const auto& a){return int(a.scope)==scopeIndex_&&a.targetId==target;}),doc.attachments.end());
            if(!remove)doc.attachments.push_back({static_cast<bridge::GameplayAttributeScope>(scopeIndex_),target,selectedProfile_,{}});
            Commit(std::move(doc));
        }
        void Apply()
        {
            const auto* a=Attribute();if(!a)return;auto doc=session_.Document();std::string newId=selectedAttribute_;
            if(overrideMode_)
            {
                const auto* current=Attachment();if(!current||current->profileId!=selectedProfile_){status_="Attach the selected profile to this scope first.";return;}
                bridge::GameplayAttributeOverride value;value.attributeId=selectedAttribute_;double n=0;
                if(mask_&1){bridge::GameplayAttributeValue initial;if(!Value(initial)){status_="Invalid initial override.";return;}value.initial=std::move(initial);}
                if(mask_&2){if(!Number(minimum_.GetValue(),n)){status_="Minimum override needs a finite number.";return;}value.minimum=n;}
                if(mask_&4){if(!Number(maximum_.GetValue(),n)){status_="Maximum override needs a finite number.";return;}value.maximum=n;}
                if(mask_&8){if(!Number(rate_.GetValue(),n)){status_="Rate override needs a finite number.";return;}value.regeneratePerSecond=n;}
                for(auto& attachment:doc.attachments)if(int(attachment.scope)==scopeIndex_&&attachment.targetId==current->targetId)
                {
                    auto& values=attachment.overrides;values.erase(std::remove_if(values.begin(),values.end(),[&](const auto& v){return v.attributeId==selectedAttribute_;}),values.end());
                    if(mask_)values.push_back(std::move(value));
                }
            }
            else
            {
                auto value=*a;value.id=id_.GetValue();value.name=name_.GetValue();newId=value.id;
                if(!Value(value.initial)||!Optional(minimum_.GetValue(),value.minimum)||!Optional(maximum_.GetValue(),value.maximum)||
                    !Number(rate_.GetValue(),value.regeneratePerSecond)){status_="Invalid typed initial value, bounds or rate.";return;}
                for(auto& p:doc.profiles)if(p.id==selectedProfile_)for(auto& attribute:p.attributes)if(attribute.id==selectedAttribute_)attribute=value;
                for(auto& attachment:doc.attachments)if(attachment.profileId==selectedProfile_)
                    for(auto& v:attachment.overrides)if(v.attributeId==selectedAttribute_)v.attributeId=newId;
            }
            std::string error;if(!session_.Replace(std::move(doc),error)){status_=error;return;}
            selectedAttribute_=newId;LoadFields();
        }
        void Save()
        {
            std::string error;if(!session_.Save(error)){status_=error;return;}
            const auto& doc=session_.Document();
            if(!attachCallback_||!attachCallback_(doc.envelope.documentId,doc.envelope.pathHint,error))
            {status_="Profile file saved; project attachment failed: "+error+" // "+session_.Path();return;}
            documentId_=doc.envelope.documentId;hint_=doc.envelope.pathHint;
            if(savedCallback_)savedCallback_(doc);Labels();status_="GAMEPLAY MODULE SAVED // "+doc.envelope.pathHint;
        }
        void Reload()
        {
            if(documentId_.empty()){status_="Save a module before reloading it.";return;}
            std::string path,error;
            if(!bridge::ResolveGameplayAttributeDocumentPath(root_,projectId_,documentId_,hint_,path,error)||
                !session_.Open(path,projectId_,error)){status_=error;return;}LoadFields();
        }
        void PickProfile()
        {
            if(!session_.IsLoaded())return;std::vector<RenegadeScreenChoicePicker::Choice> choices;
            for(const auto& p:session_.Document().profiles)choices.push_back({p.name,p.id});
            picker_.Open("SHARED GAMEPLAY PROFILE",std::move(choices),selectedProfile_,[this](const std::string& id){selectedProfile_=id;selectedAttribute_.clear();LoadFields();});
        }
        void PickAttribute()
        {
            const auto* p=Profile();if(!p)return;std::vector<RenegadeScreenChoicePicker::Choice> choices;
            for(const auto& a:p->attributes)choices.push_back({a.name+" // "+a.id,a.id});
            picker_.Open("GAMEPLAY ATTRIBUTE",std::move(choices),selectedAttribute_,[this](const std::string& id){selectedAttribute_=id;LoadFields();});
        }
        void PickActor()
        {
            std::vector<RenegadeScreenChoicePicker::Choice> choices;
            for(const auto& actor:actors_.characters)choices.push_back({actor.name+" // "+actor.id.substr(0,8),actor.id});
            picker_.Open("CURRENT LEVEL CHARACTER",std::move(choices),actorId_,[this](const std::string& id){actorId_=id;LoadFields();});
        }
        void Layout()
        {
            const float left=x_+16,lw=264,right=x_+300,rw=width_-316,half=(lw-6)*.5f,third=(rw-12)/3,quarter=(rw-18)/4;
            const auto place=[](wi::gui::Widget& c,float x,float y,float w){c.SetVisible(true);c.SetPos(XMFLOAT2(x,y));c.SetSize(XMFLOAT2(w,28));};
            place(close_,x_+width_-100,y_+8,84);place(profile_,left,y_+52,lw);place(profileName_,left,y_+88,lw);
            place(addProfile_,left,y_+124,half);place(renameProfile_,left+half+6,y_+124,half);place(removeProfile_,left,y_+160,lw);
            place(scope_,left,y_+196,lw);place(actor_,left,y_+232,lw);place(attach_,left,y_+268,lw);place(detach_,left,y_+304,lw);
            place(undo_,left,y_+340,half);place(redo_,left+half+6,y_+340,half);place(save_,left,y_+376,lw);place(reload_,left,y_+412,lw);
            place(attribute_,right,y_+52,rw);place(id_,right,y_+88,(rw-6)*.5f);place(name_,right+(rw+6)*.5f,y_+88,(rw-6)*.5f);
            place(type_,right,y_+124,third);place(initial_,right+third+6,y_+124,rw-third-6);
            place(minimum_,right,y_+160,third);place(maximum_,right+third+6,y_+160,third);place(rate_,right+2*(third+6),y_+160,third);
            place(mode_,right,y_+196,rw);for(int i=0;i<4;++i)place(masks_[i],right+i*(quarter+6),y_+232,quarter);
            place(apply_,right,y_+268,third);place(addAttribute_,right+third+6,y_+268,third);place(removeAttribute_,right+2*(third+6),y_+268,third);
        }
        std::vector<wi::gui::Widget*> Controls()
        {
            return {&close_,&profile_,&profileName_,&addProfile_,&renameProfile_,&removeProfile_,&scope_,&actor_,&attach_,&detach_,&undo_,&redo_,&save_,&reload_,
                &attribute_,&id_,&name_,&type_,&initial_,&minimum_,&maximum_,&rate_,&mode_,&masks_[0],&masks_[1],&masks_[2],&masks_[3],&apply_,&addAttribute_,&removeAttribute_};
        }
        bridge::GameplayAttributeAuthoringSession session_;bridge::ScreenBindingActorCatalogue actors_;
        std::string root_,projectId_,documentId_,hint_,selectedProfile_,selectedAttribute_,actorId_,status_;
        AttachCallback attachCallback_;SavedCallback savedCallback_;bool open_=false,overrideMode_=false;int scopeIndex_=0,typeIndex_=0;unsigned mask_=0;
        float x_=0,y_=0,width_=1120,height_=600;
        RenegadeButton close_,profile_,addProfile_,renameProfile_,removeProfile_,scope_,actor_,attach_,detach_,undo_,redo_,save_,reload_;
        RenegadeButton attribute_,type_,mode_,masks_[4],apply_,addAttribute_,removeAttribute_;
        RenegadeScreenLiteralTextInput profileName_,id_,name_,initial_,minimum_,maximum_,rate_;RenegadeScreenChoicePicker picker_;
    };
}
