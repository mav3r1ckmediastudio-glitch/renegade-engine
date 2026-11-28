void StudioRenderPath::MapHistory(std::function<void()> swap)
{
    if(mapReplaying_)return;
    mapHistory_.resize(mapCursor_);
    if(mapHistory_.size()==24)mapHistory_.erase(mapHistory_.begin());
    mapHistory_.push_back(std::move(swap));mapCursor_=mapHistory_.size();
}
void StudioRenderPath::ReplayMapEdit(bool redo)
{
    if(redo ? mapCursor_==mapHistory_.size() : mapCursor_==0)return;
    mapReplaying_=true;
    auto& swap=mapHistory_[redo?mapCursor_++:--mapCursor_];swap();
    mapReplaying_=false;
    if(importMaterials_->pin.historyRestored)importMaterials_->pin.historyRestored=false;
    else if(!mappedTakes_.empty() && mappedTakes_.count(rtClip_.GetSelected()) && mappedTakes_.at(rtClip_.GetSelected()).clip>=0)
        WriteMappedTake(false);
    SelectMapTarget(mapTargetIndex_);RefreshMapRows();LayoutImportWorkspace();
}
bool StudioRenderPath::ScrollRetargetTable()
{
    const auto pointer = wi::input::GetPointer();
    if (!wi::input::Down(wi::input::MOUSE_BUTTON_MIDDLE))
        mapDragging_ = false;

    if (importPage_ != ImportPage::Retarget || !mapTargetHeader_.IsVisible() ||
        !rtScene_.IsValid() || importCameraDrag_ != ImportCameraDrag::None)
    {
        mapDragging_ = false;
        return false;
    }
    // An expanded picker owns its own list and wheel input.
    if (reviewSources_.GetState() == wi::gui::ACTIVE || rtClip_.GetState() == wi::gui::ACTIVE || std::any_of(mapSourceRows_.begin(), mapSourceRows_.end(),
        [](const auto& row) { return row.IsVisible() && row.GetState() == wi::gui::ACTIVE; }))
        return false;

    const auto top = mapSourceHeader_.GetPos();
    const auto right = mapTargetHeader_.GetPos().x + mapTargetHeader_.GetSize().x;
    const auto bottom = mapSourceLabel_.GetPos().y;
    const bool overTable = pointer.x >= top.x && pointer.x < right &&
        pointer.y >= top.y && pointer.y < bottom;
    if (overTable && wi::input::Press(wi::input::MOUSE_BUTTON_MIDDLE))
    {
        mapDragging_ = true;
        mapDragY_ = pointer.y;
    }
    int rows = 0;
    if (mapDragging_)
    {
        const float pitch = std::max(1.f, mapTargetRows_[1].GetPos().y - mapTargetRows_[0].GetPos().y);
        rows = int((mapDragY_ - pointer.y) / pitch);
        mapDragY_ -= rows * pitch;
    }
    else if (overTable && pointer.z != 0)
        rows = pointer.z > 0 ? -std::max(1, int(pointer.z)) : std::max(1, int(-pointer.z));
    else
        return false;

    const int first = std::clamp(mapFirst_ + rows, 0, std::max(0, int(mapFiltered_.size()) - MapRows));
    if (first != mapFirst_)
    {
        mapFirst_ = first;
        RefreshMapRows();
    }
    return true;
}

void StudioRenderPath::SelectMapTarget(int t)
{
    if(t<0 || t>=int(rtTargetNames_.size()))return;
    mapTargetIndex_=t;rtTargetTurn_.SetEnabled(true);
    auto it=std::find(rtPairs_.begin(),rtPairs_.end(),t);
    rtSource_.SetSelectedWithoutCallback(it==rtPairs_.end()?-1:int(it-rtPairs_.begin()));
    auto& scene=modelImportPreview_->InspectionScene();
    auto e=ImportAnimationBones(scene)[t];
    auto bone=std::find_if(importBones_.begin(),importBones_.end(),[&](const auto& b){return b.entity==e;});
    if(bone!=importBones_.end())importBone_.SetSelected(int(bone-importBones_.begin()));
    RefreshMapRows();
}
void StudioRenderPath::RefreshMapRows()
{
    if(!rtScene_.IsValid())return;
    auto lower=[](std::string v){for(char& c:v)c=char(std::tolower(static_cast<unsigned char>(c)));return v;};
    const auto search=lower(mapSearch_.GetValue());
    mapFiltered_.clear();
    for(int t=0;t<int(targetLabels_.size());++t){
        auto it=std::find(rtPairs_.begin(),rtPairs_.end(),t);
        if (reviewingPairs_ && (it != rtPairs_.end() || mapKept_.count(t))) continue;
        std::string words=targetLabels_[t]+" "+rtTargetNames_[t];
        if(it!=rtPairs_.end())words+=" "+sourceLabels_[it-rtPairs_.begin()]+" "+rtSourceNames_[it-rtPairs_.begin()];
        if(search.empty() || lower(words).find(search)!=std::string::npos)mapFiltered_.push_back(t);
    }
    mapFirst_=std::clamp(mapFirst_,0,std::max(0,int(mapFiltered_.size())-MapRows));
    const bool rt=importPage_==ImportPage::Retarget;
    for(int row=0;row<MapRows;++row){
        auto& c=mapSourceRows_[row];auto& b=mapTargetRows_[row];
        bool show=rt && mapFirst_+row<int(mapFiltered_.size());c.SetVisible(show);b.SetVisible(show);
        if(!show)continue;
        int t=mapFiltered_[mapFirst_+row];auto it=std::find(rtPairs_.begin(),rtPairs_.end(),t);
        c.ClearItems();c.AddItem("Choose source...",uint64_t(-1));c.AddItem("Keep reference pose",0);
        int selected=mapKept_.count(t)?1:0;
        for(int j=0;j<int(sourceLabels_.size());++j){
            if (reviewingPairs_ && rtPairs_[j] != -2) continue;
            c.AddItem(sourceLabels_[j],j+1);
            if(it!=rtPairs_.end() && j==it-rtPairs_.begin())selected=int(c.GetItemCount())-1;
        }
        c.SetSelectedWithoutCallback(selected);b.SetText(reviewingPairs_ && mapTargetIndex_ == t ? "> " + targetLabels_[t] : targetLabels_[t]);b.SetTooltip(rtTargetNames_[t]);
        b.SetColor(reviewingPairs_ ? wi::Color(151,47,48) :
            (mapTargetIndex_==t ? theme::Accent : theme::Control), wi::gui::IDLE);
    }
    const int count = int(mapFiltered_.size());
    const std::string range = count ? std::to_string(mapFirst_ + 1) + "-" +
        std::to_string(std::min(mapFirst_ + MapRows, count)) + " of " + std::to_string(count) : "0";
    mapTargetHeader_.SetText((reviewingPairs_ ? "UNASSIGNED   " : "TARGET   ") + range);
    mapPrev_.SetEnabled(mapFirst_>0);mapNext_.SetEnabled(mapFirst_+MapRows<int(mapFiltered_.size()));
    int src=rtSource_.GetSelected();mapSourceLabel_.SetEnabled(src>=0 && src<int(sourceLabels_.size()));
    mapSourceLabel_.SetValue(src>=0 && src<int(sourceLabels_.size())?sourceLabels_[src]:"");
    if(mapTargetIndex_>=0 && mapTargetIndex_<int(targetLabels_.size()))mapTargetLabel_.SetValue(targetLabels_[mapTargetIndex_]);
    auto extra=std::find(rtPairs_.begin(),rtPairs_.end(),-2);
    if (reviewingPairs_) {
        reviewSources_.ClearItems();
        int selected = 0;
        for (int i = 0; i < int(rtPairs_.size()); ++i) {
            if (rtPairs_[i] != -2) continue;
            if (reviewSource_ == i) selected = int(reviewSources_.GetItemCount());
            reviewSources_.AddItem(sourceLabels_[i], i);
        }
        reviewSource_ = extra == rtPairs_.end() ? -1 :
            int(reviewSources_.GetItemUserData(selected));
        reviewSources_.SetSelectedWithoutCallback(reviewSource_ < 0 ? -1 : selected);
        reviewSources_.SetEnabled(reviewSource_ >= 0);
        rtSource_.SetSelectedWithoutCallback(reviewSource_);
        reviewAssign_.SetEnabled(reviewSource_ >= 0 &&
            std::find(mapFiltered_.begin(), mapFiltered_.end(), mapTargetIndex_) != mapFiltered_.end());
    }
    mapReviewExtras_.SetVisible(rt && extra!=rtPairs_.end());
    if(extra!=rtPairs_.end())mapReviewExtras_.SetText(reviewingPairs_ ? "Ignore selected source" : "Ignore source: "+rtSourceNames_[extra-rtPairs_.begin()]);
    mapUndo_.SetEnabled(mapCursor_>0);mapRedo_.SetEnabled(mapCursor_<mapHistory_.size());
    UpdateRetargetStatus();
}
void StudioRenderPath::AssignMapSource(int target,int source)
{
    if(target<0 || target>=int(rtTargetNames_.size()) || source>=int(rtPairs_.size()))return;
    auto old=rtPairs_;auto kept=mapKept_;
    auto bind=[&](int t,int s){
        auto it=std::find(rtPairs_.begin(),rtPairs_.end(),t);
        if(it!=rtPairs_.end())*it=-1;
        if(s==-2)mapKept_.erase(t);else if(s<0)mapKept_.insert(t);else{rtPairs_[s]=t;mapKept_.erase(t);}
    };
    bind(target,source);
    // Selecting a digit matches its corresponding segments in hierarchy order.
    // End markers and branches never consume an extra rotation channel.
    if(source>=0){
        auto digit=[](const std::string& n){
            auto key=bridge::RetargetJointKey(n);
            for(std::string stem:{"thumb","index","middle","ring","pinky"})
                if(key.rfind(stem,0)==0 && key.size()==stem.size()+3 && key[stem.size()]=='0')return stem;
            return std::string{};
        };
        auto chain=[&](const auto& scene,const auto& names,int start){
            std::vector<int> list{start};auto stem=digit(names[start]);
            const auto bones=ImportAnimationBones(scene);
            while(!stem.empty()){
                int child=-1;
                for(int j=0;j<int(bones.size());++j){const auto* h=scene.hierarchy.GetComponent(bones[j]);
                    if(h && h->parentID==bones[list.back()] && digit(names[j])==stem){if(child>=0){child=-1;break;}child=j;}}
                if(child<0 || std::find(list.begin(),list.end(),child)!=list.end())break;
                list.push_back(child);
            }
            return list;
        };
        auto a=chain(*rtScene_,rtSourceNames_,source),b=chain(modelImportPreview_->InspectionScene(),rtTargetNames_,target);
        if(!digit(rtSourceNames_[source]).empty() && !digit(rtTargetNames_[target]).empty() && bridge::RetargetJointKey(rtTargetNames_[target]).find("01")!=std::string::npos)
            for(size_t j=1;j<std::min(a.size(),b.size());++j)bind(b[j],a[j]);
    }
    if(old!=rtPairs_ || kept!=mapKept_){
        MapHistory([this,old,kept]()mutable{rtPairs_.swap(old);mapKept_.swap(kept);});mapDirty_=true;
    }
    SelectMapTarget(target);
}
