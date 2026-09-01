#include "StudioApplication.h"

#include "renegade/bridge/VegetationService.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>

namespace
{
    constexpr float VegetationTop = 710.0f;

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

    struct VegetationControls
    {
        wi::gui::Label header;
        wi::gui::Label source;
        renegade::studio::SceneInspectorSlider brushSize;
        renegade::studio::SceneInspectorButton paint;
        renegade::studio::SceneInspectorButton erase;
        wi::gui::Label status;

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

        state.paint.Create("WD01 Paint Grass");
        state.paint.SetText("PAINT");
        state.paint.SetTooltip(
            "Paint Wicked grass using native per-chunk HairParticle masks.");
        state.paint.OnClick([this](const wi::gui::EventArgs&)
        {
            auto& value = Controls();
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
            auto& value = Controls();
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

    void StudioRenderPath::RefreshWd01VegetationControls(
        const bool hasTerrain)
    {
        auto& state = Controls();
        if (!state.created)
            return;
        const bool visible = terrainWorkspaceActive_;
        wi::gui::Widget* widgets[] = {
            &state.header,
            &state.source,
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

        bridge::SynchronizeWickedVegetation(scene, *terrain);
        wd01SynchronizedTerrainEntity_ = terrainEntity;
        wd01SynchronizedChunkCount_ = terrain->chunks.size();
    }

    void StudioRenderPath::DisableWd01VegetationBrush()
    {
        auto& state = Controls();
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
        if (!state.created || !state.brushActive ||
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
                        *strokeTerrain,
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

        if (wi::input::Press(
                wi::input::MOUSE_BUTTON_LEFT) &&
            picked.entity != wi::ecs::INVALID_ENTITY)
        {
            std::string error;
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

        if (state.strokeActive &&
            wi::input::Down(
                wi::input::MOUSE_BUTTON_LEFT) &&
            picked.entity != wi::ecs::INVALID_ENTITY)
        {
            const auto result =
                bridge::PaintWickedVegetation(
                    scene,
                    *terrain,
                    picked.position,
                    state.brushRadius,
                    state.mode,
                    &state.strokeBefore);
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

        return true;
    }

}
