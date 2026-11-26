// Short-lived, depth-tested game effects. Every barrel samples its own animated socket.
std::array<Entity,4> flashMeshes{};
std::map<std::string,Entity> visibleFlashes;
std::map<std::string,float> flashBursts;
bool flashPanel=false, flashWasPlaying=false;
std::optional<float> lastFlashTime;
int flashIndex=0;
bool automaticFire=false,loopShotSound=false;
std::string shotSound;
wi::audio::Sound previewShotSound;
wi::audio::SoundInstance previewShotVoice;
wi::gui::Button fireModeButton,shotSoundButton,loopSoundButton;
void stopPreviewShotLoop(){
    if(previewShotVoice.IsValid()&&previewShotVoice.IsLooped()){
        wi::audio::Stop(&previewShotVoice);previewShotVoice={};
    }
}
void setShotSound(std::string path){
    stopPreviewShotLoop();
    shotSound=std::move(path);previewShotSound={};
    if(!shotSound.empty())wi::audio::CreateSound((projectRoot/fs::u8path(shotSound)).u8string(),&previewShotSound);
}
wi::gui::Button flashPanelButton, flashSet, flashClear, flashTry;
wi::gui::ComboBox flashSocketList, flashLooks;
wi::gui::Label flashInfo,flashTick;

bool soundPage=false;
wi::gui::Button effectsTab,importFlash,addAudioCue,moveAudioCue,removeAudioCue;
wi::gui::ComboBox audioCueList;
std::string flashTexture;
using AudioCues=std::map<std::string,std::vector<std::pair<float,std::string>>>;
AudioCues audioCues;
std::map<std::string,wi::audio::Sound> cueSounds;
std::array<wi::audio::SoundInstance,8> cueVoices;
unsigned cueVoice=0;
std::string listedClip;
bool loadCue(const std::string& path){
    if(cueSounds.count(path))return true;
    wi::audio::Sound sound;
    if(!wi::audio::CreateSound((projectRoot/fs::u8path(path)).u8string(),&sound))return false;
    cueSounds.emplace(path,std::move(sound));return true;
}
void playCue(const std::string& path){
    if(!loadCue(path))return;
    auto& voice=cueVoices[cueVoice++%cueVoices.size()];
    if(voice.IsValid())wi::audio::Stop(&voice);
    if(wi::audio::CreateSoundInstance(&cueSounds.at(path),&voice)){
        wi::audio::SetVolume(.65f,&voice);wi::audio::Play(&voice);
    }
}
std::string importEffectFile(const fs::path& file,bool sound){
    if(file.empty())return {};
    try{
        bool valid=false;
        if(sound){wi::audio::Sound test;valid=wi::audio::CreateSound(file.u8string(),&test);}
        else {auto image=wi::resourcemanager::Load(file.u8string());valid=image.IsValid()&&image.GetTexture().IsValid();}
        if(!valid)throw std::runtime_error("Cannot read the selected effect file.");
        auto root=fs::weakly_canonical(projectRoot);
        auto rel=fs::weakly_canonical(file).lexically_relative(root);
        if(rel.generic_u8string().rfind("Content/",0)==0)return rel.generic_u8string();
        auto dir=root/"Content/Player/WeaponFX"/(sound?"Audio":"Textures");
        fs::create_directories(dir);
        for(unsigned index=0;;++index){
            auto name=index?file.stem().wstring()+L"-"+std::to_wstring(index)+file.extension().wstring():file.filename().wstring();
            auto dest=dir/name;
            if(fs::copy_file(file,dest,fs::copy_options::skip_existing))return dest.lexically_relative(root).generic_u8string();
            if(fs::file_size(file)!=fs::file_size(dest))continue;
            std::ifstream a(file,std::ios::binary),b(dest,std::ios::binary);
            if(a&&b&&std::equal(std::istreambuf_iterator<char>(a),{},std::istreambuf_iterator<char>(b)))return dest.lexically_relative(root).generic_u8string();
        }
    }catch(const std::exception& error){status.SetText(error.what());return {};}
}
void refreshAudioList(){
    listedClip=currentClip();audioCueList.ClearItems();
    auto found=audioCues.find(listedClip);
    if(found==audioCues.end())return;
    for(const auto& cue:found->second){
        char time[32];snprintf(time,sizeof(time),"%.3f s  ",cue.first);
        audioCueList.AddItem(time+fs::u8path(cue.second).filename().u8string());
    }
}
void resetFlashTexture(std::string path){
    clearFlashPlayback();
    if(flashMeshes[3]){arms.Entity_Remove(flashMeshes[3]);flashMeshes[3]=0;}
    flashTexture=std::move(path);
}
void hideFlash(const std::string& socket){
    auto it=visibleFlashes.find(socket);
    if(it==visibleFlashes.end())return;
    arms.Entity_Remove(it->second,false);visibleFlashes.erase(it);
}
#include "ParticleCues.h"
void clearFlashPlayback(){
    stopPreviewShotLoop();
    particlePreview.Clear(arms);
    while(!visibleFlashes.empty())hideFlash(visibleFlashes.begin()->first);
    flashBursts.clear();lastFlashTime.reset();flashWasPlaying=false;
}
Entity flashMesh(int style){
    if(flashMeshes[style])return flashMeshes[style];
    static const char* files[]={"Compact.png","Star.png","Long.png"};
    auto path=(style==3?projectRoot/fs::u8path(flashTexture):home/"FlashEffects"/files[style]).u8string();
    auto texture=wi::resourcemanager::Load(path);
    if(!texture.IsValid()||!texture.GetTexture().IsValid()){
        status.SetText("Muzzle flash texture missing: "+path);return 0;
    }
    Entity e=wi::ecs::CreateEntity();
    auto& mat=arms.materials.Create(e);
    mat.shaderType=MaterialComponent::SHADERTYPE_UNLIT;
    mat.userBlendMode=wi::enums::BLENDMODE_ADDITIVE;
    mat.baseColor={1.f,.76f,.37f,1};mat.SetCastShadow(false);
    auto& map=mat.textures[MaterialComponent::BASECOLORMAP];map.name=path;map.resource=texture;
    auto& mesh=arms.meshes.Create(e);mesh.SetDoubleSided(true);
    auto quad=[&](XMFLOAT3 a,XMFLOAT3 b,XMFLOAT3 c,XMFLOAT3 d,XMFLOAT3 normal){
        uint32_t n=(uint32_t)mesh.vertex_positions.size();
        for(auto p:{a,b,c,d})mesh.vertex_positions.push_back(p);
        for(auto uv:{XMFLOAT2{0,1},XMFLOAT2{1,1},XMFLOAT2{1,0},XMFLOAT2{0,0}})mesh.vertex_uvset_0.push_back(uv);
        for(int i=0;i<4;++i)mesh.vertex_normals.push_back(normal);
        for(auto i:{0,1,2,0,2,3})mesh.indices.push_back(n+i);
    };
    // The bright base of each side texture lies about 14% above its bottom edge.
    quad({-.10f,0,-.034f},{.10f,0,-.034f},{.10f,0,.21f},{-.10f,0,.21f},{0,-1,0});
    quad({0,-.10f,-.034f},{0,.10f,-.034f},{0,.10f,.21f},{0,-.10f,.21f},{1,0,0});
    quad({-.043f,-.043f,.017f},{.043f,-.043f,.017f},{.043f,.043f,.017f},{-.043f,.043f,.017f},{0,0,1});
    auto& subset=mesh.subsets.emplace_back();subset.materialID=e;subset.indexCount=uint32_t(mesh.indices.size());
    mesh.CreateRenderData();flashMeshes[style]=e;return e;
}
bool showFlash(const SpawnPoint& socket,float brightness){
    XMFLOAT3 p,d;
    if(!mountedSocketPose(socket,p,d)){hideFlash(socket.name);return false;}
    auto mesh=flashMesh(flashTexture.empty()?flashStyle:3);if(!mesh)return false;
    auto& obj=visibleFlashes[socket.name];
    if(!obj)obj=arms.Entity_CreateObject("Muzzle flash preview");
    auto object=arms.objects.GetComponent(obj);object->meshID=mesh;object->color={brightness,brightness,brightness,brightness};
    auto f=XMLoadFloat3(&d);
    auto up=std::abs(d.y)>.95f?XMVectorSet(1,0,0,0):XMVectorSet(0,1,0,0);
    auto side=XMVector3Normalize(XMVector3Cross(up,f));
    auto t=arms.transforms.GetComponent(obj);t->ClearTransform();
    auto size=.80f+.20f*brightness;
    t->MatrixTransform(XMMatrixScaling(size,size,size)*XMMATRIX(side,XMVector3Cross(f,side),f,XMVectorSetW(XMLoadFloat3(&p),1)));
    t->UpdateTransform();return true;
}
void updateFlashPlayback(float dt){
    if(editing||(!assembled&&!instance)||!arms.animations.GetCount()){
        if(lastFlashTime||!flashBursts.empty()||!visibleFlashes.empty())clearFlashPlayback();
        return;
    }
    particlePreview.Update(arms,dt);
    if(!particlePreview.live.empty())bridge::UpdateParticleEffectSprites(arms,dt);
    const auto& a=arms.animations[anim];const bool playing=a.IsPlaying();
    const float time=posePhase()*(a.end-a.start);
    const auto clip=currentClip();
    auto audio=audioCues.find(clip);
    const bool firingClip=clip=="Fire"||clip=="ADS Fire"||(assembled&&
        (assembledActions.value("Attack",std::string{})==clip||assembledActions.value("AimAttack",std::string{})==clip));
    const bool previewLoop=playing && firingClip && automaticFire && loopShotSound && audio==audioCues.end();
    if(!previewLoop)stopPreviewShotLoop();
    else if(previewShotSound.IsValid() && !previewShotVoice.IsLooped()){
        previewShotVoice={};previewShotVoice.SetLooped(true);
        if(wi::audio::CreateSoundInstance(&previewShotSound,&previewShotVoice)){
            wi::audio::SetVolume(.65f,&previewShotVoice);wi::audio::Play(&previewShotVoice);
        }else previewShotVoice={};
    }
    if(playing && audio!=audioCues.end())for(const auto& cue:audio->second)
        if(crossedPreviewCue(lastFlashTime.value_or(0.f),time,cue.first,flashWasPlaying))playCue(cue.second);
    if(melee){lastFlashTime=time;flashWasPlaying=playing;return;}
    for(auto& s:spawnPoints){
        float duration=flashSeconds(s.name);float intensity=0;
        auto effect=barrelEffects.find(s.name);
        if(playing&&effect!=barrelEffects.end()&&effect->contains("times")&&effect->at("times").contains(clip)&&crossedPreviewCue(lastFlashTime.value_or(0.f),time,effect->at("times").at(clip).get<float>(),flashWasPlaying))previewParticle(s);
        auto cues=flashTimes.find(s.name);
        if(cues!=flashTimes.end()){
            auto it=cues->second.find(clip);
            if(it==cues->second.end()&&assembled&&firingClip)it=cues->second.find(assembledActions.value("AimAttack",std::string{})==clip?"ADS Fire":"Fire");
            if(it!=cues->second.end()){
                float elapsed=time-it->second;
                if(elapsed>=0&&elapsed<duration)intensity=1-elapsed/duration;
                if(playing&&crossedPreviewCue(lastFlashTime.value_or(0.f),time,it->second,flashWasPlaying))
                {
                    flashBursts[s.name]=duration;
                    if(!previewLoop && audio==audioCues.end() && &s==&spawnPoints.front() && previewShotSound.IsValid() && wi::audio::CreateSoundInstance(&previewShotSound,&previewShotVoice)){
                        wi::audio::SetVolume(.65f,&previewShotVoice);wi::audio::Play(&previewShotVoice);}
                }
            }
        }
        auto burst=flashBursts.find(s.name);
        if(burst!=flashBursts.end()){
            intensity=std::max(intensity,std::min(1.f,burst->second/duration));
            burst->second-=std::max(0.f,dt);
            if(burst->second<=0)flashBursts.erase(burst);
        }
        if(intensity>0)showFlash(s,intensity);else hideFlash(s.name);
    }
    lastFlashTime=time;flashWasPlaying=playing;
}
void refreshFlashChoices(){
    flashSocketList.ClearItems();
    for(const auto& s:spawnPoints)flashSocketList.AddItem(s.name);
    flashIndex=std::clamp(flashIndex,0,std::max(0,int(spawnPoints.size())-1));
    if(!spawnPoints.empty())flashSocketList.SetSelectedWithoutCallback(flashIndex);
    flashLooks.SetSelectedWithoutCallback(flashTexture.empty()?flashStyle:3);
}
void openFlashPanel(){
    if(editing){if(!instance)mount();else setMode(false);}
    if(!assembled&&!instance)return;
    pauseForFit();selectWeapon(false);cleanAdvancedOpen=false;flashPanel=true;
    refreshFlashChoices();
}
void createFlashControls(){
    createParticleControls();createFiring();createMelee();
    cleanButton(flashPanelButton,"Flash / SFX","Choose a PSP and preview the muzzle flash on an animation.",[this]{openFlashPanel();});
    cleanButton(fireModeButton,"Trigger","Toggle between one shot per click and repeating while held.",[this]{automaticFire=!automaticFire;});
    cleanButton(effectsTab,"Animation SFX","Switch between flash placement and animation sound cues.",[this]{particlePage=false;soundPage=!soundPage;refreshAudioList();});
    cleanButton(loopSoundButton,"Sound: per shot","Loop the firing sound while an automatic weapon is firing.",[this]{loopShotSound=!loopShotSound;});
    cleanButton(shotSoundButton,"Import firing sound...","Default firing sound when a firing clip has no authored sound cues.",[this]{
        auto path=importEffectFile(choose(false,L"Firing audio (*.wav;*.ogg)\0*.wav;*.ogg\0\0",nullptr),true);
        if(!path.empty())setShotSound(path);
    });
    cleanButton(importFlash,"Import flash PNG...","Import a flash texture from disk for this weapon.",[this]{
        auto path=importEffectFile(choose(false,L"Flash image (*.png)\0*.png\0\0",L"png"),false);
        if(!path.empty()){rememberMarkers();resetFlashTexture(path);refreshFlashChoices();}
    });
    cleanButton(addAudioCue,"Add WAV at playhead...","Pause at a frame and choose a sound. Each clip can have several sound cues.",[this]{
        if(!arms.animations.GetCount())return;
        pauseForFit();const auto& a=arms.animations[anim];float time=a.timer-a.start;
        auto path=importEffectFile(choose(false,L"Firing audio (*.wav;*.ogg)\0*.wav;*.ogg\0\0",nullptr),true);
        if(path.empty())return;
        auto& cues=audioCues[currentClip()];cues.emplace_back(time,path);
        std::stable_sort(cues.begin(),cues.end());refreshAudioList();
        audioCueList.SetSelectedWithoutCallback(int(std::find(cues.begin(),cues.end(),std::make_pair(time,path))-cues.begin()));
    });
    audioCueList.Create("Sound cues");audioCueList.SetText("");GetGUI().AddWidget(&audioCueList);
    audioCueList.OnSelect([this](auto& ev){
        auto it=audioCues.find(currentClip());if(it==audioCues.end()||size_t(ev.iValue)>=it->second.size())return;
        pauseForFit();auto& a=arms.animations[anim];const auto& cue=it->second[ev.iValue];
        a.timer=a.start+cue.first;a.last_update_time=-1;playCue(cue.second);
    });
    cleanButton(moveAudioCue,"Move cue to playhead","Retime the selected sound to the current animation frame.",[this]{
        auto it=audioCues.find(currentClip());int index=audioCueList.GetSelected();
        if(it==audioCues.end()||size_t(index)>=it->second.size())return;
        pauseForFit();const auto& a=arms.animations[anim];auto& cues=it->second;cues[index].first=a.timer-a.start;
        auto moved=cues[index];std::stable_sort(cues.begin(),cues.end());refreshAudioList();
        audioCueList.SetSelectedWithoutCallback(int(std::find(cues.begin(),cues.end(),moved)-cues.begin()));
    });
    cleanButton(removeAudioCue,"Remove selected cue","Remove this sound from this clip.",[this]{
        auto it=audioCues.find(currentClip());int index=audioCueList.GetSelected();
        if(it==audioCues.end()||size_t(index)>=it->second.size())return;
        it->second.erase(it->second.begin()+index);if(it->second.empty())audioCues.erase(it);refreshAudioList();
    });
    flashSocketList.Create("Projectile Spawn Point");flashSocketList.SetText("");flashSocketList.SetTooltip("Barrel to preview and edit.");
    flashSocketList.OnSelect([this](auto& e){flashIndex=e.iValue;});GetGUI().AddWidget(&flashSocketList);
    flashLooks.Create("Flash effect");flashLooks.SetText("");flashLooks.SetTooltip("Flash appearance for this weapon.");
    for(auto s:{"Narrow flash","Broad flash","Star flash","Imported PNG"})flashLooks.AddItem(s);
    flashLooks.OnSelect([this](auto& e){if(e.iValue==3){if(flashTexture.empty())refreshFlashChoices();return;}rememberMarkers();resetFlashTexture({});flashStyle=e.iValue;});GetGUI().AddWidget(&flashLooks);
    cleanButton(flashSet,"Set flash at playhead","Save this barrel's flash frame for the selected animation.",[this]{
        if(flashIndex<0||flashIndex>=int(spawnPoints.size())||!arms.animations.GetCount())return;
        pauseForFit();auto& a=arms.animations[anim];rememberMarkers();
        flashTimes[spawnPoints[flashIndex].name][currentClip()]=a.timer-a.start;
    });
    cleanButton(flashClear,"Clear flash frame","Remove this barrel's cue from the selected animation.",[this]{
        if(flashIndex<0||flashIndex>=int(spawnPoints.size()))return;
        auto it=flashTimes.find(spawnPoints[flashIndex].name);if(it==flashTimes.end())return;
        rememberMarkers();it->second.erase(currentClip());if(it->second.empty())flashTimes.erase(it);
    });
    cleanButton(flashTry,"Preview flash now","Show this PSP's flash at the current pose.",[this]{
        if(flashIndex>=0&&flashIndex<int(spawnPoints.size()))flashBursts[spawnPoints[flashIndex].name]=flashSeconds(spawnPoints[flashIndex].name);
    });
    cleanLabel(flashInfo,"");cleanLabel(flashTick,"");flashTick.SetColor(wi::Color(255,175,54));
}
void layoutFlashControls(float x,float span,float tx,float tw,float y){
    for(auto w:std::initializer_list<wi::gui::Widget*>{&particleTab,&pickParticle,&testParticle,&setParticle,&clearParticle,&flashLengthLabel,&emissionLabel,&flashLength,&emissionLength,&effectsTab,&importFlash,&addAudioCue,&moveAudioCue,&removeAudioCue,&audioCueList,&fireModeButton,&shotSoundButton,&loopSoundButton,&flashPanelButton,&flashSocketList,&flashLooks,&flashSet,&flashClear,&flashTry,&flashInfo,&flashTick})cleanSetVisible(*w,false);
    if(model.empty()||(!assembled&&!instance&&!points[0]))return;
    if(editing){cleanPlace(flashPanelButton,x,506,span,34);return;}
    cleanPlace(flashPanelButton,12,casingSocket?632:590,cleanLeft()-24,34);
    if(!flashPanel)return;
    cleanSetVisible(cleanAdvanced,false);
    fireModeButton.SetText(automaticFire?"Trigger: Automatic":"Trigger: Single shot");
    cleanPlace(effectsTab,x,194,span,34);effectsTab.SetText(soundPage?"Show muzzle flash":"Show animation SFX");
    cleanSetVisible(cleanInstruction,false);
    cleanPlace(particleTab,x,particlePage?236:594,span,32);particleTab.SetText(particlePage?"Back to muzzle flash":"PSP particles / timing");
    if(particlePage){layoutParticles(x,span);return;}
    if(soundPage){
        if(listedClip!=currentClip())refreshAudioList();
        cleanInspector.SetText("Animation SFX");cleanDescription.SetText("Choose a clip and scrub to the frame where the sound should start.");
        cleanPlace(shotSoundButton,x,242,span,32);
        cleanPlace(loopSoundButton,x,492,span,32);loopSoundButton.SetEnabled(automaticFire);loopSoundButton.SetText(loopShotSound?"Sound: loop while firing":"Sound: per shot");
        cleanPlace(flashInfo,x,280,span,32);flashInfo.SetText(shotSound.empty()?"No default firing sound":fs::u8path(shotSound).filename().u8string());
        cleanPlace(addAudioCue,x,322,span,32);cleanPlace(audioCueList,x,365,span,30);
        cleanPlace(moveAudioCue,x,408,span,32);moveAudioCue.SetEnabled(audioCueList.GetItemCount()>0);
        cleanPlace(removeAudioCue,x,450,span,32);removeAudioCue.SetEnabled(audioCueList.GetItemCount()>0);
        return;
    }
    cleanPlace(importFlash,x,510,span,32);cleanPlace(fireModeButton,x,552,span,32);
    importFlash.SetTooltip(flashTexture.empty()?"Choose a local PNG texture.":flashTexture);
    cleanPlace(flashInfo,x,246,span,36);
    if(spawnPoints.empty()){
        flashInfo.SetText("Add a PSP in Weapon markers.");return;
    }
    cleanPlace(flashSocketList,x,294,span,30);cleanPlace(flashLooks,x,337,span,30);
    cleanPlace(flashSet,x,380,span,34);cleanPlace(flashTry,x,424,span,34);
    const auto& s=spawnPoints[flashIndex];
    auto t=flashTimes.find(s.name);const std::string clip=currentClip();
    if(t!=flashTimes.end()&&t->second.count(clip)){
        float seconds=t->second.at(clip);char label[64];snprintf(label,sizeof(label)," flash at %.3f s",seconds);
        flashInfo.SetText(clip+label);cleanPlace(flashClear,x,468,span,32);
        auto& a=arms.animations[anim];cleanPlace(flashTick,tx+8+std::clamp(seconds/std::max(.0001f,a.end-a.start),0.f,1.f)*(tw-108)-2,y+51,4,15);
    }else flashInfo.SetText(clip+" has no flash cue for this PSP.");
}


