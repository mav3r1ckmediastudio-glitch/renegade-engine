#include "StudioApplication.h"
#include "InspectorSectionFramework.h"
#include "renegade/bridge/RiverTerrainLayerService.h"
#include "renegade/bridge/RiverWaterwayService.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
namespace renegade::studio
{
namespace
{
    constexpr int TerrainParents[15]={-1,0,0,0,-1,4,4,4,4,4,4,-1,11,11,11};
    constexpr const char* TerrainIds[15]={"terrain","terrain.size","terrain.sculpt","terrain.materials",
        "rivers","rivers.paint","rivers.bed","rivers.flow","rivers.appearance","rivers.waterfalls","rivers.advanced",
        "vegetation","vegetation.paint","vegetation.appearance","vegetation.simulation"};
    constexpr const char* TerrainTitles[15]={"TERRAIN","SIZE & HEIGHT RANGE","SCULPTING","MATERIALS & BLENDING",
        "RIVERS","PAINTING","RIVERBED & BANKS","FLOW & DIRECTION","WATER COLOUR & CLARITY","WATERFALLS","ADVANCED",
        "VEGETATION","PAINTING","APPEARANCE","SIMULATION & DETAIL"};
    void Place(wi::gui::Widget& widget,float x,float y,float width,float height=28)
    {widget.SetPos(XMFLOAT2(x,y));widget.SetSize(XMFLOAT2(std::max(1.f,width),height));}
}
void StudioRenderPath::LayoutTerrainSections()
{
    const bool active=session_ && terrainWorkspaceActive_ && !renderWorkspaceActive_ && !projectHubVisible_;
    const bool hasTerrain=active && session_->Scenes().GetScene().terrains.GetCount()>0;
    wi::ecs::Entity selectedRiver = wi::ecs::INVALID_ENTITY;
    if(session_)
    {
        const auto& scene=session_->Scenes().GetScene();
        const auto selected=session_->Selection().SelectedEntity();
        for(std::size_t i=0;i<scene.splines.GetCount();++i)
        {
            const auto root=scene.splines.GetEntity(i);
            const auto* water=scene.materials.GetComponent(root);
            if(water && water->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER &&
                (root==selected || scene.Entity_IsDescendant(selected,root)))
            { selectedRiver=root; break; }
        }
    }
    // SELECT exposes the properties of the selected river directly. Terrain
    // continues to show all creation/painting tools and retains its own folds.
    const bool selectedMode=session_ && selectedRiver!=wi::ecs::INVALID_ENTITY &&
        !terrainWorkspaceActive_ && !environmentWorkspaceActive_ &&
        !renderWorkspaceActive_ && !projectHubVisible_;
    selectedRiverInspectorActive_=selectedMode;
    const float width=std::max(1.f,inspectorPanel_.GetSize().x-24.f);
    std::array<std::vector<wi::gui::Widget*>,15> rows;
    rows[1]={&terrainSizeReadout_,&expandTerrainButton_,&terrainChunkScale_,&terrainMinimumHeight_,&terrainMaximumHeight_,&terrainLodBias_};
    rows[2]={&terrainSculptMode_,&terrainBrushRadius_,&terrainBrushStrength_,&terrainBrushFalloff_,&terrainBrushReadout_,&terrainStrokeDiagnostic_};
    rows[3]={&terrainMaterialPreset_,&terrainTextureScale_,&terrainLowAltitudeBlend_,&terrainBaseBlend_,&terrainSlopeBlend_,&terrainApplyDefaultGrassButton_,&terrainReloadMaterialButton_};
    rows[4]={&terrainRiverStatus_};
    rows[5]={&terrainRiverBrushButton_,&terrainRiverWidth_};
    rows[6]={&terrainRiverFitButton_,&terrainRiverReflectionAlignButton_,&terrainRiverRockyBedButton_,&terrainRiverCarveDepth_,&terrainRiverCarveBank_,&terrainRiverCarveButton_,&terrainRiverUnlinkButton_};
    if(selectedMode) rows[6].insert(rows[6].begin(),&terrainRiverStatus_);
    rows[7]={&terrainRiverFlowDirection_,&terrainRiverFlowSpeed_,&terrainRiverFlowSlowerButton_,&terrainRiverFlowFasterButton_,&terrainRiverFlowReverseButton_};
    SyncRiverFlowV2(selectedRiver);
    bridge::RiverFlowProfile v2;std::string v2Error;
    if(session_){bridge::RiverWaterwayControls waterControls;
        if(bridge::ReadRiverWaterwayControls(session_->Scenes().GetScene(),selectedRiver,waterControls,v2Error))
            bridge::ReadRiverFlowProfile(session_->Scenes().GetScene(),waterControls.owner,v2,v2Error);
    }
    auto legacyFlow=rows[7];
    rows[7]={&riverDrawTributary_,&riverFlowEnable_,&riverFlowArrows_,&riverFlowStatus_,
        &riverFlowStartHere_,&riverConnect_,&riverDisconnect_,&riverConnectCancel_};
    if(v2.enabled){
        rows[7].insert(rows[7].end(),{&riverFlowFoam_,&riverFlowRapids_,&riverFlowReverse_,
            &riverFlowStartFoam_,&riverFlowReset_});
        // Width is always at the top of selected river properties, not buried
        // among flow settings; source whitewater belongs in this flow section.
        for(std::size_t k=0;k<riverFlowSliders_.size();++k)
            if(k!=6)rows[7].push_back(&riverFlowSliders_[k]);
        for(auto* widget:legacyFlow)widget->SetVisible(false);
    }else rows[7].insert(rows[7].end(),legacyFlow.begin(),legacyFlow.end());
    rows[8]={&terrainRiverAppearance_[0],&terrainRiverAppearance_[1],&terrainRiverAppearance_[2],&terrainRiverAppearance_[3],&terrainRiverFogDensity_,&terrainRiverAppearance_[4]};
    rows[9]={&addWaterfallButton_};
    rows[10]={&terrainRiverExtendFloorButton_};
    rows[11]=TerrainVegetationWidgets(3);rows[12]=TerrainVegetationWidgets(0);rows[13]=TerrainVegetationWidgets(1);rows[14]=TerrainVegetationWidgets(2);
    // Keep the per-river material controls synchronized with whichever water
    // spline or child control point is selected. Do not fight an active drag.
    if(active || selectedMode)
    {
        auto& scene=session_->Scenes().GetScene();
        const auto* material=scene.materials.GetComponent(selectedRiver);
        const bool isRiver=material!=nullptr && selectedRiver!=wi::ecs::INVALID_ENTITY;
        terrainRiverFlowSpeed_.SetEnabled(isRiver);
        terrainRiverFlowDirection_.SetEnabled(isRiver);
        // The bed sliders belong to the selected persistent river, not to
        // whichever river the user edited previously. Re-read on selection or
        // history transitions; avoid parser/GPU work on every UI frame.
        const auto terrainEntity=scene.terrains.GetCount()==1
            ? scene.terrains.GetEntity(0):wi::ecs::INVALID_ENTITY;
        const auto undoCount=session_->Commands().UndoCount();
        const auto redoCount=session_->Commands().RedoCount();
        const bool syncWater=selectedRiver!=terrainRiverInspectorEntity_ ||
            terrainEntity!=terrainRiverInspectorTerrain_ ||
            undoCount!=terrainRiverInspectorUndoCount_ || redoCount!=terrainRiverInspectorRedoCount_;
        if(selectedRiver!=terrainRiverInspectorEntity_ ||
            terrainEntity!=terrainRiverInspectorTerrain_ ||
            undoCount!=terrainRiverInspectorUndoCount_ ||
            redoCount!=terrainRiverInspectorRedoCount_)
        {
            terrainRiverInspectorEntity_=selectedRiver;
            terrainRiverInspectorTerrain_=terrainEntity;
            terrainRiverInspectorUndoCount_=undoCount;
            terrainRiverInspectorRedoCount_=redoCount;
            terrainRiverInspectorLinked_=false;
            terrainRiverInspectorMetadataValid_=false;
            bridge::RiverTerrainLayerSettings settings;
            std::string error;
            const bool valid=isRiver && terrainEntity!=wi::ecs::INVALID_ENTITY &&
                bridge::ReadRiverTerrainLayerSettings(scene,terrainEntity,
                    selectedRiver,settings,error);
            terrainRiverInspectorMetadataValid_=valid;
            if(valid)
            {
                terrainRiverInspectorLinked_=settings.linked;
                if(!terrainRiverCarveDepth_.IsDragging())
                {
                    terrainRiverCarveDepthValue_=settings.depthMeters;
                    terrainRiverCarveDepth_.SetValue(settings.depthMeters);
                }
                if(!terrainRiverCarveBank_.IsDragging())
                {
                    terrainRiverCarveBankValue_=settings.bankWidthMeters;
                    terrainRiverCarveBank_.SetValue(settings.bankWidthMeters);
                }
            }
            terrainRiverStatus_.SetText(!isRiver ? "CHANNEL // SELECT A RIVER"
                : terrainEntity==wi::ecs::INVALID_ENTITY ? "CHANNEL // REQUIRE ONE TERRAIN"
                : !valid ? "CHANNEL // INVALID LAYER METADATA"
                : settings.linked ? "CHANNEL // LINKED TO TERRAIN"
                : "CHANNEL // NOT LINKED // BASE SCULPT SAFE");
        }
        const bool editableBed=isRiver && terrainRiverInspectorMetadataValid_;
        terrainRiverCarveDepth_.SetEnabled(editableBed);
        terrainRiverCarveBank_.SetEnabled(editableBed);
        terrainRiverCarveButton_.SetEnabled(editableBed);
        terrainRiverFitButton_.SetEnabled(editableBed);
        terrainRiverReflectionAlignButton_.SetEnabled(isRiver);
        terrainRiverRockyBedButton_.SetEnabled(editableBed && terrainRiverInspectorLinked_);
        terrainRiverRockyBedButton_.SetText(terrainRiverInspectorLinked_
            ? "PAINT ROCKY BED (TEST)" : "LINK RIVERBED BEFORE PAINTING");
        terrainRiverUnlinkButton_.SetEnabled(editableBed && terrainRiverInspectorLinked_);
        terrainRiverCarveButton_.SetText(terrainRiverInspectorLinked_ ? "UPDATE LINKED BED" : "LINK RIVERBED");
        for(auto& slider:terrainRiverAppearance_)slider.SetEnabled(isRiver);
        terrainRiverFogDensity_.SetEnabled(isRiver);
        for(auto* button:{&terrainRiverFlowSlowerButton_,&terrainRiverFlowFasterButton_,&terrainRiverFlowReverseButton_})
            button->SetEnabled(isRiver);
        if(isRiver && syncWater)
        {
            const std::array<float,5> channels={
                material->baseColor.x,material->baseColor.y,
                material->baseColor.z,material->baseColor.w,material->normalMapStrength};
            for(std::size_t i=0;i<channels.size();++i)
                if(!terrainRiverAppearance_[i].IsDragging())
                    terrainRiverAppearance_[i].SetValue(i == 3 ? std::clamp(1.0f-channels[i],0.0f,1.0f) : channels[i]);
            if(!terrainRiverFogDensity_.IsDragging())
                terrainRiverFogDensity_.SetValue(std::clamp(material->baseColor.w,0.f,16.f));
            if(!terrainRiverFlowDirection_.IsDragging())
            {
                float degrees=DirectX::XMConvertToDegrees(std::atan2(-material->texAnimDirection.x,-material->texAnimDirection.y));
                if(degrees<0)degrees+=360;
                terrainRiverFlowDirection_.SetValue(degrees);
            }
            if(!terrainRiverFlowSpeed_.IsDragging())
                terrainRiverFlowSpeed_.SetValue(std::clamp(std::hypot(material->texAnimDirection.x,material->texAnimDirection.y),0.00025f,0.01f));
        }
    }
    if(!terrainSectionsCreated_)
    {
        for(size_t i=0;i<terrainSectionHeaders_.size();++i)
        {
            auto& header=terrainSectionHeaders_[i];header.Create(std::string("Terrain disclosure ")+TerrainTitles[i]);
            header.SetTooltip("Expand or collapse this group. Tool settings and active brushes are preserved.");
            if(session_){ProjectInspectorSectionPreferenceStore prefs(session_->Projects());terrainSectionExpanded_[i]=prefs.ReadExpanded(TerrainIds[i],false);}
            header.OnClick([this,i](const wi::gui::EventArgs&){
                if(selectedRiverInspectorActive_ && (i==4 || (i>=6 && i<=8)))
                {
                    if(i==4) selectedRiverInspectorExpanded_=!selectedRiverInspectorExpanded_;
                    else selectedRiverSubsectionExpanded_[i-6]=!selectedRiverSubsectionExpanded_[i-6];
                }
                else
                {
                    terrainSectionExpanded_[i]=!terrainSectionExpanded_[i];
                    if(session_){ProjectInspectorSectionPreferenceStore prefs(session_->Projects());prefs.WriteExpanded(TerrainIds[i],terrainSectionExpanded_[i]);}
                }
                QueueInspectorRefresh();
            });
            inspectorPanel_.AddWidget(&header);
        }
        terrainSectionsCreated_=true;
    }
    terrainLabel_.SetVisible(false);terrainMaterialLabel_.SetVisible(false);terrainSculptLabel_.SetVisible(false);terrainRiverLabel_.SetVisible(false);
    // Resolve each row visibility once. Hide/show cycling clears native ACTIVE drag state.
    createTerrainButton_.SetVisible(active&&!hasTerrain);
    if(active&&!hasTerrain)Place(createTerrainButton_,12,44,width);
    float y=hasTerrain?44.f:84.f;
    for(size_t i=0;i<rows.size();++i)
    {
        const int parent=TerrainParents[i];
        const bool visible=selectedMode
            ? (i==4 || (i>=6 && i<=8 && selectedRiverInspectorExpanded_))
            : active&&(hasTerrain||i==4||i==9)&&(parent<0||terrainSectionExpanded_[parent]);
        const bool expanded=selectedMode
            ? (i==4 ? selectedRiverInspectorExpanded_
                : (i>=6 && i<=8 && selectedRiverSubsectionExpanded_[i-6]))
            : terrainSectionExpanded_[i];
        const bool rowsVisible=visible && expanded && !(selectedMode && i==4);
        for(auto* widget:rows[i])widget->SetVisible(rowsVisible);
        auto& header=terrainSectionHeaders_[i];header.SetVisible(visible);
        std::string title=selectedMode && i==4 ? "RIVER // SELECTED PROPERTIES" : TerrainTitles[i];
        // Render-pipeline identity belongs in the visible inspector, not
        // just a startup log. The launch flag alone is not proof of binding.
        // Show the actual selected river material's shader state.
        if(selectedMode && i==4)
        {
            const auto* water=session_->Scenes().GetScene().materials.GetComponent(selectedRiver);
            const char* requested=std::getenv("RENEGADE_RIVER_LOCAL_DEPTH_TEST");
            const bool wantTest=requested && requested[0]=='1' && requested[1]==0;
            title = v2.enabled ? "RIVER // SPLINE FLOW" : !wantTest ? "RIVER // NATIVE WATER"
                : water && water->IsCustomShader()
                    ? "RIVER // LOCAL DEPTH ACTIVE"
                    : "RIVER // TEST NOT BOUND";
        }
        if(i==4&&terrainRiverBrushActive_&&!selectedMode)title+=" (DRAW ACTIVE)";
        if(i==11&&IsWd01VegetationBrushActive())title+=" (BRUSH ACTIVE)";
        header.SetText(std::string(expanded?"-  ":"+  ")+title);
        if(!visible)continue;
        const float x=parent<0?12.f:24.f;
        Place(header,x,y,width-(x-12));y+=34;
        if(!expanded)continue;
        if(selectedMode && i==4){
            // Width editing is a fundamental spline property, NOT a Flow
            // advanced control. Display both whole-river and selected-point
            // sliders immediately under the selected river heading.
            auto& scene=session_->Scenes().GetScene();
            const auto* spline=scene.splines.GetComponent(selectedRiver);
            const auto node=session_->Selection().SelectedEntity();
            const bool selectedNode=spline &&
                std::find(spline->spline_node_entities.begin(),
                    spline->spline_node_entities.end(),node)!=spline->spline_node_entities.end();
            riverFlowSliders_[6].SetVisible(true);
            Place(riverFlowSliders_[6],24,y,width-12);y+=34;
            riverNodeWidth_.SetVisible(selectedNode);
            riverNodeWidthReset_.SetVisible(selectedNode);
            if(selectedNode){
                Place(riverNodeWidth_,24,y,width-12);y+=34;
                Place(riverNodeWidthReset_,24,y,width-12);y+=38;
            }
            y+=8;
            continue;
        }
        for(auto* widget:rows[i])
        {
            widget->SetVisible(true);
            const float height=(widget==&riverFlowStatus_||widget==&terrainRiverStatus_||widget==&terrainStrokeDiagnostic_||i==11)?40.f:28.f;
            Place(*widget,36,y,width-24,height);y+=height+6;
        }
        if(!rows[i].empty())y+=4;
    }
    if(active || selectedMode)
    {

        focusButton_.SetVisible(false);duplicateButton_.SetVisible(false);deleteButton_.SetVisible(false);
        const float half=(width-8)/2,third=(width-16)/3;
        Place(undoButton_,12,y+10,half);Place(redoButton_,20+half,y+10,half);
        std::array<wi::gui::Widget*,3> saves={&saveButton_,&saveAsButton_,&reopenButton_};
        for(size_t i=0;i<saves.size();++i)Place(*saves[i],12+i*(third+8),y+48,third);
    }
}
float StudioRenderPath::LayoutOceanSubsections(float top,float width,bool visible)
{
    constexpr const char* ids[4]={"ocean.surface","ocean.waves","ocean.wind","ocean.colour"};
    constexpr const char* titles[4]={"SURFACE & QUALITY","WAVES & ANIMATION","WIND","COLOUR & VISIBILITY"};
    const std::array<std::vector<wi::gui::Widget*>,4> rows={{
        {&oceanEnabled_,&oceanPreset_,&oceanWaterHeight_,&oceanPatchLength_,&oceanResolution_,&oceanSurfaceDetail_,&oceanDisplacementTolerance_},
        {&oceanWaveAmplitude_,&oceanChoppyScale_,&oceanTimeScale_},
        {&oceanWindAzimuth_,&oceanWindSpeed_,&oceanWindDependency_},
        {&oceanWaterRed_,&oceanWaterGreen_,&oceanWaterBlue_,&oceanWaterOpacity_,&oceanExtinctionRed_,&oceanExtinctionGreen_,&oceanExtinctionBlue_}
    }};
    if(!oceanSubsectionsCreated_)
    {
        for(size_t i=0;i<rows.size();++i)
        {
            auto& header=oceanSubsectionHeaders_[i];header.Create(std::string("Ocean disclosure ")+titles[i]);
            header.SetTooltip("Expand or collapse these Ocean settings.");
            if(session_){ProjectInspectorSectionPreferenceStore prefs(session_->Projects());oceanSubsectionExpanded_[i]=prefs.ReadExpanded(ids[i],false);}
            const std::string id=ids[i];
            header.OnClick([this,i,id](const wi::gui::EventArgs&){
                oceanSubsectionExpanded_[i]=!oceanSubsectionExpanded_[i];
                if(session_){ProjectInspectorSectionPreferenceStore prefs(session_->Projects());prefs.WriteExpanded(id,oceanSubsectionExpanded_[i]);}
                QueueInspectorRefresh();
            });
            inspectorPanel_.AddWidget(&header);
        }
        oceanSubsectionsCreated_=true;
    }
    for(size_t i=0;i<rows.size();++i)
    {
        auto& header=oceanSubsectionHeaders_[i];header.SetVisible(visible);
        header.SetText(std::string(oceanSubsectionExpanded_[i]?"-  ":"+  ")+titles[i]);
        if(visible){Place(header,24,top,width-12);top+=34;}
        for(auto* widget:rows[i])
        {
            const bool show=visible&&oceanSubsectionExpanded_[i];widget->SetVisible(show);
            if(show){Place(*widget,36,top,width-24);top+=34;}
        }
    }
    return top;
}
}
