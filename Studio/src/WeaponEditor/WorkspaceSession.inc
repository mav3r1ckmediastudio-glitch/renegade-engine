// Called between frames so Studio never renders or updates two workspaces at once.
static std::unique_ptr<Preview> active;
static wi::RenderPath* previousPath = nullptr;
static bool resume = false;
static fs::path lastProject;
struct WorkspaceSelection {
    fs::path project, model, preset, setup, arms;
    bool assembled=false;
};
static WorkspaceSelection pending;

void Choose(const fs::path& project, const fs::path& model,bool useAsArms,bool assembled)
{
    wchar_t exe[32768]{};
    GetModuleFileNameW(nullptr,exe,32768);
    home=fs::path(exe).parent_path()/"Content/WeaponEditor";
    editorWindow=GetActiveWindow();
    HMENU menu=CreatePopupMenu();
    if(!menu)return;
    AppendMenuW(menu,MF_STRING,1,L"Weapon only");
    AppendMenuW(menu,MF_STRING,6,L"Use assembled arms and weapon...");
    AppendMenuW(menu,MF_STRING,4,L"Use my arms model...");
    AppendMenuW(menu,MF_STRING,2,L"Open saved setup...");
    AppendMenuW(menu,MF_STRING,5,L"Import weapon prefab into this project...");
    if(active && lastProject==project)AppendMenuW(menu,MF_STRING,3,L"Resume last edit");
    std::vector<fs::path> presets;
    std::error_code error;
    for(const auto& item:fs::directory_iterator(home/"Presets",error)){
        const auto path=item.path()/"preset.json";
        if(fs::is_regular_file(path,error))presets.push_back(path);
    }
    std::sort(presets.begin(),presets.end());
    for(size_t i=0;i<presets.size();++i){
        auto title=presets[i].parent_path().filename().wstring();
        AppendMenuW(menu,MF_STRING,100+i,title.c_str());
    }
    POINT pos{};GetCursorPos(&pos);
    int choice=assembled?6:useAsArms?4:!model.empty()?1:TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,pos.x,pos.y,0,editorWindow,nullptr);
    DestroyMenu(menu);
    if(!choice)return;
    fs::path selectedPreset,setup,selectedArms;
    if(choice==5){
        auto file=choose(false,L"Weapon prefab\0*.weaponprefab\0\0",L"weaponprefab",project);if(file.empty())return;
        try{installWeaponPrefab(project,file);MessageBoxW(editorWindow,L"Weapon added to the Asset Library. Choose it under Player Start > Starting Equipment.",L"Weapon prefab",MB_OK);}
        catch(const std::exception& ex){MessageBoxA(editorWindow,ex.what(),"Import weapon prefab",MB_OK);}return;
    }
    if(choice==2){
        setup=choose(false,L"Weapon setup\0*.weapon\0\0",L"weapon",project);
        if(setup.empty())return;
        try{
            json doc;std::ifstream(setup)>>doc;
            if(doc.contains("armsModel"))selectedArms=(setup.parent_path()/fs::u8path(doc.at("armsModel").get<std::string>())).lexically_normal();
            if(doc.contains("localArmsPreset"))selectedPreset=(setup.parent_path()/fs::u8path(doc.at("localArmsPreset").get<std::string>())).lexically_normal();
            else if(doc.contains("armsPreset"))selectedPreset=home/fs::u8path(doc.at("armsPreset").get<std::string>());
            if(doc.contains("fitTransform") && selectedArms.empty() && selectedPreset.empty()){
                selectedArms=choose(false,L"Original arms rig for this setup\0*.glb;*.gltf;*.fbx\0\0",nullptr);
                if(selectedArms.empty())return;
            }
        }catch(const std::exception& e){MessageBoxA(editorWindow,e.what(),"Open weapon setup",MB_OK);return;}
    }else if(choice==6){
        selectedArms=model.empty()?choose(false,L"Assembled model or library asset\0*.rasset;*.fbx;*.glb;*.gltf\0\0",nullptr,project):model;
        if(selectedArms.empty())return;
    }else if(choice==4){
        selectedArms=useAsArms?model:choose(false,L"Arms model\0*.glb;*.gltf;*.fbx\0\0",nullptr);
        if(selectedArms.empty())return;
    }else if(choice>=100)selectedPreset=presets.at(choice-100);
    if(active && choice!=3 && active->unsaved()){
        int action=MessageBoxW(editorWindow,L"Save the previous weapon edit before opening another?",L"Weapon editor",MB_YESNOCANCEL);
        if(action==IDCANCEL)return;
        if(action==IDYES){
            auto dest=choose(true,L"Weapon setup\0*.weapon\0\0",L"weapon");if(dest.empty())return;
            try{active->setupFile(true,dest);}catch(const std::exception& e){MessageBoxA(editorWindow,e.what(),"Save weapon setup",MB_OK);return;}
        }
    }
    resume=choice==3;
    if(!resume)pending={project,useAsArms?fs::path{}:model,selectedPreset,setup,selectedArms,choice==6};
    enterRequested=true;
}

void Choose(const fs::path& project,const fs::path& model,bool useAsArms){Choose(project,model,useAsArms,false);}

bool Run(wi::Application& app)
{
    if(enterRequested){
        enterRequested=false;
        WorkspaceSelection last={projectRoot,requestedModel,presetFile,requestedSetup,requestedArms};
        try{
            if(!resume){
                projectRoot=pending.project;
                requestedModel=pending.model;
                presetFile=pending.preset;
                requestedSetup=pending.setup;
                requestedArms=pending.arms;
                auto next=std::make_unique<Preview>();
                next->assembled=pending.assembled;
                next->init(app.canvas);
                next->Load();
                active=std::move(next);
                lastProject=projectRoot;
            }
            previousPath=app.GetActivePath();
            app.ActivatePath(active.get());
        }catch(const std::exception& e){
            projectRoot=last.project;requestedModel=last.model;presetFile=last.preset;
            requestedSetup=last.setup;requestedArms=last.arms;
            MessageBoxA(app.window,e.what(),"Weapon editor",MB_OK|MB_ICONERROR);
        }
    }
    if(!active || app.GetActivePath()!=active.get())return false;
    if(leaveRequested){
        leaveRequested=false;
        active->pauseForFit();
        app.ActivatePath(previousPath);
        previousPath=nullptr;
        return false;
    }
    app.Run();
    return true;
}
bool Wheel(HWND window,WPARAM buttons,LPARAM position)
{
    if(!previousPath || !active)return false;
    active->onOrbitWheel(window,buttons,position);
    return true;
}
void Close(wi::Application& app)
{
    if(previousPath && active && app.GetActivePath()==active.get())app.ActivatePath(previousPath);
    previousPath=nullptr;
    active.reset();
}
} // namespace renegade::studio::weapons
