// Weapon preparation uses the projectile editor's native launch-socket schema.
using SpawnPoint = renegade::bridge::LaunchSocketDefinition;
std::vector<SpawnPoint> spawnPoints;
std::optional<SpawnPoint> casingSocket;
static constexpr int casingSelection = -2;
static constexpr int placeCasing = 4;
static constexpr XMFLOAT4 casingColour{.82f,.46f,1.f,1.f};
int selectedSpawn = -1;
Entity markerHandle = 0;
wi::gui::Button markerSetup, addSpawn, removeSpawn, replaceSpawn, casingButton, removeCasing;
wi::gui::ComboBox spawnList;
wi::gui::TextInputField spawnName;

struct MarkerPreview {
    XMFLOAT3 position{};
    std::optional<SpawnPoint> socket;
    XMFLOAT3 direction{};
};
std::optional<MarkerPreview> ghost;
bool markerPressed=false, paletteDrag=false;

void clearMarkerPreview(){
    ghost.reset();markerPressed=false;paletteDrag=false;
}
using FlashTimes=std::map<std::string,std::map<std::string,float>>;
FlashTimes flashTimes;
json barrelEffects=json::object();
int flashStyle=0;
struct MarkerEdit {
    json firing;
    json effects;
    FlashTimes flash;
    int style;
    std::string texture;
    std::array<std::optional<XMFLOAT3>,2> grips;
    std::vector<SpawnPoint> sockets;
    std::optional<SpawnPoint> casing;
    int selected;
    bool fit;
};
std::vector<MarkerEdit> markerUndo,markerRedo;
MarkerEdit markerState() const {return {firingDraft,barrelEffects,flashTimes,flashStyle,flashTexture,points,spawnPoints,casingSocket,selectedSpawn,hasFit};}
void rememberMarkers(){
    markerUndo.push_back(markerState());
    if(markerUndo.size()>100)markerUndo.erase(markerUndo.begin());
    markerRedo.clear();
}
bool hasSelectedSpawn() const {return selectedSpawn>=0&&size_t(selectedSpawn)<spawnPoints.size();}
SpawnPoint* selectedSocket(){
    if(selectedSpawn==casingSelection && casingSocket)return &*casingSocket;
    return hasSelectedSpawn()?&spawnPoints[selectedSpawn]:nullptr;
}
void refreshMarkerList(){
    spawnList.ClearItems();
    for(const auto& s:spawnPoints)spawnList.AddItem(s.name);
    if(hasSelectedSpawn()){
        spawnList.SetSelectedWithoutCallback(selectedSpawn);
        spawnName.SetValue(spawnPoints[selectedSpawn].name);
    }
}
void selectSpawn(int index){
    clearMarkerPreview();selectedSpawn=index;marker=-1;gizmo.selected.clear();
    refreshMarkerList();
    const auto* active=selectedSocket();
    if(!editing||!active)return;
    const auto& s=*active;
    auto parent=renegade::bridge::ResolveLaunchSocketParent(prop,s.parentPath);
    auto t=prop.transforms.GetComponent(parent);
    if(!t){status.SetText("The marker parent is missing from this model.");return;}
    if(!placement.transforms.Contains(markerHandle))markerHandle=placement.Entity_CreateTransform("PSP editing handle");
    auto handle=placement.transforms.GetComponent(markerHandle);
    const auto& r=s.rotationDegrees;
    auto local=XMMatrixRotationRollPitchYaw(XMConvertToRadians(r.x),XMConvertToRadians(r.y),XMConvertToRadians(r.z))*XMMatrixTranslationFromVector(XMLoadFloat3(&s.position));
    auto world=local*XMLoadFloat4x4(&t->world);
    handle->ClearTransform();handle->Rotate(gripBasis(world));handle->Translate(world.r[3]);handle->UpdateTransform();
    gizmo.scene=&placement;gizmo.pivot_offset={};
    PickResult pick;pick.entity=markerHandle;gizmo.selected.push_back(pick);
    if(tool==2)setTool(0);
}
void undoMarker(bool redo){
    auto& from=redo?markerRedo:markerUndo;auto& to=redo?markerUndo:markerRedo;
    if(from.empty()||draggingFit)return;
    to.push_back(markerState());auto state=std::move(from.back());from.pop_back();
    firingDraft=std::move(state.firing);barrelEffects=std::move(state.effects);resetFlashTexture(std::move(state.texture));flashTimes=std::move(state.flash);flashStyle=state.style;
    points=state.grips;spawnPoints=std::move(state.sockets);casingSocket=std::move(state.casing);hasFit=state.fit;selectSpawn(state.selected);refreshFlashChoices();
}
std::optional<MarkerPreview> previewMarker(const wi::primitive::Ray& ray,int kind){
    if(kind<0||kind>placeCasing)return {};
    const auto hit=prop.Intersects(ray,wi::enums::FILTER_OPAQUE|wi::enums::FILTER_TRANSPARENT);
    if(!hit.entity)return {};
    MarkerPreview result;result.position=hit.position;
    if(kind<2)return result;
    if(kind==2 && spawnPoints.size()>=16)return {};
    if(kind==3 && !hasSelectedSpawn())return {};
    const bool casing=kind==placeCasing;
    const bool add=kind==2 || (casing && !casingSocket);
    SpawnPoint s;
    if(!add)s=casing?*casingSocket:spawnPoints[selectedSpawn];
    if(add){
        if(casing)s.name="Casing ejection";
        else{
            int n=1;
            do{s.name="PSP "+std::to_string(n++);}
            while(std::any_of(spawnPoints.begin(),spawnPoints.end(),[&](const auto& other){return other.name==s.name;}));
        }
        if(!renegade::bridge::ResolveLaunchSocketParent(prop,"")){
            for(auto& p:renegade::bridge::CollectLaunchSocketParents(prop))
                if(p.entity==hit.entity){s.parentPath=p.path;break;}
        }
    }
    auto parent=prop.transforms.GetComponent(renegade::bridge::ResolveLaunchSocketParent(prop,s.parentPath));
    if(!parent)return {};
    XMVECTOR determinant;
    auto inverse=XMMatrixInverse(&determinant,XMLoadFloat4x4(&parent->world));
    if(std::abs(XMVectorGetX(determinant))<1.e-7f)return {};
    s.position=ArmFit::f3(XMVector3TransformCoord(XMLoadFloat3(&hit.position),inverse));
    if(add){
        auto n=XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&hit.normal),inverse));
        s.rotationDegrees={-XMConvertToDegrees(asinf(std::clamp(XMVectorGetY(n),-1.f,1.f))),XMConvertToDegrees(atan2f(XMVectorGetX(n),XMVectorGetZ(n))),0};
    }
    std::string error;
    if(!renegade::bridge::ValidateLaunchSockets({s},error)||
       !renegade::bridge::LaunchSocketInspectionPose(prop,s,result.position,result.direction,error))return {};
    result.socket=s;
    return result;
}
void dropMarker(){
    if(!ghost||marker<0)return;
    rememberMarkers();
    if(marker<2){
        points[marker]=ghost->position;hasFit=false;
        status.SetText(marker?"Support contact placed.":"Main grip placed.");
        selectSpawn(-1);
    }else if(marker==placeCasing){
        casingSocket=ghost->socket;
        selectSpawn(casingSelection);
        status.SetText("Casing marker placed. W moves it. E rotates the ejection arrow.");
    }else{
        auto s=*ghost->socket;
        if(marker==2){spawnPoints.push_back(s);selectSpawn(int(spawnPoints.size())-1);}
        else{spawnPoints[selectedSpawn]=s;selectSpawn(selectedSpawn);}
        status.SetText("PSP placed. W moves it. E rotates the direction arrow.");
    }
}
void armMarker(int kind){
    if(model.empty()||draggingFit)return;
    if(!editing)setMode(true);
    selectSpawn(kind==placeCasing?casingSelection:kind==3?selectedSpawn:-1);
    marker=kind;cleanAdvancedOpen=false;
    status.SetText("Drag over the model. Release to place. Esc cancels.");
}
void paletteMarker(wi::gui::Button& button,int kind){
    button.OnDragStart([this,kind](auto&){
        if(!editing||draggingFit)return;
        armMarker(kind);markerPressed=paletteDrag=true;
    });
    button.SetTooltip("Drag onto the model, or click and hover to preview. Release on the surface to place.");
}
XMFLOAT3 markerScreenPosition(const XMFLOAT3& point) const{
    return ArmFit::f3(XMVector3Project(XMLoadFloat3(&point),0,0,GetLogicalWidth(),GetLogicalHeight(),0,1,XMLoadFloat4x4(&camera->Projection),XMLoadFloat4x4(&camera->View),XMMatrixIdentity()));
}
int markerAt(XMFLOAT4 pointer) const{
    int chosen=-1;float nearest=11.f*11.f;
    auto consider=[&](const XMFLOAT3& pos,int index){
        auto p=markerScreenPosition(pos);
        float dx=pointer.x-p.x,dy=pointer.y-p.y,d=dx*dx+dy*dy;
        if(p.z>0&&p.z<1&&d<=nearest){chosen=index;nearest=d;}
    };
    for(int i=0;i<2;++i)if(points[i])consider(*points[i],i);
    for(size_t i=0;i<spawnPoints.size();++i){
        XMFLOAT3 pos,dir;std::string error;
        if(renegade::bridge::LaunchSocketInspectionPose(prop,spawnPoints[i],pos,dir,error))consider(pos,int(i)+2);
    }
    if(casingSocket){
        XMFLOAT3 pos,dir;std::string error;
        if(renegade::bridge::LaunchSocketInspectionPose(prop,*casingSocket,pos,dir,error))consider(pos,casingSelection);
    }
    return chosen;
}
void updateMarkerPlacement(XMFLOAT4 pointer,bool viewport){
    if(!editing||draggingFit){ghost.reset();return;}
    if(GetForegroundWindow()!=editorWindow||wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE)){
        if(markerPressed||ghost||marker>=0)selectSpawn(selectedSpawn);
        return;
    }
    const bool pressed=wi::input::Press(wi::input::MOUSE_BUTTON_LEFT);
    const bool down=wi::input::Down(wi::input::MOUSE_BUTTON_LEFT);
    if(marker<0&&viewport&&pressed){
        int hit=markerAt(pointer);
        if(hit==casingSelection){armMarker(placeCasing);}
        else if(hit>=0){
            if(hit>=2)selectedSpawn=hit-2;
            armMarker(hit<2?hit:3);
        }
    }
    if(marker<0)return;
    const bool overCanvas=pointer.x>cleanLeft()&&pointer.x<GetLogicalWidth()-cleanRight()&&pointer.y>viewportTop&&pointer.y<viewportBottom();
    const bool surfaceInput=viewport||(paletteDrag&&overCanvas);
    ghost.reset();
    if(surfaceInput&&!wi::input::Down(wi::input::MOUSE_BUTTON_RIGHT)&&!wi::input::Down(wi::input::MOUSE_BUTTON_MIDDLE)){
        ghost=previewMarker(wi::renderer::GetPickRay((long)pointer.x,(long)pointer.y,*this,*camera),marker);
        if(pressed)markerPressed=true;
    }
    if(markerPressed&&!down){
        if(ghost)dropMarker();
        else{selectSpawn(selectedSpawn);status.SetText("Marker placement cancelled.");}
    }
}
void updateSpawnHandle(XMFLOAT4 pointer,bool viewport){
    auto active=selectedSocket();
    if(!active||gizmo.selected.empty())return;
    if(!viewport&&!draggingFit){pointer.x=-10000;pointer.y=-10000;}
    gizmo.Update(*camera,pointer,*this);
    if(gizmo.IsDragStarted()){rememberMarkers();draggingFit=true;}
    if(!draggingFit)return;
    auto& s=*active;
    auto parent=prop.transforms.GetComponent(renegade::bridge::ResolveLaunchSocketParent(prop,s.parentPath));
    auto handle=placement.transforms.GetComponent(markerHandle);
    if(handle&&parent){
        const auto world=XMLoadFloat4x4(&parent->world);
        const auto inverse=XMMatrixInverse(nullptr,world);
        auto candidate=s;
        candidate.position=ArmFit::f3(XMVector3TransformCoord(handle->GetPositionV(),inverse));
        if(tool==1){
            const auto forward=XMVector3Normalize(XMVector3TransformNormal(handle->GetForwardV(),inverse));
            candidate.rotationDegrees.x=-XMConvertToDegrees(asinf(std::clamp(XMVectorGetY(forward),-1.f,1.f)));
            candidate.rotationDegrees.y=XMConvertToDegrees(atan2f(XMVectorGetX(forward),XMVectorGetZ(forward)));
        }
        std::string error;
        if(renegade::bridge::ValidateLaunchSockets({candidate},error))s=candidate;else status.SetText(error);
    }
    if(gizmo.IsDragEnded()){draggingFit=false;selectSpawn(selectedSpawn);}
}
void createMarkerControls(){
    cleanButton(markerSetup,"Weapon markers","Place grips, projectile spawn points and the casing ejection marker.",[this]{if(!editing){pauseForFit();setMode(true);}marker=-1;cleanAdvancedOpen=false;selectSpawn(-1);});
    cleanButton(addSpawn,"Add PSP","Click on the model to add a Projectile Spawn Point.",[this]{armMarker(2);});
    cleanButton(replaceSpawn,"Place again","Click a surface to move the selected PSP.",[this]{armMarker(3);});
    cleanButton(removeSpawn,"Remove PSP","Remove the selected spawn point. Undo restores it.",[this]{
        if(!hasSelectedSpawn())return;
        rememberMarkers();barrelEffects.erase(spawnPoints[selectedSpawn].name);flashTimes.erase(spawnPoints[selectedSpawn].name);spawnPoints.erase(spawnPoints.begin()+selectedSpawn);
        selectSpawn(std::min(selectedSpawn,int(spawnPoints.size())-1));
    });
    cleanButton(casingButton,"Add casing marker","Place the optional casing ejection marker.",[this]{
        if(casingSocket)selectSpawn(casingSelection);
        else armMarker(placeCasing);
    });
    paletteMarker(casingButton,placeCasing);
    casingButton.SetTooltip("Drag onto the model to place the casing marker. Click an existing marker's button to select it. E rotates its arrow.");
    cleanButton(removeCasing,"Remove casing marker","Remove the ejection marker. Undo restores it.",[this]{
        if(!casingSocket)return;
        rememberMarkers();casingSocket.reset();selectSpawn(-1);
    });
    paletteMarker(mainGrip,0);paletteMarker(support,1);paletteMarker(addSpawn,2);paletteMarker(replaceSpawn,3);
    spawnList.Create("Projectile Spawn Points");spawnList.SetText("");spawnList.SetMaxVisibleItemCount(6);
    spawnList.SetTooltip("Select a PSP to move or rotate it.");spawnList.OnSelect([this](auto& a){selectSpawn(a.iValue);});GetGUI().AddWidget(&spawnList);
    spawnName.Create("PSP name");spawnName.SetTooltip("Type a unique PSP name and press Enter.");spawnName.OnInputAccepted([this](auto&){
        if(!hasSelectedSpawn())return;
        auto candidate=spawnPoints;candidate[selectedSpawn].name=spawnName.GetText();std::string error;
        if(renegade::bridge::ValidateLaunchSockets(candidate,error)){rememberMarkers();
            auto previous=spawnPoints[selectedSpawn].name;auto binding=firingBinding();if(binding.launchSocketName==previous&&(originalFiring||!firingDraft.is_null()))firingDraft["socket"]=candidate[selectedSpawn].name;if(binding.secondSocketName==previous)firingDraft["second"]=candidate[selectedSpawn].name;if(barrelEffects.contains(previous)){auto fx=barrelEffects.at(previous);barrelEffects.erase(previous);barrelEffects[candidate[selectedSpawn].name]=std::move(fx);}
            auto cue=flashTimes.extract(spawnPoints[selectedSpawn].name);
            if(!cue.empty()){cue.key()=candidate[selectedSpawn].name;flashTimes.insert(std::move(cue));}
            spawnPoints=std::move(candidate);}
        else status.SetText(error);
        refreshMarkerList();
    });GetGUI().AddWidget(&spawnName);
}
void layoutMarkerControls(){
    for(wi::gui::Widget* w:std::initializer_list<wi::gui::Widget*>{&markerSetup,&spawnList,&spawnName,&addSpawn,&removeSpawn,&replaceSpawn,&casingButton,&removeCasing})cleanSetVisible(*w,false);
    if(!model.empty()&&!editing)cleanPlace(markerSetup,12,548,cleanLeft()-24,34);
    if(!editing||model.empty())return;
    cleanObjects.SetText("WEAPON MARKERS");
    for(auto& item:cleanItems)cleanSetVisible(item,false);
    cleanPlace(mainGrip,12,124,cleanLeft()-24,34);cleanPlace(support,12,165,cleanLeft()-24,34);
    cleanPlace(addSpawn,12,216,cleanLeft()-24,34);addSpawn.SetEnabled(spawnPoints.size()<16);
    if(!spawnPoints.empty())cleanPlace(spawnList,12,259,cleanLeft()-24,30);
    cleanPlace(casingButton,12,332,cleanLeft()-24,30);
    casingButton.SetText(casingSocket?"Casing marker":"Add casing marker");
    if(selectedSpawn==casingSelection && casingSocket)
        cleanPlace(removeCasing,12,464,cleanLeft()-24,30);
    if(hasSelectedSpawn()){
        cleanPlace(spawnName,12,298,cleanLeft()-24,28);cleanPlace(replaceSpawn,12,464,cleanLeft()-24,30);
        cleanPlace(removeSpawn,12,503,cleanLeft()-24,30);
    }
    cleanSetVisible(cleanNote,false);
}
void drawMarkerDisc(const XMFLOAT3& point,XMFLOAT4 colour,const std::string& label,bool selected,wi::graphics::CommandList cmd,bool preview=false) const{
    const float width=GetLogicalWidth(),height=GetLogicalHeight();
    auto p=markerScreenPosition(point);
    if(!(p.z>0&&p.z<1&&p.x>=cleanLeft()&&p.x<=width-cleanRight()&&p.y>=viewportTop&&p.y<=viewportBottom()))return;
    auto disc=[&](float radius,XMFLOAT4 c){
        wi::image::Params f;f.pos={p.x-radius,p.y-radius,0};f.siz={radius*2,radius*2};f.color=c;f.enableCornerRounding();
        for(auto& corner:f.corners_rounding)corner.radius=radius;
        wi::image::Draw(nullptr,f,cmd);
    };
    if(preview){
        disc(14,{1,1,1,.32f});colour.w=.48f;disc(11,colour);
    }else{
        disc(selected?12:11,selected?XMFLOAT4{1,1,1,1}:XMFLOAT4{.03f,.035f,.04f,1});disc(9,colour);
    }
    wi::font::Params font;font.posX=p.x;font.posY=p.y;font.size=12;font.h_align=wi::font::WIFALIGN_CENTER;font.v_align=wi::font::WIFALIGN_CENTER;font.color=wi::Color(15,20,22);
    wi::font::Draw(label,font,cmd);
}
void drawWeaponMarkers(wi::graphics::CommandList cmd) const{
    if(!editing)return;
    wi::image::SetCanvas(*this);wi::font::SetCanvas(*this);
    if(points[0])drawMarkerDisc(*points[0],{1,.48f,.12f,1},"M",false,cmd);
    if(points[1])drawMarkerDisc(*points[1],{.1f,.85f,1,1},"S",false,cmd);
    for(size_t i=0;i<spawnPoints.size();i++){
        XMFLOAT3 p,d;std::string error;
        if(renegade::bridge::LaunchSocketInspectionPose(prop,spawnPoints[i],p,d,error))
            drawMarkerDisc(p,{.57f,1,.32f,1},std::to_string(i+1),int(i)==selectedSpawn,cmd);
    }
    if(casingSocket){
        XMFLOAT3 pos,dir;std::string error;
        if(renegade::bridge::LaunchSocketInspectionPose(prop,*casingSocket,pos,dir,error))
            drawMarkerDisc(pos,casingColour,"C",selectedSpawn==casingSelection,cmd);
    }
    if(ghost&&marker>=0){
        auto colour=marker==placeCasing?casingColour:marker==0?XMFLOAT4{1,.48f,.12f,1}:marker==1?XMFLOAT4{.1f,.85f,1,1}:XMFLOAT4{.57f,1,.32f,1};
        auto label=marker==placeCasing?"C":marker==0?"M":marker==1?"S":std::to_string(marker==2?int(spawnPoints.size()+1):selectedSpawn+1);
        drawMarkerDisc(ghost->position,colour,label,false,cmd,true);
    }
}
void drawMarkerArrow(XMFLOAT3 p,XMFLOAT3 d,XMFLOAT4 colour){
        auto start=XMLoadFloat3(&p),dir=XMLoadFloat3(&d);
        float length=std::max(.0001f,distance*.065f);
        auto tip=start+dir*length;
        auto side=XMVector3Cross(dir,XMLoadFloat3(&camera->Eye)-tip);
        if(ArmFit::length(side)<1.e-7f)side=XMVector3Cross(dir,XMVectorSet(0,1,0,0));
        if(ArmFit::length(side)<1.e-7f)side=XMVector3Cross(dir,XMVectorSet(1,0,0,0));
        side=XMVector3Normalize(side)*length*.12f;
        auto line=[&](XMVECTOR a,XMVECTOR b){wi::renderer::RenderableLine l;l.start=ArmFit::f3(a);l.end=ArmFit::f3(b);l.color_start=l.color_end=colour;wi::renderer::DrawLine(l);};
        line(start,tip);line(tip,tip-dir*length*.25f+side);line(tip,tip-dir*length*.25f-side);
}
void drawSpawnDirections(){
    if(!editing)return;
    for(const auto& s:spawnPoints){
        XMFLOAT3 p,d;std::string error;
        if(renegade::bridge::LaunchSocketInspectionPose(prop,s,p,d,error))drawMarkerArrow(p,d,{.57f,1,.32f,1.f});
    }
    if(casingSocket){
        XMFLOAT3 pos,dir;std::string error;
        if(renegade::bridge::LaunchSocketInspectionPose(prop,*casingSocket,pos,dir,error))
            drawMarkerArrow(pos,dir,casingColour);
    }
    if(ghost&&ghost->socket){
        auto colour=marker==placeCasing?casingColour:XMFLOAT4{.57f,1,.32f,1.f};
        colour.w=.62f;drawMarkerArrow(ghost->position,ghost->direction,colour);
    }
}

