bridge::WeaponParticlePlayback particlePreview;
wi::gui::Button particleTab,pickParticle,setParticle,clearParticle,testParticle;
wi::gui::Label flashLengthLabel,emissionLabel;
wi::gui::TextInputField flashLength,emissionLength;
bool particlePage=false;
json& barrelSettings(){auto& s=barrelEffects[spawnPoints.at(flashIndex).name];if(s.is_null())s=json::object();return s;}
float flashSeconds(const std::string& socket)const{
    auto it=barrelEffects.find(socket);return it==barrelEffects.end()?.075f:it->value("flashSeconds",.075f);
}
void previewParticle(const SpawnPoint& socket){
    auto it=barrelEffects.find(socket.name);if(it==barrelEffects.end())return;
    auto file=it->value("file",std::string{});if(file.empty())return;
    XMFLOAT3 p,d;if(!mountedSocketPose(socket,p,d))return;
    auto f=XMLoadFloat3(&d);auto side=XMVector3Normalize(XMVector3Cross(std::abs(d.y)>.95f?XMVectorSet(1,0,0,0):XMVectorSet(0,1,0,0),f));
    if(!particlePreview.Spawn(arms,projectRoot/fs::u8path(file),XMMATRIX(side,XMVector3Cross(f,side),f,XMVectorSetW(XMLoadFloat3(&p),1)),it->value("emitSeconds",.15f)))
        status.SetText("Particle effect could not be opened.");
}
void assignParticle(bridge::ParticleEffect effect,const bridge::ProjectMetadata& project){
    Scene source;bridge::ParticleEffectCommand create(source,projectRoot.u8string(),project.projectId,effect);
    if(!create.Execute())throw std::runtime_error(create.Error());
    auto file=fs::path("Content/Particles/WeaponEffects")/(bridge::GenerateStableId()+".wiscene");
    fs::create_directories((projectRoot/file).parent_path());saveNativePart(source,projectRoot/file);
    rememberMarkers();auto& s=barrelSettings();s["file"]=file.generic_u8string();s["name"]=effect.name;
    if(arms.animations.GetCount()){auto& a=arms.animations[anim];s["times"][currentClip()]=a.timer-a.start;}
}
void chooseParticle(){
    try{
        std::string error;bridge::ProjectMetadata project;
        for(auto& p:fs::directory_iterator(projectRoot))if(p.path().extension()==".renegade"&&bridge::ProjectService().InspectProject(p.path().u8string(),project,error))break;
        if(project.projectId.empty())throw std::runtime_error("Open a project first.");
        auto list=bridge::ListParticleEffects(projectRoot.u8string(),project.projectId,error);
        HMENU menu=CreatePopupMenu();if(!menu)return;
        for(size_t i=0;i<list.size();++i)AppendMenuW(menu,MF_STRING,100+i,fs::u8path(list[i].name).c_str());
        AppendMenuW(menu,MF_STRING,2,L"Create smoke preset in Particles...");AppendMenuW(menu,MF_STRING,1,L"Remove particle from this PSP");
        POINT point{};GetCursorPos(&point);int id=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,point.x,point.y,0,editorWindow,nullptr);DestroyMenu(menu);
        if(id==1){rememberMarkers();auto& s=barrelSettings();for(auto k:{"name","file","times"})s.erase(k);}
        if(id>=100)assignParticle(list.at(id-100),project);
        if(id==2){
            bridge::ParticleEffect p;p.name="Muzzle smoke";bridge::ParticleEffectLayer l;l.id=bridge::GenerateStableId();l.name="Smoke";
            l.emitter.emitCount=55;l.emitter.size=.045f;l.emitter.life=.8f;l.emitter.randomLife=.2f;l.emitter.velocity={0,.09f,.08f};l.emitter.color={.55f,.55f,.55f,.32f};p.layers.push_back(l);
            if(!bridge::SaveParticleEffect(projectRoot.u8string(),project.projectId,p,true,error))throw std::runtime_error(error);
            assignParticle(p,project);
        }
    }catch(const std::exception& ex){status.SetText(ex.what());}
}
void createParticleControls(){
    cleanButton(particleTab,"PSP particles / timing","Set flash duration or attach a Particle Library effect.",[this]{particlePage=!particlePage;soundPage=false;});
    cleanButton(pickParticle,"Choose particle...","Choose a saved effect from Content/Particles for this PSP.",[this]{pauseForFit();chooseParticle();});
    cleanButton(testParticle,"Preview particle","Play at the selected PSP and let emitted particles finish.",[this]{previewParticle(spawnPoints.at(flashIndex));});
    cleanButton(setParticle,"Set particle at playhead","Set the start time for this PSP and clip.",[this]{pauseForFit();rememberMarkers();auto& a=arms.animations[anim];barrelSettings()["times"][currentClip()]=a.timer-a.start;});
    cleanButton(clearParticle,"Clear particle cue","Clear this clip's particle start time.",[this]{rememberMarkers();barrelSettings()["times"].erase(currentClip());});
    auto input=[this](auto& field,const char* key,const char* help){
        field.Create(key);field.SetTooltip(help);GetGUI().AddWidget(&field);
        field.OnInputAccepted([this,key](auto& ev){if(!std::isfinite(ev.fValue)||ev.fValue<.01f||ev.fValue>60){status.SetText("Enter 0.01 to 60 seconds.");return;}rememberMarkers();barrelSettings()[key]=ev.fValue;});
    };
    input(flashLength,"flashSeconds","Duration of the flash, in seconds.");input(emissionLength,"emitSeconds","How long to emit. Existing particles finish their lifetime.");
    cleanLabel(flashLengthLabel,"Flash duration (s)");cleanLabel(emissionLabel,"Emit for (s)");
}
void layoutParticles(float x,float span){
    cleanInspector.SetText("PSP particles / timing");cleanDescription.SetText("Choose a barrel, then a particle from the project.");
    if(spawnPoints.empty()){cleanPlace(flashInfo,x,290,span,32);flashInfo.SetText("Add a PSP in Weapon markers.");return;}
    cleanPlace(flashSocketList,x,288,span,30);
    auto it=barrelEffects.find(spawnPoints[flashIndex].name);json empty=json::object();const auto& s=it==barrelEffects.end()?empty:*it;
    cleanPlace(pickParticle,x,328,span,32);pickParticle.SetText(s.value("name",std::string("Choose particle...")));
    cleanPlace(flashLengthLabel,x,370,span*.58f,28);cleanPlace(flashLength,x+span*.6f,370,span*.4f,28);
    cleanPlace(emissionLabel,x,405,span*.58f,28);cleanPlace(emissionLength,x+span*.6f,405,span*.4f,28);
    if(!GetGUI().IsTyping()){flashLength.SetValue(s.value("flashSeconds",.075f));emissionLength.SetValue(s.value("emitSeconds",.15f));}
    bool has=s.contains("file");cleanPlace(testParticle,x,445,span,30);testParticle.SetEnabled(has);
    cleanPlace(setParticle,x,483,span,30);setParticle.SetEnabled(has&&arms.animations.GetCount());
    bool timed=has&&s.contains("times")&&s.at("times").contains(currentClip());
    cleanPlace(clearParticle,x,520,span,30);clearParticle.SetEnabled(timed);
    cleanPlace(flashInfo,x,558,span,32);flashInfo.SetText(timed?currentClip()+" at "+std::to_string(s.at("times").at(currentClip()).get<float>())+" s":"No particle cue for "+currentClip());
}
