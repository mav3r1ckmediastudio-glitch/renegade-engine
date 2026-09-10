#include "RenegadeParticleEmitterWorkspace.h"

#include "RenegadeStudioChrome.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
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
    using renegade::studio::RenegadeTextInputField;
    using renegade::studio::SceneInspectorButton;
    using renegade::studio::SceneInspectorCheckBox;
    using renegade::studio::SceneInspectorComboBox;
    using renegade::studio::SceneInspectorSlider;

    constexpr float HeaderHeight = 116.0f;
    constexpr float FooterHeight = 34.0f;
    constexpr float RowHeight = 30.0f;
    constexpr float RowGap = 6.0f;
    constexpr float ScrollStep = 72.0f;
    constexpr float NumericInputWidth = 64.0f;
    constexpr float ScrollbarWidth = 5.0f;

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

    float SliderSteps(const float minimum, const float maximum, const float increment) noexcept
    {
        if (!(maximum > minimum) || !(increment > 0.0f))
            return 1.0f;
        return std::max(1.0f, std::round((maximum - minimum) / increment));
    }

    void DrawRect(float x, float y, float width, float height,
        wi::Color color, wi::graphics::CommandList cmd)
    {
        if (width <= 0.0f || height <= 0.0f) return;
        wi::image::Params params(x, y, width, height, color);
        params.blendFlag = wi::enums::BLENDMODE_ALPHA;
        wi::image::Draw(nullptr, params, cmd);
    }

    void DrawText(const std::string& value, float x, float y, int size,
        wi::Color color, wi::graphics::CommandList cmd, float bolden = 0.12f)
    {
        wi::font::Params params(x, y, size, wi::font::WIFALIGN_LEFT,
            wi::font::WIFALIGN_TOP, color, wi::Color::Transparent());
        params.bolden = bolden;
        wi::font::Draw(value, params, cmd);
    }

    bool PointInside(const XMFLOAT4& pointer, const wi::gui::Widget& widget) noexcept
    {
        const auto pos = widget.GetPos();
        const auto size = widget.GetSize();
        return pointer.x >= pos.x && pointer.x < pos.x + size.x &&
            pointer.y >= pos.y && pointer.y < pos.y + size.y;
    }

    std::string EntityName(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        if (entity == wi::ecs::INVALID_ENTITY) return "NONE // WORLD";
        if (const auto* name = scene.names.GetComponent(entity);
            name != nullptr && !name->name.empty()) return name->name;
        return "Unnamed Entity " + std::to_string(entity);
    }

    float RadiansToDegrees(float value) noexcept { return value * (180.0f / XM_PI); }
    float DegreesToRadians(float value) noexcept { return value * (XM_PI / 180.0f); }

    class ParticleNumericInputField final : public RenegadeTextInputField
    {
    public:
        ParticleNumericInputField() { SetRenderTextSize(12); }

        void Update(const wi::Canvas& canvas, const float dt) override
        {
            const auto before = GetState();
            RenegadeTextInputField::Update(canvas, dt);
            if (before != wi::gui::ACTIVE && GetState() == wi::gui::ACTIVE)
                SetAsActive(true);
        }

        const char* GetWidgetTypeName() const override
        {
            return "ParticleNumericInputField";
        }
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
        SceneInspectorSlider burstCount;
        SceneInspectorCheckBox debugVisual;
        SceneInspectorComboBox parent;
        SceneInspectorSlider localX, localY, localZ;
        SceneInspectorSlider localScaleX, localScaleY, localScaleZ;
        SceneInspectorComboBox shaderType;
        SceneInspectorSlider colorR, colorG, colorB, opacity;
        SceneInspectorSlider emissiveR, emissiveG, emissiveB, emissiveStrength;
        SceneInspectorCheckBox paused, sorting, depthCollision, sph, volume;
        SceneInspectorCheckBox frameBlending, collidersDisabled, takeColorFromMesh;
        SceneInspectorComboBox emitterMesh;
        SceneInspectorSlider maxParticles, emitCount, burstOnCreate, size, life;
        SceneInspectorSlider rotationDegrees, particleScaleX, particleScaleY;
        SceneInspectorSlider normalFactor, randomness, lifeRandomness;
        SceneInspectorSlider colorRandomness, opacityStart, opacityEnd;
        SceneInspectorSlider motionBlur, mass;
        SceneInspectorSlider velocityX, velocityY, velocityZ;
        SceneInspectorSlider gravityX, gravityY, gravityZ, drag, restitution;
        SceneInspectorSlider framesX, framesY, frameCount, frameStart, frameRate;
        SceneInspectorSlider fixedTimestep, sphH, sphK, sphP0, sphE;

        struct NumericBinding
        {
            SceneInspectorSlider* slider = nullptr;
            std::unique_ptr<ParticleNumericInputField> input;
            float minimum = 0.0f;
            float maximum = 1.0f;
            bool integer = false;
            int precision = 3;
        };
        std::vector<NumericBinding> numericBindings;

        [[nodiscard]] wi::scene::Scene* Scene() const noexcept
        {
            return session != nullptr ? &session->Scenes().GetScene() : nullptr;
        }
        [[nodiscard]] bool SelectedIsEmitter() const noexcept
        {
            const auto* scene = Scene();
            return scene != nullptr && bridge::IsParticleEmitter(*scene, selected);
        }
        void SetStatus(std::string value, bool error = false)
        {
            status = std::move(value); statusError = error;
        }

        template <typename Command, typename... Args>
        bool ExecuteCommand(Args&&... args)
        {
            if (session == nullptr) return false;
            const bool changed = session->Commands().Execute(
                std::make_unique<Command>(std::forward<Args>(args)...));
            if (changed) refreshPending = true;
            return changed;
        }

        template <typename Fn>
        void EditEmitter(Fn&& edit)
        {
            auto* scene = Scene();
            if (scene == nullptr || !SelectedIsEmitter()) return;
            const auto before = bridge::CaptureParticleEmitter(*scene, selected);
            auto after = before;
            edit(after);
            after = bridge::SanitizeParticleEmitterState(after);
            if (ExecuteCommand<bridge::SetParticleEmitterCommand>(
                    *scene, selected, before, after))
                SetStatus("PARTICLE EMITTER // APPLIED");
        }

        template <typename Fn>
        void EditTransform(Fn&& edit)
        {
            auto* scene = Scene();
            auto* transform = scene != nullptr && SelectedIsEmitter()
                ? scene->transforms.GetComponent(selected) : nullptr;
            if (transform == nullptr) return;
            const auto before = bridge::CaptureTransform(*transform);
            auto after = before;
            edit(after);
            if (ExecuteCommand<bridge::SetTransformCommand>(
                    *scene, selected, before, after))
                SetStatus("ATTACHMENT OFFSET // APPLIED");
        }

        void ToggleSection(const ParticleSection section)
        {
            const auto index = static_cast<std::size_t>(section);
            const bool opening = !sectionExpanded[index];
            sectionExpanded.fill(false);
            if (opening)
                sectionExpanded[index] = true;
            Layout();
        }

        void RegisterNumericInput(
            SceneInspectorSlider& slider,
            const float minimum,
            const float maximum,
            const bool integer,
            const int precision,
            std::function<void(float)> commit)
        {
            auto input = std::make_unique<ParticleNumericInputField>();
            input->Create(slider.GetName() + " Exact Value");
            input->SetTooltip("Type an exact value and press Enter. Click selects the current value.");
            input->SetFloatPrecision(integer ? 0 : precision);
            input->OnInputAccepted(
                [this, &slider, minimum, maximum, integer, commit = std::move(commit)](
                    const wi::gui::EventArgs& args)
                {
                    float value = std::isfinite(args.fValue) ? args.fValue : slider.GetValue();
                    value = std::clamp(value, minimum, maximum);
                    if (integer)
                        value = std::round(value);
                    slider.SetRange(minimum, maximum);
                    slider.SetValue(value);
                    if (commit)
                        commit(value);
                    refreshPending = true;
                });
            numericBindings.push_back({
                &slider, std::move(input), minimum, maximum, integer, precision});
        }

        void PositionNumericInputs()
        {
            for (auto& binding : numericBindings)
            {
                if (binding.slider == nullptr || binding.input == nullptr ||
                    !binding.slider->IsVisible())
                {
                    if (binding.input != nullptr)
                        binding.input->SetVisible(false);
                    continue;
                }
                const auto pos = binding.slider->GetPos();
                const auto size = binding.slider->GetSize();
                const float inputWidth = std::min(
                    NumericInputWidth, std::max(42.0f, size.x * 0.38f));
                binding.input->SetVisible(true);
                binding.input->SetPos(XMFLOAT2(
                    pos.x + size.x - inputWidth, pos.y));
                binding.input->SetSize(XMFLOAT2(inputWidth, size.y));
            }
        }

        void SyncNumericInputs()
        {
            for (auto& binding : numericBindings)
            {
                if (binding.slider == nullptr || binding.input == nullptr ||
                    binding.input->GetState() == wi::gui::ACTIVE)
                    continue;
                if (binding.integer)
                    binding.input->SetValue(
                        static_cast<int>(std::lround(binding.slider->GetValue())));
                else
                    binding.input->SetValue(binding.slider->GetValue());
            }
        }

        void SuppressSlidersBehindExactInputs(const XMFLOAT4& pointer, const bool suppress)
        {
            for (auto& binding : numericBindings)
            {
                if (binding.slider == nullptr || binding.input == nullptr)
                    continue;
                binding.slider->force_disable = suppress &&
                    binding.input->IsVisible() && PointInside(pointer, *binding.input);
            }
        }

        void CreateEmitter()
        {
            if (session == nullptr) session = bridge::StudioSession::Current();
            if (session == nullptr || !session->Projects().HasProject())
            {
                SetStatus("PARTICLES // OPEN A PROJECT FIRST", true); return;
            }
            auto& scene = session->Scenes().GetScene();
            const auto& camera = wi::scene::GetCamera();
            const XMFLOAT3 position{
                camera.Eye.x + camera.At.x * 5.0f,
                camera.Eye.y + camera.At.y * 5.0f,
                camera.Eye.z + camera.At.z * 5.0f};
            auto command = std::make_unique<bridge::CreateParticleEmitterCommand>(scene, position);
            auto* createdCommand = command.get();
            if (!session->Commands().Execute(std::move(command)))
            {
                SetStatus("PARTICLES // CREATION FAILED", true); return;
            }
            selected = createdCommand->CreatedEntity();
            session->Selection().Select(selected);
            active = true; refreshPending = true;
            SetStatus("PARTICLE EMITTER // CREATED 5M IN FRONT OF VIEW // USE GIZMO OR LOCAL OFFSET");
        }

        void ChooseParticleTexture()
        {
            if (session == nullptr || !session->Projects().HasProject() ||
                !SelectedIsEmitter() || wi::jobsystem::IsBusy(textureImportWorkload)) return;
            const auto emitterEntity = selected;
            wi::helper::FileDialogParams params;
            params.type = wi::helper::FileDialogParams::OPEN;
            params.description = "Particle texture / sprite sheet";
            params.extensions = {"png", "tga", "dds", "jpg", "jpeg", "bmp", "hdr"};
            wi::helper::FileDialog(params,
                [this, emitterEntity](const std::string& sourcePath)
                {
                    wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                        [this, emitterEntity, sourcePath](std::uint64_t)
                        {
                            if (sourcePath.empty() || session == nullptr) return;
                            auto& liveScene = session->Scenes().GetScene();
                            if (!bridge::IsParticleEmitter(liveScene, emitterEntity))
                            { SetStatus("PARTICLE TEXTURE // TARGET NO LONGER EXISTS", true); return; }
                            const auto format = bridge::DetectResourceSourceFormat(sourcePath);
                            if (format == bridge::ResourceSourceFormat::Unknown ||
                                bridge::ClassifyResourceSourceFormat(format) != bridge::ResourceClass::Texture)
                            { SetStatus("PARTICLE TEXTURE // UNSUPPORTED IMAGE FORMAT", true); return; }

                            struct ImportState
                            {
                                std::string projectRoot;
                                bridge::StableId projectId;
                                wi::ecs::Entity emitter = wi::ecs::INVALID_ENTITY;
                                std::string sourcePath;
                                bridge::CreatorTextureImportResult imported;
                            };
                            auto state = std::make_shared<ImportState>();
                            const auto& project = session->Projects().CurrentProject();
                            state->projectRoot = project.rootPath;
                            state->projectId = project.projectId;
                            state->emitter = emitterEntity;
                            state->sourcePath = sourcePath;
                            SetStatus("PARTICLE TEXTURE // IMPORTING INTO PROJECT // " +
                                fs::u8path(sourcePath).filename().generic_u8string());
                            wi::jobsystem::Execute(textureImportWorkload,
                                [this, state](wi::jobsystem::JobArgs)
                                {
                                    bridge::CreatorTextureWorkflowService workflow;
                                    state->imported = workflow.ImportTexture(
                                        state->projectRoot, state->projectId, state->sourcePath);
                                    wi::eventhandler::Subscribe_Once(
                                        wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                                        [this, state](std::uint64_t)
                                        {
                                            if (session == nullptr) return;
                                            if (!state->imported.succeeded)
                                            { SetStatus("PARTICLE TEXTURE // IMPORT FAILED // " + state->imported.error, true); return; }
                                            auto& scene = session->Scenes().GetScene();
                                            if (!bridge::IsParticleEmitter(scene, state->emitter))
                                            { SetStatus("PARTICLE TEXTURE // IMPORTED // TARGET GONE", true); return; }
                                            bridge::PreparedMaterialTextureAsset prepared;
                                            std::string error;
                                            if (!bridge::PrepareMaterialTextureAsset(
                                                    state->projectRoot, state->projectId,
                                                    state->imported.assetId, prepared, error))
                                            { SetStatus("PARTICLE TEXTURE // PREPARE FAILED // " + error, true); return; }
                                            auto command = std::make_unique<bridge::SetMaterialTextureAssetCommand>(
                                                scene, state->emitter,
                                                bridge::MaterialTextureSlot::BaseColor,
                                                std::move(prepared));
                                            if (!session->Commands().Execute(std::move(command)))
                                            { SetStatus("PARTICLE TEXTURE // ASSIGN FAILED", true); return; }
                                            refreshPending = true;
                                            SetStatus("PARTICLE TEXTURE // PROJECT-OWNED + ASSIGNED // " +
                                                fs::u8path(state->sourcePath).filename().generic_u8string());
                                        });
                                });
                        });
                });
        }

        void ClearParticleTexture()
        {
            auto* scene = Scene();
            if (scene != nullptr && SelectedIsEmitter() &&
                ExecuteCommand<bridge::ClearMaterialTextureAssetCommand>(
                    *scene, selected, bridge::MaterialTextureSlot::BaseColor))
                SetStatus("PARTICLE TEXTURE // CLEARED");
        }
        void RestartEmitter()
        {
            auto* scene = Scene();
            auto* emitter = scene != nullptr && SelectedIsEmitter()
                ? scene->emitters.GetComponent(selected) : nullptr;
            if (emitter != nullptr) { emitter->Restart(); SetStatus("PARTICLE EMITTER // RESTARTED"); }
        }
        void BurstNow()
        {
            auto* scene = Scene();
            auto* emitter = scene != nullptr && SelectedIsEmitter()
                ? scene->emitters.GetComponent(selected) : nullptr;
            if (emitter != nullptr)
            {
                emitter->Burst(std::max(0, static_cast<int>(std::lround(burstCount.GetValue()))));
                SetStatus("PARTICLE EMITTER // BURST");
            }
        }
        void AttachTo(wi::ecs::Entity parentEntity)
        {
            auto* scene = Scene();
            if (scene != nullptr && SelectedIsEmitter() &&
                ExecuteCommand<bridge::SetParticleEmitterParentCommand>(
                    *scene, selected, parentEntity))
                SetStatus(parentEntity == wi::ecs::INVALID_ENTITY
                    ? "ATTACHMENT // DETACHED TO WORLD"
                    : "ATTACHMENT // NATIVE WICKED PARENT // LOCAL OFFSET PRESERVED");
        }

        void CreateControls()
        {
            const auto sectionHeader = [this](
                SceneInspectorButton& button,
                const char* name,
                const char* title,
                const ParticleSection section)
            {
                button.Create(name);
                button.SetText(std::string("▶  ") + title);
                button.SetTooltip("Expand or collapse this Particle Emitter Inspector section.");
                button.OnClick([this, section](const wi::gui::EventArgs&)
                { ToggleSection(section); });
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
            chooseTexture.OnClick([this](const wi::gui::EventArgs&) { ChooseParticleTexture(); });
            clearTexture.Create("Particle Clear Texture"); clearTexture.SetText("CLEAR");
            clearTexture.OnClick([this](const wi::gui::EventArgs&) { ClearParticleTexture(); });
            restart.Create("Particle Restart"); restart.SetText("RESTART");
            restart.OnClick([this](const wi::gui::EventArgs&) { RestartEmitter(); });
            burst.Create("Particle Burst"); burst.SetText("BURST NOW");
            burst.OnClick([this](const wi::gui::EventArgs&) { BurstNow(); });
            burstCount.Create(1.0f, 10000.0f, 10.0f,
                SliderSteps(1.0f, 10000.0f, 1.0f),
                "Particle Burst Count", "BURST COUNT");
            RegisterNumericInput(burstCount, 1.0f, 10000.0f, true, 0,
                [this](float value) { burstCount.SetValue(value); });
            debugVisual.Create("DEBUG EMITTER VISUALISER: ");
            debugVisual.OnClick([](const wi::gui::EventArgs& args)
            { wi::renderer::SetToDrawDebugEmitters(args.bValue); });
            parent.Create("Particle Parent");
            parent.OnSelect([this](const wi::gui::EventArgs& args)
            { AttachTo(static_cast<wi::ecs::Entity>(args.userdata)); });

            const auto transformSlider = [this](SceneInspectorSlider& slider,
                const char* name, const char* label, int axis, bool isScale)
            {
                const float minimum = isScale ? 0.001f : -10000.0f;
                const float maximum = isScale ? 100.0f : 10000.0f;
                const float initial = isScale ? 1.0f : 0.0f;
                slider.Create(minimum, maximum, initial,
                    SliderSteps(minimum, maximum, 0.01f), name, label);
                const auto commit = [this, axis, isScale](float value)
                {
                    EditTransform([=](bridge::TransformState& state)
                    {
                        XMFLOAT3& target = isScale ? state.scale : state.translation;
                        if (axis == 0) target.x = value;
                        else if (axis == 1) target.y = value;
                        else target.z = value;
                    });
                };
                slider.OnValueCommitted(commit);
                RegisterNumericInput(slider, minimum, maximum, false, 3, commit);
            };
            transformSlider(localX,"Particle Local X","LOCAL X",0,false);
            transformSlider(localY,"Particle Local Y","LOCAL Y",1,false);
            transformSlider(localZ,"Particle Local Z","LOCAL Z",2,false);
            transformSlider(localScaleX,"Particle Local Scale X","SCALE X",0,true);
            transformSlider(localScaleY,"Particle Local Scale Y","SCALE Y",1,true);
            transformSlider(localScaleZ,"Particle Local Scale Z","SCALE Z",2,true);

            shaderType.Create("Particle Shader Type");
            shaderType.AddItem("SOFT", wi::EmittedParticleSystem::SOFT);
            shaderType.AddItem("DISTORTION", wi::EmittedParticleSystem::SOFT_DISTORTION);
            shaderType.AddItem("SIMPLE", wi::EmittedParticleSystem::SIMPLE);
            shaderType.AddItem("LIGHTING", wi::EmittedParticleSystem::SOFT_LIGHTING);
            shaderType.OnSelect([this](const wi::gui::EventArgs& args)
            { EditEmitter([&](auto& s){ s.shaderType = static_cast<wi::EmittedParticleSystem::PARTICLESHADERTYPE>(args.userdata); }); });

            const auto field = [this](
                SceneInspectorSlider& slider,
                float minimum,
                float maximum,
                float initial,
                float increment,
                bool integer,
                int precision,
                const char* name,
                const char* label,
                std::function<void(bridge::ParticleEmitterState&, float)> set)
            {
                slider.Create(minimum, maximum, initial,
                    SliderSteps(minimum, maximum, increment), name, label);
                const auto commit = [this, minimum, maximum, integer, set](float value)
                {
                    float safe = std::clamp(value, minimum, maximum);
                    if (integer)
                        safe = std::round(safe);
                    EditEmitter([&](auto& state) { set(state, safe); });
                };
                slider.OnValueCommitted(commit);
                RegisterNumericInput(slider, minimum, maximum, integer, precision, commit);
            };

            field(colorR,0,1,1,0.01f,false,2,"Particle Color R","COLOR R",[](auto&s,float v){s.color.x=v;});
            field(colorG,0,1,1,0.01f,false,2,"Particle Color G","COLOR G",[](auto&s,float v){s.color.y=v;});
            field(colorB,0,1,1,0.01f,false,2,"Particle Color B","COLOR B",[](auto&s,float v){s.color.z=v;});
            field(opacity,0,1,1,0.01f,false,2,"Particle Opacity","OPACITY",[](auto&s,float v){s.color.w=v;});
            field(emissiveR,0,1,0,0.01f,false,2,"Particle Emissive R","EMISSIVE R",[](auto&s,float v){s.emissiveColor.x=v;});
            field(emissiveG,0,1,0,0.01f,false,2,"Particle Emissive G","EMISSIVE G",[](auto&s,float v){s.emissiveColor.y=v;});
            field(emissiveB,0,1,0,0.01f,false,2,"Particle Emissive B","EMISSIVE B",[](auto&s,float v){s.emissiveColor.z=v;});
            field(emissiveStrength,0,100,0,0.1f,false,1,"Particle Emissive Strength","EMISSIVE",[](auto&s,float v){s.emissiveStrength=v;});

            const auto flag = [this](SceneInspectorCheckBox& box, const char* label,
                std::function<void(bridge::ParticleEmitterState&, bool)> set)
            {
                box.Create(label);
                box.OnClick([this,set=std::move(set)](const wi::gui::EventArgs& args)
                { EditEmitter([&](auto& s){ set(s,args.bValue); }); });
            };
            flag(paused,"PAUSED: ",[](auto&s,bool v){s.paused=v;});
            flag(sorting,"SORT PARTICLES: ",[](auto&s,bool v){s.sorted=v;});
            flag(depthCollision,"DEPTH COLLISION: ",[](auto&s,bool v){s.depthCollision=v;});
            flag(sph,"SPH FLUID SIM: ",[](auto&s,bool v){s.sph=v;});
            flag(volume,"EMIT IN VOLUME: ",[](auto&s,bool v){s.volume=v;});
            flag(frameBlending,"SPRITE FRAME BLENDING: ",[](auto&s,bool v){s.frameBlending=v;});
            flag(collidersDisabled,"DISABLE COLLIDERS: ",[](auto&s,bool v){s.collidersDisabled=v;});
            flag(takeColorFromMesh,"TAKE COLOR FROM MESH: ",[](auto&s,bool v){s.takeColorFromMesh=v;});
            emitterMesh.Create("Particle Emitter Mesh");
            emitterMesh.OnSelect([this](const wi::gui::EventArgs& args)
            { EditEmitter([&](auto& s){s.meshId=static_cast<wi::ecs::Entity>(args.userdata);}); });

            field(maxParticles,100,1000000,1000,100,true,0,"Particle Max","MAX PARTICLES",[](auto&s,float v){s.maxParticles=static_cast<std::uint32_t>(std::lround(v));});
            field(emitCount,0,10000,20,1,false,0,"Particle Emit Count","EMIT / SEC",[](auto&s,float v){s.emitCount=v;});
            field(burstOnCreate,0,100000,0,1,true,0,"Particle Burst On Create","BURST ON CREATE",[](auto&s,float v){s.burstOnCreate=static_cast<int>(std::lround(v));});
            field(size,0.001f,100,0.2f,0.01f,false,3,"Particle Size","SIZE",[](auto&s,float v){s.size=v;});
            field(life,0.001f,600,2,0.01f,false,3,"Particle Life","LIFETIME",[](auto&s,float v){s.life=v;});
            field(rotationDegrees,-360,360,0,1,false,1,"Particle Rotation","ROTATION DEG",[](auto&s,float v){s.rotation=DegreesToRadians(v);});
            field(particleScaleX,0.001f,100,1,0.01f,false,3,"Particle Scale X","PARTICLE SCALE X",[](auto&s,float v){s.scaleX=v;});
            field(particleScaleY,0.001f,100,1,0.01f,false,3,"Particle Scale Y","PARTICLE SCALE Y",[](auto&s,float v){s.scaleY=v;});
            field(normalFactor,-10,10,0,0.01f,false,2,"Particle Normal Factor","NORMAL FACTOR",[](auto&s,float v){s.normalFactor=v;});
            field(randomness,0,1,0.35f,0.01f,false,2,"Particle Random","RANDOMNESS",[](auto&s,float v){s.randomFactor=v;});
            field(lifeRandomness,0,10,0.25f,0.01f,false,2,"Particle Life Random","LIFE RANDOM",[](auto&s,float v){s.randomLife=v;});
            field(colorRandomness,0,2,0,0.01f,false,2,"Particle Color Random","COLOR RANDOM",[](auto&s,float v){s.randomColor=v;});
            field(opacityStart,0,1,0.1f,0.01f,false,2,"Particle Opacity Start","OPACITY PEAK START",[](auto&s,float v){s.opacityPeakStart=v;});
            field(opacityEnd,0,1,0.5f,0.01f,false,2,"Particle Opacity End","OPACITY PEAK END",[](auto&s,float v){s.opacityPeakEnd=v;});
            field(motionBlur,0,1,0,0.01f,false,2,"Particle Motion Blur","MOTION BLUR",[](auto&s,float v){s.motionBlurAmount=v;});
            field(mass,0.001f,1000,1,0.01f,false,3,"Particle Mass","MASS",[](auto&s,float v){s.mass=v;});
            field(velocityX,-1000,1000,0,0.1f,false,2,"Particle Velocity X","VELOCITY X",[](auto&s,float v){s.velocity.x=v;});
            field(velocityY,-1000,1000,0.75f,0.1f,false,2,"Particle Velocity Y","VELOCITY Y",[](auto&s,float v){s.velocity.y=v;});
            field(velocityZ,-1000,1000,0,0.1f,false,2,"Particle Velocity Z","VELOCITY Z",[](auto&s,float v){s.velocity.z=v;});
            field(gravityX,-1000,1000,0,0.1f,false,2,"Particle Gravity X","GRAVITY X",[](auto&s,float v){s.gravity.x=v;});
            field(gravityY,-1000,1000,0,0.1f,false,2,"Particle Gravity Y","GRAVITY Y",[](auto&s,float v){s.gravity.y=v;});
            field(gravityZ,-1000,1000,0,0.1f,false,2,"Particle Gravity Z","GRAVITY Z",[](auto&s,float v){s.gravity.z=v;});
            field(drag,0,1,1,0.01f,false,2,"Particle Drag","DRAG",[](auto&s,float v){s.drag=v;});
            field(restitution,0,1,0.98f,0.01f,false,2,"Particle Restitution","RESTITUTION",[](auto&s,float v){s.restitution=v;});
            field(framesX,1,64,1,1,true,0,"Particle Frames X","SPRITE COLUMNS",[](auto&s,float v){s.framesX=static_cast<std::uint32_t>(std::lround(v));});
            field(framesY,1,64,1,1,true,0,"Particle Frames Y","SPRITE ROWS",[](auto&s,float v){s.framesY=static_cast<std::uint32_t>(std::lround(v));});
            field(frameCount,1,4096,1,1,true,0,"Particle Frame Count","FRAME COUNT",[](auto&s,float v){const auto cells=std::max<std::uint32_t>(1u,s.framesX*s.framesY);s.frameCount=std::min<std::uint32_t>(static_cast<std::uint32_t>(std::lround(v)),cells);});
            field(frameStart,0,4095,0,1,true,0,"Particle Frame Start","START FRAME",[](auto&s,float v){const auto last=s.frameCount>0?s.frameCount-1u:0u;s.frameStart=std::min<std::uint32_t>(static_cast<std::uint32_t>(std::lround(v)),last);});
            field(frameRate,0,240,0,1,false,1,"Particle Frame Rate","FRAME RATE FPS",[](auto&s,float v){s.frameRate=v;});
            field(fixedTimestep,-1,0.1f,-1,0.001f,false,3,"Particle Fixed Timestep","FIXED TIMESTEP",[](auto&s,float v){s.fixedTimestep=v;});
            field(sphH,0.001f,100,1,0.01f,false,3,"Particle SPH H","SPH H",[](auto&s,float v){s.sphH=v;});
            field(sphK,0,10000,250,1,false,1,"Particle SPH K","SPH K",[](auto&s,float v){s.sphK=v;});
            field(sphP0,0.001f,1000,1,0.01f,false,3,"Particle SPH P0","SPH P0",[](auto&s,float v){s.sphP0=v;});
            field(sphE,0,100,0.018f,0.001f,false,3,"Particle SPH E","SPH VISCOSITY",[](auto&s,float v){s.sphE=v;});

            controls = {
                &appearanceHeader,&attachmentHeader,&emissionHeader,&particleHeader,
                &motionHeader,&spriteHeader,&advancedHeader,
                &chooseTexture,&clearTexture,&restart,&burst,&burstCount,&debugVisual,
                &parent,&localX,&localY,&localZ,&localScaleX,&localScaleY,&localScaleZ,
                &shaderType,&colorR,&colorG,&colorB,&opacity,&emissiveR,&emissiveG,&emissiveB,&emissiveStrength,
                &paused,&sorting,&depthCollision,&sph,&volume,&frameBlending,&collidersDisabled,&takeColorFromMesh,&emitterMesh,
                &maxParticles,&emitCount,&burstOnCreate,&size,&life,&rotationDegrees,&particleScaleX,&particleScaleY,
                &normalFactor,&randomness,&lifeRandomness,&colorRandomness,&opacityStart,&opacityEnd,&motionBlur,&mass,
                &velocityX,&velocityY,&velocityZ,&gravityX,&gravityY,&gravityZ,&drag,&restitution,
                &framesX,&framesY,&frameCount,&frameStart,&frameRate,&fixedTimestep,&sphH,&sphK,&sphP0,&sphE};
            for (auto& binding : numericBindings)
                controls.push_back(binding.input.get());
            for (auto* control : controls) control->SetVisible(false);
            created = true;
        }

        void Refresh()
        {
            if (session == nullptr) session = bridge::StudioSession::Current();
            selected = session != nullptr && session->Selection().HasSelection()
                ? session->Selection().SelectedEntity() : wi::ecs::INVALID_ENTITY;
            if (!SelectedIsEmitter())
            {
                for (auto* control : controls) control->SetVisible(false);
                textureDisplay = "NO PARTICLE EMITTER SELECTED";
                parentDisplay = "WORLD"; refreshPending = false; return;
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
            for (std::size_t i=0;i<scene.meshes.GetCount();++i)
            {
                const auto entity = scene.meshes.GetEntity(i);
                emitterMesh.AddItem(EntityName(scene, entity), static_cast<std::uint64_t>(entity));
            }
            emitterMesh.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(state.meshId));
            shaderType.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(state.shaderType));
            colorR.SetValue(state.color.x); colorG.SetValue(state.color.y); colorB.SetValue(state.color.z); opacity.SetValue(state.color.w);
            emissiveR.SetValue(state.emissiveColor.x); emissiveG.SetValue(state.emissiveColor.y); emissiveB.SetValue(state.emissiveColor.z); emissiveStrength.SetValue(state.emissiveStrength);
            paused.SetCheck(state.paused); sorting.SetCheck(state.sorted); depthCollision.SetCheck(state.depthCollision); sph.SetCheck(state.sph); volume.SetCheck(state.volume); frameBlending.SetCheck(state.frameBlending); collidersDisabled.SetCheck(state.collidersDisabled); takeColorFromMesh.SetCheck(state.takeColorFromMesh);
            maxParticles.SetValue(static_cast<float>(state.maxParticles)); emitCount.SetValue(state.emitCount); burstOnCreate.SetValue(static_cast<float>(state.burstOnCreate));
            size.SetValue(state.size); life.SetValue(state.life); rotationDegrees.SetValue(RadiansToDegrees(state.rotation)); particleScaleX.SetValue(state.scaleX); particleScaleY.SetValue(state.scaleY); normalFactor.SetValue(state.normalFactor); randomness.SetValue(state.randomFactor); lifeRandomness.SetValue(state.randomLife); colorRandomness.SetValue(state.randomColor); opacityStart.SetValue(state.opacityPeakStart); opacityEnd.SetValue(state.opacityPeakEnd); motionBlur.SetValue(state.motionBlurAmount); mass.SetValue(state.mass);
            velocityX.SetValue(state.velocity.x); velocityY.SetValue(state.velocity.y); velocityZ.SetValue(state.velocity.z); gravityX.SetValue(state.gravity.x); gravityY.SetValue(state.gravity.y); gravityZ.SetValue(state.gravity.z); drag.SetValue(state.drag); restitution.SetValue(state.restitution);
            framesX.SetValue(static_cast<float>(state.framesX)); framesY.SetValue(static_cast<float>(state.framesY)); frameCount.SetValue(static_cast<float>(state.frameCount)); frameStart.SetValue(static_cast<float>(state.frameStart)); frameRate.SetValue(state.frameRate); fixedTimestep.SetValue(state.fixedTimestep); sphH.SetValue(state.sphH); sphK.SetValue(state.sphK); sphP0.SetValue(state.sphP0); sphE.SetValue(state.sphE);
            debugVisual.SetCheck(wi::renderer::GetToDrawDebugEmitters());
            if (transform != nullptr)
            {
                const auto local = bridge::CaptureTransform(*transform);
                localX.SetValue(local.translation.x); localY.SetValue(local.translation.y); localZ.SetValue(local.translation.z);
                localScaleX.SetValue(local.scale.x); localScaleY.SetValue(local.scale.y); localScaleZ.SetValue(local.scale.z);
            }
            textureDisplay = "NO PARTICLE TEXTURE // WICKED DEFAULT WHITE";
            if (const auto* material = scene.materials.GetComponent(selected))
            {
                const auto& texture = material->textures[bridge::WickedTextureSlot(bridge::MaterialTextureSlot::BaseColor)];
                if (!texture.name.empty()) textureDisplay = fs::u8path(texture.name).filename().generic_u8string();
                else if (texture.resource.IsValid()) textureDisplay = "PARTICLE TEXTURE // LOADED";
            }
            SyncNumericInputs();
            refreshPending = false;
        }

        void ClampScroll()
        {
            const float visible = std::max(0.0f, bounds.w - HeaderHeight - FooterHeight);
            scrollY = std::clamp(scrollY, 0.0f, std::max(0.0f, contentHeight-visible));
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
            const auto two = [&](wi::gui::Widget& a, wi::gui::Widget& b)
            {
                const float half = (width - RowGap) * 0.5f;
                const float sy = top + y - scrollY;
                const bool visible = active && SelectedIsEmitter() &&
                    sy + RowHeight >= top && sy <= bottom;
                a.SetVisible(visible); b.SetVisible(visible);
                a.SetPos(XMFLOAT2(x, sy));
                b.SetPos(XMFLOAT2(x + half + RowGap, sy));
                a.SetSize(XMFLOAT2(half, RowHeight));
                b.SetSize(XMFLOAT2(half, RowHeight));
                y += RowHeight + RowGap;
            };
            const auto header = [&](
                SceneInspectorButton& button,
                const ParticleSection section,
                const char* title)
            {
                const auto index = static_cast<std::size_t>(section);
                full(button);
                button.SetText(std::string(sectionExpanded[index] ? "▼  " : "▶  ") + title);
                return sectionExpanded[index];
            };

            if (header(appearanceHeader, ParticleSection::Appearance, "APPEARANCE"))
            {
                two(chooseTexture,clearTexture); full(shaderType);
                two(colorR,colorG); two(colorB,opacity);
                two(emissiveR,emissiveG); two(emissiveB,emissiveStrength);
            }
            if (header(attachmentHeader, ParticleSection::Attachment, "ATTACHMENT"))
            {
                full(parent); two(localX,localY); full(localZ);
                two(localScaleX,localScaleY); full(localScaleZ);
            }
            if (header(emissionHeader, ParticleSection::Emission, "PLAYBACK + EMISSION"))
            {
                two(restart,burst); full(burstCount); full(debugVisual);
                two(paused,sorting); two(volume,depthCollision);
                two(sph,frameBlending); two(collidersDisabled,takeColorFromMesh);
                full(emitterMesh); full(maxParticles); full(emitCount); full(burstOnCreate);
            }
            if (header(particleHeader, ParticleSection::Particle, "PARTICLE"))
            {
                two(size,life); full(rotationDegrees);
                two(particleScaleX,particleScaleY); full(normalFactor);
                full(randomness); full(lifeRandomness); full(colorRandomness);
                two(opacityStart,opacityEnd); full(motionBlur); full(mass);
            }
            if (header(motionHeader, ParticleSection::Motion, "MOTION"))
            {
                two(velocityX,velocityY); full(velocityZ);
                two(gravityX,gravityY); full(gravityZ); two(drag,restitution);
            }
            if (header(spriteHeader, ParticleSection::SpriteSheet, "SPRITE SHEET // ANIMATED PARTICLES"))
            {
                two(framesX,framesY); two(frameCount,frameStart); full(frameRate);
                full(frameBlending);
            }
            if (header(advancedHeader, ParticleSection::Advanced, "ADVANCED // NATIVE WICKED"))
            {
                full(fixedTimestep); two(sphH,sphK); two(sphP0,sphE);
            }

            contentHeight = y + 8.0f;
            ClampScroll();
            if (std::abs(scrollY - scrollBefore) > 0.01f)
            {
                Layout();
                return;
            }
            PositionNumericInputs();
        }

        void RenderScrollbar(wi::graphics::CommandList cmd) const
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
            const float thumbY = trackY + travel * std::clamp(scrollY / maximumScroll, 0.0f, 1.0f);
            DrawRect(trackX, trackY, ScrollbarWidth, trackHeight, BorderSoft, cmd);
            DrawRect(trackX, thumbY, ScrollbarWidth, thumbHeight, Forge, cmd);
        }
    };

    RenegadeParticleEmitterWorkspace::RenegadeParticleEmitterWorkspace():impl_(std::make_unique<Impl>()){}
    RenegadeParticleEmitterWorkspace::~RenegadeParticleEmitterWorkspace()=default;
    void RenegadeParticleEmitterWorkspace::Create(){if(!impl_->created){SetName("Particle Emitter Inspector");wi::gui::Widget::SetVisible(false);impl_->CreateControls();}}
    void RenegadeParticleEmitterWorkspace::SetActive(bool active){impl_->active=active;impl_->refreshPending=true;wi::gui::Widget::SetVisible(active);if(!active)for(auto* c:impl_->controls)c->SetVisible(false);}
    bool RenegadeParticleEmitterWorkspace::IsActive()const noexcept{return impl_->active;}
    void RenegadeParticleEmitterWorkspace::SetBounds(const XMFLOAT4& b){impl_->bounds=b;SetPos(XMFLOAT2(b.x,b.y));SetSize(XMFLOAT2(b.z,b.w));if(impl_->created)impl_->Layout();}
    bool RenegadeParticleEmitterWorkspace::ContainsPointer(const XMFLOAT4& p)const noexcept{const auto&b=impl_->bounds;return impl_->active&&p.x>=b.x&&p.x<b.x+b.z&&p.y>=b.y&&p.y<b.y+b.w;}
    bool RenegadeParticleEmitterWorkspace::ConsumedPointerThisFrame()const noexcept{return impl_->pointerConsumed;}
    bool RenegadeParticleEmitterWorkspace::HasSelectedEmitter()const noexcept{return impl_->SelectedIsEmitter();}
    void RenegadeParticleEmitterWorkspace::CreateEmitterInFrontOfCamera(){impl_->CreateEmitter();SetActive(impl_->SelectedIsEmitter());}

    void RenegadeParticleEmitterWorkspace::Update(const wi::Canvas& canvas,float dt)
    {
        impl_->pointerConsumed=false;if(!impl_->created||!impl_->active)return;wi::gui::Widget::Update(canvas,dt);
        auto* current=bridge::StudioSession::Current();const auto selection=current&&current->Selection().HasSelection()?current->Selection().SelectedEntity():wi::ecs::INVALID_ENTITY;const auto revision=current?current->Scenes().Revision():0;
        if(current!=impl_->session||selection!=impl_->selected||revision!=impl_->sceneRevision){impl_->session=current;impl_->sceneRevision=revision;impl_->refreshPending=true;}
        if(impl_->refreshPending){impl_->Refresh();impl_->Layout();}
        const XMFLOAT4 pointer=wi::input::GetPointer();const bool consumed=ContainsPointer(pointer);impl_->pointerConsumed=consumed;
        if(consumed){Activate();const float top=impl_->bounds.y+HeaderHeight,bottom=impl_->bounds.y+impl_->bounds.w-FooterHeight;if(pointer.y>=top&&pointer.y<bottom&&std::abs(pointer.z)>0.1f){impl_->scrollY+=pointer.z>0.0f?-ScrollStep:ScrollStep;impl_->ClampScroll();impl_->Layout();}}
        else state=wi::gui::IDLE;
        impl_->SuppressSlidersBehindExactInputs(pointer,true);
        for(auto* c:impl_->controls)if(c->IsVisible())c->Update(canvas,dt);
        impl_->SuppressSlidersBehindExactInputs(pointer,false);
        impl_->SyncNumericInputs();
    }

    void RenegadeParticleEmitterWorkspace::Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd)const
    {
        if(!impl_->created||!impl_->active)return;const auto&b=impl_->bounds;DrawRect(b.x,b.y,b.z,b.w,Surface0,cmd);DrawRect(b.x,b.y,b.z,1.0f,Border,cmd);
        DrawText("PARTICLE EMITTER // NATIVE WICKED GPU PARTICLES",b.x+12,b.y+10,12,TextStrong,cmd);DrawText(impl_->textureDisplay,b.x+12,b.y+34,10,TextSecondary,cmd,0.08f);DrawText("PARENT // "+impl_->parentDisplay,b.x+12,b.y+54,9,Muted,cmd,0.08f);DrawText("Click a section to expand it. Mouse wheel scrolls long sections.",b.x+12,b.y+74,9,Muted,cmd,0.06f);
        if(impl_->SelectedIsEmitter())if(const auto* emitter=impl_->Scene()->emitters.GetComponent(impl_->selected))DrawText("ALIVE "+std::to_string(emitter->statistics.aliveCount)+" // CULLED "+std::to_string(emitter->statistics.culledCount),b.x+12,b.y+94,9,Forge,cmd,0.08f);
        const float top=b.y+HeaderHeight;DrawRect(b.x,top,b.z,1.0f,Border,cmd);for(auto*c:impl_->controls)if(c->IsVisible())c->Render(canvas,cmd);impl_->RenderScrollbar(cmd);
        const float footer=b.y+b.w-FooterHeight;DrawRect(b.x,footer,b.z,FooterHeight,Surface1,cmd);DrawRect(b.x,footer,b.z,1.0f,Border,cmd);DrawText(impl_->status,b.x+12,footer+9,9,impl_->statusError?Error:Muted,cmd,0.08f);
    }
}
