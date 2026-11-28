#pragma once
#include "StudioMultilineInputRoute.h"
#include "StudioButtonVisual.h"
#include <WickedEngine.h>
#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace renegade::studio
{
    // Native Studio control: literal Unicode, multiline selection, clipboard and scrolling.
    // Uses the normal native character stream without introducing a child OS window.
    class StudioMultilineTextInput final : public wi::gui::Widget
    {
        struct Row{size_t begin,end;};
        std::wstring text_;
        size_t caret_=0,anchor_=0;
        wchar_t pendingHigh_=0;
        std::vector<Row> rows_;
        std::vector<std::wstring> undo_,redo_;
        bool focused_=false,dragging_=false,dirty_=true;
        float width_=-1;
        int firstRow_=0;
        wi::gui::TextInputField typingGate_;
        std::array<float,7> repeats_{};
        static constexpr float LineHeight=23;
        wi::font::Params Metrics()const {wi::font::Params p(0,0,14);p.bolden=.12f;return p;}
        float Measure(size_t begin,size_t end)const
        {return wi::font::TextSize(text_.c_str()+begin,static_cast<int>(end-begin),Metrics()).x;}
        static bool High(wchar_t c){return c>=0xd800&&c<=0xdbff;}
        static bool Low(wchar_t c){return c>=0xdc00&&c<=0xdfff;}
        size_t Before(size_t i)const {if(!i)return 0;--i;if(i&&Low(text_[i])&&High(text_[i-1]))--i;return i;}
        size_t After(size_t i)const {if(i>=text_.size())return text_.size();return i+((High(text_[i])&&i+1<text_.size()&&Low(text_[i+1]))?2:1);}
        std::string Utf8(const std::wstring& value)const
        {std::string result;wi::helper::StringConvert(value,result);return result;}
        void Replace(std::wstring value)
        {
            value.erase(std::remove(value.begin(),value.end(),L'\r'),value.end());
            const size_t begin=std::min(caret_,anchor_),end=std::max(caret_,anchor_);
            auto next=text_;next.replace(begin,end-begin,value);
            if(Utf8(next).size()>16384)return;
            undo_.push_back(text_);if(undo_.size()>64)undo_.erase(undo_.begin());redo_.clear();
            text_=std::move(next);caret_=anchor_=begin+value.size();dirty_=true;
        }
        void Layout()
        {
            const float width=std::max(20.f,scale.x-26);
            if(!dirty_&&width==width_)return;
            width_=width;rows_.clear();size_t begin=0,pos=0;float used=0;
            while(pos<text_.size())
            {
                if(text_[pos]==L'\n'){rows_.push_back({begin,pos});begin=++pos;used=0;continue;}
                const auto next=After(pos);const float advance=Measure(pos,next);
                if(used+advance>width && pos>begin){rows_.push_back({begin,pos});begin=pos;used=0;}
                used+=advance;pos=next;
            }
            rows_.push_back({begin,text_.size()});dirty_=false;
        }
        int CaretRow()const
        {
            int row=0;
            for(size_t i=1;i<rows_.size()&&rows_[i].begin<=caret_;++i)row=static_cast<int>(i);
            return row;
        }
        size_t AtX(const Row& row,float x)const
        {
            float used=0;
            for(size_t i=row.begin;i<row.end;)
            {const auto next=After(i);const float advance=Measure(i,next);if(x<used+advance*.5f)return i;used+=advance;i=next;}
            return row.end;
        }
        bool Repeat(wi::input::BUTTON key,int index,float dt)
        {
            if(wi::input::Press(key)){repeats_[index]=0;return true;}
            if(!wi::input::Down(key)){repeats_[index]=0;return false;}
            const float before=repeats_[index];repeats_[index]+=dt;
            return repeats_[index]>.4f && int((repeats_[index]-.4f)/.045f)!=int(std::max(0.f,before-.4f)/.045f);
        }
        bool Character(wchar_t ch)
        {
            if(!focused_)return false;
            if(High(ch)){pendingHigh_=ch;return true;}
            if(Low(ch)&&pendingHigh_){Replace(std::wstring{pendingHigh_,ch});pendingHigh_=0;return true;}
            if(pendingHigh_&&ch==L'\b'){pendingHigh_=0;return true;}
            pendingHigh_=0;
            if(ch==26||ch==25)
            {
                auto& from=ch==26?undo_:redo_;auto& to=ch==26?redo_:undo_;
                if(!from.empty()){to.push_back(text_);text_=std::move(from.back());from.pop_back();
                    caret_=anchor_=text_.size();dirty_=true;}
                return true;
            }
            if(ch==1){anchor_=0;caret_=text_.size();return true;}
            if(ch==3||ch==24)
            {
                const auto a=std::min(caret_,anchor_),b=std::max(caret_,anchor_);
                wi::helper::SetClipboardText(text_.substr(a,b-a));
                if(ch==24)Replace(L"");return true;
            }
            if(ch==22){Replace(wi::helper::GetClipboardText());return true;}
            if(ch==L'\b')
            {
                if(caret_==anchor_)anchor_=Before(caret_);
                Replace(L"");return true;
            }
            if(ch==L'\r'||ch==L'\n'){Replace(L"\n");return true;}
            if(ch>=32&&ch!=127)Replace(std::wstring(1,ch));
            return true;
        }
        void Focus()
        {
            focused_=true;Widget::Activate();typingGate_.Activate();
            MultilineInputRoute()={this,[](void* owner,wchar_t ch){return static_cast<StudioMultilineTextInput*>(owner)->Character(ch);}};
        }
    public:
        StudioMultilineTextInput(){typingGate_.Create("Studio multiline typing ownership");SetShadowRadius(0);}
        ~StudioMultilineTextInput(){Blur();}
        void Blur()
        {
            if(MultilineInputRoute().owner==this){MultilineInputRoute()={};typingGate_.Deactivate();}
            focused_=dragging_=false;state=wi::gui::IDLE;priority_change=false;pendingHigh_=0;
        }
        void SetValue(const std::string& value)
        {
            Blur();wi::helper::StringConvert(value,text_);
            text_.erase(std::remove(text_.begin(),text_.end(),L'\r'),text_.end());
            caret_=anchor_=text_.size();firstRow_=0;dirty_=true;undo_.clear();redo_.clear();
        }
        std::string GetValue()const{return Utf8(text_);}
        void Update(const wi::Canvas& canvas,float dt)override
        {
            Widget::Update(canvas,dt);
            if(!IsVisible()||!IsEnabled()){Blur();return;}
            Layout();
            const auto p=wi::input::GetPointer();
            const bool inside=p.x>=translation.x&&p.x<translation.x+scale.x&&p.y>=translation.y&&p.y<translation.y+scale.y;
            const int visible=std::max(1,int((scale.y-16)/LineHeight));
            if(inside&&p.z!=0)firstRow_=std::clamp(firstRow_+(p.z<0?2:-2),0,std::max(0,int(rows_.size())-visible));
            if(wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
            {
                if(!inside){Blur();return;}
                Focus();
                const int row=std::clamp(firstRow_+int((p.y-translation.y-8)/LineHeight),0,int(rows_.size())-1);
                caret_=anchor_=AtX(rows_[row],p.x-translation.x-10);dragging_=true;
            }
            if(dragging_&&wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))
            {
                const int row=std::clamp(firstRow_+int((p.y-translation.y-8)/LineHeight),0,int(rows_.size())-1);
                caret_=AtX(rows_[row],p.x-translation.x-10);
            }
            else dragging_=false;
            if(!focused_)return;
            const bool shift=wi::input::Down(wi::input::KEYBOARD_BUTTON_LSHIFT)||wi::input::Down(wi::input::KEYBOARD_BUTTON_RSHIFT);
            bool moved=false;const int row=CaretRow();
            if(Repeat(wi::input::KEYBOARD_BUTTON_LEFT,0,dt)){caret_=Before(caret_);moved=true;}
            if(Repeat(wi::input::KEYBOARD_BUTTON_RIGHT,1,dt)){caret_=After(caret_);moved=true;}
            if(Repeat(wi::input::KEYBOARD_BUTTON_UP,2,dt)&&row>0){caret_=AtX(rows_[row-1],Measure(rows_[row].begin,caret_));moved=true;}
            if(Repeat(wi::input::KEYBOARD_BUTTON_DOWN,3,dt)&&row+1<int(rows_.size())){caret_=AtX(rows_[row+1],Measure(rows_[row].begin,caret_));moved=true;}
            if(Repeat(wi::input::KEYBOARD_BUTTON_HOME,4,dt)){caret_=rows_[row].begin;moved=true;}
            if(Repeat(wi::input::KEYBOARD_BUTTON_END,5,dt)){caret_=rows_[row].end;moved=true;}
            if(moved&&!shift)anchor_=caret_;
            if(Repeat(wi::input::KEYBOARD_BUTTON_DELETE,6,dt)){if(caret_==anchor_)anchor_=After(caret_);Replace(L"");}
            Layout();
            if(!inside||p.z==0)
            {
                const int current=CaretRow();
                if(current<firstRow_)firstRow_=current;
                if(current>=firstRow_+visible)firstRow_=current-visible+1;
            }
        }
        void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd)const override
        {
            if(!IsVisible())return;
            ApplyScissor(canvas,scissorRect,cmd);
            theme::ButtonRect(translation.x,translation.y,scale.x,scale.y,focused_?theme::Accent:theme::Border,cmd);
            theme::ButtonRect(translation.x+1,translation.y+1,scale.x-2,scale.y-2,theme::Idle,cmd,3);
            auto clip=scissorRect;clip.left=std::max(clip.left,int(translation.x+6));clip.right=std::min(clip.right,int(translation.x+scale.x-6));
            clip.top=std::max(clip.top,int(translation.y+5));clip.bottom=std::min(clip.bottom,int(translation.y+scale.y-5));ApplyScissor(canvas,clip,cmd);
            const auto begin=std::min(caret_,anchor_),end=std::max(caret_,anchor_);
            const int visible=std::max(1,int((scale.y-16)/LineHeight));
            for(int i=firstRow_;i<int(rows_.size())&&i<firstRow_+visible;++i)
            {
                const auto row=rows_[i];const float y=translation.y+8+(i-firstRow_)*LineHeight,x=translation.x+10;
                const auto a=std::max(begin,row.begin),b=std::min(end,row.end);
                if(focused_&&a<b)theme::ButtonRect(x+Measure(row.begin,a),y,std::max(2.f,Measure(a,b)),20,theme::Selected,cmd,0);
                auto params=Metrics();params.posX=x;params.posY=y;params.color=theme::Text;
                const auto text=text_.substr(row.begin,row.end-row.begin);wi::font::Draw(text,params,cmd);
                if(focused_&&i==CaretRow())theme::ButtonRect(x+Measure(row.begin,caret_),y,1,19,theme::Text,cmd,0);
            }
            if(text_.empty()&&!focused_)
            {auto p=Metrics();p.posX=translation.x+10;p.posY=translation.y+8;p.color=theme::Muted;wi::font::Draw("Leave blank for no caption.",p,cmd);}
            ApplyScissor(canvas,scissorRect,cmd);
        }
    };
}
