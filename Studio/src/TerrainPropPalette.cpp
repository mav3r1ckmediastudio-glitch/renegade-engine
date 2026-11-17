#include "StudioApplication.h"
#include "CreatorAssetFrameBoundary.h"
#include "renegade/bridge/TerrainPropAuthoring.h"
#include "renegade/bridge/TerrainPropEditService.h"
#include "renegade/bridge/TerrainPropTerrain.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ReusableAssetService.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <future>
#include <fstream>
#include <cstdlib>
#include <stdexcept>
#include <sstream>
#include <unordered_map>

namespace renegade::studio
{
namespace
{
    using namespace bridge;
    void StyleLabel(wi::gui::Label& label)
    {
        label.SetColor(wi::Color(24,28,29,255));label.SetShadowRadius(0);
        label.font.params.size=12;label.font.params.bolden=.18f;
        label.font.params.color=wi::Color(236,238,239,255);label.font.params.h_align=wi::font::WIFALIGN_LEFT;
        label.font.params.shadowColor=wi::Color::Transparent();
    }
    struct PropEntryControls
    {
        wi::gui::Label label;
        SceneInspectorSlider density;
        SceneInspectorButton remove;
    };
    struct PropStaging
    {
        std::atomic<bool> cancelled{false};
        std::atomic<float> progress{0};
        TerrainPropDocument document;
        std::string expectedRevision, error;
        std::uint64_t sceneRevision = 0, commandRevision = 0;
        bool ready = false;
    };
    struct PropPalette
    {
        SceneInspectorComboBox picker, mixes;
        SceneInspectorButton refresh, add, saveMix, generate, paint, erase, reproject, cancel;
        SceneInspectorTextInputField mixName;
        SceneInspectorSlider seed, radius, spacing;
        wi::gui::Label status;
        std::vector<std::unique_ptr<PropEntryControls>> rows;
        std::vector<std::pair<StableId,std::string>> choices;
        TerrainPropDocument draft;
        std::shared_ptr<TerrainPropEditService> service;
        std::unordered_map<wi::ecs::Entity,std::shared_ptr<TerrainPropEditService>> services;
        wi::ecs::Entity owner = wi::ecs::INVALID_ENTITY, terrain = wi::ecs::INVALID_ENTITY;
        std::uint64_t reconcileRevision = UINT64_MAX;
        bool ownerPresentLast = true;
        std::string projectId, revision;
        std::uint64_t lifecycleRevision = UINT64_MAX;
        bool created = false, armed = false, stroke = false, rebuild = false, reload = false, addPending = false;
        int pending = -2;
        TerrainPropEditOperation mode = TerrainPropEditOperation::Paint;
        std::vector<std::array<float,3>> path;
        std::shared_ptr<PropStaging> staging;
        std::future<void> worker;
        detail::CreatorAssetFrameMutation mutation;
    };
    std::unique_ptr<PropPalette> palette;
    PropPalette& Props() { if (!palette) palette = std::make_unique<PropPalette>(); return *palette; }
    void CommitDensity(size_t index,float maximum,float input)
    {
        auto& p=Props();auto& entries=p.draft.authoring.recipe.mix.entries;if(index>=entries.size())return;
        const float value=std::isfinite(input)?std::clamp(input,0.f,maximum):entries[index].densityPerSquareMetre;
        entries[index].densityPerSquareMetre=value;
        if(index<p.rows.size()){
            auto& slider=p.rows[index]->density;slider.SetRange(0,maximum);slider.SetValue(value);slider.valueInputField.SetValue(value);
        }
    }
    TerrainPropProfileId ProfileId(const StableId& id)
    {
        std::string hex;
        for (char c : id) if (c != '-') hex += c;
        TerrainPropProfileId result{};
        if (hex.size() == 32) for (size_t i=0;i<16;++i)
            result[i] = static_cast<std::uint8_t>(std::stoul(hex.substr(i*2,2),nullptr,16));
        return result;
    }
}
void StudioRenderPath::CreateTerrainPropControls()
{
    auto& p=Props(); if(p.created)return; p.created=true;
    auto button=[this](SceneInspectorButton& b,const char* name,const char* text,const char* tip,auto callback){
        b.Create(name);b.SetText(text);b.SetTooltip(tip);b.OnClick(callback);inspectorPanel_.AddWidget(&b);
    };
    p.picker.Create("Terrain prop model");p.picker.SetTooltip("Choose a registered model from the project's governed asset picker.");
    inspectorPanel_.AddWidget(&p.picker);
    button(p.refresh,"Refresh terrain prop models","REFRESH MODELS","Reload the project model picker after import.",[](const wi::gui::EventArgs&){Props().reload=true;});
    button(p.add,"Add terrain prop model","ADD MODEL TO MIX","Add the selected static model. Existing placements stay unchanged until Apply.",[](const wi::gui::EventArgs&){Props().addPending=true;});
    p.mixes.Create("Saved terrain prop mixes");p.mixes.SetTooltip("Choose a saved layer mix, then Generate Apply or paint.");
    p.mixes.OnSelect([this](const wi::gui::EventArgs& a){
        auto& s=Props();if(a.iValue<=0 || size_t(a.iValue)>s.draft.authoring.savedMixes.size())return;
        s.draft.authoring.recipe.mix=s.draft.authoring.savedMixes[a.iValue-1];s.rebuild=true;
    });inspectorPanel_.AddWidget(&p.mixes);
    p.mixName.Create("Terrain prop mix name");p.mixName.SetText("Woodland");
    p.mixName.SetTooltip("Name for Save Mix. Mixes are saved with this layer and participate in Undo.");
    inspectorPanel_.AddWidget(&p.mixName);
    button(p.saveMix,"Save terrain prop mix","SAVE MIX","Persist this named mix as one Undo operation.",[](const wi::gui::EventArgs&){
        auto& s=Props();auto mix=s.draft.authoring.recipe.mix;mix.name=s.mixName.GetText();
        if(mix.name.empty() || mix.name.size()>128){s.status.SetText("Enter a mix name (1-128 characters).");return;}
        auto& mixes=s.draft.authoring.savedMixes;
        auto it=std::find_if(mixes.begin(),mixes.end(),[&](const auto& m){return m.name==mix.name;});
        if(it==mixes.end())mixes.push_back(mix);else *it=mix;
        s.draft.authoring.recipe.mix.name=mix.name;s.pending=-1;
    });
    auto slider=[this](SceneInspectorSlider& s,float min,float max,float value,const char* name,const char* text,auto changed){
        s.Create(min,max,value,10000,name,text);s.OnValuePreview(changed);s.OnValueCommitted(changed);inspectorPanel_.AddWidget(&s);
    };
    slider(p.seed,1,1000000,1,"Terrain prop seed","SEED",[](float v){Props().draft.authoring.recipe.seed=std::uint64_t(v);});
    slider(p.radius,1,100,8,"Terrain prop brush radius","BRUSH RADIUS",[](float){});
    slider(p.spacing,.25f,20,2,"Terrain prop minimum spacing","MINIMUM SPACING",[](float v){for(auto& e:Props().draft.authoring.recipe.mix.entries)e.minSpacing=v;});
    button(p.generate,"Generate terrain props","GENERATE APPLY","Deterministically fill current finite terrain. Preserves clearings, overrides and retained IDs.",[](const wi::gui::EventArgs&){Props().pending=int(TerrainPropEditOperation::Generate);});
    auto arm=[this](TerrainPropEditOperation mode){
        auto& s=Props();s.armed=true;s.mode=mode;
        DisableWd01VegetationBrush();
        terrainRiverBrushActive_=false;terrainSculptMode_.SetSelectedWithoutCallback(0);
        s.paint.SetText(mode==TerrainPropEditOperation::Paint?"PAINT [ACTIVE]":"PAINT");
        s.erase.SetText(mode==TerrainPropEditOperation::Erase?"ERASE [ACTIVE]":"ERASE");
        s.status.SetText("Drag on terrain. One release is one Undo. Escape cancels.");
    };
    button(p.paint,"Paint terrain props","PAINT","Paint by travelled distance, independent of frame rate.",[arm](const wi::gui::EventArgs&){arm(TerrainPropEditOperation::Paint);});
    button(p.erase,"Erase terrain props","ERASE","Create persistent clearings which survive regeneration.",[arm](const wi::gui::EventArgs&){arm(TerrainPropEditOperation::Erase);});
    button(p.reproject,"Reproject terrain props","REPROJECT TO TERRAIN","Reposition retained placements after sculpting or terrain expansion.",[](const wi::gui::EventArgs&){Props().pending=int(TerrainPropEditOperation::Reproject);});
    button(p.cancel,"Cancel terrain props","CANCEL / STOP BRUSH","Discard staging or the current stroke without changing the scene.",[](const wi::gui::EventArgs&){
        auto& s=Props();if(s.staging)s.staging->cancelled=true;s.mutation.Cancel();s.armed=s.stroke=false;s.path.clear();s.pending=-2;
        s.paint.SetText("PAINT");s.erase.SetText("ERASE");s.status.SetText("Cancelled. Previous display retained.");
    });
    p.status.Create("Terrain props progress");p.status.SetText("Add models, set density, then Generate Apply or Paint.");
    p.status.SetWrapEnabled(true);StyleLabel(p.status);inspectorPanel_.AddWidget(&p.status);
}
void StudioRenderPath::RebuildTerrainPropRows()
{
    auto& p=Props();
    for(auto& row:p.rows){inspectorPanel_.RemoveWidget(&row->label);inspectorPanel_.RemoveWidget(&row->density);inspectorPanel_.RemoveWidget(&row->remove);}
    p.rows.clear();
    const auto& entries=p.draft.authoring.recipe.mix.entries;
    for(size_t i=0;i<entries.size();++i)
    {
        auto row=std::make_unique<PropEntryControls>();
        std::string name="Model "+std::to_string(i+1);
        for(const auto& profile:p.draft.profiles)if(profile.id==entries[i].profileId)
            for(const auto& choice:p.choices)if(choice.first==profile.modelAssetId)name=choice.second;
        row->label.Create("Terrain prop entry "+std::to_string(i));row->label.SetText(name);StyleLabel(row->label);
        const float step=p.draft.authoring.recipe.candidateStep;
        const float maximum=1.f/(step*step);
        row->density.Create(0,maximum,entries[i].densityPerSquareMetre,10000,"Terrain prop density "+std::to_string(i),"DENSITY / m2");
        row->density.SetTooltip("Linked slider and numeric entry. Density is independent for each model.");
        auto density=[i,maximum](float v){CommitDensity(i,maximum,v);};
        row->density.OnValuePreview(density);row->density.OnValueCommitted(density);
        row->remove.Create("Remove terrain prop entry "+std::to_string(i));row->remove.SetText("REMOVE FROM MIX");
        row->remove.OnClick([i](const wi::gui::EventArgs&){
            auto& s=Props();auto& e=s.draft.authoring.recipe.mix.entries;if(i<e.size()){e.erase(e.begin()+i);s.rebuild=true;}
        });
        inspectorPanel_.AddWidget(&row->label);inspectorPanel_.AddWidget(&row->density);inspectorPanel_.AddWidget(&row->remove);
        p.rows.push_back(std::move(row));
    }
    p.mixes.ClearItems();p.mixes.AddItem("SAVED MIX...");
    for(const auto& mix:p.draft.authoring.savedMixes)p.mixes.AddItem(mix.name);
    p.mixes.SetSelectedWithoutCallback(0);
    p.seed.SetValue(float(p.draft.authoring.recipe.seed));
    if(!entries.empty())p.spacing.SetValue(entries.front().minSpacing);
    p.rebuild=false;QueueInspectorRefresh();
}
std::vector<wi::gui::Widget*> StudioRenderPath::TerrainPropWidgets()
{
    auto& p=Props();if(!p.created)return {};
    std::vector<wi::gui::Widget*> rows{&p.picker,&p.refresh,&p.add,&p.mixes,&p.mixName,&p.saveMix,&p.seed,&p.spacing};
    for(auto& e:p.rows)rows.insert(rows.end(),{&e->label,&e->density,&e->remove});
    rows.insert(rows.end(),{&p.radius,&p.generate,&p.paint,&p.erase,&p.reproject,&p.cancel,&p.status});return rows;
}
void StudioRenderPath::RefreshTerrainPropControls()
{
    auto& p=Props();if(!p.created)return;
    bool enabled=session_ && session_->Projects().HasProject() && session_->Scenes().GetScene().terrains.GetCount()==1 &&
        p.service && session_->Scenes().GetScene().transforms.Contains(p.owner);
    const bool busy=p.staging || p.mutation.IsPending();
    for(auto* w:TerrainPropWidgets())w->SetEnabled(enabled&&!busy);
    p.status.SetEnabled(true);p.cancel.SetEnabled(enabled);
    // Native AddWidget/global theme can repaint stock labels. Reassert the
    // owned Inspector treatment after those passes, including dynamic rows.
    StyleLabel(p.status);for(auto& row:p.rows)StyleLabel(row->label);
}
void StudioRenderPath::SyncTerrainPropPalette(bool force)
{
    auto& p=Props();
    const bool hasProject=session_ && session_->Projects().HasProject();
    const bool sameDocument=hasProject && p.projectId==session_->Projects().CurrentProject().projectId &&
        p.lifecycleRevision==session_->Scenes().Revision();
    if(!sameDocument && (p.service || !p.services.empty())){
        if(p.staging)p.staging->cancelled=true;p.mutation.Cancel();p.armed=p.stroke=false;p.pending=-2;p.path.clear();
        p.service.reset();p.services.clear();p.owner=wi::ecs::INVALID_ENTITY;p.revision.clear();p.reconcileRevision=UINT64_MAX;p.ownerPresentLast=true;
    }
    if(!hasProject)return;
    auto& scene=session_->Scenes().GetScene();const auto& project=session_->Projects().CurrentProject();
    auto reconcile=[this,&p,&scene]{
        if(!p.service || p.staging || p.mutation.IsPending())return;
        bool present=scene.transforms.Contains(p.owner);
        auto history=session_->Commands().Revision();
        if(history==p.reconcileRevision && present==p.ownerPresentLast)return;
        auto service=p.service;auto lifecycle=session_->Scenes().Revision();
        p.mutation=detail::QueueCreatorAssetFrameMutation([this,service,lifecycle,history,present](std::uint64_t){
            if(!session_ || lifecycle!=session_->Scenes().Revision())return;
            std::string error;if(!service->Reconcile(error))Props().status.SetText(error);
            if(Props().service==service){Props().reconcileRevision=history;Props().ownerPresentLast=present;}
        });
    };
    const bool hasTerrain=scene.terrains.GetCount()==1;
    if(hasTerrain && p.service && !scene.transforms.Contains(p.owner) && scene.terrains.GetEntity(0)!=p.terrain){
        reconcile();p.service.reset();p.owner=wi::ecs::INVALID_ENTITY;p.revision.clear();
        p.reconcileRevision=UINT64_MAX;p.ownerPresentLast=true;
    }
    if(!hasTerrain || (p.service && !scene.transforms.Contains(p.owner))){
        if(p.staging)p.staging->cancelled=true;
        p.armed=p.stroke=false;p.pending=-2;p.path.clear();
        reconcile();return;
    }
    auto terrain=scene.terrains.GetEntity(0);
    if(!p.service){
        wi::ecs::Entity owner=wi::ecs::INVALID_ENTITY;std::string error;
        if(!ResolveTerrainPropAuthoringOwner(scene,project.projectId,terrain,owner,error)){p.status.SetText(error);return;}
        auto known=p.services.find(owner);
        if(known!=p.services.end())p.service=known->second;
        else {p.service=std::make_shared<TerrainPropEditService>(scene,project.rootPath,project.projectId,owner);p.services.emplace(owner,p.service);}
        p.owner=owner;p.projectId=project.projectId;p.lifecycleRevision=session_->Scenes().Revision();
        p.revision="__refresh__";p.reload=true;
    }
    p.terrain=terrain;
    const auto revision=p.service->Revision();
    if(!p.ownerPresentLast)force=true;
    if(force || revision!=p.revision){
        TerrainPropDocument document;std::string error;
        if(!p.service->Read(document,error)){p.status.SetText(error);return;}
        p.draft=std::move(document);p.revision=revision;p.rebuild=true;
        if(p.revision.empty() && p.draft.pages.empty())p.draft.authoring.recipe.candidateStep=4;
    }
    if(p.reload){
        p.reload=false;p.choices.clear();p.picker.ClearItems();
        AssetRegistry registry;std::string error;
        if(ReadAssetRegistry(project.rootPath,project.projectId,registry,error))
            for(const auto& r:registry.records)
                if(r.sourceAvailable && r.dependencyClass==DependencyClass::ImportedContent &&
                    std::filesystem::u8path(r.projectRelativePath).extension()==ReusableAssetExtension){
                    const auto label=std::filesystem::u8path(r.projectRelativePath).stem().u8string();
                    p.choices.emplace_back(r.assetId,label);p.picker.AddItem(label);
                }
        if(!p.choices.empty())p.picker.SetSelectedWithoutCallback(0);
    }
    reconcile();
}

void StudioRenderPath::QueueTerrainPropEdit(int operation)
{
    auto& p=Props();if(!session_ || !p.service || p.staging || p.mutation.IsPending() ||
        session_->Scenes().GetScene().terrains.GetCount()!=1 ||
        session_->Scenes().GetScene().terrains.GetEntity(0)!=p.terrain)return;
    auto service=p.service;auto document=p.draft;const auto expected=p.revision;
    auto job=std::make_shared<PropStaging>();job->document=document;job->expectedRevision=expected;
    job->sceneRevision=session_->Scenes().Revision();job->commandRevision=session_->Commands().Revision();
    p.staging=job;
    const auto path=p.path;const float radius=p.radius.GetValue();const auto terrainEntity=p.terrain;
    p.status.SetText("PREPARING TERRAIN PROPS... 0%");
    p.mutation=detail::QueueCreatorAssetFrameMutation([this,job,service,operation,path,radius,terrainEntity](std::uint64_t){
        if(job->cancelled || !session_ || session_->Scenes().Revision()!=job->sceneRevision ||
            session_->Commands().Revision()!=job->commandRevision){
            job->cancelled=true;job->ready=true;return;
        }
        if(operation<0){job->ready=true;return;}
        TerrainPropTerrainSnapshot terrain;std::string error;
        if(!CaptureTerrainPropTerrain(session_->Scenes().GetScene(),terrainEntity,terrain,error)){
            job->error=error;job->ready=true;return;
        }
        TerrainPropEditRequest request;request.operation=static_cast<TerrainPropEditOperation>(operation);
        request.bounds=terrain.bounds;request.path=path;request.radius=radius;request.sampleDistance=std::max(.25f,radius*.25f);
        if(!path.empty() && (request.operation==TerrainPropEditOperation::Paint || request.operation==TerrainPropEditOperation::Erase)){
            TerrainPropBounds brush{path.front()[0]-radius,path.front()[2]-radius,path.front()[0]+radius,path.front()[2]+radius};
            for(const auto& point:path){brush.minX=std::min(brush.minX,point[0]-radius);brush.minZ=std::min(brush.minZ,point[2]-radius);
                brush.maxX=std::max(brush.maxX,point[0]+radius);brush.maxZ=std::max(brush.maxZ,point[2]+radius);}
            request.bounds={std::max(brush.minX,terrain.bounds.minX),std::max(brush.minZ,terrain.bounds.minZ),
                std::min(brush.maxX,terrain.bounds.maxX),std::min(brush.maxZ,terrain.bounds.maxZ)};
        }
        request.expectedRevision=job->expectedRevision;request.currentRevision=job->expectedRevision;
        request.progress=[job](float fraction){job->progress=fraction;return !job->cancelled;};
        Props().worker=std::async(std::launch::async,[job,request=std::move(request),terrain=std::move(terrain)]()mutable{
            TerrainPropEdit edit;
            if(BuildTerrainPropEdit(job->document.pages,job->document.authoring,job->document.nextPlacementId,
                request,terrain.sample,edit,job->error)){
                job->document.pages=std::move(edit.pages);job->document.authoring=std::move(edit.authoring);
                job->document.nextPlacementId=edit.nextPlacementId;
            }
        });
    });
}
void StudioRenderPath::TickTerrainProps()
{
    auto& p=Props();if(!p.created || !session_)return;
    SyncTerrainPropPalette();
    if(p.worker.valid() && p.worker.wait_for(std::chrono::seconds(0))==std::future_status::ready)
    {
        try {p.worker.get();}catch(const std::exception& e){if(p.staging)p.staging->error=e.what();}
        if(p.staging)p.staging->ready=true;
    }
    if(p.staging)
    {
        auto job=p.staging;
        if(job->cancelled && !p.worker.valid())job->ready=true;
        if(job->ready)
        {
            if(job->cancelled)p.status.SetText("CANCELLED // Previous display retained");
            else if(!job->error.empty())p.status.SetText(job->error);
            else if(session_->Scenes().Revision()!=job->sceneRevision || session_->Commands().Revision()!=job->commandRevision)
                p.status.SetText("SCENE CHANGED // Staged props discarded. Apply again.");
            else
            {
                auto service=p.service;
                p.mutation=detail::QueueCreatorAssetFrameMutation([this,job,service](std::uint64_t){
                    if(job->cancelled || !session_ || session_->Scenes().Revision()!=job->sceneRevision ||
                        session_->Commands().Revision()!=job->commandRevision || gizmoDragActive_ ||
                        gizmo_.IsInteracting() || terrainStrokeActive_ || riverGizmoPreview_){
                        Props().status.SetText("SCENE CHANGED // Staged props discarded. Apply again.");return;
                    }
                    auto command=service->MakeCommand(job->document,job->expectedRevision);
                    if(command && session_->Commands().Execute(std::move(command))){
                        Props().revision="__refresh__";SyncTerrainPropPalette();RefreshStatus();RefreshHierarchy();
                        Props().status.SetText("TERRAIN PROPS // APPLIED // One Undo operation");
                    }else Props().status.SetText(service->LastError());
                });
            }
            p.staging.reset();
        }
        else p.status.SetText("PREPARING TERRAIN PROPS... "+std::to_string(int(job->progress.load()*100))+"%");
    }
    if(p.addPending)
    {
        p.addPending=false;
        int index=p.picker.GetSelected();
        if(index>=0 && size_t(index)<p.choices.size()){
            const auto& choice=p.choices[index];
            const auto existing=std::find_if(p.draft.profiles.begin(),p.draft.profiles.end(),[&](const auto& profile){return profile.modelAssetId==choice.first;});
            const auto id=existing!=p.draft.profiles.end()?existing->id:ProfileId(choice.first);
            auto& entries=p.draft.authoring.recipe.mix.entries;
            if(std::none_of(entries.begin(),entries.end(),[&](const auto& e){return e.profileId==id;})){
                if(std::none_of(p.draft.profiles.begin(),p.draft.profiles.end(),[&](const auto& e){return e.id==id;})){
                    TerrainPropProfile profile;profile.id=id;profile.modelAssetId=choice.first;
                    profile.collision.shape=TerrainPropShape::None;p.draft.profiles.push_back(profile);
                }
                TerrainPropMixEntry entry;entry.profileId=id;entry.minSpacing=p.spacing.GetValue();entries.push_back(entry);p.rebuild=true;
            }
        } else p.status.SetText("Import a static model, then Refresh Models.");
    }
    if(p.rebuild)RebuildTerrainPropRows();
    if(p.pending!=-2 && !p.staging && !p.mutation.IsPending()){
        int operation=p.pending;p.pending=-2;
        if(operation==-1){
            auto mix=p.draft.authoring.recipe.mix;mix.name=p.mixName.GetText();
            auto& saved=p.draft.authoring.savedMixes;
            auto found=std::find_if(saved.begin(),saved.end(),[&](const auto& m){return m.name==mix.name;});
            if(found==saved.end())saved.push_back(mix);else *found=mix;
        }
        QueueTerrainPropEdit(operation);
    }
    if(!terrainWorkspaceActive_){p.armed=false;if(p.stroke){p.stroke=false;p.path.clear();}}
    RefreshTerrainPropControls();
}
bool StudioRenderPath::HandleTerrainProps(const XMFLOAT4& pointer)
{
    auto& p=Props();
    if(wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE)){
        if(p.staging)p.staging->cancelled=true;p.mutation.Cancel();p.stroke=p.armed=false;p.pending=-2;p.path.clear();
        p.paint.SetText("PAINT");p.erase.SetText("ERASE");
        p.status.SetText("TERRAIN PROPS // CANCELLED");
    }
    if(!session_ || !p.armed || !terrainWorkspaceActive_)return false;
    if(p.stroke && wi::input::Release(wi::input::MOUSE_BUTTON_LEFT)){
        p.stroke=false;p.pending=int(p.mode);return true;
    }
    if(p.staging || p.mutation.IsPending())return IsPointerOverViewport(pointer) && wi::input::Down(wi::input::MOUSE_BUTTON_LEFT);
    if(flyCameraActive_ || GetGUI().HasFocus() || !SceneOverlayInputAllowed(pointer) || !camera)return false;
    auto ray=wi::renderer::GetPickRay(long(pointer.x),long(pointer.y),*this,*camera);
    auto picked=wi::scene::Pick(ray,wi::enums::FILTER_TERRAIN,~0u,session_->Scenes().GetScene());
    if(picked.entity==wi::ecs::INVALID_ENTITY)return p.stroke;

    wi::renderer::DrawSphere(wi::primitive::Sphere(picked.position,p.radius.GetValue()),
        p.mode==TerrainPropEditOperation::Erase?XMFLOAT4(.95f,.25f,.1f,.3f):XMFLOAT4(1,.5f,.1f,.3f),true);
    if(wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)){p.stroke=true;p.path.clear();}
    if(p.stroke && wi::input::Down(wi::input::MOUSE_BUTTON_LEFT)){
        std::array<float,3> point{picked.position.x,picked.position.y,picked.position.z};
        if(p.path.empty() || std::hypot(point[0]-p.path.back()[0],point[2]-p.path.back()[2])>=.05f)p.path.push_back(point);
    }
    return p.stroke;
}
}

namespace renegade::studio
{
    bool StudioRenderPath::IsTerrainPropBrushActive() const
    {return palette && palette->armed;}
}

namespace renegade::studio
{
void StudioRenderPath::DisableTerrainPropBrush()
{
    if(!palette)return;palette->armed=palette->stroke=false;palette->path.clear();
    palette->paint.SetText("PAINT");palette->erase.SetText("ERASE");
}
// Explicit disposable-project controller proof. No effect without both opt-ins.
// It exercises the ordinary palette staging, command and document boundaries;
// this scripted check is separate from human pointer/layout acceptance.
void StudioRenderPath::TickTerrainPropProof()
{
    const char* descriptor=std::getenv("RENEGADE_TERRAIN_PROP_PROOF_PROJECT");
    const char* logfile=std::getenv("RENEGADE_TERRAIN_PROP_PROOF_LOG");
    if(!descriptor || !logfile || !session_ || !camera)return;
    static int stage=0,frames=0,settle=0;static size_t generated=0,erased=0;
    static std::string generatedRevision, erasedRevision;
    static auto previousTick=std::chrono::steady_clock::now();
    const auto now=std::chrono::steady_clock::now();
    const double intervalMs=std::chrono::duration<double,std::milli>(now-previousTick).count();previousTick=now;
    static std::vector<double> baselineIntervals,populatedIntervals;
    static int populatedWarmup=0;
    if(stage<0 || stage>=20)return;
    auto log=[logfile](const std::string& text){std::ofstream out(logfile,std::ios::app);out<<text<<std::endl;};
    auto require=[](bool okay,const std::string& message){if(!okay)throw std::runtime_error(message);};
    auto reportFrames=[this,log](std::vector<double> intervals,const char* label){
        std::sort(intervals.begin(),intervals.end());
        const auto& texture=GetRenderResult3D();
        log(std::string("FRAME_INTERVAL ")+label+" samples="+std::to_string(intervals.size())+
            " median_ms="+std::to_string(intervals[intervals.size()/2])+" p95_ms="+std::to_string(intervals[intervals.size()*95/100])+
            " native_objects="+std::to_string(session_->Scenes().GetScene().objects.GetCount())+
            " viewport="+std::to_string(texture.desc.width)+"x"+std::to_string(texture.desc.height)+
            " camera="+std::to_string(camera->Eye.x)+","+std::to_string(camera->Eye.y)+","+std::to_string(camera->Eye.z));
    };
    auto count=[](const TerrainPropDocument& d){size_t n=0;for(const auto& page:d.pages)n+=page.placements.size();return n;};
    try
    {
        if(++frames>6000)throw std::runtime_error("P2 Studio proof exceeded frame budget");
        if(stage==0){log("SCRIPTED_STUDIO_PROOF_BEGIN");BeginProjectLoad(descriptor);stage=1;return;}
        if(!session_->Projects().HasProject() || projectLoadingOverlay_.IsBlocking())return;
        const auto& project=session_->Projects().CurrentProject();
        require(std::filesystem::weakly_canonical(project.descriptorPath)==std::filesystem::weakly_canonical(descriptor),
            "Proof refused a different project");
        auto& p=Props();auto& scene=session_->Scenes().GetScene();
        if(stage==1)
        {
            // The opt-in descriptor must refer to a dedicated disposable project.
            require(std::filesystem::path(project.rootPath).filename().u8string().find("TerrainProof")!=std::string::npos,
                "Use a disposable TerrainProof project for this destructive QA fixture");
            SetProjectHubVisible(false);SetTerrainWorkspaceActive(true);
            if(const char* text=std::getenv("RENEGADE_TERRAIN_PROP_PROOF_INSPECTOR_WIDTH")){
                char* end=nullptr;const float width=std::strtof(text,&end);
                require(end!=text && *end==0 && std::isfinite(width) && width>=280 && width<=520,
                    "Proof inspector width must be a finite number from 280 to 520");
                studioChrome_.SetPanelSizes(studioChrome_.HierarchyWidth(),width,studioChrome_.DrawerHeight());
                ResizeLayout();log("PROOF_INSPECTOR_WIDTH "+std::to_string(studioChrome_.InspectorWidth()));
            }
            p.mutation=detail::QueueCreatorAssetFrameMutation([this](std::uint64_t){
                auto& s=session_->Scenes().GetScene();
                if(!s.weathers.GetCount())(void)bridge::CreateEnvironment(s,WeatherState{});
                if(!s.terrains.GetCount()){
                    TerrainState terrain;terrain.visibleChunkRadius=1;terrain.physicsChunkRadius=2;
                    terrain.minimumHeight=-1;terrain.maximumHeight=2;(void)bridge::CreateTerrain(s,terrain,"P2 Review Terrain");
                }
                auto& light=s.lights.Create(wi::ecs::CreateEntity());light.SetType(wi::scene::LightComponent::DIRECTIONAL);light.intensity=3;
                editorCameraTransform_.ClearTransform();
                auto view=DirectX::XMMatrixLookAtLH(DirectX::XMVectorSet(18,22,-32,1),
                    DirectX::XMVectorSet(0,0,0,1),DirectX::XMVectorSet(0,1,0,0));
                editorCameraTransform_.MatrixTransform(DirectX::XMMatrixInverse(nullptr,view));
                editorCameraTransform_.UpdateTransform();camera->TransformCamera(editorCameraTransform_);
            });stage=2;settle=0;return;
        }
        if(p.staging || p.mutation.IsPending() || p.pending!=-2)return;
        if(stage==2)
        {
            if(++settle<90)return;
            SyncTerrainPropPalette();require(p.service!=nullptr,"Palette service unavailable");
            if(p.mutation.IsPending())return;
            if(baselineIntervals.size()<120){baselineIntervals.push_back(intervalMs);return;}
            reportFrames(baselineIntervals,"baseline");
            require(p.choices.size()>=3,"Fixture needs tree, bush, boulder registered by P1 SliceProof");
            p.draft.authoring.recipe.candidateStep=2;p.draft.authoring.recipe.seed=3926;
            p.draft.authoring.recipe.mix.name="P2 woodland";p.draft.authoring.recipe.mix.entries.clear();
            for(size_t i=0;i<3;++i){
                const auto& choice=p.choices[i];TerrainPropProfile profile;profile.id=ProfileId(choice.first);profile.modelAssetId=choice.first;
                auto existing=std::find_if(p.draft.profiles.begin(),p.draft.profiles.end(),[&](const auto& p){return p.modelAssetId==choice.first;});
                if(existing!=p.draft.profiles.end())profile=*existing;
                if(std::none_of(p.draft.profiles.begin(),p.draft.profiles.end(),[&](const auto& p){return p.id==profile.id;}))p.draft.profiles.push_back(profile);
                TerrainPropMixEntry entry;entry.profileId=profile.id;entry.densityPerSquareMetre=.01f;entry.minSpacing=2;
                p.draft.authoring.recipe.mix.entries.push_back(entry);
            }
            p.draft.authoring.savedMixes.push_back(p.draft.authoring.recipe.mix);p.rebuild=true;
            RebuildTerrainPropRows();
            CommitDensity(0,.25f,.5f);
            require(p.draft.authoring.recipe.mix.entries[0].densityPerSquareMetre==.25f && p.rows[0]->density.GetValue()==.25f,
                "Density handler failed to clamp out-of-range numeric value");
            CommitDensity(0,.25f,.01f);log("DENSITY_HANDLER_CLAMP_PASS input=.5 canonical=.25 step=2 (shared callback handler)");
            terrainSectionExpanded_.fill(false);terrainSectionExpanded_[15]=true;
            RefreshInspector();ResizeLayout();QueueTerrainPropEdit(int(TerrainPropEditOperation::Generate));stage=3;return;
        }
        if(stage==3)
        {
            require(p.status.GetText().find("APPLIED")!=std::string::npos,"Generate failed: "+p.status.GetText());
            generated=count(p.draft);require(generated>0,"Generate produced no placements");generatedRevision=p.revision;
            if(++populatedWarmup<60)return;
            if(populatedIntervals.size()<120){populatedIntervals.push_back(intervalMs);return;}
            reportFrames(populatedIntervals,"populated");
            log("GENERATE_APPLY_PASS placements="+std::to_string(generated));
            require(wi::helper::saveTextureToFile(GetRenderResult3D(),(std::filesystem::path(logfile).parent_path()/"studio-generated.png").u8string()),"Capture failed");
            p.path={{{-4,0,-4}},{{4,0,4}}};p.radius.SetValue(12);QueueTerrainPropEdit(int(TerrainPropEditOperation::Erase));stage=4;return;
        }
        if(stage==4)
        {
            erased=count(p.draft);require(erased<generated,"Erase did not remove placements");erasedRevision=p.revision;
            require(!p.draft.authoring.clearings.empty(),"Erase failed to retain clearing");
            log("ERASE_PASS placements="+std::to_string(erased));
            p.mutation=detail::QueueCreatorAssetFrameMutation([this,log](std::uint64_t){
                try {
                    if(!session_->Commands().Undo() || !session_->Commands().Redo())throw std::runtime_error("Erase Undo/Redo failed");
                }catch(const std::exception& e){log(std::string("SCRIPTED_STUDIO_PROOF_FAIL ")+e.what());stage=-1;}
            });stage=5;return;
        }
        if(stage==5)
        {
            require(count(p.draft)==erased,"Redo population mismatch");
            QueueTerrainPropEdit(int(TerrainPropEditOperation::Generate));stage=6;return;
        }
        if(stage==6)
        {
            require(count(p.draft)==erased,"Generate refilled manual clearing");
            log("UNDO_REDO_REGENERATE_CLEARING_PASS");
            for(auto& e:p.draft.authoring.recipe.mix.entries)e.densityPerSquareMetre=.02f;
            p.path={{{-20,0,-20}},{{20,0,-20}}};p.radius.SetValue(8);
            QueueTerrainPropEdit(int(TerrainPropEditOperation::Paint));stage=7;return;
        }
        if(stage==7)
        {
            require(count(p.draft)>erased,"Paint did not add placements");log("PAINT_PASS placements="+std::to_string(count(p.draft)));
            QueueTerrainPropEdit(int(TerrainPropEditOperation::Generate));p.staging->cancelled=true;p.mutation.Cancel();stage=8;return;
        }
        if(stage==8)
        {
            log("CANCEL_STAGING_PASS");
            p.mutation=detail::QueueCreatorAssetFrameMutation([this,log,require](std::uint64_t){
                try {
                const auto& project=session_->Projects().CurrentProject();
                auto path=(std::filesystem::path(project.rootPath)/"Content/Scenes/P2Review.wiscene").u8string();
                require(session_->Documents().Save(path),"Save As failed");
                log("SAVE_AS_PASS path="+path);
                auto prepared=session_->Documents().PrepareOpen(path);
                require(session_->Documents().CommitPreparedOpen(std::move(prepared)),"Reopen failed");
                RefreshHierarchy();RefreshInspector();RefreshStatus();SyncGizmoSelection();
                log("REOPEN_PASS");
                }catch(const std::exception& e){log(std::string("SCRIPTED_STUDIO_PROOF_FAIL ")+e.what());stage=-1;}
            });stage=9;return;
        }
        if(stage==9)
        {
            SyncTerrainPropPalette();require(count(p.draft)>0,"Reopen lost population");
            require(!p.draft.authoring.clearings.empty(),"Reopen lost clearings");
            SetProjectHubVisible(false);SetTerrainWorkspaceActive(true);terrainSectionExpanded_[15]=true;
            editorCameraTransform_.ClearTransform();
            auto view=DirectX::XMMatrixLookAtLH(DirectX::XMVectorSet(18,22,-32,1),
                DirectX::XMVectorSet(0,0,0,1),DirectX::XMVectorSet(0,1,0,0));
            editorCameraTransform_.MatrixTransform(DirectX::XMMatrixInverse(nullptr,view));
            editorCameraTransform_.UpdateTransform();camera->TransformCamera(editorCameraTransform_);camera->UpdateCamera();
            RefreshInspector();RefreshStatus();ResizeLayout();
            log("SCRIPTED_STUDIO_PROOF_PASS placements="+std::to_string(count(p.draft))+" project="+project.descriptorPath);
            stage=20;
        }
    }
    catch(const std::exception& e){log(std::string("SCRIPTED_STUDIO_PROOF_FAIL ")+e.what());stage=-1;}
}
}
