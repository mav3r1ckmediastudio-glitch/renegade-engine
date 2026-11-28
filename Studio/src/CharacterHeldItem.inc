// Rigid equipment is instanced beneath the selected joint in both preview and save.
struct HeldItem {
    using Scene=wi::scene::Scene;
    using Entity=wi::ecs::Entity;
    static constexpr Entity none=wi::ecs::INVALID_ENTITY;
    StudioRenderPath& app;
    wi::gui::Window panel;
    wi::gui::Button choose,remove,pick,move,rotate;
    wi::gui::ComboBox bone;
    wi::gui::CheckBox equipped;
    wi::gui::TextInputField fields[7];
    bridge::ModelImportCandidate item;
    std::string itemName;
    std::vector<std::pair<size_t,size_t>> joints;
    std::array<float,7> grip{0,0,0,0,0,0,1};
    wi::unordered_set<Entity> members;
    Scene* attachedScene=nullptr;
    Entity root=none;
    bool rotating=false,picking=false,dragging=false;
    XMFLOAT3 previous{};
    explicit HeldItem(StudioRenderPath& owner):app(owner) {
        panel.Create("Held item",wi::gui::Window::WindowControls::NONE);
        app.modelImportPanel_.AddWidget(&panel);
        auto add=[&](auto& w,const char* label){w.Create(label);panel.AddWidget(&w);};
        add(choose,"Choose held item...");add(remove,"Remove");add(bone,"Bone   ");add(pick,"Pick bone");
        add(move,"Move");add(rotate,"Rotate");add(equipped,"Equipped at spawn   ");equipped.SetCheck(true);
        const char* labels[]={"Pos X   ","Y   ","Z   ","Rot X   ","Y   ","Z   ","Scale   "};
        for(int i=0;i<7;++i){
            add(fields[i],labels[i]);fields[i].SetDescription(labels[i]);fields[i].SetFloatPrecision(4);
            fields[i].OnInputAccepted([this,i](const auto& a){
                if(std::isfinite(a.fValue)&&(i!=6||a.fValue>0))grip[i]=a.fValue;
                syncFields();applyPreview();
            });
        }
        choose.OnClick([this](auto){browse();});
        remove.OnClick([this](auto){detach();item={};choose.SetText("Choose held item...");});
        bone.OnSelect([this](auto){detach();});
        pick.OnClick([this](auto){picking=!picking;pick.SetText(picking?"Click joint":"Pick bone");});
        move.OnClick([this](auto){rotating=false;});
        rotate.OnClick([this](auto){rotating=true;});
        syncFields();panel.SetVisible(false);
    }
    void syncFields(){for(int i=0;i<7;++i)fields[i].SetValue(grip[i]);}
    void layout(float x,float w,float footer){
        panel.SetPos({x,279});panel.SetSize({w,std::max(85.f,std::min(293.f,footer-290))});
        auto box=[](auto& widget,float x,float y,float w){widget.SetPos({x,y});widget.SetSize({w,25});};
        float usable=w-24;
        box(choose,4,5,usable-78);box(remove,usable-69,5,69);
        box(bone,55,38,usable-154);box(pick,usable-90,38,90);
        box(move,4,72,usable*.5f-6);box(rotate,usable*.5f+4,72,usable*.5f-4);
        for(int i=0;i<7;++i)box(fields[i],62+(i%3)*usable/3,109+(i/3)*34,usable/3-68);
        box(equipped,160,218,22);
    }
    void show(bool visible){panel.SetVisible(visible);if(!visible){dragging=picking=false;pick.SetText("Pick bone");}}
    Entity joint(const Scene& scene,int index) const {
        if(index<0||size_t(index)>=joints.size())return none;
        auto [rig,j]=joints[index];
        if(rig>=scene.armatures.GetCount()||j>=scene.armatures[rig].boneCollection.size())return none;
        return scene.armatures[rig].boneCollection[j];
    }
    void reset(){
        members.clear();attachedScene=nullptr;root=none;item={};joints.clear();bone.ClearItems();
        grip={0,0,0,0,0,0,1};syncFields();equipped.SetCheck(true);choose.SetText("Choose held item...");
        auto& s=*app.modelImportCandidate_->PeekScene();int selected=-1;
        for(size_t a=0;a<s.armatures.GetCount();++a){
            const auto& bones=s.armatures[a].boneCollection;
            auto mapping=bridge::BuildAutoHumanoidMapping(s,bones).mapping;
            auto hand=mapping.bones[size_t(bridge::HumanoidBone::RightHand)];
            for(size_t j=0;j<bones.size();++j){
                auto* name=s.names.GetComponent(bones[j]);if(!name)continue;
                if(bones[j]==hand && selected<0)selected=int(joints.size());
                joints.emplace_back(a,j);bone.AddItem(name->name);
            }
        }
        bone.SetSelectedWithoutCallback(selected);show(false);
    }
    void browse(){
        const auto generation=app.modelImportGeneration_;
        wi::helper::FileDialogParams d;d.type=wi::helper::FileDialogParams::OPEN;d.description="Library asset or model";d.extensions={"rasset","glb","gltf","fbx","obj"};
        wi::helper::FileDialog(d,[this,generation](const std::string& path){
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this,generation,path](uint64_t){
                if(path.empty()||generation!=app.modelImportGeneration_||!panel.IsVisible()||!app.modelImportPanel_.IsVisible())return;
                bridge::ModelImportCandidate next;
                auto filename=fs::u8path(path);
                if(filename.extension()==".rasset"){
                    const auto& project=app.session_->Projects().CurrentProject();
                    bridge::CreatorAssetWorkflowService workflow;
                    bridge::AssetCatalogue catalogue;std::string error;
                    if(!workflow.BuildCatalogueSnapshot(project.rootPath,project.projectId,catalogue,error)){
                        app.ShowStudioMessageBox(error,"Held item");return;
                    }
                    const auto entry=std::find_if(catalogue.entries.begin(),catalogue.entries.end(),[&](const auto& e){
                        std::error_code ec;
                        return e.registered && e.model.known && fs::equivalent(filename,fs::u8path(project.rootPath)/fs::u8path(e.projectRelativePath),ec);
                    });
                    if(entry==catalogue.entries.end()){app.ShowStudioMessageBox("Choose a model from this project's Content folder.","Held item");return;}
                    auto prepared=workflow.PrepareModelPlacement(project.rootPath,project.projectId,entry->assetId);
                    if(!prepared.IsReady()){app.ShowStudioMessageBox(prepared.Result().error,"Held item");return;}
                    next.SetComposedScene(prepared.ReleaseScene());
                }else next=bridge::ModelImportCandidateService().PrepareStaticModel(path);
                if(!next.IsReady()){app.ShowStudioMessageBox(next.Error(),"Held item");return;}
                if(next.PeekScene()->animations.GetCount() || next.PeekScene()->armatures.GetCount() || !next.PeekScene()->objects.GetCount()){
                    app.ShowStudioMessageBox("Choose a rigid held item with a mesh.","Held item");return;
                }
                detach();item=std::move(next);itemName=filename.stem().u8string();
                grip={0,0,0,0,0,0,1};syncFields();
                choose.SetText(filename.filename().u8string());
            });
        });
    }
    void detach(){
        if(app.modelImportPreview_ && attachedScene==&app.modelImportPreview_->InspectionScene() && attachedScene->transforms.Contains(root)){
            for(auto e:members)attachedScene->Entity_Remove(e,false);
            app.modelImportPreview_->RefreshPose();
        }
        members.clear();root=none;attachedScene=nullptr;dragging=false;
    }
    void transform(Scene& s,Entity e)const {
        auto* t=s.transforms.GetComponent(e);if(!t)return;
        t->translation_local={grip[0],grip[1],grip[2]};
        XMStoreFloat4(&t->rotation_local,XMQuaternionRotationRollPitchYaw(XMConvertToRadians(grip[3]),XMConvertToRadians(grip[4]),XMConvertToRadians(grip[5])));
        t->scale_local={grip[6],grip[6],grip[6]};t->SetDirty();
    }
    Entity attach(Scene& s,Entity parent,bool preview){
        Scene instance;auto& src=*item.PeekMutableScene();auto e=instance.Instantiate(src,true);
        for(size_t i=0;i<src.materials.GetCount();++i)instance.materials[i]=src.materials[i];
        instance.names.Create(e).name="Held item / "+itemName;
        auto& md=instance.metadatas.Create(e);
        md.string_values.set("renegade.equipment.slot","held");
        md.string_values.set("renegade.equipment.atSpawn",equipped.GetCheck()?"true":"false");
        if(preview){members.clear();instance.FindAllEntities(members);}
        else if(!equipped.GetCheck())for(size_t i=0;i<instance.objects.GetCount();++i){instance.objects[i].SetRenderable(false);instance.objects[i].SetCastShadow(false);}
        s.Merge(instance);s.Component_Attach(e,parent,true);transform(s,e);return e;
    }
    void applyPreview(){
        if(!app.modelImportPreview_)return;
        auto& s=app.modelImportPreview_->InspectionScene();
        if(attachedScene==&s&&s.transforms.Contains(root)){transform(s,root);app.modelImportPreview_->RefreshPose();}
    }
    void update(){
        if(!item.IsReady()||!app.modelImportPreview_)return;
        if(app.importWorkflow_!=ImportWorkflow::Character){detach();return;}
        auto& s=app.modelImportPreview_->InspectionScene();auto parent=joint(s,bone.GetSelected());
        if(parent!=none&&(attachedScene!=&s||!s.transforms.Contains(root))){
            root=attach(s,parent,true);attachedScene=&s;app.modelImportPreview_->RefreshPose();
        }
        if(!panel.IsVisible())return;
        move.SetColor(rotating?theme::Control:theme::Selected,wi::gui::IDLE);
        rotate.SetColor(rotating?theme::Selected:theme::Control,wi::gui::IDLE);
        const auto pointer=wi::input::GetPointer();auto pos=app.modelImportPreviewImage_.GetPos(),size=app.modelImportPreviewImage_.GetSize();
        bool inside=pointer.x>=pos.x&&pointer.x<pos.x+size.x&&pointer.y>=pos.y&&pointer.y<pos.y+size.y;
        if(inside&&wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)&&picking){
            float best=18;int selected=-1;
            for(int i=0;i<int(joints.size());++i){auto* t=s.transforms.GetComponent(joint(s,i));if(!t)continue;
                auto p=app.ProjectImportPoint(t->GetPosition());float d=std::hypot(pointer.x-p.x,pointer.y-p.y);
                if(p.z>=0&&p.z<=1&&d<best){best=d;selected=i;}}
            if(selected>=0){bone.SetSelected(selected);picking=false;pick.SetText("Pick bone");}return;
        }
        auto* t=s.transforms.GetComponent(root);auto* hand=s.transforms.GetComponent(parent);if(!t||!hand)return;
        auto p=app.ProjectImportPoint(t->GetPosition());
        if(inside&&wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)&&std::hypot(pointer.x-p.x,pointer.y-p.y)<18){
            dragging=true;previous={pointer.x,pointer.y,p.z};
        }
        if(!wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))dragging=false;
        if(!dragging)return;
        if(rotating){grip[4]+=(pointer.x-previous.x)*.5f;grip[3]+=(pointer.y-previous.y)*.5f;}
        else {
            auto a=app.UnprojectImportPoint(previous),b=app.UnprojectImportPoint({pointer.x,pointer.y,previous.z});
            XMFLOAT3 delta;XMStoreFloat3(&delta,XMVector3TransformNormal(XMLoadFloat3(&b)-XMLoadFloat3(&a),XMMatrixInverse(nullptr,XMLoadFloat4x4(&hand->world))));
            grip[0]+=delta.x;grip[1]+=delta.y;grip[2]+=delta.z;
        }
        previous.x=pointer.x;previous.y=pointer.y;syncFields();applyPreview();
    }
    void draw(wi::graphics::CommandList cmd)const{
        if(!panel.IsVisible()||!app.modelImportPreview_||!item.IsReady())return;
        auto& scene=app.modelImportPreview_->InspectionScene();auto pos=app.modelImportPreviewImage_.GetPos(),size=app.modelImportPreviewImage_.GetSize();
        auto marker=[&](Entity e,float radius,wi::Color color){
            auto* t=scene.transforms.GetComponent(e);if(!t)return;
            auto p=app.ProjectImportPoint(t->GetPosition());
            if(p.z<0||p.z>1||p.x-radius<pos.x||p.y-radius<pos.y||p.x+radius>pos.x+size.x||p.y+radius>pos.y+size.y)return;
            wi::image::Draw(nullptr,wi::image::Params(p.x-radius,p.y-1,2*radius,2,color),cmd);
            wi::image::Draw(nullptr,wi::image::Params(p.x-1,p.y-radius,2,2*radius,color),cmd);
        };
        if(picking)for(int i=0;i<int(joints.size());++i)marker(joint(scene,i),3,theme::Text);
        marker(root,12,theme::Accent);
    }
    bool save(bridge::ModelImportCandidate& candidate,std::string& error){
        if(!item.IsReady())return true;
        auto copy=wi::allocator::make_shared<Scene>();auto& src=*candidate.PeekMutableScene();
        wi::Archive archive;src.Serialize(archive);archive.SetReadModeAndResetPos(true);copy->Serialize(archive);
        auto parent=joint(*copy,bone.GetSelected());if(parent==none){error="Choose the bone that holds this item.";return false;}
        for(size_t i=0;i<src.materials.GetCount();++i)copy->materials[i]=src.materials[i];
        attach(*copy,parent,false);
        for(auto d:item.Dependencies()){d.retainedRelativePath="equipment/"+d.retainedRelativePath;candidate.RetainMaterialInput(std::move(d));}
        candidate.SetComposedScene(std::move(copy));return true;
    }
};