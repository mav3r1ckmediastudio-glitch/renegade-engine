    void startOriginalArms(){
        fingerFit.restoreBase();armFit.restoreBase();selectWeapon(false);
        armFit.profiles.clear();armFit.keys.clear();armFit.keyEditing=false;keyEdits.SetCheck(false);
        fingerFit.poses.clear();fingerOverrides.clear();fingerPoseIndex=0;fingerPoseChoice.SetSelectedWithoutCallback(0);
        fingerMode=false;fingerGesture=false;undoFits.clear();redoFits.clear();setupPath.clear();
        boltContact.clear();magazineOffsetValid=false;magazineMode=2;pickupPhase=.17f;seatPhase=.77f;
        setFullIK(false,false);armFit.visible=true;showIK.SetCheck(true);
        int idle=0;for(size_t i=0;i<arms.animations.GetCount();i++){auto n=arms.names.GetComponent(arms.animations.GetEntity(i));if(n&&n->name=="Idle"){idle=(int)i;break;}}
        selectClip(idle);auto& a=arms.animations[anim];a.Pause();a.timer=a.start;a.last_update_time=-1;play.SetText("Play");
        arms.Update(.000001f);armFit.phase=0;armFit.clip=currentClip();armFit.captureBase();fingerFit.captureBase();armFit.targets();armFit.refreshSkin();fingerFit.cacheVisible();
        frameArmsControls();cleanAdvancedOpen=false;layoutCleanUI();
        status.SetText("Fresh setup: original Idle pose at frame 0. No weapon, fitting, finger presets or IK keys loaded.");
    }
    
