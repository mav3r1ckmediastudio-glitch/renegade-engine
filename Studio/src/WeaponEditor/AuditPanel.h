    ContactAudit contacts;
    wi::gui::Button checkContact,scanContacts;
    wi::gui::Label contactInfo;
    bool checkPending=false,auditActive=false,auditWarmup=false;
    int auditFrame=0,auditMax=0;
    float auditRestoreTime=0;
    json auditFrames;
    void createAuditPanel(){
        btn(checkContact,"Check mesh contacts",15,[this]{pauseForFit();checkPending=true;});checkContact.SetSize({162,31});
        btn(scanContacts,"Scan reload / 120 fps",15,[this]{
            if(auditActive){auditActive=false;status.SetText("Contact scan stopped.");return;}
            if(currentClip()!="Reload"){status.SetText("Select Reload before scanning.");return;}
            pauseForFit();auditRestoreTime=arms.animations[anim].timer;fingerPoseIndex=0;fingerPoseChoice.SetSelectedWithoutCallback(0);selectWeapon(false);auditFrame=auditMax=0;auditFrames=json::array();auditActive=true;auditWarmup=true;
        });scanContacts.SetSize({174,31});
        contactInfo.Create("Mesh contacts");contactInfo.SetSize({570,24});contactInfo.SetColor(wi::Color(31,35,38));contactInfo.SetText("Mesh contacts: press Check to inspect the current pose.");GetGUI().AddWidget(&contactInfo);
    }
    void layoutAuditPanel(){checkContact.SetPos({812,15});scanContacts.SetPos({983,15});contactInfo.SetPos({530,56});}
    void auditBeginFrame(){if(auditActive){auto& a=arms.animations[anim];a.Pause();a.timer=std::min(a.end,a.start+auditFrame/120.f);a.last_update_time=-1;}}
    void auditEndFrame(){
        if(auditActive&&auditWarmup){auditWarmup=false;return;}
        if(checkPending||auditActive){
            bool single=checkPending;checkPending=false;contacts.check(arms,instance);if(single)std::ofstream(home/"mesh-contact-current.json")<<json{{"seconds",arms.animations[anim].timer},{"phase",posePhase()},{"intersectingTriangles",contacts.count},{"bones",contacts.bones}}.dump(2);
            std::string text="Intersecting arm triangles: "+std::to_string(contacts.count);int listed=0;for(auto& b:contacts.bones)if(listed++<3)text+=" | "+b.first+":"+std::to_string(b.second);contactInfo.SetText(text);
        }
        for(auto& p:contacts.points)wi::renderer::DrawPoint({p,.0018f,XMFLOAT4{1,.05f,.03f,1}});
        if(auditActive){
            auto& a=arms.animations[anim];auditFrames.push_back({{"seconds",a.timer},{"phase",posePhase()},{"intersectingTriangles",contacts.count},{"bones",contacts.bones}});auditMax=std::max(auditMax,contacts.count);
            status.SetText("Scanning reload: "+std::to_string(auditFrame)+" samples.\nMaximum crossed triangles: "+std::to_string(auditMax));
            if(a.timer>=a.end-.000001f){
                auditActive=false;std::ofstream(home/"reload-contact-audit.json")<<json{{"sampleRate",120},{"sampleCount",auditFrames.size()},{"maximumIntersectingArmTriangles",auditMax},{"frames",auditFrames}}.dump(2);
                status.SetText("Contact scan saved. Maximum: "+std::to_string(auditMax));a.timer=auditRestoreTime;a.last_update_time=-1;
            }else auditFrame++;
        }
    }
