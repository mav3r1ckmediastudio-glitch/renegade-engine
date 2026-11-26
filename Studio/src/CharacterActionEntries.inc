// The FBX takes remain intact. Rows are independent views of their source take.
namespace {
constexpr const char* entryTypes[]={"Unassigned","Idle","Walk","Run","Attack","Melee","Fire","Cast","Reload","Hit","Death","Gesture","Situational idle"};
}
void StudioRenderPath::SyncActionSources()
{
    const auto& scene=*modelImportCandidate_->PeekScene();
    while(actionSourceCount_<scene.animations.GetCount()) {
        const size_t index=actionSourceCount_++;
        const auto& clip=scene.animations[index];
        const auto* name=scene.names.GetComponent(scene.animations.GetEntity(index));
        bridge::CreatorAnimationImportRecipe entry;
        entry.sourceAnimationIndex=uint32_t(index);
        entry.name=name && !name->name.empty() ? name->name : "Animation "+std::to_string(index+1);
        entry.start=clip.start; entry.end=clip.end; entry.speed=clip.speed;
        entry.action=index<modelImportActions_.size() ? modelImportActions_[index] : "Unassigned";
        actionEntries_.rows.push_back(entry);
        // New sources must also survive undoing an earlier row edit.
        for(auto* history:{&entryUndoStack_,&entryRedoStack_})
            for(auto& snapshot:*history) snapshot.rows.push_back(entry);
        if(int(index)+1==modelImportClip_.GetSelected()) actionEntries_.selected=int(actionEntries_.rows.size())-1;
    }
}
void StudioRenderPath::RememberActionEdit()
{
    entryUndoStack_.push_back(actionEntries_);
    entryRedoStack_.clear();
}
void StudioRenderPath::UndoActionEdit(bool redo)
{
    auto& from=redo ? entryRedoStack_ : entryUndoStack_;
    auto& to=redo ? entryUndoStack_ : entryRedoStack_;
    if(from.empty()) return;
    to.push_back(std::move(actionEntries_));
    actionEntries_=std::move(from.back()); from.pop_back();
    SelectActionEntry(actionEntries_.selected);
}
void StudioRenderPath::PreviewActionSource(int index)
{
    if(!modelImportCandidate_ || !modelImportPreview_ || index<0 ||
       size_t(index)>=modelImportCandidate_->PeekScene()->animations.GetCount()) return;
    actionEntries_.selected=-1;
    const auto& original=modelImportCandidate_->PeekScene()->animations[index];
    auto& preview=modelImportPreview_->InspectionScene().animations[index];
    preview.start=original.start;preview.end=original.end;preview.speed=original.speed;
    modelImportClip_.SetSelected(index+1);
    modelImportPreview_->Scrub(original.start);
    modelImportPreview_->PlayPause();
}
void StudioRenderPath::SelectActionEntry(int row)
{
    if(!modelImportPreview_ || row>=int(actionEntries_.rows.size())) return;
    actionEntries_.selected=row;
    if(row<0) {modelImportClip_.SetSelected(0);return;}
    const auto& entry=actionEntries_.rows[row];
    auto& preview=modelImportPreview_->InspectionScene().animations[entry.sourceAnimationIndex];
    preview.start=entry.start;preview.end=entry.end;preview.speed=entry.speed;
    modelImportClip_.SetSelected(int(entry.sourceAnimationIndex)+1);
    modelImportPreview_->Scrub(entry.start);
    modelImportTime_.SetValue(entry.start);
    modelImportPreview_->PlayPause();
}
void StudioRenderPath::PositionActionRow(ActionRowWidgets& w,int row)
{
    const auto& actionColumnEdges = importWorkflow_ == ImportWorkflow::Animated ? modelColumnEdges : characterColumnEdges;
    const float width=actionTable_.GetSize().x-24;
    const float y=3+row*34.f;
    auto place=[&](auto& widget,int column,float left,float reserve) {
        widget.SetPos({5+width*actionColumnEdges[column]+left,y});
        widget.SetSize({std::max(12.f,width*(actionColumnEdges[column+1]-actionColumnEdges[column])-left-reserve),29});
    };
    w.preview.SetPos({3,y});w.preview.SetSize({7,29});
    place(w.name,0,8,3);place(w.type,1,0,3);place(w.source,2,0,3);
    place(w.start,3,0,3);place(w.end,4,0,3);place(w.speed,5,0,3);
}
void StudioRenderPath::CreateActionRow(int row)
{
    auto& w=*actionWidgets_.emplace_back(std::make_unique<ActionRowWidgets>());
    auto add=[&](auto& widget,const char* caption) {
        widget.Create(caption);
        widget.SetColor(HologramIdle,wi::gui::IDLE);widget.SetColor(HologramFocus,wi::gui::FOCUS);
        widget.SetColor(HologramActive,wi::gui::ACTIVE);
        actionTable_.AddWidget(&widget);
    };
    add(w.preview,"");add(w.name,"");add(w.type,"");add(w.source,"");
    add(w.start,"");add(w.end,"");add(w.speed,"");
    PositionActionRow(w,row);
    w.start.SetTooltip("Start frame (30 fps)");w.end.SetTooltip("End frame (30 fps)");
    w.speed.SetTooltip("Playback speed (0.1 to 4)");w.name.SetTooltip("Entry name");
    w.preview.SetTooltip("Preview this entry");
    w.source.SetTooltip("Source animation");w.type.SetTooltip("Gameplay action");
    w.preview.OnClick([this,row](const auto&){SelectActionEntry(row);});
    for(const auto* type:entryTypes) w.type.AddItem(type);
    w.type.OnSelect([this,row](const auto& a){EditActionRow(row,1,a.sValue,a.iValue);});
    w.source.OnSelect([this,row](const auto& a){EditActionRow(row,2,a.sValue,a.iValue);});
    int field=3;
    for(auto* input:{&w.start,&w.end,&w.speed}) {
        input->OnInputAccepted([this,row,field](const auto& a){EditActionRow(row,field,a.sValue);});
        ++field;
    }
    w.name.OnInputAccepted([this,row](const auto& a){EditActionRow(row,0,a.sValue);});
}
void StudioRenderPath::RefreshCharacterClips()
{
    SyncActionSources();
    const auto& scene=*modelImportCandidate_->PeekScene();
    while (sourceClipButtons_.size() < actionSourceCount_) {
        const int index = int(sourceClipButtons_.size());
        const auto* name = scene.names.GetComponent(scene.animations.GetEntity(index));
        auto& button = *sourceClipButtons_.emplace_back(std::make_unique<wi::gui::Button>());
        button.Create(name && !name->name.empty() ? name->name : "Animation " + std::to_string(index+1));
        button.font.params.h_align = wi::font::WIFALIGN_LEFT;
        button.font.params.spacingX = 0;
        button.OnClick([this,index](const auto&) { PreviewActionSource(index); });
        actionSources_.AddWidget(&button);
        button.SetPos({3.f, 2.f + index * 34.f});
    }
    for (size_t i=0; i<sourceClipButtons_.size(); ++i) {
        auto& button = *sourceClipButtons_[i];
        const bool selected = actionEntries_.selected < 0 && modelImportClip_.GetSelected() == int(i)+1;
        button.SetColor(selected ? theme::Accent : theme::Control, wi::gui::IDLE);
        button.SetColor(selected ? theme::Accent : HologramFocus, wi::gui::FOCUS);
        button.SetColor(theme::Accent, wi::gui::ACTIVE);
        button.SetSize({std::max(20.f, actionSources_.GetSize().x-20.f), 32.f});
    }
    const int selected = actionEntries_.selected;
    const bool hasEntry = selected >= 0 && selected < int(actionEntries_.rows.size());
    importMaterials_->objectPlayback.SetEnabled(hasEntry);
    importMaterials_->objectPlayback.SetSelectedWithoutCallback(hasEntry ? std::max(0,int(actionEntries_.rows[selected].modelStart)-1) : 0);
    removeEntry_.SetEnabled(hasEntry);
    const auto count=actionEntries_.rows.size();
    while(actionWidgets_.size()<count) CreateActionRow(int(actionWidgets_.size()));
    for(size_t i=0;i<actionWidgets_.size();++i) {
        auto& w=*actionWidgets_[i];
        for(wi::gui::Widget* widget:std::array<wi::gui::Widget*,7>{&w.preview,&w.name,&w.type,&w.source,&w.start,&w.end,&w.speed})
            widget->SetVisible(i<count);
        w.type.SetVisible(i<count && importWorkflow_ == ImportWorkflow::Character);
        if(i>=count) continue;
        const auto& entry=actionEntries_.rows[i];
        w.preview.SetColor(int(i)==actionEntries_.selected ? theme::Accent : theme::Control,wi::gui::IDLE);
        w.name.SetDescription("");w.source.SetText("");w.type.SetText("");
        if(w.name.GetState()!=wi::gui::ACTIVE) w.name.SetValue(entry.name);
        if(w.source.GetItemCount()!=actionSourceCount_) {
            w.source.ClearItems();
            float longestName = 0.f;
            for(size_t src=0;src<actionSourceCount_;++src) {
                const auto* name=scene.names.GetComponent(scene.animations.GetEntity(src));
                const auto label = name ? name->name : "Animation "+std::to_string(src+1);
                w.source.AddItem(label);
                longestName = std::max(longestName, wi::font::TextWidth(label, w.source.font.params));
            }
            w.source.SetFixedDropWidth(std::min(modelImportPanel_.GetSize().x-32.f, std::max(w.source.GetSize().x, longestName+36.f)));
        }
        w.source.SetTooltip(w.source.GetItemText(int(entry.sourceAnimationIndex)));
        w.source.SetSelectedWithoutCallback(int(entry.sourceAnimationIndex));
        for(int j=0;j<int(std::size(entryTypes));++j)
            if(entry.action==entryTypes[j]) w.type.SetSelectedWithoutCallback(j);
        if(w.start.GetState()!=wi::gui::ACTIVE) w.start.SetValue(std::to_string(std::lround(entry.start*30)));
        if(w.end.GetState()!=wi::gui::ACTIVE) w.end.SetValue(std::to_string(std::lround(entry.end*30)));
        std::ostringstream speed;speed<<entry.speed;if(w.speed.GetState()!=wi::gui::ACTIVE) w.speed.SetValue(speed.str());
    }
    entryUndo_.SetEnabled(!entryUndoStack_.empty());entryRedo_.SetEnabled(!entryRedoStack_.empty());
    addEntry_.SetEnabled(actionSourceCount_>0);
    referencePose_.SetColor(modelImportClip_.GetSelected()==0 ? theme::Accent : theme::Control,wi::gui::IDLE);
}
void StudioRenderPath::EditActionRow(int row,int field,const std::string& value,int choice)
{
    if(row<0 || row>=int(actionEntries_.rows.size())) return;
    auto updated=actionEntries_.rows[row];
    const auto& scene=*modelImportCandidate_->PeekScene();
    const auto& native=scene.animations[updated.sourceAnimationIndex];
    bool valid=true;
    if(field==0) {
        valid=value.find_first_not_of(" \\t")!=std::string::npos;
        if(valid) updated.name=value;
    } else if(field==1) {
        valid=choice>=0 && choice<int(std::size(entryTypes));
        if(valid) updated.action=entryTypes[choice];
    } else if(field==2) {
        valid=choice>=0 && size_t(choice)<actionSourceCount_;
        if(valid && uint32_t(choice)!=updated.sourceAnimationIndex) {
            updated.sourceAnimationIndex=uint32_t(choice);
            updated.start=scene.animations[choice].start;updated.end=scene.animations[choice].end;
        }
    } else {
        char* tail=nullptr;
        float n=std::strtof(value.c_str(),&tail);
        valid=tail!=value.c_str() && *tail==0 && std::isfinite(n);
        if(field==5) {valid=valid && n>=0.1f && n<=4.f;updated.speed=n;}
        else {
            valid=valid && n==std::floor(n) && n>=std::lround(native.start*30) && n<=std::lround(native.end*30);
            const float time=n==std::lround(native.start*30) ? native.start : n==std::lround(native.end*30) ? native.end : n/30.f;
            if(field==3) updated.start=time;else updated.end=time;
            valid=valid && updated.end>updated.start;
        }
    }
    if(!valid) {
        modelImportSummary_.SetText(field==5 ? "Speed must be between 0.1 and 4." :
            field==0 ? "Enter a name for this animation entry." : "Enter a frame within the source range. Start must precede End.");
        RefreshCharacterClips();return;
    }
    RememberActionEdit();
    actionEntries_.rows[row]=std::move(updated);
    SelectActionEntry(row);
    if(field==4) modelImportPreview_->Scrub(actionEntries_.rows[row].end);
}
