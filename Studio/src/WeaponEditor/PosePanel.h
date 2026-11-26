// Included in Preview. All pose changes are exposed as normal editor actions.
    wi::gui::Label poseCaption,poseMoveLabel,poseRotateLabel,poseInfo;
    wi::gui::CheckBox keyEdits;
    wi::gui::Button previousFrame,nextFrame,addPoseKey,deletePoseKey,seatHand;
    wi::gui::TextInputField poseNumbers[6];
    bool numericPoseEdit=false;
    void stepFrame(int direction){
        if(!arms.animations.GetCount())return;pauseForFit();auto& a=arms.animations[anim];a.timer=std::clamp(a.timer+direction/60.f,a.start,a.end);a.last_update_time=-1;
    }
    static XMFLOAT3 poseEuler(XMFLOAT4 q){return {XMConvertToDegrees(asinf(std::clamp(2*(q.w*q.x-q.y*q.z),-1.f,1.f))),XMConvertToDegrees(atan2f(2*(q.w*q.y+q.x*q.z),1-2*(q.x*q.x+q.y*q.y))),XMConvertToDegrees(atan2f(2*(q.w*q.z+q.x*q.y),1-2*(q.x*q.x+q.z*q.z)))};}
    bool editingBoltPose() const {float p=posePhase();return fitSelection==1&&currentClip()=="Reload"&&boltContact.captured&&p>=boltContact.grab&&p<=boltContact.release;}
    ArmFit::Settings shownArmPose(){
        int side=(fitSelection-1)%2;auto p=armFit.settings(side);
        if(fitSelection==1&&!editingBoltPose()&&activeGripWeight()>0){p.position=gripFit.offset;p.rotation=gripFit.rotation;return p;}
        if(editingBoltPose()){
            auto& c=armFit.chain[0];auto t=arms.transforms.GetComponent(c.target);XMVECTOR sc,q,pos;XMMatrixDecompose(&sc,&q,&pos,XMLoadFloat4x4(&c.handWorld));p.position=ArmFit::f3(t->GetPositionV()-pos);p.rotation=ArmFit::f4(XMQuaternionNormalize(XMQuaternionMultiply(XMQuaternionInverse(q),t->GetRotationV())));
        }
        return p;
    }
    void numericArmPose(int which,float value){
        if(!std::isfinite(value))return;
        if(selectedWeapon&&fitSelection==0){
            rememberFit();pauseForFit();auto t=arms.transforms.GetComponent(instance);auto parent=arms.transforms.GetComponent(hand);XMVECTOR sc,q,pos;XMMatrixDecompose(&sc,&q,&pos,XMLoadFloat4x4(&parent->world));auto scale=ArmFit::f3(sc);
            if(which<3)(&t->translation_local.x)[which]=value*.01f/(&scale.x)[which];else{auto e=poseEuler(t->rotation_local);(&e.x)[which-3]=value;t->rotation_local=ArmFit::f4(XMQuaternionRotationRollPitchYaw(XMConvertToRadians(e.x),XMConvertToRadians(e.y),XMConvertToRadians(e.z)));}t->SetDirty();hasFit=true;if(!armFit.enabled)gripFit.bindOnPreview=true;status.SetText("Weapon fit updated.");return;
        }
        if(fitSelection>=16&&which>=3){
            rememberFit();pauseForFit();int k=(fitSelection-16)/3,side=k/5,n=k%5,j=(fitSelection-16)%3;
            fingerFit.prepareVisiblePose(fingerKey(),side,n);fingerOverrides[k]=fingerKey();auto& p=fingerFit.poses[fingerKey()][side][n];auto e=poseEuler(p.anchor[j]);(&e.x)[which-3]=value;
            p.anchor[j]=ArmFit::f4(XMQuaternionRotationRollPitchYaw(XMConvertToRadians(e.x),XMConvertToRadians(e.y),XMConvertToRadians(e.z)));return;
        }
        if(fitSelection>=6&&fitSelection<16&&which<3){
            rememberFit();pinFingerPose();int side=(fitSelection-6)/5,n=(fitSelection-6)%5;auto& f=fingerFit.fingers[side][n];fingerFit.prepareVisiblePose(fingerKey(),side,n);fingerFit.enableIK(fingerKey(),side,n);auto& p=fingerFit.poses[fingerKey()][side][n];XMVECTOR sc,q,pos;XMMatrixDecompose(&sc,&q,&pos,fingerFit.handWorld(f));auto scale=ArmFit::f3(sc);(&p.target.x)[which]=value*.01f/(&scale.x)[which];status.SetText("Fingertip target updated in the hand frame.");return;
        }
        if(fitSelection<1||fitSelection>4)return;rememberFit();pauseForFit();armFit.phase=posePhase();
        int side=(fitSelection-1)%2;auto p=shownArmPose();
        if(which<3){auto& v=fitSelection<3?p.position:p.pole;(&v.x)[which]=value*.01f;}
        else{auto e=poseEuler(p.rotation);(&e.x)[which-3]=value;p.rotation=ArmFit::f4(XMQuaternionRotationRollPitchYaw(XMConvertToRadians(e.x),XMConvertToRadians(e.y),XMConvertToRadians(e.z)));}
        if(editingBoltPose()){
            auto& c=armFit.chain[0];auto t=arms.transforms.GetComponent(c.target);XMVECTOR sc,q,pos;XMMatrixDecompose(&sc,&q,&pos,XMLoadFloat4x4(&c.handWorld));t->ClearTransform();t->Rotate(XMQuaternionMultiply(q,XMLoadFloat4(&p.rotation)));t->Translate(pos+XMLoadFloat3(&p.position));t->UpdateTransform();boltContact.editTarget(armFit);
        }else if(fitSelection==1&&activeGripWeight()>0){gripFit.offset=p.position;gripFit.rotation=p.rotation;}else armFit.store(side,p);
        status.SetText(armFit.keyEditing?"IK key updated at this frame.":"IK offset updated.");
    }
    void alignMagazineSeat(){
        if(!magazineBone||!magazineOffsetValid){status.SetText("Capture a magazine grip first.");return;}
        rememberFit();pauseForFit();armFit.phase=posePhase();
        auto seat=magazineSeat.GetLocalMatrix()*XMLoadFloat4x4(&arms.transforms.GetComponent(magazineParent)->world);
        auto hand=XMMatrixInverse(nullptr,XMLoadFloat4x4(&magazineHandOffset))*seat;
        XMVECTOR sc,q,pos;XMMatrixDecompose(&sc,&q,&pos,hand);auto target=arms.transforms.GetComponent(armFit.chain[0].target);target->ClearTransform();target->Rotate(q);target->Translate(pos);target->UpdateTransform();armFit.edited(0);
        selectFit(1);status.SetText("Hand aligned using the captured magazine grip. Check reach, then seat.");
    }
    void createPosePanel(){
        poseCaption.Create("Pose keys");poseCaption.SetText("POSE KEYS / PRECISION");poseCaption.SetSize({232,24});poseCaption.SetColor(wi::Color(31,35,38));GetGUI().AddWidget(&poseCaption);
        keyEdits.Create("Key hand / elbow edits");keyEdits.SetSize({22,22});keyEdits.SetCheck(false);keyEdits.OnClick([this](auto& e){armFit.keyEditing=e.bValue;pauseForFit();});GetGUI().AddWidget(&keyEdits);
        btn(previousFrame,"< Frame",812,[this]{stepFrame(-1);});previousFrame.SetSize({104,27});
        btn(nextFrame,"Frame >",812,[this]{stepFrame(1);});nextFrame.SetSize({104,27});
        btn(addPoseKey,"Add IK key",871,[this]{if(fitSelection<1||fitSelection>4){status.SetText("Select a hand or elbow first.");return;}rememberFit();pauseForFit();armFit.phase=posePhase();armFit.addKey((fitSelection-1)%2);armFit.keyEditing=true;keyEdits.SetCheck(true);});addPoseKey.SetSize({104,27});
        btn(deletePoseKey,"Delete key",871,[this]{if(fitSelection<1||fitSelection>4)return;rememberFit();auto& k=armFit.keys[armFit.clip][(fitSelection-1)%2];auto it=k.lower_bound(posePhase()-.0001f);if(it!=k.end()&&std::abs(it->first-posePhase())<.0002f)k.erase(it);});deletePoseKey.SetSize({104,27});
        poseMoveLabel.Create("Offset cm");poseMoveLabel.SetText("Move offset cm   X / Y / Z");poseMoveLabel.SetSize({232,21});GetGUI().AddWidget(&poseMoveLabel);
        poseRotateLabel.Create("Wrist degrees");poseRotateLabel.SetText("Wrist degrees   X / Y / Z");poseRotateLabel.SetSize({232,21});GetGUI().AddWidget(&poseRotateLabel);
        for(int i=0;i<6;i++){poseNumbers[i].Create("Pose value");poseNumbers[i].SetSize({65,22});poseNumbers[i].SetValue(0.f);poseNumbers[i].OnInputAccepted([this,i](auto& e){numericArmPose(i,e.fValue);});GetGUI().AddWidget(&poseNumbers[i]);}
        poseInfo.Create("Pose info");poseInfo.SetSize({232,24});poseInfo.SetColor(wi::Color(31,35,38));GetGUI().AddWidget(&poseInfo);
        btn(seatHand,"Align hand / magazine seat",548,[this]{alignMagazineSeat();});seatHand.SetSize({244,29});
    }
    void layoutPosePanel(){
        float x=std::max(800.f,GetLogicalWidth()-262.f);seatHand.SetPos({x,550});seatHand.SetVisible(!fingerMode);
        armHint.SetSize({244,30});armHint.SetText("Pause a pose. Move / Rotate.\nUse IK keys to correct one moment.");
        previousFrame.SetPos({18,812});nextFrame.SetPos({128,812});poseCaption.SetPos({18,844});keyEdits.SetPos({228,875});addPoseKey.SetPos({18,905});deletePoseKey.SetPos({128,905});
        poseMoveLabel.SetPos({18,938});poseRotateLabel.SetPos({18,988});
        for(int i=0;i<3;i++){poseNumbers[i].SetPos({18.f+i*74,961});poseNumbers[i+3].SetPos({18.f+i*74,1011});}
        // Compact layout for a 1080p desktop, leaving the OS taskbar clear.
        poseCaption.SetPos({18,797});previousFrame.SetPos({18,824});nextFrame.SetPos({128,824});keyEdits.SetPos({228,856});addPoseKey.SetPos({18,881});deletePoseKey.SetPos({128,881});poseMoveLabel.SetPos({18,915});poseRotateLabel.SetPos({18,962});
        for(int i=0;i<3;i++){poseNumbers[i].SetPos({18.f+i*74,938});poseNumbers[i+3].SetPos({18.f+i*74,985});}
        poseInfo.SetPos({280,56});
        refreshPoseValues();
    }
    void refreshPoseValues(){
        if(GetGUI().IsTyping())return;
        bool selected=fitSelection>=1&&fitSelection<=4;
        for(int i=0;i<6;i++)poseNumbers[i].SetEnabled(selected&&(i<3||fitSelection<3));
        poseMoveLabel.SetText("Move offset cm   X / Y / Z");poseRotateLabel.SetText("Wrist degrees   X / Y / Z");
        if(fitSelection==1&&!editingBoltPose()&&activeGripWeight()>0){poseMoveLabel.SetText("Grip socket offset  X / Y / Z");poseRotateLabel.SetText("Grip rotation degrees  X / Y / Z");}
        if(selectedWeapon){
            auto t=arms.transforms.GetComponent(instance);XMVECTOR sc,q,pos;XMMatrixDecompose(&sc,&q,&pos,XMLoadFloat4x4(&arms.transforms.GetComponent(hand)->world));auto scale=ArmFit::f3(sc);auto e=poseEuler(t->rotation_local);
            for(int i=0;i<3;i++){poseNumbers[i].SetEnabled(true);poseNumbers[i+3].SetEnabled(true);poseNumbers[i].SetValue((&t->translation_local.x)[i]*(&scale.x)[i]*100);poseNumbers[i+3].SetValue((&e.x)[i]);}poseMoveLabel.SetText("Weapon local cm  X / Y / Z");poseRotateLabel.SetText("Weapon degrees  X / Y / Z");
        }else if(fitSelection>=16){
            int k=(fitSelection-16)/3,j=(fitSelection-16)%3;auto& f=fingerFit.fingers[k/5][k%5];auto q=f.hasVisible?f.visible[j].rotation_local:arms.transforms.GetComponent(f.joints[j])->rotation_local;auto e=poseEuler(q);
            for(int i=0;i<3;i++){poseNumbers[i].SetEnabled(false);poseNumbers[i+3].SetEnabled(true);poseNumbers[i+3].SetValue((&e.x)[i]);}poseMoveLabel.SetText("Finger joints keep fixed lengths");poseRotateLabel.SetText("Joint local degrees  X / Y / Z");
        }else if(fitSelection>=6&&fitSelection<16){
            int side=(fitSelection-6)/5,n=(fitSelection-6)%5;auto& f=fingerFit.fingers[side][n];FingerFit::Pose p;auto found=fingerFit.poses.find(fingerKey());if(found!=fingerFit.poses.end())p=found->second[side][n];if(f.hasVisible)p.target=ArmFit::f3(XMVector3TransformCoord(XMLoadFloat3(&f.visibleTip),XMMatrixInverse(nullptr,XMLoadFloat4x4(&f.visibleHand))));XMVECTOR sc,q,pos;XMMatrixDecompose(&sc,&q,&pos,fingerFit.handWorld(f));auto scale=ArmFit::f3(sc);for(int i=0;i<3;i++){poseNumbers[i].SetEnabled(true);poseNumbers[i+3].SetEnabled(false);poseNumbers[i].SetValue((&p.target.x)[i]*(&scale.x)[i]*100);}poseMoveLabel.SetText("Tip in hand cm  X / Y / Z");
        }else if(selected){auto p=shownArmPose();auto move=fitSelection<3?p.position:p.pole;auto rot=poseEuler(p.rotation);for(int i=0;i<3;i++){poseNumbers[i].SetValue((&move.x)[i]*100);poseNumbers[i+3].SetValue((&rot.x)[i]);}}
        int count=0;if(selected){auto it=armFit.keys.find(armFit.clip);if(it!=armFit.keys.end())count=(int)it->second[(fitSelection-1)%2].size();}
        char b[160];snprintf(b,sizeof(b),"%5.1f%% | %d IK keys",posePhase()*100,count);poseInfo.SetText(b);
    }
