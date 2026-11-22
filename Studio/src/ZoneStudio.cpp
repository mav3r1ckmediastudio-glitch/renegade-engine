#include "StudioApplication.h"
#include "ZoneScriptEditor.h"
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
        zonePreset_.SetMaxVisibleItemCount(13);
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
            if (!selected && !studioChrome_.ShowZones()) continue;
            if (selected && zoneFace_ >= 0 && zoneDragEntity_ == id) z = zonePreview_;
            const auto world = XMLoadFloat4x4(&t->world);
            const auto matrix = XMMatrixScaling((z.high[0]-z.low[0])*.5f,(z.high[1]-z.low[1])*.5f,(z.high[2]-z.low[2])*.5f)
                * XMMatrixTranslation((z.high[0]+z.low[0])*.5f,(z.high[1]+z.low[1])*.5f,(z.high[2]+z.low[2])*.5f) * world;
            wi::renderer::DrawBox(matrix, selected ? XMFLOAT4(1,.55f,.14f,1) : XMFLOAT4(.25f,.8f,.65f,.8f), false);
            if (selected)
                for (int face = 0; face < 6; ++face)
                    wi::renderer::DrawSphere(wi::primitive::Sphere(FacePoint(z,face,world),.09f), {1,.65f,.2f,1}, false);
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
                auto command = std::make_unique<zones::Create>(scene,p);
                const auto created = command->Id();
                const auto owner = command->PersistentId();
                auto batch = std::make_unique<bridge::SelectionBatchCommand>();
                batch->Add(std::move(command));
                if (session_->Projects().HasProject())
                {
                    bridge::StableId instance;
                    bool changed;
                    std::string error;
                    if (!PrepareZoneScript(*session_,owner,zones::Behaviour::Trigger,*batch,instance,changed,error))
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
