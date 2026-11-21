#include "StudioApplication.h"

#include "renegade/bridge/VegetationService.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <filesystem>
#include <string>

namespace
{
    constexpr float VegetationTop = 980.0f; // Riverbed tools sit above vegetation.

    enum class SettingField
    {
        Density,
        Length,
        Width,
        Stiffness,
        Drag,
        Gravity,
        Randomness,
        RandomSeed,
        Segments,
        Billboards,
        ViewDistance,
        Uniformity,
    };

    using renegade::bridge::PlantGroup;
    struct PlantFilter { PlantGroup group; const char* label; };
    constexpr PlantFilter PlantFilters[] = {
        {PlantGroup::All, "All plants"}, {PlantGroup::Grass, "Grass and weeds"},
        {PlantGroup::Freshwater, "Tropical freshwater"}, {PlantGroup::Marine, "Marine"},
        {PlantGroup::Alien, "Alien"}, {PlantGroup::Custom, "Custom"}
    };

    class GrassThumbnail final : public renegade::studio::RenegadeButton
    {
    public:
        std::filesystem::path path;
        PlantGroup group = PlantGroup::Grass;
        wi::Resource preview;
        bool selected = false;
        void Render(const wi::Canvas& canvas, wi::graphics::CommandList cmd) const override
        {
            RenegadeButton::Render(canvas,cmd);
            if (!IsVisible() || !preview.IsValid()) return;
            const float side = std::min(scale.x,scale.y)-8.f;
            wi::image::Params params(translation.x+(scale.x-side)*.5f,
                translation.y+(scale.y-side)*.5f,side,side,wi::Color::White());
            wi::image::Draw(&preview.GetTexture(),params,cmd);
        }
        const char* GetWidgetTypeName() const override { return "GrassThumbnail"; }
    };

    struct VegetationControls
    {
        wi::gui::Label header;
        wi::gui::Label source;
        renegade::studio::SceneInspectorComboBox plantFilter;
        PlantGroup group = PlantGroup::Grass;
        renegade::studio::SceneInspectorSlider brushSize;
        renegade::studio::SceneInspectorButton paint;
        renegade::studio::SceneInspectorButton erase;
        wi::gui::Label status;
        renegade::studio::SceneInspectorButton importSheet, testInteraction;
        renegade::studio::SceneInspectorSlider importColumns, importRows;
        unsigned columns=1, rows=1;
        std::string project;
        bool refreshPalette=true, rebuildAtlas=true, interaction=false;
        wi::ecs::Entity force=wi::ecs::INVALID_ENTITY;
        std::vector<std::unique_ptr<GrassThumbnail>> thumbnails;
        size_t entryCount=0;
        renegade::bridge::GrassAtlas atlas;

        renegade::studio::SceneInspectorSlider density;
        renegade::studio::SceneInspectorSlider length;
        renegade::studio::SceneInspectorSlider width;
        renegade::studio::SceneInspectorSlider stiffness;
        renegade::studio::SceneInspectorSlider drag;
        renegade::studio::SceneInspectorSlider gravity;
        renegade::studio::SceneInspectorSlider randomness;
        renegade::studio::SceneInspectorSlider randomSeed;
        renegade::studio::SceneInspectorSlider segments;
        renegade::studio::SceneInspectorSlider billboards;
        renegade::studio::SceneInspectorSlider viewDistance;
        renegade::studio::SceneInspectorSlider uniformity;
        wi::gui::CheckBox cameraBend;

        bool created = false;
        bool brushActive = false;
        renegade::bridge::VegetationBrushMode mode =
            renegade::bridge::VegetationBrushMode::Paint;
        float brushRadius = 12.0f;

        bool settingsDragging = false;
        wi::ecs::Entity settingsTerrain =
            wi::ecs::INVALID_ENTITY;
        renegade::bridge::WickedVegetationSettings settingsBefore;

        bool strokeActive = false;
        bool strokeChanged = false;
        wi::ecs::Entity strokeTerrain =
            wi::ecs::INVALID_ENTITY;
        std::size_t affectedChunks = 0;
        std::size_t affectedVertices = 0;
        renegade::bridge::VegetationStrokeState strokeBefore;
    };

    std::unique_ptr<VegetationControls> controls;

    VegetationControls& Controls()
    {
        if (!controls)
            controls = std::make_unique<VegetationControls>();
        return *controls;
    }

    wi::terrain::Terrain* CurrentTerrain(
        renegade::bridge::StudioSession* session,
        wi::ecs::Entity* entity = nullptr)
    {
        if (session == nullptr)
            return nullptr;
        auto& scene = session->Scenes().GetScene();
        if (scene.terrains.GetCount() == 0)
            return nullptr;
        const auto terrainEntity = scene.terrains.GetEntity(0);
        if (entity != nullptr)
            *entity = terrainEntity;
        return scene.terrains.GetComponent(terrainEntity);
    }

    void SetField(
        renegade::bridge::WickedVegetationSettings& settings,
        const SettingField field,
        const float value)
    {
        switch (field)
        {
        case SettingField::Density:
            settings.density = value;
            break;
        case SettingField::Length:
            settings.length = value;
            break;
        case SettingField::Width:
            settings.width = value;
            break;
        case SettingField::Stiffness:
            settings.stiffness = value;
            break;
        case SettingField::Drag:
            settings.drag = value;
            break;
        case SettingField::Gravity:
            settings.gravityPower = value;
            break;
        case SettingField::Randomness:
            settings.randomness = value;
            break;
        case SettingField::RandomSeed:
            settings.randomSeed = static_cast<std::uint32_t>(
                std::max(1l, std::lround(value)));
            break;
        case SettingField::Segments:
            settings.segmentCount = static_cast<std::uint32_t>(
                std::max(1l, std::lround(value)));
            break;
        case SettingField::Billboards:
            settings.billboardCount = static_cast<std::uint32_t>(
                std::max(1l, std::lround(value)));
            break;
        case SettingField::ViewDistance:
            settings.viewDistance = value;
            break;
        case SettingField::Uniformity:
            settings.uniformity = value;
            break;
        }
    }

    void StyleLabel(wi::gui::Label& label)
    {
        label.SetColor(wi::Color::Transparent());
        label.font.params.size = 12;
        label.font.params.color = wi::Color(178, 178, 176, 255);
        label.font.params.h_align = wi::font::WIFALIGN_LEFT;
    }
}

namespace renegade::studio
{
    void StudioRenderPath::CreateWd01VegetationControls()
    {
        auto& state = Controls();
        if (state.created)
            return;
        state.created = true;

        state.header.Create("WD01 Wicked Vegetation Header");
        state.header.SetText("VEGETATION // WICKED GRASS");
        StyleLabel(state.header);
        inspectorPanel_.AddWidget(&state.header);

        state.source.Create("WD01 Wicked Vegetation Source");
        state.source.SetText("NATIVE TERRAIN HAIRPARTICLE SYSTEM");
        state.source.SetTooltip(
            "Pinned Wicked grass.wiscene, native GPU simulation, culling, wind and collider response.");
        StyleLabel(state.source);
        inspectorPanel_.AddWidget(&state.source);

        state.plantFilter.Create("Plant group");
        state.plantFilter.SetTooltip("Filter the palette. Selected plants from other groups stay in the brush. Custom reads PNGs from Content/Vegetation/Palette/Custom.");
        for (const auto& filter : PlantFilters) state.plantFilter.AddItem(filter.label);
        state.plantFilter.SetSelectedWithoutCallback(1);
        state.plantFilter.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if (args.iValue < 0 || size_t(args.iValue) >= std::size(PlantFilters)) return;
            auto& s = Controls();
            s.group = PlantFilters[args.iValue].group;
            if (s.group == PlantGroup::Custom || s.group == PlantGroup::All) s.refreshPalette = true;
            QueueInspectorRefresh();
        });
        inspectorPanel_.AddWidget(&state.plantFilter);

        state.brushSize.Create(
            1.0f, 100.0f, state.brushRadius, 990.0f,
            "WD01 Grass Brush Size", "BRUSH SIZE");
        state.brushSize.SetTooltip(
            "World-space radius. A single stroke can cross any number of terrain chunks.");
        const auto updateBrush = [](const float value)
        {
            Controls().brushRadius =
                std::clamp(value, 1.0f, 100.0f);
        };
        state.brushSize.OnValuePreview(updateBrush);
        state.brushSize.OnValueCommitted(updateBrush);
        inspectorPanel_.AddWidget(&state.brushSize);

        auto gridSize=[this](SceneInspectorSlider& slider,const char* name,const char* label,unsigned& value)
        {
            slider.Create(1,64,1,63,name,label);
            slider.SetTooltip("Grid dimensions for the next PNG import. Use 1 by 1 for a single cutout.");
            slider.OnValueCommitted([&value](float v){value=static_cast<unsigned>(std::lround(v));});
            inspectorPanel_.AddWidget(&slider);
        };
        gridSize(state.importColumns,"Grass sheet columns","SHEET COLUMNS",state.columns);
        gridSize(state.importRows,"Grass sheet rows","SHEET ROWS",state.rows);
        state.importSheet.Create("Import grass PNG");
        state.importSheet.SetText("IMPORT GRASS TEXTURE");
        state.importSheet.SetTooltip("Add a transparent PNG to this project's palette. Each cell of a sheet becomes a selectable grass.");
        state.importSheet.OnClick([this](const wi::gui::EventArgs&)
        {
            if (!session_ || !session_->Projects().HasProject()) return;
            const auto project=session_->Projects().CurrentProject();
            const unsigned cols=Controls().columns, rows=Controls().rows;
            wi::helper::FileDialogParams dialog;
            dialog.type=wi::helper::FileDialogParams::OPEN;
            dialog.extensions={"png"};
            dialog.description="Grass cutout or sprite sheet";
            wi::helper::FileDialog(dialog,[this,project,cols,rows](const std::string& path)
            {
                wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this,project,cols,rows,path](uint64_t)
                {
                    if (path.empty() || !session_ || !session_->Projects().HasProject() ||
                        session_->Projects().CurrentProject().projectId!=project.projectId) return;
                    std::string error;
                    const bool imported=bridge::ImportGrassSheet(std::filesystem::u8path(project.rootPath),
                        std::filesystem::u8path(path),cols,rows,error);
                    if (imported)
                    {
                        Controls().group = PlantGroup::Custom;
                        Controls().plantFilter.SetSelectedWithoutCallback(int(std::size(PlantFilters))-1);
                    }
                    Controls().refreshPalette=true;
                    Controls().status.SetText(imported ? "Grass added to the palette." : error);
                    QueueInspectorRefresh();
                });
            });
        });
        inspectorPanel_.AddWidget(&state.importSheet);
        state.testInteraction.Create("Test grass interaction");
        state.testInteraction.SetText("TEST INTERACTION");
        state.testInteraction.SetTooltip("Hold the left mouse button over the terrain to bend grass.");
        state.testInteraction.OnClick([this](const wi::gui::EventArgs&)
        {
            DisableTerrainPropBrush();
            DisableWd01VegetationBrush();
            auto& s=Controls(); s.interaction=true; s.brushActive=true;
            s.testInteraction.SetAccentOutlineStyle(true);
            s.status.SetText("Hold left mouse over grass to test bending.");
        });
        inspectorPanel_.AddWidget(&state.testInteraction);
        state.paint.Create("WD01 Paint Grass");
        state.paint.SetText("PAINT");
        state.paint.SetTooltip(
            "Paint Wicked grass using native per-chunk HairParticle masks.");
        state.paint.OnClick([this](const wi::gui::EventArgs&)
        {
            DisableWd01VegetationBrush();
            auto& value = Controls();
            terrainRiverBrushActive_ = false;
            terrainRiverStrokeActive_ = false;
            terrainRiverStrokePoints_.clear();
            terrainRiverBrushButton_.SetText("DRAW RIVER");
            terrainRiverStatus_.SetText("RIVER // READY");
            DisableTerrainPropBrush();
            value.brushActive = true;
            value.mode = bridge::VegetationBrushMode::Paint;
            value.paint.SetText("PAINT [ACTIVE]");
            value.erase.SetText("DELETE");
            value.status.SetText("VEGETATION // PAINT ACTIVE");
            RefreshStatus();
        });
        inspectorPanel_.AddWidget(&state.paint);

        state.erase.Create("WD01 Delete Grass");
        state.erase.SetText("DELETE");
        state.erase.SetTooltip(
            "Remove native Wicked grass with the same cross-chunk brush.");
        state.erase.OnClick([this](const wi::gui::EventArgs&)
        {
            DisableWd01VegetationBrush();
            auto& value = Controls();
            terrainRiverBrushActive_ = false;
            terrainRiverStrokeActive_ = false;
            terrainRiverStrokePoints_.clear();
            terrainRiverBrushButton_.SetText("DRAW RIVER");
            terrainRiverStatus_.SetText("RIVER // READY");
            DisableTerrainPropBrush();
            value.brushActive = true;
            value.mode = bridge::VegetationBrushMode::Delete;
            value.paint.SetText("PAINT");
            value.erase.SetText("DELETE [ACTIVE]");
            value.status.SetText("VEGETATION // DELETE ACTIVE");
            RefreshStatus();
        });
        inspectorPanel_.AddWidget(&state.erase);

        state.status.Create("WD01 Wicked Vegetation Status");
        state.status.SetText("VEGETATION // READY");
        state.status.SetWrapEnabled(true);
        StyleLabel(state.status);
        inspectorPanel_.AddWidget(&state.status);

        const auto createSetting = [this](
            SceneInspectorSlider& slider,
            const float minimum,
            const float maximum,
            const float initial,
            const float steps,
            const char* name,
            const char* label,
            const char* tooltip,
            const SettingField field)
        {
            slider.Create(
                minimum, maximum, initial, steps, name, label);
            slider.SetTooltip(tooltip);
            slider.OnDragStarted([this](const float)
            {
                auto& value = Controls();
                wi::ecs::Entity terrainEntity =
                    wi::ecs::INVALID_ENTITY;
                auto* terrain =
                    CurrentTerrain(session_, &terrainEntity);
                if (terrain == nullptr || session_ == nullptr)
                    return;
                std::string error;
                if (!bridge::InitializeWickedVegetation(
                        session_->Scenes().GetScene(),
                        *terrain,
                        &error))
                {
                    value.status.SetText(
                        "VEGETATION // " + error);
                    return;
                }
                value.settingsDragging = true;
                value.settingsTerrain = terrainEntity;
                value.settingsBefore =
                    bridge::CaptureWickedVegetationSettings(
                        *terrain);
            });
            slider.OnValuePreview([this, field](const float input)
            {
                auto* terrain = CurrentTerrain(session_);
                if (terrain == nullptr || session_ == nullptr)
                    return;
                auto settings =
                    bridge::CaptureWickedVegetationSettings(
                        *terrain);
                SetField(settings, field, input);
                bridge::ApplyWickedVegetationSettings(
                    session_->Scenes().GetScene(),
                    *terrain,
                    settings);
            });
            slider.OnValueCommitted([this](const float)
            {
                auto& value = Controls();
                if (!value.settingsDragging ||
                    session_ == nullptr)
                {
                    return;
                }
                value.settingsDragging = false;
                auto& scene = session_->Scenes().GetScene();
                auto* terrain = scene.terrains.GetComponent(
                    value.settingsTerrain);
                if (terrain == nullptr)
                    return;
                const auto after =
                    bridge::CaptureWickedVegetationSettings(
                        *terrain);
                session_->Commands().RecordExecuted(
                    std::make_unique<
                        bridge::SetWickedVegetationSettingsCommand>(
                        scene,
                        value.settingsTerrain,
                        value.settingsBefore,
                        after));
                value.status.SetText(
                    "VEGETATION // SETTINGS UPDATED");
                RefreshStatus();
            });
            inspectorPanel_.AddWidget(&slider);
        };

        createSetting(
            state.density, 0.0f, 4.0f, 1.0f, 1000.0f,
            "WD01 Grass Density", "DENSITY",
            "Wicked terrain grass density multiplier.",
            SettingField::Density);
        createSetting(
            state.length, 0.0f, 4.0f, 1.0f, 1000.0f,
            "WD01 Grass Length", "LENGTH",
            "Native HairParticle strand length.",
            SettingField::Length);
        createSetting(
            state.width, 0.0f, 2.0f, 1.0f, 1000.0f,
            "WD01 Grass Width", "WIDTH",
            "Native HairParticle card width.",
            SettingField::Width);
        createSetting(
            state.stiffness, 0.0f, 10.0f, 0.5f, 1000.0f,
            "WD01 Grass Stiffness", "STIFFNESS",
            "How strongly grass returns to its rest position.",
            SettingField::Stiffness);
        createSetting(
            state.drag, 0.0f, 1.0f, 0.1f, 1000.0f,
            "WD01 Grass Drag", "DRAG",
            "How quickly simulated movement loses energy.",
            SettingField::Drag);
        createSetting(
            state.gravity, 0.0f, 1.0f, 0.0f, 1000.0f,
            "WD01 Grass Gravity", "GRAVITY",
            "Native HairParticle gravity power.",
            SettingField::Gravity);
        createSetting(
            state.randomness, 0.0f, 1.0f, 0.2f, 1000.0f,
            "WD01 Grass Randomness", "RANDOMNESS",
            "Native strand-length randomization.",
            SettingField::Randomness);
        createSetting(
            state.randomSeed, 1.0f, 12345.0f, 1.0f, 12344.0f,
            "WD01 Grass Random Seed", "RANDOM SEED",
            "Native emitter random seed.",
            SettingField::RandomSeed);
        createSetting(
            state.segments, 1.0f, 10.0f, 1.0f, 9.0f,
            "WD01 Grass Segments", "SEGMENTS",
            "Geometry and simulation segments per strand. Higher costs more.",
            SettingField::Segments);
        createSetting(
            state.billboards, 1.0f, 10.0f, 1.0f, 9.0f,
            "WD01 Grass Billboards", "BILLBOARDS",
            "Crossed billboards per strand. Higher costs more.",
            SettingField::Billboards);
        createSetting(
            state.viewDistance, 0.0f, 1000.0f, 200.0f, 10000.0f,
            "WD01 Grass View Distance", "VIEW DISTANCE",
            "Wicked fade and chunk-generation distance.",
            SettingField::ViewDistance);
        createSetting(
            state.uniformity, 0.01f, 2.0f, 1.0f, 1990.0f,
            "WD01 Grass Uniformity", "UNIFORMITY",
            "Native atlas-selection distribution noise.",
            SettingField::Uniformity);

        state.cameraBend.Create("CAMERA BEND: ");
        state.cameraBend.SetTooltip(
            "Wicked camera-facing bend that reduces the card appearance from above.");
        state.cameraBend.SetCheck(true);
        state.cameraBend.OnClick(
            [this](const wi::gui::EventArgs& args)
        {
            wi::ecs::Entity terrainEntity =
                wi::ecs::INVALID_ENTITY;
            auto* terrain =
                CurrentTerrain(session_, &terrainEntity);
            if (terrain == nullptr || session_ == nullptr)
                return;
            auto& scene = session_->Scenes().GetScene();
            std::string error;
            if (!bridge::InitializeWickedVegetation(
                    scene, *terrain, &error))
            {
                Controls().status.SetText(
                    "VEGETATION // " + error);
                return;
            }
            auto before =
                bridge::CaptureWickedVegetationSettings(
                    *terrain);
            auto after = before;
            after.cameraBend = args.bValue;
            session_->Commands().Execute(
                std::make_unique<
                    bridge::SetWickedVegetationSettingsCommand>(
                    scene,
                    terrainEntity,
                    before,
                    after));
            Controls().status.SetText(
                "VEGETATION // CAMERA BEND UPDATED");
            RefreshStatus();
        });
        inspectorPanel_.AddWidget(&state.cameraBend);

        RefreshWd01VegetationControls(false);
    }

    void StudioRenderPath::LayoutWd01VegetationControls(
        const float fieldWidth)
    {
        auto& state = Controls();
        if (!state.created)
            return;
        const auto place = [fieldWidth](
            wi::gui::Widget& widget,
            const float y,
            const float height = 28.0f)
        {
            widget.SetPos(XMFLOAT2(12.0f, y));
            widget.SetSize(XMFLOAT2(fieldWidth, height));
        };

        place(state.header, VegetationTop, 20.0f);
        place(state.source, VegetationTop + 22.0f, 20.0f);
        place(state.brushSize, VegetationTop + 48.0f);

        const float buttonWidth = (fieldWidth - 8.0f) * 0.5f;
        state.paint.SetPos(XMFLOAT2(12.0f, VegetationTop + 82.0f));
        state.paint.SetSize(XMFLOAT2(buttonWidth, 28.0f));
        state.erase.SetPos(XMFLOAT2(
            20.0f + buttonWidth, VegetationTop + 82.0f));
        state.erase.SetSize(XMFLOAT2(buttonWidth, 28.0f));
        place(state.status, VegetationTop + 116.0f, 36.0f);

        float y = VegetationTop + 158.0f;
        wi::gui::Widget* rows[] = {
            &state.density,
            &state.length,
            &state.width,
            &state.stiffness,
            &state.drag,
            &state.gravity,
            &state.randomness,
            &state.randomSeed,
            &state.segments,
            &state.billboards,
            &state.viewDistance,
            &state.uniformity,
            &state.cameraBend,
        };
        for (auto* row : rows)
        {
            place(*row, y);
            y += 34.0f;
        }
    }

    std::vector<wi::gui::Widget*> StudioRenderPath::TerrainVegetationWidgets(int group)
    {
        auto& s=Controls();
        if(!s.created)return {};
        if(group==0){s.header.SetVisible(false);s.source.SetVisible(false);std::vector<wi::gui::Widget*> rows;
            rows.push_back(&s.plantFilter);
            for (size_t i=0; i<s.entryCount; ++i)
            {
                auto& thumb = *s.thumbnails[i];
                if (s.group == PlantGroup::All || thumb.group == s.group) rows.push_back(&thumb);
                else thumb.SetVisible(false);
            }
            rows.insert(rows.end(),{&s.brushSize,&s.paint,&s.erase,&s.testInteraction,
                &s.importColumns,&s.importRows,&s.importSheet});
            return rows;}
        if(group==3)return {&s.status};
        if(group==1)return {&s.density,&s.length,&s.width,&s.randomness,&s.randomSeed,&s.uniformity};
        return {&s.stiffness,&s.drag,&s.gravity,&s.segments,&s.billboards,&s.viewDistance,&s.cameraBend};
    }

    void StudioRenderPath::RefreshWd01VegetationControls(
        const bool hasTerrain)
    {
        auto& state = Controls();
        if (!state.created)
            return;
        const bool visible = terrainWorkspaceActive_ && vegetationWorkspace_;
        const auto project=session_ && session_->Projects().HasProject()
            ? session_->Projects().CurrentProject().rootPath : std::string{};
        if (visible && !project.empty() && (state.refreshPalette || state.project!=project))
        {
            std::vector<std::filesystem::path> selected;
            for (const auto& thumbnail:state.thumbnails)
                if (state.project==project && thumbnail->selected) selected.push_back(thumbnail->path);
            state.refreshPalette=false; state.project=project; state.rebuildAtlas=true;
            const auto root = std::filesystem::u8path(project);
            // Older projects acquire the drop folder when the palette is first opened.
            std::error_code folderError;
            std::filesystem::create_directories(root / bridge::CustomPlantDirectory,folderError);
            if (folderError) state.status.SetText("Could not create the Custom plant folder: "+folderError.message());
            state.entryCount = 0;
            for (const auto& filter : PlantFilters)
            {
                if (filter.group == PlantGroup::All) continue;
                for (const auto& entry : bridge::GrassPaletteEntries(root,filter.group))
                {
                    const size_t i = state.entryCount++;
                    if (i==state.thumbnails.size())
                    {
                        auto button=std::make_unique<GrassThumbnail>();
                        button->Create("Grass palette "+std::to_string(i));
                        button->SetText("");
                        button->OnClick([i](const wi::gui::EventArgs&)
                        {
                            auto& s=Controls(); auto& t=*s.thumbnails[i];
                            t.selected=!t.selected; t.SetAccentOutlineStyle(t.selected);
                            s.rebuildAtlas=true;
                            size_t count=0;
                            for(size_t j=0;j<s.entryCount;++j) if(s.thumbnails[j]->selected) ++count;
                            s.status.SetText(count ? std::to_string(count)+" plant types selected" : "Stock Wicked grass selected");
                        });
                        inspectorPanel_.AddWidget(button.get());
                        state.thumbnails.push_back(std::move(button));
                    }
                    auto& t=*state.thumbnails[i]; t.path=entry; t.group=filter.group;
                    t.preview=wi::resourcemanager::Load(t.path.generic_u8string());
                    t.selected=std::find(selected.begin(),selected.end(),t.path)!=selected.end();
                    t.SetAccentOutlineStyle(t.selected);
                    t.SetTooltip(t.path.stem().u8string());
                }
            }
            for (size_t i=state.entryCount;i<state.thumbnails.size();++i)
            { state.thumbnails[i]->SetVisible(false); state.thumbnails[i]->selected=false; }
        }
        wi::gui::Widget* widgets[] = {
            &state.header,
            &state.source,
            &state.plantFilter,
            &state.importSheet,&state.importRows,&state.importColumns,&state.testInteraction,
            &state.brushSize,
            &state.paint,
            &state.erase,
            &state.status,
            &state.density,
            &state.length,
            &state.width,
            &state.stiffness,
            &state.drag,
            &state.gravity,
            &state.randomness,
            &state.randomSeed,
            &state.segments,
            &state.billboards,
            &state.viewDistance,
            &state.uniformity,
            &state.cameraBend,
        };
        for (auto* widget : widgets)
        {
            widget->SetVisible(visible);
            widget->SetEnabled(hasTerrain);
        }
        state.header.SetEnabled(true);
        state.source.SetEnabled(true);
        state.status.SetEnabled(true);

        if (!hasTerrain)
        {
            state.status.SetText(
                "VEGETATION // CREATE TERRAIN FIRST");
            return;
        }
        auto* terrain = CurrentTerrain(session_);
        if (terrain == nullptr || state.settingsDragging)
            return;
        const auto settings =
            bridge::CaptureWickedVegetationSettings(*terrain);
        state.density.SetValue(settings.density);
        state.length.SetValue(settings.length);
        state.width.SetValue(settings.width);
        state.stiffness.SetValue(settings.stiffness);
        state.drag.SetValue(settings.drag);
        state.gravity.SetValue(settings.gravityPower);
        state.randomness.SetValue(settings.randomness);
        state.randomSeed.SetValue(
            static_cast<float>(settings.randomSeed));
        state.segments.SetValue(
            static_cast<float>(settings.segmentCount));
        state.billboards.SetValue(
            static_cast<float>(settings.billboardCount));
        state.viewDistance.SetValue(settings.viewDistance);
        state.uniformity.SetValue(settings.uniformity);
        state.cameraBend.SetCheck(settings.cameraBend);
    }

    void StudioRenderPath::TickWd01Vegetation()
    {
        wi::ecs::Entity terrainEntity = wi::ecs::INVALID_ENTITY;
        auto* terrain = CurrentTerrain(session_, &terrainEntity);
        if (terrain == nullptr || session_ == nullptr)
        {
            wd01SynchronizedTerrainEntity_ = wi::ecs::INVALID_ENTITY;
            wd01SynchronizedChunkCount_ = 0;
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        if (!bridge::IsWickedVegetationInitialized(scene, *terrain))
        {
            wd01SynchronizedTerrainEntity_ = wi::ecs::INVALID_ENTITY;
            wd01SynchronizedChunkCount_ = 0;
            return;
        }

        // Terrain generation and expansion add chunks asynchronously. The
        // native chunk count is the lifecycle signal: unchanged terrain does
        // no traversal, allocation or emitter repair during an idle frame.
        if (wd01SynchronizedTerrainEntity_ == terrainEntity &&
            wd01SynchronizedChunkCount_ == terrain->chunks.size())
        {
            return;
        }

        bool allChunksReady = false;
        bridge::SynchronizeWickedVegetation(scene, *terrain, allChunksReady);
        // A final batch can already be counted while its meshes still belong
        // to the generator scene. Retry until publication; then idle frames
        // retain the existing constant-time identity/count check.
        wd01SynchronizedTerrainEntity_ = allChunksReady
            ? terrainEntity : wi::ecs::INVALID_ENTITY;
        wd01SynchronizedChunkCount_ = allChunksReady ? terrain->chunks.size() : 0;
    }

    void StudioRenderPath::DisableWd01VegetationBrush()
    {
        auto& state = Controls();
        if (session_)
        {
            auto& scene = session_->Scenes().GetScene();
            bridge::ClearGrassInteraction(scene,state.force);
            if (state.strokeChanged)
            {
                if (auto* terrain = scene.terrains.GetComponent(state.strokeTerrain))
                {
                    bridge::FinalizeWickedVegetationStroke(scene,*terrain,state.strokeBefore);
                    auto after=bridge::CaptureWickedVegetationAfter(scene,*terrain,state.strokeBefore);
                    if (!after.chunks.empty())
                        session_->Commands().RecordExecuted(std::make_unique<bridge::WickedVegetationStrokeCommand>(
                            scene,state.strokeTerrain,std::move(state.strokeBefore),std::move(after)));
                }
            }
        }
        state.interaction=false;
        state.testInteraction.SetAccentOutlineStyle(false);
        state.brushActive = false;
        state.strokeActive = false;
        state.strokeChanged = false;
        state.strokeBefore = {};
        if (!state.created)
            return;
        state.paint.SetText("PAINT");
        state.erase.SetText("DELETE");
        state.status.SetText("VEGETATION // READY");
    }

    bool StudioRenderPath::HandleWd01Vegetation(
        const XMFLOAT4& pointer)
    {
        auto& state = Controls();
        if (state.force!=wi::ecs::INVALID_ENTITY && session_ &&
            (!state.interaction || !terrainWorkspaceActive_ || !vegetationWorkspace_ ||
             !wi::input::Down(wi::input::MOUSE_BUTTON_LEFT) || flyCameraActive_ || GetGUI().HasFocus() ||
             !IsPointerOverViewport(pointer) || camera==nullptr))
            bridge::ClearGrassInteraction(session_->Scenes().GetScene(),state.force);
        if (!state.created || !state.brushActive || !vegetationWorkspace_ ||
            !terrainWorkspaceActive_ || session_ == nullptr)
        {
            return false;
        }

        wi::ecs::Entity terrainEntity =
            wi::ecs::INVALID_ENTITY;
        auto* terrain =
            CurrentTerrain(session_, &terrainEntity);
        if (terrain == nullptr)
            return false;

        auto& scene = session_->Scenes().GetScene();

        // A stroke can begin in the viewport and be released over Renegade's
        // top menu, Inspector or bottom drawer. Complete that release before
        // applying viewport/focus guards so the brush never retains input
        // ownership outside its surface.
        if (state.strokeActive &&
            wi::input::Release(
                wi::input::MOUSE_BUTTON_LEFT))
        {
            state.strokeActive = false;
            auto* strokeTerrain = scene.terrains.GetComponent(
                state.strokeTerrain);
            if (strokeTerrain == nullptr)
                strokeTerrain = terrain;
            if (state.strokeChanged)
            {
                bridge::FinalizeWickedVegetationStroke(
                    scene,
                    *strokeTerrain,
                    state.strokeBefore);
                auto after =
                    bridge::CaptureWickedVegetationAfter(
                        scene, *strokeTerrain,
                        state.strokeBefore);
                if (!after.chunks.empty())
                {
                    session_->Commands().RecordExecuted(
                        std::make_unique<
                            bridge::WickedVegetationStrokeCommand>(
                            scene,
                            state.strokeTerrain,
                            std::move(state.strokeBefore),
                            std::move(after)));
                    std::ostringstream message;
                    message << "LAST GRASS STROKE // "
                            << state.affectedVertices
                            << " VERTICES // "
                            << state.affectedChunks
                            << " CHUNK UPDATES";
                    state.status.SetText(message.str());
                    RefreshStatus();
                }
            }
            state.strokeBefore = {};
            state.strokeChanged = false;
            return IsPointerOverViewport(pointer);
        }

        if (flyCameraActive_ || GetGUI().HasFocus() ||
            !IsPointerOverViewport(pointer) || camera == nullptr)
        {
            return false;
        }

        const auto ray = wi::renderer::GetPickRay(
            static_cast<long>(pointer.x),
            static_cast<long>(pointer.y),
            *this,
            *camera);
        const auto picked = wi::scene::Pick(
            ray,
            wi::enums::FILTER_TERRAIN,
            ~0u,
            scene);
        const bool leftPressed = wi::input::Press(
            wi::input::MOUSE_BUTTON_LEFT);
        const bool leftDown = wi::input::Down(
            wi::input::MOUSE_BUTTON_LEFT);

        if (state.interaction)
        {
            if (leftDown && picked.entity!=wi::ecs::INVALID_ENTITY)
                bridge::UpdateGrassInteraction(scene,state.force,picked.position,picked.normal);
            else
                bridge::ClearGrassInteraction(scene,state.force);
            return leftDown;
        }

        if (picked.entity != wi::ecs::INVALID_ENTITY)
        {
            const XMFLOAT4 colour =
                state.mode ==
                    bridge::VegetationBrushMode::Paint
                ? XMFLOAT4(0.25f, 0.92f, 0.42f, 0.24f)
                : XMFLOAT4(0.95f, 0.28f, 0.18f, 0.24f);
            wi::renderer::DrawSphere(
                wi::primitive::Sphere(
                    picked.position,
                    state.brushRadius),
                colour,
                true);
        }

        if (leftPressed &&
            picked.entity != wi::ecs::INVALID_ENTITY)
        {
            std::string error;
            if (state.mode==bridge::VegetationBrushMode::Paint && state.rebuildAtlas)
            {
                std::vector<std::filesystem::path> selected;
                for (size_t i=0; i<state.entryCount; ++i)
                    if (state.thumbnails[i]->selected) selected.push_back(state.thumbnails[i]->path);
                if (selected.empty()) state.atlas={};
                else if (!bridge::BuildGrassAtlas(std::filesystem::u8path(state.project),selected,state.atlas,error))
                { state.status.SetText(error); return true; }
                state.rebuildAtlas=false;
            }
            if (!bridge::InitializeWickedVegetation(
                    scene, *terrain, &error))
            {
                state.status.SetText(
                    "VEGETATION // " + error);
                return true;
            }
            state.strokeActive = true;
            state.strokeChanged = false;
            state.strokeTerrain = terrainEntity;
            state.affectedChunks = 0;
            state.affectedVertices = 0;
            state.strokeBefore = {};
        }

        if (state.strokeActive && leftDown &&
            picked.entity != wi::ecs::INVALID_ENTITY)
        {
            const auto result =
                bridge::PaintWickedVegetation(
                    scene,
                    *terrain,
                    picked.position,
                    state.brushRadius,
                    state.mode,
                    &state.strokeBefore, state.atlas);
            if (!result.error.empty())
            {
                state.status.SetText(
                    "VEGETATION // " + result.error);
                return true;
            }
            state.strokeChanged |= result.changed;
            state.affectedChunks += result.affectedChunks;
            state.affectedVertices +=
                result.affectedVertices;
        }

        // An armed vegetation brush owns only left-button painting. Merely
        // hovering the viewport must fall through to Studio navigation so RMB
        // look/pan and keyboard movement remain available between strokes.
        return leftPressed || leftDown || state.strokeActive;
    }

}

namespace renegade::studio
{
    bridge::DiagnosticState StudioRenderPath::CaptureVegetationDiagnostics() const
    {
        return {{"brush_active", controls && controls->brushActive},
            {"stroke_active", controls && controls->strokeActive},
            {"mode", controls && controls->mode == bridge::VegetationBrushMode::Paint ? std::string("paint") : std::string("erase")}};
    }
}

namespace renegade::studio
{
    bool StudioRenderPath::IsWd01VegetationBrushActive() const
    {
        return controls && controls->brushActive;
    }
}
