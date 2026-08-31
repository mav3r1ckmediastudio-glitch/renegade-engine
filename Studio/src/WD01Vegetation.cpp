#include "StudioApplication.h"

#include "renegade/bridge/VegetationAppearanceService.h"
#include "renegade/bridge/VegetationService.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>

namespace
{
    constexpr wi::Color VegetationMuted = wi::Color(178, 178, 176, 255);

    // StudioApplication's existing Terrain action rows end at Y 834:
    // selection 726-754, history 766-794, save 806-834. WD01 is appended
    // after them rather than occupying the same rows. Wicked Window children
    // are scrollable by default, so the lower WD01 controls automatically
    // extend the Inspector's vertical scroll range.
    constexpr float TerrainInspectorActionsBottom = 834.0f;
    constexpr float VegetationInspectorGap = 16.0f;
    constexpr float VegetationInspectorTop =
        TerrainInspectorActionsBottom + VegetationInspectorGap;

    enum class AppearanceField
    {
        Length,
        Width,
        Randomness,
        RandomSeed,
        Uniformity,
    };

    struct VegetationControls
    {
        wi::gui::Label sectionLabel;
        wi::gui::Label presetReadout;
        renegade::studio::SceneInspectorSlider brushSize;
        renegade::studio::SceneInspectorSlider density;
        renegade::studio::SceneInspectorButton paint;
        renegade::studio::SceneInspectorButton erase;
        wi::gui::Label status;

        renegade::studio::SceneInspectorButton appearanceHeader;
        renegade::studio::SceneInspectorSlider length;
        renegade::studio::SceneInspectorSlider width;
        renegade::studio::SceneInspectorSlider randomness;
        renegade::studio::SceneInspectorSlider randomSeed;
        renegade::studio::SceneInspectorSlider uniformity;

        bool controlsCreated = false;
        bool brushActive = false;
        renegade::bridge::VegetationBrushMode brushMode =
            renegade::bridge::VegetationBrushMode::Paint;
        float brushSizeValue = 12.0f;
        bool densityDragging = false;
        float densityBefore = 1.0f;
        wi::ecs::Entity densityTerrain = wi::ecs::INVALID_ENTITY;

        bool appearanceExpanded = true;
        bool appearanceDragging = false;
        renegade::bridge::VegetationAppearanceSettings appearanceBefore;
        wi::ecs::Entity appearanceTerrain = wi::ecs::INVALID_ENTITY;

        bool strokeActive = false;
        bool strokeChanged = false;
        wi::ecs::Entity strokeTerrain = wi::ecs::INVALID_ENTITY;
        std::size_t strokeAffectedChunks = 0;
        std::size_t strokeAffectedVertices = 0;
        renegade::bridge::VegetationStrokeState strokeBefore;
    };

    std::unique_ptr<VegetationControls> vegetation;

    VegetationControls& Controls()
    {
        if (!vegetation)
            vegetation = std::make_unique<VegetationControls>();
        return *vegetation;
    }

    void StyleLabel(wi::gui::Label& label)
    {
        label.font.params.size = 12;
        label.font.params.color = VegetationMuted;
        label.font.params.h_align = wi::font::WIFALIGN_LEFT;
        label.SetColor(wi::Color::Transparent());
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
        const wi::ecs::Entity terrainEntity = scene.terrains.GetEntity(0);
        if (entity != nullptr)
            *entity = terrainEntity;
        return scene.terrains.GetComponent(terrainEntity);
    }

    void SetAppearanceField(
        renegade::bridge::VegetationAppearanceSettings& settings,
        const AppearanceField field,
        const float value)
    {
        switch (field)
        {
        case AppearanceField::Length:
            settings.length = value;
            break;
        case AppearanceField::Width:
            settings.width = value;
            break;
        case AppearanceField::Randomness:
            settings.randomness = value;
            break;
        case AppearanceField::RandomSeed:
            settings.randomSeed = static_cast<std::uint32_t>(
                std::max(1l, std::lround(value)));
            break;
        case AppearanceField::Uniformity:
            settings.uniformity = value;
            break;
        }
    }
}

namespace renegade::studio
{
    void StudioRenderPath::CreateWd01VegetationControls()
    {
        auto& controls = Controls();
        if (controls.controlsCreated)
            return;
        controls.controlsCreated = true;

        controls.sectionLabel.Create("WD01 Vegetation Section");
        controls.sectionLabel.SetText("VEGETATION // GRASS");
        StyleLabel(controls.sectionLabel);
        inspectorPanel_.AddWidget(&controls.sectionLabel);

        controls.presetReadout.Create("WD01 Vegetation Preset");
        controls.presetReadout.SetText("GRASS // WICKED DEFAULT");
        controls.presetReadout.SetTooltip(
            "Pinned Wicked grass.wiscene preset. Custom vegetation assets are deferred.");
        StyleLabel(controls.presetReadout);
        inspectorPanel_.AddWidget(&controls.presetReadout);

        controls.brushSize.Create(
            1.0f,
            100.0f,
            controls.brushSizeValue,
            990.0f,
            "WD01 Vegetation Brush Size",
            "BRUSH SIZE");
        controls.brushSize.SetTooltip(
            "World-space radius of the vegetation paint/delete brush.");
        const auto updateBrushSize = [](const float value)
        {
            Controls().brushSizeValue = std::clamp(value, 1.0f, 100.0f);
        };
        controls.brushSize.OnValuePreview(updateBrushSize);
        controls.brushSize.OnValueCommitted(updateBrushSize);
        inspectorPanel_.AddWidget(&controls.brushSize);

        controls.density.Create(
            0.0f,
            4.0f,
            1.0f,
            1000.0f,
            "WD01 Vegetation Density",
            "DENSITY");
        controls.density.SetTooltip(
            "Overall density multiplier for painted grass. This is stored by native Wicked terrain.");
        controls.density.OnDragStarted([this](const float)
        {
            auto& state = Controls();
            wi::ecs::Entity terrainEntity = wi::ecs::INVALID_ENTITY;
            auto* terrain = CurrentTerrain(session_, &terrainEntity);
            if (terrain == nullptr)
                return;
            state.densityDragging = true;
            state.densityTerrain = terrainEntity;
            state.densityBefore = bridge::CaptureVegetationDensity(*terrain);
        });
        controls.density.OnValuePreview([this](const float value)
        {
            wi::ecs::Entity terrainEntity = wi::ecs::INVALID_ENTITY;
            auto* terrain = CurrentTerrain(session_, &terrainEntity);
            if (terrain == nullptr || session_ == nullptr)
                return;
            auto& scene = session_->Scenes().GetScene();
            std::string error;
            if (!bridge::EnsureDefaultGrassVegetation(scene, *terrain, &error))
            {
                Controls().status.SetText("VEGETATION // " + error);
                return;
            }
            bridge::SetVegetationDensity(scene, *terrain, value);
        });
        controls.density.OnValueCommitted([this](const float value)
        {
            auto& state = Controls();
            if (!state.densityDragging || session_ == nullptr)
                return;
            state.densityDragging = false;
            auto& scene = session_->Scenes().GetScene();
            auto* terrain = scene.terrains.GetComponent(state.densityTerrain);
            if (terrain == nullptr)
                return;
            const float after = bridge::CaptureVegetationDensity(*terrain);
            if (std::abs(after - state.densityBefore) > 0.0001f)
            {
                session_->Commands().RecordExecuted(
                    std::make_unique<bridge::SetVegetationDensityCommand>(
                        scene,
                        state.densityTerrain,
                        state.densityBefore,
                        after));
                state.status.SetText("VEGETATION // DENSITY UPDATED");
                RefreshStatus();
            }
        });
        inspectorPanel_.AddWidget(&controls.density);

        controls.paint.Create("WD01 Paint Vegetation");
        controls.paint.SetText("PAINT");
        controls.paint.SetTooltip(
            "Paint the bundled Wicked grass onto terrain with the viewport brush.");
        controls.paint.OnClick([this](const wi::gui::EventArgs&)
        {
            auto& state = Controls();
            state.brushActive = true;
            state.brushMode = bridge::VegetationBrushMode::Paint;
            state.status.SetText("VEGETATION // PAINT ACTIVE");
            state.paint.SetText("PAINT [ACTIVE]");
            state.erase.SetText("DELETE");
            RefreshStatus();
        });
        inspectorPanel_.AddWidget(&controls.paint);

        controls.erase.Create("WD01 Delete Vegetation");
        controls.erase.SetText("DELETE");
        controls.erase.SetTooltip(
            "Delete painted grass under the same viewport brush.");
        controls.erase.OnClick([this](const wi::gui::EventArgs&)
        {
            auto& state = Controls();
            state.brushActive = true;
            state.brushMode = bridge::VegetationBrushMode::Delete;
            state.status.SetText("VEGETATION // DELETE ACTIVE");
            state.paint.SetText("PAINT");
            state.erase.SetText("DELETE [ACTIVE]");
            RefreshStatus();
        });
        inspectorPanel_.AddWidget(&controls.erase);

        controls.status.Create("WD01 Vegetation Status");
        controls.status.SetText("VEGETATION // READY");
        controls.status.SetWrapEnabled(true);
        StyleLabel(controls.status);
        inspectorPanel_.AddWidget(&controls.status);

        controls.appearanceHeader.Create("WD01 Appearance Header");
        controls.appearanceHeader.SetText("APPEARANCE [-]");
        controls.appearanceHeader.SetTooltip(
            "Native Wicked grass appearance controls. This section does not alter the accepted paint backend.");
        controls.appearanceHeader.OnClick([this](const wi::gui::EventArgs&)
        {
            auto& state = Controls();
            state.appearanceExpanded = !state.appearanceExpanded;
            state.appearanceHeader.SetText(
                state.appearanceExpanded ? "APPEARANCE [-]" : "APPEARANCE [+]");
            ResizeLayout();
            RefreshWd01VegetationControls(CurrentTerrain(session_) != nullptr);
        });
        inspectorPanel_.AddWidget(&controls.appearanceHeader);

        const auto createAppearanceSlider = [this](
            SceneInspectorSlider& slider,
            const float minimum,
            const float maximum,
            const float defaultValue,
            const float steps,
            const std::string& name,
            const std::string& label,
            const std::string& tooltip,
            const AppearanceField field)
        {
            slider.Create(minimum, maximum, defaultValue, steps, name, label);
            slider.SetTooltip(tooltip);
            slider.OnDragStarted([this](const float)
            {
                auto& state = Controls();
                wi::ecs::Entity terrainEntity = wi::ecs::INVALID_ENTITY;
                auto* terrain = CurrentTerrain(session_, &terrainEntity);
                if (terrain == nullptr || session_ == nullptr)
                    return;
                auto& scene = session_->Scenes().GetScene();
                std::string error;
                if (!bridge::EnsureDefaultGrassVegetation(scene, *terrain, &error))
                {
                    state.status.SetText("VEGETATION // " + error);
                    return;
                }
                state.appearanceDragging = true;
                state.appearanceTerrain = terrainEntity;
                state.appearanceBefore =
                    bridge::CaptureVegetationAppearanceSettings(*terrain);
            });
            slider.OnValuePreview([this, field](const float value)
            {
                auto* terrain = CurrentTerrain(session_);
                if (terrain == nullptr || session_ == nullptr)
                    return;
                auto settings =
                    bridge::CaptureVegetationAppearanceSettings(*terrain);
                SetAppearanceField(settings, field, value);
                bridge::ApplyVegetationAppearanceSettings(
                    session_->Scenes().GetScene(), *terrain, settings);
            });
            slider.OnValueCommitted([this](const float)
            {
                auto& state = Controls();
                if (!state.appearanceDragging || session_ == nullptr)
                    return;
                state.appearanceDragging = false;
                auto& scene = session_->Scenes().GetScene();
                auto* terrain = scene.terrains.GetComponent(state.appearanceTerrain);
                if (terrain == nullptr)
                    return;
                const auto after =
                    bridge::CaptureVegetationAppearanceSettings(*terrain);
                if (!bridge::VegetationAppearanceSettingsEqual(
                        state.appearanceBefore, after))
                {
                    session_->Commands().RecordExecuted(
                        std::make_unique<bridge::SetVegetationAppearanceSettingsCommand>(
                            scene,
                            state.appearanceTerrain,
                            state.appearanceBefore,
                            after));
                    state.status.SetText("VEGETATION // APPEARANCE UPDATED");
                    RefreshStatus();
                }
            });
            inspectorPanel_.AddWidget(&slider);
        };

        createAppearanceSlider(
            controls.length,
            0.0f, 4.0f, 1.0f, 1000.0f,
            "WD01 Grass Length", "LENGTH",
            "Wicked HairParticle strand length. Use about 0.3-0.4 for ordinary grass.",
            AppearanceField::Length);
        createAppearanceSlider(
            controls.width,
            0.0f, 2.0f, 1.0f, 1000.0f,
            "WD01 Grass Width", "WIDTH",
            "Horizontal width multiplier for each grass card.",
            AppearanceField::Width);
        createAppearanceSlider(
            controls.randomness,
            0.0f, 1.0f, 0.2f, 1000.0f,
            "WD01 Grass Randomness", "RANDOMNESS",
            "Variation in individual strand length.",
            AppearanceField::Randomness);
        createAppearanceSlider(
            controls.randomSeed,
            1.0f, 12345.0f, 1.0f, 12344.0f,
            "WD01 Grass Random Seed", "RANDOM SEED",
            "Native Wicked random seed controlling patch placement variation.",
            AppearanceField::RandomSeed);
        createAppearanceSlider(
            controls.uniformity,
            0.01f, 2.0f, 1.0f, 1990.0f,
            "WD01 Grass Uniformity", "UNIFORMITY",
            "Modulates sprite-selection distribution noise by particle position.",
            AppearanceField::Uniformity);

        RefreshWd01VegetationControls(false);
    }

    void StudioRenderPath::LayoutWd01VegetationControls(const float fieldWidth)
    {
        auto& controls = Controls();
        if (!controls.controlsCreated)
            return;

        const auto place = [fieldWidth](
            wi::gui::Widget& widget,
            const float y,
            const float height = 28.0f)
        {
            widget.SetPos(XMFLOAT2(12.0f, y));
            widget.SetSize(XMFLOAT2(fieldWidth, height));
        };

        place(controls.sectionLabel, VegetationInspectorTop, 20.0f);
        place(controls.presetReadout, VegetationInspectorTop + 22.0f, 20.0f);
        place(controls.brushSize, VegetationInspectorTop + 46.0f);
        place(controls.density, VegetationInspectorTop + 80.0f);

        const float buttonWidth = (fieldWidth - 8.0f) * 0.5f;
        controls.paint.SetPos(XMFLOAT2(
            12.0f,
            VegetationInspectorTop + 114.0f));
        controls.paint.SetSize(XMFLOAT2(buttonWidth, 28.0f));
        controls.erase.SetPos(XMFLOAT2(
            20.0f + buttonWidth,
            VegetationInspectorTop + 114.0f));
        controls.erase.SetSize(XMFLOAT2(buttonWidth, 28.0f));
        place(controls.status, VegetationInspectorTop + 148.0f, 34.0f);

        place(controls.appearanceHeader, VegetationInspectorTop + 190.0f);
        if (controls.appearanceExpanded)
        {
            place(controls.length, VegetationInspectorTop + 224.0f);
            place(controls.width, VegetationInspectorTop + 258.0f);
            place(controls.randomness, VegetationInspectorTop + 292.0f);
            place(controls.randomSeed, VegetationInspectorTop + 326.0f);
            place(controls.uniformity, VegetationInspectorTop + 360.0f);
        }
    }

    void StudioRenderPath::RefreshWd01VegetationControls(const bool hasTerrain)
    {
        auto& controls = Controls();
        if (!controls.controlsCreated)
            return;

        const bool visible = terrainWorkspaceActive_;
        controls.sectionLabel.SetVisible(visible);
        controls.presetReadout.SetVisible(visible);
        controls.brushSize.SetVisible(visible);
        controls.density.SetVisible(visible);
        controls.paint.SetVisible(visible);
        controls.erase.SetVisible(visible);
        controls.status.SetVisible(visible);
        controls.appearanceHeader.SetVisible(visible);

        const bool appearanceVisible = visible && controls.appearanceExpanded;
        controls.length.SetVisible(appearanceVisible);
        controls.width.SetVisible(appearanceVisible);
        controls.randomness.SetVisible(appearanceVisible);
        controls.randomSeed.SetVisible(appearanceVisible);
        controls.uniformity.SetVisible(appearanceVisible);

        controls.brushSize.SetEnabled(hasTerrain);
        controls.density.SetEnabled(hasTerrain);
        controls.paint.SetEnabled(hasTerrain);
        controls.erase.SetEnabled(hasTerrain);
        controls.appearanceHeader.SetEnabled(hasTerrain);
        controls.length.SetEnabled(hasTerrain);
        controls.width.SetEnabled(hasTerrain);
        controls.randomness.SetEnabled(hasTerrain);
        controls.randomSeed.SetEnabled(hasTerrain);
        controls.uniformity.SetEnabled(hasTerrain);

        if (!hasTerrain)
        {
            controls.status.SetText("VEGETATION // CREATE TERRAIN FIRST");
            return;
        }

        auto* terrain = CurrentTerrain(session_);
        if (terrain != nullptr && !controls.densityDragging)
            controls.density.SetValue(bridge::CaptureVegetationDensity(*terrain));

        if (terrain != nullptr && !controls.appearanceDragging)
        {
            const auto settings =
                bridge::CaptureVegetationAppearanceSettings(*terrain);
            controls.length.SetValue(settings.length);
            controls.width.SetValue(settings.width);
            controls.randomness.SetValue(settings.randomness);
            controls.randomSeed.SetValue(static_cast<float>(settings.randomSeed));
            controls.uniformity.SetValue(settings.uniformity);
        }
    }

    void StudioRenderPath::TickWd01Vegetation()
    {
        if (session_ == nullptr)
            return;
        auto& scene = session_->Scenes().GetScene();
        if (scene.terrains.GetCount() == 0)
            return;
        auto* terrain = scene.terrains.GetComponent(scene.terrains.GetEntity(0));
        if (terrain == nullptr ||
            !bridge::IsManualVegetationEnabled(scene, *terrain))
        {
            return;
        }
        bridge::SynchronizeManualVegetation(scene, *terrain);
    }

    void StudioRenderPath::DisableWd01VegetationBrush()
    {
        auto& controls = Controls();
        controls.brushActive = false;
        controls.strokeActive = false;
        controls.strokeChanged = false;
        controls.strokeBefore.chunks.clear();
        controls.paint.SetText("PAINT");
        controls.erase.SetText("DELETE");
        if (controls.controlsCreated)
            controls.status.SetText("VEGETATION // READY");
    }

    bool StudioRenderPath::HandleWd01Vegetation(const XMFLOAT4& pointer)
    {
        auto& controls = Controls();
        if (!controls.controlsCreated || !controls.brushActive ||
            !terrainWorkspaceActive_ || session_ == nullptr ||
            flyCameraActive_ || GetGUI().HasFocus() ||
            !IsPointerOverViewport(pointer))
        {
            return false;
        }

        if (camera == nullptr)
            return false;

        auto& scene = session_->Scenes().GetScene();
        if (scene.terrains.GetCount() == 0)
            return false;
        const wi::ecs::Entity terrainEntity = scene.terrains.GetEntity(0);
        auto* terrain = scene.terrains.GetComponent(terrainEntity);
        if (terrain == nullptr)
            return false;

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
            const XMFLOAT4 brushColor =
                controls.brushMode == bridge::VegetationBrushMode::Paint
                    ? XMFLOAT4(0.25f, 0.92f, 0.42f, 0.24f)
                    : XMFLOAT4(0.95f, 0.28f, 0.18f, 0.24f);
            wi::renderer::DrawSphere(
                wi::primitive::Sphere(
                    picked.position,
                    controls.brushSizeValue),
                brushColor,
                true);
        }

        if (wi::input::Press(wi::input::MOUSE_BUTTON_LEFT) &&
            picked.entity != wi::ecs::INVALID_ENTITY)
        {
            std::string error;
            if (!bridge::EnsureDefaultGrassVegetation(scene, *terrain, &error))
            {
                controls.status.SetText("VEGETATION // " + error);
                return true;
            }
            controls.strokeActive = true;
            controls.strokeChanged = false;
            controls.strokeTerrain = terrainEntity;
            controls.strokeAffectedChunks = 0;
            controls.strokeAffectedVertices = 0;
            controls.strokeBefore.chunks.clear();
        }

        if (controls.strokeActive &&
            wi::input::Down(wi::input::MOUSE_BUTTON_LEFT) &&
            picked.entity != wi::ecs::INVALID_ENTITY)
        {
            const auto result = bridge::PaintVegetation(
                scene,
                *terrain,
                picked.position,
                controls.brushSizeValue,
                controls.brushMode,
                &controls.strokeBefore);
            if (!result.error.empty())
            {
                controls.status.SetText("VEGETATION // " + result.error);
                return true;
            }
            controls.strokeChanged |= result.changed;
            controls.strokeAffectedChunks += result.affectedChunks;
            controls.strokeAffectedVertices += result.affectedVertices;
            return true;
        }

        if (controls.strokeActive &&
            wi::input::Release(wi::input::MOUSE_BUTTON_LEFT))
        {
            controls.strokeActive = false;
            if (controls.strokeChanged)
            {
                auto after = bridge::CaptureVegetationAfter(
                    scene,
                    *terrain,
                    controls.strokeBefore);
                if (!after.chunks.empty())
                {
                    session_->Commands().RecordExecuted(
                        std::make_unique<bridge::VegetationStrokeCommand>(
                            scene,
                            controls.strokeTerrain,
                            std::move(controls.strokeBefore),
                            std::move(after)));
                    std::ostringstream message;
                    message << "LAST VEGETATION STROKE // "
                            << controls.strokeAffectedVertices
                            << " VERTICES";
                    controls.status.SetText(message.str());
                    RefreshStatus();
                }
            }
            controls.strokeBefore.chunks.clear();
            controls.strokeChanged = false;
            return true;
        }

        // An active vegetation tool owns left-click terrain interaction so the
        // sculpt/selection paths cannot fire underneath it.
        return true;
    }
}
