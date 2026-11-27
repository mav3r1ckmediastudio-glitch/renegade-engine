#pragma once
#include "StudioModalWindow.h"
#include "StudioMultilineTextInput.h"
#include "RenegadeStudioChrome.h"
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>

namespace renegade::studio
{
    class ScreenCarouselEditor final : public StudioModalWindow,
        public std::enable_shared_from_this<ScreenCarouselEditor>
    {
        class Picture final : public wi::gui::Widget
        {
        public:
            wi::Resource image;
            void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd)const override
            {
                if(!IsVisible())return;
                ApplyScissor(canvas,scissorRect,cmd);
                theme::ButtonRect(translation.x,translation.y,scale.x,scale.y,theme::Idle,cmd,6);
                if(image.IsValid())
                {
                    const auto& texture=image.GetTexture();
                    const auto& size=texture.GetDesc();
                    const float factor=std::min(scale.x/size.width,scale.y/size.height);
                    const float w=size.width*factor,h=size.height*factor;
                    wi::image::Params p(translation.x+(scale.x-w)/2,translation.y+(scale.y-h)/2,w,h);
                    wi::image::Draw(&texture,p,cmd);
                }
            }
        } picture_;
        bridge::ScreenCarousel deck_;
        std::string root_,status_;
        bridge::StableId project_;
        int selected_=-1,pendingSelection_=-1;
        wi::gui::Window* content_=nullptr;
        std::uint64_t generation_=0;
        std::function<bool(bridge::ScreenCarousel,std::string&)> save_;
        std::function<void()> closed_;
        wi::gui::TreeList entries_;
        StudioMultilineTextInput caption_;
        RenegadeTextInputField seconds_;
        RenegadeCheckBox shuffle_;
        RenegadeButton add_,replace_,remove_,up_,down_,previous_,next_,saveButton_,cancelButton_;
        std::vector<wi::gui::Widget*> bodyControls_;
        std::string imageLabel_;
        void Add(wi::gui::Widget& control,float x,float y,float w,float h)
        {
            control.SetPos({x,y});control.SetSize({w,h});control.SetShadowRadius(0);
            AddWidget(&control);bodyControls_.push_back(&control);
        }
        void Button(RenegadeButton& button,const char* text,float x,float y,float w)
        {button.Create(text);button.SetText(text);button.SetAccentOutlineStyle(true);button.SetRenderTextSize(14);Add(button,x,y,w,32);}
        void KeepCaption()
        {if(selected_>=0&&selected_<static_cast<int>(deck_.entries.size()))deck_.entries[selected_].caption=caption_.GetValue();}
        void Rows()
        {
            entries_.ClearItems();
            for(size_t i=0;i<deck_.entries.size();++i)
            {
                wi::gui::TreeList::Item row;
                row.name=std::to_string(i+1)+". "+std::filesystem::u8path(deck_.entries[i].image).filename().u8string();
                row.userdata=i;row.selected=static_cast<int>(i)==selected_;entries_.AddItem(row);
            }
        }
        void Select(int index)
        {
            KeepCaption();selected_=index;Rows();picture_.image={};
            const bool ready=selected_>=0&&selected_<static_cast<int>(deck_.entries.size());
            caption_.SetValue(ready?deck_.entries[selected_].caption:"");caption_.SetEnabled(ready);
            for(auto* button:{&replace_,&remove_,&up_,&down_,&previous_,&next_})button->SetEnabled(ready);
            up_.SetEnabled(ready&&selected_>0);
            down_.SetEnabled(ready&&selected_+1<static_cast<int>(deck_.entries.size()));
            imageLabel_=ready?"Image "+std::to_string(selected_+1)+" of "+std::to_string(deck_.entries.size())+
                "  /  "+std::filesystem::u8path(deck_.entries[selected_].image).filename().u8string():"Add an image to begin.";
            if(ready)
            {
                std::string file,error;
                if(bridge::ResolveScreenResourcePath(root_,deck_.entries[selected_].image,file,error))
                    picture_.image=wi::resourcemanager::Load(file);
                if(!picture_.image.IsValid())status_="Image preview unavailable. "+error;
            }
        }
        void Navigate(int direction,bool move)
        {
            if(deck_.entries.empty())return;
            KeepCaption();
            const int count=static_cast<int>(deck_.entries.size());
            const int target=move?std::clamp(selected_+direction,0,count-1):(selected_+direction+count)%count;
            if(move)std::swap(deck_.entries[selected_],deck_.entries[target]);
            // Caption already belongs to its entry; do not write it into the swapped entry.
            selected_=-1;Select(target);
        }
        void Choose(bool replace)
        {
            if(!replace&&deck_.entries.size()>=256){status_="A carousel supports up to 256 images.";return;}
            KeepCaption();caption_.Blur();
            const auto generation=generation_;
            const auto target=replace&&selected_>=0?deck_.entries[selected_].id:bridge::StableId{};
            const auto weak=weak_from_this();
            wi::helper::FileDialogParams dialog;dialog.type=wi::helper::FileDialogParams::OPEN;
            dialog.description="Choose carousel image";dialog.extensions={"png","jpg","jpeg","bmp","tga","dds","hdr"};
            wi::helper::FileDialog(dialog,[weak,generation,target](const std::string& source){
                if(source.empty())return;
                wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [weak,generation,target,source](std::uint64_t){
                        const auto owner=weak.lock();
                        if(!owner||!owner->IsVisible()||owner->generation_!=generation)return;
                        owner->Import(source,target);
                    });
            });
        }
        void Import(const std::string& source,const bridge::StableId& target)
        {
            namespace fs=std::filesystem;
            const auto format=bridge::DetectResourceSourceFormat(source);
            if(bridge::ClassifyResourceSourceFormat(format)!=bridge::ResourceClass::Texture)
            {status_="Choose a supported image.";return;}
            std::string relative,absolute,error;
            const auto candidate=fs::u8path(source).lexically_relative(fs::u8path(root_)).generic_u8string();
            if(bridge::ResolveScreenResourcePath(root_,candidate,absolute,error))relative=candidate;
            else
            {
                const auto imported=bridge::CreatorTextureWorkflowService{}.ImportTexture(root_,project_,source);
                if(!imported.succeeded){status_=imported.error;return;}
                const auto image=fs::path("Content/UI/Images")/fs::u8path(imported.assetId+fs::u8path(source).extension().u8string());
                std::error_code ec;fs::create_directories((fs::u8path(root_)/image).parent_path(),ec);
                if(!ec)fs::copy_file(fs::u8path(root_)/fs::u8path(imported.sourceProjectRelativePath),
                    fs::u8path(root_)/image,fs::copy_options::overwrite_existing,ec);
                if(ec){status_=ec.message();return;}
                relative=image.generic_u8string();
            }
            KeepCaption();
            if(!target.empty())
            {
                const auto found=std::find_if(deck_.entries.begin(),deck_.entries.end(),[&](const auto& entry){return entry.id==target;});
                if(found==deck_.entries.end()){status_="The selected entry no longer exists.";return;}
                found->image=relative;selected_=-1;Select(static_cast<int>(found-deck_.entries.begin()));
            }
            else
            {
                deck_.entries.push_back({bridge::GenerateStableId(),relative,{}});
                selected_=-1;Select(static_cast<int>(deck_.entries.size())-1);
            }
            status_.clear();
        }
        void Save()
        {
            KeepCaption();caption_.Blur();
            const auto value=seconds_.GetState()==wi::gui::ACTIVE?seconds_.GetCurrentInputValue():seconds_.GetValue();
            char* end=nullptr;const float seconds=std::strtof(value.c_str(),&end);
            if(end==value.c_str()||*end||!std::isfinite(seconds)||seconds<.5f||seconds>3600)
            {status_="Seconds per image must be between 0.5 and 3600.";return;}
            deck_.seconds=seconds;deck_.shuffle=shuffle_.GetCheck();
            std::string error;
            auto accept=save_;
            if(accept&&accept(deck_,error))Close();
            else status_=error.empty()?"Could not save the carousel.":error;
        }
        static void Label(const std::string& text,float x,float y,int size,wi::Color colour,wi::graphics::CommandList cmd,float wrap=-1)
        {wi::font::Params p(x,y,size);p.color=colour;p.bolden=.15f;p.h_wrap=wrap;wi::font::Draw(text,p,cmd);}
    public:
        ~ScreenCarouselEditor(){caption_.Blur();RemoveWidgets();}
        void Create()
        {
            StudioModalWindow::Create("Edit carousel",WindowControls::CLOSE|WindowControls::MOVE);
            content_=&ContentWindow();
            SetSize({1120,850});SetControlSize(32);SetShadowRadius(0);
            entries_.Create("Carousel images");entries_.SetText("");Add(entries_,34,64,272,482);
            entries_.OnSelect([this](const wi::gui::EventArgs& a){
                if(a.userdata<deck_.entries.size()&&selected_!=static_cast<int>(a.userdata))pendingSelection_=static_cast<int>(a.userdata);
            });
            picture_.SetName("Carousel image preview");Add(picture_,358,64,724,350);
            Button(add_,"Add image...",34,568,132);add_.OnClick([this](const auto&){Choose(false);});
            Button(replace_,"Change image...",174,568,132);replace_.OnClick([this](const auto&){Choose(true);});
            Button(remove_,"Remove",34,612,84);
            remove_.OnClick([this](const auto&){
                if(selected_<0)return;const int index=selected_;
                deck_.entries.erase(deck_.entries.begin()+index);selected_=-1;
                Select(deck_.entries.empty()?-1:std::min(index,static_cast<int>(deck_.entries.size())-1));
            });
            Button(up_,"Move up",126,612,84);up_.OnClick([this](const auto&){Navigate(-1,true);});
            Button(down_,"Move down",218,612,88);down_.OnClick([this](const auto&){Navigate(1,true);});
            Button(previous_,"Previous",358,652,112);previous_.OnClick([this](const auto&){Navigate(-1,false);});
            Button(next_,"Next",482,652,112);next_.OnClick([this](const auto&){Navigate(1,false);});
            caption_.SetName("Carousel caption");Add(caption_,358,482,724,150);
            seconds_.Create("Carousel seconds");seconds_.SetDescription("");seconds_.SetRenderTextSize(14);
            Add(seconds_,160,0,76,30);PinFooter(seconds_,160,12);
            shuffle_.Create("Shuffle images");shuffle_.SetText("Shuffle images");shuffle_.SetRenderTextSize(14);
            Add(shuffle_,0,0,20,20);PinFooter(shuffle_,280,18);
            Button(saveButton_,"Save",0,0,116);PinFooter(saveButton_,260,12,true);
            saveButton_.OnClick([this](const auto&){Save();});
            Button(cancelButton_,"Cancel",0,0,116);PinFooter(cancelButton_,132,12,true);
            cancelButton_.OnClick([this](const auto&){Close();});
            OnClose([this](const auto&){Close();});
            wi::gui::Theme skin;skin.image.background=false;skin.image.color=theme::Idle;
            skin.font.color=theme::Text;skin.font.bolden=.12f;skin.shadow=0;
            SetTheme(skin);SetColor(theme::Panel,wi::gui::WIDGET_ID_WINDOW_BASE);
            entries_.SetColor(theme::Idle,wi::gui::IDLE);entries_.SetColor(theme::Focus,wi::gui::FOCUS);
            entries_.SetColor(theme::Selected,wi::gui::ACTIVE);SetVisible(false);
        }
        void Open(bridge::ScreenCarousel deck,std::string root,bridge::StableId project,
            std::function<bool(bridge::ScreenCarousel,std::string&)> save,std::function<void()> closed)
        {
            Close();++generation_;deck_=std::move(deck);root_=std::move(root);project_=std::move(project);
            save_=std::move(save);closed_=std::move(closed);status_.clear();selected_=-1;
            seconds_.SetValue(deck_.seconds);shuffle_.SetCheck(deck_.shuffle);
            Select(deck_.entries.empty()?-1:0);SetVisible(true);
        }
        void Close()
        {
            caption_.Blur();++generation_;pendingSelection_=-1;SetVisible(false);picture_.image={};
            if(closed_){auto callback=std::move(closed_);callback();}
            save_={};
        }
        void Update(const wi::Canvas& canvas,float dt)override
        {
            if(!IsVisible())return;
            if(wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
            {
                const auto pointer=wi::input::GetPointer();const auto p=caption_.GetPos(),s=caption_.GetSize();
                if(pointer.x<p.x||pointer.x>=p.x+s.x||pointer.y<p.y||pointer.y>=p.y+s.y)caption_.Blur();
                // Preserve a numeric draft before the native field cancels on an outside click.
                const auto n=seconds_.GetPos(),z=seconds_.GetSize();
                if(seconds_.GetState()==wi::gui::ACTIVE&&
                    (pointer.x<n.x||pointer.x>=n.x+z.x||pointer.y<n.y||pointer.y>=n.y+z.y))
                {const auto value=seconds_.GetCurrentInputValue();seconds_.Deactivate();seconds_.SetValue(value);}

            }
            StudioModalWindow::Update(canvas,dt);
            if(IsVisible()&&pendingSelection_>=0)
            {const int index=pendingSelection_;pendingSelection_=-1;Select(index);}
        }
        void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd)const override
        {
            if(!IsVisible())return;
            const auto p=GetPos(),s=GetSize();ApplyScissor(canvas,scissorRect,cmd);
            theme::ButtonRect(p.x,p.y,s.x,s.y,theme::Border,cmd,8);
            theme::ButtonRect(p.x+1,p.y+1,s.x-2,s.y-2,theme::Canvas,cmd,7);
            theme::ButtonRect(p.x+1,p.y+1,s.x-2,38,theme::Panel,cmd,6);
            Label("EDIT CAROUSEL",p.x+18,p.y+9,18,theme::Text,cmd);
            const auto close=closeButton.GetPos(),closeSize=closeButton.GetSize();
            theme::DrawButton(close.x,close.y,closeSize.x,closeSize.y,"X",
                {true,closeButton.GetState()==wi::gui::FOCUS,closeButton.GetState()==wi::gui::ACTIVE},14,cmd);
            const auto origin=ContentOrigin();
            ApplyScissor(canvas,ContentWindowClip(),cmd);
            theme::ButtonRect(origin.x+18,origin.y+16,304,694,theme::Panel,cmd,6);
            theme::ButtonRect(origin.x+342,origin.y+16,756,694,theme::Panel,cmd,6);
            Label("IMAGES",origin.x+34,origin.y+32,15,theme::Accent,cmd);
            Label("IMAGE PREVIEW",origin.x+358,origin.y+32,15,theme::Accent,cmd);
            Label(imageLabel_,origin.x+358,origin.y+426,13,theme::Muted,cmd,710);
            Label("CAPTION (OPTIONAL)",origin.x+358,origin.y+456,15,theme::Accent,cmd);
            Label("Font and position: select Carousel caption in Layers.",origin.x+610,origin.y+661,12,theme::Muted,cmd,470);
            for(auto* control:bodyControls_)
            {
                if(control==&seconds_||control==&shuffle_||control==&saveButton_||control==&cancelButton_)continue;
                control->Render(canvas,cmd);
            }
            if(content_)
            {content_->scrollbar_vertical.Render(canvas,cmd);content_->scrollbar_horizontal.Render(canvas,cmd);}
            ApplyScissor(canvas,scissorRect,cmd);
            Label("Seconds per image",p.x+18,p.y+s.y-58,13,theme::Muted,cmd);
            for(auto* control:std::array<wi::gui::Widget*,4>{const_cast<RenegadeTextInputField*>(&seconds_),
                const_cast<RenegadeCheckBox*>(&shuffle_),const_cast<RenegadeButton*>(&saveButton_),const_cast<RenegadeButton*>(&cancelButton_)})
            {ApplyScissor(canvas,scissorRect,cmd);control->Render(canvas,cmd);}
            if(!status_.empty())Label(status_,p.x+18,p.y+s.y-24,12,theme::Accent,cmd,s.x-36);
        }
        wi::graphics::Rect ContentWindowClip()const
        {
            const auto origin=ContentOrigin();const auto size=GetSize();
            auto rect=scissorRect;rect.left=std::max(rect.left,int(GetPos().x));
            rect.top=std::max(rect.top,int(GetPos().y+GetControlSize()));
            rect.right=std::min(rect.right,int(GetPos().x+size.x));
            rect.bottom=std::min(rect.bottom,int(GetPos().y+size.y-76));return rect;
        }
    };
}
