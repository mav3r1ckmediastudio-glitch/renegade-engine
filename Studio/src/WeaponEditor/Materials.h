    wi::gui::Button surfaceButton,surfaceReset,surfaceSave;
    wi::gui::ComboBox surfaceChoice;
    wi::gui::Slider surfaceSliders[3];
    wi::gui::Label surfaceLabels[3];
    wi::gui::CheckBox useSurfaceMap;
    std::set<std::string> disabledSurfaceMaps;
    std::map<std::string,wi::scene::MaterialComponent::TextureMap> originalMaps;
    bool surfaceOpen=false;
    renegade::bridge::WeaponSurfaces surfaceOriginal,surfaceEdits;
    std::vector<std::string> surfaceNames;
    void visitSurfaces(const std::function<void(std::string,wi::scene::MaterialComponent&)>& visit){
        auto walk=[&](Scene& scene,Entity root,bool outside,const char* group){
            renegade::bridge::VisitWeaponMaterials(scene,root,outside,[&](std::string name,auto& m){visit(std::string(group)+name,m);});
        };
        walk(arms,instance,true,"Arms / ");
        walk(prop,0,false,assembled?"Arms / ":"Weapon / ");
        walk(placement,0,false,assembled?"Arms / ":"Weapon / ");
        if(instance)walk(arms,instance,false,"Weapon / ");
    }
    void applySurfaces(){
        visitSurfaces([&](std::string key,auto& mat){
            auto found=surfaceEdits.find(key);
            if(found!=surfaceEdits.end())renegade::bridge::SetWeaponSurface(mat,found->second);
            auto& slot=mat.textures[wi::scene::MaterialComponent::SURFACEMAP];
            if(disabledSurfaceMaps.count(key)){slot={};mat.SetDirty();}
            else if(originalMaps.count(key) && !slot.resource.IsValid()){slot=originalMaps.at(key);mat.SetDirty();}
        });
    }
    void refreshSurfaceValues(){
        int idx=surfaceChoice.GetSelected();
        if(idx<0 || idx>=int(surfaceNames.size()))return;
        auto& key=surfaceNames[idx];
        useSurfaceMap.SetCheck(!disabledSurfaceMaps.count(key));
        auto values=surfaceEdits.count(key)?surfaceEdits.at(key):surfaceOriginal.at(key);
        for(int i=0;i<3;++i)surfaceSliders[i].SetValue(values[i]);
    }
    void refreshSurfaceList(){
        surfaceChoice.ClearItems();surfaceNames.clear();
        visitSurfaces([&](std::string key,auto& mat){
            originalMaps.emplace(key,mat.textures[wi::scene::MaterialComponent::SURFACEMAP]);
            surfaceOriginal.emplace(key,renegade::bridge::WeaponSurface{mat.roughness,mat.metalness,mat.reflectance});
            if(std::find(surfaceNames.begin(),surfaceNames.end(),key)!=surfaceNames.end())return;
            surfaceNames.push_back(key);surfaceChoice.AddItem(assembled && key.rfind("Arms / ",0)==0 ? key.substr(7) : key);
        });
        if(!surfaceNames.empty())surfaceChoice.SetSelectedWithoutCallback(0);
        refreshSurfaceValues();
    }
    void createSurfaces(){
        cleanButton(surfaceButton,"Materials","Adjust the surface of the selected arms or weapon material.",[this]{surfaceOpen=!surfaceOpen;if(surfaceOpen)refreshSurfaceList();});
        surfaceChoice.Create("Material");surfaceChoice.OnSelect([this](auto&){refreshSurfaceValues();});GetGUI().AddWidget(&surfaceChoice);
        const char* names[]={"Roughness","Metalness","Reflectance"};
        const char* help[]={"Higher values broaden the highlight. The roughness texture still supplies detail.","Amount of metallic shading. Use zero for skin.","Strength of reflections on a non-metal surface."};
        for(int i=0;i<3;++i){
            cleanLabel(surfaceLabels[i],names[i]);auto& slider=surfaceSliders[i];
            slider.Create(0.f,1.f,0.f,1000,names[i]);slider.SetText("");slider.SetTooltip(help[i]);GetGUI().AddWidget(&slider);
            slider.OnSlide([this,i](auto& e){int idx=surfaceChoice.GetSelected();if(idx<0 || idx>=int(surfaceNames.size()))return;
                const auto& key=surfaceNames[idx];auto it=surfaceEdits.emplace(key,surfaceOriginal.at(key)).first;it->second[i]=e.fValue;applySurfaces();});
        }
        useSurfaceMap.Create("Use packed surface map");useSurfaceMap.SetTooltip("Use the imported texture for roughness and metalness. Turn off to use the sliders alone.");GetGUI().AddWidget(&useSurfaceMap);
        useSurfaceMap.OnClick([this](auto& e){auto i=surfaceChoice.GetSelected();if(i<0||i>=int(surfaceNames.size()))return;auto& key=surfaceNames[i];if(e.bValue)disabledSurfaceMaps.erase(key);else disabledSurfaceMaps.insert(key);surfaceEdits.emplace(key,surfaceOriginal.at(key));applySurfaces();});
        cleanButton(surfaceSave,"Save material changes","Save this setup with its current material settings.",[this]{try{setupFile(true,setupPath);}catch(const std::exception& e){status.SetText(e.what());}});
        cleanButton(surfaceReset,"Restore imported material","Restore the imported values for this material.",[this]{int idx=surfaceChoice.GetSelected();if(idx<0||idx>=int(surfaceNames.size()))return;
            auto key=surfaceNames[idx];disabledSurfaceMaps.erase(key);surfaceEdits[key]=surfaceOriginal.at(key);applySurfaces();surfaceEdits.erase(key);refreshSurfaceValues();});
    }
    void layoutSurfaces(float x,float span,float width){
        cleanPlace(surfaceButton,std::min(966.f,width-282),18,115,32);
        cleanSetVisible(useSurfaceMap,false);cleanSetVisible(surfaceSave,false);cleanSetVisible(surfaceChoice,false);cleanSetVisible(surfaceReset,false);
        for(int i=0;i<3;++i){cleanSetVisible(surfaceSliders[i],false);cleanSetVisible(surfaceLabels[i],false);}
        if(!surfaceOpen)return;
        for(auto& item:cleanVisibility)if(item.second && item.first->GetPos().x>=x-2 && item.first->GetPos().y>=75)item.second=false;
        cleanPlace(cleanInspector,x,90,span,32);cleanInspector.SetText("Materials");
        cleanPlace(surfaceChoice,x,144,span,32);
        for(int i=0;i<3;++i){cleanPlace(surfaceLabels[i],x,198.f+i*78,span,24);cleanPlace(surfaceSliders[i],x+3,233.f+i*78,span-86,19);surfaceSliders[i].valueInputField.SetSize({64,25});}
        cleanPlace(useSurfaceMap,x+span-25,435,22,22);cleanPlace(surfaceReset,x,479,span,32);cleanPlace(surfaceSave,x,525,span,32);
    }
