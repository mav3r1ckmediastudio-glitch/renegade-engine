struct ModelPartInspector {
    StudioRenderPath& app;
    wi::gui::Label heading, detail;
    wi::gui::ComboBox parts;
    wi::ecs::Entity first = wi::ecs::INVALID_ENTITY;
    std::vector<wi::ecs::Entity> ids;
    explicit ModelPartInspector(StudioRenderPath& owner) : app(owner) {
        heading.Create("Part pivot");
        parts.Create("");
        parts.SetTooltip("Choose an imported part or click its mesh in the viewport.");
        detail.Create("");
        app.modelImportPanel_.AddWidget(&heading);
        app.modelImportPanel_.AddWidget(&parts);
        app.modelImportPanel_.AddWidget(&detail);
        show(false,0);
    }
    void show(bool visible,float right) {
        heading.SetVisible(visible);parts.SetVisible(visible);detail.SetVisible(visible);
        heading.SetPos({right,317});heading.SetSize({91,28});
        parts.SetPos({right+97,317});parts.SetSize({333,28});
        detail.SetPos({right,352});detail.SetSize({430,90});
    }
    void update() {
        if (!parts.IsVisible() || !app.modelImportPreview_ || !app.modelImportCandidate_) return;
        auto& preview=*app.modelImportPreview_;
        auto& scene=preview.InspectionScene();
        const auto* source = app.modelImportCandidate_->PeekMutableScene();
        if (!source) return;
        const size_t count = std::min(source->transforms.GetCount(), scene.transforms.GetCount());
        const auto key=count ? scene.transforms.GetEntity(0) : wi::ecs::INVALID_ENTITY;
        if (first!=key || ids.size()!=count) {
            first=key;
            ids.clear();
            for (size_t i=0; i<count; ++i) ids.push_back(scene.transforms.GetEntity(i));
            parts.ClearItems();
            for(size_t i=0;i<ids.size();++i) {
                auto* name=scene.names.GetComponent(ids[i]);
                parts.AddItem(name && !name->name.empty()? name->name : "Unnamed part "+std::to_string(i+1));
            }
            if(!ids.empty()) parts.SetSelectedWithoutCallback(0);
        }
        const auto pointer=wi::input::GetPointer();
        const auto pos=app.modelImportPreviewImage_.GetPos(),size=app.modelImportPreviewImage_.GetSize();
        if (wi::input::Press(wi::input::MOUSE_BUTTON_LEFT) && pointer.x>=pos.x && pointer.x<pos.x+size.x && pointer.y>=pos.y && pointer.y<pos.y+size.y) {
            const XMFLOAT3 point=app.UnprojectImportPoint({pointer.x,pointer.y,0.5f});
            const auto origin=XMLoadFloat3(&preview.InspectionCamera().Eye);
            const auto ray=wi::primitive::Ray(origin,XMVector3Normalize(XMLoadFloat3(&point)-origin));
            const auto hit=scene.Intersects(ray,wi::enums::FILTER_OPAQUE|wi::enums::FILTER_TRANSPARENT);
            const auto it=std::find(ids.begin(),ids.end(),hit.entity);
            if(it!=ids.end()) parts.SetSelectedWithoutCallback(int(it-ids.begin()));
        }
        const int selected=parts.GetSelected();
        if(selected<0 || size_t(selected)>=ids.size()){detail.SetText("No imported parts.");return;}
        const auto e=ids[selected];
        const auto* transform=scene.transforms.GetComponent(e);
        if(!transform)return;
        const auto* hierarchy=scene.hierarchy.GetComponent(e);
        const auto* parent=hierarchy?scene.names.GetComponent(hierarchy->parentID):nullptr;
        const auto local=transform->translation_local, world=transform->GetPosition();
        std::ostringstream out;
        out<<std::fixed<<std::setprecision(3)<<"Parent: "<<(hierarchy?(parent?parent->name:"Unnamed parent"):"Root")
           <<"\nLocal XYZ: "<<local.x<<"   "<<local.y<<"   "<<local.z
           <<"\nViewport XYZ (m): "<<world.x<<"   "<<world.y<<"   "<<world.z;
        detail.SetText(out.str());
    }
    void draw(wi::graphics::CommandList cmd) const {
        if(!parts.IsVisible() || !app.modelImportPreview_)return;
        const int index=parts.GetSelected();
        if(index<0 || size_t(index)>=ids.size())return;
        const auto& scene=app.modelImportPreview_->InspectionScene();
        const auto* tr=scene.transforms.GetComponent(ids[index]);
        if(!tr)return;
        const auto world=XMLoadFloat4x4(&tr->world);
        const auto origin=app.ProjectImportPoint(tr->GetPosition());
        const auto pos=app.modelImportPreviewImage_.GetPos(), size=app.modelImportPreviewImage_.GetSize();
        if(!(origin.z>=0 && origin.z<=1 && origin.x>=pos.x && origin.x<=pos.x+size.x && origin.y>=pos.y && origin.y<=pos.y+size.y))return;
        const float dpi=app.GetPhysicalWidth()/std::max(1.f,app.GetLogicalWidth());
        wi::graphics::Rect clip={long(pos.x*dpi),long(pos.y*dpi),long((pos.x+size.x)*dpi),long((pos.y+size.y)*dpi)};
        auto* device=wi::graphics::GetDevice();device->BindScissorRects(1,&clip,cmd);
        const float length=XMVectorGetX(XMVector3Length(world.r[3]-XMLoadFloat3(&app.modelImportPreview_->InspectionCamera().Eye))) * .07f;
        const wi::Color colours[]={wi::Color(255,105,82,255),wi::Color(111,224,116,255),wi::Color(110,178,255,255)};
        for(int i=0;i<3;++i) {
            if(XMVectorGetX(XMVector3LengthSq(world.r[i]))<1e-12f)continue;
            XMFLOAT3 point;XMStoreFloat3(&point,world.r[3]+length*XMVector3Normalize(world.r[i]));
            const auto end=app.ProjectImportPoint(point);
            if(!(end.z>=0 && end.z<=1))continue;
            const float dx=end.x-origin.x,dy=end.y-origin.y;
            wi::image::Params p;p.pos={(origin.x+end.x)*.5f,(origin.y+end.y)*.5f,0};p.siz={std::hypot(dx,dy),3};
            p.rotation=std::atan2(dy,dx);p.pivot={.5f,.5f};p.color=colours[i];wi::image::Draw(nullptr,p,cmd);
            wi::font::Params fp(int(end.x+2),int(end.y-7),13,wi::font::WIFALIGN_LEFT,wi::font::WIFALIGN_TOP,colours[i]);
            fp.shadowColor=wi::Color::Black();wi::font::Draw(i==0?"X":i==1?"Y":"Z",fp,cmd);
        }
        wi::image::Draw(nullptr,wi::image::Params(origin.x-3,origin.y-3,6,6,wi::Color::White()),cmd);
        const wi::graphics::Rect full={0,0,long(app.GetPhysicalWidth()),long(app.GetPhysicalHeight())};device->BindScissorRects(1,&full,cmd);
    }
};
