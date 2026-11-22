#include "StudioApplication.h"
#include "renegade/bridge/GroundPaint.h"
#include <algorithm>
#include <cmath>

namespace renegade::studio
{
    namespace
    {
        using bridge::GroundTool;
        struct GroundThumb final : RenegadeButton
        {
            wi::Resource image;
            void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const override
            {
                RenegadeButton::Render(canvas,cmd);
                if(!IsVisible() || !image.IsValid())return;
                wi::image::Params rect(translation.x+4,translation.y+4,scale.x-8,scale.y-8,wi::Color::White());
                wi::image::Draw(&image.GetTexture(),rect,cmd);
            }
            const char* GetWidgetTypeName() const override {return "GroundThumbnail";}
        };
        struct Panel
        {
            SceneInspectorComboBox group;
            SceneInspectorSlider size,strength,edge,tile,grass,slopeStart,slopeEnd,heightStart,heightEnd;
            SceneInspectorButton paint,erase,soften,base,import,options;
            SceneInspectorCheckBox height;
            wi::gui::Label status;
            std::vector<std::unique_ptr<GroundThumb>> thumbs;
            std::vector<bridge::GroundMaterial> materials;
            std::string project;
            uint64_t history=0;wi::ecs::Entity loadedTerrain=wi::ecs::INVALID_ENTITY;
            size_t selected=0;
            int filter=0;
            bool armed=false,reload=true,optionsOpen=false;
            GroundTool tool=GroundTool::Paint;
            wi::ecs::Entity terrain=wi::ecs::INVALID_ENTITY;
            std::unique_ptr<bridge::GroundStroke> stroke;
        };
        Panel& Ground(){static Panel p;return p;}
        void SelectGround(size_t index)
        {
            auto& p=Ground();if(index>=p.materials.size())return;
            p.selected=index;
            for(size_t i=0;i<p.materials.size();++i)p.thumbs[i]->SetAccentOutlineStyle(i==index);
            auto& m=p.materials[index];p.tile.SetValue(m.tileMetres);p.grass.SetValue(100*m.grassCoverage);
            p.status.SetText(m.name);
        }
    }
    void StudioRenderPath::CreateGroundPanel()
    {
        auto& p=Ground();auto add=[&](wi::gui::Widget& w){inspectorPanel_.AddWidget(&w);w.SetVisible(false);};
        p.group.Create("Ground material group");for(auto label:{"All ground materials","Stock","Custom"})p.group.AddItem(label);
        p.group.SetSelectedWithoutCallback(0);
        p.group.OnSelect([this](const wi::gui::EventArgs& a){auto& p=Ground();p.filter=a.iValue;p.reload=true;QueueInspectorRefresh();});
        add(p.group);
        auto slider=[&](auto& w,const char* name,float lo,float hi,float value,float steps)
        {w.Create(lo,hi,value,steps,name,name);add(w);};
        slider(p.size,"BRUSH RADIUS (M)",.5f,100,6,199);slider(p.strength,"STRENGTH (%)",1,100,75,99);
        slider(p.edge,"EDGE SOFTNESS (%)",0,100,65,100);slider(p.tile,"TILE SIZE (M)",.25f,64,4,255);
        slider(p.grass,"GRASS COVERAGE (%)",0,100,100,100);
        slider(p.slopeStart,"ROCK START ANGLE",0,89,32,89);slider(p.slopeEnd,"FULL ROCK ANGLE",1,90,55,89);
        slider(p.heightStart,"ROCK START HEIGHT (M)",-200,1000,0,1200);slider(p.heightEnd,"FULL ROCK HEIGHT (M)",-199,1001,30,1200);
        p.height.Create("Use height for exposed rock");p.height.SetText("USE HEIGHT FOR ROCK");add(p.height);
        auto button=[&](auto& w,const char* name){w.Create(name);w.SetText(name);add(w);};
        button(p.paint,"PAINT");button(p.erase,"ERASE MATERIAL");button(p.soften,"SOFTEN EDGE");
        button(p.base,"SET AS BASE");button(p.import,"IMPORT GROUND MATERIAL");button(p.options,"+  AUTOMATIC ROCK");
        p.options.OnClick([this](const wi::gui::EventArgs&){auto& p=Ground();p.optionsOpen=!p.optionsOpen;
            p.options.SetText(p.optionsOpen?"-  AUTOMATIC ROCK":"+  AUTOMATIC ROCK");QueueInspectorRefresh();});
        p.status.Create("Ground brush status");p.status.SetText("Select a ground material.");add(p.status);
        for(auto pair:{std::make_pair(&p.paint,GroundTool::Paint),std::make_pair(&p.erase,GroundTool::Erase),std::make_pair(&p.soften,GroundTool::Soften)})
            pair.first->OnClick([this,tool=pair.second](const wi::gui::EventArgs&){
                auto& p=Ground();const bool toggle=p.armed && p.tool==tool;
                DisableTerrainPropBrush();DisableWd01VegetationBrush();terrainRiverBrushActive_=false;terrainSculptMode_.SetSelectedWithoutCallback(0);
                p.armed=!toggle;p.tool=tool;
                p.paint.SetAccentOutlineStyle(p.armed && tool==GroundTool::Paint);p.erase.SetAccentOutlineStyle(p.armed && tool==GroundTool::Erase);
                p.soften.SetAccentOutlineStyle(p.armed && tool==GroundTool::Soften);
            });
        auto settings=[this](float){
            auto& p=Ground();if(!session_ || p.selected>=p.materials.size())return;
            auto& m=p.materials[p.selected];m.tileMetres=p.tile.GetValue();m.grassCoverage=p.grass.GetValue()/100.f;
            auto& scene=session_->Scenes().GetScene();auto id=bridge::FindGroundMaterial(scene,m);
            if(scene.terrains.GetCount() && id!=wi::ecs::INVALID_ENTITY)
                session_->Commands().Execute(bridge::SetGroundSettings(scene,scene.terrains.GetEntity(0),id,m.tileMetres,m.grassCoverage));
        };
        p.grass.OnValueCommitted(settings);p.tile.OnValueCommitted(settings);
        p.base.OnClick([this](const wi::gui::EventArgs&){
            auto& p=Ground();if(!session_ || p.selected>=p.materials.size())return;
            auto& s=session_->Scenes().GetScene();if(!s.terrains.GetCount())return;
            const auto e=s.terrains.GetEntity(0);std::string error;
            auto m=p.materials[p.selected];m.tileMetres=p.tile.GetValue();m.grassCoverage=p.grass.GetValue()/100.f;
            bridge::GroundRules r{p.slopeStart.GetValue(),p.slopeEnd.GetValue(),p.heightStart.GetValue(),p.heightEnd.GetValue(),p.height.GetCheck()};
            if(r.rockStart>=r.rockFull || (r.highEnabled && r.highStart>=r.highFull)){p.status.SetText("Full rock must be above the start value.");return;}
            const auto id=bridge::ResolveGroundMaterial(s,m,std::filesystem::u8path(p.project),error);
            const auto rock=bridge::ResolveGroundMaterial(s,p.materials.at(4),std::filesystem::u8path(p.project),error);
            bool ok=id && rock && session_->Commands().Execute(bridge::SetGroundBase(s,e,id,rock,r));
            p.status.SetText(ok?"Base ground: "+m.name:error);RefreshStatus();
        });
        p.import.SetTooltip("Choose the colour map. Include matching _normal_dx and _ORM, or separate _roughness, _metallic and _AO maps.");
        p.import.OnClick([this](const wi::gui::EventArgs&){
            const auto project=Ground().project;if(project.empty())return;
            wi::helper::FileDialogParams params;params.type=wi::helper::FileDialogParams::OPEN;
            params.extensions={"png","jpg","tga","dds"};params.description="Ground material colour map";
            wi::helper::FileDialog(params,[this,project](const std::string& path){
                wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this,project,path](uint64_t){
                    auto& p=Ground();if(path.empty() || p.project!=project || !session_ || !session_->Projects().HasProject() ||
                        session_->Projects().CurrentProject().rootPath!=project)return;
                    bridge::GroundMaterial result;std::string error;
                    if(bridge::ImportGroundMaterial(std::filesystem::u8path(project),std::filesystem::u8path(path),result,error))
                    {p.filter=2;p.group.SetSelectedWithoutCallback(2);p.reload=true;QueueInspectorRefresh();}
                    else p.status.SetText(error);
                });
            });
        });
    }
    void StudioRenderPath::StopGroundPaint()
    {
        auto& p=Ground();
        if(p.stroke && p.stroke->Changed() && session_ && session_->Scenes().GetScene().terrains.Contains(p.terrain) && session_->Projects().HasProject() && session_->Projects().CurrentProject().rootPath==p.project)
            session_->Commands().RecordExecuted(std::move(p.stroke));
        p.stroke.reset();p.armed=false;
        for(auto* w:{&p.paint,&p.erase,&p.soften})w->SetAccentOutlineStyle(false);
    }
    std::vector<wi::gui::Widget*> StudioRenderPath::GroundPaintWidgets()
    {
        auto& p=Ground();const bool active=vegetationWorkspace_ && terrainWorkspaceActive_ && session_ && session_->Projects().HasProject();
        if(active)
        {
            const auto root=session_->Projects().CurrentProject().rootPath;
            auto& currentScene=session_->Scenes().GetScene();
            const auto loaded=currentScene.terrains.GetCount()?currentScene.terrains.GetEntity(0):wi::ecs::INVALID_ENTITY;
            if(root!=p.project || p.reload || p.loadedTerrain!=loaded)
            {
                auto previous=p.selected<p.materials.size()?p.materials[p.selected].colour:std::filesystem::path{};
                if(root!=p.project){StopGroundPaint();previous.clear();}
                p.project=root;p.loadedTerrain=loaded;p.reload=false;p.materials=bridge::GroundPalette(std::filesystem::u8path(root));
                size_t selected=p.filter==2 && p.materials.size()>10?10:0;
                for(size_t i=0;i<p.materials.size();++i)
                {
                    if(i==p.thumbs.size())
                    {
                        auto w=std::make_unique<GroundThumb>();w->Create("Ground thumbnail "+std::to_string(i));w->SetText("");
                        w->OnClick([i](const wi::gui::EventArgs&){SelectGround(i);});inspectorPanel_.AddWidget(w.get());p.thumbs.push_back(std::move(w));
                    }
                    auto& m=p.materials[i];bridge::ReadGroundSettings(session_->Scenes().GetScene(),m);p.thumbs[i]->image=wi::resourcemanager::Load(m.colour.generic_u8string());p.thumbs[i]->SetTooltip(m.name);
                    if(m.colour==previous && (p.filter==0 || (p.filter==2)==(i>=10)))selected=i;
                }
                if(p.filter==2 && p.materials.size()==10){p.selected=SIZE_MAX;p.status.SetText("Import a custom ground material.");}
                else SelectGround(selected);
                auto& scene=session_->Scenes().GetScene();
                if(scene.terrains.GetCount()){
                    const auto r=bridge::ReadGroundRules(scene,scene.terrains.GetEntity(0));
                    p.slopeStart.SetValue(r.rockStart);p.slopeEnd.SetValue(r.rockFull);p.height.SetCheck(r.highEnabled);
                    p.heightStart.SetValue(r.highStart);p.heightEnd.SetValue(r.highFull);
                }
            }
            if(p.history!=session_->Commands().Revision() && p.selected<p.materials.size() && !p.tile.IsDragging() && !p.grass.IsDragging())
            {
                p.history=session_->Commands().Revision();
                auto& m=p.materials[p.selected];bridge::ReadGroundSettings(currentScene,m);
                p.tile.SetValue(m.tileMetres);p.grass.SetValue(m.grassCoverage*100);
            }
        }
        std::vector<wi::gui::Widget*> rows{&p.group};
        for(size_t i=0;i<p.thumbs.size();++i)
            if(i<p.materials.size() && (p.filter==0 || (p.filter==2)==(i>=10)))rows.push_back(p.thumbs[i].get());else p.thumbs[i]->SetVisible(false);
        rows.insert(rows.end(),{&p.status,&p.paint,&p.erase,&p.soften,&p.size,&p.strength,&p.edge,&p.tile,&p.grass,&p.import,&p.options});
        std::vector<wi::gui::Widget*> rules{&p.slopeStart,&p.slopeEnd,&p.height,&p.heightStart,&p.heightEnd};
        for(auto* w:rules)if(p.optionsOpen)rows.push_back(w);else w->SetVisible(false);
        rows.push_back(&p.base);
        return rows;
    }
    bool StudioRenderPath::HandleGroundPaint(const XMFLOAT4& pointer)
    {
        auto& p=Ground();if(!p.armed || !session_)return false;
        auto& scene=session_->Scenes().GetScene();
        if(!vegetationWorkspace_ || !terrainWorkspaceActive_ || !scene.terrains.GetCount() || !session_->Projects().HasProject() ||
            p.project!=session_->Projects().CurrentProject().rootPath){StopGroundPaint();return false;}
        if(p.stroke && !wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))
        {
            if(p.stroke->Changed())session_->Commands().RecordExecuted(std::move(p.stroke));
            p.stroke.reset();RefreshStatus();return false;
        }
        if(flyCameraActive_ || GetGUI().HasFocus() || !IsPointerOverViewport(pointer) || !camera || p.selected>=p.materials.size())return false;
        auto ray=wi::renderer::GetPickRay(long(pointer.x),long(pointer.y),*this,*camera);
        auto hit=wi::scene::Pick(ray,wi::enums::FILTER_TERRAIN,~0u,scene);
        if(hit.entity==wi::ecs::INVALID_ENTITY)return false;
        auto ring=[&](float radius)
        {
            if(radius<=0)return;
            for(int i=0;i<48;++i)
            {
                auto vertex=[&](int j){float a=DirectX::XM_2PI*float(j)/48;const float x=radius*std::cos(a),z=radius*std::sin(a);
                    return XMFLOAT3{hit.position.x+x,hit.position.y+.06f-(hit.normal.x*x+hit.normal.z*z)/std::max(.15f,hit.normal.y),hit.position.z+z};};
                wi::renderer::RenderableLine line;line.start=vertex(i);line.end=vertex(i+1);
                line.color_start=line.color_end={1,.62f,.18f,1};wi::renderer::DrawLine(line,false);
            }
        };
        ring(p.size.GetValue());ring(p.size.GetValue()*(1-p.edge.GetValue()/100.f));
        if(wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
        {
            auto m=p.materials[p.selected];m.tileMetres=p.tile.GetValue();m.grassCoverage=p.grass.GetValue()/100.f;
            std::string error;auto id=bridge::ResolveGroundMaterial(scene,m,std::filesystem::u8path(p.project),error);
            if(id==wi::ecs::INVALID_ENTITY){p.status.SetText(error);return true;}
            p.terrain=scene.terrains.GetEntity(0);
            p.stroke=std::make_unique<bridge::GroundStroke>(scene,p.terrain,id,p.tool,p.size.GetValue(),p.strength.GetValue()/100.f,p.edge.GetValue()/100.f);
        }
        if(p.stroke && wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))
        {
            p.stroke->Dab(hit.position);if(!p.stroke->Error().empty())p.status.SetText(p.stroke->Error());return true;
        }
        return false;
    }
}
