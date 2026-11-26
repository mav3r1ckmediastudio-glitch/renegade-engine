    wi::gui::Label gripLabel;
    wi::gui::Button addGripKey;
    std::optional<float> gripDragKey;
    bool gripDragStarted=false;
    XMFLOAT2 gripDragStart{};
    float gripLeft() const {return cleanLeft()+230.f;}
    float gripWidth() const {return std::max(32.f,GetLogicalWidth()-cleanRight()-gripLeft()-136.f);}
    float gripTop() const {return cleanTimelineY()+152.f;}
    static constexpr float gripHeight=29.f;
    void createGripTimeline(){
        cleanLabel(gripLabel,"Support hand on grip");
        cleanButton(addGripKey,"Add grip key","Add a weight key at the playhead. Drag a key sideways to change its time or vertically to change contact. Right-click a key to delete it.",[this]{
            if(!hasSupportGrip())return;
            pauseForFit();
            auto curve=gripCurve();
            float phase=posePhase();
            if(curve.count(phase))return;
            rememberFit();
            curve[phase]=gripWeight(curve,phase);
            gripFit.curves[currentClip()]=curve;
        });
    }
    void layoutGripTimeline(){
        cleanSetVisible(gripLabel,hasSupportGrip());
        cleanSetVisible(addGripKey,hasSupportGrip());
        if(!hasSupportGrip())return;
        float w=activeGripWeight();
        bool unreachable=w>.999f&&armFit.chain[0].error>.005f;
        char label[112];
        snprintf(label,sizeof(label),"Support hand on grip\n%s",!armFit.enabled?"IK off":unreachable?"Out of reach":w==0?"Released":"");
        if(armFit.enabled&&w>0&&!unreachable)snprintf(label,sizeof(label),"Support hand on grip\n%.0f%% contact",w*100);
        gripLabel.SetText(label);
        cleanPlace(gripLabel,cleanLeft()+18,gripTop()-4,200,40);
        cleanPlace(addGripKey,gripLeft()+gripWidth()+12,gripTop()-1,108,32);
    }
    void drawGripTimeline(wi::graphics::CommandList cmd) const {
        if(!hasSupportGrip() || clips.GetState()==wi::gui::ACTIVE)return;
        const auto curve=gripCurve();
        float x=gripLeft(),y=gripTop(),width=gripWidth();
        wi::image::SetCanvas(*this);
        auto rect=[&](float left,float top,float w,float h,XMFLOAT4 color){
            wi::image::Params fx;fx.pos={left,top,0};fx.siz={w,h};fx.color=color;wi::image::Draw(nullptr,fx,cmd);
        };
        rect(x-4,y-4,width+8,gripHeight+8,{.12f,.15f,.18f,1});
        for(int i=0;i<96;++i){
            float level=gripWeight(curve,(i+.5f)/96.f)*gripHeight;
            rect(x+i*width/96.f,y+gripHeight-level,width/96.f+.5f,level+1,{.24f,.44f,.44f,1});
        }
        rect(x+posePhase()*width-1,y-4,2,gripHeight+8,{1,.52f,.17f,1});
        for(auto key:curve)rect(x+key.first*width-4,y+(1-key.second)*gripHeight-4,8,8,{.95f,.95f,.93f,1});
    }
    void handleGripTimeline(XMFLOAT4 mouse){
        if(!hasSupportGrip() || clips.GetState()>=wi::gui::ACTIVE){gripDragKey.reset();return;}
        float phase=std::clamp((mouse.x-gripLeft())/gripWidth(),0.f,1.f);
        float weight=std::clamp(1-(mouse.y-gripTop())/gripHeight,0.f,1.f);
        if(gripDragKey){
            if(!wi::input::Down(wi::input::MOUSE_BUTTON_LEFT)){gripDragKey.reset();return;}
            if(!gripDragStarted&&std::abs(mouse.x-gripDragStart.x)+std::abs(mouse.y-gripDragStart.y)<3.f)return;
            auto curve=gripCurve();auto key=curve.find(*gripDragKey);if(key==curve.end()){gripDragKey.reset();return;}
            if(!gripDragStarted){rememberFit();pauseForFit();gripDragStarted=true;}
            auto next=std::next(key);
            float lo=key==curve.begin()?0.f:std::prev(key)->first+.00001f;
            float hi=next==curve.end()?1.f:next->first-.00001f;
            if(hi<lo)return;
            phase=std::clamp(phase,lo,hi);
            curve.erase(key);curve[phase]=weight;gripDragKey=phase;
            gripFit.curves[currentClip()]=std::move(curve);
            return;
        }
        if(mouse.x<gripLeft()-6||mouse.x>gripLeft()+gripWidth()+6||mouse.y<gripTop()-6||mouse.y>gripTop()+gripHeight+6)return;
        bool left=wi::input::Press(wi::input::MOUSE_BUTTON_LEFT),right=wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT);
        if(!left&&!right)return;
        auto curve=gripCurve();
        for(auto k:curve){
            if(std::abs(gripLeft()+k.first*gripWidth()-mouse.x)>7||std::abs(gripTop()+(1-k.second)*gripHeight-mouse.y)>7)continue;
            if(left){gripDragKey=k.first;gripDragStarted=false;gripDragStart={mouse.x,mouse.y};}
            else{rememberFit();curve.erase(k.first);if(curve.empty())gripFit.curves.erase(currentClip());else gripFit.curves[currentClip()]=curve;}
            return;
        }
    }
