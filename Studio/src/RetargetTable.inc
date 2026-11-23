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
    if(!mappedTakes_.empty() && mappedTakes_.count(rtClip_.GetSelected()) && mappedTakes_.at(rtClip_.GetSelected()).clip>=0)
        WriteMappedTake(false);
    SelectMapTarget(mapTargetIndex_);RefreshMapRows();LayoutImportWorkspace();
}
void StudioRenderPath::SelectMapTarget(int t)
{
    if(t<0 || t>=int(rtTargetNames_.size()))return;
    mapTargetIndex_=t;rtTargetTurn_.SetEnabled(true);
    auto it=std::find(rtPairs_.begin(),rtPairs_.end(),t);
    rtSource_.SetSelectedWithoutCallback(it==rtPairs_.end()?-1:int(it-rtPairs_.begin()));
    auto& scene=modelImportPreview_->InspectionScene();
    auto e=scene.armatures[0].boneCollection[t];
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
        std::string words=targetLabels_[t]+" "+rtTargetNames_[t];
        if(it!=rtPairs_.end())words+=" "+sourceLabels_[it-rtPairs_.begin()]+" "+rtSourceNames_[it-rtPairs_.begin()];
        if(search.empty() || lower(words).find(search)!=std::string::npos)mapFiltered_.push_back(t);
    }
    mapFirst_=std::clamp(mapFirst_,0,std::max(0,(int(mapFiltered_.size())-1)/MapRows*MapRows));
    const bool rt=importPage_==ImportPage::Retarget;
    for(int row=0;row<MapRows;++row){
        auto& c=mapSourceRows_[row];auto& b=mapTargetRows_[row];
        bool show=rt && mapFirst_+row<int(mapFiltered_.size());c.SetVisible(show);b.SetVisible(show);
        if(!show)continue;
        int t=mapFiltered_[mapFirst_+row];auto it=std::find(rtPairs_.begin(),rtPairs_.end(),t);
        c.ClearItems();c.AddItem("Choose source...",uint64_t(-1));c.AddItem("Keep reference pose",0);
        int selected=mapKept_.count(t)?1:0;
        for(int j=0;j<int(sourceLabels_.size());++j){
            c.AddItem(sourceLabels_[j],j+1);
            if(it!=rtPairs_.end() && j==it-rtPairs_.begin())selected=int(c.GetItemCount())-1;
        }
        c.SetSelectedWithoutCallback(selected);b.SetText(targetLabels_[t]);b.SetTooltip(rtTargetNames_[t]);
        b.SetColor(mapTargetIndex_==t?theme::Accent:theme::Control,wi::gui::IDLE);
    }
    mapPrev_.SetEnabled(mapFirst_>0);mapNext_.SetEnabled(mapFirst_+MapRows<int(mapFiltered_.size()));
    int src=rtSource_.GetSelected();mapSourceLabel_.SetEnabled(src>=0 && src<int(sourceLabels_.size()));
    mapSourceLabel_.SetValue(src>=0 && src<int(sourceLabels_.size())?sourceLabels_[src]:"");
    if(mapTargetIndex_>=0 && mapTargetIndex_<int(targetLabels_.size()))mapTargetLabel_.SetValue(targetLabels_[mapTargetIndex_]);
    auto extra=std::find(rtPairs_.begin(),rtPairs_.end(),-2);
    mapReviewExtras_.SetVisible(rt && extra!=rtPairs_.end());
    if(extra!=rtPairs_.end())mapReviewExtras_.SetText("Ignore source: "+rtSourceNames_[extra-rtPairs_.begin()]);
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
            const auto& bones=scene.armatures[0].boneCollection;
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
