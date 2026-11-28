// Included inside Preview: finger authoring widgets and interaction.
    FingerFit fingerFit;
    bool fingerMode=false,fingerGesture=false;
    int fingerSide=0,fingerIndex=1,fingerPoseIndex=0;
    std::string fingerDragGrip;
    std::map<int,std::string> fingerOverrides;
    wi::gui::Button fingerToggle,fingerSelectTip,fingerFrame,fingerReset,fingerResetHand;
    wi::gui::ComboBox fingerHandChoice,fingerChoice,fingerPoseChoice;
    wi::gui::CheckBox fingerEnabled,fingerIK;
    wi::gui::Label fingerLabels[8],fingerHint;
    wi::gui::Slider fingerAngles[5];
    std::string fingerContact() const {
        if(magazineMode==1)return FingerFit::key(1);
        if(currentClip()=="Reload"){
            float p=posePhase();
            if(p>=pickupPhase&&p<seatPhase)return FingerFit::key(1);
            if(boltContact.captured&&p>=std::max(seatPhase,boltContact.grab-.04f)&&p<boltContact.release+.04f)return FingerFit::key(2);
        }
        return FingerFit::key(0);
    }
    std::string fingerKey() const {return fingerPoseIndex?FingerFit::key(fingerPoseIndex-1):fingerContact();}
    void applyFingers(){
        float magazine=0,bolt=0;
        if(magazineMode==1)magazine=1;
        else if(currentClip()=="Reload"){
            float p=posePhase();
            magazine=BoltContact::smooth((p-pickupPhase+.035f)/.035f)*(1-BoltContact::smooth((p-seatPhase)/.04f));
            if(boltContact.captured){float begin=std::max(seatPhase,boltContact.grab-.04f);bolt=BoltContact::smooth((p-begin)/std::max(.0001f,boltContact.grab-begin))*(1-BoltContact::smooth((p-boltContact.release)/.04f));}
        }
        fingerFit.apply(magazine,bolt,fingerPoseIndex?fingerKey():std::string(),fingerOverrides);
    }
    void refreshFingerUI(){
        if(!fingerMode||GetGUI().IsTyping()||fingerGesture)return;
        FingerFit::Pose value;auto found=fingerFit.poses.find(fingerKey());if(found!=fingerFit.poses.end())value=found->second[fingerSide][fingerIndex];
        for(int i=0;i<3;i++)fingerAngles[i].SetValue(value.curl[i]);fingerAngles[3].SetValue(value.spread);fingerAngles[4].SetValue(value.opposition);
        fingerIK.SetCheck(value.ik);fingerEnabled.SetCheck(fingerFit.enabled);
        fingerAngles[4].SetEnabled(fingerIndex==0);
        for(int i=0;i<4;i++)fingerAngles[i].SetEnabled(!value.ik&&fingerFit.enabled);
        fingerAngles[4].SetEnabled(fingerIndex==0&&!value.ik&&fingerFit.enabled);
        fingerLabels[2].SetText("Grip preset");
        auto& f=fingerFit.fingers[fingerSide][fingerIndex];
        std::string text=fingerKey()+" / "+FingerFit::name(fingerIndex)+"\n";
        text+=fingerKey()==FingerFit::key(0)?"Normal hold on the rifle fore-end.\n":fingerKey()==FingerFit::key(1)?"Hand shape for holding the magazine.\n":"Hand shape for pulling the handle.\n";
        if(!f.valid)text+="This rig is missing the finger chain.";
        else if(value.ik&&f.error>.002f)text+="Target is beyond finger reach.\nMove it closer; lengths stay fixed.";
        else text+="Curl adjusts each joint.\nTip IK: drag the Move handles.\nPlay returns to automatic grips.";
        fingerHint.SetText(text);
    }
    void pinFingerPose(){
        pauseForFit();fingerOverrides[fingerSide*5+fingerIndex]=fingerKey();
    }
    void setFingerPin(bool enabled){
        rememberFit();pinFingerPose();
        if(enabled){fingerFit.prepareVisiblePose(fingerKey(),fingerSide,fingerIndex);fingerFit.enableIK(fingerKey(),fingerSide,fingerIndex);selectFingertip(fingerSide,fingerIndex);}
        else{fingerFit.poses[fingerKey()][fingerSide][fingerIndex].ik=false;}
        refreshFingerUI();
    }
    void selectFingertip(int side,int index){
        if(side<0||side>1||index<0||index>4||draggingFit||!fullIKEnabled())return;
        auto& f=fingerFit.fingers[side][index];if(!f.valid){status.SetText("This rig has no compatible finger chain.");return;}
        pauseForFit();fingerMode=true;fingerSide=side;fingerIndex=index;
        selectWeapon(false);fitSelection=6+side*5+index;gizmo.pivot_offset={};
        PickResult pick;pick.entity=f.target;gizmo.scene=&arms;gizmo.selected={pick};setTool(0);
        fingerHandChoice.SetSelectedWithoutCallback(side);fingerChoice.SetSelectedWithoutCallback(index);
        status.SetText("Fingertip selected. Drag to move its target; selection keeps the pose.");refreshFingerUI();
    }
    void selectFingerJoint(int side,int index,int joint){
        if(side<0||side>1||index<0||index>4||joint<0||joint>2||draggingFit||!fullIKEnabled())return;
        auto& f=fingerFit.fingers[side][index];if(!f.valid)return;
        pauseForFit();fingerMode=true;fingerSide=side;fingerIndex=index;selectWeapon(false);
        fitSelection=16+(side*5+index)*3+joint;gizmo.pivot_offset={};
        PickResult pick;pick.entity=f.jointTarget[joint];gizmo.scene=&arms;gizmo.selected={pick};setTool(1);
        fingerHandChoice.SetSelectedWithoutCallback(side);fingerChoice.SetSelectedWithoutCallback(index);
        status.SetText("Finger joint selected. Drag a rotation ring to edit this joint.");refreshFingerUI();
    }
    void layoutFingerPanel(){
        float x=std::max(800.f,GetLogicalWidth()-262.f);
        fingerToggle.SetPos({x+118,48});fingerToggle.SetSize({126,29});fingerToggle.SetText(fingerMode?"Arms / Reload":"Finger grips");
        armPanel.SetText(fingerMode?"FINGER GRIPS / IK":"ARM FIT  /  IK CONTROLS");
        for(auto w:std::initializer_list<wi::gui::Widget*>{&applyIK,&showIK,&armReset,&armFrame,&pickupHere,&seatHere,&grabHere,&seatNow,&armHint,&eventLabel,&pickupLabel,&seatLabel,&pickupInput,&seatInput,&boltCaption,&boltGrabLabel,&boltReleaseLabel,&reloadStage,&boltGrabHere,&boltReleaseHere,&boltGrabInput,&boltReleaseInput})w->SetVisible(!fingerMode);
        for(auto& w:armSelect)w.SetVisible(!fingerMode);
        for(auto w:std::initializer_list<wi::gui::Widget*>{&fingerHandChoice,&fingerChoice,&fingerPoseChoice,&fingerEnabled,&fingerIK,&fingerSelectTip,&fingerFrame,&fingerReset,&fingerResetHand,&fingerHint})w->SetVisible(fingerMode);
        for(auto& w:fingerLabels)w.SetVisible(fingerMode);for(auto& w:fingerAngles)w.SetVisible(fingerMode);
        fingerEnabled.SetPos({x+224,121});
        fingerLabels[0].SetPos({x,156});fingerHandChoice.SetPos({x,183});
        fingerLabels[1].SetPos({x,221});fingerChoice.SetPos({x,248});
        fingerLabels[2].SetPos({x,286});fingerPoseChoice.SetPos({x,313});
        for(int i=0;i<5;i++){fingerLabels[i+3].SetPos({x,356.f+i*62});fingerAngles[i].SetPos({x+5,386.f+i*62});}
        fingerIK.SetPos({x+224,675});fingerSelectTip.SetPos({x,711});fingerFrame.SetPos({x,748});fingerReset.SetPos({x,785});fingerResetHand.SetPos({x,822});fingerHint.SetPos({x,868});
        if(fitSelection>=6&&fitSelection<16){tools[1].SetEnabled(false);tools[2].SetEnabled(false);}
    }
    void createFingerPanel(){
        btn(fingerToggle,"Finger grips / Tip IK",48,[this]{if(draggingFit)return;fingerMode=!fingerMode;if(fingerMode&&fitSelection<6)selectWeapon(false);if(!fingerMode){fingerPoseIndex=0;fingerPoseChoice.SetSelectedWithoutCallback(0);if(fitSelection>=6)selectWeapon(false);}refreshFingerUI();});fingerToggle.SetSize({244,29});
        const char* labels[]={"Hand","Finger","Grip follows reload","Base joint curl (degrees)","Middle joint curl (degrees)","Tip joint curl (degrees)","Finger spread (degrees)","Thumb opposition (degrees)"};
        for(int i=0;i<8;i++){auto& l=fingerLabels[i];l.Create(labels[i]);l.SetText(labels[i]);l.SetSize({244,24});l.SetColor(wi::Color(31,35,38));GetGUI().AddWidget(&l);}
        fingerEnabled.Create("Apply finger grips");fingerEnabled.SetSize({22,22});fingerEnabled.SetCheck(true);fingerEnabled.OnClick([this](auto& e){rememberFit();fingerFit.enabled=e.bValue;});GetGUI().AddWidget(&fingerEnabled);
        fingerHandChoice.Create("Hand");fingerHandChoice.SetSize({244,28});fingerHandChoice.SetText("");fingerHandChoice.AddItem("Left / Support hand");fingerHandChoice.AddItem("Right / Main hand");fingerHandChoice.SetSelectedWithoutCallback(0);fingerHandChoice.OnSelect([this](auto& e){if(draggingFit)return;fingerSide=e.iValue;if(fitSelection>=6)selectWeapon(false);refreshFingerUI();});GetGUI().AddWidget(&fingerHandChoice);
        fingerChoice.Create("Finger");fingerChoice.SetSize({244,28});fingerChoice.SetText("");for(int i=0;i<5;i++)fingerChoice.AddItem(FingerFit::name(i));fingerChoice.SetSelectedWithoutCallback(1);fingerChoice.OnSelect([this](auto& e){if(draggingFit)return;fingerIndex=e.iValue;if(fitSelection>=6)selectWeapon(false);refreshFingerUI();});GetGUI().AddWidget(&fingerChoice);
        fingerPoseChoice.Create("Grip pose");fingerPoseChoice.SetSize({244,28});fingerPoseChoice.SetText("");fingerPoseChoice.AddItem("Automatic / follow reload");for(auto label:{"Rifle hold","Magazine grip","Handle grip"})fingerPoseChoice.AddItem(label);fingerPoseChoice.SetSelectedWithoutCallback(0);fingerPoseChoice.OnSelect([this](auto& e){if(draggingFit)return;fingerPoseIndex=e.iValue;fingerOverrides.clear();pauseForFit();if(fitSelection>=6)selectWeapon(false);refreshFingerUI();});GetGUI().AddWidget(&fingerPoseChoice);
        for(int i=0;i<5;i++){
            auto& slider=fingerAngles[i];float limit=i<3?120.f:i==3?60.f:90.f;
            slider.Create(-limit,limit,0,limit*4,"");slider.SetSize({162,18});slider.valueInputField.SetSize({66,24});
            slider.OnSlide([this,i](auto& e){
                if(!fingerGesture){rememberFit();fingerGesture=true;}pinFingerPose();
                auto& p=fingerFit.ensure(fingerKey(),fingerSide,fingerIndex);p.ik=false;
                if(i<3)p.curl[i]=e.fValue;else if(i==3)p.spread=e.fValue;else p.opposition=e.fValue;
            });GetGUI().AddWidget(&slider);
        }
        fingerIK.Create("Pin this fingertip");fingerIK.SetSize({22,22});fingerIK.OnClick([this](auto& e){setFingerPin(e.bValue);});GetGUI().AddWidget(&fingerIK);
        btn(fingerSelectTip,"Select fingertip / Move",711,[this]{selectFingertip(fingerSide,fingerIndex);});fingerSelectTip.SetSize({244,29});
        btn(fingerFrame,"Frame selected hand",748,[this]{auto& f=fingerFit.fingers[fingerSide][fingerIndex];if(!f.valid)return;XMStoreFloat3(&center,(arms.transforms.GetComponent(f.hand)->GetPositionV()+fingerFit.tip(f))*.5f);first=false;distance=.46f;});fingerFrame.SetSize({244,29});
        btn(fingerReset,"Reset this finger grip",785,[this]{rememberFit();fingerFit.poses[fingerKey()][fingerSide][fingerIndex]={};if(fitSelection>=6)selectWeapon(false);refreshFingerUI();});fingerReset.SetSize({244,29});
        btn(fingerResetHand,"Reset this hand grip",822,[this]{rememberFit();fingerFit.poses[fingerKey()][fingerSide]={};if(fitSelection>=6)selectWeapon(false);refreshFingerUI();});fingerResetHand.SetSize({244,29});
        fingerHint.Create("Finger help");fingerHint.SetSize({244,100});fingerHint.SetColor(wi::Color(31,35,38));GetGUI().AddWidget(&fingerHint);
        layoutFingerPanel();
    }
    void controlRing(XMFLOAT3 point,XMFLOAT4 color,float pixels=5){
        float radius=std::max(.0008f,wi::math::Distance(camera->Eye,point)*2*tanf(camera->fov*.5f)*pixels/GetLogicalHeight());
        auto inv=XMMatrixInverse(nullptr,XMLoadFloat4x4(&camera->View));auto right=XMVector3Normalize(inv.r[0]),up=XMVector3Normalize(inv.r[1]);
        for(int i=0;i<16;i++){float a=XM_2PI*i/16,b=XM_2PI*(i+1)/16;wi::renderer::RenderableLine line;
            line.start=ArmFit::f3(XMLoadFloat3(&point)+(right*cosf(a)+up*sinf(a))*radius);
            line.end=ArmFit::f3(XMLoadFloat3(&point)+(right*cosf(b)+up*sinf(b))*radius);line.color_start=line.color_end=color;wi::renderer::DrawLine(line);}
    }
    void drawFingerControls(){
        if(!fingerFit.enabled||!armFit.visible||editing)return;
        for(int side=0;side<2;side++)for(int n=0;n<5;n++){
            auto& f=fingerFit.fingers[side][n];if(!f.valid)continue;
            bool active=side==fingerSide&&(fitSelection>=6||fitSelection==side+1);
            if(!active)continue;
            for(int j=0;j<3;j++){
                int id=16+(side*5+n)*3+j;
                controlRing(arms.transforms.GetComponent(f.joints[j])->GetPosition(),id==fitSelection?XMFLOAT4{1,.55f,.05f,1}:XMFLOAT4{.15f,.8f,.95f,1},id==fitSelection?7:4);
            }
            controlRing(ArmFit::f3(fingerFit.tip(f)),fitSelection==6+side*5+n?XMFLOAT4{1,.55f,.05f,1}:XMFLOAT4{.3f,1,.65f,1},6);
        }
    }
    int pickFingertip(XMFLOAT4 pointer){
        if(!fingerFit.enabled||!armFit.visible)return 0;float nearest=10*10;int selected=0;
        for(int side=0;side<2;side++)for(int n=0;n<5;n++){
            auto& f=fingerFit.fingers[side][n];if(!f.valid)continue;
            if(!(side==fingerSide&&(fitSelection>=6||fitSelection==side+1)))continue;
            for(int j=0;j<4;j++){
                auto world=j==3?fingerFit.tip(f):arms.transforms.GetComponent(f.joints[j])->GetPositionV();
                auto p=ArmFit::f3(XMVector3Project(world,0,0,GetLogicalWidth(),GetLogicalHeight(),0,1,XMLoadFloat4x4(&camera->Projection),XMLoadFloat4x4(&camera->View),XMMatrixIdentity()));
                float d=(p.x-pointer.x)*(p.x-pointer.x)+(p.y-pointer.y)*(p.y-pointer.y);
                if(p.z>=0&&p.z<=1&&d<nearest){nearest=d;selected=j==3?6+side*5+n:16+(side*5+n)*3+j;}
            }
        }return selected;
    }
