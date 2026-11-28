// Editor-only visual playback. Meshes and materials are shared by temporary casing objects.
struct CasingPart { Entity mesh; XMFLOAT4X4 local; };
struct FlyingCasing {
    std::vector<Entity> objects;
    XMFLOAT3 origin, direction;
    float age=0;
    bool manual=false;
};
wi::gui::Button casingPanelButton, casingAssetButton, casingSetTime, casingClearTime, casingTry;
wi::gui::Label casingAssetLabel, casingTick, casingScaleLabel;
wi::gui::TextInputField casingScaleInput;
static constexpr float includedCasingScale=1.f;
float casingScale=includedCasingScale;
bool casingPanel=false, casingWasPlaying=false, casingAssetFailed=false;
std::optional<float> lastCasingTime;
fs::path casingAsset;
std::map<std::string,float> casingTimes;
wi::unordered_set<Entity> casingResources;
std::vector<CasingPart> casingParts;
std::vector<FlyingCasing> flyingCasings;


void clearFlyingCasings(){
    for(auto& c:flyingCasings)for(auto e:c.objects)arms.Entity_Remove(e,false);
    flyingCasings.clear();
    lastCasingTime.reset();
    casingWasPlaying=false;
}
void clearCasingAsset(){
    clearFlyingCasings();
    for(auto e:casingResources)arms.Entity_Remove(e,false);
    casingResources.clear();
    casingParts.clear();
    casingAssetFailed=false;
}
void loadCasingMesh(const fs::path& path){
    if(!fs::is_regular_file(path))throw std::runtime_error("Casing model is missing. Choose a casing model to relink it.");
    Scene source;
    ImportModel_GLTF(path.u8string(),source);
    source.Update(0);
    std::vector<CasingPart> parts;
    for(size_t i=0;i<source.objects.GetCount();++i){
        auto mesh=source.objects[i].meshID;
        auto t=source.transforms.GetComponent(source.objects.GetEntity(i));
        auto m=source.meshes.GetComponent(mesh);
        if(!m||m->vertex_positions.empty()||!t)continue;
        if(m->armatureID)throw std::runtime_error("Choose a static casing model.");
        parts.push_back({mesh,t->world});
    }
    if(parts.empty())throw std::runtime_error("The casing model contains no visible mesh.");
    Scene resources;
    resources.meshes.Merge(source.meshes);
    resources.materials.Merge(source.materials);
    clearCasingAsset();
    resources.FindAllEntities(casingResources);
    arms.Merge(resources);
    casingParts=std::move(parts);
}
Entity mountedSocketParent(const SpawnPoint& socket){
    if(assembled)return renegade::bridge::ResolveLaunchSocketParent(arms,socket.parentPath);
    if(!instance)return false;
    // Find the authored node in the animated weapon, including repeated names in different branches.
    auto parent=renegade::bridge::ResolveLaunchSocketParent(prop,socket.parentPath);
    if(!parent)return false;
    std::vector<std::string> chain;
    while(parent){
        auto name=prop.names.GetComponent(parent);
        chain.push_back(name?name->name:"");
        auto h=prop.hierarchy.GetComponent(parent);parent=h?h->parentID:0;
    }
    Entity mounted=instance;
    for(auto it=chain.rbegin();it!=chain.rend();++it){
        Entity match=0;
        for(size_t i=0;i<arms.hierarchy.GetCount();++i){
            if(arms.hierarchy[i].parentID!=mounted)continue;
            auto e=arms.hierarchy.GetEntity(i);auto n=arms.names.GetComponent(e);
            if((n?n->name:"")!=*it)continue;
            if(match)return false;
            match=e;
        }
        if(!match)return false;
        mounted=match;
    }
    return mounted;
}
bool mountedSocketPose(const SpawnPoint& socket,XMFLOAT3& pos,XMFLOAT3& dir){
    auto t=arms.transforms.GetComponent(mountedSocketParent(socket));
    if(!t)return false;
    const auto& r=socket.rotationDegrees;
    auto world=XMMatrixRotationRollPitchYaw(XMConvertToRadians(r.x),XMConvertToRadians(r.y),XMConvertToRadians(r.z))*
        XMMatrixTranslationFromVector(XMLoadFloat3(&socket.position))*XMLoadFloat4x4(&t->world);
    auto f=world.r[2];if(!(XMVectorGetX(XMVector3LengthSq(f))>1.e-12f))return false;
    pos=ArmFit::f3(world.r[3]);dir=ArmFit::f3(XMVector3Normalize(f));return true;
}
void poseFlyingCasing(const FlyingCasing& c){
    const auto age=c.age;
    auto f=XMLoadFloat3(&c.direction);
    auto up=std::abs(c.direction.y)>.95f?XMVectorSet(1,0,0,0):XMVectorSet(0,1,0,0);
    auto side=XMVector3Normalize(XMVector3Cross(up,f));
    auto position=XMLoadFloat3(&c.origin)+f*(1.8f*age)+XMVectorSet(0,-2.4f*age*age,0,0);
    XMMATRIX basis(side,XMVector3Cross(f,side),f,XMVectorSetW(position,1));
    auto pose=XMMatrixScaling(casingScale,casingScale,casingScale)*XMMatrixRotationRollPitchYaw(age*9,age*5,0)*basis;
    for(size_t i=0;i<c.objects.size();++i){
        auto t=arms.transforms.GetComponent(c.objects[i]);
        t->ClearTransform();t->MatrixTransform(XMLoadFloat4x4(&casingParts[i].local)*pose);t->UpdateTransform();
    }
}
bool emitPreviewCasing(bool manual){
    FlyingCasing c;c.manual=manual;
    if(!casingSocket||!mountedSocketPose(*casingSocket,c.origin,c.direction)){
        status.SetText("Place the casing marker on the mounted weapon first.");return false;
    }
    if(casingParts.empty()){
        if(casingAssetFailed&&!manual)return false;
        try{if(casingAsset.empty())throw std::runtime_error("Choose a casing model to preview ejection.");loadCasingMesh(casingAsset);}
        catch(const std::exception& e){status.SetText(e.what());casingAssetFailed=true;return false;}
    }
    for(const auto& part:casingParts){
        auto e=arms.Entity_CreateObject("Casing preview");arms.objects.GetComponent(e)->meshID=part.mesh;c.objects.push_back(e);
    }
    poseFlyingCasing(c);flyingCasings.push_back(std::move(c));return true;
}
static bool crossedPreviewCue(float before,float time,float event,bool wasPlaying){
    if(time<before)return event>before||event<=time;
    return event<=time && (event>before||(!wasPlaying&&event==before));
}
void updateCasingPreview(float dt){
    if(editing||(!assembled&&!instance)||!casingSocket||!arms.animations.GetCount()){
        if(!flyingCasings.empty()||lastCasingTime)clearFlyingCasings();
        return;
    }
    auto& clip=arms.animations[anim];
    const bool playing=clip.IsPlaying();
    const float duration=clip.end-clip.start;
    const float time=posePhase()*duration;
    auto cue=casingTimes.find(currentClip());
    if(playing&&cue!=casingTimes.end()){
        const float event=cue->second;
        const float before=lastCasingTime.value_or(0.f);
        if(crossedPreviewCue(before,time,event,casingWasPlaying))
            emitPreviewCasing(false);
    }
    for(auto it=flyingCasings.begin();it!=flyingCasings.end();){
        if(it->manual||playing)it->age+=std::max(0.f,dt);
        if(it->age>=.65f||(!playing&&!it->manual&&lastCasingTime&&time!=*lastCasingTime)){
            for(auto e:it->objects)arms.Entity_Remove(e,false);
            it=flyingCasings.erase(it);
        }else{poseFlyingCasing(*it);++it;}
    }
    lastCasingTime=time;casingWasPlaying=playing;
}
void createCasingPreviewUI(){
    cleanButton(casingPanelButton,"Casing ejection","Choose a casing model and preview ejection with the arms animation.",[this]{
        if(editing){if(!instance)mount();else setMode(false);}
        if(!assembled&&!instance)return;
        pauseForFit();selectWeapon(false);cleanAdvancedOpen=false;casingPanel=true;
    });
    cleanButton(casingAssetButton,"Choose casing model...","Open a static .glb or .gltf casing model.",[this]{
        auto path=choose(false,L"Casing model (*.glb;*.gltf)\0*.glb;*.gltf\0\0",nullptr);
        if(path.empty())return;
        try{loadCasingMesh(path);casingAsset=path;casingScale=1.f;status.SetText("Casing model ready.");}
        catch(const std::exception& e){status.SetText(e.what());}
    });
    cleanButton(casingSetTime,"Set ejection at playhead","Store one ejection cue in the selected animation.",[this]{
        pauseForFit();auto& a=arms.animations[anim];casingTimes[currentClip()]=a.timer-a.start;
        status.SetText("Ejection frame set. Rewind and play to preview.");
    });
    cleanButton(casingClearTime,"Clear ejection frame","Remove the ejection cue from this animation.",[this]{casingTimes.erase(currentClip());});
    cleanButton(casingTry,"Preview ejection now","Preview one casing at the current pose.",[this]{emitPreviewCasing(true);});
    cleanLabel(casingScaleLabel,"Casing scale");
    casingScaleInput.Create("Casing scale");casingScaleInput.SetTooltip("Multiplier for this casing model. Press Enter to apply.");
    casingScaleInput.OnInputAccepted([this](auto& a){
        if(std::isfinite(a.fValue)&&a.fValue>0)casingScale=a.fValue;
        else status.SetText("Enter a positive casing scale.");
    });
    GetGUI().AddWidget(&casingScaleInput);
    cleanLabel(casingAssetLabel,"");cleanLabel(casingTick,"");casingTick.SetColor(wi::Color(209,117,255));
}
void layoutCasingPreviewUI(float x,float span,float tx,float tw,float y){
    for(auto w:std::initializer_list<wi::gui::Widget*>{&casingPanelButton,&casingAssetButton,&casingSetTime,&casingClearTime,&casingTry,&casingAssetLabel,&casingTick,&casingScaleLabel,&casingScaleInput})cleanSetVisible(*w,false);
    if(!casingSocket||(!assembled&&!instance&&!points[0]))return;
    if(editing){
        cleanPlace(casingPanelButton,x,464,span,34);return;
    }
    cleanPlace(casingPanelButton,12,590,cleanLeft()-24,34);
    const auto cue=casingTimes.find(currentClip());
    if(cue!=casingTimes.end()&&arms.animations.GetCount()){
        const auto& a=arms.animations[anim];
        cleanPlace(casingTick,tx+8+std::clamp(cue->second/std::max(.0001f,a.end-a.start),0.f,1.f)*(tw-108)-2,y+51,4,15);
    }
    if(!casingPanel)return;
    cleanSetVisible(cleanAdvanced,false);
    cleanPlace(casingAssetLabel,x,247,span,52);
    auto label=(casingAsset.empty()?"Choose casing model":casingAsset.filename().u8string());
    if(cue==casingTimes.end())label+="\nNo ejection frame for "+currentClip();
    else{char seconds[48];snprintf(seconds,sizeof(seconds),"   %.3f s",cue->second);label+="\n"+currentClip()+seconds;}
    casingAssetLabel.SetText(label);
    cleanPlace(casingAssetButton,x,305,span,34);cleanPlace(casingSetTime,x,349,span,34);
    cleanPlace(casingTry,x,393,span,34);
    cleanPlace(casingScaleLabel,x,437,span*.53f,30);cleanPlace(casingScaleInput,x+span*.55f,437,span*.45f,30);
    if(!GetGUI().IsTyping())casingScaleInput.SetValue(casingScale);
    if(cue!=casingTimes.end())cleanPlace(casingClearTime,x,478,span,32);
}
