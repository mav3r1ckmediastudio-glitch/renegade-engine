#include "RenegadeStudioChrome.h"
#include "renegade/bridge/ParticleEmitterService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace renegade::studio
{
    // Particle Inspector-specific combo box policy. Wicked/Renegade already
    // render an opaque dropdown surface; the owner defect was z-order and
    // hit-through because this workspace updates/renders children manually.
    // Track the open combo and let it own interaction until the following
    // frame after it closes, so a selection click can never fall through to a
    // slider/check box underneath the popup.
    class ParticleInspectorComboBox : public RenegadeComboBox
    {
    public:
        ParticleInspectorComboBox()
        {
            SetRenderTextSize(12);
        }

        static bool BlocksInteractionFor(
            const wi::gui::Widget* requester) noexcept
        {
            return interactionOwner_ != nullptr &&
                interactionOwner_ != requester;
        }

        static bool PopupOpenForOtherControl(
            const wi::gui::Widget* requester) noexcept
        {
            return interactionOwner_ != nullptr &&
                interactionOwner_ != requester &&
                interactionOwner_->GetState() == wi::gui::ACTIVE;
        }

        void Update(
            const wi::Canvas& canvas,
            const float dt) override
        {
            if (BlocksInteractionFor(this))
                return;

            const bool wasOpen = GetState() == wi::gui::ACTIVE;
            RenegadeComboBox::Update(canvas, dt);
            const bool isOpen = GetState() == wi::gui::ACTIVE;

            if (isOpen)
            {
                interactionOwner_ = this;
            }
            else if (interactionOwner_ == this && !wasOpen)
            {
                // If this frame closed an open popup, keep ownership for the
                // remainder of that frame. Clear it only on the next update so
                // the same mouse press cannot operate a control behind the
                // dropdown item that was just chosen.
                interactionOwner_ = nullptr;
            }
        }

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            if (PopupOpenForOtherControl(this))
                return;
            RenegadeComboBox::Render(canvas, cmd);
        }

        const char* GetWidgetTypeName() const override
        {
            return "ParticleInspectorComboBox";
        }

    private:
        static inline ParticleInspectorComboBox* interactionOwner_ = nullptr;
    };

    class ParticleInspectorButton : public RenegadeButton
    {
    public:
        enum class Role
        {
            Normal,
            StaticTexture,
            SpriteSheet,
        };

        ParticleInspectorButton()
        {
            SetRenderTextSize(11);
        }

        ~ParticleInspectorButton()
        {
            if (staticTextureButton_ == this)
                staticTextureButton_ = nullptr;
            if (spriteSheetButton_ == this)
                spriteSheetButton_ = nullptr;
        }

        void Create(const std::string& name)
        {
            RenegadeButton::Create(name);

            if (name == "Particle Choose Texture")
            {
                role_ = Role::StaticTexture;
                staticTextureButton_ = this;
                SetTooltip(
                    "Load a single static particle image using the current/native emitter setup. "
                    "Right-click to clear the assigned particle texture.");
            }
            else if (name == "Particle Clear Texture")
            {
                // V2 gives this half of the row to CLEAR. The compiled V3
                // owner-feedback shim repurposes that visible slot as the
                // requested second creator path, while retaining CLEAR on RMB.
                role_ = Role::SpriteSheet;
                spriteSheetButton_ = this;
                SetTooltip(
                    "Load an animated sprite sheet. Starts from 100 max particles, "
                    "1 emitted particle/sec, 24 FPS, frame 0 and no frame blending. "
                    "All native Wicked controls remain editable. Right-click to clear.");
            }
        }

        void SetText(const std::string& text)
        {
            if (role_ == Role::StaticTexture)
            {
                RenegadeButton::SetText("STATIC TEXTURE...");
                return;
            }
            if (role_ == Role::SpriteSheet)
            {
                RenegadeButton::SetText("SPRITESHEET...");
                return;
            }
            RenegadeButton::SetText(text);
        }

        void OnClick(
            std::function<void(const wi::gui::EventArgs& args)> func)
        {
            if (role_ == Role::StaticTexture)
            {
                primaryCallback_ = std::move(func);
                RenegadeButton::OnClick(
                    [this](const wi::gui::EventArgs& args)
                    {
                        if (primaryCallback_)
                            primaryCallback_(args);
                    });
                RenegadeButton::OnRightClick(
                    [](const wi::gui::EventArgs& args)
                    {
                        InvokeClear(args);
                    });
                return;
            }

            if (role_ == Role::SpriteSheet)
            {
                // V2 wires this widget to ClearParticleTexture(). Preserve
                // that callback for RMB, but use the normal left-click as the
                // new sprite-sheet chooser requested by the creator workflow.
                clearCallback_ = std::move(func);
                RenegadeButton::OnClick(
                    [](const wi::gui::EventArgs& args)
                    {
                        ApplySpriteSheetPreset();
                        if (staticTextureButton_ != nullptr &&
                            staticTextureButton_->primaryCallback_)
                        {
                            staticTextureButton_->primaryCallback_(args);
                        }
                    });
                RenegadeButton::OnRightClick(
                    [](const wi::gui::EventArgs& args)
                    {
                        InvokeClear(args);
                    });
                return;
            }

            RenegadeButton::OnClick(std::move(func));
        }

        void Update(const wi::Canvas& canvas, const float dt) override
        {
            if (ParticleInspectorComboBox::BlocksInteractionFor(this))
                return;
            RenegadeButton::Update(canvas, dt);
        }

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            if (ParticleInspectorComboBox::PopupOpenForOtherControl(this))
                return;
            RenegadeButton::Render(canvas, cmd);
        }

        const char* GetWidgetTypeName() const override
        {
            return "ParticleInspectorButton";
        }

    private:
        static void InvokeClear(const wi::gui::EventArgs& args)
        {
            if (spriteSheetButton_ != nullptr &&
                spriteSheetButton_->clearCallback_)
            {
                spriteSheetButton_->clearCallback_(args);
            }
        }

        static void ApplySpriteSheetPreset()
        {
            auto* session = bridge::StudioSession::Current();
            if (session == nullptr ||
                !session->Projects().HasProject() ||
                !session->Selection().HasSelection())
            {
                return;
            }

            auto& scene = session->Scenes().GetScene();
            const auto entity = session->Selection().SelectedEntity();
            if (!bridge::IsParticleEmitter(scene, entity))
                return;

            const auto before =
                bridge::CaptureParticleEmitter(scene, entity);
            auto after = before;

            // Creator-safe whole-effect flipbook starting point. This does not
            // hide or lock any Wicked controls; it only avoids immediately
            // stacking tens of independently-aged copies of a complete effect.
            after.maxParticles = 100u;
            after.emitCount = 1.0f;
            after.burstOnCreate = 0;
            after.frameRate = 24.0f;
            after.frameStart = 0u;
            after.frameBlending = false;
            after = bridge::SanitizeParticleEmitterState(after);

            session->Commands().Execute(
                std::make_unique<bridge::SetParticleEmitterCommand>(
                    scene,
                    entity,
                    before,
                    after));

            if (auto* emitter = scene.emitters.GetComponent(entity);
                emitter != nullptr)
            {
                emitter->Restart();
            }
        }

        Role role_ = Role::Normal;
        std::function<void(const wi::gui::EventArgs& args)> primaryCallback_;
        std::function<void(const wi::gui::EventArgs& args)> clearCallback_;
        static inline ParticleInspectorButton* staticTextureButton_ = nullptr;
        static inline ParticleInspectorButton* spriteSheetButton_ = nullptr;
    };

    class ParticleInspectorCheckBox : public RenegadeCheckBox
    {
    public:
        ParticleInspectorCheckBox()
        {
            SetRenderTextSize(11);
        }

        void Update(const wi::Canvas& canvas, const float dt) override
        {
            if (ParticleInspectorComboBox::BlocksInteractionFor(this))
                return;
            RenegadeCheckBox::Update(canvas, dt);
        }

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            if (ParticleInspectorComboBox::PopupOpenForOtherControl(this))
                return;
            RenegadeCheckBox::Render(canvas, cmd);
        }

        const char* GetWidgetTypeName() const override
        {
            return "ParticleInspectorCheckBox";
        }
    };

    // Reserve the numeric field as an exclusive interaction target. The stock
    // Wicked Slider recalculates its rail hit box from the live text-field
    // width; that width can contract to the rendered number and let the rail
    // overlap the fixed-width Renegade number box. Intercept clicks inside the
    // creator-visible value box (plus a small safety gutter) before the base
    // Slider sees them. The gutter is intentionally inert; the actual box
    // selects all text so typing immediately replaces the old value.
    class ParticleInspectorSliderBase : public RenegadeSlider
    {
    public:
        static constexpr float NumericHitGutter = 7.0f;
        static constexpr float NumericBoxMaximumWidth = 70.0f;

        // Wicked uses frameRate == 0 as the special "animate over particle
        // lifetime" sentinel. V2 represented that mode by disabling the FPS
        // slider, which also disabled its numeric box and made it impossible
        // for a creator to type a positive FPS to switch into fixed-rate mode.
        // Keep that one field interactive at zero. V2's existing commit path
        // already writes any positive value to emitter->frameRate, and Refresh
        // then derives FIXED FPS from the resulting non-zero native state.
        void SetEnabled(const bool enabled)
        {
            if (GetName() == "Particle Frame Rate")
            {
                RenegadeSlider::SetEnabled(true);
                valueInputField.SetEnabled(true);
                return;
            }
            RenegadeSlider::SetEnabled(enabled);
        }

        void Update(
            const wi::Canvas& canvas,
            const float dt) override
        {
            if (ParticleInspectorComboBox::BlocksInteractionFor(this))
                return;

            const auto pos = GetPos();
            const auto size = GetSize();
            const float inputWidth = std::min(
                NumericBoxMaximumWidth,
                std::max(48.0f, size.x * 0.38f));
            const float inputX = pos.x + size.x - inputWidth;
            const XMFLOAT4 pointer = wi::input::GetPointer();
            const bool sameRow =
                pointer.y >= pos.y && pointer.y < pos.y + size.y;
            const bool insideValueBox =
                sameRow && pointer.x >= inputX &&
                pointer.x < pos.x + size.x;
            const bool insideSafetyGutter =
                sameRow && pointer.x >= inputX - NumericHitGutter &&
                pointer.x < inputX;

            if (wi::input::Press(wi::input::MOUSE_BUTTON_LEFT) &&
                (insideValueBox || insideSafetyGutter))
            {
                // Update transform state without allowing Slider::Update to
                // interpret this press as a rail click.
                wi::gui::Widget::Update(canvas, dt);
                valueInputField.SetSize(XMFLOAT2(inputWidth, size.y));
                valueInputField.SetPos(XMFLOAT2(inputX, pos.y));

                if (insideValueBox)
                {
                    valueInputField.Update(canvas, dt);
                    valueInputField.SetAsActive(true);
                }
                return;
            }

            RenegadeSlider::Update(canvas, dt);
        }

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            if (ParticleInspectorComboBox::PopupOpenForOtherControl(this))
                return;
            RenegadeSlider::Render(canvas, cmd);
        }

        const char* GetWidgetTypeName() const override
        {
            return "ParticleInspectorSliderBase";
        }
    };
}

// Reuse the accepted V2 authoring implementation, but swap only its local
// Inspector control types for the owner-feedback-safe wrappers above. V2 stays
// as the readable feature implementation; this file is the compiled shim that
// resolves the final hit-testing and dropdown z-order defects without changing
// global Studio controls.
#define RenegadeSlider ParticleInspectorSliderBase
#define SceneInspectorButton ParticleInspectorButton
#define SceneInspectorCheckBox ParticleInspectorCheckBox
#define SceneInspectorComboBox ParticleInspectorComboBox
#include "RenegadeParticleEmitterWorkspaceV2.cpp"
#undef SceneInspectorComboBox
#undef SceneInspectorCheckBox
#undef SceneInspectorButton
#undef RenegadeSlider