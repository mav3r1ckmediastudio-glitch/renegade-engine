// Native material values and texture resources live on the import candidate; the
// editor retains the separate and packed inputs so a mode switch is reversible.
struct StudioRenderPath::ImportMaterials {
    using Mat=wi::scene::MaterialComponent;
    using Map=Mat::TextureMap;
    enum Role { Base, Normal, Rough, Metal, AO, Emission, Packed, Extra, Count };
    inline static const char* labels[Count]={"Base colour","Normal","Roughness","Metalness","Ambient occlusion","Emissive","Packed surface","Other map"};
    inline static const char* values[6]={"Normal strength","Roughness","Metalness","Reflectance","Emissive intensity","Alpha cutoff"};
    inline static constexpr float maxima[6]={4,4,1,1,32,1};
    inline static constexpr int native[Count]={Mat::BASECOLORMAP,Mat::NORMALMAP,-1,-1,Mat::OCCLUSIONMAP,Mat::EMISSIVEMAP,Mat::SURFACEMAP,-1};
    struct State {
        Mat mat;
        Map inputs[Count];
        bool packed=false, edited=false;
        int channel[3]={0,1,2};
    };
    struct History { int index; State state; };
    StudioRenderPath& app;
    std::vector<State> states;
    std::vector<History> undo,redo;
    wi::gui::Window panel;
    wi::gui::Label thumbs[Count], names[Count], preview,caption;
    wi::gui::Button files[Count], clear[Count], back,forward,colour;
    wi::gui::ComboBox mode,alpha,channels[3],shader,extra;
    wi::gui::CheckBox sides;
    struct InspectorSlider : wi::gui::Slider {
        void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const override {
            if(!IsVisible())return;
            font.Draw(cmd);
            ApplyScissor(canvas,scissorRect,cmd);
            auto pos=GetPos(), size=GetSize();
            float y=pos.y+size.y*.5f, fraction=std::clamp((value-start)/std::max(.001f,end-start),0.f,1.f);
            wi::image::Draw(nullptr,wi::image::Params(pos.x,y-2,size.x,4,theme::Control),cmd);
            wi::image::Draw(nullptr,wi::image::Params(pos.x,y-2,size.x*fraction,4,theme::Accent),cmd);
            wi::image::Params thumb(pos.x+size.x*fraction,y,8,8,theme::Text);
            thumb.pivot={.5f,.5f};thumb.rotation=XM_PIDIV4;wi::image::Draw(nullptr,thumb,cmd);
            thumb.siz={4,4};thumb.color=theme::Accent;wi::image::Draw(nullptr,thumb,cmd);
            valueInputField.Render(canvas,cmd);
        }
    };
    InspectorSlider sliders[6];
    wi::gui::Label mapHeading,valueHeading;
    int drag=-1,hovered=-1;
    bool alphaView=false;
    float panelWidth=500;
    Map hoverMap;
    wi::Resource alphaImage;
    int index() const {return app.modelImportMaterial_.GetSelected();}
    bool valid() const {return app.modelImportCandidate_ && index()>=0 && size_t(index())<states.size();}
    State& current(){return states[index()];}
    static bool ok(const Map& m){return m.resource.IsValid() && m.resource.GetTexture().IsValid();}
    static std::string leaf(const Map& m){return fs::u8path(m.name).filename().u8string();}
    static Map load(const std::string& path){
        Map result; result.name=path; result.resource=wi::resourcemanager::Load(path,wi::resourcemanager::Flags::IMPORT_RETAIN_FILEDATA); return result;
    }
    static Map image(const wi::vector<uint8_t>& rgba,unsigned w,unsigned h,const char* name){
        Map result; wi::graphics::TextureDesc desc;desc.width=w;desc.height=h;desc.format=wi::graphics::Format::R8G8B8A8_UNORM;
        wi::vector<uint8_t> png;
        if(!wi::helper::saveTextureToMemoryFile(rgba,desc,"PNG",png))return result;
        result.name="__renegade_material/"+bridge::GenerateStableId()+"/"+name;
        result.resource=wi::resourcemanager::Load(result.name,wi::resourcemanager::Flags::IMPORT_RETAIN_FILEDATA,png.data(),png.size());
        return result;
    }
    struct Pixels {
        int w=0,h=0;
        std::unique_ptr<unsigned char,decltype(&stbi_image_free)> data{nullptr,stbi_image_free};
        bool read(const Map& m){
            if(!ok(m))return true;
            const auto& bytes=m.resource.GetFileData(); int n;
            if(!bytes.empty())data.reset(stbi_load_from_memory(bytes.data(),int(bytes.size()),&w,&h,&n,4));
            if(!data){wi::vector<uint8_t> png;
                if(!wi::helper::saveTextureToMemoryFile(m.resource.GetTexture(),"PNG",png))return false;
                data.reset(stbi_load_from_memory(png.data(),int(png.size()),&w,&h,&n,4));}
            return data!=nullptr;
        }
        uint8_t at(int x,int y,int width,int height,int c) const {
            return data ? data.get()[4*(size_t(y)*h/height*w+size_t(x)*w/width)+c] : 255;
        }
    };
    static bool compose(State& st){
        auto& surface=st.mat.textures[Mat::SURFACEMAP];
        {
            const int roles[3]={AO,Rough,Metal};
            Pixels pixels[3];int w=0,h=0;
            for(int j=0;j<(st.packed?1:3);++j){
                if(!pixels[j].read(st.inputs[st.packed?Packed:roles[j]]))return false;
                w=std::max(w,pixels[j].w);h=std::max(h,pixels[j].h);
            }
            if(!w)surface={};
            else {
                wi::vector<uint8_t> rgba(size_t(w)*h*4,255);
                for(int y=0;y<h;++y)for(int x=0;x<w;++x)for(int c=0;c<3;++c)
                    rgba[4*(size_t(y)*w+x)+c]=pixels[st.packed?0:c].at(x,y,w,h,st.packed?st.channel[c]:0);
                auto cooked=image(rgba,w,h,"Surface_ORM.png");if(!ok(cooked))return false;
                cooked.uvset=surface.uvset;surface=std::move(cooked);
            }
        }
        st.mat.SetOcclusionEnabled_Primary(st.packed?ok(st.inputs[Packed]):ok(st.inputs[AO]));
        st.mat.SetOcclusionEnabled_Secondary(false);st.mat.textures[Mat::OCCLUSIONMAP]={};
        return true;
    }
    // Recognise only an exact material-name family beside the model. Ambiguous
    // matches stay empty for the artist to resolve.
    void discover(State& s,const std::string& name) {
        if(name.empty()||s.packed)return;
        std::map<std::string,std::vector<fs::path>> images;
        const auto dir=fs::u8path(app.modelImportCandidate_->SourcePath()).parent_path();
        auto lower=[](std::string v){for(auto& c:v)c=char(std::tolower(static_cast<unsigned char>(c)));return v;};
        for(auto folder:{dir,dir/"textures"}){
            std::error_code ec;
            for(fs::directory_iterator it(folder,ec),end;!ec&&it!=end;it.increment(ec)){
                const auto p=it->path();auto ext=lower(p.extension().u8string());
                if(ext==".png"||ext==".jpg"||ext==".jpeg"||ext==".dds"||ext==".tga"||ext==".bmp")
                    images[lower(p.stem().u8string())].push_back(p);
            }
        }
        for(const auto& pair:std::array<std::pair<int,const char*>,10>{{{Base,"albedo"},{Base,"basecolor"},{Normal,"normal"},{Rough,"roughness"},{Metal,"metallic"},{Metal,"metalness"},{AO,"ao"},{Emission,"emissive"},{Packed,"orm"},{Extra,"opacity"}}}){
            auto& m=pair.first==Extra?s.mat.textures[Mat::TRANSPARENCYMAP]:s.inputs[pair.first];
            if(ok(m))continue;
            auto found=images.find(lower(name)+"_"+pair.second);
            if(found==images.end()||found->second.size()!=1)continue;
            auto input=load(found->second.front().u8string());if(!ok(input))continue;
            m=input;s.edited=true;
            if(native[pair.first]>=0)s.mat.textures[native[pair.first]]=input;
        }
        if(!s.edited)return;
        s.packed=ok(s.inputs[Packed]);
        if(ok(s.inputs[Rough])||s.packed)s.mat.roughness=1;
        if(ok(s.inputs[Metal])||s.packed)s.mat.metalness=1;
        if(!compose(s))app.ShowStudioMessageBox("Surface inputs could not be combined. Review the material maps.","Material maps");
    }
    void reset(){
        states.clear();undo.clear();redo.clear();hovered=drag=-1;hoverMap={};alphaImage={};
        auto& scene=*app.modelImportCandidate_->PeekMutableScene();
        for(size_t i=0;i<scene.materials.GetCount();++i){
            auto& s=states.emplace_back();s.mat=scene.materials[i];
            for(int r=0;r<Count;++r)if(native[r]>=0)s.inputs[r]=s.mat.textures[native[r]];
            s.packed=ok(s.inputs[Packed]);
            auto path=fs::u8path(s.inputs[Packed].name);
            for(const auto& d:app.modelImportCandidate_->Dependencies())
                if(d.previewResourceName==s.inputs[Packed].name && !d.sourcePath.empty())path=fs::u8path(d.sourcePath);
            auto stem=path.stem().u8string(); auto lower=stem;
            std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});
            auto sep=lower.find_last_of("_-. ");
            if(!s.packed){
                auto* name=scene.names.GetComponent(scene.materials.GetEntity(i));
                if(name)discover(s,name->name);
                if(s.edited){scene.materials[i]=s.mat;scene.materials[i].SetDirty();}
                continue;
            }
            if(sep==std::string::npos)continue;
            auto token=lower.substr(sep+1);
            int role=token=="metallic"||token=="metalness"?Metal:token=="roughness"?Rough:-1;
            if(role<0)continue;
            s.inputs[role]=s.inputs[Packed];s.inputs[Packed]={};s.packed=false;
            for(const auto& pair:std::array<std::pair<int,const char*>,4>{{{Rough,"roughness"},{Metal,"metallic"},{Metal,"metalness"},{AO,"AO"}}}){
                if(ok(s.inputs[pair.first]))continue;
                auto sibling=path.parent_path()/fs::u8path(stem.substr(0,sep+1)+pair.second+path.extension().u8string());
                std::error_code ec;if(fs::is_regular_file(sibling,ec))s.inputs[pair.first]=load(sibling.u8string());
            }
            if(ok(s.inputs[Rough]))s.mat.roughness=1;
            if(ok(s.inputs[Metal]))s.mat.metalness=1;
            if(compose(s)){s.edited=true;scene.materials[i]=s.mat;scene.materials[i].SetDirty();}
            else {s.mat=scene.materials[i];s.inputs[Packed]=s.mat.textures[Mat::SURFACEMAP];s.packed=true;}
        }
        if(app.modelImportPreview_)app.modelImportPreview_->RefreshMaterials(scene);
    }
    void apply(int i){
        auto& scene=*app.modelImportCandidate_->PeekMutableScene();
        scene.materials[i]=states[i].mat;scene.materials[i].SetDirty();
        app.modelImportPreview_->RefreshMaterials(scene);refresh();
    }
    void record(){if(undo.size()==64)undo.erase(undo.begin());undo.push_back({index(),current()});redo.clear();}
    void history(bool reverse){
        auto& from=reverse?redo:undo;auto& to=reverse?undo:redo;if(from.empty())return;
        auto h=std::move(from.back());from.pop_back();to.push_back({h.index,states[h.index]});
        states[h.index]=std::move(h.state);app.modelImportMaterial_.SetSelectedWithoutCallback(h.index);apply(h.index);
    }
    template<class F> void edit(F change,bool surface=false){
        if(!valid())return;auto next=current();change(next);
        if(surface && !compose(next)){app.ShowStudioMessageBox("The surface image could not be decoded or packed.","Material maps");refresh();return;}
        record();current()=std::move(next);current().edited=true;apply(index());
    }
    int extraSlot()const{return int(extra.GetSelectedUserdata());}
    Map& map(State& s,int r){return r==Extra?s.mat.textures[extraSlot()]:s.inputs[r];}
    void assign(int r,const Map& m){
        const bool first=!ok(map(current(),r));
        edit([&](State& s){map(s,r)=m;
            if(first && ok(m) && r==Rough)s.mat.roughness=1;
            if(native[r]>=0 && r!=Packed && r!=AO)s.mat.textures[native[r]]=m;
            if(ok(m) && r==Normal && s.mat.normalMapStrength==0)s.mat.normalMapStrength=1;
            if(ok(m) && (r==Metal||r==Packed) && s.mat.metalness==0)s.mat.metalness=1;
        },r==AO||r==Metal||r==Rough||r==Packed);
    }
    void browse(int r){
        if(!valid())return;
        const auto gen=app.modelImportGeneration_;int i=index(),slot=extraSlot();
        wi::helper::FileDialogParams p;p.type=wi::helper::FileDialogParams::OPEN;p.description=labels[r];p.extensions={"png","jpg","jpeg","tga","dds","bmp","webp"};
        wi::helper::FileDialog(p,[this,gen,i,r,slot](const std::string& path){wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[=](uint64_t){
            if(path.empty()||!valid()||gen!=app.modelImportGeneration_||index()!=i||!panel.IsVisible()||!app.modelImportPanel_.IsVisible()||(r==Extra&&extraSlot()!=slot))return;
            auto m=load(path);if(ok(m))assign(r,m);else app.ShowStudioMessageBox("Could not read the selected texture.","Material maps");
        });});
    }

    explicit ImportMaterials(StudioRenderPath& owner):app(owner){
        panel.Create("Material properties",wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        app.modelImportPanel_.AddWidget(&panel);
        auto add=[&](auto& widget,const char* text){widget.Create(text);panel.AddWidget(&widget);};
        add(mapHeading,"TEXTURE MAPS");add(valueHeading,"MATERIAL VALUES");
        add(mode,"Surface maps");mode.AddItem("Separate maps");mode.AddItem("Packed surface map");
        mode.OnSelect([this](auto a){edit([&](State& s){s.packed=a.iValue!=0;},true);});
        add(shader,"Shader");
        for(auto name:{"Standard PBR","Planar reflection","Parallax","Anisotropic","Water","Cartoon","Unlit","Cloth","Clearcoat","Cloth + clearcoat","Terrain blended","Interior mapping"})shader.AddItem(name);
        shader.OnSelect([this](auto a){edit([&](State& s){s.mat.customShaderID=-1;s.mat.shaderType=Mat::SHADERTYPE(a.iValue);});});
        add(extra,"Other map");
        for(auto item:std::array<std::pair<int,const char*>,9>{{{Mat::TRANSPARENCYMAP,"Opacity"},{Mat::DISPLACEMENTMAP,"Height"},{Mat::TRANSMISSIONMAP,"Transmission"},{Mat::SHEENCOLORMAP,"Sheen colour"},{Mat::SHEENROUGHNESSMAP,"Sheen roughness"},{Mat::CLEARCOATMAP,"Clearcoat"},{Mat::CLEARCOATROUGHNESSMAP,"Coat roughness"},{Mat::CLEARCOATNORMALMAP,"Coat normal"},{Mat::SPECULARMAP,"Specular"}}})extra.AddItem(item.second,item.first);
        extra.AddItem("Anisotropy",Mat::ANISOTROPYMAP);extra.SetSelectedWithoutCallback(0);extra.SetText("");
        extra.OnSelect([this](auto){refresh();});
        for(int r=0;r<Count;++r){
            add(thumbs[r],"");add(names[r],labels[r]);add(files[r],"Choose map...");add(clear[r],"x");
            files[r].OnClick([this,r](auto){browse(r);});clear[r].OnClick([this,r](auto){assign(r,{});});
            clear[r].SetTooltip("Clear this map");
        }
        for(int c=0;c<3;++c){
            add(channels[c],c==0?"AO":c==1?"Rough":"Metal");
            for(auto l:{"R","G","B","A"})channels[c].AddItem(l);
            channels[c].OnSelect([this,c](auto a){edit([&](State& s){s.channel[c]=a.iValue;},true);});
        }
        add(colour,"Thumbnail preview: Colour");colour.OnClick([this](auto){alphaView=!alphaView;hovered=-1;refresh();});
        add(alpha,"Alpha mode");for(auto l:{"Opaque","Cutout","Blended"})alpha.AddItem(l);
        alpha.OnSelect([this](auto a){edit([&](State& s){s.mat.userBlendMode=a.iValue==2?wi::enums::BLENDMODE_ALPHA:wi::enums::BLENDMODE_OPAQUE;s.mat.alphaRef=a.iValue==1?.5f:1.f;});});
        add(sides,"Double-sided");sides.OnClick([this](auto a){edit([&](State& s){s.mat.SetDoubleSided(a.bValue);});});
        for(int i=0;i<6;++i){
            sliders[i].Create(0,maxima[i],0,1000,values[i]);panel.AddWidget(&sliders[i]);
            sliders[i].valueInputField.SetFloatPrecision(3);
            sliders[i].OnSlide([this,i](auto a){
                if(!valid()||!std::isfinite(a.fValue))return;
                bool held=wi::input::Down(wi::input::MOUSE_BUTTON_LEFT);
                if(!held||drag!=i)record();drag=held?i:-1;
                auto& s=current();float v=std::clamp(a.fValue,0.f,maxima[i]);auto& m=s.mat;
                switch(i){case 0:m.normalMapStrength=v;break;case 1:m.roughness=v;break;case 2:m.metalness=v;break;
                    case 3:m.reflectance=v;break;case 4:m.SetEmissiveStrength(v);
                    if(v>0&&m.emissiveColor.x==0&&m.emissiveColor.y==0&&m.emissiveColor.z==0)m.emissiveColor={1,1,1,v};break;
                    case 5:m.alphaRef=1-std::max(1.f/256,v);break;}
                s.edited=true;apply(index());
            });
        }
        add(back,"Undo");add(forward,"Redo");
        back.OnClick([this](auto){history(false);});forward.OnClick([this](auto){history(true);});
        preview.Create("");caption.Create("");
        show(false);
    }
    void layout(float right,float width,float footer){
        panel.SetPos({right,168});panel.SetSize({width,std::max(150.f,footer-180)});
        panelWidth=width;arrange();
    }
    void arrange(){
        const float w=panelWidth-22;float y=3;bool packed=valid()&&current().packed;
        auto box=[](auto& widget,float x,float y,float w,float h){widget.SetPos({x,y});widget.SetSize({w,h});};
        box(shader,110,y,w-110,25);y+=29;box(mapHeading,3,y,w-3,23);y+=26;box(mode,110,y,w-110,25);y+=32;
        for(int r=0;r<Count;++r){
            bool on=r==Packed?packed:r==Rough||r==Metal||r==AO?!packed:true;
            thumbs[r].SetVisible(on);names[r].SetVisible(on&&r!=Extra);files[r].SetVisible(on);clear[r].SetVisible(on);
            if(!on)continue;
            box(thumbs[r],3,y,31,31);box(names[r],40,y,119,31);box(files[r],162,y,w-194,31);box(clear[r],w-29,y,28,31);
            if(r==Extra)box(extra,40,y,119,31);y+=35;
        }
        for(int c=0;c<3;++c){channels[c].SetVisible(packed);box(channels[c],52+c*w/3,y,w/3-55,24);}
        if(packed)y+=30;
        box(colour,3,y,w-3,25);y+=30;box(valueHeading,3,y,w-3,23);y+=29;
        for(int i=0;i<6;++i){if(i==5)continue;box(sliders[i],143,y,w-206,17);y+=28;}
        box(alpha,110,y,w-110,24);y+=31;
        bool cut=valid()&&current().mat.IsAlphaTestEnabled();
        sliders[5].SetVisible(cut);if(cut){box(sliders[5],143,y,w-206,17);y+=26;}
        box(sides,110,y,20,20);y+=29;
        box(back,3,y,w/2-4,25);box(forward,w/2+3,y,w/2-4,25);
    }
    void show(bool visible){panel.SetVisible(visible);preview.SetVisible(false);caption.SetVisible(false);if(visible)refresh();}
    void refresh(){
        if(!valid())return;auto& s=current();
        mode.SetSelectedWithoutCallback(s.packed?1:0);shader.SetSelectedWithoutCallback(int(s.mat.shaderType));
        for(int r=0;r<Count;++r){
            const auto& m=map(s,r);files[r].SetText(ok(m)?leaf(m):"Choose map...");
            files[r].SetTooltip(ok(m)?leaf(m):labels[r]);clear[r].SetEnabled(ok(m));
            thumbs[r].SetImage(m.resource);thumbs[r].SetColor(ok(m)?wi::Color::White():theme::Control);thumbs[r].SetShadowRadius(0);
        }
        for(int c=0;c<3;++c)channels[c].SetSelectedWithoutCallback(s.channel[c]);
        sides.SetCheck(s.mat.IsDoubleSided());
        alpha.SetSelectedWithoutCallback(s.mat.userBlendMode!=wi::enums::BLENDMODE_OPAQUE?2:s.mat.IsAlphaTestEnabled()?1:0);
        float v[]={s.mat.normalMapStrength,s.mat.roughness,s.mat.metalness,s.mat.reflectance,s.mat.GetEmissiveStrength(),1-s.mat.alphaRef};
        for(int i=0;i<6;++i){sliders[i].SetRange(0,maxima[i]);sliders[i].SetValue(v[i]);}
        colour.SetText(alphaView?"Thumbnail preview: Alpha":"Thumbnail preview: Colour");
        back.SetEnabled(!undo.empty());forward.SetEnabled(!redo.empty());arrange();
    }
    void hover(){
        if(!wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))drag=-1;
        const auto p=wi::input::GetPointer();int r=-1;
        if(panel.IsVisible()&&valid()){
            auto top=panel.GetPos();auto sz=panel.GetSize();
            if(p.y>=top.y&&p.y<top.y+sz.y)for(int j=0;j<Count;++j){
                auto a=thumbs[j].GetPos(),b=thumbs[j].GetSize();
                if(thumbs[j].IsVisible()&&p.x>=a.x&&p.x<a.x+b.x&&p.y>=a.y&&p.y<a.y+b.y&&ok(map(current(),j)))r=j;
            }
        }
        if(r<0){hovered=-1;return;}
        const auto& m=map(current(),r);auto desc=m.resource.GetTexture().GetDesc();
        float k=256.f/std::max(desc.width,desc.height);int w=std::max(1,int(desc.width*k)),h=std::max(1,int(desc.height*k));
        if(hovered!=r||hoverMap.name!=m.name){
            hovered=r;hoverMap=m;
            if(alphaView){Pixels src;if(!src.read(m))return;wi::vector<uint8_t> out(size_t(w)*h*4,255);
                for(int y=0;y<h;++y)for(int x=0;x<w;++x){auto j=(size_t(y)*w+x)*4;out[j]=out[j+1]=out[j+2]=src.at(x,y,w,h,3);}
                alphaImage=image(out,w,h,"Alpha.png").resource;}
            preview.SetImage(alphaView?alphaImage:m.resource);preview.SetColor(wi::Color::White());
            std::string text=std::string(labels[r])+"  /  "+leaf(m)+"\n"+std::to_string(desc.width)+" x "+std::to_string(desc.height)+(alphaView?"  /  Alpha":"  /  Colour");
            if(r==Packed){const char* rgba="RGBA";text+="  AO / Rough / Metal: ";for(int c:current().channel)text+=rgba[c];}
            caption.SetText(text);
        }
        auto a=panel.GetPos();float x=a.x-320,y=std::clamp(p.y-40.f,2.f,std::max(2.f,app.modelImportPanel_.GetSize().y-h-90));
        hoverX=x;hoverY=y;hoverWidth=float(w);hoverHeight=float(h);
    }
    float hoverX=0,hoverY=0,hoverWidth=0,hoverHeight=0;
    void render(wi::graphics::CommandList cmd) const {
        if(!panel.IsVisible()||hovered<0||!ok(hoverMap))return;
        const auto& texture=alphaView?alphaImage:hoverMap.resource;
        if(!texture.IsValid())return;
        wi::image::Params border(hoverX-2,hoverY-2,320,hoverHeight+51,theme::Border);
        wi::image::Draw(nullptr,border,cmd);
        wi::image::Params bg(hoverX,hoverY,316,hoverHeight+47,theme::Idle);wi::image::Draw(nullptr,bg,cmd);
        wi::image::Params pic(hoverX,hoverY,hoverWidth,hoverHeight,wi::Color::White());wi::image::Draw(&texture.GetTexture(),pic,cmd);
        wi::font::Params font; font.posX=hoverX+4;font.posY=hoverY+hoverHeight+4;font.size=14;
        wi::font::Draw(caption.GetText(),font,cmd);
    }
    bool retain(bridge::ModelImportCandidate& candidate,std::string& error){
        auto& scene=*candidate.PeekMutableScene();
        for(size_t i=0;i<states.size();++i){
            auto& s=states[i];if(!s.edited)continue;
            auto entity=scene.materials.GetEntity(i);auto* md=scene.metadatas.GetComponent(entity);if(!md)md=&scene.metadatas.Create(entity);
            md->string_values.set("renegade.material.surfaceMode",s.packed?"packed":"separate");
            std::string channels;for(auto c:s.channel)channels+="RGBA"[c];md->string_values.set("renegade.material.channels",channels);
            auto retainMap=[&](const Map& m,int role)->bool{
                if(!ok(m))return true;
                for(const auto& dep:candidate.Dependencies())if(dep.previewResourceName==m.name){
                    if(role>=0)md->string_values.set(std::string("renegade.material.input.")+labels[role],dep.retainedRelativePath);return true;
                }
                const auto& bytes=m.resource.GetFileData();if(bytes.empty()){error="Cannot retain material map "+leaf(m);return false;}
                bridge::ModelImportDependency d;d.referenceName=m.name;d.previewResourceName=m.name;
                d.retainedRelativePath="materials/"+std::to_string(i)+"/"+std::to_string(candidate.Dependencies().size())+"/"+leaf(m);
                d.bytes.assign(bytes.begin(),bytes.end());
                if(role>=0)md->string_values.set(std::string("renegade.material.input.")+labels[role],d.retainedRelativePath);
                candidate.RetainMaterialInput(std::move(d));return true;
            };
            for(int r=0;r<Extra;++r)if(!retainMap(s.inputs[r],r))return false;
            for(auto& m:s.mat.textures)if(!retainMap(m,-1))return false;
        }
        return true;
    }
};
