#pragma once
#include "StudioButtonVisual.h"
#include <array>
#include <vector>
#include <string_view>

namespace renegade::studio::icons
{
    struct Entry { const char* id; const char* title; };
    inline constexpr Entry Entries[] = {
        {"select", "Select"},
        {"move", "Move / Translate"},
        {"rotate", "Rotate"},
        {"scale", "Scale"},
        {"pan", "Pan"},
        {"focus", "Focus"},
        {"undo", "Undo"},
        {"redo", "Redo"},
        {"scene", "Scene"},
        {"environment", "Environment"},
        {"terrain", "Terrain"},
        {"render", "Render Settings"},
        {"physics", "Physics Lab"},
        {"audio", "Audio"},
        {"characters", "Characters"},
        {"storyflow", "StoryFlow"},
        {"prefabs", "Prefabs"},
        {"particles", "Particles"},
        {"vegetation", "Vegetation"},
        {"water", "Water"},
        {"weather", "Weather"},
        {"lighting", "Lighting"},
        {"assets", "Assets"},
        {"layers", "Layers"},
        {"hierarchy", "Hierarchy"},
        {"inspector", "Inspector"},
        {"settings", "Settings"},
        {"play", "Play"},
        {"stop", "Stop"},
        {"save", "Save"},
        {"add", "Add"},
        {"delete", "Delete"},
        {"duplicate", "Duplicate"},
        {"link", "Link"},
        {"unlink", "Unlink"},
        {"visibility", "Visibility"},
        {"hide", "Hide"},
        {"search", "Search"},
        {"filter", "Filter"},
        {"arrange", "Arrange"},
        {"build-game", "Build Game"},
        {"validate", "Validate"},
        {"hub", "Project Hub"},
        {"levels", "Levels"},
        {"screens", "Screens"},
        {"variables", "Variables"},
        {"journey", "Journey View"},
        {"graph", "Graph View"},
        {"game-start", "Game Start"},
        {"level", "Level Destination"},
        {"victory", "Complete Game"},
        {"dialogue", "Dialogue Line"},
        {"choice", "Choice"},
        {"branch", "Branch"},
        {"condition", "Condition"},
        {"event", "Action / Event"},
        {"flag", "Flag"},
        {"quest", "Quest"},
        {"camera", "Camera"},
        {"delay", "Delay / Wait"},
        {"note", "Note"},
        {"refresh", "Refresh"},
        {"fit", "Fit View"},
        {"return", "Return"},
        {"quit", "Quit"},    };
    inline wi::Resource Atlas;
    inline void Load()
    {
        if (!Atlas.IsValid()) Atlas = wi::resourcemanager::Load("Content/ui/icons/studio-icons.png");
    }
    inline int Index(std::string_view id)
    {
        for (int i=0;i<int(std::size(Entries));++i) if (id==Entries[i].id) return i;
        return -1;
    }
    inline std::string Title(std::string_view id)
    {
        const int i=Index(id); return i>=0 ? Entries[i].title : std::string(id);
    }
    struct Control
    {
        int command=-1;
        std::string_view icon;
        XMFLOAT4 bounds{}; // x, y, width, height in logical canvas pixels
        bool enabled=true, selected=false;
        std::string title;
    };
    inline bool Contains(const XMFLOAT4& b, const XMFLOAT4& p)
    { return p.x>=b.x && p.y>=b.y && p.x<b.x+b.z && p.y<b.y+b.w; }
    inline void Draw(std::string_view id, float x,float y,float side, wi::Color color,
                     wi::graphics::CommandList cmd)
    {
        const int i=Index(id);
        if(i<0 || !Atlas.IsValid()) return;
        wi::image::Params p(x,y,side,side,color);
        p.enableDrawRect({float(i%16*52+2),float(i/16*52+2),48,48});
        p.sampleFlag=wi::image::SAMPLEMODE_CLAMP;
        p.blendFlag=wi::enums::BLENDMODE_ALPHA;
        wi::image::Draw(&Atlas.GetTexture(),p,cmd);
    }
    inline void Button(const Control& c, bool hover,bool held,bool focus,
                       wi::graphics::CommandList cmd)
    {
        const auto& b=c.bounds;
        ui::ButtonVisual v{c.enabled,c.enabled&&hover,c.enabled&&held,false,c.selected};
        const auto edge=c.enabled&&(hover||held||c.selected)?theme::Accent:theme::Border;
        const auto face=!c.enabled?theme::Disabled:held?theme::Active:hover?theme::Focus:c.selected?theme::Selected:theme::Control;
        theme::ButtonRect(b.x,b.y,b.z,b.w,edge,cmd,6);
        theme::ButtonRect(b.x+1,b.y+1+v.Depression(),b.z-2,b.w-2-v.Depression(),face,cmd,5);
        if(focus) {
            theme::ButtonRect(b.x+3,b.y+3,b.z-6,b.w-6,theme::Text,cmd,3);
            theme::ButtonRect(b.x+4,b.y+4,b.z-8,b.w-8,held?theme::Active:theme::Control,cmd,2);
        }
        const float side=std::min(20.f,std::min(b.z,b.w)-10.f);
        Draw(c.icon,b.x+(b.z-side)*.5f,b.y+(b.w-side)*.5f+v.Depression(),side,
            c.enabled?theme::Text:theme::DisabledText,cmd);
    }
    class Tooltip
    {
        std::string title_;
        XMFLOAT4 anchor_{};
        float seconds_=0;
    public:
        void Clear() { title_.clear();seconds_=0; }
        void Update(const std::string& title,const XMFLOAT4& anchor,float dt,bool allowed=true)
        {
            if(!allowed || dt<=0 || wi::input::Down(wi::input::MOUSE_BUTTON_LEFT) ||
               wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE)) {Clear();return;}
            if(title!=title_ || anchor.x!=anchor_.x || anchor.y!=anchor_.y) {
                title_=title;anchor_=anchor;seconds_=0;
            } else seconds_+=dt;
        }
        void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const
        {
            if(title_.empty() || seconds_<.45f) return;
            wi::font::Params fp(0,0,12);fp.bolden=.12f;
            const float w=std::min(float(canvas.GetLogicalWidth())-16.f,wi::font::TextWidth(title_,fp)+20.f);
            const float x=std::clamp(anchor_.x,8.f,std::max(8.f,float(canvas.GetLogicalWidth())-w-8.f));
            float y=anchor_.y+anchor_.w+8.f;
            if(y+30>canvas.GetLogicalHeight()) y=anchor_.y-36.f;
            theme::ButtonRect(x,y,w,28,theme::Border,cmd,4);
            theme::ButtonRect(x+1,y+1,w-2,26,theme::Idle,cmd,3);
            fp.posX=x+10;fp.posY=y+7;fp.color=theme::Text;
            wi::font::Draw(title_,fp,cmd);
        }
    };
    class Interaction
    {
        ui::ReleaseAction gesture_;
        int focus_=-1;
        bool consumed_=false;
        Tooltip tooltip_;
    public:
        void Clear() {gesture_.Cancel();focus_=-1;tooltip_.Clear();}
        void CancelPointer() {gesture_.Cancel();tooltip_.Clear();}
        bool Consumed() const {return consumed_;}
        bool Held(int id) const {return gesture_.Target()==id;}
        bool Focused(int id) const {return focus_==id;}
        void RenderTooltip(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const {tooltip_.Render(canvas,cmd);}
        int Update(const std::vector<Control>& controls,float dt,bool allowed=true)
        {
            consumed_=false;
            const auto p=wi::input::GetPointer();
            const Control* hover=nullptr;
            for(const auto& c:controls) if(Contains(c.bounds,p)) {hover=&c;break;}
            if(wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE)) {Clear();return -1;}
            // Native GUI priority may suspend this backdrop for a frame during
            // workspace activation. Keep keyboard focus, but never a held gesture
            // or tooltip, across that suspension. Any outside click relinquishes it.
            if(wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)&&!hover)focus_=-1;
            if(!allowed || dt<=0) {CancelPointer();return -1;}
            tooltip_.Update(hover?(hover->title.empty()?Title(hover->icon):hover->title):"",hover?hover->bounds:XMFLOAT4{},dt);
            if(wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)) {
                focus_=hover&&hover->enabled?hover->command:-1;
                if(hover) {consumed_=true;if(hover->enabled)gesture_.Begin(hover->command);}
            }
            if(gesture_.Target()>=0) {
                consumed_=true;
                if(wi::input::Release(wi::input::MOUSE_BUTTON_LEFT))
                    return gesture_.Release(hover&&hover->enabled?hover->command:-1);
                if(!wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))gesture_.Cancel();
            }
            // Keyboard navigation is scoped to an already-focused toolbar;
            // clicking another editor surface releases that ownership.
            if(focus_>=0 && wi::input::Press(wi::input::KEYBOARD_BUTTON_TAB)) {
                std::size_t start=0;
                for(std::size_t i=0;i<controls.size();++i)if(controls[i].command==focus_)start=i;
                for(std::size_t step=1;step<=controls.size();++step) {
                    const auto& c=controls[(start+step)%controls.size()];
                    if(c.enabled){focus_=c.command;tooltip_.Update(c.title.empty()?Title(c.icon):c.title,c.bounds,dt);break;}
                }
                consumed_=true;
            }
            if(focus_>=0 && wi::input::Release(wi::input::KEYBOARD_BUTTON_ENTER)) {
                for(const auto& c:controls)if(c.command==focus_&&c.enabled){consumed_=true;return focus_;}
            }
            return -1;
        }
    };
}
