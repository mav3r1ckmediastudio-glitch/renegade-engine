struct ClipPinEditor {
    StudioRenderPath& app;
    wi::gui::ComboBox anchor,locks;
    wi::gui::TextInputField start,end,parents,blend;
    wi::gui::Button preview,accept,detect,ground;
    std::shared_ptr<bridge::ModelImportCandidate> staged;
    bridge::ClipJointPin proposed;
    std::map<int,std::vector<bridge::ClipJointPin>> applied;
    int clip=-1;
    uint64_t generation=0;
    bool historyRestored=false;

    explicit ClipPinEditor(StudioRenderPath& owner):app(owner){}
    void create(){
        auto& panel=app.modelImportPanel_;
        anchor.Create("Anchor");panel.AddWidget(&anchor);
        locks.Create("Lock");for(auto s:{"Position","Rotation","Position and rotation"})locks.AddItem(s);
        panel.AddWidget(&locks);
        auto field=[&](auto& f,const char* text,int value){
            f.Create(text);f.SetDescription(std::string(text)+"  ");f.SetValue(value);
            f.SetCancelInputEnabled(false);panel.AddWidget(&f);
        };
        field(start,"Start frame",0);field(end,"End frame",0);field(parents,"Parent bones",2);field(blend,"Blend frames",0);
        preview.Create("Preview pin");accept.Create("Apply pin");
        auto bind=[&](auto& button,bool keep){
            button.OnClick([this,keep](const wi::gui::EventArgs&){
                const auto gen=app.modelImportGeneration_;
                wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this,gen,keep](uint64_t){
                    if(app.modelImportPanel_.IsVisible() && gen==app.modelImportGeneration_ && active())click(keep);
                });
            });panel.AddWidget(&button);
        };bind(preview,false);bind(accept,true);
        detect.Create("Find next source contact");detect.SetTooltip("Suggest a frame range where the mapped source joint stays still. Preview before applying.");
        detect.OnClick([this](const wi::gui::EventArgs&){auto gen=app.modelImportGeneration_;wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this,gen](uint64_t){if(gen==app.modelImportGeneration_ && active())detectRange();});});
        panel.AddWidget(&detect);
        ground.Create("Preview floor lift");ground.SetTooltip("Lift this clip until its lowest sampled mesh contact meets the floor. Keeps the pose and horizontal motion.");
        ground.OnClick([this](const wi::gui::EventArgs&){
            auto gen=app.modelImportGeneration_;
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this,gen](uint64_t){
                if(app.modelImportPanel_.IsVisible() && gen==app.modelImportGeneration_ && active() && !staged)previewGround();
            });
        });panel.AddWidget(&ground);
    }
    bool canDetect()const {
        if(!app.rtScene_.IsValid())return false;
        auto take=app.mappedTakes_.find(app.rtClip_.GetSelected());
        return take!=app.mappedTakes_.end() && take->second.clip==clip;
    }
    void detectRange(){
        if(!canDetect() || staged)return;
        const int selected=app.importBone_.GetSelected();
        if(selected<0)return;
        const auto name=app.importBone_.GetItemText(selected);
        auto dst=std::find(app.rtTargetNames_.begin(),app.rtTargetNames_.end(),name);
        auto src=std::find(app.rtPairs_.begin(),app.rtPairs_.end(),int(dst-app.rtTargetNames_.begin()));
        if(dst==app.rtTargetNames_.end() || src==app.rtPairs_.end()){app.ShowStudioMessageBox("This joint has no mapped source track.","Find contact");return;}
        wi::scene::Scene inspection;wi::Archive copy;
        app.rtScene_->Serialize(copy);copy.SetReadModeAndResetPos(true);inspection.Serialize(copy);
        bridge::ModelAnimationPreviewService player;player.Prepare(inspection);
        const int selectedClip=app.rtClip_.GetSelected();player.Select(selectedClip);
        const auto source=player.Clips().at(selectedClip);
        const auto joint=inspection.Entity_FindByName(app.rtSourceNames_[src-app.rtPairs_.begin()]);
        auto h=inspection.hierarchy.GetComponent(joint);
        if(!h || !inspection.transforms.Contains(h->parentID)){app.ShowStudioMessageBox("Select a joint with a parent.","Find contact");return;}
        const auto destination=app.modelImportPreview_->Clips().at(clip);
        const int first=int(std::lround(destination.start*30)),last=int(std::lround(destination.end*30));
        int from=first;
        std::vector<std::pair<int,int>> ranges;
        XMVECTOR origin=XMVectorZero(),previous=XMVectorZero();
        float length=0;
        for(int f=first;f<=last;++f){
            const float u=float(f-first)/std::max(1,last-first);
            player.Scrub(source.start+(source.end-source.start)*u);inspection.Update(0);
            auto pos=inspection.transforms.GetComponent(joint)->GetPositionV();
            if(f==first){
                length=XMVectorGetX(XMVector3Length(pos-inspection.transforms.GetComponent(h->parentID)->GetPositionV()));
                if(length<1e-6f)break;
                origin=previous=pos;
            }
            if(XMVectorGetX(XMVector3Length(pos-previous))>length*.012f || XMVectorGetX(XMVector3Length(pos-origin))>length*.04f){
                if(f-from>=5)ranges.emplace_back(from,f-1);
                origin=pos;from=f;
            }
            previous=pos;
            if(f==last && last-from>=4)ranges.emplace_back(from,last);
        }
        if(ranges.empty()){app.ShowStudioMessageBox("No sustained still interval found. Set the frame range manually.","Find contact");return;}
        int after=int(std::lround(app.modelImportPreview_->ClipTime()*30));
        auto next=std::find_if(ranges.begin(),ranges.end(),[&](const auto& v){return v.first>after;});
        if(next==ranges.end())next=ranges.begin();
        start.SetValue(next->first);end.SetValue(next->second);
        app.modelImportPreview_->Scrub(std::clamp(next->first/30.f,destination.start,destination.end));
        app.RefreshPoseFields();
    }
    bool active()const{return app.mapEditingClip_ && app.importPage_==ImportPage::Pose && app.clipEditScope_.GetSelected()==3;}
    bool supported(int index)const{
        if(index<0 || !app.modelImportCandidate_)return false;
        for(const auto& e:app.modelImportCandidate_->ExternalAnimations())
            if(e.firstAnimationIndex==size_t(index) && e.animationCount==1 &&
               fs::path(app.modelImportCandidate_->Dependencies().at(e.dependencyIndex).retainedRelativePath).extension()==".wiscene")return true;
        return false;
    }
    void open(int index){
        if(generation!=app.modelImportGeneration_){applied.clear();generation=app.modelImportGeneration_;}
        clip=index;staged.reset();anchor.ClearItems();anchor.AddItem("Fixed in clip space");
        for(const auto& bone:app.importBones_)anchor.AddItem(app.modelImportPreview_->InspectionScene().names.GetComponent(bone.entity)->name);
        anchor.SetSelectedWithoutCallback(0);locks.SetSelectedWithoutCallback(0);
        auto c=app.modelImportPreview_->Clips().at(index);start.SetValue(int(std::lround(c.start*30)));end.SetValue(int(std::lround(c.end*30)));
        parents.SetValue(2);blend.SetValue(0);
    }
    void layout(float x,float w,bool show){
        auto place=[&](auto& f,float dx,float y,float width){
            f.SetVisible(show);f.SetPos({x+dx,y});f.SetSize({width,26});
        };
        const float half=(w-10)/2;
        place(anchor,95,495,w-95);place(locks,95,527,w-95);
        place(start,95,559,half-95);place(end,half+103,559,half-95);
        place(parents,95,591,half-95);place(blend,half+103,591,half-95);
        place(preview,0,632,half);place(accept,half+10,632,half);
        place(detect,0,666,half);place(ground,half+10,666,half);ground.SetEnabled(!staged);detect.SetEnabled(!staged && canDetect());
        preview.SetText(staged?"Discard preview":"Preview pin");
        accept.SetText(staged?(proposed.floorOffsets.empty()?"Keep previewed pin":"Keep floor lift"):"Apply pin");
    }
    void previewGround(){
        if(clip!=app.modelImportClip_.GetSelected()-1 || !supported(clip))return;
        const float time=app.modelImportPreview_->ClipTime();
        bridge::ClipJointPin correction;std::string error;
        bridge::ModelImportCandidateService importer;
        auto next=std::make_shared<bridge::ModelImportCandidate>(app.modelImportCandidate_->CaptureBakeState());
        if(!importer.MeasureClipFloor(app.modelImportPreview_->InspectionScene(),clip,correction,error) ||
           !importer.PinBakedClip(*next,clip,correction,error)){app.ShowStudioMessageBox(error,"Floor lift");return;}
        proposed=std::move(correction);staged=next;
        std::swap(*app.modelImportCandidate_,*staged);
        app.RefreshBakedPreview(clip,time);
        std::swap(*app.modelImportCandidate_,*staged);
    }
    void click(bool keep){
        const float time=app.modelImportPreview_->ClipTime();
        if(clip!=app.modelImportClip_.GetSelected()-1 || !supported(clip))return;
        if(staged && !keep){staged.reset();app.RefreshBakedPreview(clip,time);return;}
        if(!staged){
            const int index=app.importBone_.GetSelected();
            if(index<0 || index>=int(app.importBones_.size()))return;
            auto& scene=app.modelImportPreview_->InspectionScene();
            auto entity=app.importBones_[index].entity;
            bridge::ClipJointPin pin;pin.joint=scene.names.GetComponent(entity)->name;
            int anchorIndex=anchor.GetSelected();if(anchorIndex>0)pin.relativeTo=anchor.GetItemText(anchorIndex);
            auto c=app.modelImportPreview_->Clips().at(clip);
            int s,e,b,n;
            auto number=[](auto& field,int& dest){const auto v=field.GetValue();char* end=nullptr;long val=strtol(v.c_str(),&end,10);
                if(v.empty()||*end||val<0||val>10000000)return false;dest=int(val);return true;};
            if(!number(start,s)||!number(end,e)||!number(blend,b)||!number(parents,n)||
                s<int(std::lround(c.start*30))||e>int(std::lround(c.end*30))||e<s||n<1||n>32){
                app.ShowStudioMessageBox("Enter a frame range inside the clip and 1 to 32 parent bones.","Pin joint");return;
            }
            pin.captureTime=std::clamp(time,c.start,c.end);
            pin.start=s==std::lround(c.start*30)?c.start:s/30.f;
            pin.end=e==std::lround(c.end*30)?c.end:e/30.f;
            pin.blend=b/30.f;pin.parents=n;pin.position=locks.GetSelected()!=1;pin.rotation=locks.GetSelected()!=0;
            if(pin.position){
                auto p=app.ImportPosePoint(3);
                XMFLOAT3 offset;XMStoreFloat3(&offset,XMVector3TransformCoord(XMLoadFloat3(&p),XMMatrixInverse(nullptr,XMLoadFloat4x4(&scene.transforms.GetComponent(entity)->world))));
                if(!std::isfinite(offset.x+offset.y+offset.z)){app.ShowStudioMessageBox("Enter a valid target position.","Pin joint");return;}
                pin.offsetX=offset.x;pin.offsetY=offset.y;pin.offsetZ=offset.z;
            }
            auto next=std::make_shared<bridge::ModelImportCandidate>(app.modelImportCandidate_->CaptureBakeState());
            std::string error;
            if(!bridge::ModelImportCandidateService().PinBakedClip(*next,clip,pin,error)){app.ShowStudioMessageBox(error,"Pin joint");return;}
            proposed=pin;staged=next;
        }
        if(!keep){
            std::swap(*app.modelImportCandidate_,*staged);
            app.RefreshBakedPreview(clip,time);
            std::swap(*app.modelImportCandidate_,*staged);
            return;
        }
        auto previous=staged;staged.reset();
        std::swap(*app.modelImportCandidate_,*previous);
        auto old=applied[clip];applied[clip].push_back(proposed);
        app.MapHistory([this,previous,old,index=clip,time]() mutable {
            staged.reset();std::swap(*app.modelImportCandidate_,*previous);applied[index].swap(old);
            historyRestored=true;app.RefreshBakedPreview(index,time);
        });
        app.RefreshBakedPreview(clip,time);
    }
};
