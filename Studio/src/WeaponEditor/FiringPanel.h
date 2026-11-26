using FireBinding=bridge::EquipmentProjectileBinding;
json firingDraft;
std::string linkedItem,linkedHash,linkedPresentationHash;
fs::path linkedSetup;
wi::gui::Button firingButton,saveExisting,exportCopy,shotPreview,makeProjectile,createProjectile,cancelProjectile;
wi::gui::ComboBox firingMode,projectileList,firstBarrel,otherBarrel,barrelPolicy,newPreset;
wi::gui::TextInputField newName;
wi::gui::Slider fireTime;
wi::gui::Label fireCaption;
bool firingOpen=false,newProjectile=false,scheduledShot=false;
unsigned alternatingBarrel=0;
bridge::ProjectMetadata firingProject;
std::vector<bridge::ProjectileAssetDocument> projectileLibrary;
bridge::ProjectileSimulation shotSimulation;
std::vector<std::pair<XMFLOAT3,XMFLOAT3>> shotLines;
std::map<uint64_t,Entity> shotObjects;
std::map<std::string,std::shared_ptr<Scene>> previewModels;
void clearShotObjects(){
    for(auto& visual:shotObjects)arms.Entity_Remove(visual.second);
    shotObjects.clear();shotSimulation.Reset();shotLines.clear();scheduledShot=false;
}
void spawnShotObject(uint64_t id,const bridge::ProjectileAssetDocument& asset){
    auto root=arms.Entity_CreateTransform("Shot preview");shotObjects.emplace(id,root);
    if(!asset.meshAssetId.empty()){
        auto& mesh=previewModels[asset.meshAssetId];
        if(!mesh){
            auto placed=bridge::ReusableAssetService().PrepareModelAssetPlacement({projectRoot.u8string(),firingProject.projectId,asset.meshAssetId});
            if(placed.IsReady()){mesh=std::make_shared<Scene>();mesh->Merge(*placed.PeekMutableScene());}
        }
        if(mesh){auto node=arms.Instantiate(*mesh,true);arms.Component_Attach(node,root,true);}
    }
    for(auto& layer:asset.flightEffects){
        auto emitter=bridge::CreateProjectileEffectEmitter(arms,layer,{layer.offset[0],layer.offset[1],layer.offset[2]});
        if(emitter)arms.Component_Attach(emitter,root,true);
    }
}
void positionShotObjects(){
    std::set<uint64_t> live;
    for(auto& shot:shotSimulation.Records()){
        live.insert(shot.id);auto it=shotObjects.find(shot.id);if(it==shotObjects.end())continue;
        for(auto& d:projectileLibrary)if(d.assetId==shot.launch.source.sourceAssetId){
            auto t=arms.transforms.GetComponent(it->second);t->ClearTransform();t->MatrixTransform(bridge::ProjectileVisualPose(d,shot.launch.velocity,shot.launch.position,shot.grenadeAngle));t->UpdateTransform();break;
        }
    }
    for(auto it=shotObjects.begin();it!=shotObjects.end();){if(live.count(it->first)){++it;continue;}arms.Entity_Remove(it->second);it=shotObjects.erase(it);}
}

float trailTime=0;
std::optional<FireBinding> originalFiring;
void saveExistingWeapon();
void exportWeaponCopy();
void updateLibraryWeapon();
std::string equipmentForSetup(const fs::path& path){
    auto folder=projectRoot/"Content/Player/Loadouts";
    if(path.empty()||!fs::is_directory(folder))return {};
    for(auto& f:fs::directory_iterator(folder))if(f.path().extension()==".slots")
        for(auto& slot:bridge::ReadWeaponSlots(projectRoot,f.path().stem().u8string())){
            std::error_code ec;if(!slot.setup.empty()&&fs::equivalent(path,projectRoot/fs::u8path(slot.setup),ec))return slot.equipment;
        }
    return {};
}
void resolveFiringProject(){
    std::string error;firingProject={};
    for(auto& f:fs::directory_iterator(projectRoot))if(f.path().extension()==".renegade"&&bridge::ProjectService().InspectProject(f.path().u8string(),firingProject,error))return;
    throw std::runtime_error("Open a project before editing firing.");
}
void linkEquipment(const fs::path& path){
    linkedItem=equipmentForSetup(path);linkedSetup=linkedItem.empty()?fs::path{}:path;
    linkedHash.clear();linkedPresentationHash.clear();originalFiring.reset();if(linkedItem.empty())return;
    resolveFiringProject();bridge::AssetRegistry reg;std::string error;
    if(!bridge::ReadAssetRegistry(projectRoot.u8string(),firingProject.projectId,reg,error))throw std::runtime_error(error);
    for(auto& rec:reg.records)if(rec.assetId==linkedItem)linkedHash=rec.contentHash;
    bridge::EquipmentAssetDocument source;
    if(!bridge::LoadEquipmentAsset(projectRoot.u8string(),firingProject.projectId,linkedItem,source,error))throw std::runtime_error(error);
    for(auto& rec:reg.records)if(rec.assetId==source.equipment.presentationAssetId)linkedPresentationHash=rec.contentHash;
    for(auto& v:source.equipment.projectiles)if(v.action==bridge::EquipmentAction::PrimaryUse)originalFiring=v;
}
FireBinding firingBinding()const{
    FireBinding b=originalFiring.value_or(FireBinding{});
    if(!originalFiring)b.fireMode=bridge::EquipmentFireMode::Hitscan;
    if(!firingDraft.is_null()){
        b.fireMode=static_cast<bridge::EquipmentFireMode>(firingDraft.value("mode",int(b.fireMode)));
        b.projectileAssetId=firingDraft.value("projectile",b.projectileAssetId);
        b.launchSocketName=firingDraft.value("socket",b.launchSocketName);b.socketPolicy=firingDraft.value("policy",b.socketPolicy);
        b.secondSocketName=firingDraft.value("second",b.secondSocketName);
    }
    if(b.launchSocketName.empty()&&!spawnPoints.empty())b.launchSocketName=spawnPoints.front().name;
    if(!originalFiring){auto it=flashTimes.find(b.launchSocketName);if(it!=flashTimes.end()&&it->second.count("Fire"))b.releaseSeconds=it->second.at("Fire");}
    if(firingDraft.contains("time"))b.releaseSeconds=firingDraft.at("time").get<float>();
    if(!b.socketPolicy)b.secondSocketName.clear();
    return b;
}
void applyFiring(bridge::EquipmentDefinition& item)const{
    if(melee){item.projectiles.clear();return;}
    if(firingDraft.is_null())return;
    auto binding=firingBinding();
    if(binding.fireMode==bridge::EquipmentFireMode::Projectile&&binding.projectileAssetId.empty())throw std::runtime_error("Choose a projectile before saving.");
    auto present=[&](auto& n){return std::any_of(spawnPoints.begin(),spawnPoints.end(),[&](auto& s){return s.name==n;});};
    if(!present(binding.launchSocketName)||(binding.socketPolicy&&(!present(binding.secondSocketName)||binding.secondSocketName==binding.launchSocketName)))throw std::runtime_error("Choose the spawn point for each barrel.");
    auto it=std::find_if(item.projectiles.begin(),item.projectiles.end(),[](auto& v){return v.action==bridge::EquipmentAction::PrimaryUse;});
    if(it==item.projectiles.end())item.projectiles.push_back(binding);else *it=binding;
}
void syncFiring(){
    auto b=firingBinding();firingMode.ClearItems();firingMode.AddItem("Travelling projectile");firingMode.AddItem("Hitscan");if(int(b.fireMode)>1)firingMode.AddItem(b.fireMode==bridge::EquipmentFireMode::Storm?"Storm (advanced)":"Channel (advanced)");firingMode.SetSelectedWithoutCallback(std::min(2,int(b.fireMode)));
    projectileList.ClearItems();projectileList.AddItem("Choose projectile...");int pick=0;
    for(auto& p:projectileLibrary){projectileList.AddItem(p.name);if(p.assetId==b.projectileAssetId)pick=projectileList.GetItemCount()-1;}
    projectileList.SetSelectedWithoutCallback(pick);
    firstBarrel.ClearItems();otherBarrel.ClearItems();int first=-1,second=-1;
    for(size_t i=0;i<spawnPoints.size();++i){auto& n=spawnPoints[i].name;firstBarrel.AddItem(n);otherBarrel.AddItem(n);if(n==b.launchSocketName)first=int(i);if(n==b.secondSocketName)second=int(i);}
    firstBarrel.SetSelectedWithoutCallback(first);otherBarrel.SetSelectedWithoutCallback(second);barrelPolicy.SetSelectedWithoutCallback(b.socketPolicy);
    float length=.5f;for(size_t i=0;i<arms.animations.GetCount();++i){auto n=arms.names.GetComponent(arms.animations.GetEntity(i));if(n&&n->name=="Fire")length=arms.animations[i].end-arms.animations[i].start;}
    fireTime.SetRange(0,std::max(.001f,length));fireTime.SetValue(b.releaseSeconds);
}
void openFiring(){
    try{
        if(firingOpen){firingOpen=false;return;}
        resolveFiringProject();std::string error;projectileLibrary=bridge::ListProjectileAssets(projectRoot.u8string(),firingProject.projectId,error);
        if(!error.empty())throw std::runtime_error(error);
        if(editing)setMode(false);selectWeapon(false);surfaceOpen=false;flashPanel=casingPanel=false;
        firingOpen=true;newProjectile=false;pauseForFit();syncFiring();
    }catch(const std::exception& ex){status.SetText(ex.what());}
}
void startShotPreview(){
    auto b=firingBinding();
    if(spawnPoints.empty()||(b.fireMode==bridge::EquipmentFireMode::Projectile&&b.projectileAssetId.empty())){status.SetText("Choose a PSP and projectile first.");return;}
    std::string wanted=currentClip()=="ADS Fire"?"ADS Fire":"Fire";
    for(size_t i=0;i<arms.animations.GetCount();++i){
        auto n=arms.names.GetComponent(arms.animations.GetEntity(i));if(!n||n->name!=wanted)continue;
        selectClip(int(i));arms.animations[i].SetLooped(false);scheduledShot=true;shotLines.clear();trailTime=.35f;
        return;
    }
    status.SetText("Assign a Fire animation to preview the shot.");
}
void updateShotPreview(float dt){
    if(!firingOpen){clearShotObjects();return;}
    auto binding=firingBinding();auto& a=arms.animations[anim];
    if(scheduledShot&&a.timer-a.start+.0001f>=binding.releaseSeconds){
        scheduledShot=false;std::vector<std::string> names={binding.launchSocketName};
        if(binding.socketPolicy==1&&alternatingBarrel++%2)names[0]=binding.secondSocketName;
        if(binding.socketPolicy==2)names.push_back(binding.secondSocketName);
        for(auto& name:names)for(auto& socket:spawnPoints)if(socket.name==name){
            XMFLOAT3 p,d;if(!mountedSocketPose(socket,p,d))continue;trailTime=.35f;
            if(binding.fireMode==bridge::EquipmentFireMode::Hitscan)shotLines.push_back({p,{p.x+d.x*binding.hitscanRangeMetres,p.y+d.y*binding.hitscanRangeMetres,p.z+d.z*binding.hitscanRangeMetres}});
            else for(auto& asset:projectileLibrary)if(asset.assetId==binding.projectileAssetId){
                bridge::ProjectileLaunch l;l.definition=bridge::ProjectileSimulationDefinition(asset);l.source={"preview",asset.assetId,"editor"};l.position={p.x,p.y,p.z};auto v=asset.speedMetresPerSecond;l.velocity={v*d.x,v*d.y,v*d.z};
                uint64_t id;std::string error;if(shotSimulation.Launch(l,id,error))spawnShotObject(id,asset);else status.SetText(error);
            }
        }
    }
    std::vector<bridge::ProjectileImpact> hits;std::string error;
    if(!shotSimulation.Records().empty())shotSimulation.Update(std::clamp(dt,0.f,1.f),[this](const auto&,const auto& from,const auto& to){if(shotLines.size()<500)shotLines.push_back({{from.x,from.y,from.z},{to.x,to.y,to.z}});return bridge::ProjectileQueryResult{};},hits,error);
    positionShotObjects();
    if((trailTime-=dt)<=0){shotLines.clear();return;}
    for(auto& segment:shotLines){wi::renderer::RenderableLine l;l.start=segment.first;l.end=segment.second;l.color_start=l.color_end={1,.7f,.15f,1};wi::renderer::DrawLine(l);}
}
void createFiring(){
    cleanButton(firingButton,"Firing","Set this weapon's projectile and preview its shot.",[this]{openFiring();});
    cleanButton(saveExisting,"Save changes","Update this library weapon wherever it is equipped.",[this]{saveExistingWeapon();});
    cleanButton(exportCopy,"Export prefab...","Write a portable copy of this weapon for another project.",[this]{exportWeaponCopy();});
    auto combo=[this](auto& c,const char* name){c.Create(name);c.SetText("");c.SetTooltip(name);GetGUI().AddWidget(&c);};
    combo(firingMode,"Fire mode");for(auto n:{"Travelling projectile","Hitscan","Channel","Storm"})firingMode.AddItem(n);
    firingMode.OnSelect([this](auto& ev){if(ev.iValue>1){syncFiring();status.SetText("Use Weapon Firing in Starting Equipment for advanced abilities.");return;}firingDraft["mode"]=ev.iValue;if(ev.iValue==1)firingDraft["projectile"]="";syncFiring();});
    combo(projectileList,"Projectile");projectileList.OnSelect([this](auto& ev){if(ev.iValue<=0)return;firingDraft["mode"]=0;firingDraft["projectile"]=projectileLibrary.at(ev.iValue-1).assetId;syncFiring();});
    combo(firstBarrel,"Spawn point");combo(otherBarrel,"Second spawn point");combo(barrelPolicy,"Barrel firing order");
    for(auto n:{"Single barrel","Alternate barrels","Both barrels together"})barrelPolicy.AddItem(n);
    firstBarrel.OnSelect([this](auto& ev){firingDraft["socket"]=spawnPoints.at(ev.iValue).name;syncFiring();});
    otherBarrel.OnSelect([this](auto& ev){firingDraft["second"]=spawnPoints.at(ev.iValue).name;syncFiring();});
    barrelPolicy.OnSelect([this](auto& ev){firingDraft["policy"]=ev.iValue;if(ev.iValue&&firingBinding().secondSocketName.empty())for(auto& s:spawnPoints)if(s.name!=firingBinding().launchSocketName){firingDraft["second"]=s.name;break;}syncFiring();});
    fireTime.Create(0,1,0,1000,"Fire time (seconds)");fireTime.SetText("");fireTime.SetTooltip("Drag to set the release frame. The flash and sound keep their own timing.");
    fireTime.OnSlide([this](auto& ev){firingDraft["time"]=ev.fValue;for(size_t i=0;i<arms.animations.GetCount();++i){auto n=arms.names.GetComponent(arms.animations.GetEntity(i));if(n&&n->name=="Fire"){if(anim!=int(i))selectClip(int(i));pauseForFit();auto& a=arms.animations[i];a.timer=a.start+ev.fValue;a.last_update_time=-1;break;}}});GetGUI().AddWidget(&fireTime);
    cleanLabel(fireCaption,"Fire time (seconds)");
    cleanButton(shotPreview,"Preview shot","Play the firing clip, its effects and the projectile path.",[this]{startShotPreview();});
    cleanButton(makeProjectile,"New projectile...","Create a reusable projectile in this project.",[this]{newProjectile=true;newName.SetValue("New projectile");});
    combo(newPreset,"Projectile preset");for(int i=0;i<5;++i)newPreset.AddItem(bridge::ProjectilePresetName(static_cast<bridge::ProjectilePreset>(i)));
    newName.Create("Projectile name");GetGUI().AddWidget(&newName);
    cleanButton(cancelProjectile,"Cancel","Return to firing.",[this]{newProjectile=false;});
    cleanButton(createProjectile,"Create and select","Save to Content/Projectiles and bind to this weapon.",[this]{
        auto d=bridge::MakeProjectilePreset(static_cast<bridge::ProjectilePreset>(std::max(0,newPreset.GetSelected())));d.name=newName.GetValue();
        auto result=bridge::SaveProjectileAsset(projectRoot.u8string(),firingProject.projectId,d);if(!result.succeeded){status.SetText(result.error);return;}
        projectileLibrary.push_back(result.document);firingDraft["projectile"]=result.document.assetId;firingDraft["mode"]=0;newProjectile=false;syncFiring();
    });
}
void layoutFiring(float x,float span,float tx,float tw,float timelineY){
    for(auto w:std::initializer_list<wi::gui::Widget*>{&firingButton,&saveExisting,&exportCopy,&firingMode,&firstBarrel,&otherBarrel,&barrelPolicy,&fireTime,&fireCaption,&shotPreview,&projectileList,&newName,&newPreset,&makeProjectile,&createProjectile,&cancelProjectile})cleanSetVisible(*w,false);
    if((!assembled&&(!instance||!hand))||editing)return;
    cleanPlace(firingButton,12,512,cleanLeft()-24,32);
    if(!linkedItem.empty()){cleanPlace(saveExisting,12,678,cleanLeft()-24,32);cleanPlace(exportCopy,12,715,cleanLeft()-24,32);publishWeapon.SetText("Save as new weapon...");}else publishWeapon.SetText("Save Weapon to Library...");
    if(!firingOpen||surfaceOpen||flashPanel||casingPanel)return;
    for(auto& entry:cleanVisibility)if(entry.second&&entry.first->GetPos().x>=x-1&&entry.first!=&fullIKButton)entry.second=false;
    cleanPlace(cleanInspector,x,90,span,32);cleanInspector.SetText(newProjectile?"New projectile":"Firing");
    cleanPlace(cleanDescription,x,133,span,48);cleanDescription.SetText(newProjectile?"Choose a preset and name it.":spawnPoints.empty()?"Place a PSP under Weapon markers.":"Primary fire and aimed fire");
    if(newProjectile){cleanPlace(newPreset,x,200,span,32);cleanPlace(newName,x,245,span,32);cleanPlace(createProjectile,x,290,span,32);cleanPlace(cancelProjectile,x,335,span,32);return;}
    cleanPlace(firingMode,x,198,span,32);cleanPlace(projectileList,x,239,span,32);cleanPlace(makeProjectile,x,280,span,32);
    float y=325;if(spawnPoints.size()>1){cleanPlace(firstBarrel,x,y,span,30);y+=39;cleanPlace(barrelPolicy,x,y,span,30);y+=39;if(firingBinding().socketPolicy){cleanPlace(otherBarrel,x,y,span,30);y+=39;}}
    for(auto& event:cleanEvents)cleanSetVisible(event,false);
    cleanPlace(fireCaption,tx,timelineY+108,130,28);cleanPlace(fireTime,tx+140,timelineY+108,tw-220,28);
    cleanPlace(shotPreview,x,y,span,34);
}
