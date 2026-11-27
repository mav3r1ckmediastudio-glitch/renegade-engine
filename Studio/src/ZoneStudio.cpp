#include "StudioApplication.h"
#include "ZoneScriptEditor.h"
#include "renegade/bridge/WorldNavigationService.h"
#include "renegade/bridge/ScriptLibraryService.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace renegade::studio
{
    namespace zones = bridge::zones;
    namespace
    {
        // The attachment and the box share one Undo operation. Legacy native zones
        // acquire their project script when the creator opens the source.
        bool PrepareZoneScript(bridge::StudioSession& session, const bridge::StableId& owner,
                               zones::Behaviour behaviour, bridge::SelectionBatchCommand& batch,
                               bridge::StableId& instance, bool& changed, std::string& error)
        {
            changed = false;
            auto& scripts = session.Scripts();
            if (!scripts.EnsureZoneDocument(error)) return false;
            auto& doc = *scripts.Document();
            const std::string path = zones::ScriptPath(behaviour);
            for (const auto& a : doc.attachments)
            {
                if (a.ownerEntityId != owner) continue;
                if (a.sourcePath == path) { instance = a.scriptInstanceId; return true; }
                for (const auto& preset : zones::Presets)
                    if (a.sourcePath == preset.source) instance = a.scriptInstanceId;
            }
            std::vector<bridge::ScriptAuthoringSource> sources;
            std::vector<bridge::ScriptMetadataDiagnostic> diagnostics;
            if (!scripts.EnumerateProjectSources(bridge::ScriptPresentation::Script,sources,diagnostics,error)) return false;
            auto source = std::find_if(sources.begin(),sources.end(),[&](const auto& s){return s.sourcePath==path;});
            if (source == sources.end()) { error = "Cannot find a valid zone script: " + path; return false; }
            auto binding = source->binding;
            if (source->libraryEntry && !source->libraryAdopted)
            {
                bridge::ScriptLibraryAdoptionResult adoption;
                if (!bridge::ScriptLibraryService{}.AdoptEntry(session.Projects().CurrentProject().rootPath,
                    source->libraryManifestPath,source->libraryEntryPath,adoption,error)) return false;
                binding = std::move(adoption.binding);
            }
            std::unique_ptr<bridge::ICommand> edit;
            if (instance.empty())
            {
                auto attachment = bridge::CreateScriptAttachment(bridge::ScriptScope::Entity,owner,binding);
                if (!bridge::ApplyScriptMetadataDefaults(source->metadata,attachment,error)) return false;
                instance = attachment.scriptInstanceId;
                edit = bridge::MakeAddScriptAttachmentCommand(doc,std::move(attachment),error);
            }
            else edit = bridge::MakeReplaceScriptSourceCommand(doc,instance,binding,error);
            if (!edit) return false;
            batch.Add(std::move(edit));
            changed = true;
            return true;
        }

        XMFLOAT3 FacePoint(const zones::Definition& z, int face, FXMMATRIX world, float outward = 0)
        {
            std::array<float, 3> p;
            for (size_t i = 0; i < 3; ++i) p[i] = (z.low[i] + z.high[i]) * .5f;
            const int axis = face / 2;
            p[axis] = (face % 2 ? z.high[axis] : z.low[axis]) + outward;
            XMFLOAT3 result;
            XMStoreFloat3(&result, XMVector3TransformCoord(XMVectorSet(p[0],p[1],p[2],1), world));
            return result;
        }
    }

    void StudioRenderPath::CreateZoneControls()
    {
        zoneInspector_.Create("GAMEPLAY ZONE");
        GetGUI().AddWidget(&zoneInspector_);
        zoneName_.Create("Zone name");
        zoneName_.SetDescription("Name ");
        zoneName_.OnInputAccepted([this](const wi::gui::EventArgs&) { CommitSelectedSceneName(zoneName_.GetValue()); });
        zoneInspector_.AddWidget(&zoneName_);
        zonePreset_.Create("Zone behaviour");
        for (const auto& preset : zones::Presets) zonePreset_.AddItem(preset.label);
        zonePreset_.SetMaxVisibleItemCount(14);
        zonePreset_.OnSelect([this](const wi::gui::EventArgs& args) {
            if (args.iValue >= 0 && args.iValue < int(zones::Behaviour::Count))
                EditSelectedZone([&](auto& zone) {
                    if (int(zone.behaviour) != args.iValue) zones::Choose(zone, zones::Behaviour(args.iValue));
                });
        });
        zoneInspector_.AddWidget(&zonePreset_);
        const char* labels[] = {"WIDTH (m)", "HEIGHT (m)", "DEPTH (m)"};
        for (size_t i = 0; i < zoneSizes_.size(); ++i)
        {
            auto& slider = zoneSizes_[i];
            slider.Create(.1f, 100.f, 2.f, 999.f, labels[i], labels[i]);
            slider.SetTooltip("Enter a size in metres or drag a gold face handle in the viewport.");
            slider.OnValueCommitted([this,i](float extent) {
                EditSelectedZone([&](auto& zone) {
                    if (i != 1) zone.low[i] += (zone.high[i] - zone.low[i] - extent) * .5f;
                    zone.high[i] = zone.low[i] + extent;
                });
            });
            zoneInspector_.AddWidget(&slider);
        }
        for (size_t i=0; i<zoneOptions_.size(); ++i)
        {
            auto& combo=zoneOptions_[i];
            combo.Create(i==0 ? "Effect mode" : i==1 ? "Activation" : "Repeat policy");
            if (i==1) { combo.AddItem("On entry"); combo.AddItem("Press Interact inside"); }
            if (i==2) { combo.AddItem("Every visit"); combo.AddItem("Once per level visit"); combo.AddItem("After cooldown"); }
            combo.OnSelect([this,i](const wi::gui::EventArgs& a){
                EditSelectedZone([&](auto& z){ (i==0 ? z.mode : i==1 ? z.activation : z.repeat)=a.iValue; });
            });
            zoneInspector_.AddWidget(&combo);
        }
        for (size_t i=0; i<zoneNumbers_.size(); ++i)
        {
            auto& n=zoneNumbers_[i];
            const char* label=i==0 ? "Health points" : i==1 ? "Duration (s)" : "Cooldown (s)";
            n.Create(0, i==0 ? 1000.f : 300.f, 1, 1000, label, label);
            n.OnValueCommitted([this,i](float value){
                EditSelectedZone([&](auto& z){ (i==0 ? z.amount : i==1 ? z.seconds : z.cooldown)=value; });
            });
            zoneInspector_.AddWidget(&n);
        }
        zoneTarget_.Create("Target");
        zoneTarget_.OnSelect([this](const wi::gui::EventArgs& a) {
            if (a.iValue >= 0 && size_t(a.iValue) < zoneTargetIds_.size())
                EditSelectedZone([&](auto& zone) { zone.target = zoneTargetIds_[a.iValue]; });
        });
        zoneInspector_.AddWidget(&zoneTarget_);
        for (size_t i = 0; i < zoneEvents_.size(); ++i)
        {
            auto& field = zoneEvents_[i];
            field.Create(i == 0 ? "Enter Event" : "Exit Event");
            field.SetDescription(i == 0 ? "Enter event " : "Exit event ");
            field.OnInputAccepted([this,i](const wi::gui::EventArgs&) {
                EditSelectedZone([&](auto& zone) { if (zone.behaviour==zones::Behaviour::Trigger) (i==0 ? zone.enter : zone.exit)=zoneEvents_[i].GetValue();
                    else (i==0 ? zone.primary : zone.secondary)=zoneEvents_[i].GetValue(); });
            });
            zoneInspector_.AddWidget(&field);
        }
        zoneScript_.Create("Edit Zone Lua");
        zoneScript_.SetText("EDIT LUA SCRIPT");
        zoneScript_.SetTooltip("Open the project copy of this stock zone script. Save to refresh its preview.");
        zoneScript_.OnClick([this](const auto&) { OpenSelectedZoneScript(); });
        zoneInspector_.AddWidget(&zoneScript_);
        CreateLocationControls();
        zoneInspector_.SetVisible(false);
    }

    void StudioRenderPath::OpenSelectedZoneScript()
    {
        if (!session_ || !session_->Projects().HasProject()) return;
        auto& scripts = session_->Scripts();
        const auto& scene = session_->Scenes().GetScene();
        const auto entity = session_->Selection().SelectedEntity();
        zones::Definition zone;
        if (!zones::Read(scene,entity,zone)) return;
        const auto owner = bridge::PersistentEntityId(scene,entity);
        const auto root = session_->Projects().CurrentProject().rootPath;
        std::string error;
        auto fail = [&] { studioChrome_.SetStatusText("ZONE LUA // " + error); };
        auto batch = std::make_unique<bridge::SelectionBatchCommand>();
        bridge::StableId instance;
        bool changed;
        if (!PrepareZoneScript(*session_,owner,zone.behaviour,*batch,instance,changed,error)) { fail(); return; }
        if (changed && !session_->Commands().Execute(std::move(batch))) { error="Could not attach the zone script."; fail(); return; }
        auto* attachment = bridge::FindScriptAttachment(*scripts.Document(),instance);
        if (!attachment) return;
        RefreshStatus();
        static ZoneScriptEditor editor;
        if (!editor.Open(GetActiveWindow(),root,*attachment,zone,[this] { QueueInspectorRefresh(); },error)) fail();
    }

    void StudioRenderPath::EditSelectedZone(const std::function<void(zones::Definition&)>& change)
    {
        if (!session_) return;
        auto& scene = session_->Scenes().GetScene();
        auto id = session_->Selection().SelectedEntity();
        zones::Definition zone;
        if (!zones::Read(scene, id, zone)) return;
        const auto oldBehaviour = zone.behaviour;
        change(zone);
        auto batch = std::make_unique<bridge::SelectionBatchCommand>();
        batch->Add(std::make_unique<zones::Edit>(scene,id,zone));
        if (zone.behaviour != oldBehaviour && session_->Projects().HasProject())
        {
            bridge::StableId instance;
            bool changed;
            std::string error;
            if (!PrepareZoneScript(*session_,bridge::PersistentEntityId(scene,id),zone.behaviour,*batch,instance,changed,error))
            {
                studioChrome_.SetStatusText("ZONE LUA // " + error);
                QueueInspectorRefresh(); return;
            }
        }
        if (session_->Commands().Execute(std::move(batch)))
            QueueInspectorRefresh();
    }

    bool StudioRenderPath::LayoutZoneInspector()
    {
        zones::Definition zone;
        const bool show = session_ && !projectHubVisible_ && studioChrome_.IsSceneWorkspaceActive()
            && !studioChrome_.IsPhysicsLabActive() && !studioChrome_.IsAudioWorkspaceActive()
            && session_->Selection().Entities().size() == 1 &&
            zones::Read(session_->Scenes().GetScene(), session_->Selection().SelectedEntity(), zone);
        zoneInspector_.SetVisible(show);
        if (!show) return false;
        waterfallInspector_.SetVisible(false);
        inspectorPanel_.wi::gui::Widget::SetVisible(false);
        zoneInspector_.SetPos(inspectorPanel_.GetPos());
        zoneInspector_.SetSize(inspectorPanel_.GetSize());
        zoneInspector_.SetEnabled(true);
        const float width = ui::InspectorContentWidth(zoneInspector_.GetSize().x,zoneInspector_.GetControlSize());
        float y = 36;
        auto place = [&](wi::gui::Widget& w, float indent = 0.f) {
            w.SetPos({12 + indent,y}); w.SetSize({width-indent,26}); y += 35;
        };
        const auto* name = session_->Scenes().GetScene().names.GetComponent(session_->Selection().SelectedEntity());
        zoneName_.SetValue(name ? name->name : "");
        place(zoneName_, 45.f);
        zonePreset_.SetSelectedWithoutCallback(int(zone.behaviour));
        place(zonePreset_);
        for (size_t i = 0; i < zoneSizes_.size(); ++i)
        {
            if (!zoneSizes_[i].IsDragging()) zoneSizes_[i].SetValue(zone.high[i] - zone.low[i]);
            place(zoneSizes_[i]);
        }
        LayoutLocationControls(zone,width,y);
        if(zone.behaviour==zones::Behaviour::Location){
            for(auto& control:zoneEvents_)control.SetVisible(false);
            for(auto& control:zoneOptions_)control.SetVisible(false);
            for(auto& control:zoneNumbers_)control.SetVisible(false);
            zoneTarget_.SetVisible(false);
            zoneScript_.SetVisible(locationAdvancedOpen_);if(locationAdvancedOpen_)place(zoneScript_);
            return true;
        }
        bool trigger = zone.behaviour == zones::Behaviour::Trigger;
        const auto& preset=zones::Info(zone.behaviour);
        zoneTarget_.SetVisible(*preset.targetLabel);
        if (*preset.targetLabel)
        {
            zoneTarget_.ClearItems();
            zoneTargetIds_ = {""};
            zoneTarget_.AddItem(trigger ? "Broadcast to all" : std::string(preset.targetLabel)+" (choose target)");
            int selected = 0;
            const auto& scene = session_->Scenes().GetScene();
            for (size_t i = 0; i < scene.names.GetCount(); ++i)
            {
                auto id = bridge::PersistentEntityId(scene, scene.names.GetEntity(i));
                if (id.empty()) continue;
                if (id == zone.target) selected = int(zoneTargetIds_.size());
                zoneTargetIds_.push_back(id);
                zoneTarget_.AddItem(scene.names[i].name);
            }
            if (!zone.target.empty() && selected == 0)
            {
                selected = int(zoneTargetIds_.size());
                zoneTargetIds_.push_back(zone.target);
                zoneTarget_.AddItem("Missing target");
            }
            zoneTarget_.SetSelectedWithoutCallback(selected);
            place(zoneTarget_);
        }
        for (size_t i=0; i<zoneEvents_.size(); ++i)
        {
            const char* label=i==0 ? preset.primaryLabel : preset.secondaryLabel;
            auto& field=zoneEvents_[i];
            field.SetVisible(*label);
            if (!*label) continue;
            field.SetDescription(std::string(label)+" ");
            field.SetValue(trigger ? (i==0 ? zone.enter : zone.exit) : (i==0 ? zone.primary : zone.secondary));
            place(field,100.f);
        }
        auto& modes=zoneOptions_[0];
        modes.SetVisible(*preset.modes);
        if (*preset.modes)
        {
            modes.ClearItems();
            std::string options=preset.modes;
            size_t start=0;
            for (;;) {
                const auto end=options.find('|',start);
                modes.AddItem(options.substr(start,end-start));
                if (end==std::string::npos) break;
                start=end+1;
            }
            modes.SetSelectedWithoutCallback(zone.mode);
            place(modes);
        }
        zoneOptions_[1].SetSelectedWithoutCallback(zone.activation); place(zoneOptions_[1]);
        zoneOptions_[2].SetSelectedWithoutCallback(zone.repeat); place(zoneOptions_[2]);
        for (size_t i=0; i<zoneNumbers_.size(); ++i)
        {
            auto& n=zoneNumbers_[i];
            const char* label=i==0 ? preset.amountLabel : i==1 ? preset.secondsLabel : "Cooldown (s)";
            const bool visible=*label && (i!=2 || zone.repeat==2);
            n.SetVisible(visible);
            if (!visible) continue;
            n.SetTooltip(label);
            if (!n.IsDragging()) n.SetValue(i==0 ? zone.amount : i==1 ? zone.seconds : zone.cooldown);
            place(n);
        }
        place(zoneScript_);
        return true;
    }

    namespace
    {
        // Atlas cells follow the stable, serialized gameplay preset order.
        constexpr wi::Color ZoneColors[] = {
            {228,157,100}, {244,198,92}, {117,188,245}, {237,217,128},
            {85,219,171}, {113,202,249}, {185,154,250}, {255,136,127},
            {250,120,115}, {239,165,116}, {126,219,200}, {128,187,255}, {164,212,153}, {239,194,108}
        };
        static_assert(std::size(ZoneColors)==zones::Presets.size());
    }

    void StudioRenderPath::DrawZoneLabels(wi::graphics::CommandList cmd) const
    {
        if (!session_ || !camera || !studioChrome_.IsSceneWorkspaceActive() || editorZones_.empty()) return;
        const auto& scene=session_->Scenes().GetScene();
        const bool visible=studioChrome_.ShowZones();
        const auto viewport=StoryFlowWorkspaceBounds();
        static const auto atlas=wi::resourcemanager::Load("Content/editor/markericons/zone_presets.png");
        struct Label { XMFLOAT2 anchor; size_t index; float distance; bool selected; };
        std::vector<Label> labels;
        const auto eye=XMLoadFloat3(&camera->Eye);
        for (const auto id : editorZones_) {
            const bool selected=session_->Selection().Contains(id);
            if ((!visible && !selected) || !session_->Scenes().IsHierarchyVisible(id)) continue;
            zones::Definition zone;
            const auto* t=scene.transforms.GetComponent(id);
            if (!t || !zones::Read(scene,id,zone)) continue;
            if (selected && zoneFace_>=0 && zoneDragEntity_==id) zone=zonePreview_;
            const auto world=FacePoint(zone,3,XMLoadFloat4x4(&t->world));
            XMFLOAT2 anchor;
            if (ProjectEditorPoint(world,anchor))
                labels.push_back({anchor,size_t(zone.behaviour),XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&world)-eye)),selected});
        }
        // Give selected and nearby boxes first choice of the available space.
        std::stable_sort(labels.begin(),labels.end(),[](const Label& a,const Label& b) {
            return a.selected!=b.selected ? a.selected : a.distance<b.distance;
        });
        std::vector<XMFLOAT4> used;
        auto rect=[&](float x,float y,float w,float h,wi::Color color) {
            wi::image::Params p(x,y,w,h,color); wi::image::Draw(nullptr,p,cmd);
        };
        for (const auto& label:labels) {
            const auto& preset=zones::Presets[label.index];
            const auto color=ZoneColors[label.index];
            wi::font::Params font(0,0,16,wi::font::WIFALIGN_LEFT,wi::font::WIFALIGN_CENTER,
                wi::Color(248,248,244),wi::Color(0,0,0,0));
            const float textWidth=wi::font::TextWidth(preset.label,font);
            bool placed=false;
            for (int attempt=0;attempt<5 && !placed;++attempt) {
                const bool compact=attempt>=3;
                const float width=compact ? 28.f : 38.f+textWidth;
                const float offset=attempt==1 ? -32.f : attempt==2 ? 32.f : attempt==4 ? -30.f : 0.f;
                if (viewport.z-viewport.x<width+4 || viewport.w-viewport.y<32) break;
                const float x=std::clamp(label.anchor.x-width*.5f,viewport.x+2,viewport.z-width-2);
                const float y=std::clamp(label.anchor.y-31+offset,viewport.y+2,viewport.w-30);
                if (std::any_of(used.begin(),used.end(),[&](const XMFLOAT4& r) {
                    return x<r.z+2 && x+width+2>r.x && y<r.w+2 && y+29>r.y;
                })) continue;
                used.push_back({x,y,x+width,y+27});
                const float ax=std::clamp(label.anchor.x,x+2,x+width-2);
                if (label.anchor.y>y+27) rect(ax-1,y+27,2,label.anchor.y-y-27,color);
                else if(label.anchor.y<y) rect(ax-1,label.anchor.y,2,y-label.anchor.y,color);
                rect(x,y,width,27,label.selected?wi::Color(251,151,60):wi::Color(68,82,92));
                rect(x+1,y+1,width-2,25,wi::Color(18,26,32,247));
                rect(x+1,y+1,3,25,color);
                if (atlas.IsValid()) {
                    wi::image::Params icon(x+6,y+3,21,21,color);
                    const size_t cell=label.index==size_t(zones::Behaviour::Location)?0:label.index;
                    icon.enableDrawRect({float(cell%4)*64,float(cell/4)*64,64,64});
                    wi::image::Draw(&atlas.GetTexture(),icon,cmd);
                }
                if (!compact) {
                    font.posX=x+31; font.posY=y+13;
                    wi::font::Draw(preset.label,font,cmd);
                }
                placed=true;
            }
        }
    }

    void StudioRenderPath::DrawZones()
    {
        if (projectHubVisible_ || !studioChrome_.IsSceneWorkspaceActive()) return;
        const auto& scene = session_->Scenes().GetScene();
        for (auto id : editorZones_)
        {
            zones::Definition z;
            auto* t = scene.transforms.GetComponent(id);
            if (!t || !zones::Read(scene,id,z)) continue;
            const bool selected = session_->Selection().Contains(id);
            if ((!selected && !studioChrome_.ShowZones()) || !session_->Scenes().IsHierarchyVisible(id)) continue;
            if (selected && zoneFace_ >= 0 && zoneDragEntity_ == id) z = zonePreview_;
            const auto world = XMLoadFloat4x4(&t->world);
            const auto matrix = XMMatrixScaling((z.high[0]-z.low[0])*.5f,(z.high[1]-z.low[1])*.5f,(z.high[2]-z.low[2])*.5f)
                * XMMatrixTranslation((z.high[0]+z.low[0])*.5f,(z.high[1]+z.low[1])*.5f,(z.high[2]+z.low[2])*.5f) * world;
            wi::renderer::DrawBox(matrix, selected ? XMFLOAT4(1,.55f,.14f,1) : ZoneColors[size_t(z.behaviour)].toFloat4(), false);
            if (selected)
                for (int face = 0; face < 6; ++face)
                    wi::renderer::DrawSphere(wi::primitive::Sphere(FacePoint(z,face,world),gizmo_.isScalator ? .15f : .09f), {1,.65f,.2f,1}, false);
        }
    }

    bool StudioRenderPath::HandleZonePointer(const XMFLOAT4& pointer)
    {
        if (!session_ || !camera) return false;
        auto& scene = session_->Scenes().GetScene();
        const auto id = session_->Selection().SelectedEntity();
        const bool cancel = wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT) || wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE);
        if (zoneFace_ >= 0)
        {
            if (cancel || id != zoneDragEntity_) { zoneFace_ = -1; return true; }
            const int axis = zoneFace_ / 2;
            const auto v = zonePixelsPerMetre_;
            const float delta = ((pointer.x-zonePointerStart_.x)*v.x+(pointer.y-zonePointerStart_.y)*v.y)/(v.x*v.x+v.y*v.y);
            zonePreview_ = zoneStart_;
            auto& low = zonePreview_.low[axis]; auto& high = zonePreview_.high[axis];
            if (zoneFace_%2) high = std::clamp(high + delta, low+.1f, low+100.f);
            else low = std::clamp(low + delta, high-100.f, high-.1f);
            if (!wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))
            {
                zoneFace_ = -1;
                if (zonePreview_.low != zoneStart_.low || zonePreview_.high != zoneStart_.high)
                    EditSelectedZone([this](auto& z) { z.low=zonePreview_.low; z.high=zonePreview_.high; });
            }
            return true;
        }
        if (zonePlacing_ && cancel) { zonePlacing_ = false; return true; }
        if (!studioChrome_.IsSceneWorkspaceActive()) { zonePlacing_ = false; return false; }
        if (GetGUI().HasFocus() || !IsPointerOverViewport(pointer)) return false;
        if (zonePlacing_)
        {
            const auto ray = wi::renderer::GetPickRay(long(pointer.x),long(pointer.y),*this,*camera);
            auto hit = wi::scene::Pick(ray, wi::enums::FILTER_OBJECT_ALL | wi::enums::FILTER_TERRAIN, ~0u, scene);
            XMFLOAT3 p = hit.position;
            if (hit.entity == wi::ecs::INVALID_ENTITY)
            {
                if (std::abs(ray.direction.y) < .0001f) return true;
                const float d = -ray.origin.y / ray.direction.y;
                if (d < ray.TMin || d > ray.TMax) return true;
                p = {ray.origin.x + d*ray.direction.x, 0, ray.origin.z + d*ray.direction.z};
            }
            wi::renderer::DrawSphere(wi::primitive::Sphere(p,.12f),{1,.65f,.2f,1},false);
            if (wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
            {
                auto command = std::make_unique<zones::Create>(scene,p,zonePlacementPreset_);
                const auto created = command->Id();
                const auto owner = command->PersistentId();
                auto batch = std::make_unique<bridge::SelectionBatchCommand>();
                batch->Add(std::move(command));
                if (session_->Projects().HasProject())
                {
                    bridge::StableId instance;
                    bool changed;
                    std::string error;
                    if (!PrepareZoneScript(*session_,owner,zonePlacementPreset_,*batch,instance,changed,error))
                    {
                        studioChrome_.SetStatusText("ZONE LUA // " + error); return true;
                    }
                }
                if (session_->Commands().Execute(std::move(batch)))
                {
                    zonePlacing_ = false;
                    session_->Selection().Select(created);
                    SyncGizmoSelection(); RefreshHierarchy(); RefreshInspector(); RefreshStatus();
                }
            }
            return true;
        }
        if (gizmoDragActive_ || !wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)) return false;
        auto* transform = scene.transforms.GetComponent(id);
        if (!transform || !zones::Read(scene,id,zoneStart_)) return false;
        const auto world = XMLoadFloat4x4(&transform->world);
        float best = 100;
        for (int face = 0; face < 6; ++face)
        {
            XMFLOAT2 start, along;
            if (!ProjectEditorPoint(FacePoint(zoneStart_,face,world),start) ||
                !ProjectEditorPoint(FacePoint(zoneStart_,face,world,.25f),along)) continue;
            XMFLOAT2 v{4*(along.x-start.x),4*(along.y-start.y)};
            if (v.x*v.x+v.y*v.y < 1) continue;
            float dx = pointer.x-start.x, dy = pointer.y-start.y;
            if (dx*dx+dy*dy >= best) continue;
            best = dx*dx+dy*dy;
            zoneFace_ = face;
            zonePixelsPerMetre_ = v;
        }
        if (zoneFace_ < 0) return false;
        zoneDragEntity_ = id; zonePreview_ = zoneStart_; zonePointerStart_ = {pointer.x,pointer.y};
        return true;
    }
}


namespace renegade::studio
{
    void StudioRenderPath::CreateLocationControls()
    {
        locationType_.Create("Location type");locationType_.SetDescription("Type ");
        locationType_.SetTooltip("Location, Quest, Dungeon, POI, Event or your own type. Type does not limit your custom icon.");
        locationType_.OnInputAccepted([this](const auto&){EditSelectedZone([&](auto& z){z.location.type=locationType_.GetValue();});});
        zoneInspector_.AddWidget(&locationType_);
        locationIcon_.Create("Location icon");locationIcon_.SetText("CHOOSE LOCATION ICON...");
        locationIcon_.SetTooltip("Import an image for this location only. Other locations of the same type keep their own icons.");
        locationIcon_.OnClick([this](const auto&){ChooseLocationIcon();});zoneInspector_.AddWidget(&locationIcon_);
        locationAnnounce_.Create("Show location name nearby");
        locationAnnounce_.SetTooltip("Show the location name once when the player approaches within the trigger range.");
        locationAnnounce_.OnClick([this](const auto&){EditSelectedZone([&](auto& z){z.location.announcement.enabled=locationAnnounce_.GetCheck();});});zoneInspector_.AddWidget(&locationAnnounce_);
        locationEffect_.Create("Text animation");for(const char* name:{"None","Fade in / out","Glitch","Wobble"})locationEffect_.AddItem(name);
        locationEffect_.OnSelect([this](const auto& a){if(a.iValue>=0&&a.iValue<4)EditSelectedZone([&](auto& z){z.location.announcement.effect=bridge::LocationTextEffect(a.iValue);});});
        locationEffect_.SetTooltip("Optional text animation. Reduced motion keeps the fade and removes wobble/glitch movement.");zoneInspector_.AddWidget(&locationEffect_);
        const char* labels[]={"ANNOUNCE RANGE (m)","TOTAL DISPLAY TIME (s)","FADE IN (s)","FADE OUT (s)","EFFECT STRENGTH (px)","EFFECT SPEED"};
        const float lo[]={0,.1f,0,0,0,.1f},hi[]={10000,600,60,60,128,30};
        for(size_t i=0;i<locationNumbers_.size();++i){
            auto& control=locationNumbers_[i];control.Create(lo[i],hi[i],i==0?40.f:4.f,1000,labels[i],labels[i]);
            control.SetTooltip(i==0?"Horizontal distance from the zone's placement in metres.":i==1?"Total time on screen, including fade in and fade out.":labels[i]);
            control.OnValueCommitted([this,i](float value){EditSelectedZone([&](auto& z){auto& a=z.location.announcement;switch(i){case 0:a.range=value;break;case 1:a.duration=value;break;case 2:a.fadeIn=value;break;case 3:a.fadeOut=value;break;case 4:a.strength=value;break;case 5:a.speed=value;break;}});});
            zoneInspector_.AddWidget(&control);
        }
        locationAdvanced_.Create("Advanced location settings");locationAdvanced_.SetText("ADVANCED...");
        locationAdvanced_.OnClick([this](const auto&){locationAdvancedOpen_=!locationAdvancedOpen_;QueueInspectorRefresh();});zoneInspector_.AddWidget(&locationAdvanced_);
        locationRepeat_.Create("Repeat announcement on return");locationRepeat_.SetTooltip("Requires leaving beyond the trigger range plus a small margin before it can trigger again.");
        locationRepeat_.OnClick([this](const auto&){EditSelectedZone([&](auto& z){z.location.announcement.repeatOnReturn=locationRepeat_.GetCheck();});});zoneInspector_.AddWidget(&locationRepeat_);
        locationCompass_.Create("Show on compass");locationCompass_.OnClick([this](const auto&){EditSelectedZone([&](auto& z){z.location.showOnCompass=locationCompass_.GetCheck();});});zoneInspector_.AddWidget(&locationCompass_);
        locationVisibility_.Create("Compass discovery");locationVisibility_.AddItem("Show before discovery");locationVisibility_.AddItem("Show after entering zone");
        locationVisibility_.OnSelect([this](const auto& a){EditSelectedZone([&](auto& z){z.location.onlyAfterDiscovery=a.iValue==1;});});zoneInspector_.AddWidget(&locationVisibility_);
    }
    void StudioRenderPath::LayoutLocationControls(const zones::Definition& z,float width,float& y)
    {
        const bool show=z.behaviour==zones::Behaviour::Location;
        for(wi::gui::Widget* control:std::array<wi::gui::Widget*,8>{&locationType_,&locationIcon_,&locationAnnounce_,&locationEffect_,&locationAdvanced_,&locationRepeat_,&locationCompass_,&locationVisibility_})control->SetVisible(show);
        for(auto& control:locationNumbers_)control.SetVisible(show);
        if(!show)return;
        auto place=[&](wi::gui::Widget& w){w.SetPos({12,y});w.SetSize({width,26});y+=35;};
        auto check=[&](wi::gui::CheckBox& w){w.SetPos({12,y});w.SetSize({20,20});y+=35;};
        locationType_.SetValue(z.location.type);place(locationType_);place(locationIcon_);
        locationIcon_.SetText(z.location.icon.empty()?"CHOOSE LOCATION ICON...":"CHANGE LOCATION ICON...");
        locationAnnounce_.SetCheck(z.location.announcement.enabled);check(locationAnnounce_);
        const auto& a=z.location.announcement;
        const float values[]={a.range,a.duration,a.fadeIn,a.fadeOut,a.strength,a.speed};
        for(size_t i=0;i<2;++i){auto& control=locationNumbers_[i];control.SetVisible(a.enabled);if(a.enabled){if(!control.IsDragging())control.SetValue(values[i]);place(control);}}
        locationEffect_.SetVisible(a.enabled);if(a.enabled){locationEffect_.SetSelectedWithoutCallback(int(a.effect));place(locationEffect_);}
        locationAdvanced_.SetText(locationAdvancedOpen_?"ADVANCED: OPEN":"ADVANCED...");place(locationAdvanced_);
        for(size_t i=2;i<6;++i){auto& control=locationNumbers_[i];control.SetVisible(locationAdvancedOpen_&&a.enabled);if(control.IsVisible()){if(!control.IsDragging())control.SetValue(values[i]);place(control);}}
        locationRepeat_.SetVisible(locationAdvancedOpen_&&a.enabled);if(locationRepeat_.IsVisible()){locationRepeat_.SetCheck(a.repeatOnReturn);check(locationRepeat_);}
        locationCompass_.SetVisible(locationAdvancedOpen_);locationVisibility_.SetVisible(locationAdvancedOpen_);
        if(locationAdvancedOpen_){locationCompass_.SetCheck(z.location.showOnCompass);check(locationCompass_);locationVisibility_.SetSelectedWithoutCallback(z.location.onlyAfterDiscovery?1:0);place(locationVisibility_);}
    }
    void StudioRenderPath::ChooseLocationIcon()
    {
        if(!session_||!session_->Projects().HasProject())return;
        const auto entity=session_->Selection().SelectedEntity();const auto owner=bridge::PersistentEntityId(session_->Scenes().GetScene(),entity);
        const auto root=session_->Projects().CurrentProject().rootPath;const auto project=session_->Projects().CurrentProject().projectId;
        wi::helper::FileDialogParams dialog;dialog.type=wi::helper::FileDialogParams::OPEN;dialog.description="Choose a location icon";dialog.extensions={"png","jpg","jpeg","bmp"};
        wi::helper::FileDialog(dialog,[this,entity,owner,root,project](const std::string& source){if(source.empty())return;
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this,entity,owner,root,project,source](uint64_t){
                if(!session_||!session_->Projects().HasProject()||session_->Projects().CurrentProject().rootPath!=root||session_->Selection().SelectedEntity()!=entity||bridge::PersistentEntityId(session_->Scenes().GetScene(),entity)!=owner)return;
                std::string resource,error;if(!bridge::ImportScreenNavigationImage(root,project,source,resource,error)){studioChrome_.SetStatusText(error);return;}
                EditSelectedZone([&](auto& z){z.location.icon=resource;});
            });
        });
    }
}
