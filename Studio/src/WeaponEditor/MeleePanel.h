bool melee=false,meleeOpen=false,twoHandMelee=false;
bridge::MeleeWeaponSettings strike;
std::string restClip,attackClip;
wi::gui::ComboBox weaponKind,meleeIdle,meleeAttack,meleeGrip;
wi::gui::Button meleePanelButton,previewSwing,meleeSounds;
wi::gui::TextInputField meleeReach,meleeDamage;
wi::gui::Label reachLabel,damageLabel,contactLabel;
wi::gui::Slider contactTime;
std::array<std::string,5> extraMeleeClips{};
const std::array<const char*,5> meleeRoles={"Attack2","Attack3","BlockIn","BlockHold","BlockOut"};
wi::gui::ComboBox meleeRole,meleeRoleClip;
wi::gui::Label meleeRoleLabel;
void refreshRoleClip(){
    meleeRoleClip.ClearItems();meleeRoleClip.AddItem("(None)");
    int selected=0,index=0;
    for(auto& item:preset.at("clips")){
        auto name=item.at("source").get<std::string>();
        meleeRoleClip.AddItem(name);++index;
        if(name==extraMeleeClips[std::max(0,meleeRole.GetSelected())])selected=index;
    }
    meleeRoleClip.SetSelectedWithoutCallback(selected);
}
json meleeDocument()const{
    return {{"extraClips",extraMeleeClips},{"idle",restClip},{"attack",attackClip},{"twoHands",twoHandMelee},{"reach",strike.reach},{"damage",strike.damage},{"contact",strike.contact}};
}
void readMelee(const json& setup){
    melee=setup.value("weaponType",std::string("firearm"))=="melee";auto d=setup.value("melee",json::object());
    extraMeleeClips=d.value("extraClips",std::array<std::string,5>{});
    restClip=d.value("idle",std::string{});attackClip=d.value("attack",std::string{});twoHandMelee=d.value("twoHands",false);
    strike={d.value("reach",2.25f),d.value("damage",12.f),d.value("contact",.5f)};
    if(!strike.Valid())throw std::runtime_error("Invalid melee reach, damage or contact time.");
}
void refreshMeleeClips(){
    meleeIdle.ClearItems();meleeAttack.ClearItems();
    for(const auto& clip:preset.at("clips")){
        const std::string name=clip.at("source");
        if(restClip.empty() && (name=="Idle" || name=="idle"))restClip=name;
        if(attackClip.empty() && (name=="Attack" || name=="Swing"))attackClip=name;
    }
    for(auto& c:preset.at("clips")){
        auto n=c.at("source").get<std::string>();auto lower=n;
        std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char v){return char(std::tolower(v));});
        if(restClip.empty()&&lower.find("idle")!=lower.npos)restClip=n;
        if(attackClip.empty()&&(lower.find("attack")!=lower.npos||lower.find("swing")!=lower.npos||lower.find("slash")!=lower.npos))attackClip=n;
        meleeIdle.AddItem(n);meleeAttack.AddItem(n);
    }
    int idle=-1,attack=-1,index=0;
    for(auto& c:preset.at("clips")){if(c.at("source")==restClip)idle=index;if(c.at("source")==attackClip)attack=index;++index;}
    meleeIdle.SetSelectedWithoutCallback(idle);meleeAttack.SetSelectedWithoutCallback(attack);meleeGrip.SetSelectedWithoutCallback(twoHandMelee?1:0);
    refreshRoleClip();
    contactTime.SetValue(strike.contact*100);
}
void meleePose(bool playSwing){
    for(size_t i=0;i<arms.animations.GetCount();++i){auto n=arms.names.GetComponent(arms.animations.GetEntity(i));if(!n||n->name!=attackClip)continue;
        selectClip(int(i));auto& a=arms.animations[i];a.SetLooped(false);
        if(!playSwing){pauseForFit();a.timer=a.start+(a.end-a.start)*strike.contact;a.last_update_time=-1;}return;
    }
    status.SetText("Choose an attack animation first.");
}
void createMelee(){
    auto combo=[this](auto& c,const char* title){c.Create(title);c.SetText("");c.SetTooltip(title);GetGUI().AddWidget(&c);};
    cleanLabel(meleeRoleLabel,"Additional melee actions");
    combo(meleeRole,"Action to assign");combo(meleeRoleClip,"Animation for the selected action");
    for(auto label:{"Second attack","Third attack","Raise guard","Hold block","Lower guard"})meleeRole.AddItem(label);
    meleeRole.SetSelectedWithoutCallback(0);
    meleeRole.OnSelect([this](auto&){refreshRoleClip();});
    meleeRoleClip.OnSelect([this](auto& ev){extraMeleeClips[std::max(0,meleeRole.GetSelected())]=ev.iValue>0?preset.at("clips").at(ev.iValue-1).at("source").get<std::string>():std::string{};});
    combo(weaponKind,"Weapon type");weaponKind.AddItem("Firearm");weaponKind.AddItem("Melee");
    weaponKind.OnSelect([this](auto& ev){clearFlashPlayback();clearFlyingCasings();melee=ev.iValue==1;firingOpen=flashPanel=casingPanel=false;meleeOpen=melee;refreshMeleeClips();});
    cleanButton(meleePanelButton,"Melee attack","Choose animations and place the contact frame.",[this]{selectWeapon(false);surfaceOpen=false;meleeOpen=true;refreshMeleeClips();});
    combo(meleeIdle,"Idle animation");combo(meleeAttack,"Attack animation");combo(meleeGrip,"Grip");
    meleeIdle.OnSelect([this](auto& ev){restClip=preset.at("clips").at(ev.iValue).at("source");});
    meleeAttack.OnSelect([this](auto& ev){attackClip=preset.at("clips").at(ev.iValue).at("source");meleePose(false);});
    meleeGrip.AddItem("One-handed");meleeGrip.AddItem("Two-handed");meleeGrip.OnSelect([this](auto& ev){twoHandMelee=ev.iValue==1;});
    cleanLabel(reachLabel,"Reach (m)");cleanLabel(damageLabel,"Damage");cleanLabel(contactLabel,"Contact (%)");
    auto field=[this](auto& w,const char* title,float lo,float hi,float* value){w.Create(title);w.SetTooltip(title);GetGUI().AddWidget(&w);w.OnInputAccepted([this,value,lo,hi](auto& ev){if(std::isfinite(ev.fValue)&&ev.fValue>=lo&&ev.fValue<=hi)*value=ev.fValue;else status.SetText("Enter a value within the displayed range.");});};
    field(meleeReach,"Contact reach in metres (0.1 to 20)",.1f,20,&strike.reach);field(meleeDamage,"Damage per contact (0 to 100000)",0,100000,&strike.damage);
    contactTime.Create(0,100,50,1000,"Contact (%)");contactTime.SetText("");contactTime.SetTooltip("Scrub to the point where the swing can hit.");contactTime.OnSlide([this](auto& ev){strike.contact=std::clamp(ev.fValue*.01f,0.f,1.f);meleePose(false);});GetGUI().AddWidget(&contactTime);
    cleanButton(previewSwing,"Preview swing","Play the selected attack animation once.",[this]{meleePose(true);});
    cleanButton(meleeSounds,"Animation sounds...","Place swing sounds at animation frames.",[this]{openFlashPanel();soundPage=true;particlePage=false;meleeOpen=false;refreshAudioList();});
}
void layoutMelee(float x,float span,float tx,float tw,float y){
    for(auto w:std::initializer_list<wi::gui::Widget*>{&meleeRoleLabel,&meleeRole,&meleeRoleClip,&weaponKind,&meleeIdle,&meleeAttack,&meleeGrip,&meleePanelButton,&meleeReach,&meleeDamage,&reachLabel,&damageLabel,&contactLabel,&contactTime,&previewSwing,&meleeSounds})cleanSetVisible(*w,false);
    cleanPlace(weaponKind,12,55,cleanLeft()-24,29);weaponKind.SetSelectedWithoutCallback(melee?1:0);
    if(!melee)return;
    for(auto w:std::initializer_list<wi::gui::Widget*>{&firingButton,&flashPanelButton,&casingPanelButton,&addSpawn,&spawnList,&spawnName,&replaceSpawn,&removeSpawn,&casingButton,&removeCasing,&effectsTab,&particleTab,&shotSoundButton})cleanSetVisible(*w,false);
    for(auto& v:cleanEvents)cleanSetVisible(v,false);
    for(int i=1;i<=2;++i)cleanSetVisible(cleanItems[i],false);
    if(editing||!hand||!instance)return;
    cleanPlace(meleePanelButton,12,512,cleanLeft()-24,32);
    if(!meleeOpen||surfaceOpen||flashPanel)return;
    for(auto& entry:cleanVisibility)if(entry.second&&entry.first->GetPos().x>=x-1&&entry.first!=&fullIKButton)entry.second=false;
    cleanPlace(cleanInspector,x,90,span,32);cleanInspector.SetText("Melee attack");
    cleanPlace(cleanDescription,x,133,span,45);cleanDescription.SetText("Choose the idle and attack clips, then scrub to contact.");
    cleanPlace(meleeIdle,x,190,span,30);cleanPlace(meleeAttack,x,229,span,30);cleanPlace(meleeGrip,x,270,span,30);
    cleanPlace(reachLabel,x,314,span*.52f,30);cleanPlace(meleeReach,x+span*.54f,314,span*.46f,30);
    cleanPlace(damageLabel,x,352,span*.52f,30);cleanPlace(meleeDamage,x+span*.54f,352,span*.46f,30);
    if(!GetGUI().IsTyping()){meleeReach.SetValue(strike.reach);meleeDamage.SetValue(strike.damage);}
    cleanPlace(previewSwing,x,396,span,32);cleanPlace(meleeSounds,x,436,span,32);
    cleanPlace(meleeRoleLabel,x,477,span,24);cleanPlace(meleeRole,x,505,span,30);cleanPlace(meleeRoleClip,x,542,span,30);
    cleanPlace(contactLabel,tx,y+108,110,28);cleanPlace(contactTime,tx+120,y+108,tw-200,28);
}
