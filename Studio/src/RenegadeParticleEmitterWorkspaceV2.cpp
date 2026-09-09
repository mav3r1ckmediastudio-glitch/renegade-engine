#include "RenegadeParticleEmitterWorkspace.h"

#include "RenegadeStudioChrome.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ParticleBlendModeService.h"
#include "renegade/bridge/ParticleEmitterService.h"
#include "renegade/bridge/ResourceImportService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using renegade::studio::RenegadeSlider;
    using renegade::studio::SceneInspectorButton;
    using renegade::studio::SceneInspectorCheckBox;
    using renegade::studio::SceneInspectorComboBox;

    constexpr float HeaderHeight = 136.0f;
    constexpr float FooterHeight = 34.0f;
    constexpr float RowHeight = 30.0f;
    constexpr float RowGap = 6.0f;
    constexpr float ScrollStep = 72.0f;
    constexpr float ScrollbarWidth = 5.0f;
    constexpr float NumericWidth = 70.0f;

    constexpr wi::Color Surface0 = wi::Color(8, 12, 16, 255);
    constexpr wi::Color Surface1 = wi::Color(12, 18, 22, 255);
    constexpr wi::Color Border = wi::Color(38, 52, 61, 255);
    constexpr wi::Color BorderSoft = wi::Color(25, 36, 43, 255);
    constexpr wi::Color TextStrong = wi::Color(244, 244, 244, 255);
    constexpr wi::Color TextSecondary = wi::Color(214, 222, 226, 255);
    constexpr wi::Color Muted = wi::Color(139, 151, 158, 255);
    constexpr wi::Color Forge = wi::Color(210, 91, 29, 255);
    constexpr wi::Color Error = wi::Color(229, 92, 92, 255);

    enum class ParticleSection : std::size_t
    {
        Appearance,
        Attachment,
        Emission,
        Particle,
        Motion,
        SpriteSheet,
        Advanced,
        Count,
    };

    constexpr std::size_t ParticleSectionCount =
        static_cast<std::size_t>(ParticleSection::Count);

    enum class TimestepPreset : std::uint64_t
    {
        Variable = 0,
        Hz120,
        Hz60,
        Hz30,
        LegacyCustom,
    };

    enum class SpriteAnimationMode : std::uint64_t
    {
        OverLifetime = 0,
        FixedFps,
    };

    float SliderSteps(
        const float minimum,
        const float maximum,
        const float increment) noexcept
    {
        if (!(maximum > minimum) || !(increment > 0.0f))
            return 1.0f;
        return std::max(
            1.0f,
            std::round((maximum - minimum) / increment));
    }

    void DrawRect(
        const float x,
        const float y,
        const float width,
        const float height,
        const wi::Color color,
        const wi::graphics::CommandList cmd)
    {
        if (width <= 0.0f || height <= 0.0f)
            return;
        wi::image::Params params(x, y, width, height, color);
        params.blendFlag = wi::enums::BLENDMODE_ALPHA;
        wi::image::Draw(nullptr, params, cmd);
    }

    void DrawBorderedRect(
        const float x,
        const float y,
        const float width,
        const float height,
        const wi::Color fill,
        const wi::Color border,
        const wi::graphics::CommandList cmd)
    {
        DrawRect(x, y, width, height, border, cmd);
        DrawRect(
            x + 1.0f, y + 1.0f,
            width - 2.0f, height - 2.0f,
            fill, cmd);
    }

    void DrawText(
        const std::string& value,
        const float x,
        const float y,
        const int size,
        const wi::Color color,
        const wi::graphics::CommandList cmd,
        const float bolden = 0.12f)
    {
        wi::font::Params params(
            x, y, size,
            wi::font::WIFALIGN_LEFT,
            wi::font::WIFALIGN_TOP,
            color,
            wi::Color::Transparent());
        params.bolden = bolden;
        wi::font::Draw(value, params, cmd);
    }

    std::string EntityName(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
    {
        if (entity == wi::ecs::INVALID_ENTITY)
            return "NONE // WORLD";
        if (const auto* name = scene.names.GetComponent(entity);
            name != nullptr && !name->name.empty())
        {
            return name->name;
        }
        return "Unnamed Entity " + std::to_string(entity);
    }

    float RadiansToDegrees(const float value) noexcept
    {
        return value * (180.0f / XM_PI);
    }

    float DegreesToRadians(const float value) noexcept
    {
        return value * (XM_PI / 180.0f);
    }

    std::uint32_t SpriteCellCount(
        const renegade::bridge::ParticleEmitterState& state) noexcept
    {
        const auto cells =
            static_cast<std::uint64_t>(std::max(1u, state.framesX)) *
            static_cast<std::uint64_t>(std::max(1u, state.framesY));
        return static_cast<std::uint32_t>(
            std::min<std::uint64_t>(1048576ull, cells));
    }

    const char* TimestepLabel(const float value) noexcept
    {
        if (value < 0.0f)
            return "VARIABLE";
        if (std::abs(value - (1.0f / 120.0f)) < 0.0001f)
            return "120 FPS";
        if (std::abs(value - (1.0f / 60.0f)) < 0.0001f)
            return "60 FPS";
        if (std::abs(value - (1.0f / 30.0f)) < 0.0001f)
            return "30 FPS";
        return "CUSTOM";
    }

    class ParticleSlider final : public RenegadeSlider
    {
    public:
        void CreateField(
            const float trackMinimum,
            const float trackMaximum,
            const float defaultValue,
            const float increment,
            const float hardMinimum,
            const float hardMaximum,
            const bool integer,
            const int precision,
            const std::string& name,
            std::string label,
            std::function<void(float)> commit)
        {
            hardMinimum_ = hardMinimum;
            hardMaximum_ = hardMaximum;
            integer_ = integer;
            precision_ = integer ? 0 : precision;
            commit_ = std::move(commit);

            RenegadeSlider::Create(
                trackMinimum,
                trackMaximum,
                defaultValue,
                SliderSteps(trackMinimum, trackMaximum, increment),
                name,
                std::move(label));

            valueInputField.SetFloatPrecision(precision_);
            valueInputField.SetTooltip(
                "Click the number, type an exact value, then press Enter. "
                "Slider dragging is deliberately coarse for creator-friendly tuning.");

            OnValueCommitted(
                [this](const float value)
                {
                    Commit(value);
                });

            valueInputField.OnInputAccepted(
                [this](const wi::gui::EventArgs& args)
                {
                    const float value =
                        std::isfinite(args.fValue) ? args.fValue : GetValue();
                    Commit(value);
                });
        }

        void Update(
            const wi::Canvas& canvas,
            const float dt) override
        {
            RenegadeSlider::Update(canvas, dt);

            const auto pos = GetPos();
            const auto size = GetSize();
            const float inputWidth =
                std::min(NumericWidth, std::max(48.0f, size.x * 0.38f));
            valueInputField.SetSize(XMFLOAT2(inputWidth, size.y));
            valueInputField.SetPos(
                XMFLOAT2(pos.x + size.x - inputWidth, pos.y));
        }

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            RenegadeSlider::Render(canvas, cmd);

            if (valueInputField.GetState() != wi::gui::ACTIVE)
                return;

            const auto pos = valueInputField.GetPos();
            const auto size = valueInputField.GetSize();
            auto& input =
                const_cast<wi::gui::TextInputField&>(valueInputField);
            DrawBorderedRect(
                pos.x, pos.y, size.x, size.y,
                wi::Color(6, 10, 12, 255), Forge, cmd);
            DrawText(
                input.GetCurrentInputValue(),
                pos.x + 7.0f,
                pos.y + std::max(4.0f, (size.y - 11.0f) * 0.5f - 1.0f),
                11,
                TextStrong,
                cmd,
                0.18f);
        }

    private:
        void Commit(float value)
        {
            value = std::clamp(value, hardMinimum_, hardMaximum_);
            if (integer_)
                value = std::round(value);
            SetValue(value);
            if (commit_)
                commit_(value);
        }

        float hardMinimum_ = 0.0f;
        float hardMaximum_ = 1.0f;
        bool integer_ = false;
        int precision_ = 3;
        std::function<void(float)> commit_;
    };
}

namespace renegade::studio
{
    struct RenegadeParticleEmitterWorkspace::Impl
    {
        bool created = false;
        bool active = false;
        bool pointerConsumed = false;
        bool refreshPending = true;
        bool autoRestartOnEdit = true;
        XMFLOAT4 bounds = {};
        float scrollY = 0.0f;
        float contentHeight = 0.0f;
        bridge::StudioSession* session = nullptr;
        wi::ecs::Entity selected = wi::ecs::INVALID_ENTITY;
        std::uint64_t sceneRevision = 0;
        std::string status = "PARTICLES // READY";
        bool statusError = false;
        std::string textureDisplay = "NO PARTICLE TEXTURE";
        std::string parentDisplay = "WORLD";
        std::vector<wi::gui::Widget*> controls;
        wi::jobsystem::context textureImportWorkload;

        std::array<bool, ParticleSectionCount> sectionExpanded = {
            true, false, false, false, false, false, false};

        SceneInspectorButton appearanceHeader;
        SceneInspectorButton attachmentHeader;
        SceneInspectorButton emissionHeader;
        SceneInspectorButton particleHeader;
        SceneInspectorButton motionHeader;
        SceneInspectorButton spriteHeader;
        SceneInspectorButton advancedHeader;

        SceneInspectorButton chooseTexture;
        SceneInspectorButton clearTexture;
        SceneInspectorButton restart;
        SceneInspectorButton burst;
        SceneInspectorButton resetDefaults;
        SceneInspectorButton useAllFrames;
        ParticleSlider burstCount;
        SceneInspectorCheckBox debugVisual;
        SceneInspectorCheckBox autoRestart;
        SceneInspectorComboBox parent;
        ParticleSlider localX, localY, localZ;
        ParticleSlider localScaleX, localScaleY, localScaleZ;

        SceneInspectorComboBox blendMode;
        SceneInspectorComboBox shaderType;
        ParticleSlider colorR, colorG, colorB, opacity;
        ParticleSlider emissiveR, emissiveG, emissiveB, emissiveStrength;

        SceneInspectorCheckBox paused, sorting, depthCollision, sph, volume;
        SceneInspectorCheckBox frameBlending, collidersDisabled, takeColorFromMesh;
        SceneInspectorComboBox emitterMesh;

        ParticleSlider maxParticles, emitCount, burstOnCreate, size, life;
        ParticleSlider rotationDegrees, particleScaleX, particleScaleY;
        ParticleSlider normalFactor, randomness, lifeRandomness;
        ParticleSlider colorRandomness, opacityStart, opacityEnd;
        ParticleSlider motionBlur, mass;

        ParticleSlider velocityX, velocityY, velocityZ;
        ParticleSlider gravityX, gravityY, gravityZ, drag, restitution;

        SceneInspectorComboBox animationMode;
        ParticleSlider framesX, framesY, frameCount, frameStart, frameRate;

        SceneInspectorComboBox fixedTimestep;
        ParticleSlider sphH, sphK, sphP0, sphE;

        [[nodiscard]] wi::scene::Scene* Scene() const noexcept
        {
            return session != nullptr
                ? &session->Scenes().GetScene()
                : nullptr;
        }

        [[nodiscard]] bool SelectedIsEmitter() const noexcept
        {
            const auto* scene = Scene();
            return scene != nullptr &&
                bridge::IsParticleEmitter(*scene, selected);
        }

        void SetStatus(std::string value, const bool error = false)
        {
            status = std::move(value);
            statusError = error;
        }

        template <typename Command, typename... Args>
        bool ExecuteCommand(Args&&... args)
        {
            if (session == nullptr)
                return false;
            const bool changed = session->Commands().Execute(
                std::make_unique<Command>(
                    std::forward<Args>(args)...));
            if (changed)
                refreshPending = true;
            return changed;
        }

        void RestartPreview()
        {
            auto* scene = Scene();
            auto* emitter =
                scene != nullptr && SelectedIsEmitter()
                ? scene->emitters.GetComponent(selected)
                : nullptr;
            if (emitter != nullptr)
                emitter->Restart();
        }

        template <typename Fn>
        void EditEmitter(
            Fn&& edit,
            const bool restartPreview = true)
        {
            auto* scene = Scene();
            if (scene == nullptr || !SelectedIsEmitter())
                return;

            const auto before =
                bridge::CaptureParticleEmitter(*scene, selected);
            auto after = before;
            edit(after);
            after = bridge::SanitizeParticleEmitterState(after);

            if (ExecuteCommand<bridge::SetParticleEmitterCommand>(
                    *scene, selected, before, after))
            {
                if (autoRestartOnEdit && restartPreview)
                    RestartPreview();
                SetStatus(
                    autoRestartOnEdit && restartPreview
                    ? "PARTICLE EMITTER // APPLIED + PREVIEW RESTARTED"
                    : "PARTICLE EMITTER // APPLIED");
            }
        }

        template <typename Fn>
        void EditTransform(Fn&& edit)
        {
            auto* scene = Scene();
            auto* transform =
                scene != nullptr && SelectedIsEmitter()
                ? scene->transforms.GetComponent(selected)
                : nullptr;
            if (transform == nullptr)
                return;

            const auto before = bridge::CaptureTransform(*transform);
            auto after = before;
            edit(after);
            if (ExecuteCommand<bridge::SetTransformCommand>(
                    *scene, selected, before, after))
            {
                SetStatus("ATTACHMENT OFFSET // APPLIED");
            }
        }

        void ToggleSection(const ParticleSection section)
        {
            const auto index =
                static_cast<std::size_t>(section);
            const bool opening = !sectionExpanded[index];
            sectionExpanded.fill(false);
            if (opening)
                sectionExpanded[index] = true;
            Layout();
        }

        void CreateEmitter()
        {
            if (session == nullptr)
                session = bridge::StudioSession::Current();
            if (session == nullptr ||
                !session->Projects().HasProject())
            {
                SetStatus(
                    "PARTICLES // OPEN A PROJECT FIRST", true);
                return;
            }

            auto& scene = session->Scenes().GetScene();
            const auto& camera = wi::scene::GetCamera();
            const XMFLOAT3 position{
                camera.Eye.x + camera.At.x * 5.0f,
                camera.Eye.y + camera.At.y * 5.0f,
                camera.Eye.z + camera.At.z * 5.0f};

            auto command =
                std::make_unique<bridge::CreateParticleEmitterCommand>(
                    scene, position);
            auto* createdCommand = command.get();

            if (!session->Commands().Execute(std::move(command)))
            {
                SetStatus(
                    "PARTICLES // CREATION FAILED", true);
                return;
            }

            selected = createdCommand->CreatedEntity();
            session->Selection().Select(selected);
            active = true;
            refreshPending = true;
            SetStatus(
                "PARTICLE EMITTER // CREATED 5M IN FRONT OF VIEW");
        }

        void ChooseParticleTexture()
        {
            if (session == nullptr ||
                !session->Projects().HasProject() ||
                !SelectedIsEmitter() ||
                wi::jobsystem::IsBusy(textureImportWorkload))
            {
                return;
            }

            const auto emitterEntity = selected;
            wi::helper::FileDialogParams params;
            params.type = wi::helper::FileDialogParams::OPEN;
            params.description =
                "Particle texture / sprite sheet";
            params.extensions = {
                "png", "tga", "dds", "jpg",
                "jpeg", "bmp", "hdr"};

            wi::helper::FileDialog(
                params,
                [this, emitterEntity](
                    const std::string& sourcePath)
                {
                    wi::eventhandler::Subscribe_Once(
                        wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                        [this, emitterEntity, sourcePath](
                            std::uint64_t)
                        {
                            if (sourcePath.empty() ||
                                session == nullptr)
                            {
                                return;
                            }

                            auto& liveScene =
                                session->Scenes().GetScene();
                            if (!bridge::IsParticleEmitter(
                                    liveScene, emitterEntity))
                            {
                                SetStatus(
                                    "PARTICLE TEXTURE // TARGET NO LONGER EXISTS",
                                    true);
                                return;
                            }

                            const auto format =
                                bridge::DetectResourceSourceFormat(
                                    sourcePath);
                            if (format ==
                                    bridge::ResourceSourceFormat::Unknown ||
                                bridge::ClassifyResourceSourceFormat(
                                    format) !=
                                    bridge::ResourceClass::Texture)
                            {
                                SetStatus(
                                    "PARTICLE TEXTURE // UNSUPPORTED IMAGE FORMAT",
                                    true);
                                return;
                            }

                            struct ImportState
                            {
                                std::string projectRoot;
                                bridge::StableId projectId;
                                wi::ecs::Entity emitter =
                                    wi::ecs::INVALID_ENTITY;
                                std::string sourcePath;
                                bridge::CreatorTextureImportResult imported;
                            };

                            auto state =
                                std::make_shared<ImportState>();
                            const auto& project =
                                session->Projects().CurrentProject();
                            state->projectRoot = project.rootPath;
                            state->projectId = project.projectId;
                            state->emitter = emitterEntity;
                            state->sourcePath = sourcePath;

                            SetStatus(
                                "PARTICLE TEXTURE // IMPORTING // " +
                                fs::u8path(sourcePath)
                                    .filename()
                                    .generic_u8string());

                            wi::jobsystem::Execute(
                                textureImportWorkload,
                                [this, state](
                                    wi::jobsystem::JobArgs)
                                {
                                    bridge::CreatorTextureWorkflowService workflow;
                                    state->imported =
                                        workflow.ImportTexture(
                                            state->projectRoot,
                                            state->projectId,
                                            state->sourcePath);

                                    wi::eventhandler::Subscribe_Once(
                                        wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                                        [this, state](
                                            std::uint64_t)
                                        {
                                            if (session == nullptr)
                                                return;

                                            if (!state->imported.succeeded)
                                            {
                                                SetStatus(
                                                    "PARTICLE TEXTURE // IMPORT FAILED // " +
                                                    state->imported.error,
                                                    true);
                                                return;
                                            }

                                            auto& scene =
                                                session->Scenes().GetScene();
                                            if (!bridge::IsParticleEmitter(
                                                    scene,
                                                    state->emitter))
                                            {
                                                SetStatus(
                                                    "PARTICLE TEXTURE // IMPORTED // TARGET GONE",
                                                    true);
                                                return;
                                            }

                                            bridge::PreparedMaterialTextureAsset prepared;
                                            std::string error;
                                            if (!bridge::PrepareMaterialTextureAsset(
                                                    state->projectRoot,
                                                    state->projectId,
                                                    state->imported.assetId,
                                                    prepared,
                                                    error))
                                            {
                                                SetStatus(
                                                    "PARTICLE TEXTURE // PREPARE FAILED // " +
                                                    error,
                                                    true);
                                                return;
                                            }

                                            auto command =
                                                std::make_unique<
                                                    bridge::SetMaterialTextureAssetCommand>(
                                                    scene,
                                                    state->emitter,
                                                    bridge::MaterialTextureSlot::BaseColor,
                                                    std::move(prepared));

                                            if (!session->Commands().Execute(
                                                    std::move(command)))
                                            {
                                                SetStatus(
                                                    "PARTICLE TEXTURE // ASSIGN FAILED",
                                                    true);
                                                return;
                                            }

                                            refreshPending = true;
                                            SetStatus(
                                                "PARTICLE TEXTURE // PROJECT-OWNED + ASSIGNED // " +
                                                fs::u8path(state->sourcePath)
                                                    .filename()
                                                    .generic_u8string());
                                        });
                                });
                        });
                });
        }

        void ClearParticleTexture()
        {
            auto* scene = Scene();
            if (scene != nullptr &&
                SelectedIsEmitter() &&
                ExecuteCommand<bridge::ClearMaterialTextureAssetCommand>(
                    *scene,
                    selected,
                    bridge::MaterialTextureSlot::BaseColor))
            {
                SetStatus("PARTICLE TEXTURE // CLEARED");
            }
        }

        void RestartEmitter()
        {
            RestartPreview();
            SetStatus("PARTICLE EMITTER // RESTARTED");
        }

        void BurstNow()
        {
            auto* scene = Scene();
            auto* emitter =
                scene != nullptr && SelectedIsEmitter()
                ? scene->emitters.GetComponent(selected)
                : nullptr;
            if (emitter != nullptr)
            {
                emitter->Burst(
                    std::max(
                        0,
                        static_cast<int>(
                            std::lround(burstCount.GetValue()))));
                SetStatus("PARTICLE EMITTER // BURST");
            }
        }

        void ResetEmitterDefaults()
        {
            auto* scene = Scene();
            if (scene == nullptr || !SelectedIsEmitter())
                return;

            const auto before =
                bridge::CaptureParticleEmitter(*scene, selected);
            auto after = bridge::MakeNewParticleEmitterState();

            if (ExecuteCommand<bridge::SetParticleEmitterCommand>(
                    *scene, selected, before, after))
            {
                RestartPreview();
                SetStatus(
                    "PARTICLE EMITTER // SAFE DEFAULTS RESTORED");
            }
        }

        void AttachTo(const wi::ecs::Entity parentEntity)
        {
            auto* scene = Scene();
            if (scene != nullptr &&
                SelectedIsEmitter() &&
                ExecuteCommand<bridge::SetParticleEmitterParentCommand>(
                    *scene, selected, parentEntity))
            {
                SetStatus(
                    parentEntity == wi::ecs::INVALID_ENTITY
                    ? "ATTACHMENT // DETACHED TO WORLD"
                    : "ATTACHMENT // NATIVE WICKED PARENT // LOCAL OFFSET PRESERVED");
            }
        }

        void SetBlendMode(const wi::enums::BLENDMODE mode)
        {
            auto* scene = Scene();
            if (scene != nullptr &&
                SelectedIsEmitter() &&
                ExecuteCommand<bridge::SetParticleBlendModeCommand>(
                    *scene, selected, mode))
            {
                SetStatus("BLEND MODE // APPLIED");
            }
        }

        void UseAllSpriteFrames()
        {
            auto* scene = Scene();
            if (scene == nullptr || !SelectedIsEmitter())
                return;

            const auto current =
                bridge::CaptureParticleEmitter(*scene, selected);
            const auto cells = SpriteCellCount(current);

            EditEmitter(
                [cells](bridge::ParticleEmitterState& state)
                {
                    state.frameCount = cells;
                    state.frameStart =
                        std::min(
                            state.frameStart,
                            state.frameCount - 1u);
                });
        }

        void ConfigureField(
            ParticleSlider& slider,
            const float trackMinimum,
            const float trackMaximum,
            const float hardMinimum,
            const float hardMaximum,
            const float initial,
            const float increment,
            const bool integer,
            const int precision,
            const char* name,
            const char* label,
            std::function<
                void(bridge::ParticleEmitterState&, float)> set,
            const bool restartPreview = true)
        {
            slider.CreateField(
                trackMinimum,
                trackMaximum,
                initial,
                increment,
                hardMinimum,
                hardMaximum,
                integer,
                precision,
                name,
                label,
                [this,
                    hardMinimum,
                    hardMaximum,
                    integer,
                    set = std::move(set),
                    restartPreview](
                    float value)
                {
                    float safe =
                        std::clamp(
                            value,
                            hardMinimum,
                            hardMaximum);
                    if (integer)
                        safe = std::round(safe);
                    EditEmitter(
                        [&](bridge::ParticleEmitterState& state)
                        {
                            set(state, safe);
                        },
                        restartPreview);
                });
        }

        void CreateControls()
        {
            const auto sectionHeader =
                [this](
                    SceneInspectorButton& button,
                    const char* name,
                    const char* title,
                    const ParticleSection section)
                {
                    button.Create(name);
                    button.SetText(
                        std::string("▶  ") + title);
                    button.SetTooltip(
                        "Expand or collapse this Particle Emitter Inspector section.");
                    button.OnClick(
                        [this, section](
                            const wi::gui::EventArgs&)
                        {
                            ToggleSection(section);
                        });
                };

            sectionHeader(appearanceHeader, "Particle Appearance Header", "APPEARANCE", ParticleSection::Appearance);
            sectionHeader(attachmentHeader, "Particle Attachment Header", "ATTACHMENT", ParticleSection::Attachment);
            sectionHeader(emissionHeader, "Particle Emission Header", "PLAYBACK + EMISSION", ParticleSection::Emission);
            sectionHeader(particleHeader, "Particle Properties Header", "PARTICLE", ParticleSection::Particle);
            sectionHeader(motionHeader, "Particle Motion Header", "MOTION", ParticleSection::Motion);
            sectionHeader(spriteHeader, "Particle Sprite Header", "SPRITE SHEET", ParticleSection::SpriteSheet);
            sectionHeader(advancedHeader, "Particle Advanced Header", "ADVANCED", ParticleSection::Advanced);

            chooseTexture.Create("Particle Choose Texture");
            chooseTexture.SetText("TEXTURE / SPRITESHEET...");
            chooseTexture.OnClick([this](const wi::gui::EventArgs&){ ChooseParticleTexture(); });

            clearTexture.Create("Particle Clear Texture");
            clearTexture.SetText("CLEAR");
            clearTexture.OnClick([this](const wi::gui::EventArgs&){ ClearParticleTexture(); });

            restart.Create("Particle Restart");
            restart.SetText("RESTART");
            restart.OnClick([this](const wi::gui::EventArgs&){ RestartEmitter(); });

            burst.Create("Particle Burst");
            burst.SetText("BURST NOW");
            burst.OnClick([this](const wi::gui::EventArgs&){ BurstNow(); });

            resetDefaults.Create("Particle Reset Defaults");
            resetDefaults.SetText("RESET SAFE DEFAULTS");
            resetDefaults.OnClick([this](const wi::gui::EventArgs&){ ResetEmitterDefaults(); });

            useAllFrames.Create("Particle Use All Frames");
            useAllFrames.SetText("USE ALL SHEET CELLS");
            useAllFrames.OnClick([this](const wi::gui::EventArgs&){ UseAllSpriteFrames(); });

            burstCount.CreateField(
                1.0f, 500.0f, 10.0f, 1.0f,
                1.0f, 10000.0f, true, 0,
                "Particle Burst Count", "BURST COUNT",
                [this](const float value){ burstCount.SetValue(value); });

            debugVisual.Create("DEBUG EMITTER VISUALISER: ");
            debugVisual.OnClick([](const wi::gui::EventArgs& args)
            { wi::renderer::SetToDrawDebugEmitters(args.bValue); });

            autoRestart.Create("AUTO RESTART PREVIEW ON EDIT: ");
            autoRestart.SetCheck(true);
            autoRestart.OnClick(
                [this](const wi::gui::EventArgs& args)
                {
                    autoRestartOnEdit = args.bValue;
                    SetStatus(autoRestartOnEdit
                        ? "PREVIEW // AUTO RESTART ON EDIT ENABLED"
                        : "PREVIEW // AUTO RESTART ON EDIT DISABLED");
                });

            parent.Create("Particle Parent");
            parent.OnSelect([this](const wi::gui::EventArgs& args)
            { AttachTo(static_cast<wi::ecs::Entity>(args.userdata)); });

            const auto transformSlider =
                [this](ParticleSlider& slider, const char* name,
                    const char* label, const int axis, const bool isScale)
                {
                    const float trackMinimum = isScale ? 0.01f : -100.0f;
                    const float trackMaximum = isScale ? 10.0f : 100.0f;
                    const float hardMinimum = isScale ? 0.001f : -10000.0f;
                    const float hardMaximum = isScale ? 100.0f : 10000.0f;
                    const float initial = isScale ? 1.0f : 0.0f;
                    const float increment = isScale ? 0.05f : 0.25f;

                    slider.CreateField(
                        trackMinimum, trackMaximum, initial, increment,
                        hardMinimum, hardMaximum, false, 3, name, label,
                        [this, axis, isScale](const float value)
                        {
                            EditTransform([=](bridge::TransformState& state)
                            {
                                XMFLOAT3& target = isScale ? state.scale : state.translation;
                                if (axis == 0) target.x = value;
                                else if (axis == 1) target.y = value;
                                else target.z = value;
                            });
                        });
                };

            transformSlider(localX, "Particle Local X", "LOCAL X", 0, false);
            transformSlider(localY, "Particle Local Y", "LOCAL Y", 1, false);
            transformSlider(localZ, "Particle Local Z", "LOCAL Z", 2, false);
            transformSlider(localScaleX, "Particle Local Scale X", "SCALE X", 0, true);
            transformSlider(localScaleY, "Particle Local Scale Y", "SCALE Y", 1, true);
            transformSlider(localScaleZ, "Particle Local Scale Z", "SCALE Z", 2, true);

            blendMode.Create("Particle Blend Mode");
            blendMode.AddItem("OPAQUE", wi::enums::BLENDMODE_OPAQUE);
            blendMode.AddItem("ALPHA", wi::enums::BLENDMODE_ALPHA);
            blendMode.AddItem("PREMULTIPLIED", wi::enums::BLENDMODE_PREMULTIPLIED);
            blendMode.AddItem("ADDITIVE", wi::enums::BLENDMODE_ADDITIVE);
            blendMode.AddItem("MULTIPLY", wi::enums::BLENDMODE_MULTIPLY);
            blendMode.AddItem("INVERSE", wi::enums::BLENDMODE_INVERSE);
            blendMode.OnSelect([this](const wi::gui::EventArgs& args)
            { SetBlendMode(static_cast<wi::enums::BLENDMODE>(args.userdata)); });

            shaderType.Create("Particle Shader Type");
            shaderType.AddItem("SOFT", wi::EmittedParticleSystem::SOFT);
            shaderType.AddItem("SOFT DISTORTION", wi::EmittedParticleSystem::SOFT_DISTORTION);
            shaderType.AddItem("SIMPLE", wi::EmittedParticleSystem::SIMPLE);
            shaderType.AddItem("SOFT LIGHTING", wi::EmittedParticleSystem::SOFT_LIGHTING);
            shaderType.OnSelect([this](const wi::gui::EventArgs& args)
            {
                EditEmitter([&](bridge::ParticleEmitterState& state)
                {
                    state.shaderType = static_cast<wi::EmittedParticleSystem::PARTICLESHADERTYPE>(args.userdata);
                }, false);
            });

            ConfigureField(colorR, 0, 1, 0, 1, 1, 0.02f, false, 2, "Particle Color R", "COLOR R", [](auto& s, float v){ s.color.x = v; }, false);
            ConfigureField(colorG, 0, 1, 0, 1, 1, 0.02f, false, 2, "Particle Color G", "COLOR G", [](auto& s, float v){ s.color.y = v; }, false);
            ConfigureField(colorB, 0, 1, 0, 1, 1, 0.02f, false, 2, "Particle Color B", "COLOR B", [](auto& s, float v){ s.color.z = v; }, false);
            ConfigureField(opacity, 0, 1, 0, 1, 1, 0.02f, false, 2, "Particle Opacity", "OPACITY", [](auto& s, float v){ s.color.w = v; }, false);
            ConfigureField(emissiveR, 0, 1, 0, 1, 0, 0.02f, false, 2, "Particle Emissive R", "EMISSIVE R", [](auto& s, float v){ s.emissiveColor.x = v; }, false);
            ConfigureField(emissiveG, 0, 1, 0, 1, 0, 0.02f, false, 2, "Particle Emissive G", "EMISSIVE G", [](auto& s, float v){ s.emissiveColor.y = v; }, false);
            ConfigureField(emissiveB, 0, 1, 0, 1, 0, 0.02f, false, 2, "Particle Emissive B", "EMISSIVE B", [](auto& s, float v){ s.emissiveColor.z = v; }, false);
            ConfigureField(emissiveStrength, 0, 20, 0, 100, 0, 0.25f, false, 2, "Particle Emissive Strength", "EMISSIVE", [](auto& s, float v){ s.emissiveStrength = v; }, false);

            const auto flag = [this](SceneInspectorCheckBox& box, const char* label,
                std::function<void(bridge::ParticleEmitterState&, bool)> set)
            {
                box.Create(label);
                box.OnClick([this, set = std::move(set)](const wi::gui::EventArgs& args)
                {
                    EditEmitter([&](bridge::ParticleEmitterState& state)
                    { set(state, args.bValue); }, false);
                });
            };

            flag(paused, "PAUSED: ", [](auto& s, bool v){ s.paused = v; });
            flag(sorting, "SORT PARTICLES: ", [](auto& s, bool v){ s.sorted = v; });
            flag(depthCollision, "DEPTH COLLISION: ", [](auto& s, bool v){ s.depthCollision = v; });
            flag(sph, "SPH FLUID SIM: ", [](auto& s, bool v){ s.sph = v; });
            flag(volume, "EMIT IN VOLUME: ", [](auto& s, bool v){ s.volume = v; });
            flag(frameBlending, "SPRITE FRAME BLENDING: ", [](auto& s, bool v){ s.frameBlending = v; });
            flag(collidersDisabled, "DISABLE COLLIDERS: ", [](auto& s, bool v){ s.collidersDisabled = v; });
            flag(takeColorFromMesh, "TAKE COLOR FROM MESH: ", [](auto& s, bool v){ s.takeColorFromMesh = v; });

            emitterMesh.Create("Particle Emitter Mesh");
            emitterMesh.OnSelect([this](const wi::gui::EventArgs& args)
            {
                EditEmitter([&](bridge::ParticleEmitterState& state)
                { state.meshId = static_cast<wi::ecs::Entity>(args.userdata); });
            });

            ConfigureField(maxParticles, 100, 10000, 100, 1000000, 1000, 100, true, 0, "Particle Max", "MAX PARTICLES", [](auto& s, float v){ s.maxParticles = static_cast<std::uint32_t>(std::lround(v)); });
            ConfigureField(emitCount, 0, 120, 0, 10000, 20, 1, false, 1, "Particle Emit Count", "EMIT / SEC", [](auto& s, float v){ s.emitCount = v; });
            ConfigureField(burstOnCreate, 0, 500, 0, 100000, 0, 1, true, 0, "Particle Burst On Create", "BURST ON CREATE", [](auto& s, float v){ s.burstOnCreate = static_cast<int>(std::lround(v)); });
            ConfigureField(size, 0.01f, 10.0f, 0.001f, 100.0f, 0.2f, 0.05f, false, 3, "Particle Size", "SIZE", [](auto& s, float v){ s.size = v; });
            ConfigureField(life, 0.05f, 30.0f, 0.001f, 600.0f, 2.0f, 0.1f, false, 3, "Particle Life", "LIFETIME", [](auto& s, float v){ s.life = v; });
            ConfigureField(rotationDegrees, -360, 360, -360, 360, 0, 2, false, 1, "Particle Rotation", "ROTATION DEG", [](auto& s, float v){ s.rotation = DegreesToRadians(v); });
            ConfigureField(particleScaleX, 0.05f, 10.0f, 0.001f, 100.0f, 1, 0.05f, false, 3, "Particle Scale X", "PARTICLE SCALE X", [](auto& s, float v){ s.scaleX = v; });
            ConfigureField(particleScaleY, 0.05f, 10.0f, 0.001f, 100.0f, 1, 0.05f, false, 3, "Particle Scale Y", "PARTICLE SCALE Y", [](auto& s, float v){ s.scaleY = v; });
            ConfigureField(normalFactor, -5, 5, -10, 10, 0, 0.05f, false, 2, "Particle Normal Factor", "NORMAL FACTOR", [](auto& s, float v){ s.normalFactor = v; });
            ConfigureField(randomness, 0, 1, 0, 1, 0.35f, 0.02f, false, 2, "Particle Random", "RANDOMNESS", [](auto& s, float v){ s.randomFactor = v; });
            ConfigureField(lifeRandomness, 0, 2, 0, 10, 0.25f, 0.05f, false, 2, "Particle Life Random", "LIFE RANDOM", [](auto& s, float v){ s.randomLife = v; });
            ConfigureField(colorRandomness, 0, 1, 0, 2, 0, 0.02f, false, 2, "Particle Color Random", "COLOR RANDOM", [](auto& s, float v){ s.randomColor = v; });
            ConfigureField(opacityStart, 0, 1, 0, 1, 0.1f, 0.02f, false, 2, "Particle Opacity Start", "OPACITY PEAK START", [](auto& s, float v){ s.opacityPeakStart = v; }, false);
            ConfigureField(opacityEnd, 0, 1, 0, 1, 0.5f, 0.02f, false, 2, "Particle Opacity End", "OPACITY PEAK END", [](auto& s, float v){ s.opacityPeakEnd = v; }, false);
            ConfigureField(motionBlur, 0, 1, 0, 1, 0, 0.02f, false, 2, "Particle Motion Blur", "MOTION BLUR", [](auto& s, float v){ s.motionBlurAmount = v; });
            ConfigureField(mass, 0.01f, 50.0f, 0.001f, 1000.0f, 1, 0.1f, false, 3, "Particle Mass", "MASS", [](auto& s, float v){ s.mass = v; });

            ConfigureField(velocityX, -50, 50, -1000, 1000, 0, 0.25f, false, 2, "Particle Velocity X", "VELOCITY X", [](auto& s, float v){ s.velocity.x = v; });
            ConfigureField(velocityY, -50, 50, -1000, 1000, 0.75f, 0.25f, false, 2, "Particle Velocity Y", "VELOCITY Y", [](auto& s, float v){ s.velocity.y = v; });
            ConfigureField(velocityZ, -50, 50, -1000, 1000, 0, 0.25f, false, 2, "Particle Velocity Z", "VELOCITY Z", [](auto& s, float v){ s.velocity.z = v; });
            ConfigureField(gravityX, -50, 50, -1000, 1000, 0, 0.25f, false, 2, "Particle Gravity X", "GRAVITY X", [](auto& s, float v){ s.gravity.x = v; });
            ConfigureField(gravityY, -50, 50, -1000, 1000, 0, 0.25f, false, 2, "Particle Gravity Y", "GRAVITY Y", [](auto& s, float v){ s.gravity.y = v; });
            ConfigureField(gravityZ, -50, 50, -1000, 1000, 0, 0.25f, false, 2, "Particle Gravity Z", "GRAVITY Z", [](auto& s, float v){ s.gravity.z = v; });
            ConfigureField(drag, 0, 1, 0, 1, 1, 0.02f, false, 2, "Particle Drag", "DRAG", [](auto& s, float v){ s.drag = v; });
            ConfigureField(restitution, 0, 1, 0, 1, 0.98f, 0.02f, false, 2, "Particle Restitution", "RESTITUTION", [](auto& s, float v){ s.restitution = v; });

            animationMode.Create("Particle Sprite Animation Mode");
            animationMode.AddItem("OVER PARTICLE LIFETIME", static_cast<std::uint64_t>(SpriteAnimationMode::OverLifetime));
            animationMode.AddItem("FIXED FPS", static_cast<std::uint64_t>(SpriteAnimationMode::FixedFps));
            animationMode.OnSelect([this](const wi::gui::EventArgs& args)
            {
                const auto mode = static_cast<SpriteAnimationMode>(args.userdata);
                EditEmitter([mode](bridge::ParticleEmitterState& state)
                {
                    if (mode == SpriteAnimationMode::OverLifetime)
                        state.frameRate = 0.0f;
                    else if (state.frameRate <= 0.0f)
                        state.frameRate = 24.0f;
                });
            });

            framesX.CreateField(
                1, 128, 1, 1, 1, 1024, true, 0,
                "Particle Frames X", "SPRITE COLUMNS",
                [this](float value)
                {
                    EditEmitter([value](bridge::ParticleEmitterState& state)
                    {
                        state.framesX = static_cast<std::uint32_t>(std::lround(value));
                        const auto cells = SpriteCellCount(state);
                        state.frameCount = std::min(state.frameCount, cells);
                        state.frameStart = std::min(state.frameStart, state.frameCount - 1u);
                    });
                });

            framesY.CreateField(
                1, 128, 1, 1, 1, 1024, true, 0,
                "Particle Frames Y", "SPRITE ROWS",
                [this](float value)
                {
                    EditEmitter([value](bridge::ParticleEmitterState& state)
                    {
                        state.framesY = static_cast<std::uint32_t>(std::lround(value));
                        const auto cells = SpriteCellCount(state);
                        state.frameCount = std::min(state.frameCount, cells);
                        state.frameStart = std::min(state.frameStart, state.frameCount - 1u);
                    });
                });

            frameCount.CreateField(
                1, 256, 1, 1, 1, 1048576, true, 0,
                "Particle Frame Count", "FRAMES USED",
                [this](float value)
                {
                    auto* scene = Scene();
                    if (scene == nullptr || !SelectedIsEmitter())
                        return;
                    const auto current = bridge::CaptureParticleEmitter(*scene, selected);
                    const auto cells = SpriteCellCount(current);
                    const auto safe = std::clamp<std::uint32_t>(
                        static_cast<std::uint32_t>(std::lround(value)), 1u, cells);
                    frameCount.SetValue(static_cast<float>(safe));
                    EditEmitter([safe](bridge::ParticleEmitterState& state)
                    {
                        state.frameCount = safe;
                        state.frameStart = std::min(state.frameStart, state.frameCount - 1u);
                    });
                });

            frameStart.CreateField(
                0, 255, 0, 1, 0, 1048575, true, 0,
                "Particle Frame Start", "START FRAME",
                [this](float value)
                {
                    auto* scene = Scene();
                    if (scene == nullptr || !SelectedIsEmitter())
                        return;
                    const auto current = bridge::CaptureParticleEmitter(*scene, selected);
                    const auto last = current.frameCount > 0 ? current.frameCount - 1u : 0u;
                    const auto safe = std::min<std::uint32_t>(
                        static_cast<std::uint32_t>(std::max(0.0f, std::round(value))), last);
                    frameStart.SetValue(static_cast<float>(safe));
                    EditEmitter([safe](bridge::ParticleEmitterState& state)
                    { state.frameStart = safe; });
                });

            frameRate.CreateField(
                1, 120, 24, 1, 1, 240, false, 1,
                "Particle Frame Rate", "ANIMATION FPS",
                [this](float value)
                {
                    EditEmitter([value](bridge::ParticleEmitterState& state)
                    { state.frameRate = value; });
                });

            fixedTimestep.Create("Particle Fixed Timestep");
            fixedTimestep.AddItem("VARIABLE // FRAME DELTA", static_cast<std::uint64_t>(TimestepPreset::Variable));
            fixedTimestep.AddItem("FIXED // 120 FPS", static_cast<std::uint64_t>(TimestepPreset::Hz120));
            fixedTimestep.AddItem("FIXED // 60 FPS", static_cast<std::uint64_t>(TimestepPreset::Hz60));
            fixedTimestep.AddItem("FIXED // 30 FPS", static_cast<std::uint64_t>(TimestepPreset::Hz30));
            fixedTimestep.AddItem("CUSTOM // LEGACY VALUE", static_cast<std::uint64_t>(TimestepPreset::LegacyCustom));
            fixedTimestep.OnSelect([this](const wi::gui::EventArgs& args)
            {
                const auto preset = static_cast<TimestepPreset>(args.userdata);
                if (preset == TimestepPreset::LegacyCustom)
                    return;
                float value = -1.0f;
                if (preset == TimestepPreset::Hz120) value = 1.0f / 120.0f;
                else if (preset == TimestepPreset::Hz60) value = 1.0f / 60.0f;
                else if (preset == TimestepPreset::Hz30) value = 1.0f / 30.0f;
                EditEmitter([value](bridge::ParticleEmitterState& state)
                { state.fixedTimestep = value; });
            });

            ConfigureField(sphH, 0.05f, 10, 0.001f, 100, 1, 0.05f, false, 3, "Particle SPH H", "SPH H", [](auto& s, float v){ s.sphH = v; });
            ConfigureField(sphK, 0, 1000, 0, 10000, 250, 10, false, 1, "Particle SPH K", "SPH K", [](auto& s, float v){ s.sphK = v; });
            ConfigureField(sphP0, 0.05f, 50, 0.001f, 1000, 1, 0.1f, false, 3, "Particle SPH P0", "SPH P0", [](auto& s, float v){ s.sphP0 = v; });
            ConfigureField(sphE, 0, 1, 0, 100, 0.018f, 0.005f, false, 3, "Particle SPH E", "SPH VISCOSITY", [](auto& s, float v){ s.sphE = v; });

            controls = {
                &appearanceHeader, &attachmentHeader, &emissionHeader, &particleHeader,
                &motionHeader, &spriteHeader, &advancedHeader,
                &chooseTexture, &clearTexture, &restart, &burst, &resetDefaults, &useAllFrames,
                &burstCount, &debugVisual, &autoRestart, &parent,
                &localX, &localY, &localZ, &localScaleX, &localScaleY, &localScaleZ,
                &blendMode, &shaderType,
                &colorR, &colorG, &colorB, &opacity,
                &emissiveR, &emissiveG, &emissiveB, &emissiveStrength,
                &paused, &sorting, &depthCollision, &sph, &volume,
                &frameBlending, &collidersDisabled, &takeColorFromMesh, &emitterMesh,
                &maxParticles, &emitCount, &burstOnCreate, &size, &life,
                &rotationDegrees, &particleScaleX, &particleScaleY,
                &normalFactor, &randomness, &lifeRandomness, &colorRandomness,
                &opacityStart, &opacityEnd, &motionBlur, &mass,
                &velocityX, &velocityY, &velocityZ,
                &gravityX, &gravityY, &gravityZ, &drag, &restitution,
                &animationMode, &framesX, &framesY, &frameCount, &frameStart, &frameRate,
                &fixedTimestep, &sphH, &sphK, &sphP0, &sphE,
            };

            for (auto* control : controls)
                control->SetVisible(false);
            created = true;
        }

        void Refresh()
        {
            if (session == nullptr)
                session = bridge::StudioSession::Current();

            selected = session != nullptr && session->Selection().HasSelection()
                ? session->Selection().SelectedEntity()
                : wi::ecs::INVALID_ENTITY;

            if (!SelectedIsEmitter())
            {
                for (auto* control : controls)
                    control->SetVisible(false);
                textureDisplay = "NO PARTICLE EMITTER SELECTED";
                parentDisplay = "WORLD";
                refreshPending = false;
                return;
            }

            auto& scene = *Scene();
            const auto state = bridge::CaptureParticleEmitter(scene, selected);
            const auto* transform = scene.transforms.GetComponent(selected);

            parent.ClearItems();
            parent.AddItem("NONE // WORLD", static_cast<std::uint64_t>(wi::ecs::INVALID_ENTITY));
            for (const auto& candidate : bridge::CollectParticleAttachmentCandidates(scene, selected))
                parent.AddItem(candidate.name, static_cast<std::uint64_t>(candidate.entity));
            const auto currentParent = bridge::ParticleEmitterParent(scene, selected);
            parent.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(currentParent));
            parentDisplay = EntityName(scene, currentParent);

            emitterMesh.ClearItems();
            emitterMesh.AddItem("NO MESH // POINT / VOLUME", static_cast<std::uint64_t>(wi::ecs::INVALID_ENTITY));
            for (std::size_t index = 0; index < scene.meshes.GetCount(); ++index)
            {
                const auto entity = scene.meshes.GetEntity(index);
                emitterMesh.AddItem(EntityName(scene, entity), static_cast<std::uint64_t>(entity));
            }
            emitterMesh.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(state.meshId));

            blendMode.SetSelectedByUserdataWithoutCallback(
                static_cast<std::uint64_t>(bridge::CaptureParticleBlendMode(scene, selected)));
            shaderType.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(state.shaderType));

            colorR.SetValue(state.color.x); colorG.SetValue(state.color.y);
            colorB.SetValue(state.color.z); opacity.SetValue(state.color.w);
            emissiveR.SetValue(state.emissiveColor.x); emissiveG.SetValue(state.emissiveColor.y);
            emissiveB.SetValue(state.emissiveColor.z); emissiveStrength.SetValue(state.emissiveStrength);

            paused.SetCheck(state.paused); sorting.SetCheck(state.sorted);
            depthCollision.SetCheck(state.depthCollision); sph.SetCheck(state.sph);
            volume.SetCheck(state.volume); frameBlending.SetCheck(state.frameBlending);
            collidersDisabled.SetCheck(state.collidersDisabled);
            takeColorFromMesh.SetCheck(state.takeColorFromMesh);
            autoRestart.SetCheck(autoRestartOnEdit);

            maxParticles.SetValue(static_cast<float>(state.maxParticles));
            emitCount.SetValue(state.emitCount);
            burstOnCreate.SetValue(static_cast<float>(state.burstOnCreate));
            size.SetValue(state.size); life.SetValue(state.life);
            rotationDegrees.SetValue(RadiansToDegrees(state.rotation));
            particleScaleX.SetValue(state.scaleX); particleScaleY.SetValue(state.scaleY);
            normalFactor.SetValue(state.normalFactor); randomness.SetValue(state.randomFactor);
            lifeRandomness.SetValue(state.randomLife); colorRandomness.SetValue(state.randomColor);
            opacityStart.SetValue(state.opacityPeakStart); opacityEnd.SetValue(state.opacityPeakEnd);
            motionBlur.SetValue(state.motionBlurAmount); mass.SetValue(state.mass);

            velocityX.SetValue(state.velocity.x); velocityY.SetValue(state.velocity.y);
            velocityZ.SetValue(state.velocity.z); gravityX.SetValue(state.gravity.x);
            gravityY.SetValue(state.gravity.y); gravityZ.SetValue(state.gravity.z);
            drag.SetValue(state.drag); restitution.SetValue(state.restitution);

            framesX.SetValue(static_cast<float>(state.framesX));
            framesY.SetValue(static_cast<float>(state.framesY));
            frameCount.SetValue(static_cast<float>(state.frameCount));
            frameStart.SetValue(static_cast<float>(state.frameStart));

            if (state.frameRate <= 0.0f)
            {
                animationMode.SetSelectedByUserdataWithoutCallback(
                    static_cast<std::uint64_t>(SpriteAnimationMode::OverLifetime));
                frameRate.SetValue(0.0f);
                frameRate.SetEnabled(false);
            }
            else
            {
                animationMode.SetSelectedByUserdataWithoutCallback(
                    static_cast<std::uint64_t>(SpriteAnimationMode::FixedFps));
                frameRate.SetValue(state.frameRate);
                frameRate.SetEnabled(true);
            }

            TimestepPreset timestep = TimestepPreset::LegacyCustom;
            if (state.fixedTimestep < 0.0f) timestep = TimestepPreset::Variable;
            else if (std::abs(state.fixedTimestep - (1.0f / 120.0f)) < 0.0001f) timestep = TimestepPreset::Hz120;
            else if (std::abs(state.fixedTimestep - (1.0f / 60.0f)) < 0.0001f) timestep = TimestepPreset::Hz60;
            else if (std::abs(state.fixedTimestep - (1.0f / 30.0f)) < 0.0001f) timestep = TimestepPreset::Hz30;
            fixedTimestep.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(timestep));

            sphH.SetValue(state.sphH); sphK.SetValue(state.sphK);
            sphP0.SetValue(state.sphP0); sphE.SetValue(state.sphE);
            debugVisual.SetCheck(wi::renderer::GetToDrawDebugEmitters());

            if (transform != nullptr)
            {
                const auto local = bridge::CaptureTransform(*transform);
                localX.SetValue(local.translation.x); localY.SetValue(local.translation.y);
                localZ.SetValue(local.translation.z);
                localScaleX.SetValue(local.scale.x); localScaleY.SetValue(local.scale.y);
                localScaleZ.SetValue(local.scale.z);
            }

            textureDisplay = "NO PARTICLE TEXTURE // WICKED DEFAULT WHITE";
            if (const auto* material = scene.materials.GetComponent(selected))
            {
                const auto& texture = material->textures[
                    bridge::WickedTextureSlot(bridge::MaterialTextureSlot::BaseColor)];
                if (!texture.name.empty())
                    textureDisplay = fs::u8path(texture.name).filename().generic_u8string();
                else if (texture.resource.IsValid())
                    textureDisplay = "PARTICLE TEXTURE // LOADED";
            }

            refreshPending = false;
        }

        void ClampScroll()
        {
            const float visible = std::max(0.0f, bounds.w - HeaderHeight - FooterHeight);
            scrollY = std::clamp(scrollY, 0.0f, std::max(0.0f, contentHeight - visible));
        }

        void Layout()
        {
            for (auto* control : controls)
                control->SetVisible(false);

            const float x = bounds.x + 12.0f;
            const float width = std::max(80.0f, bounds.z - 34.0f);
            const float top = bounds.y + HeaderHeight;
            const float bottom = bounds.y + bounds.w - FooterHeight;
            const float scrollBefore = scrollY;
            float y = 6.0f;

            const auto full = [&](wi::gui::Widget& widget)
            {
                const float sy = top + y - scrollY;
                widget.SetVisible(active && SelectedIsEmitter() &&
                    sy + RowHeight >= top && sy <= bottom);
                widget.SetPos(XMFLOAT2(x, sy));
                widget.SetSize(XMFLOAT2(width, RowHeight));
                y += RowHeight + RowGap;
            };

            const auto two = [&](wi::gui::Widget& left, wi::gui::Widget& right)
            {
                const float half = (width - RowGap) * 0.5f;
                const float sy = top + y - scrollY;
                const bool visible = active && SelectedIsEmitter() &&
                    sy + RowHeight >= top && sy <= bottom;
                left.SetVisible(visible); right.SetVisible(visible);
                left.SetPos(XMFLOAT2(x, sy));
                right.SetPos(XMFLOAT2(x + half + RowGap, sy));
                left.SetSize(XMFLOAT2(half, RowHeight));
                right.SetSize(XMFLOAT2(half, RowHeight));
                y += RowHeight + RowGap;
            };

            const auto header = [&](SceneInspectorButton& button,
                const ParticleSection section, const char* title)
            {
                const auto index = static_cast<std::size_t>(section);
                full(button);
                button.SetText(std::string(sectionExpanded[index] ? "▼  " : "▶  ") + title);
                return sectionExpanded[index];
            };

            if (header(appearanceHeader, ParticleSection::Appearance, "APPEARANCE"))
            {
                two(chooseTexture, clearTexture); full(blendMode); full(shaderType);
                two(colorR, colorG); two(colorB, opacity);
                two(emissiveR, emissiveG); two(emissiveB, emissiveStrength);
            }
            if (header(attachmentHeader, ParticleSection::Attachment, "ATTACHMENT"))
            {
                full(parent); two(localX, localY); full(localZ);
                two(localScaleX, localScaleY); full(localScaleZ);
            }
            if (header(emissionHeader, ParticleSection::Emission, "PLAYBACK + EMISSION"))
            {
                two(restart, burst); full(resetDefaults); full(autoRestart);
                full(burstCount); full(debugVisual); two(paused, sorting);
                two(volume, depthCollision); two(sph, collidersDisabled);
                full(takeColorFromMesh); full(emitterMesh);
                full(maxParticles); full(emitCount); full(burstOnCreate);
            }
            if (header(particleHeader, ParticleSection::Particle, "PARTICLE"))
            {
                two(size, life); full(rotationDegrees); two(particleScaleX, particleScaleY);
                full(normalFactor); full(randomness); full(lifeRandomness); full(colorRandomness);
                two(opacityStart, opacityEnd); full(motionBlur); full(mass);
            }
            if (header(motionHeader, ParticleSection::Motion, "MOTION"))
            {
                two(velocityX, velocityY); full(velocityZ);
                two(gravityX, gravityY); full(gravityZ); two(drag, restitution);
            }
            if (header(spriteHeader, ParticleSection::SpriteSheet, "SPRITE SHEET // ANIMATED PARTICLES"))
            {
                full(animationMode); two(framesX, framesY); full(useAllFrames);
                two(frameCount, frameStart); full(frameRate); full(frameBlending);
            }
            if (header(advancedHeader, ParticleSection::Advanced, "ADVANCED // NATIVE WICKED"))
            {
                full(fixedTimestep); two(sphH, sphK); two(sphP0, sphE);
            }

            contentHeight = y + 8.0f;
            ClampScroll();
            if (std::abs(scrollY - scrollBefore) > 0.01f)
            {
                Layout();
                return;
            }
        }

        void RenderScrollbar(const wi::graphics::CommandList cmd) const
        {
            const float visible = std::max(0.0f, bounds.w - HeaderHeight - FooterHeight);
            if (!(contentHeight > visible + 1.0f) || visible <= 0.0f)
                return;
            const float trackX = bounds.x + bounds.z - 10.0f;
            const float trackY = bounds.y + HeaderHeight + 5.0f;
            const float trackHeight = std::max(1.0f, visible - 10.0f);
            const float maximumScroll = std::max(1.0f, contentHeight - visible);
            const float thumbHeight = std::clamp(
                trackHeight * (visible / contentHeight), 42.0f, trackHeight);
            const float travel = std::max(0.0f, trackHeight - thumbHeight);
            const float thumbY = trackY + travel *
                std::clamp(scrollY / maximumScroll, 0.0f, 1.0f);
            DrawRect(trackX, trackY, ScrollbarWidth, trackHeight, BorderSoft, cmd);
            DrawRect(trackX, thumbY, ScrollbarWidth, thumbHeight, Forge, cmd);
        }
    };

    RenegadeParticleEmitterWorkspace::RenegadeParticleEmitterWorkspace()
        : impl_(std::make_unique<Impl>()) {}
    RenegadeParticleEmitterWorkspace::~RenegadeParticleEmitterWorkspace() = default;

    void RenegadeParticleEmitterWorkspace::Create()
    {
        if (!impl_->created)
        {
            SetName("Particle Emitter Inspector");
            wi::gui::Widget::SetVisible(false);
            impl_->CreateControls();
        }
    }

    void RenegadeParticleEmitterWorkspace::SetActive(const bool active)
    {
        impl_->active = active;
        impl_->refreshPending = true;
        wi::gui::Widget::SetVisible(active);
        if (!active)
            for (auto* control : impl_->controls) control->SetVisible(false);
    }

    bool RenegadeParticleEmitterWorkspace::IsActive() const noexcept { return impl_->active; }

    void RenegadeParticleEmitterWorkspace::SetBounds(const XMFLOAT4& bounds)
    {
        impl_->bounds = bounds;
        SetPos(XMFLOAT2(bounds.x, bounds.y));
        SetSize(XMFLOAT2(bounds.z, bounds.w));
        if (impl_->created) impl_->Layout();
    }

    bool RenegadeParticleEmitterWorkspace::ContainsPointer(const XMFLOAT4& pointer) const noexcept
    {
        const auto& bounds = impl_->bounds;
        return impl_->active && pointer.x >= bounds.x && pointer.x < bounds.x + bounds.z &&
            pointer.y >= bounds.y && pointer.y < bounds.y + bounds.w;
    }

    bool RenegadeParticleEmitterWorkspace::ConsumedPointerThisFrame() const noexcept
    { return impl_->pointerConsumed; }
    bool RenegadeParticleEmitterWorkspace::HasSelectedEmitter() const noexcept
    { return impl_->SelectedIsEmitter(); }

    void RenegadeParticleEmitterWorkspace::CreateEmitterInFrontOfCamera()
    {
        impl_->CreateEmitter();
        SetActive(impl_->SelectedIsEmitter());
    }

    void RenegadeParticleEmitterWorkspace::Update(const wi::Canvas& canvas, const float dt)
    {
        impl_->pointerConsumed = false;
        if (!impl_->created || !impl_->active) return;

        wi::gui::Widget::Update(canvas, dt);
        auto* current = bridge::StudioSession::Current();
        const auto selection = current != nullptr && current->Selection().HasSelection()
            ? current->Selection().SelectedEntity() : wi::ecs::INVALID_ENTITY;
        const auto revision = current != nullptr ? current->Scenes().Revision() : 0;

        if (current != impl_->session || selection != impl_->selected || revision != impl_->sceneRevision)
        {
            impl_->session = current;
            impl_->sceneRevision = revision;
            impl_->refreshPending = true;
        }
        if (impl_->refreshPending)
        {
            impl_->Refresh();
            impl_->Layout();
        }

        const XMFLOAT4 pointer = wi::input::GetPointer();
        const bool consumed = ContainsPointer(pointer);
        impl_->pointerConsumed = consumed;
        if (consumed)
        {
            Activate();
            const float top = impl_->bounds.y + HeaderHeight;
            const float bottom = impl_->bounds.y + impl_->bounds.w - FooterHeight;
            if (pointer.y >= top && pointer.y < bottom && std::abs(pointer.z) > 0.1f)
            {
                impl_->scrollY += pointer.z > 0.0f ? -ScrollStep : ScrollStep;
                impl_->ClampScroll();
                impl_->Layout();
            }
        }
        else state = wi::gui::IDLE;

        for (auto* control : impl_->controls)
            if (control->IsVisible()) control->Update(canvas, dt);
    }

    void RenegadeParticleEmitterWorkspace::Render(
        const wi::Canvas& canvas, const wi::graphics::CommandList cmd) const
    {
        if (!impl_->created || !impl_->active) return;
        const auto& bounds = impl_->bounds;
        DrawRect(bounds.x, bounds.y, bounds.z, bounds.w, Surface0, cmd);
        DrawRect(bounds.x, bounds.y, bounds.z, 1.0f, Border, cmd);
        DrawText("PARTICLE EMITTER // NATIVE WICKED GPU PARTICLES",
            bounds.x + 12.0f, bounds.y + 10.0f, 12, TextStrong, cmd);
        DrawText(impl_->textureDisplay, bounds.x + 12.0f, bounds.y + 34.0f,
            10, TextSecondary, cmd, 0.08f);
        DrawText("PARENT // " + impl_->parentDisplay, bounds.x + 12.0f,
            bounds.y + 54.0f, 9, Muted, cmd, 0.08f);
        DrawText("Click a section to expand it. Numeric boxes accept exact typed values.",
            bounds.x + 12.0f, bounds.y + 74.0f, 9, Muted, cmd, 0.06f);

        if (impl_->SelectedIsEmitter())
        {
            const auto particleState = bridge::CaptureParticleEmitter(*impl_->Scene(), impl_->selected);
            const auto* emitter = impl_->Scene()->emitters.GetComponent(impl_->selected);
            if (emitter != nullptr)
            {
                DrawText("ALIVE " + std::to_string(emitter->statistics.aliveCount) +
                    " // CULLED " + std::to_string(emitter->statistics.culledCount),
                    bounds.x + 12.0f, bounds.y + 94.0f, 9, Forge, cmd, 0.08f);
            }
            DrawText("RATE " + std::to_string(static_cast<int>(std::lround(particleState.emitCount))) +
                "/S // BURST " + std::to_string(particleState.burstOnCreate) +
                " // NORMAL " + std::to_string(particleState.normalFactor) +
                " // STEP " + TimestepLabel(particleState.fixedTimestep),
                bounds.x + 12.0f, bounds.y + 112.0f, 9, Muted, cmd, 0.06f);
        }

        const float top = bounds.y + HeaderHeight;
        DrawRect(bounds.x, top, bounds.z, 1.0f, Border, cmd);
        for (auto* control : impl_->controls)
            if (control->IsVisible()) control->Render(canvas, cmd);
        impl_->RenderScrollbar(cmd);

        const float footer = bounds.y + bounds.w - FooterHeight;
        DrawRect(bounds.x, footer, bounds.z, FooterHeight, Surface1, cmd);
        DrawRect(bounds.x, footer, bounds.z, 1.0f, Border, cmd);
        DrawText(impl_->status, bounds.x + 12.0f, footer + 9.0f, 9,
            impl_->statusError ? Error : Muted, cmd, 0.08f);
    }
}
