#include "StudioApplication.h"

#include "renegade/bridge/VegetationService.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>

namespace
{
    constexpr wi::Color VegetationMuted = wi::Color(178, 178, 176, 255);

    // StudioApplication's existing Terrain action rows end at Y 834.
    // WD01 is appended after them; the Wicked Window then expands its normal
    // vertical scroll range from the lowest visible child.
    constexpr float TerrainInspectorActionsBottom = 834.0f;
    constexpr float VegetationInspectorGap = 16.0f;
    constexpr float VegetationInspectorTop =
        TerrainInspectorActionsBottom + VegetationInspectorGap;
    constexpr float DefaultRenegadeGrassLength = 0.35f;

    enum class GrassField
    {
        Length,
        Width,
        Randomness,
        RandomSeed,
        Uniformity,
        Stiffness,
        Drag,
        GravityPower,
        SegmentCount,
        BillboardCount,
        ViewDistance,
    };

    enum class AtlasField
    {
        Width,
        Height,
        OffsetU,
        OffsetV,
        Size,
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

        renegade::studio::SceneInspectorButton movementHeader;
        renegade::studio::SceneInspectorSlider stiffness;
        renegade::studio::SceneInspectorSlider drag;
        renegade::studio::SceneInspectorSlider gravityPower;
        renegade::studio::SceneInspectorCheckBox cameraBend;

        renegade::studio::SceneInspectorButton geometryHeader;
        renegade::studio::SceneInspectorSlider segments;
        renegade::studio::SceneInspectorSlider billboards;
        renegade::studio::SceneInspectorSlider viewDistance;

        renegade::studio::SceneInspectorButton atlasHeader;
        wi::gui::Button texturePreview;
        wi::gui::Label spriteReadout;
        renegade::studio::SceneInspectorButton spritePrevious;
        renegade::studio::SceneInspectorButton spriteNext;
        renegade::studio::SceneInspectorButton spriteAdd;
        renegade::studio::SceneInspectorButton spriteRemove;
        renegade::studio::SceneInspectorSlider spriteWidth;
        renegade::studio::SceneInspectorSlider spriteHeight;
        renegade::studio::SceneInspectorSlider spriteOffsetU;
        renegade::studio::SceneInspectorSlider spriteOffsetV;
        renegade::studio::SceneInspectorSlider spriteSize;

        bool controlsCreated = false;
        bool brushActive = false;
        renegade::bridge::VegetationBrushMode brushMode =
            renegade::bridge::VegetationBrushMode::Paint;
        float brushSizeValue = 12.0f;

        bool densityDragging = false;
        float densityBefore = 1.0f;
        wi::ecs::Entity densityTerrain = wi::ecs::INVALID_ENTITY;

        bool settingsDragging = false;
        renegade::bridge::VegetationGrassSettings settingsBefore;
        wi::ecs::Entity settingsTerrain = wi::ecs::INVALID_ENTITY;

        bool appearanceExpanded = true;
        bool movementExpanded = false;
        bool geometryExpanded = false;
        bool atlasExpanded = false;
        std::size_t atlasIndex = 0;

        bool strokeActive = false;
        bool strokeChanged = false;
        bool lastStrokePositionValid = false;
        XMFLOAT3 lastStrokePosition = XMFLOAT3(0, 0, 0);
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

    void SetGrassField(
        renegade::bridge::VegetationGrassSettings& settings,
        const GrassField field,
        const float value)
    {
        switch (field)
        {
        case GrassField::Length:
            settings.length = value;
            break;
        case GrassField::Width:
            settings.width = value;
            break;
        case GrassField::Randomness:
            settings.randomness = value;
            break;
        case GrassField::RandomSeed:
            settings.randomSeed = static_cast<std::uint32_t>(
                std::max(1l, std::lround(value)));
            break;
        case GrassField::Uniformity:
            settings.uniformity = value;
            break;
        case GrassField::Stiffness:
            settings.stiffness = value;
            break;
        case GrassField::Drag:
            settings.drag = value;
            break;
        case GrassField::GravityPower:
            settings.gravityPower = value;
            break;
        case GrassField::SegmentCount:
            settings.segmentCount = static_cast<std::uint32_t>(
                std::max(1l, std::lround(value)));
            break;
        case GrassField::BillboardCount:
            settings.billboardCount = static_cast<std::uint32_t>(
                std::max(1l, std::lround(value)));
            break;
        case GrassField::ViewDistance:
            settings.viewDistance = value;
            break;
        }
    }

    void SetAtlasField(
        renegade::bridge::VegetationGrassSettings& settings,
        const std::size_t index,
        const AtlasField field,
        const float value)
    {
        if (index >= settings.atlasRects.size())
            return;
        auto& rect = settings.atlasRects[index];
        switch (field)
        {
        case AtlasField::Width:
            rect.texMulAdd.x = value;
            break;
        case AtlasField::Height:
            rect.texMulAdd.y = value;
            break;
        case AtlasField::OffsetU:
            rect.texMulAdd.z = value;
            break;
        case AtlasField::OffsetV:
            rect.texMulAdd.w = value;
            break;
        case AtlasField::Size:
            rect.size = value;
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
            "Pinned Wicked grass.wiscene material and HairParticle simulation. Renegade owns authoring only.");
        StyleLabel(controls.presetReadout);
        inspectorPanel_.AddWidget(&controls.presetReadout);

        controls.brushSize.Create(
            1.0f, 100.0f, controls.brushSizeValue, 990.0f,
            "WD01 Vegetation Brush Size", "BRUSH SIZE");
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
            0.0f, 4.0f, 1.0f, 1000.0f,
            "WD01 Vegetation Density", "DENSITY");
        controls.density.SetTooltip(
            "Creator-facing strand density multiplier. Raw Wicked Strand Count remains internal so the painted distribution stays stable.");
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
            auto* terrain = CurrentTerrain(session_);
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
        controls.density.OnValueCommitted([this](const float)
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
                        scene, state.densityTerrain, state.densityBefore, after));
                state.status.SetText("VEGETATION // DENSITY UPDATED");
                RefreshStatus();
            }
        });
        inspectorPanel_.AddWidget(&controls.density);

        controls.paint.Create("WD01 Paint Vegetation");
        controls.paint.SetText("PAINT");
        controls.paint.SetTooltip(
            "Paint the bundled Wicked grass onto terrain with a continuous interpolated viewport stroke.");
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
            "Delete painted grass under the same continuous viewport brush.");
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

        const auto createHeader = [this](
            SceneInspectorButton& button,
            const std::string& name,
            const std::string& text,
            bool VegetationControls::* expanded)
        {
            button.Create(name);
            button.SetText(text);
            button.OnClick([this, expanded, text](const wi::gui::EventArgs&)
            {
                auto& state = Controls();
                state.*expanded = !(state.*expanded);
                ResizeLayout();
                RefreshWd01VegetationControls(
                    CurrentTerrain(session_) != nullptr);
            });
            inspectorPanel_.AddWidget(&button);
        };

        createHeader(
            controls.appearanceHeader,
            "WD01 Appearance Header",
            "APPEARANCE",
            &VegetationControls::appearanceExpanded);
        createHeader(
            controls.movementHeader,
            "WD01 Movement Header",
            "MOVEMENT / RESPONSE",
            &VegetationControls::movementExpanded);
        createHeader(
            controls.geometryHeader,
            "WD01 Geometry Header",
            "GEOMETRY / PERFORMANCE",
            &VegetationControls::geometryExpanded);
        createHeader(
            controls.atlasHeader,
            "WD01 Atlas Header",
            "SPRITE / TEXTURE ATLAS",
            &VegetationControls::atlasExpanded);

        const auto createSettingSlider = [this](
            SceneInspectorSlider& slider,
            const float minimum,
            const float maximum,
            const float defaultValue,
            const float steps,
            const std::string& name,
            const std::string& label,
            const std::string& tooltip,
            const GrassField field)
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
                state.settingsDragging = true;
                state.settingsTerrain = terrainEntity;
                state.settingsBefore =
                    bridge::CaptureVegetationGrassSettings(*terrain);
            });
            slider.OnValuePreview([this, field](const float value)
            {
                auto* terrain = CurrentTerrain(session_);
                if (terrain == nullptr || session_ == nullptr)
                    return;
                auto settings = bridge::CaptureVegetationGrassSettings(*terrain);
                SetGrassField(settings, field, value);
                bridge::ApplyVegetationGrassSettings(
                    session_->Scenes().GetScene(), *terrain, settings);
            });
            slider.OnValueCommitted([this](const float)
            {
                auto& state = Controls();
                if (!state.settingsDragging || session_ == nullptr)
                    return;
                state.settingsDragging = false;
                auto& scene = session_->Scenes().GetScene();
                auto* terrain = scene.terrains.GetComponent(state.settingsTerrain);
                if (terrain == nullptr)
                    return;
                const auto after =
                    bridge::CaptureVegetationGrassSettings(*terrain);
                if (!bridge::VegetationGrassSettingsEqual(
                        state.settingsBefore, after))
                {
                    session_->Commands().RecordExecuted(
                        std::make_unique<bridge::SetVegetationGrassSettingsCommand>(
                            scene,
                            state.settingsTerrain,
                            state.settingsBefore,
                            after));
                    state.status.SetText("VEGETATION // GRASS SETTINGS UPDATED");
                    RefreshStatus();
                }
            });
            inspectorPanel_.AddWidget(&slider);
        };

        createSettingSlider(
            controls.length, 0.0f, 4.0f, DefaultRenegadeGrassLength, 1000.0f,
            "WD01 Grass Length", "LENGTH",
            "Native Wicked strand length. Renegade defaults to 0.35 m rather than the oversized demo preset.",
            GrassField::Length);
        createSettingSlider(
            controls.width, 0.0f, 2.0f, 1.0f, 1000.0f,
            "WD01 Grass Width", "WIDTH",
            "Native Wicked horizontal strand width multiplier.",
            GrassField::Width);
        createSettingSlider(
            controls.randomness, 0.0f, 1.0f, 0.2f, 1000.0f,
            "WD01 Grass Randomness", "RANDOMNESS",
            "Native Wicked grass length randomisation.",
            GrassField::Randomness);
        createSettingSlider(
            controls.randomSeed, 1.0f, 12345.0f, 1.0f, 12344.0f,
            "WD01 Grass Random Seed", "RANDOM SEED",
            "Native Wicked system-wide placement random seed.",
            GrassField::RandomSeed);
        createSettingSlider(
            controls.uniformity, 0.01f, 2.0f, 1.0f, 1000.0f,
            "WD01 Grass Uniformity", "UNIFORMITY",
            "Native Wicked modulation of sprite-selection distribution noise by particle position.",
            GrassField::Uniformity);

        createSettingSlider(
            controls.stiffness, 0.0f, 10.0f, 0.5f, 1000.0f,
            "WD01 Grass Stiffness", "STIFFNESS",
            "Native Wicked force returning grass toward its rest direction after wind/collider interaction.",
            GrassField::Stiffness);
        createSettingSlider(
            controls.drag, 0.0f, 1.0f, 0.1f, 1000.0f,
            "WD01 Grass Drag", "DRAG",
            "Native Wicked damping controlling how quickly grass movement settles.",
            GrassField::Drag);
        createSettingSlider(
            controls.gravityPower, 0.0f, 1.0f, 0.0f, 1000.0f,
            "WD01 Grass Gravity", "GRAVITY POWER",
            "Native Wicked constant downward force on simulated strands.",
            GrassField::GravityPower);

        controls.cameraBend.Create("CAMERA BEND");
        controls.cameraBend.SetTooltip(
            "Native Wicked camera-relative bend that helps hide flat grass cards when viewed from above.");
        controls.cameraBend.OnClick([this](const wi::gui::EventArgs& args)
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
            const auto before = bridge::CaptureVegetationGrassSettings(*terrain);
            auto after = before;
            after.cameraBendEnabled = args.bValue;
            if (bridge::ApplyVegetationGrassSettings(scene, *terrain, after))
            {
                after = bridge::CaptureVegetationGrassSettings(*terrain);
                session_->Commands().RecordExecuted(
                    std::make_unique<bridge::SetVegetationGrassSettingsCommand>(
                        scene, terrainEntity, before, after));
                state.status.SetText("VEGETATION // CAMERA BEND UPDATED");
                RefreshStatus();
            }
        });
        inspectorPanel_.AddWidget(&controls.cameraBend);

        createSettingSlider(
            controls.segments, 1.0f, 10.0f, 1.0f, 9.0f,
            "WD01 Grass Segments", "SEGMENTS",
            "Native Wicked simulated segment count per strand. Higher values improve bending but increase cost.",
            GrassField::SegmentCount);
        createSettingSlider(
            controls.billboards, 1.0f, 10.0f, 1.0f, 9.0f,
            "WD01 Grass Billboards", "BILLBOARDS",
            "Native Wicked crossed billboard geometry count per strand. Higher values look fuller but cost more.",
            GrassField::BillboardCount);
        createSettingSlider(
            controls.viewDistance, 0.0f, 1000.0f, 200.0f, 10000.0f,
            "WD01 Grass View Distance", "VIEW DISTANCE",
            "Native Wicked fade/cull distance for grass strands.",
            GrassField::ViewDistance);

        controls.texturePreview.Create("WD01 Grass Texture Preview");
        controls.texturePreview.SetText("");
        controls.texturePreview.SetTooltip(
            "Base-colour texture used by the native Wicked grass material.");
        inspectorPanel_.AddWidget(&controls.texturePreview);

        controls.spriteReadout.Create("WD01 Grass Sprite Readout");
        controls.spriteReadout.SetText("SPRITES // WHOLE TEXTURE");
        StyleLabel(controls.spriteReadout);
        inspectorPanel_.AddWidget(&controls.spriteReadout);

        const auto createSpriteButton = [this](
            SceneInspectorButton& button,
            const std::string& name,
            const std::string& text)
        {
            button.Create(name);
            button.SetText(text);
            inspectorPanel_.AddWidget(&button);
        };
        createSpriteButton(
            controls.spritePrevious,
            "WD01 Previous Sprite",
            "< PREV");
        createSpriteButton(
            controls.spriteNext,
            "WD01 Next Sprite",
            "NEXT >");
        createSpriteButton(
            controls.spriteAdd,
            "WD01 Add Sprite",
            "+ ADD RECT");
        createSpriteButton(
            controls.spriteRemove,
            "WD01 Remove Sprite",
            "REMOVE RECT");

        controls.spritePrevious.OnClick([this](const wi::gui::EventArgs&)
        {
            auto& state = Controls();
            auto* terrain = CurrentTerrain(session_);
            if (terrain == nullptr)
                return;
            const auto settings = bridge::CaptureVegetationGrassSettings(*terrain);
            if (settings.atlasRects.empty())
                return;
            state.atlasIndex = state.atlasIndex == 0
                ? settings.atlasRects.size() - 1
                : state.atlasIndex - 1;
            RefreshWd01VegetationControls(true);
        });
        controls.spriteNext.OnClick([this](const wi::gui::EventArgs&)
        {
            auto& state = Controls();
            auto* terrain = CurrentTerrain(session_);
            if (terrain == nullptr)
                return;
            const auto settings = bridge::CaptureVegetationGrassSettings(*terrain);
            if (settings.atlasRects.empty())
                return;
            state.atlasIndex = (state.atlasIndex + 1) % settings.atlasRects.size();
            RefreshWd01VegetationControls(true);
        });

        controls.spriteAdd.OnClick([this](const wi::gui::EventArgs&)
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
            const auto before = bridge::CaptureVegetationGrassSettings(*terrain);
            auto after = before;
            wi::HairParticleSystem::AtlasRect rect;
            if (!after.atlasRects.empty())
            {
                const std::size_t source = std::min(
                    state.atlasIndex,
                    after.atlasRects.size() - 1);
                rect = after.atlasRects[source];
            }
            after.atlasRects.push_back(rect);
            if (bridge::ApplyVegetationGrassSettings(scene, *terrain, after))
            {
                after = bridge::CaptureVegetationGrassSettings(*terrain);
                state.atlasIndex = after.atlasRects.size() - 1;
                session_->Commands().RecordExecuted(
                    std::make_unique<bridge::SetVegetationGrassSettingsCommand>(
                        scene, terrainEntity, before, after));
                state.status.SetText("VEGETATION // SPRITE RECT ADDED");
                RefreshStatus();
                RefreshWd01VegetationControls(true);
            }
        });

        controls.spriteRemove.OnClick([this](const wi::gui::EventArgs&)
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
            const auto before = bridge::CaptureVegetationGrassSettings(*terrain);
            if (before.atlasRects.empty())
                return;
            auto after = before;
            const std::size_t index = std::min(
                state.atlasIndex,
                after.atlasRects.size() - 1);
            after.atlasRects.erase(after.atlasRects.begin() + index);
            if (bridge::ApplyVegetationGrassSettings(scene, *terrain, after))
            {
                after = bridge::CaptureVegetationGrassSettings(*terrain);
                state.atlasIndex = after.atlasRects.empty()
                    ? 0
                    : std::min(index, after.atlasRects.size() - 1);
                session_->Commands().RecordExecuted(
                    std::make_unique<bridge::SetVegetationGrassSettingsCommand>(
                        scene, terrainEntity, before, after));
                state.status.SetText("VEGETATION // SPRITE RECT REMOVED");
                RefreshStatus();
                RefreshWd01VegetationControls(true);
            }
        });

        const auto createAtlasSlider = [this](
            SceneInspectorSlider& slider,
            const std::string& name,
            const std::string& label,
            const AtlasField field,
            const float minimum,
            const float maximum,
            const float defaultValue)
        {
            slider.Create(
                minimum, maximum, defaultValue, 1000.0f, name, label);
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
                const auto settings =
                    bridge::CaptureVegetationGrassSettings(*terrain);
                if (settings.atlasRects.empty())
                    return;
                state.settingsDragging = true;
                state.settingsTerrain = terrainEntity;
                state.settingsBefore = settings;
            });
            slider.OnValuePreview([this, field](const float value)
            {
                auto& state = Controls();
                auto* terrain = CurrentTerrain(session_);
                if (terrain == nullptr || session_ == nullptr)
                    return;
                auto settings = bridge::CaptureVegetationGrassSettings(*terrain);
                if (settings.atlasRects.empty())
                    return;
                const std::size_t index = std::min(
                    state.atlasIndex,
                    settings.atlasRects.size() - 1);
                SetAtlasField(settings, index, field, value);
                bridge::ApplyVegetationGrassSettings(
                    session_->Scenes().GetScene(), *terrain, settings);
            });
            slider.OnValueCommitted([this](const float)
            {
                auto& state = Controls();
                if (!state.settingsDragging || session_ == nullptr)
                    return;
                state.settingsDragging = false;
                auto& scene = session_->Scenes().GetScene();
                auto* terrain = scene.terrains.GetComponent(state.settingsTerrain);
                if (terrain == nullptr)
                    return;
                const auto after =
                    bridge::CaptureVegetationGrassSettings(*terrain);
                if (!bridge::VegetationGrassSettingsEqual(
                        state.settingsBefore, after))
                {
                    session_->Commands().RecordExecuted(
                        std::make_unique<bridge::SetVegetationGrassSettingsCommand>(
                            scene,
                            state.settingsTerrain,
                            state.settingsBefore,
                            after));
                    state.status.SetText("VEGETATION // SPRITE RECT UPDATED");
                    RefreshStatus();
                }
            });
            inspectorPanel_.AddWidget(&slider);
        };

        createAtlasSlider(
            controls.spriteWidth,
            "WD01 Sprite Width",
            "RECT WIDTH",
            AtlasField::Width,
            0.001f, 1.0f, 1.0f);
        createAtlasSlider(
            controls.spriteHeight,
            "WD01 Sprite Height",
            "RECT HEIGHT",
            AtlasField::Height,
            0.001f, 1.0f, 1.0f);
        createAtlasSlider(
            controls.spriteOffsetU,
            "WD01 Sprite Offset U",
            "U OFFSET",
            AtlasField::OffsetU,
            0.0f, 1.0f, 0.0f);
        createAtlasSlider(
            controls.spriteOffsetV,
            "WD01 Sprite Offset V",
            "V OFFSET",
            AtlasField::OffsetV,
            0.0f, 1.0f, 0.0f);
        createAtlasSlider(
            controls.spriteSize,
            "WD01 Sprite Size",
            "SPRITE SIZE",
            AtlasField::Size,
            0.0f, 2.0f, 1.0f);

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

        float y = VegetationInspectorTop;
        place(controls.sectionLabel, y, 20.0f);
        y += 22.0f;
        place(controls.presetReadout, y, 20.0f);
        y += 24.0f;
        place(controls.brushSize, y);
        y += 34.0f;
        place(controls.density, y);
        y += 34.0f;

        const float halfWidth = (fieldWidth - 8.0f) * 0.5f;
        controls.paint.SetPos(XMFLOAT2(12.0f, y));
        controls.paint.SetSize(XMFLOAT2(halfWidth, 28.0f));
        controls.erase.SetPos(XMFLOAT2(20.0f + halfWidth, y));
        controls.erase.SetSize(XMFLOAT2(halfWidth, 28.0f));
        y += 34.0f;
        place(controls.status, y, 34.0f);
        y += 42.0f;

        const auto layoutHeader = [&place, &y](wi::gui::Widget& header)
        {
            place(header, y, 28.0f);
            y += 34.0f;
        };
        const auto layoutSlider = [&place, &y](wi::gui::Widget& slider)
        {
            place(slider, y, 28.0f);
            y += 34.0f;
        };

        layoutHeader(controls.appearanceHeader);
        if (controls.appearanceExpanded)
        {
            layoutSlider(controls.length);
            layoutSlider(controls.width);
            layoutSlider(controls.randomness);
            layoutSlider(controls.randomSeed);
            layoutSlider(controls.uniformity);
        }

        layoutHeader(controls.movementHeader);
        if (controls.movementExpanded)
        {
            layoutSlider(controls.stiffness);
            layoutSlider(controls.drag);
            layoutSlider(controls.gravityPower);
            place(controls.cameraBend, y, 28.0f);
            y += 34.0f;
        }

        layoutHeader(controls.geometryHeader);
        if (controls.geometryExpanded)
        {
            layoutSlider(controls.segments);
            layoutSlider(controls.billboards);
            layoutSlider(controls.viewDistance);
        }

        layoutHeader(controls.atlasHeader);
        if (controls.atlasExpanded)
        {
            place(controls.texturePreview, y, 84.0f);
            y += 90.0f;
            place(controls.spriteReadout, y, 20.0f);
            y += 24.0f;

            const float navWidth = (fieldWidth - 8.0f) * 0.5f;
            controls.spritePrevious.SetPos(XMFLOAT2(12.0f, y));
            controls.spritePrevious.SetSize(XMFLOAT2(navWidth, 28.0f));
            controls.spriteNext.SetPos(XMFLOAT2(20.0f + navWidth, y));
            controls.spriteNext.SetSize(XMFLOAT2(navWidth, 28.0f));
            y += 34.0f;
            controls.spriteAdd.SetPos(XMFLOAT2(12.0f, y));
            controls.spriteAdd.SetSize(XMFLOAT2(navWidth, 28.0f));
            controls.spriteRemove.SetPos(XMFLOAT2(20.0f + navWidth, y));
            controls.spriteRemove.SetSize(XMFLOAT2(navWidth, 28.0f));
            y += 34.0f;

            layoutSlider(controls.spriteWidth);
            layoutSlider(controls.spriteHeight);
            layoutSlider(controls.spriteOffsetU);
            layoutSlider(controls.spriteOffsetV);
            layoutSlider(controls.spriteSize);
        }
    }

    void StudioRenderPath::RefreshWd01VegetationControls(const bool hasTerrain)
    {
        auto& controls = Controls();
        if (!controls.controlsCreated)
            return;

        const bool visible = terrainWorkspaceActive_;
        const auto show = [visible](wi::gui::Widget& widget, const bool condition = true)
        {
            widget.SetVisible(visible && condition);
        };

        show(controls.sectionLabel);
        show(controls.presetReadout);
        show(controls.brushSize);
        show(controls.density);
        show(controls.paint);
        show(controls.erase);
        show(controls.status);
        show(controls.appearanceHeader);
        show(controls.movementHeader);
        show(controls.geometryHeader);
        show(controls.atlasHeader);

        show(controls.length, controls.appearanceExpanded);
        show(controls.width, controls.appearanceExpanded);
        show(controls.randomness, controls.appearanceExpanded);
        show(controls.randomSeed, controls.appearanceExpanded);
        show(controls.uniformity, controls.appearanceExpanded);

        show(controls.stiffness, controls.movementExpanded);
        show(controls.drag, controls.movementExpanded);
        show(controls.gravityPower, controls.movementExpanded);
        show(controls.cameraBend, controls.movementExpanded);

        show(controls.segments, controls.geometryExpanded);
        show(controls.billboards, controls.geometryExpanded);
        show(controls.viewDistance, controls.geometryExpanded);

        show(controls.texturePreview, controls.atlasExpanded);
        show(controls.spriteReadout, controls.atlasExpanded);
        show(controls.spritePrevious, controls.atlasExpanded);
        show(controls.spriteNext, controls.atlasExpanded);
        show(controls.spriteAdd, controls.atlasExpanded);
        show(controls.spriteRemove, controls.atlasExpanded);
        show(controls.spriteWidth, controls.atlasExpanded);
        show(controls.spriteHeight, controls.atlasExpanded);
        show(controls.spriteOffsetU, controls.atlasExpanded);
        show(controls.spriteOffsetV, controls.atlasExpanded);
        show(controls.spriteSize, controls.atlasExpanded);

        controls.appearanceHeader.SetText(
            controls.appearanceExpanded ? "APPEARANCE // -" : "APPEARANCE // +");
        controls.movementHeader.SetText(
            controls.movementExpanded ? "MOVEMENT / RESPONSE // -" : "MOVEMENT / RESPONSE // +");
        controls.geometryHeader.SetText(
            controls.geometryExpanded ? "GEOMETRY / PERFORMANCE // -" : "GEOMETRY / PERFORMANCE // +");
        controls.atlasHeader.SetText(
            controls.atlasExpanded ? "SPRITE / TEXTURE ATLAS // -" : "SPRITE / TEXTURE ATLAS // +");

        wi::gui::Widget* terrainWidgets[] = {
            &controls.brushSize, &controls.density, &controls.paint, &controls.erase,
            &controls.appearanceHeader, &controls.length, &controls.width,
            &controls.randomness, &controls.randomSeed, &controls.uniformity,
            &controls.movementHeader, &controls.stiffness, &controls.drag,
            &controls.gravityPower, &controls.cameraBend,
            &controls.geometryHeader, &controls.segments, &controls.billboards,
            &controls.viewDistance, &controls.atlasHeader, &controls.texturePreview,
            &controls.spritePrevious, &controls.spriteNext, &controls.spriteAdd,
            &controls.spriteRemove, &controls.spriteWidth, &controls.spriteHeight,
            &controls.spriteOffsetU, &controls.spriteOffsetV, &controls.spriteSize,
        };
        for (auto* widget : terrainWidgets)
            widget->SetEnabled(hasTerrain);

        if (!hasTerrain)
        {
            controls.status.SetText("VEGETATION // CREATE TERRAIN FIRST");
            return;
        }

        auto* terrain = CurrentTerrain(session_);
        if (terrain == nullptr)
            return;

        if (!controls.densityDragging)
            controls.density.SetValue(bridge::CaptureVegetationDensity(*terrain));

        if (!controls.settingsDragging)
        {
            auto settings = bridge::CaptureVegetationGrassSettings(*terrain);
            if (!bridge::IsManualVegetationEnabled(
                    session_->Scenes().GetScene(), *terrain))
            {
                settings.length = DefaultRenegadeGrassLength;
            }

            controls.length.SetValue(settings.length);
            controls.width.SetValue(settings.width);
            controls.randomness.SetValue(settings.randomness);
            controls.randomSeed.SetValue(static_cast<float>(settings.randomSeed));
            controls.uniformity.SetValue(settings.uniformity);
            controls.stiffness.SetValue(settings.stiffness);
            controls.drag.SetValue(settings.drag);
            controls.gravityPower.SetValue(settings.gravityPower);
            controls.cameraBend.SetCheck(settings.cameraBendEnabled);
            controls.segments.SetValue(static_cast<float>(settings.segmentCount));
            controls.billboards.SetValue(static_cast<float>(settings.billboardCount));
            controls.viewDistance.SetValue(settings.viewDistance);

            const auto& texture = terrain->grass_material.textures[
                wi::scene::MaterialComponent::BASECOLORMAP].resource;
            controls.texturePreview.SetImage(texture);

            const bool hasRects = !settings.atlasRects.empty();
            if (hasRects)
            {
                controls.atlasIndex = std::min(
                    controls.atlasIndex,
                    settings.atlasRects.size() - 1);
                const auto& rect = settings.atlasRects[controls.atlasIndex];
                std::ostringstream readout;
                readout << "SPRITE RECT // "
                        << (controls.atlasIndex + 1)
                        << " / " << settings.atlasRects.size();
                controls.spriteReadout.SetText(readout.str());
                controls.spriteWidth.SetValue(rect.texMulAdd.x);
                controls.spriteHeight.SetValue(rect.texMulAdd.y);
                controls.spriteOffsetU.SetValue(rect.texMulAdd.z);
                controls.spriteOffsetV.SetValue(rect.texMulAdd.w);
                controls.spriteSize.SetValue(rect.size);
            }
            else
            {
                controls.atlasIndex = 0;
                controls.spriteReadout.SetText("SPRITES // WHOLE TEXTURE");
                controls.spriteWidth.SetValue(1.0f);
                controls.spriteHeight.SetValue(1.0f);
                controls.spriteOffsetU.SetValue(0.0f);
                controls.spriteOffsetV.SetValue(0.0f);
                controls.spriteSize.SetValue(1.0f);
            }

            controls.spritePrevious.SetEnabled(hasRects);
            controls.spriteNext.SetEnabled(hasRects);
            controls.spriteRemove.SetEnabled(hasRects);
            controls.spriteWidth.SetEnabled(hasRects);
            controls.spriteHeight.SetEnabled(hasRects);
            controls.spriteOffsetU.SetEnabled(hasRects);
            controls.spriteOffsetV.SetEnabled(hasRects);
            controls.spriteSize.SetEnabled(hasRects);
            controls.spriteAdd.SetEnabled(true);
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
        controls.lastStrokePositionValid = false;
        controls.settingsDragging = false;
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
            controls.lastStrokePosition = picked.position;
            controls.lastStrokePositionValid = true;
        }

        if (controls.strokeActive &&
            wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))
        {
            if (picked.entity == wi::ecs::INVALID_ENTITY)
            {
                controls.lastStrokePositionValid = false;
                return true;
            }

            const XMFLOAT3 current = picked.position;
            XMFLOAT3 start = controls.lastStrokePositionValid
                ? controls.lastStrokePosition
                : current;
            const float dx = current.x - start.x;
            const float dy = current.y - start.y;
            const float dz = current.z - start.z;
            const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

            // Wicked's stock painter substeps long pointer motion. WD01 does
            // the same in world space so a fast mouse stroke cannot leave
            // frame-rate-sized holes. Quarter-radius spacing gives overlapping
            // stamps while keeping the work bounded.
            const float spacing = std::max(
                0.5f,
                controls.brushSizeValue * 0.25f);
            const int sampleCount = std::clamp(
                static_cast<int>(std::ceil(distance / spacing)),
                1,
                64);

            for (int sample = 1; sample <= sampleCount; ++sample)
            {
                const float t = static_cast<float>(sample) /
                    static_cast<float>(sampleCount);
                const XMFLOAT3 center = XMFLOAT3(
                    start.x + dx * t,
                    start.y + dy * t,
                    start.z + dz * t);
                const auto result = bridge::PaintVegetation(
                    scene,
                    *terrain,
                    center,
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
            }

            controls.lastStrokePosition = current;
            controls.lastStrokePositionValid = true;
            return true;
        }

        if (controls.strokeActive &&
            wi::input::Release(wi::input::MOUSE_BUTTON_LEFT))
        {
            controls.strokeActive = false;
            controls.lastStrokePositionValid = false;
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
