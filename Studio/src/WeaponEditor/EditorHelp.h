// Included inside Preview. One master switch and contextual help.
    wi::gui::Button fullIKButton;
    wi::gui::Label fullIKHint;
    bool fullIKEnabled() const {return armFit.enabled&&fingerFit.enabled;}
    void setFullIK(bool enabled,bool undo=true){
        if(undo)rememberFit();
        if(!enabled)wristToBind.reset();
        armFit.enabled=fingerFit.enabled=enabled;
        applyIK.SetCheck(enabled);fingerEnabled.SetCheck(enabled);
        if(!enabled&&fitSelection>0)selectWeapon(false);
        status.SetText(enabled?"IK on. Fit the palm, then refine the finger grip.":"IK off. Previewing the source arm and finger pose.");
    }
    void layoutEditorHelp(){
        float x=std::max(800.f,GetLogicalWidth()-262.f);
        fullIKButton.SetPos({x,48});fullIKButton.SetText(fullIKEnabled()?"IK: ON":"IK: OFF");
        fullIKButton.SetColor(fullIKEnabled()?wi::Color(174,83,29):wi::Color(40,45,48));
        fullIKHint.SetPos({x,117});fullIKHint.SetText(fullIKEnabled()?"Fit the palm, then the fingers.\nChoose a grip preset to edit.":"IK is off: showing the source pose.\nTurn it on to apply your saved fitting.");
        applyIK.SetVisible(false);fingerEnabled.SetVisible(false);
        for(int i=0;i<4;i++)armSelect[i].SetEnabled(fullIKEnabled());
        armReset.SetEnabled(fullIKEnabled());seatHand.SetEnabled(fullIKEnabled());
        keyEdits.SetEnabled(fullIKEnabled());addPoseKey.SetEnabled(fullIKEnabled());deletePoseKey.SetEnabled(fullIKEnabled());
        fingerIK.SetEnabled(fullIKEnabled());fingerSelectTip.SetEnabled(fullIKEnabled());
        fingerReset.SetEnabled(fullIKEnabled());fingerResetHand.SetEnabled(fullIKEnabled());
    }
    void createEditorHelp(){
        fullIKButton.Create("IK: ON");fullIKButton.SetSize({110,29});
        fullIKButton.OnClick([this](auto&){if(fullIKEnabled())setFullIK(false);else previewHandFit(false);});GetGUI().AddWidget(&fullIKButton);
        fullIKHint.Create("IK help");fullIKHint.SetSize({244,31});fullIKHint.SetColor(wi::Color(31,35,38));GetGUI().AddWidget(&fullIKHint);
        auto help=[](wi::gui::Widget& widget,const char* text){widget.SetTooltip(text);};
        help(fullIKButton,"Turn hand, arm and finger fitting on or off together.\nOFF previews the source pose; your saved edits are kept.");
        help(fingerToggle,"Switch between hand/elbow controls and finger grips.\nThe IK switch applies to both panels.");
        help(tools[0],"Move the selected weapon, hand, elbow or fingertip.\nDrag a viewport axis or plane handle. Shortcut: W.");
        help(tools[1],"Rotate the selected weapon, wrist or held magazine.\nFingertip targets use Move. Shortcut: E.");
        help(tools[2],"Resize the selected weapon.\nIK keeps arm and finger lengths fixed. Shortcut: R.");
        help(undoButton,"Undo the last fitting change, including grip poses,\nIK keys and reload contacts. Shortcut: Ctrl+Z.");
        help(redoButton,"Redo a fitting change that you undid.\nShortcut: Ctrl+Y.");
        help(clips,"Choose the arms animation to preview and fit.\nSelect Reload to set magazine and handle contacts.");
        help(timeline,"Scrub the animation in seconds. Scrubbing pauses it.\nThe value at the right accepts an exact time.");
        help(timeline.valueInputField,"Type the exact animation time in seconds, then Enter.\nUse the frame buttons for small steps.");
        help(play,"Play or pause the animation.\nPlay returns finger grips to automatic reload switching.");
        help(view,"Switch between first-person and orbit views.\nOrbit: RMB rotates, MMB pans, wheel zooms. F frames the weapon.");
        help(open,"Open a replacement glTF weapon model.\nThis starts a new weapon fit; save the current setup first.");
        help(mainGrip,"Mark the weapon point that belongs in the main hand.\nClick the grip on the model, then preview it with the arms.");
        help(support,"Mark the support-hand contact on the weapon.\nThis helps initial alignment. Use hand IK for final palm fitting.");
        help(assemble,"Attach the weapon to the arms and preview the fit.\nThen select the weapon or a hand to refine its placement.");
        help(selectWeaponButton,"Select the whole weapon and show its outline and gizmo.\nUse W, E or R to move, rotate or scale it.");
        help(frameWeaponButton,"Centre and zoom the orbit view around the weapon.\nThis moves only the camera. Shortcut: F.");
        help(scaleInput,"Set the weapon's uniform scale.\nA value of 1 keeps its imported size.");
        help(matchSize,"Match the weapon's longest dimension to the preset's\nreference weapon, then inspect the fit.");
        help(save,"Save weapon placement, grip poses, IK keys and reload\ncontacts to a .weapon setup. Ctrl+S updates the current file.");
        help(load,"Open a saved weapon setup and restore its fitting.\nSave your current edits before opening another setup.");
        help(magazineControl,"Choose magazine ownership: in the rifle, following the\nsupport hand, or switching automatically during Reload.");
        help(armSelect[0],"Move the whole support hand. Rotate sets its wrist angle.\nFit the palm first, then refine the fingers. IK must be on.");
        help(armSelect[1],"Move or rotate the whole main hand.\nThe attached weapon moves with it. IK must be on.");
        help(armSelect[2],"Move the left elbow pole to steer the elbow's bend.\nThis changes the arm's route while keeping its lengths fixed.");
        help(armSelect[3],"Move the right elbow pole to steer the elbow's bend.\nUse it to keep the forearm clear of the weapon.");
        help(armSelect[4],"Move or rotate the magazine within the support hand.\nThis edits the held-magazine offset, not the rifle's position.");
        help(showIK,"Show or hide the hand and elbow markers.\nVisibility does not turn IK fitting on or off.");
        help(armFrame,"Frame both arms and their controls in orbit view.\nThis changes only the camera.");
        help(armReset,"Reset the selected hand or elbow correction.\nWith timed corrections, this resets the current key.");
        help(grabHere,"Capture the magazine's current position relative to the\nsupport hand. It will follow that hand through the reload.");
        help(seatNow,"Put the magazine back in the rifle.\nDuring Reload, also record this time as the seating event.");
        help(pickupInput,"Magazine pickup time as a percentage of Reload.\nThe magazine follows the support hand from pickup to seating.");
        help(seatInput,"Magazine seating time as a percentage of Reload.\nAfter seating, the hand can move to the charging handle.");
        help(pickupHere,"Use this paused Reload pose as magazine pickup.\nCapture the visible grip so the magazine follows the hand.");
        help(seatHere,"Use this paused Reload pose as magazine seating.\nThe magazine returns to the rifle and releases the hand.");
        help(boltGrabHere,"Capture the support-hand grip on the charging handle\nat this time. Seat the magazine before marking this contact.");
        help(boltReleaseHere,"Mark when the support hand lets go of the handle.\nThe bolt returns forward and the hand returns toward its normal hold.");
        help(boltGrabInput,"Handle-grab time as a percentage of Reload.\nIt must follow magazine seating and precede handle release.");
        help(boltReleaseInput,"Handle-release time as a percentage of Reload.\nIt must follow the grab and occur before the animation ends.");
        help(fingerHandChoice,"Choose the hand whose fingers you want to fit.\nHand IK positions the palm; this panel adjusts the fingers.");
        help(fingerChoice,"Choose a finger to curl, spread or pin with a tip target.\nEach finger can have a different fit for each grip preset.");
        help(fingerPoseChoice,"Foregrip: normal rifle hold. Magazine: reload grip.\nCharging handle: grip for the pull. Automatic switches during playback.\nEdits save separately for each selected hand and grip.");
        const char* angleHelp[]={
            "Bend the finger where it meets the palm.\nThis edits the selected hand and grip preset.",
            "Bend the finger's middle joint.\nAdjust it to wrap the finger around the object.",
            "Bend the last joint near the fingertip.\nUse small changes to place the finger pad.",
            "Move this finger sideways relative to its neighbours.\nUse it to improve spacing around the grip.",
            "Roll the thumb around toward the palm or away from it.\nCombine this with thumb curl and spread for a natural grip."
        };
        for(int i=0;i<5;i++){
            help(fingerAngles[i],angleHelp[i]);help(fingerAngles[i].valueInputField,angleHelp[i]);help(fingerLabels[i+3],angleHelp[i]);
        }
        help(fingerIK,"Pin this finger's tip to a movable target.\nThe joints bend to reach it. Unpin to use the curl sliders.");
        help(fingerSelectTip,"Select the fingertip and show its Move gizmo.\nDragging activates IK. Selection keeps the pose.");
        help(fingerFrame,"Centre and zoom the orbit view around the selected hand.\nThis changes only the camera.");
        help(fingerReset,"Remove this finger's edits from the selected grip preset.\nThe other fingers and grip presets are kept.");
        help(fingerResetHand,"Remove this hand's finger edits from the selected grip.\nThe other hand and grip presets are kept.");
        help(keyEdits,"Record hand and elbow changes at the current animation time.\nCorrections blend between keys. Existing keyed tracks stay keyed.");
        help(previousFrame,"Pause and move backward by 1/60 second.\nUse this to inspect a transition closely.");
        help(nextFrame,"Pause and move forward by 1/60 second.\nUse this to locate the first frame of a collision.");
        help(addPoseKey,"Save the selected hand/elbow correction at this time.\nThe first key also creates start/end anchors for smooth blending.");
        help(deletePoseKey,"Delete this hand's correction key at the current time.\nSelect a hand or elbow and scrub to its key first.");
        help(seatHand,"Move the support hand so its captured magazine grip\nlines up with the seated magazine. Check arm reach before seating.");
        for(int i=0;i<6;i++)help(poseNumbers[i],i<3?
            "Enter the selected control's X, Y or Z position in centimetres.\nThe row label shows whether it is an offset or a local position.":
            "Enter the selected weapon, wrist or finger joint's X, Y or Z rotation in degrees.\nFingertip targets and elbow poles use position controls.");
        help(checkContact,"Pause and mark where posed arm/finger triangles cross\nthe weapon. Red markers highlight the detected crossings.\nRun it again after editing to refresh the result.");
        help(scanContacts,"Check the complete Reload at 120 samples per second.\nThis diagnostic marks intersections and saves a report; it does not fix poses.\nClick again to stop.");
        help(poseInfo,"Current position in the animation and correction-key count\nfor the selected hand or elbow.");
        help(contactInfo,"Result of the latest mesh contact check.\nRed markers show detected surface crossings.");
    }
