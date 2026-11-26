// Native editor shell. Reuses the fitting, handoff, undo and serialization controls.
    wi::gui::Label cleanBackground[5],cleanObjects,cleanInspector,cleanDescription,cleanInstruction,cleanCameraHint,cleanTime,cleanGripInfo,cleanAdvancedLabel,cleanNote,cleanMarkerTicks[4];
    wi::gui::Button cleanItems[5],cleanAdvanced,cleanJoint[4],cleanElbow,cleanFrame,cleanEvents[4],placementPreview;
    wi::gui::ComboBox cleanAdvancedChoice;
    wi::gui::TextInputField cleanTimeInput;
    bool cleanAdvancedOpen=false,cleanHandleSelected=false,cleanDeferred=false;
    std::map<wi::gui::Widget*,bool> cleanVisibility;
    void cleanSetVisible(wi::gui::Widget& w,bool visible){if(cleanDeferred)cleanVisibility[&w]=visible;else if(w.IsVisible()!=visible)w.SetVisible(visible);}
    wi::gui::Button publishWeapon;
    void publishWeaponDialog();
    void bakePrefab(Scene& baked,json& recipe);
    void makeWeaponPrefab(const fs::path& dest);
    int cleanAdvancedPage=0;
    float cleanLeft() const {return 190;}
    float cleanRight() const {return 300;}
    float cleanTimelineY() const {return GetLogicalHeight()-(hasSupportGrip()?226:182);}
    static constexpr float viewportTop = 110.f;
    float viewportBottom() const { return cleanTimelineY() + 50.f; }
    void styleControl(wi::gui::Widget& widget, bool selected=false){
        const bool highlighted=widget.IsEnabled() && (selected || widget.GetState()!=wi::gui::IDLE);
        widget.SetColor(selected?theme::Selected:theme::Control,wi::gui::IDLE);
        widget.SetColor(theme::Focus,wi::gui::FOCUS);
        widget.SetColor(theme::Active,wi::gui::ACTIVE);
        widget.SetColor(theme::Focus,wi::gui::DEACTIVATING);
        widget.SetShadowRadius(1);
        widget.SetShadowColor(highlighted?theme::Accent:theme::Border);
        widget.font.params.bolden=.20f;
        widget.font.params.color=theme::Text;
        for(auto& sprite:widget.sprites){
            sprite.params.enableCornerRounding();
            for(auto& corner:sprite.params.corners_rounding)corner.radius=4;
        }
    }
    void cleanLabel(wi::gui::Label& label,const char* name){label.Create(name);label.SetText(name);label.SetColor(theme::Idle);label.SetShadowRadius(0);GetGUI().AddWidget(&label);}
    void createCleanBackgrounds(){for(auto& panel:cleanBackground){panel.Create("Panel");panel.SetText("");panel.SetEnabled(false);panel.SetColor(theme::Canvas);panel.SetShadowRadius(0);GetGUI().AddWidget(&panel);}}
    void cleanPlace(wi::gui::Widget& w,float x,float y,float width,float height=30){cleanSetVisible(w,true);w.SetPos({x,y});w.SetSize({width,height});if(dynamic_cast<wi::gui::Button*>(&w)||dynamic_cast<wi::gui::ComboBox*>(&w)||dynamic_cast<wi::gui::TextInputField*>(&w))styleControl(w);}
    void cleanButton(wi::gui::Button& b,const char* title,const char* help,std::function<void()> action){btn(b,title,0,action);b.SetTooltip(help);for(auto& sprite:b.sprites){sprite.params.enableCornerRounding();for(auto& corner:sprite.params.corners_rounding)corner.radius=5;}}
    Entity pickedRigBone(const PickResult& hit){
        auto object=arms.objects.GetComponent(hit.entity);if(!object)return 0;
        auto mesh=arms.meshes.GetComponent(object->meshID);if(!mesh)return 0;auto rig=arms.armatures.GetComponent(mesh->armatureID);if(!rig)return 0;
        std::map<Entity,float> weights;int vertices[]={hit.vertexID0,hit.vertexID1,hit.vertexID2};float bary[]={1-hit.bary.x-hit.bary.y,hit.bary.x,hit.bary.y};
        for(int v=0;v<3;v++){int id=vertices[v];if(id<0||id>=(int)mesh->vertex_boneindices.size())continue;
            for(int k=0;k<4;k++){auto bone=(&mesh->vertex_boneindices[id].x)[k];if(bone<rig->boneCollection.size())weights[rig->boneCollection[bone]]+=std::max(0.f,bary[v])*(&mesh->vertex_boneweights[id].x)[k];}}
        Entity bone=0;float best=0;for(auto& entry:weights)if(entry.second>best){best=entry.second;bone=entry.first;}return bone;
    }
    void selectHit(const PickResult& hit){
        Entity bone=pickedRigBone(hit);
        if(bone&&fullIKEnabled())for(int side=0;side<2;side++){
            for(int n=0;n<5;n++)for(int j=0;j<3;j++)if(bone==fingerFit.fingers[side][n].joints[j]){selectFingerJoint(side,n,j);return;}
            if(bone==armFit.chain[side].wrist){selectFit(side+1);return;}
        }
        if(bone&&bone==magazineBone){selectFit(5);return;}
        if(bone&&bone==boltContact.bone){pauseForFit();selectWeapon(false);cleanHandleSelected=true;return;}
        bool picked=hit.entity&&(hit.entity==instance||arms.Entity_IsDescendant(hit.entity,instance));selectWeapon(picked);
    }
    void frameCleanSelection(){
        if(editing){frameMarkerModel();return;}
        if(fitSelection>=6){auto& f=fingerFit.fingers[fingerSide][fingerIndex];if(!f.valid)return;XMStoreFloat3(&center,(arms.transforms.GetComponent(f.hand)->GetPositionV()+fingerFit.tip(f))*.5f);first=false;distance=.4f;}
        else if(fitSelection>=1&&fitSelection<=4)frameArmsControls();else if(!instance)frameArmsControls();else frameWeapon();
    }
    void cleanJumpEvent(int index){
        if(draggingFit||!arms.animations.GetCount())return;
        if(currentClip()!="Reload"){for(size_t i=0;i<arms.animations.GetCount();i++){auto n=arms.names.GetComponent(arms.animations.GetEntity(i));if(n&&n->name=="Reload"){selectClip((int)i);break;}}}
        pauseForFit();fingerOverrides.clear();auto& a=arms.animations[anim];float phases[]={pickupPhase,seatPhase,boltContact.grab,boltContact.release};a.timer=a.start+phases[index]*(a.end-a.start);a.last_update_time=-1;
        status.SetText("Playhead moved to the saved reload event. Use the part inspector to change its time.");
    }
    void createCleanUI(){
        cleanLabel(cleanObjects,"OBJECTS");cleanLabel(cleanInspector,"Weapon");cleanInspector.font.params.size=22;
        cleanLabel(cleanDescription,"Select a palm, joint or fingertip in the viewport.");
        cleanLabel(cleanInstruction,"Selection keeps the pose. Dragging edits it.");
        cleanLabel(cleanCameraHint,"RMB orbit  |  MMB pan  |  Wheel zoom  |  F frame selection");
        cleanTimeInput.Create("Exact time in seconds");cleanTimeInput.SetValue(0.f);cleanTimeInput.SetTooltip("Type an exact time in seconds, then Enter. This pauses and scrubs the animation.");
        cleanTimeInput.OnInputAccepted([this](auto& e){if(!arms.animations.GetCount()||!std::isfinite(e.fValue))return;pauseForFit();auto& a=arms.animations[anim];a.timer=std::clamp(e.fValue,a.start,a.end);a.last_update_time=-1;fingerOverrides.clear();});GetGUI().AddWidget(&cleanTimeInput);
        cleanLabel(cleanTime,"Time");cleanLabel(cleanGripInfo,"Automatic follows the reload contacts.");
        cleanLabel(cleanAdvancedLabel,"Advanced tools");cleanLabel(cleanNote,"Click to select. Drag to edit.\nSave setup keeps the complete fit.");
        const char* items[]={"Weapon","Magazine","Charging handle","Left hand","Right hand"};
        const char* hints[]={"Select the rifle. Move, rotate or scale it relative to the main hand.","Select the magazine without changing its current attachment. Drag to adjust its fit.","Inspect handle contact and release. Its movement follows the imported bolt rail.","Select the support palm. Fit the wrist, then its fingers. Selection keeps the current pose.","Select the main palm. The rifle follows this hand. Selection keeps the current pose."};
        for(int i=0;i<5;i++)cleanButton(cleanItems[i],items[i],hints[i],[this,i]{
            if(draggingFit)return;cleanAdvancedOpen=false;cleanHandleSelected=false;
            if(i==0)selectWeapon(true);else if(i==1)selectFit(5);else if(i==2){pauseForFit();selectWeapon(false);cleanHandleSelected=true;}else selectFit(i-2);
        });
        cleanButton(cleanAdvanced,"Advanced", "Show exact values, timed correction keys, diagnostics or initial model setup.",[this]{cleanAdvancedOpen=!cleanAdvancedOpen;});
        cleanAdvancedChoice.Create("Advanced section");cleanAdvancedChoice.SetText("");cleanAdvancedChoice.AddItem("Exact values and keys");cleanAdvancedChoice.AddItem("Mesh contact checks");cleanAdvancedChoice.AddItem("Initial weapon setup");cleanAdvancedChoice.AddItem("Finger curl and spread");cleanAdvancedChoice.OnSelect([this](auto& e){cleanAdvancedPage=e.iValue;});GetGUI().AddWidget(&cleanAdvancedChoice);cleanAdvancedChoice.SetTooltip("Choose which advanced controls to show in this inspector.");
        const char* joints[]={"Base","Middle","End joint","Tip IK"};
        for(int j=0;j<4;j++)cleanButton(cleanJoint[j],joints[j],j==3?"Select the fingertip Move target. Dragging starts IK; selection does not activate it.":"Select this finger joint. Drag a Rotate ring to adjust its angle; the other joints retain their angles.",[this,j]{cleanHandleSelected=false;if(j==3)selectFingertip(fingerSide,fingerIndex);else selectFingerJoint(fingerSide,fingerIndex,j);});
        cleanButton(cleanElbow,"Select elbow", "Select the elbow pole for this hand. Move it to steer the arm bend.",[this]{selectFit(3+fingerSide);});
        cleanButton(placementPreview,"Preview hand fit","Switch between positioning the weapon against the paused original arms and previewing its fitted hands.",[this]{
            if(!instance||draggingFit)return;
            if(fullIKEnabled())positionWeapon(true);
            else previewHandFit(true);
        });
        cleanButton(publishWeapon,"Save Weapon to Library...","Publish an equippable weapon and a portable copy for other projects.",[this]{publishWeaponDialog();});
        cleanButton(cleanFrame,"Frame selection  [F]", "Frame the selected weapon, arm or hand. This changes only the camera.",[this]{frameCleanSelection();});
        const char* events[]={"Grab magazine","Seat magazine","Grab handle","Let go"};
        for(int i=0;i<4;i++){cleanButton(cleanEvents[i],events[i],"Jump to this saved reload handoff. To move the event, use its weapon-part inspector.",[this,i]{cleanJumpEvent(i);});cleanLabel(cleanMarkerTicks[i],"|");cleanMarkerTicks[i].SetColor(wi::Color(168,80,31));}
        fingerChoice.OnSelect([this](auto& e){if(draggingFit)return;fingerIndex=e.iValue;selectFingerJoint(fingerSide,fingerIndex,0);});

        createSurfaces();cleanAdvanced.SetColor(wi::Color(40,46,55));createGripTimeline();createMarkerControls();createCasingPreviewUI();createFlashControls();createCleanBackgrounds();layoutCleanUI();
    }
    void layoutCleanUI(){
        cleanDeferred=true;cleanVisibility.clear();
        float width=GetLogicalWidth(),height=GetLogicalHeight(),left=cleanLeft(),right=cleanRight(),edge=width-right,x=edge+18,y=cleanTimelineY(),span=right-36;
        for(auto w:std::initializer_list<wi::gui::Widget*>{&clipLabel,&fitLabel,&scaleLabel,&scaleInput,&sizeLabel,&matchSize,&mainGrip,&support,&assemble,&open,&selectWeaponButton,&frameWeaponButton,&magazineLabel,&magazineControl,&armPanel,&applyIK,&showIK,&armReset,&armFrame,&pickupHere,&seatHere,&grabHere,&seatNow,&armHint,&eventLabel,&pickupLabel,&seatLabel,&pickupInput,&seatInput,&boltCaption,&boltGrabLabel,&boltReleaseLabel,&reloadStage,&boltGrabHere,&boltReleaseHere,&boltGrabInput,&boltReleaseInput,&fingerToggle,&fingerHandChoice,&fingerChoice,&fingerPoseChoice,&fingerEnabled,&fingerIK,&fingerSelectTip,&fingerFrame,&fingerReset,&fingerResetHand,&fingerHint,&poseCaption,&keyEdits,&addPoseKey,&deletePoseKey,&poseMoveLabel,&poseRotateLabel,&poseInfo,&seatHand,&checkContact,&scanContacts,&contactInfo,&placementPreview,&fullIKHint,&cleanGripInfo,&cleanAdvancedChoice,&cleanAdvancedLabel,&cleanElbow})cleanSetVisible(*w,false);
        for(auto& w:armSelect)cleanSetVisible(w,false);for(auto& w:fingerLabels)cleanSetVisible(w,false);for(auto& w:fingerAngles)cleanSetVisible(w,false);for(auto& w:poseNumbers)cleanSetVisible(w,false);for(auto& w:cleanJoint)cleanSetVisible(w,false);
        cleanPlace(cleanBackground[0],0,0,width,viewportTop);cleanPlace(cleanBackground[1],0,68,left,height-68);cleanPlace(cleanBackground[2],edge,68,right,height-68);cleanPlace(cleanBackground[3],left,viewportBottom(),edge-left,height-viewportBottom());cleanPlace(cleanBackground[4],0,height-30,width,30);
        cleanSetVisible(title,false);cleanPlace(backToStudio,18,17,185,32);title.SetText("RENEGADE / WEAPONS");title.font.params.size=16;
        cleanPlace(load,220,18,104,32);load.SetText("Open setup");cleanPlace(save,332,18,104,32);save.SetText("Save setup");
        for(int i=0;i<3;i++)cleanPlace(tools[i],460.f+i*109,18,103,32);
        cleanPlace(undoButton,802,18,72,32);cleanPlace(redoButton,882,18,72,32);
        cleanPlace(fullIKButton,width-130,18,112,32);
        cleanPlace(cleanObjects,18,88,left-36,24);cleanObjects.SetText("OBJECTS");
        for(int i=0;i<5;i++){cleanPlace(cleanItems[i],12,124.f+i*44,left-24,36);bool selected=i==0?selectedWeapon:i==1?fitSelection==5:i==2?cleanHandleSelected:i==3?fitSelection==1||fitSelection==3||(fitSelection>=6&&fingerSide==0):fitSelection==2||fitSelection==4||(fitSelection>=6&&fingerSide==1);cleanItems[i].SetEnabled(i<3||fullIKEnabled());styleControl(cleanItems[i],selected);}
        cleanPlace(view,12,370,left-24,34);view.SetText(first?"First-person / Orbit":"Orbit / First-person");
        cleanPlace(cleanFrame,12,414,left-24,34);
        cleanSetVisible(cleanNote,false);cleanPlace(publishWeapon,12,470,left-24,36);publishWeapon.SetEnabled((assembled || (instance && hand)) && !editing);
        const bool wideHeader=width>=1450.f;
        cleanPlace(cleanCameraHint,wideHeader?1096.f:left+18,wideHeader?19.f:70.f,wideHeader?width-1238.f:edge-left-36,30);
        cleanCameraHint.font.params.size=theme::CaptionSize;
        if(!wideHeader && hand)cleanSetVisible(cleanCameraHint,false);
        cleanPlace(cleanInspector,x,90,span,32);
        std::string name="Nothing selected",desc="Click the rifle or choose an object.\nSelect a hand to reveal finger controls.",instruction="Selection keeps the pose.\nDrag a handle to edit.";
        if(model.empty()){name="Original arms";desc="Import a weapon to begin a new setup.\nNo saved fitting is loaded.";instruction="The source animation is unchanged.\nIK is off until you choose to enable it.";}
        else if(editing){name="Weapon markers";desc="M / S grip markers. Green PSPs.\nPurple C marks casing ejection.";instruction="Drag a marker onto the model.\nRelease to place. Esc cancels.";}
        else if(flashPanel){name="Muzzle flash";desc="Choose the barrel and flash effect.";instruction="Scrub to the flash frame and set it.\nPlay to preview with casing ejection.";}
        else if(casingPanel){name="Casing ejection";desc="Choose the animation below and scrub to the ejection frame.";instruction="Set the frame, then play the clip.\nPreview now also works while paused.";}
        else if(selectedWeapon){
            name=fullIKEnabled()?"Hand fit preview":"Position weapon";
            desc=fullIKEnabled()?"The support hand follows the grip.":"Move, rotate or scale the weapon\nagainst the original hands.";
            instruction=fullIKEnabled()?"Choose Position weapon to keep\nboth hands still while placing it.":canBindVisibleGrip()?"Line up the rifle, then choose\nUse current grip to keep this pose.":"Preview the saved hand fitting.";
        }
        else if(cleanHandleSelected){name="Charging handle";desc="The handle moves along its bolt rail.\nSet when the hand grabs and lets go.";instruction="Scrub to contact, fit the palm,\nthen capture the visible grip.";}
        else if(fitSelection==5){name="Magazine";desc=magazineHeld?"Following the support hand.\nMove / Rotate adjusts its held fit.":"Seated in the rifle.\nMove / Rotate adjusts the seated fit.";instruction="Grab captures the current hand grip.\nSeat returns the magazine to the rifle.";}
        else if(fitSelection>=1&&fitSelection<=4){name=std::string(fingerSide?"Right ":"Left ")+(fitSelection<3?"hand":"elbow");desc=fitSelection<3?"Move positions the palm.\nRotate changes the wrist angle.":"Move the pole to steer the elbow.\nArm lengths stay fixed.";instruction="Fit the palm first, then the fingers.\nChoose a finger below or click a ring.";}
        else if(fitSelection>=6){name=std::string(fingerSide?"Right ":"Left ")+FingerFit::name(fingerIndex);if(fitSelection<16){name+=" tip";desc="Move the tip target to place the pad.\nThe finger bends to reach it.";instruction="Selecting does not pin the tip.\nDragging starts fingertip IK.";}else{int j=(fitSelection-16)%3;name+=j==0?" / base":j==1?" / middle":" / end";desc="Rotate this joint to shape the finger.\nThe other joint angles stay fixed.";instruction="Dragging switches this finger to\njoint rotation for the current grip.";}}
        cleanInspector.SetText(name);cleanPlace(cleanDescription,x,134,span,48);cleanDescription.SetText(desc);
        cleanPlace(cleanInstruction,x,194,span,48);cleanInstruction.SetText(instruction);
        cleanPlace(cleanAdvanced,x,y-60,span,32);cleanAdvanced.SetText(cleanAdvancedOpen?"Back to selected control":"Advanced...");
        if(cleanAdvancedOpen){
            cleanPlace(cleanAdvancedChoice,x,260,span,30);
            if(cleanAdvancedPage==0){
                cleanPlace(poseMoveLabel,x,308,span,24);for(int i=0;i<3;i++)cleanPlace(poseNumbers[i],x+i*90,338,82,26);
                cleanPlace(poseRotateLabel,x,378,span,24);for(int i=0;i<3;i++)cleanPlace(poseNumbers[i+3],x+i*90,408,82,26);
                cleanPlace(keyEdits,x+span-24,450,22,22);keyEdits.SetText("Key edits at this time");
                cleanPlace(addPoseKey,x,488,126,30);cleanPlace(deletePoseKey,x+138,488,126,30);
                cleanPlace(poseInfo,x,530,span,28);
                cleanPlace(armReset,x,570,span,30);cleanPlace(fingerReset,x,610,span,30);
            }else if(cleanAdvancedPage==1){
                cleanPlace(checkContact,x,312,span,34);cleanPlace(scanContacts,x,358,span,34);
                cleanPlace(contactInfo,x,412,span,106);cleanPlace(showIK,x+span-24,544,22,22);showIK.SetText("Show viewport controls");
                cleanPlace(armFrame,x,588,span,32);
            }else if(cleanAdvancedPage==3){
                for(int i=0;i<5;i++){cleanPlace(fingerLabels[i+3],x,310.f+i*70,span,24);cleanPlace(fingerAngles[i],x+6,342.f+i*70,176,18);fingerAngles[i].valueInputField.SetSize({70,24});}
                cleanPlace(fingerReset,x,682,span,30);
            }else{
                cleanPlace(open,x,310,span,32);cleanPlace(mainGrip,x,354,span,32);cleanPlace(support,x,398,span,32);cleanPlace(assemble,x,442,span,32);
                cleanPlace(scaleLabel,x,492,142,24);cleanPlace(scaleInput,x+156,492,108,26);cleanPlace(sizeLabel,x,534,span,26);cleanPlace(matchSize,x,574,span,32);
            }
        }else if(model.empty()){
            cleanPlace(open,x,270,span,36);open.SetText("Import weapon...");
        }else if(editing){
            cleanPlace(mainGrip,x,270,span,34);cleanPlace(support,x,316,span,34);cleanPlace(assemble,x,362,span,34);cleanPlace(open,x,418,span,34);
        }else if(selectedWeapon){
            cleanPlace(placementPreview,x,263,span,34);
            placementPreview.SetText(fullIKEnabled()?"Position weapon":canBindVisibleGrip()?"Use current grip":"Preview hand fit");
            cleanPlace(sizeLabel,x,321,span,28);cleanPlace(scaleLabel,x,367,142,24);cleanPlace(scaleInput,x+156,367,108,26);
        }else if(fitSelection==5){
            cleanPlace(magazineLabel,x,260,span,28);cleanPlace(magazineControl,x,300,span,30);
            cleanPlace(grabHere,x,352,span,32);cleanPlace(seatNow,x,396,span,32);cleanPlace(seatHand,x,452,span,32);
            cleanPlace(pickupLabel,x,506,128,26);cleanPlace(pickupInput,x+144,506,120,26);cleanPlace(seatLabel,x,546,128,26);cleanPlace(seatInput,x+144,546,120,26);
        }else if(cleanHandleSelected){
            cleanPlace(boltGrabHere,x,270,span,34);boltGrabHere.SetText("Grab handle at this time");cleanPlace(boltReleaseHere,x,316,span,34);boltReleaseHere.SetText("Let go at this time");
            cleanPlace(boltGrabLabel,x,374,128,26);cleanPlace(boltGrabInput,x+144,374,120,26);cleanPlace(boltReleaseLabel,x,414,128,26);cleanPlace(boltReleaseInput,x+144,414,120,26);
            cleanPlace(reloadStage,x,466,span,56);
        }else if(fitSelection>0){
            cleanPlace(fingerLabels[2],x,260,span,24);fingerLabels[2].SetText("Grip preview / edit");cleanPlace(fingerPoseChoice,x,292,span,30);
            cleanPlace(cleanGripInfo,x,332,span,54);cleanGripInfo.SetText(fingerPoseIndex==0?"Automatic follows the reload contacts.\nEdits affect the current grip.":fingerPoseIndex==1?"Rifle hold: normal weapon grip.\nPreviewing this hand shape.":fingerPoseIndex==2?"Magazine grip: used while carrying it.\nPreviewing this hand shape.":"Handle grip: used while pulling it.\nPreviewing this hand shape.");
            cleanPlace(fingerChoice,x,400,span,30);fingerChoice.SetSelectedWithoutCallback(fingerIndex);
            for(int j=0;j<4;j++)cleanPlace(cleanJoint[j],x+(j%2)*138,446.f+(j/2)*42,126,32);
            cleanPlace(cleanElbow,x,548,span,32);
            cleanPlace(fingerIK,x+span-24,598,22,22);fingerIK.SetText("Pin selected fingertip");
            if(fitSelection>=6){
                // Sliders are optional precise controls; direct joint rings are always available.
                cleanPlace(fingerReset,x,644,span,32);
            }
        }
        // Advanced actions still belong to the selected control.
        bool armSelected=fitSelection>=1&&fitSelection<=4,fingerSelected=fitSelection>=6;
        if(!armSelected)for(auto w:std::initializer_list<wi::gui::Widget*>{&keyEdits,&addPoseKey,&deletePoseKey,&poseInfo,&armReset})cleanSetVisible(*w,false);
        if(!fingerSelected)cleanSetVisible(fingerReset,false);
        if(cleanAdvancedOpen&&cleanAdvancedPage==0&&!armSelected&&!fingerSelected&&!selectedWeapon){
            cleanSetVisible(poseMoveLabel,false);cleanSetVisible(poseRotateLabel,false);for(auto& n:poseNumbers)cleanSetVisible(n,false);
            cleanPlace(cleanAdvancedLabel,x,316,span,70);cleanAdvancedLabel.SetText("Use this part's inspector for contacts.\nMagazine fitting uses Move / Rotate.");
        }
        if(cleanAdvancedOpen&&cleanAdvancedPage==3&&!armSelected&&!fingerSelected){
            for(auto& slider:fingerAngles)cleanSetVisible(slider,false);for(auto& label:fingerLabels)cleanSetVisible(label,false);cleanSetVisible(fingerReset,false);
            cleanPlace(cleanAdvancedLabel,x,316,span,56);cleanAdvancedLabel.SetText("Select a hand or finger to edit\nits curl and spread.");
        }
        bool joint=fitSelection>=16,tip=fitSelection>=6&&fitSelection<16;
        tools[0].SetEnabled(!joint&&!cleanHandleSelected);tools[1].SetEnabled(!tip&&fitSelection!=3&&fitSelection!=4&&!cleanHandleSelected);tools[2].SetEnabled(selectedWeapon);
        for(auto& b:cleanJoint)b.SetEnabled(fullIKEnabled());
        float tx=left+18,tw=edge-left-36;
        cleanPlace(play,tx,70,76,32);cleanPlace(clips,tx+90,70,180,32);cleanPlace(previousFrame,tx+286,70,72,32);cleanPlace(nextFrame,tx+366,70,72,32);previousFrame.SetText("< Frame");nextFrame.SetText("Frame >");
        cleanPlace(cleanTime,tx+460,70,std::max(180.f,tw-460),32);char time[128];if(arms.animations.GetCount()){auto& a=arms.animations[anim];snprintf(time,sizeof(time),"%.3f / %.3f s   |   %5.1f%%",a.timer,a.end,posePhase()*100);cleanTime.SetText(time);}
        cleanPlace(timeline,tx+8,y+70,tw-108,18);cleanSetVisible(timeline.valueInputField,false);cleanPlace(cleanTimeInput,edge-110,y+64,92,30);if(!GetGUI().IsTyping()&&arms.animations.GetCount())cleanTimeInput.SetValue(arms.animations[anim].timer);
        float phases[]={pickupPhase,seatPhase,boltContact.grab,boltContact.release};const char* labels[]={"Grab magazine","Seat magazine","Grab handle","Let go"};
        for(int i=0;i<4;i++){cleanPlace(cleanMarkerTicks[i],tx+8+phases[i]*(tw-108)-2,y+54,4,12);cleanMarkerTicks[i].SetText("");cleanSetVisible(cleanMarkerTicks[i],currentClip()=="Reload");cleanPlace(cleanEvents[i],tx+i*(tw+8)/4,y+108,(tw-24)/4,30);char event[96];snprintf(event,sizeof(event),"%s  %.1f%%",labels[i],phases[i]*100);cleanEvents[i].SetText(event);}
        layoutMarkerControls();layoutGripTimeline();layoutCasingPreviewUI(x,span,tx,tw,y);layoutFlashControls(x,span,tx,tw,y);
        cleanPlace(status,14,height-27,width-28,25);status.font.params.size=12;
        if(!hand&&!assembled){
            for(auto& item:cleanItems)cleanSetVisible(item,false);
            for(auto& event:cleanEvents)cleanSetVisible(event,false);
            for(auto w:std::initializer_list<wi::gui::Widget*>{&fullIKButton,&assemble,&view,&clips,&play,&timeline,&cleanTimeInput,&previousFrame,&nextFrame,&cleanTime,&cleanAdvanced,&cleanFrame,&cleanNote})
                cleanSetVisible(*w,false);
            cleanPlace(open,x,270,span,34);open.SetText("Open weapon model...");
            cleanInspector.SetText("Weapon markers");
            cleanDescription.SetText(model.empty()?"Open a model to place its markers.":model.filename().u8string());
            cleanInstruction.SetText("Choose a marker, then drag it onto the model. Save setup keeps its position.");
            if(model.empty()){
                cleanSetVisible(mainGrip,false);cleanSetVisible(support,false);
            }else{
                cleanPlace(cleanFrame,12,552,left-24,32);
            }
        }
        for(int i=0;i<3;i++)styleControl(tools[i],tool==i);
        styleControl(fullIKButton,fullIKEnabled());
        if(height<880){cleanSetVisible(cleanNote,false);}
        layoutSurfaces(x,span,width);layoutFiring(x,span,tx,tw,y);layoutMelee(x,span,tx,tw,y);styleControl(surfaceButton,surfaceOpen);
        if(assembled){
            for(auto& item:cleanItems)cleanSetVisible(item,false);
            for(auto& tool:tools)cleanSetVisible(tool,false);
            for(auto widget:std::initializer_list<wi::gui::Widget*>{&fullIKButton,&cleanAdvanced,&open,&mainGrip,&support})cleanSetVisible(*widget,false);
            for(auto& event:cleanEvents)cleanSetVisible(event,false);
            cleanObjects.SetText(editing?"WEAPON MARKERS":"ASSEMBLED WEAPON");
            if(editing){cleanPlace(assemble,x,362,span,34);assemble.SetText("Return to animation");}
            cleanSetVisible(actionRole,false);cleanSetVisible(actionSource,false);
            if(!editing && !flashPanel && !casingPanel && !surfaceOpen && !firingOpen){
                cleanPlace(actionRole,12,130,left-24,32);
                cleanPlace(actionSource,12,180,left-24,32);
                cleanInspector.SetText("Assembled weapon");
                cleanDescription.SetText("Choose a gameplay action and assign its source animation.");
                cleanInstruction.SetText("Preview the original clips using the animation menu.");
            }
        }
        cleanDeferred=false;for(auto& entry:cleanVisibility)cleanSetVisible(*entry.first,entry.second);
    }
