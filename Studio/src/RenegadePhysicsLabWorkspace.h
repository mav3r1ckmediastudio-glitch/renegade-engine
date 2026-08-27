#pragma once

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <WickedEngine.h>

#include "RenegadeStudioChrome.h"
#include "renegade/bridge/PhysicsService.h"
#include "renegade/bridge/CollisionService.h"
#include "renegade/bridge/ConstraintService.h"
#include "renegade/bridge/CharacterPhysicsService.h"
#include "renegade/bridge/VehiclePhysicsService.h"
#include "renegade/bridge/RagdollPhysicsService.h"
#include "renegade/bridge/SoftBodyPhysicsService.h"
#include "renegade/bridge/WickedColliderService.h"
#include "renegade/bridge/StudioSession.h"

namespace renegade::studio
{
    // JP01 creator workspace. Wicked's Scene and physics implementation remain
    // authoritative; this class is only Renegade's curated editor surface over
    // the JP01 bridge services and the shared Studio command history.
    class RenegadePhysicsLabWorkspace final
    {
    public:
        enum class Page : std::uint8_t
        {
            World,
            RigidBody,
            Constraint,
            Character,
            Vehicle,
            Ragdoll,
            SoftBody,
            WickedCollider,
            Count,
        };

        void Create()
        {
            if (created_)
                return;
            created_ = true;

            CreateWorldControls();
            CreateRigidBodyControls();
            CreateConstraintControls();
            CreateCharacterControls();
            CreateVehicleControls();
            CreateRagdollControls();
            CreateSoftBodyControls();
            CreateWickedColliderControls();
            Refresh();
        }

        void SetActive(const bool active)
        {
            if (active_ == active)
                return;
            active_ = active;
            scrollY_ = 0.0f;
            if (active_)
                Refresh();
        }

        [[nodiscard]] bool IsActive() const noexcept { return active_; }

        void SetBounds(const XMFLOAT4& bounds)
        {
            bounds_ = bounds;
            LayoutControls();
        }

        [[nodiscard]] bool ContainsPointer(const XMFLOAT4& pointer) const noexcept
        {
            return active_ && pointer.x >= bounds_.x && pointer.x < bounds_.z &&
                pointer.y >= bounds_.y && pointer.y < bounds_.w;
        }

        [[nodiscard]] bool ConsumedPointerThisFrame() const noexcept
        {
            return pointerConsumed_;
        }

        void Update(const wi::Canvas& canvas, const float dt)
        {
            pointerConsumed_ = false;
            if (!active_ || !created_)
                return;

            auto* session = bridge::StudioSession::Current();
            if (session != session_)
            {
                session_ = session;
                selectedEntity_ = wi::ecs::INVALID_ENTITY;
                Refresh();
            }

            if (session_ != nullptr)
            {
                const wi::ecs::Entity selected = session_->Selection().HasSelection()
                    ? session_->Selection().SelectedEntity()
                    : wi::ecs::INVALID_ENTITY;
                if (selected != selectedEntity_)
                {
                    selectedEntity_ = selected;
                    scrollY_ = 0.0f;
                    Refresh();
                }
            }

            const XMFLOAT4 pointer = wi::input::GetPointer();
            if (ContainsPointer(pointer))
            {
                pointerConsumed_ = true;
                if (pointer.y >= bounds_.y + HeaderHeight &&
                    pointer.y < bounds_.y + HeaderHeight + TabsHeight &&
                    wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
                {
                    const float tabWidth = std::max(
                        76.0f,
                        (bounds_.z - bounds_.x - 32.0f) /
                            static_cast<float>(PageCount));
                    const int index = static_cast<int>(
                        (pointer.x - (bounds_.x + 16.0f)) / tabWidth);
                    if (index >= 0 && index < static_cast<int>(Page::Count))
                    {
                        page_ = static_cast<Page>(index);
                        scrollY_ = 0.0f;
                        Refresh();
                    }
                }

                const float contentTop = bounds_.y + HeaderHeight + TabsHeight + 8.0f;
                if (pointer.y >= contentTop && std::abs(pointer.z) > 0.1f)
                {
                    scrollY_ += pointer.z > 0.0f ? -ScrollStep : ScrollStep;
                    ClampScroll();
                    LayoutControls();
                }
            }

            LayoutControls();
            for (auto* widget : activeControls_)
            {
                if (widget != nullptr && widget->IsVisible())
                    widget->Update(canvas, dt);
            }
        }

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const
        {
            if (!active_ || !created_)
                return;

            const float width = std::max(1.0f, bounds_.z - bounds_.x);
            const float height = std::max(1.0f, bounds_.w - bounds_.y);
            Rect(bounds_.x, bounds_.y, width, height, Surface0, cmd);
            Rect(bounds_.x, bounds_.y, width, 1.0f, Border, cmd);
            Rect(bounds_.x, bounds_.w - 1.0f, width, 1.0f, Border, cmd);

            Text("PHYSICS LAB", bounds_.x + 22.0f, bounds_.y + 15.0f,
                17, TextStrong, cmd, 1.2f, 0.18f);
            Text("WICKED / JOLT AUTHORING", bounds_.x + 22.0f,
                bounds_.y + 38.0f, 9, Forge, cmd, 1.1f, 0.16f);

            const std::string selected = SelectedEntityLabel();
            Text(selected, bounds_.z - 420.0f, bounds_.y + 18.0f,
                10, TextSecondary, cmd, 0.25f, 0.14f);
            Text("ADVANCED RUNTIME: renegade.physics", bounds_.z - 420.0f,
                bounds_.y + 38.0f, 9, Muted, cmd, 0.2f, 0.12f);

            RenderTabs(cmd);
            RenderPageBackground(cmd);
            RenderPageReadout(cmd);

            for (const auto* widget : activeControls_)
            {
                if (widget != nullptr && widget->IsVisible())
                {
                    widget->Render(canvas, cmd);
                }
            }

            if (!status_.empty())
            {
                const float statusY = bounds_.w - 27.0f;
                Rect(bounds_.x + 14.0f, statusY - 4.0f,
                    width - 28.0f, 23.0f, Surface1, cmd);
                Text(status_, bounds_.x + 24.0f, statusY + 2.0f,
                    9, statusIsError_ ? Error : Muted, cmd, 0.15f, 0.14f);
            }
        }

        [[nodiscard]] Page ActivePage() const noexcept { return page_; }

    private:
        static constexpr std::size_t PageCount =
            static_cast<std::size_t>(Page::Count);
        static constexpr float HeaderHeight = 62.0f;
        static constexpr float TabsHeight = 38.0f;
        static constexpr float ScrollStep = 54.0f;
        static constexpr float RowHeight = 34.0f;
        static constexpr float RowGap = 8.0f;
        static constexpr float CardGap = 14.0f;
        static constexpr float CardHeader = 30.0f;

        static constexpr wi::Color Surface0 = wi::Color(7, 11, 14, 255);
        static constexpr wi::Color Surface1 = wi::Color(11, 17, 21, 255);
        static constexpr wi::Color Surface2 = wi::Color(16, 23, 28, 255);
        static constexpr wi::Color Border = wi::Color(38, 52, 61, 255);
        static constexpr wi::Color BorderSoft = wi::Color(25, 36, 43, 255);
        static constexpr wi::Color TextStrong = wi::Color(244, 244, 244, 255);
        static constexpr wi::Color TextSecondary = wi::Color(214, 222, 226, 255);
        static constexpr wi::Color Muted = wi::Color(139, 151, 158, 255);
        static constexpr wi::Color Forge = wi::Color(210, 91, 29, 255);
        static constexpr wi::Color Success = wi::Color(76, 195, 138, 255);
        static constexpr wi::Color Warning = wi::Color(229, 165, 82, 255);
        static constexpr wi::Color Error = wi::Color(229, 92, 92, 255);

        class SetPhysicsGravityCommand final : public bridge::ICommand
        {
        public:
            SetPhysicsGravityCommand(
                wi::scene::Scene& scene,
                const XMFLOAT3& before,
                const XMFLOAT3& after)
                : scene_(&scene)
                , before_(bridge::SanitizePhysicsGravity(before))
                , after_(bridge::SanitizePhysicsGravity(after))
            {
            }

            bool Execute() override
            {
                if (scene_ == nullptr || Equal(before_, after_))
                    return false;
                bridge::SetPhysicsGravity(*scene_, after_);
                return true;
            }

            void Undo() override
            {
                if (scene_ != nullptr)
                    bridge::SetPhysicsGravity(*scene_, before_);
            }

        private:
            static bool Equal(const XMFLOAT3& a, const XMFLOAT3& b) noexcept
            {
                return std::abs(a.x - b.x) < 0.00001f &&
                    std::abs(a.y - b.y) < 0.00001f &&
                    std::abs(a.z - b.z) < 0.00001f;
            }

            wi::scene::Scene* scene_ = nullptr;
            XMFLOAT3 before_ = {};
            XMFLOAT3 after_ = {};
        };

        static void Rect(
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

        static void BorderedRect(
            const float x,
            const float y,
            const float width,
            const float height,
            const wi::Color fill,
            const wi::Color border,
            const wi::graphics::CommandList cmd)
        {
            Rect(x, y, width, height, border, cmd);
            Rect(x + 1.0f, y + 1.0f, width - 2.0f, height - 2.0f, fill, cmd);
        }

        static void Text(
            const std::string& value,
            const float x,
            const float y,
            const int size,
            const wi::Color color,
            const wi::graphics::CommandList cmd,
            const float spacing = 0.0f,
            const float bolden = 0.12f)
        {
            wi::font::Params params(
                x, y, size,
                wi::font::WIFALIGN_LEFT,
                wi::font::WIFALIGN_TOP,
                color,
                wi::Color::Transparent());
            params.spacingX = spacing;
            params.bolden = bolden;
            wi::font::Draw(value, params, cmd);
        }

        [[nodiscard]] wi::scene::Scene* Scene() const noexcept
        {
            return session_ != nullptr ? &session_->Scenes().GetScene() : nullptr;
        }

        [[nodiscard]] bool HasSelection() const noexcept
        {
            return session_ != nullptr &&
                selectedEntity_ != wi::ecs::INVALID_ENTITY;
        }

        [[nodiscard]] std::string EntityLabel(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity entity) const
        {
            if (entity == wi::ecs::INVALID_ENTITY)
                return "<NONE>";
            const auto* name = scene.names.GetComponent(entity);
            const std::string prefix = name != nullptr && !name->name.empty()
                ? name->name
                : "ENTITY";
            return prefix + " [" +
                std::to_string(static_cast<unsigned long long>(entity)) + "]";
        }

        [[nodiscard]] std::string SelectedEntityLabel() const
        {
            const auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
                return "SELECTION // NONE";
            return "SELECTION // " + EntityLabel(*scene, selectedEntity_);
        }

        void SetStatus(std::string value, const bool error = false)
        {
            status_ = std::move(value);
            statusIsError_ = error;
        }

        template <typename Command, typename... Args>
        bool Execute(Args&&... args)
        {
            if (session_ == nullptr)
                return false;
            const bool changed = session_->Commands().Execute(
                std::make_unique<Command>(std::forward<Args>(args)...));
            if (changed)
            {
                Refresh();
            }
            return changed;
        }

        template <typename Fn>
        void EditCollision(Fn&& edit)
        {
            auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
                return;
            const auto* body = scene->rigidbodies.GetComponent(selectedEntity_);
            if (body == nullptr)
            {
                SetStatus("RIGID BODY // ADD A BODY BEFORE EDITING", true);
                return;
            }
            auto state = bridge::CaptureCollision(*body);
            edit(state);
            state = bridge::SanitizeCollisionState(state);
            if (!bridge::CanAuthorCollisionShape(*scene, selectedEntity_, state.shape))
            {
                SetStatus("RIGID BODY // SELECTED SHAPE IS NOT VALID FOR THIS ENTITY", true);
                Refresh();
                return;
            }
            if (Execute<bridge::SetCollisionCommand>(
                    *scene, selectedEntity_, bridge::CaptureCollision(*body), state))
                SetStatus("RIGID BODY // APPLIED");
        }

        template <typename Fn>
        void EditConstraint(Fn&& edit)
        {
            auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
                return;
            const auto* component = scene->constraints.GetComponent(selectedEntity_);
            if (component == nullptr)
            {
                SetStatus("CONSTRAINT // ADD A CONSTRAINT BEFORE EDITING", true);
                return;
            }
            auto state = bridge::CaptureConstraint(*component);
            edit(state);
            if (Execute<bridge::SetConstraintCommand>(
                    *scene, selectedEntity_, bridge::CaptureConstraint(*component), state))
                SetStatus("CONSTRAINT // APPLIED");
        }

        template <typename Fn>
        void EditCharacter(Fn&& edit)
        {
            auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
                return;
            const auto* body = scene->rigidbodies.GetComponent(selectedEntity_);
            if (body == nullptr)
            {
                SetStatus("CHARACTER // REQUIRES A RIGID BODY", true);
                return;
            }
            auto state = bridge::CaptureCharacterPhysics(*body);
            edit(state);
            if (Execute<bridge::SetCharacterPhysicsCommand>(
                    *scene, selectedEntity_, bridge::CaptureCharacterPhysics(*body), state))
                SetStatus("CHARACTER PHYSICS // APPLIED");
        }

        template <typename Fn>
        void EditVehicle(Fn&& edit)
        {
            auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
                return;
            const auto* body = scene->rigidbodies.GetComponent(selectedEntity_);
            if (body == nullptr)
            {
                SetStatus("VEHICLE // REQUIRES A RIGID BODY", true);
                return;
            }
            auto state = bridge::CaptureVehiclePhysics(*body);
            edit(state);
            if (Execute<bridge::SetVehiclePhysicsCommand>(
                    *scene, selectedEntity_, bridge::CaptureVehiclePhysics(*body), state))
                SetStatus("VEHICLE PHYSICS // APPLIED");
        }

        template <typename Fn>
        void EditRagdoll(Fn&& edit)
        {
            auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
                return;
            const auto* humanoid = scene->humanoids.GetComponent(selectedEntity_);
            if (humanoid == nullptr)
            {
                SetStatus("RAGDOLL // SELECT A HUMANOID ENTITY", true);
                return;
            }
            auto state = bridge::CaptureRagdollPhysics(*humanoid);
            edit(state);
            if (Execute<bridge::SetRagdollPhysicsCommand>(
                    *scene, selectedEntity_, bridge::CaptureRagdollPhysics(*humanoid), state))
                SetStatus("RAGDOLL PHYSICS // APPLIED");
        }

        template <typename Fn>
        void EditSoftBody(Fn&& edit)
        {
            auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
                return;
            const auto mesh = bridge::ResolveSoftBodyMeshEntity(*scene, selectedEntity_);
            const auto* body = scene->softbodies.GetComponent(mesh);
            if (body == nullptr)
            {
                SetStatus("SOFT BODY // ADD SOFT BODY TO THE SELECTED MESH", true);
                return;
            }
            auto state = bridge::CaptureSoftBodyPhysics(*body);
            edit(state);
            if (Execute<bridge::SetSoftBodyPhysicsCommand>(
                    *scene, selectedEntity_, bridge::CaptureSoftBodyPhysics(*body), state))
                SetStatus("SOFT BODY // APPLIED");
        }

        template <typename Fn>
        void EditWickedCollider(Fn&& edit)
        {
            auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
                return;
            const auto* collider = scene->colliders.GetComponent(selectedEntity_);
            if (collider == nullptr)
            {
                SetStatus("WICKED COLLIDER // ADD A COLLIDER BEFORE EDITING", true);
                return;
            }
            auto state = bridge::CaptureWickedCollider(*collider);
            edit(state);
            if (Execute<bridge::SetWickedColliderCommand>(
                    *scene, selectedEntity_, bridge::CaptureWickedCollider(*collider), state))
                SetStatus("WICKED COLLIDER // APPLIED");
        }

        template <typename Callback>
        void ConfigureSlider(
            SceneInspectorSlider& slider,
            const float minimum,
            const float maximum,
            const float value,
            const float steps,
            const char* name,
            const char* label,
            Callback&& callback)
        {
            slider.Create(minimum, maximum, value, steps, name, label);
            slider.SetRenderTextSizes(11, 11);
            slider.OnValueCommitted(std::forward<Callback>(callback));
        }

        void CreateWorldControls()
        {
            worldEnabled_.Create("PHYSICS ENABLED");
            worldEnabled_.OnClick([this](const wi::gui::EventArgs& args)
            {
                auto state = bridge::CapturePhysicsWorldState();
                state.enabled = args.bValue;
                bridge::ApplyPhysicsWorldState(state);
                SetStatus("WORLD // PHYSICS ENGINE STATE UPDATED");
                RefreshWorld();
            });
            worldSimulation_.Create("SIMULATION");
            worldSimulation_.OnClick([this](const wi::gui::EventArgs& args)
            {
                auto state = bridge::CapturePhysicsWorldState();
                state.simulationEnabled = args.bValue;
                bridge::ApplyPhysicsWorldState(state);
                SetStatus(args.bValue ? "WORLD // SIMULATION RUNNING" :
                    "WORLD // SIMULATION PAUSED");
                RefreshWorld();
            });
            worldInterpolation_.Create("INTERPOLATION");
            worldInterpolation_.OnClick([this](const wi::gui::EventArgs& args)
            {
                auto state = bridge::CapturePhysicsWorldState();
                state.interpolationEnabled = args.bValue;
                bridge::ApplyPhysicsWorldState(state);
                RefreshWorld();
            });
            worldDebug_.Create("PHYSICS VISUALIZER");
            worldDebug_.OnClick([this](const wi::gui::EventArgs& args)
            {
                auto state = bridge::CapturePhysicsWorldState();
                state.debugDrawEnabled = args.bValue;
                bridge::ApplyPhysicsWorldState(state);
                SetStatus(args.bValue ? "WORLD // PHYSICS VISUALIZER ON" :
                    "WORLD // PHYSICS VISUALIZER OFF");
                RefreshWorld();
            });

            ConfigureSlider(worldGravityX_, -100.0f, 100.0f, 0.0f, 20000,
                "Physics gravity X", "GRAVITY X", [this](const float v)
                { CommitGravity(0, v); });
            ConfigureSlider(worldGravityY_, -100.0f, 100.0f, -10.0f, 20000,
                "Physics gravity Y", "GRAVITY Y", [this](const float v)
                { CommitGravity(1, v); });
            ConfigureSlider(worldGravityZ_, -100.0f, 100.0f, 0.0f, 20000,
                "Physics gravity Z", "GRAVITY Z", [this](const float v)
                { CommitGravity(2, v); });
            ConfigureSlider(worldAccuracy_, 1.0f, 16.0f, 4.0f, 15,
                "Physics accuracy", "SOLVER ACCURACY", [this](const float v)
                { CommitWorldNumber(0, v); });
            ConfigureSlider(worldFrameRate_, 1.0f, 240.0f, 60.0f, 239,
                "Physics frame rate", "PHYSICS FRAME RATE", [this](const float v)
                { CommitWorldNumber(1, v); });
            ConfigureSlider(worldConstraintDebugSize_, 0.0f, 10.0f, 1.0f, 1000,
                "Constraint debug size", "CONSTRAINT DEBUG SIZE", [this](const float v)
                { CommitWorldNumber(2, v); });
            ConfigureSlider(worldDebugDistance_, 0.0f, 5000.0f, 500.0f, 5000,
                "Physics debug distance", "DEBUG DRAW MAX DISTANCE", [this](const float v)
                { CommitWorldNumber(3, v); });
            ConfigureSlider(worldCharacterTolerance_, 0.0f, 1.0f, 0.05f, 10000,
                "Character tolerance", "CHARACTER COLLISION TOLERANCE", [this](const float v)
                { CommitWorldNumber(4, v); });

            worldActivateAll_.Create("Physics activate all");
            worldActivateAll_.SetText("ACTIVATE ALL RIGID BODIES");
            worldActivateAll_.OnClick([this](const wi::gui::EventArgs&)
            {
                auto* scene = Scene();
                const bool ok = scene != nullptr && bridge::ActivateAllRigidBodies(*scene);
                SetStatus(ok ? "WORLD // ALL RIGID BODIES ACTIVATED" :
                    "WORLD // LIVE PHYSICS SCENE NOT READY", !ok);
            });
            worldOptimize_.Create("Physics optimize broadphase");
            worldOptimize_.SetText("OPTIMIZE BROADPHASE");
            worldOptimize_.OnClick([this](const wi::gui::EventArgs&)
            {
                auto* scene = Scene();
                const bool ok = scene != nullptr && bridge::OptimizePhysicsBroadPhase(*scene);
                SetStatus(ok ? "WORLD // BROADPHASE OPTIMIZED" :
                    "WORLD // LIVE PHYSICS SCENE NOT READY", !ok);
            });
            worldReset_.Create("Physics reset objects");
            worldReset_.SetText("RESET PHYSICS OBJECTS");
            worldReset_.OnClick([this](const wi::gui::EventArgs&)
            {
                auto* scene = Scene();
                const bool ok = scene != nullptr && bridge::ResetPhysicsObjects(*scene);
                SetStatus(ok ? "WORLD // PHYSICS OBJECTS RESET" :
                    "WORLD // LIVE PHYSICS SCENE NOT READY", !ok);
            });
        }

        void CreateRigidBodyControls()
        {
            rigidShape_.Create("Rigid body shape");
            using Shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape;
            rigidShape_.AddItem("BOX", Shape::BOX);
            rigidShape_.AddItem("SPHERE", Shape::SPHERE);
            rigidShape_.AddItem("CAPSULE", Shape::CAPSULE);
            rigidShape_.AddItem("CYLINDER", Shape::CYLINDER);
            rigidShape_.AddItem("CONVEX HULL", Shape::CONVEX_HULL);
            rigidShape_.AddItem("TRIANGLE MESH", Shape::TRIANGLE_MESH);
            rigidShape_.AddItem("HEIGHT FIELD", Shape::HEIGHTFIELD);
            rigidShape_.OnSelect([this](const wi::gui::EventArgs& args)
            {
                EditCollision([&](bridge::CollisionState& state)
                {
                    state.shape = static_cast<Shape>(args.userdata);
                });
            });

            ConfigureSlider(rigidDimX_, 0.01f, 10.0f, 1.0f, 10000,
                "Rigid dimension X", "DIMENSION X", [this](const float v)
                { CommitRigidDimension(0, v); });
            ConfigureSlider(rigidDimY_, 0.01f, 10.0f, 1.0f, 10000,
                "Rigid dimension Y", "DIMENSION Y", [this](const float v)
                { CommitRigidDimension(1, v); });
            ConfigureSlider(rigidDimZ_, 0.01f, 10.0f, 1.0f, 10000,
                "Rigid dimension Z", "DIMENSION Z", [this](const float v)
                { CommitRigidDimension(2, v); });
            ConfigureSlider(rigidMass_, 0.0f, 10000.0f, 1.0f, 100000,
                "Rigid mass", "MASS", [this](const float v)
                { EditCollision([&](auto& s) { s.mass = v; }); });
            ConfigureSlider(rigidFriction_, 0.0f, 1.0f, 0.2f, 10000,
                "Rigid friction", "FRICTION", [this](const float v)
                { EditCollision([&](auto& s) { s.friction = v; }); });
            ConfigureSlider(rigidRestitution_, 0.0f, 1.0f, 0.1f, 10000,
                "Rigid restitution", "RESTITUTION", [this](const float v)
                { EditCollision([&](auto& s) { s.restitution = v; }); });
            ConfigureSlider(rigidLinearDamping_, 0.0f, 1.0f, 0.05f, 10000,
                "Rigid linear damping", "LINEAR DAMPING", [this](const float v)
                { EditCollision([&](auto& s) { s.dampingLinear = v; }); });
            ConfigureSlider(rigidAngularDamping_, 0.0f, 1.0f, 0.05f, 10000,
                "Rigid angular damping", "ANGULAR DAMPING", [this](const float v)
                { EditCollision([&](auto& s) { s.dampingAngular = v; }); });
            ConfigureSlider(rigidBuoyancy_, 0.0f, 2.0f, 1.2f, 10000,
                "Rigid buoyancy", "BUOYANCY", [this](const float v)
                { EditCollision([&](auto& s) { s.buoyancy = v; }); });
            ConfigureSlider(rigidOffsetX_, -10.0f, 10.0f, 0.0f, 20000,
                "Rigid local offset X", "LOCAL OFFSET X", [this](const float v)
                { EditCollision([&](auto& s) { s.localOffset.x = v; }); });
            ConfigureSlider(rigidOffsetY_, -10.0f, 10.0f, 0.0f, 20000,
                "Rigid local offset Y", "LOCAL OFFSET Y", [this](const float v)
                { EditCollision([&](auto& s) { s.localOffset.y = v; }); });
            ConfigureSlider(rigidOffsetZ_, -10.0f, 10.0f, 0.0f, 20000,
                "Rigid local offset Z", "LOCAL OFFSET Z", [this](const float v)
                { EditCollision([&](auto& s) { s.localOffset.z = v; }); });
            ConfigureSlider(rigidMeshLod_, 0.0f, 6.0f, 0.0f, 6,
                "Rigid mesh LOD", "MESH LOD", [this](const float v)
                { EditCollision([&](auto& s) { s.meshLod = static_cast<uint32_t>(std::lround(v)); }); });

            rigidKinematic_.Create("KINEMATIC");
            rigidKinematic_.OnClick([this](const wi::gui::EventArgs& args)
                { EditCollision([&](auto& s) { s.kinematic = args.bValue; }); });
            rigid2D_.Create("2D LOCK");
            rigid2D_.OnClick([this](const wi::gui::EventArgs& args)
                { EditCollision([&](auto& s) { s.locked2D = args.bValue; }); });
            rigidNoSleep_.Create("DISABLE DEACTIVATION");
            rigidNoSleep_.OnClick([this](const wi::gui::EventArgs& args)
                { EditCollision([&](auto& s) { s.disableDeactivation = args.bValue; }); });
            rigidStartAsleep_.Create("START DEACTIVATED");
            rigidStartAsleep_.OnClick([this](const wi::gui::EventArgs& args)
                { EditCollision([&](auto& s) { s.startDeactivated = args.bValue; }); });

            rigidAdd_.Create("Add rigid body");
            rigidAdd_.SetText("ADD RIGID BODY");
            rigidAdd_.OnClick([this](const wi::gui::EventArgs&)
            {
                auto* scene = Scene();
                if (scene == nullptr || !HasSelection())
                    return;
                bridge::CollisionState state;
                if (!bridge::CanAuthorCollisionShape(*scene, selectedEntity_, state.shape))
                {
                    SetStatus("RIGID BODY // SELECTED ENTITY CANNOT HOST A BOX BODY", true);
                    return;
                }
                if (Execute<bridge::CreateCollisionCommand>(*scene, selectedEntity_, state))
                    SetStatus("RIGID BODY // ADDED");
            });
            rigidRemove_.Create("Remove rigid body");
            rigidRemove_.SetText("REMOVE RIGID BODY");
            rigidRemove_.OnClick([this](const wi::gui::EventArgs&)
            {
                auto* scene = Scene();
                if (scene != nullptr && HasSelection() &&
                    Execute<bridge::RemoveCollisionCommand>(*scene, selectedEntity_))
                    SetStatus("RIGID BODY // REMOVED");
            });
        }

        void CreateConstraintControls()
        {
            constraintType_.Create("Constraint type");
            using Type = wi::scene::PhysicsConstraintComponent::Type;
            constraintType_.AddItem("FIXED", Type::Fixed);
            constraintType_.AddItem("POINT", Type::Point);
            constraintType_.AddItem("DISTANCE", Type::Distance);
            constraintType_.AddItem("HINGE", Type::Hinge);
            constraintType_.AddItem("CONE", Type::Cone);
            constraintType_.AddItem("SIX DOF", Type::SixDOF);
            constraintType_.AddItem("SWING TWIST", Type::SwingTwist);
            constraintType_.AddItem("SLIDER", Type::Slider);
            constraintType_.OnSelect([this](const wi::gui::EventArgs& args)
                { EditConstraint([&](auto& s) { s.type = static_cast<Type>(args.userdata); }); });

            constraintBodyA_.Create("Constraint body A");
            constraintBodyA_.OnSelect([this](const wi::gui::EventArgs& args)
                { EditConstraint([&](auto& s) { s.bodyA = static_cast<wi::ecs::Entity>(args.userdata); }); });
            constraintBodyB_.Create("Constraint body B");
            constraintBodyB_.OnSelect([this](const wi::gui::EventArgs& args)
                { EditConstraint([&](auto& s) { s.bodyB = static_cast<wi::ecs::Entity>(args.userdata); }); });
            constraintNoSelfCollision_.Create("DISABLE SELF COLLISION");
            constraintNoSelfCollision_.OnClick([this](const wi::gui::EventArgs& args)
                { EditConstraint([&](auto& s) { s.disableSelfCollision = args.bValue; }); });

            ConfigureSlider(constraintBreak_, 0.0f, 10.0f, 10.0f, 10000,
                "Constraint break distance", "BREAK DISTANCE", [this](const float v)
                { EditConstraint([&](auto& s) { s.breakDistance = v; }); });
            ConfigureSlider(constraintMin_, -180.0f, 180.0f, 0.0f, 36000,
                "Constraint minimum", "MINIMUM", [this](const float v)
                { CommitConstraintPrimary(0, v); });
            ConfigureSlider(constraintMax_, -180.0f, 180.0f, 0.0f, 36000,
                "Constraint maximum", "MAXIMUM", [this](const float v)
                { CommitConstraintPrimary(1, v); });
            ConfigureSlider(constraintMotor1_, -100.0f, 100.0f, 0.0f, 20000,
                "Constraint motor one", "MOTOR / TARGET VELOCITY", [this](const float v)
                { CommitConstraintPrimary(2, v); });
            ConfigureSlider(constraintMotor2_, 0.0f, 10000.0f, 0.0f, 100000,
                "Constraint motor two", "MAX FORCE", [this](const float v)
                { CommitConstraintPrimary(3, v); });
            ConfigureSlider(constraintNormalCone_, 0.0f, 90.0f, 0.0f, 9000,
                "Constraint normal cone", "NORMAL CONE ANGLE", [this](const float v)
                { EditConstraint([&](auto& s) { s.swingTwistNormalHalfConeAngle = wi::math::DegreesToRadians(v); }); });
            ConfigureSlider(constraintPlaneCone_, 0.0f, 90.0f, 0.0f, 9000,
                "Constraint plane cone", "PLANE CONE ANGLE", [this](const float v)
                { EditConstraint([&](auto& s) { s.swingTwistPlaneHalfConeAngle = wi::math::DegreesToRadians(v); }); });

            for (std::size_t i = 0; i < constraintAxis_.size(); ++i)
            {
                const bool minimum = (i % 2) == 0;
                const bool rotation = i >= 6;
                const int axis = static_cast<int>((i / 2) % 3);
                const char* axisName = axis == 0 ? "X" : axis == 1 ? "Y" : "Z";
                const std::string label = std::string(minimum ? "MIN " : "MAX ") +
                    (rotation ? "ROTATION " : "TRANSLATION ") + axisName;
                constraintAxisLabels_[i] = label;
                ConfigureSlider(
                    constraintAxis_[i],
                    rotation ? (minimum ? -180.0f : 0.0f) : (minimum ? -10.0f : 0.0f),
                    rotation ? (minimum ? 0.0f : 180.0f) : (minimum ? 0.0f : 10.0f),
                    0.0f,
                    10000,
                    constraintAxisLabels_[i].c_str(),
                    constraintAxisLabels_[i].c_str(),
                    [this, i](const float v) { CommitConstraintAxis(i, v); });
            }

            constexpr std::array<const char*, 6> fixLabels = {
                "FIX X", "FIX Y", "FIX Z", "FIX ROT X", "FIX ROT Y", "FIX ROT Z"};
            for (std::size_t i = 0; i < constraintFix_.size(); ++i)
            {
                constraintFix_[i].Create(fixLabels[i]);
                constraintFix_[i].SetText(fixLabels[i]);
                constraintFix_[i].OnClick([this, i](const wi::gui::EventArgs&)
                    { FixConstraintAxis(i); });
            }

            constraintAdd_.Create("Add constraint");
            constraintAdd_.SetText("ADD CONSTRAINT");
            constraintAdd_.OnClick([this](const wi::gui::EventArgs&)
            {
                auto* scene = Scene();
                if (scene == nullptr || !HasSelection() ||
                    !scene->transforms.Contains(selectedEntity_))
                {
                    SetStatus("CONSTRAINT // SELECT AN ENTITY WITH A TRANSFORM", true);
                    return;
                }
                if (Execute<bridge::CreateConstraintCommand>(*scene, selectedEntity_))
                    SetStatus("CONSTRAINT // ADDED");
            });
            constraintRemove_.Create("Remove constraint");
            constraintRemove_.SetText("REMOVE CONSTRAINT");
            constraintRemove_.OnClick([this](const wi::gui::EventArgs&)
            {
                auto* scene = Scene();
                if (scene != nullptr && HasSelection() &&
                    Execute<bridge::RemoveConstraintCommand>(*scene, selectedEntity_))
                    SetStatus("CONSTRAINT // REMOVED");
            });
            constraintRebind_.Create("Rebind constraint");
            constraintRebind_.SetText("REBIND CONSTRAINT");
            constraintRebind_.OnClick([this](const wi::gui::EventArgs&)
            {
                auto* scene = Scene();
                const bool ok = scene != nullptr && HasSelection() &&
                    bridge::RebindConstraint(*scene, selectedEntity_);
                SetStatus(ok ? "CONSTRAINT // REBIND REQUESTED" :
                    "CONSTRAINT // REBIND NOT AVAILABLE", !ok);
            });
        }

        void CreateCharacterControls()
        {
            characterEnabled_.Create("CHARACTER PHYSICS");
            characterEnabled_.OnClick([this](const wi::gui::EventArgs& args)
                { EditCharacter([&](auto& s) { s.enabled = args.bValue; }); });
            ConfigureSlider(characterSlope_, 0.0f, 90.0f, 50.0f, 9000,
                "Character slope", "MAX SLOPE ANGLE", [this](const float v)
                { EditCharacter([&](auto& s) { s.maxSlopeAngle = wi::math::DegreesToRadians(v); }); });
            ConfigureSlider(characterGravity_, 0.0f, 4.0f, 1.0f, 4000,
                "Character gravity", "GRAVITY FACTOR", [this](const float v)
                { EditCharacter([&](auto& s) { s.gravityFactor = v; }); });
        }

        void CreateVehicleControls()
        {
            using Type = wi::scene::RigidBodyPhysicsComponent::Vehicle::Type;
            using Mode = wi::scene::RigidBodyPhysicsComponent::Vehicle::CollisionMode;
            vehicleType_.Create("Vehicle type");
            vehicleType_.AddItem("NONE", Type::None);
            vehicleType_.AddItem("CAR", Type::Car);
            vehicleType_.AddItem("MOTORCYCLE", Type::Motorcycle);
            vehicleType_.OnSelect([this](const wi::gui::EventArgs& args)
                { EditVehicle([&](auto& s) { s.type = static_cast<Type>(args.userdata); }); });
            vehicleCollision_.Create("Vehicle collision mode");
            vehicleCollision_.AddItem("RAY", Mode::Ray);
            vehicleCollision_.AddItem("SPHERE", Mode::Sphere);
            vehicleCollision_.AddItem("CYLINDER", Mode::Cylinder);
            vehicleCollision_.OnSelect([this](const wi::gui::EventArgs& args)
                { EditVehicle([&](auto& s) { s.collisionMode = static_cast<Mode>(args.userdata); }); });

            constexpr std::array<const char*, 18> labels = {
                "CHASSIS HALF WIDTH", "CHASSIS HALF HEIGHT", "CHASSIS HALF LENGTH",
                "FRONT WHEEL OFFSET", "REAR WHEEL OFFSET", "WHEEL RADIUS", "WHEEL WIDTH",
                "MAX ENGINE TORQUE", "CLUTCH STRENGTH", "MAX ROLL ANGLE", "MAX STEERING ANGLE",
                "FRONT SUSPENSION MIN", "FRONT SUSPENSION MAX", "FRONT SUSPENSION FREQUENCY",
                "FRONT SUSPENSION DAMPING", "REAR SUSPENSION MIN", "REAR SUSPENSION MAX",
                "REAR SUSPENSION FREQUENCY"};
            constexpr std::array<float, 18> mins = {
                0, -10, 0, -10, -10, .001f, .001f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            constexpr std::array<float, 18> maxs = {
                10, 10, 10, 10, 10, 10, 10, 1000, 100, 180, 90, 2, 2, 2, 2, 2, 2, 2};
            for (std::size_t i = 0; i < vehicleNumeric_.size(); ++i)
            {
                vehicleNumericLabels_[i] = labels[i];
                ConfigureSlider(vehicleNumeric_[i], mins[i], maxs[i], 0.0f, 10000,
                    vehicleNumericLabels_[i].c_str(), vehicleNumericLabels_[i].c_str(),
                    [this, i](const float v) { CommitVehicleNumber(i, v); });
            }
            ConfigureSlider(vehicleRearDamping_, 0.0f, 2.0f, 0.5f, 10000,
                "Rear suspension damping", "REAR SUSPENSION DAMPING", [this](const float v)
                { EditVehicle([&](auto& s) { s.rearSuspensionDamping = v; }); });
            ConfigureSlider(vehicleMotoAngle_, 0.0f, 90.0f, 30.0f, 9000,
                "Motorcycle suspension angle", "MOTORCYCLE FRONT SUSPENSION ANGLE", [this](const float v)
                { EditVehicle([&](auto& s) { s.motorcycleFrontSuspensionAngle = wi::math::DegreesToRadians(v); }); });
            ConfigureSlider(vehicleMotoFrontBrake_, 0.0f, 2000.0f, 500.0f, 20000,
                "Motorcycle front brake", "MOTORCYCLE FRONT BRAKE TORQUE", [this](const float v)
                { EditVehicle([&](auto& s) { s.motorcycleFrontBrakeTorque = v; }); });
            ConfigureSlider(vehicleMotoRearBrake_, 0.0f, 2000.0f, 250.0f, 20000,
                "Motorcycle rear brake", "MOTORCYCLE REAR BRAKE TORQUE", [this](const float v)
                { EditVehicle([&](auto& s) { s.motorcycleRearBrakeTorque = v; }); });

            vehicle4WD_.Create("FOUR WHEEL DRIVE");
            vehicle4WD_.OnClick([this](const wi::gui::EventArgs& args)
                { EditVehicle([&](auto& s) { s.fourWheelDrive = args.bValue; }); });
            vehicleLean_.Create("MOTORCYCLE LEAN CONTROL");
            vehicleLean_.OnClick([this](const wi::gui::EventArgs& args)
                { EditVehicle([&](auto& s) { s.motorcycleLeanControl = args.bValue; }); });

            for (std::size_t i = 0; i < vehicleWheelEntity_.size(); ++i)
            {
                vehicleWheelEntity_[i].Create("Vehicle wheel entity");
                vehicleWheelEntity_[i].OnSelect([this, i](const wi::gui::EventArgs& args)
                    { CommitVehicleWheelEntity(i, static_cast<wi::ecs::Entity>(args.userdata)); });
            }
        }

        void CreateRagdollControls()
        {
            ragdollDisabled_.Create("RAGDOLL DISABLED");
            ragdollDisabled_.OnClick([this](const wi::gui::EventArgs& args)
                { EditRagdoll([&](auto& s) { s.disabled = args.bValue; }); });
            ragdollPhysics_.Create("RAGDOLL PHYSICS ENABLED");
            ragdollPhysics_.OnClick([this](const wi::gui::EventArgs& args)
                { EditRagdoll([&](auto& s) { s.physicsEnabled = args.bValue; }); });
            ragdoll2D_.Create("2D LOCK");
            ragdoll2D_.OnClick([this](const wi::gui::EventArgs& args)
                { EditRagdoll([&](auto& s) { s.locked2D = args.bValue; }); });
            ConfigureSlider(ragdollFatness_, 0.5f, 2.0f, 1.0f, 15000,
                "Ragdoll fatness", "RAGDOLL FATNESS", [this](const float v)
                { EditRagdoll([&](auto& s) { s.fatness = v; }); });
            ConfigureSlider(ragdollHeadSize_, 0.5f, 2.0f, 1.0f, 15000,
                "Ragdoll head size", "HEAD SIZE", [this](const float v)
                { EditRagdoll([&](auto& s) { s.headSize = v; }); });
            ragdollActivateAll_.Create("Activate all ragdolls");
            ragdollActivateAll_.SetText("ACTIVATE ALL RAGDOLLS");
            ragdollActivateAll_.OnClick([this](const wi::gui::EventArgs&)
            {
                auto* scene = Scene();
                if (scene != nullptr)
                {
                    bridge::ActivateAllRagdolls(*scene);
                    SetStatus("RAGDOLL // ALL HUMANOIDS ENABLED FOR RAGDOLL PHYSICS");
                    Refresh();
                }
            });
        }

        void CreateSoftBodyControls()
        {
            ConfigureSlider(softDetail_, 0.001f, 1.0f, 1.0f, 9999,
                "Soft body detail", "DETAIL / LOD", [this](const float v)
                { EditSoftBody([&](auto& s) { s.detail = v; }); });
            ConfigureSlider(softMass_, 0.0f, 100.0f, 1.0f, 10000,
                "Soft body mass", "MASS", [this](const float v)
                { EditSoftBody([&](auto& s) { s.mass = v; }); });
            ConfigureSlider(softFriction_, 0.0f, 1.0f, 0.5f, 10000,
                "Soft body friction", "FRICTION", [this](const float v)
                { EditSoftBody([&](auto& s) { s.friction = v; }); });
            ConfigureSlider(softRestitution_, 0.0f, 1.0f, 0.0f, 10000,
                "Soft body restitution", "RESTITUTION", [this](const float v)
                { EditSoftBody([&](auto& s) { s.restitution = v; }); });
            ConfigureSlider(softPressure_, 0.0f, 100000.0f, 0.0f, 100000,
                "Soft body pressure", "PRESSURE", [this](const float v)
                { EditSoftBody([&](auto& s) { s.pressure = v; }); });
            ConfigureSlider(softVertexRadius_, 0.0f, 1.0f, 0.2f, 10000,
                "Soft body vertex radius", "VERTEX RADIUS", [this](const float v)
                { EditSoftBody([&](auto& s) { s.vertexRadius = v; }); });
            softWind_.Create("WIND");
            softWind_.OnClick([this](const wi::gui::EventArgs& args)
                { EditSoftBody([&](auto& s) { s.windEnabled = args.bValue; }); });
            softAdd_.Create("Add soft body");
            softAdd_.SetText("ADD SOFT BODY");
            softAdd_.OnClick([this](const wi::gui::EventArgs&)
            {
                auto* scene = Scene();
                if (scene == nullptr || !HasSelection() ||
                    bridge::ResolveSoftBodyMeshEntity(*scene, selectedEntity_) == wi::ecs::INVALID_ENTITY)
                {
                    SetStatus("SOFT BODY // SELECT A MESH OR OBJECT WITH A MESH", true);
                    return;
                }
                if (Execute<bridge::CreateSoftBodyPhysicsCommand>(*scene, selectedEntity_))
                    SetStatus("SOFT BODY // ADDED");
            });
            softRemove_.Create("Remove soft body");
            softRemove_.SetText("REMOVE SOFT BODY");
            softRemove_.OnClick([this](const wi::gui::EventArgs&)
            {
                auto* scene = Scene();
                if (scene != nullptr && HasSelection() &&
                    Execute<bridge::RemoveSoftBodyPhysicsCommand>(*scene, selectedEntity_))
                    SetStatus("SOFT BODY // REMOVED");
            });
            softReset_.Create("Reset soft body");
            softReset_.SetText("RESET SOFT BODY");
            softReset_.OnClick([this](const wi::gui::EventArgs&)
            {
                auto* scene = Scene();
                const bool ok = scene != nullptr && HasSelection() &&
                    bridge::ResetSoftBodyPhysics(*scene, selectedEntity_);
                SetStatus(ok ? "SOFT BODY // RESET" : "SOFT BODY // RESET UNAVAILABLE", !ok);
            });
            softActivate_.Create("Activate soft body");
            softActivate_.SetText("ACTIVATE LIVE SOFT BODY");
            softActivate_.OnClick([this](const wi::gui::EventArgs&)
            {
                auto* scene = Scene();
                const bool ok = scene != nullptr && HasSelection() &&
                    bridge::SetSoftBodyActive(*scene, selectedEntity_, true);
                SetStatus(ok ? "SOFT BODY // ACTIVATED" : "SOFT BODY // LIVE BODY NOT READY", !ok);
            });
        }

        void CreateWickedColliderControls()
        {
            using Shape = wi::scene::ColliderComponent::Shape;
            colliderShape_.Create("Wicked collider shape");
            colliderShape_.AddItem("SPHERE", Shape::Sphere);
            colliderShape_.AddItem("CAPSULE", Shape::Capsule);
            colliderShape_.AddItem("PLANE", Shape::Plane);
            colliderShape_.OnSelect([this](const wi::gui::EventArgs& args)
                { EditWickedCollider([&](auto& s) { s.shape = static_cast<Shape>(args.userdata); }); });
            colliderCPU_.Create("CPU COLLISION");
            colliderCPU_.OnClick([this](const wi::gui::EventArgs& args)
                { EditWickedCollider([&](auto& s) { s.cpuEnabled = args.bValue; }); });
            colliderGPU_.Create("GPU COLLISION");
            colliderGPU_.OnClick([this](const wi::gui::EventArgs& args)
                { EditWickedCollider([&](auto& s) { s.gpuEnabled = args.bValue; }); });
            ConfigureSlider(colliderRadius_, 0.0f, 10.0f, 1.0f, 10000,
                "Wicked collider radius", "RADIUS", [this](const float v)
                { EditWickedCollider([&](auto& s) { s.radius = v; }); });
            ConfigureSlider(colliderOffsetX_, -10.0f, 10.0f, 0.0f, 20000,
                "Wicked collider offset X", "OFFSET X", [this](const float v)
                { EditWickedCollider([&](auto& s) { s.offset.x = v; }); });
            ConfigureSlider(colliderOffsetY_, -10.0f, 10.0f, 0.0f, 20000,
                "Wicked collider offset Y", "OFFSET Y", [this](const float v)
                { EditWickedCollider([&](auto& s) { s.offset.y = v; }); });
            ConfigureSlider(colliderOffsetZ_, -10.0f, 10.0f, 0.0f, 20000,
                "Wicked collider offset Z", "OFFSET Z", [this](const float v)
                { EditWickedCollider([&](auto& s) { s.offset.z = v; }); });
            ConfigureSlider(colliderTailX_, -10.0f, 10.0f, 0.0f, 20000,
                "Wicked collider tail X", "TAIL X", [this](const float v)
                { EditWickedCollider([&](auto& s) { s.tail.x = v; }); });
            ConfigureSlider(colliderTailY_, -10.0f, 10.0f, 1.0f, 20000,
                "Wicked collider tail Y", "TAIL Y", [this](const float v)
                { EditWickedCollider([&](auto& s) { s.tail.y = v; }); });
            ConfigureSlider(colliderTailZ_, -10.0f, 10.0f, 0.0f, 20000,
                "Wicked collider tail Z", "TAIL Z", [this](const float v)
                { EditWickedCollider([&](auto& s) { s.tail.z = v; }); });
            colliderAdd_.Create("Add Wicked collider");
            colliderAdd_.SetText("ADD WICKED COLLIDER");
            colliderAdd_.OnClick([this](const wi::gui::EventArgs&)
            {
                auto* scene = Scene();
                if (scene != nullptr && HasSelection() &&
                    Execute<bridge::CreateWickedColliderCommand>(*scene, selectedEntity_))
                    SetStatus("WICKED COLLIDER // ADDED");
            });
            colliderRemove_.Create("Remove Wicked collider");
            colliderRemove_.SetText("REMOVE WICKED COLLIDER");
            colliderRemove_.OnClick([this](const wi::gui::EventArgs&)
            {
                auto* scene = Scene();
                if (scene != nullptr && HasSelection() &&
                    Execute<bridge::RemoveWickedColliderCommand>(*scene, selectedEntity_))
                    SetStatus("WICKED COLLIDER // REMOVED");
            });
        }

        void CommitGravity(const int axis, const float value)
        {
            auto* scene = Scene();
            if (scene == nullptr || session_ == nullptr)
                return;
            const XMFLOAT3 before = bridge::GetPhysicsGravity(*scene);
            XMFLOAT3 after = before;
            if (axis == 0) after.x = value;
            else if (axis == 1) after.y = value;
            else after.z = value;
            if (Execute<SetPhysicsGravityCommand>(*scene, before, after))
                SetStatus("WORLD // SERIALIZED GRAVITY UPDATED");
        }

        void CommitWorldNumber(const int field, const float value)
        {
            auto state = bridge::CapturePhysicsWorldState();
            switch (field)
            {
            case 0: state.accuracy = static_cast<int>(std::lround(value)); break;
            case 1: state.frameRate = value; break;
            case 2: state.constraintDebugSize = value; break;
            case 3: state.debugDrawMaxDistance = value; break;
            case 4: state.characterCollisionTolerance = value; break;
            default: return;
            }
            bridge::ApplyPhysicsWorldState(state);
            SetStatus("WORLD // SESSION / RUNTIME SETTING UPDATED");
            RefreshWorld();
        }

        void CommitRigidDimension(const int axis, const float value)
        {
            EditCollision([&](bridge::CollisionState& state)
            {
                using Shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape;
                switch (state.shape)
                {
                case Shape::BOX:
                    if (axis == 0) state.boxHalfExtents.x = value;
                    else if (axis == 1) state.boxHalfExtents.y = value;
                    else state.boxHalfExtents.z = value;
                    break;
                case Shape::SPHERE:
                    if (axis == 0) state.sphereRadius = value;
                    break;
                case Shape::CAPSULE:
                case Shape::CYLINDER:
                    if (axis == 0) state.capsuleHeight = value;
                    else if (axis == 1) state.capsuleRadius = value;
                    break;
                default:
                    break;
                }
            });
        }

        void CommitConstraintPrimary(const int field, const float value)
        {
            EditConstraint([&](bridge::ConstraintState& state)
            {
                using Type = wi::scene::PhysicsConstraintComponent::Type;
                switch (state.type)
                {
                case Type::Distance:
                    if (field == 0) state.distanceMin = std::max(0.0f, value);
                    else if (field == 1) state.distanceMax = std::max(0.0f, value);
                    break;
                case Type::Hinge:
                    if (field == 0) state.hingeMinAngle = wi::math::DegreesToRadians(value);
                    else if (field == 1) state.hingeMaxAngle = wi::math::DegreesToRadians(value);
                    else if (field == 2) state.hingeTargetAngularVelocity = value;
                    break;
                case Type::Cone:
                    if (field == 0) state.coneHalfAngle =
                        wi::math::DegreesToRadians(std::clamp(value, 0.0f, 90.0f));
                    break;
                case Type::SwingTwist:
                    if (field == 0) state.swingTwistMinAngle = wi::math::DegreesToRadians(value);
                    else if (field == 1) state.swingTwistMaxAngle = wi::math::DegreesToRadians(value);
                    break;
                case Type::Slider:
                    if (field == 0) state.sliderMinLimit = value;
                    else if (field == 1) state.sliderMaxLimit = value;
                    else if (field == 2) state.sliderTargetVelocity = value;
                    else if (field == 3) state.sliderMaxForce = std::max(0.0f, value);
                    break;
                default:
                    break;
                }
            });
        }

        void CommitConstraintAxis(const std::size_t slot, const float value)
        {
            if (slot >= 12)
                return;
            EditConstraint([&](bridge::ConstraintState& s)
            {
                const bool minimum = (slot % 2) == 0;
                const bool rotation = slot >= 6;
                const int axis = static_cast<int>((slot / 2) % 3);
                XMFLOAT3* target = nullptr;
                if (rotation)
                    target = minimum ? &s.sixDofMinRotation : &s.sixDofMaxRotation;
                else
                    target = minimum ? &s.sixDofMinTranslation : &s.sixDofMaxTranslation;
                const float stored = rotation ? wi::math::DegreesToRadians(value) : value;
                if (axis == 0) target->x = stored;
                else if (axis == 1) target->y = stored;
                else target->z = stored;
            });
        }

        void FixConstraintAxis(const std::size_t slot)
        {
            if (slot >= 6)
                return;
            EditConstraint([&](bridge::ConstraintState& s)
            {
                const bool rotation = slot >= 3;
                const int axis = static_cast<int>(slot % 3);
                XMFLOAT3* minimum = rotation ? &s.sixDofMinRotation : &s.sixDofMinTranslation;
                XMFLOAT3* maximum = rotation ? &s.sixDofMaxRotation : &s.sixDofMaxTranslation;
                if (axis == 0) { minimum->x = 0.0f; maximum->x = 0.0f; }
                else if (axis == 1) { minimum->y = 0.0f; maximum->y = 0.0f; }
                else { minimum->z = 0.0f; maximum->z = 0.0f; }
            });
        }

        void CommitVehicleNumber(const std::size_t field, const float value)
        {
            EditVehicle([&](bridge::VehiclePhysicsState& s)
            {
                switch (field)
                {
                case 0: s.chassisHalfWidth = value; break;
                case 1: s.chassisHalfHeight = value; break;
                case 2: s.chassisHalfLength = value; break;
                case 3: s.frontWheelOffset = value; break;
                case 4: s.rearWheelOffset = value; break;
                case 5: s.wheelRadius = value; break;
                case 6: s.wheelWidth = value; break;
                case 7: s.maxEngineTorque = value; break;
                case 8: s.clutchStrength = value; break;
                case 9: s.maxRollAngle = wi::math::DegreesToRadians(value); break;
                case 10: s.maxSteeringAngle = wi::math::DegreesToRadians(value); break;
                case 11: s.frontSuspensionMinLength = value; break;
                case 12: s.frontSuspensionMaxLength = value; break;
                case 13: s.frontSuspensionFrequency = value; break;
                case 14: s.frontSuspensionDamping = value; break;
                case 15: s.rearSuspensionMinLength = value; break;
                case 16: s.rearSuspensionMaxLength = value; break;
                case 17: s.rearSuspensionFrequency = value; break;
                default: break;
                }
            });
        }

        void CommitVehicleWheelEntity(const std::size_t slot, const wi::ecs::Entity entity)
        {
            EditVehicle([&](bridge::VehiclePhysicsState& s)
            {
                if (slot == 0) s.wheelEntityFrontLeft = entity;
                else if (slot == 1) s.wheelEntityFrontRight = entity;
                else if (slot == 2) s.wheelEntityRearLeft = entity;
                else if (slot == 3) s.wheelEntityRearRight = entity;
            });
        }

        void Refresh()
        {
            if (!created_)
                return;
            RefreshWorld();
            RefreshRigidBody();
            RefreshConstraint();
            RefreshCharacter();
            RefreshVehicle();
            RefreshRagdoll();
            RefreshSoftBody();
            RefreshWickedCollider();
            LayoutControls();
        }

        void RefreshWorld()
        {
            const auto state = bridge::CapturePhysicsWorldState();
            worldEnabled_.SetCheck(state.enabled);
            worldSimulation_.SetCheck(state.simulationEnabled);
            worldInterpolation_.SetCheck(state.interpolationEnabled);
            worldDebug_.SetCheck(state.debugDrawEnabled);
            worldAccuracy_.SetValue(static_cast<float>(state.accuracy));
            worldFrameRate_.SetValue(state.frameRate);
            worldConstraintDebugSize_.SetValue(state.constraintDebugSize);
            worldDebugDistance_.SetValue(state.debugDrawMaxDistance);
            worldCharacterTolerance_.SetValue(state.characterCollisionTolerance);
            if (const auto* scene = Scene())
            {
                const auto gravity = bridge::GetPhysicsGravity(*scene);
                worldGravityX_.SetValue(gravity.x);
                worldGravityY_.SetValue(gravity.y);
                worldGravityZ_.SetValue(gravity.z);
            }
        }

        void RefreshRigidBody()
        {
            const auto* scene = Scene();
            const auto* body = scene != nullptr && HasSelection()
                ? scene->rigidbodies.GetComponent(selectedEntity_) : nullptr;
            const bool has = body != nullptr;
            rigidAdd_.SetEnabled(!has && HasSelection());
            rigidRemove_.SetEnabled(has);
            if (!has)
                return;
            const auto s = bridge::CaptureCollision(*body);
            rigidShape_.SetSelectedWithoutCallback(ShapeIndex(s.shape));
            rigidMass_.SetValue(s.mass);
            rigidFriction_.SetValue(s.friction);
            rigidRestitution_.SetValue(s.restitution);
            rigidLinearDamping_.SetValue(s.dampingLinear);
            rigidAngularDamping_.SetValue(s.dampingAngular);
            rigidBuoyancy_.SetValue(s.buoyancy);
            rigidOffsetX_.SetValue(s.localOffset.x);
            rigidOffsetY_.SetValue(s.localOffset.y);
            rigidOffsetZ_.SetValue(s.localOffset.z);
            rigidMeshLod_.SetValue(static_cast<float>(s.meshLod));
            rigidKinematic_.SetCheck(s.kinematic);
            rigid2D_.SetCheck(s.locked2D);
            rigidNoSleep_.SetCheck(s.disableDeactivation);
            rigidStartAsleep_.SetCheck(s.startDeactivated);
            using Shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape;
            if (s.shape == Shape::BOX)
            {
                rigidDimX_.SetValue(s.boxHalfExtents.x);
                rigidDimY_.SetValue(s.boxHalfExtents.y);
                rigidDimZ_.SetValue(s.boxHalfExtents.z);
            }
            else if (s.shape == Shape::SPHERE)
            {
                rigidDimX_.SetValue(s.sphereRadius);
            }
            else if (s.shape == Shape::CAPSULE || s.shape == Shape::CYLINDER)
            {
                rigidDimX_.SetValue(s.capsuleHeight);
                rigidDimY_.SetValue(s.capsuleRadius);
            }
        }

        void RefreshConstraint()
        {
            auto* scene = Scene();
            const auto* c = scene != nullptr && HasSelection()
                ? scene->constraints.GetComponent(selectedEntity_) : nullptr;
            constraintAdd_.SetEnabled(c == nullptr && HasSelection() &&
                scene != nullptr && scene->transforms.Contains(selectedEntity_));
            constraintRemove_.SetEnabled(c != nullptr);
            constraintRebind_.SetEnabled(c != nullptr);
            RebuildConstraintBodyCombos(c);
            if (c == nullptr)
                return;
            const auto s = bridge::CaptureConstraint(*c);
            constraintType_.SetSelectedWithoutCallback(ConstraintTypeIndex(s.type));
            SelectEntityCombo(constraintBodyA_, s.bodyA);
            SelectEntityCombo(constraintBodyB_, s.bodyB);
            constraintNoSelfCollision_.SetCheck(s.disableSelfCollision);
            constraintBreak_.SetValue(std::isfinite(s.breakDistance)
                ? std::clamp(s.breakDistance, 0.0f, 10.0f) : 10.0f);
            RefreshConstraintPrimary(s);
            RefreshConstraintAxes(s);
        }

        void RefreshCharacter()
        {
            const auto* scene = Scene();
            const auto* body = scene != nullptr && HasSelection()
                ? scene->rigidbodies.GetComponent(selectedEntity_) : nullptr;
            if (body == nullptr)
            {
                characterEnabled_.SetCheck(false);
                return;
            }
            const auto s = bridge::CaptureCharacterPhysics(*body);
            characterEnabled_.SetCheck(s.enabled);
            characterSlope_.SetValue(wi::math::RadiansToDegrees(s.maxSlopeAngle));
            characterGravity_.SetValue(s.gravityFactor);
        }

        void RefreshVehicle()
        {
            auto* scene = Scene();
            const auto* body = scene != nullptr && HasSelection()
                ? scene->rigidbodies.GetComponent(selectedEntity_) : nullptr;
            RebuildVehicleWheelCombos(body);
            if (body == nullptr)
                return;
            const auto s = bridge::CaptureVehiclePhysics(*body);
            vehicleType_.SetSelectedWithoutCallback(VehicleTypeIndex(s.type));
            vehicleCollision_.SetSelectedWithoutCallback(VehicleCollisionIndex(s.collisionMode));
            const std::array<float, 18> values = {
                s.chassisHalfWidth, s.chassisHalfHeight, s.chassisHalfLength,
                s.frontWheelOffset, s.rearWheelOffset, s.wheelRadius, s.wheelWidth,
                s.maxEngineTorque, s.clutchStrength,
                wi::math::RadiansToDegrees(s.maxRollAngle),
                wi::math::RadiansToDegrees(s.maxSteeringAngle),
                s.frontSuspensionMinLength, s.frontSuspensionMaxLength,
                s.frontSuspensionFrequency, s.frontSuspensionDamping,
                s.rearSuspensionMinLength, s.rearSuspensionMaxLength,
                s.rearSuspensionFrequency};
            for (std::size_t i = 0; i < values.size(); ++i)
                vehicleNumeric_[i].SetValue(values[i]);
            vehicleRearDamping_.SetValue(s.rearSuspensionDamping);
            vehicleMotoAngle_.SetValue(wi::math::RadiansToDegrees(s.motorcycleFrontSuspensionAngle));
            vehicleMotoFrontBrake_.SetValue(s.motorcycleFrontBrakeTorque);
            vehicleMotoRearBrake_.SetValue(s.motorcycleRearBrakeTorque);
            vehicle4WD_.SetCheck(s.fourWheelDrive);
            vehicleLean_.SetCheck(s.motorcycleLeanControl);
            SelectEntityCombo(vehicleWheelEntity_[0], s.wheelEntityFrontLeft);
            SelectEntityCombo(vehicleWheelEntity_[1], s.wheelEntityFrontRight);
            SelectEntityCombo(vehicleWheelEntity_[2], s.wheelEntityRearLeft);
            SelectEntityCombo(vehicleWheelEntity_[3], s.wheelEntityRearRight);
        }

        void RefreshRagdoll()
        {
            const auto* scene = Scene();
            const auto* h = scene != nullptr && HasSelection()
                ? scene->humanoids.GetComponent(selectedEntity_) : nullptr;
            if (h == nullptr)
                return;
            const auto s = bridge::CaptureRagdollPhysics(*h);
            ragdollDisabled_.SetCheck(s.disabled);
            ragdollPhysics_.SetCheck(s.physicsEnabled);
            ragdoll2D_.SetCheck(s.locked2D);
            ragdollFatness_.SetValue(s.fatness);
            ragdollHeadSize_.SetValue(s.headSize);
        }

        void RefreshSoftBody()
        {
            auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
                return;
            const auto mesh = bridge::ResolveSoftBodyMeshEntity(*scene, selectedEntity_);
            const auto* body = scene->softbodies.GetComponent(mesh);
            softAdd_.SetEnabled(mesh != wi::ecs::INVALID_ENTITY && body == nullptr);
            softRemove_.SetEnabled(body != nullptr);
            softReset_.SetEnabled(body != nullptr);
            softActivate_.SetEnabled(body != nullptr);
            if (body == nullptr)
                return;
            const auto s = bridge::CaptureSoftBodyPhysics(*body);
            softDetail_.SetValue(s.detail);
            softMass_.SetValue(s.mass);
            softFriction_.SetValue(s.friction);
            softRestitution_.SetValue(s.restitution);
            softPressure_.SetValue(s.pressure);
            softVertexRadius_.SetValue(s.vertexRadius);
            softWind_.SetCheck(s.windEnabled);
        }

        void RefreshWickedCollider()
        {
            const auto* scene = Scene();
            const auto* c = scene != nullptr && HasSelection()
                ? scene->colliders.GetComponent(selectedEntity_) : nullptr;
            colliderAdd_.SetEnabled(c == nullptr && HasSelection());
            colliderRemove_.SetEnabled(c != nullptr);
            if (c == nullptr)
                return;
            const auto s = bridge::CaptureWickedCollider(*c);
            colliderShape_.SetSelectedWithoutCallback(ColliderShapeIndex(s.shape));
            colliderCPU_.SetCheck(s.cpuEnabled);
            colliderGPU_.SetCheck(s.gpuEnabled);
            colliderRadius_.SetValue(s.radius);
            colliderOffsetX_.SetValue(s.offset.x);
            colliderOffsetY_.SetValue(s.offset.y);
            colliderOffsetZ_.SetValue(s.offset.z);
            colliderTailX_.SetValue(s.tail.x);
            colliderTailY_.SetValue(s.tail.y);
            colliderTailZ_.SetValue(s.tail.z);
        }

        static int ShapeIndex(const wi::scene::RigidBodyPhysicsComponent::CollisionShape shape)
        {
            using Shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape;
            switch (shape)
            {
            case Shape::BOX: return 0;
            case Shape::SPHERE: return 1;
            case Shape::CAPSULE: return 2;
            case Shape::CYLINDER: return 3;
            case Shape::CONVEX_HULL: return 4;
            case Shape::TRIANGLE_MESH: return 5;
            case Shape::HEIGHTFIELD: return 6;
            default: return 0;
            }
        }

        static int ConstraintTypeIndex(const wi::scene::PhysicsConstraintComponent::Type type)
        {
            using Type = wi::scene::PhysicsConstraintComponent::Type;
            switch (type)
            {
            case Type::Fixed: return 0;
            case Type::Point: return 1;
            case Type::Distance: return 2;
            case Type::Hinge: return 3;
            case Type::Cone: return 4;
            case Type::SixDOF: return 5;
            case Type::SwingTwist: return 6;
            case Type::Slider: return 7;
            default: return 0;
            }
        }

        static int VehicleTypeIndex(const wi::scene::RigidBodyPhysicsComponent::Vehicle::Type type)
        {
            using Type = wi::scene::RigidBodyPhysicsComponent::Vehicle::Type;
            switch (type)
            {
            case Type::None: return 0;
            case Type::Car: return 1;
            case Type::Motorcycle: return 2;
            default: return 0;
            }
        }

        static int VehicleCollisionIndex(
            const wi::scene::RigidBodyPhysicsComponent::Vehicle::CollisionMode mode)
        {
            using Mode = wi::scene::RigidBodyPhysicsComponent::Vehicle::CollisionMode;
            switch (mode)
            {
            case Mode::Ray: return 0;
            case Mode::Sphere: return 1;
            case Mode::Cylinder: return 2;
            default: return 0;
            }
        }

        static int ColliderShapeIndex(const wi::scene::ColliderComponent::Shape shape)
        {
            using Shape = wi::scene::ColliderComponent::Shape;
            switch (shape)
            {
            case Shape::Sphere: return 0;
            case Shape::Capsule: return 1;
            case Shape::Plane: return 2;
            default: return 0;
            }
        }

        void RebuildConstraintBodyCombos(
            const wi::scene::PhysicsConstraintComponent*)
        {
            auto* scene = Scene();
            for (auto* combo : {&constraintBodyA_, &constraintBodyB_})
            {
                combo->ClearItems();
                combo->AddItem("<NONE>", wi::ecs::INVALID_ENTITY);
                if (scene == nullptr)
                    continue;
                for (std::size_t i = 0; i < scene->rigidbodies.GetCount(); ++i)
                {
                    const auto entity = scene->rigidbodies.GetEntity(i);
                    combo->AddItem(EntityLabel(*scene, entity), entity);
                }
            }
        }

        void RebuildVehicleWheelCombos(
            const wi::scene::RigidBodyPhysicsComponent*)
        {
            auto* scene = Scene();
            for (auto& combo : vehicleWheelEntity_)
            {
                combo.ClearItems();
                combo.AddItem("<NONE>", wi::ecs::INVALID_ENTITY);
                if (scene == nullptr)
                    continue;
                for (std::size_t i = 0; i < scene->transforms.GetCount(); ++i)
                {
                    const auto entity = scene->transforms.GetEntity(i);
                    combo.AddItem(EntityLabel(*scene, entity), entity);
                }
            }
        }

        static void SelectEntityCombo(
            SceneInspectorComboBox& combo,
            const wi::ecs::Entity entity)
        {
            int selected = 0;
            for (int i = 0; i < static_cast<int>(combo.items.size()); ++i)
            {
                if (static_cast<wi::ecs::Entity>(combo.items[static_cast<std::size_t>(i)].userdata) == entity)
                {
                    selected = i;
                    break;
                }
            }
            combo.SetSelectedWithoutCallback(selected);
        }

        void RefreshConstraintPrimary(const bridge::ConstraintState& s)
        {
            using Type = wi::scene::PhysicsConstraintComponent::Type;
            switch (s.type)
            {
            case Type::Distance:
                constraintMin_.SetValue(s.distanceMin);
                constraintMax_.SetValue(s.distanceMax);
                break;
            case Type::Hinge:
                constraintMin_.SetValue(wi::math::RadiansToDegrees(s.hingeMinAngle));
                constraintMax_.SetValue(wi::math::RadiansToDegrees(s.hingeMaxAngle));
                constraintMotor1_.SetValue(s.hingeTargetAngularVelocity);
                break;
            case Type::Cone:
                constraintMin_.SetValue(wi::math::RadiansToDegrees(s.coneHalfAngle));
                break;
            case Type::SwingTwist:
                constraintMin_.SetValue(wi::math::RadiansToDegrees(s.swingTwistMinAngle));
                constraintMax_.SetValue(wi::math::RadiansToDegrees(s.swingTwistMaxAngle));
                constraintNormalCone_.SetValue(
                    wi::math::RadiansToDegrees(s.swingTwistNormalHalfConeAngle));
                constraintPlaneCone_.SetValue(
                    wi::math::RadiansToDegrees(s.swingTwistPlaneHalfConeAngle));
                break;
            case Type::Slider:
                constraintMin_.SetValue(s.sliderMinLimit);
                constraintMax_.SetValue(s.sliderMaxLimit);
                constraintMotor1_.SetValue(s.sliderTargetVelocity);
                constraintMotor2_.SetValue(s.sliderMaxForce);
                break;
            default:
                break;
            }
        }

        void RefreshConstraintAxes(const bridge::ConstraintState& s)
        {
            const std::array<float, 12> values = {
                s.sixDofMinTranslation.x, s.sixDofMaxTranslation.x,
                s.sixDofMinTranslation.y, s.sixDofMaxTranslation.y,
                s.sixDofMinTranslation.z, s.sixDofMaxTranslation.z,
                wi::math::RadiansToDegrees(s.sixDofMinRotation.x),
                wi::math::RadiansToDegrees(s.sixDofMaxRotation.x),
                wi::math::RadiansToDegrees(s.sixDofMinRotation.y),
                wi::math::RadiansToDegrees(s.sixDofMaxRotation.y),
                wi::math::RadiansToDegrees(s.sixDofMinRotation.z),
                wi::math::RadiansToDegrees(s.sixDofMaxRotation.z)};
            for (std::size_t i = 0; i < values.size(); ++i)
            {
                const float fallback = (i % 2) == 0 ?
                    (i >= 6 ? -180.0f : -10.0f) :
                    (i >= 6 ? 180.0f : 10.0f);
                constraintAxis_[i].SetValue(std::isfinite(values[i]) ? values[i] : fallback);
            }
        }

        void RenderTabs(const wi::graphics::CommandList cmd) const
        {
            constexpr std::array<const char*, PageCount> labels = {
                "WORLD", "RIGID BODY", "CONSTRAINT", "CHARACTER",
                "VEHICLE", "RAGDOLL", "SOFT BODY", "WICKED COLLIDER"};
            const float tabWidth = std::max(
                76.0f,
                (bounds_.z - bounds_.x - 32.0f) /
                    static_cast<float>(PageCount));
            float x = bounds_.x + 16.0f;
            const float y = bounds_.y + HeaderHeight;
            for (std::size_t i = 0; i < labels.size(); ++i)
            {
                const bool active = static_cast<std::size_t>(page_) == i;
                Rect(x, y, tabWidth - 4.0f, TabsHeight - 2.0f,
                    active ? Surface2 : Surface1, cmd);
                if (active)
                    Rect(x, y + TabsHeight - 4.0f, tabWidth - 4.0f, 2.0f, Forge, cmd);
                Text(labels[i], x + 9.0f, y + 12.0f, 9,
                    active ? TextStrong : TextSecondary, cmd, 0.35f, active ? 0.17f : 0.12f);
                x += tabWidth;
            }
        }

        void RenderPageBackground(const wi::graphics::CommandList cmd) const
        {
            const float top = bounds_.y + HeaderHeight + TabsHeight + 8.0f;
            Rect(bounds_.x + 14.0f, top,
                bounds_.z - bounds_.x - 28.0f,
                bounds_.w - top - 36.0f,
                Surface1, cmd);
        }

        void RenderPageReadout(const wi::graphics::CommandList cmd) const
        {
            const float x = bounds_.x + 30.0f;
            const float y = bounds_.y + HeaderHeight + TabsHeight + 20.0f;
            switch (page_)
            {
            case Page::World:
                Text("WORLD / SESSION", x, y, 12, TextStrong, cmd, 0.8f, 0.18f);
                Text("Gravity is serialized in Wicked Scene weather. Solver/debug controls are live session/runtime settings.",
                    x, y + 20.0f, 9, Muted, cmd);
                break;
            case Page::RigidBody:
                Text("RIGID BODY", x, y, 12, TextStrong, cmd, 0.8f, 0.18f);
                RenderRigidReadout(x, y + 20.0f, cmd);
                break;
            case Page::Constraint:
                Text("CONSTRAINT", x, y, 12, TextStrong, cmd, 0.8f, 0.18f);
                Text("Eight Wicked Editor constraint types. Body binding changes recreate the native Jolt constraint.",
                    x, y + 20.0f, 9, Muted, cmd);
                break;
            case Page::Character:
                Text("PHYSICS CHARACTER", x, y, 12, TextStrong, cmd, 0.8f, 0.18f);
                RenderCharacterReadout(x, y + 20.0f, cmd);
                break;
            case Page::Vehicle:
                Text("VEHICLE", x, y, 12, TextStrong, cmd, 0.8f, 0.18f);
                RenderVehicleReadout(x, y + 20.0f, cmd);
                break;
            case Page::Ragdoll:
                Text("HUMANOID / RAGDOLL", x, y, 12, TextStrong, cmd, 0.8f, 0.18f);
                RenderRagdollReadout(x, y + 20.0f, cmd);
                break;
            case Page::SoftBody:
                Text("SOFT BODY", x, y, 12, TextStrong, cmd, 0.8f, 0.18f);
                RenderSoftBodyReadout(x, y + 20.0f, cmd);
                break;
            case Page::WickedCollider:
                Text("WICKED COLLIDER // PARTICLES, HAIR & SPRINGS // NOT JOLT",
                    x, y, 11, Warning, cmd, 0.45f, 0.18f);
                Text("Lightweight CPU/GPU collision primitives used by Wicked secondary systems.",
                    x, y + 20.0f, 9, Muted, cmd);
                break;
            default:
                break;
            }
        }

        void RenderRigidReadout(
            const float x, const float y, const wi::graphics::CommandList cmd) const
        {
            const auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
            {
                Text("Select a Scene entity to author physics.", x, y, 9, Muted, cmd);
                return;
            }
            const auto* body = scene->rigidbodies.GetComponent(selectedEntity_);
            if (body == nullptr)
            {
                Text("No rigid body component. ADD RIGID BODY is available below.", x, y, 9, Muted, cmd);
                return;
            }
            Text(bridge::HasLivePhysicsBody(*scene, selectedEntity_)
                    ? "LIVE NATIVE BODY // READY"
                    : "AUTHORED BODY // NATIVE BODY PENDING SIMULATION",
                x, y, 9, bridge::HasLivePhysicsBody(*scene, selectedEntity_) ? Success : Muted, cmd);
        }

        void RenderCharacterReadout(
            const float x, const float y, const wi::graphics::CommandList cmd) const
        {
            auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
            {
                Text("Select an entity with a rigid body. Capsule is recommended.", x, y, 9, Muted, cmd);
                return;
            }
            if (bridge::HasCharacterVehiclePhysicsConflict(*scene, selectedEntity_))
            {
                Text("CONFLICT // CHARACTER AND VEHICLE MODES ARE BOTH ENABLED", x, y, 9, Error, cmd);
                return;
            }
            bridge::CharacterGroundInfo ground;
            if (!bridge::GetCharacterGroundInfo(*scene, selectedEntity_, ground))
            {
                Text("Character runtime ground data becomes available while the native body is live.", x, y, 9, Muted, cmd);
                return;
            }
            const char* state = ground.state == bridge::CharacterGroundState::OnGround ? "ON GROUND" :
                ground.state == bridge::CharacterGroundState::OnSteepGround ? "STEEP GROUND" :
                ground.state == bridge::CharacterGroundState::InAir ? "IN AIR" : "NOT SUPPORTED";
            Text(std::string("LIVE GROUND STATE // ") + state, x, y, 9, Success, cmd);
        }

        void RenderVehicleReadout(
            const float x, const float y, const wi::graphics::CommandList cmd) const
        {
            auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
            {
                Text("Select an entity with a rigid body to configure a vehicle.", x, y, 9, Muted, cmd);
                return;
            }
            if (bridge::HasCharacterVehiclePhysicsConflict(*scene, selectedEntity_))
            {
                Text("CONFLICT // DISABLE CHARACTER PHYSICS BEFORE DRIVING", x, y, 9, Error, cmd);
                return;
            }
            float velocity = 0.0f;
            if (bridge::GetVehicleForwardVelocity(*scene, selectedEntity_, velocity))
                Text("LIVE FORWARD VELOCITY // " + std::to_string(velocity), x, y, 9, Success, cmd);
            else
                Text("Vehicle runtime velocity appears when the native vehicle is live.", x, y, 9, Muted, cmd);
        }

        void RenderRagdollReadout(
            const float x, const float y, const wi::graphics::CommandList cmd) const
        {
            const auto* scene = Scene();
            if (scene == nullptr || !HasSelection() ||
                scene->humanoids.GetComponent(selectedEntity_) == nullptr)
                Text("Select a Humanoid entity to author ragdoll physics.", x, y, 9, Muted, cmd);
            else
                Text(bridge::HasLiveRagdollPhysics(*scene, selectedEntity_)
                        ? "LIVE RAGDOLL // READY" : "RAGDOLL AUTHORED // NATIVE RAGDOLL PENDING",
                    x, y, 9, bridge::HasLiveRagdollPhysics(*scene, selectedEntity_) ? Success : Muted, cmd);
        }

        void RenderSoftBodyReadout(
            const float x, const float y, const wi::graphics::CommandList cmd) const
        {
            const auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
            {
                Text("Select a Mesh or Object that resolves to a Mesh.", x, y, 9, Muted, cmd);
                return;
            }
            const auto mesh = bridge::ResolveSoftBodyMeshEntity(*scene, selectedEntity_);
            if (mesh == wi::ecs::INVALID_ENTITY)
                Text("Selected entity does not resolve to a mesh.", x, y, 9, Error, cmd);
            else
                Text("RESOLVED MESH // " + std::to_string(static_cast<unsigned long long>(mesh)) +
                    (bridge::HasLiveSoftBodyPhysics(*scene, selectedEntity_) ? " // LIVE" : " // AUTHORED"),
                    x, y, 9, bridge::HasLiveSoftBodyPhysics(*scene, selectedEntity_) ? Success : Muted, cmd);
        }

        void LayoutControls()
        {
            activeControls_.clear();
            if (!active_ || bounds_.z <= bounds_.x || bounds_.w <= bounds_.y)
                return;

            const float contentTop = bounds_.y + HeaderHeight + TabsHeight + 58.0f;
            const float left = bounds_.x + 30.0f;
            const float right = bounds_.z - 30.0f;
            const float width = right - left;
            const float columnGap = 18.0f;
            const float columnWidth = (width - columnGap) * 0.5f;
            const float y0 = contentTop - scrollY_;
            pageContentHeight_ = 0.0f;

            switch (page_)
            {
            case Page::World: LayoutWorld(left, y0, width, columnWidth, columnGap); break;
            case Page::RigidBody: LayoutRigidBody(left, y0, width, columnWidth, columnGap); break;
            case Page::Constraint: LayoutConstraint(left, y0, width, columnWidth, columnGap); break;
            case Page::Character: LayoutCharacter(left, y0, width, columnWidth, columnGap); break;
            case Page::Vehicle: LayoutVehicle(left, y0, width, columnWidth, columnGap); break;
            case Page::Ragdoll: LayoutRagdoll(left, y0, width, columnWidth, columnGap); break;
            case Page::SoftBody: LayoutSoftBody(left, y0, width, columnWidth, columnGap); break;
            case Page::WickedCollider: LayoutWickedCollider(left, y0, width, columnWidth, columnGap); break;
            default: break;
            }
            ClampScroll();
        }

        void ClampScroll()
        {
            const float visible = std::max(
                100.0f,
                bounds_.w - (bounds_.y + HeaderHeight + TabsHeight + 96.0f));
            const float maximum = std::max(0.0f, pageContentHeight_ - visible);
            scrollY_ = std::clamp(scrollY_, 0.0f, maximum);
        }

        template <typename Widget>
        void Place(Widget& widget, const float x, const float y, const float w,
            const float h = RowHeight)
        {
            widget.SetPos(XMFLOAT2(x, y));
            widget.SetSize(XMFLOAT2(w, h));
            const float visibleTop = bounds_.y + HeaderHeight + TabsHeight + 50.0f;
            const float visibleBottom = bounds_.w - 34.0f;
            const bool visible = y + h >= visibleTop && y <= visibleBottom;
            widget.SetVisible(visible);
            activeControls_.push_back(&widget);
        }

        void HideAllControls()
        {
            for (auto* widget : allControls_)
                if (widget != nullptr) widget->SetVisible(false);
        }

        float RowY(const float y, const int row) const noexcept
        {
            return y + static_cast<float>(row) * (RowHeight + RowGap);
        }

        void LayoutWorld(const float x, const float y, const float width,
            const float column, const float gap)
        {
            RegisterAllControlsOnce();
            HideAllControls();
            Place(worldEnabled_, x, RowY(y, 0), column);
            Place(worldSimulation_, x + column + gap, RowY(y, 0), column);
            Place(worldInterpolation_, x, RowY(y, 1), column);
            Place(worldDebug_, x + column + gap, RowY(y, 1), column);
            Place(worldGravityX_, x, RowY(y, 3), column);
            Place(worldGravityY_, x + column + gap, RowY(y, 3), column);
            Place(worldGravityZ_, x, RowY(y, 4), column);
            Place(worldAccuracy_, x + column + gap, RowY(y, 4), column);
            Place(worldFrameRate_, x, RowY(y, 5), column);
            Place(worldConstraintDebugSize_, x + column + gap, RowY(y, 5), column);
            Place(worldDebugDistance_, x, RowY(y, 6), column);
            Place(worldCharacterTolerance_, x + column + gap, RowY(y, 6), column);
            Place(worldActivateAll_, x, RowY(y, 8), column);
            Place(worldOptimize_, x + column + gap, RowY(y, 8), column);
            Place(worldReset_, x, RowY(y, 9), width);
            pageContentHeight_ = RowY(0.0f, 11);
        }

        void LayoutRigidBody(const float x, const float y, const float width,
            const float column, const float gap)
        {
            RegisterAllControlsOnce();
            HideAllControls();
            const auto* scene = Scene();
            const auto* body = scene != nullptr && HasSelection()
                ? scene->rigidbodies.GetComponent(selectedEntity_) : nullptr;
            Place(rigidAdd_, x, RowY(y, 0), column);
            Place(rigidRemove_, x + column + gap, RowY(y, 0), column);
            if (body == nullptr)
            {
                pageContentHeight_ = RowY(0.0f, 3);
                return;
            }
            const auto s = bridge::CaptureCollision(*body);
            Place(rigidShape_, x, RowY(y, 2), width);
            using Shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape;
            if (s.shape == Shape::BOX)
            {
                Place(rigidDimX_, x, RowY(y, 3), column);
                Place(rigidDimY_, x + column + gap, RowY(y, 3), column);
                Place(rigidDimZ_, x, RowY(y, 4), column);
            }
            else if (s.shape == Shape::SPHERE)
                Place(rigidDimX_, x, RowY(y, 3), column);
            else if (s.shape == Shape::CAPSULE || s.shape == Shape::CYLINDER)
            {
                Place(rigidDimX_, x, RowY(y, 3), column);
                Place(rigidDimY_, x + column + gap, RowY(y, 3), column);
            }
            Place(rigidMass_, x, RowY(y, 6), column);
            Place(rigidFriction_, x + column + gap, RowY(y, 6), column);
            Place(rigidRestitution_, x, RowY(y, 7), column);
            Place(rigidBuoyancy_, x + column + gap, RowY(y, 7), column);
            Place(rigidLinearDamping_, x, RowY(y, 8), column);
            Place(rigidAngularDamping_, x + column + gap, RowY(y, 8), column);
            Place(rigidKinematic_, x, RowY(y, 10), column);
            Place(rigid2D_, x + column + gap, RowY(y, 10), column);
            Place(rigidNoSleep_, x, RowY(y, 11), column);
            Place(rigidStartAsleep_, x + column + gap, RowY(y, 11), column);
            Place(rigidOffsetX_, x, RowY(y, 13), column);
            Place(rigidOffsetY_, x + column + gap, RowY(y, 13), column);
            Place(rigidOffsetZ_, x, RowY(y, 14), column);
            if (s.shape == Shape::TRIANGLE_MESH)
                Place(rigidMeshLod_, x + column + gap, RowY(y, 14), column);
            pageContentHeight_ = RowY(0.0f, 17);
        }

        void LayoutConstraint(const float x, const float y, const float width,
            const float column, const float gap)
        {
            RegisterAllControlsOnce();
            HideAllControls();
            auto* scene = Scene();
            const auto* c = scene != nullptr && HasSelection()
                ? scene->constraints.GetComponent(selectedEntity_) : nullptr;
            Place(constraintAdd_, x, RowY(y, 0), column);
            Place(constraintRemove_, x + column + gap, RowY(y, 0), column);
            if (c == nullptr)
            {
                pageContentHeight_ = RowY(0.0f, 3);
                return;
            }
            const auto s = bridge::CaptureConstraint(*c);
            Place(constraintType_, x, RowY(y, 2), width);
            Place(constraintBodyA_, x, RowY(y, 3), column);
            Place(constraintBodyB_, x + column + gap, RowY(y, 3), column);
            Place(constraintNoSelfCollision_, x, RowY(y, 4), column);
            Place(constraintRebind_, x + column + gap, RowY(y, 4), column);
            Place(constraintBreak_, x, RowY(y, 5), width);

            using Type = wi::scene::PhysicsConstraintComponent::Type;
            int row = 7;
            if (s.type == Type::Distance || s.type == Type::Hinge ||
                s.type == Type::SwingTwist || s.type == Type::Slider)
            {
                Place(constraintMin_, x, RowY(y, row), column);
                Place(constraintMax_, x + column + gap, RowY(y, row), column);
                ++row;
            }
            else if (s.type == Type::Cone)
            {
                Place(constraintMin_, x, RowY(y, row), column);
                ++row;
            }
            if (s.type == Type::Hinge || s.type == Type::Slider)
            {
                Place(constraintMotor1_, x, RowY(y, row), column);
                if (s.type == Type::Slider)
                    Place(constraintMotor2_, x + column + gap, RowY(y, row), column);
                ++row;
            }
            if (s.type == Type::SwingTwist)
            {
                Place(constraintNormalCone_, x, RowY(y, row), column);
                Place(constraintPlaneCone_, x + column + gap, RowY(y, row), column);
                ++row;
            }
            if (s.type == Type::SixDOF)
            {
                for (std::size_t i = 0; i < 6; ++i)
                {
                    Place(constraintFix_[i],
                        i % 2 == 0 ? x : x + column + gap,
                        RowY(y, row + static_cast<int>(i / 2)), column);
                }
                row += 4;
                for (std::size_t i = 0; i < constraintAxis_.size(); ++i)
                {
                    Place(constraintAxis_[i],
                        i % 2 == 0 ? x : x + column + gap,
                        RowY(y, row + static_cast<int>(i / 2)), column);
                }
                row += 7;
            }
            pageContentHeight_ = RowY(0.0f, row + 2);
        }

        void LayoutCharacter(const float x, const float y, const float,
            const float column, const float gap)
        {
            RegisterAllControlsOnce();
            HideAllControls();
            Place(characterEnabled_, x, RowY(y, 0), column);
            Place(characterSlope_, x, RowY(y, 2), column);
            Place(characterGravity_, x + column + gap, RowY(y, 2), column);
            pageContentHeight_ = RowY(0.0f, 6);
        }

        void LayoutVehicle(const float x, const float y, const float width,
            const float column, const float gap)
        {
            RegisterAllControlsOnce();
            HideAllControls();
            const auto* scene = Scene();
            const auto* body = scene != nullptr && HasSelection()
                ? scene->rigidbodies.GetComponent(selectedEntity_) : nullptr;
            if (body == nullptr)
            {
                pageContentHeight_ = RowY(0.0f, 3);
                return;
            }
            const auto s = bridge::CaptureVehiclePhysics(*body);
            Place(vehicleType_, x, RowY(y, 0), column);
            Place(vehicleCollision_, x + column + gap, RowY(y, 0), column);
            int row = 2;
            for (std::size_t i = 0; i < vehicleNumeric_.size(); ++i)
            {
                Place(vehicleNumeric_[i], i % 2 == 0 ? x : x + column + gap,
                    RowY(y, row + static_cast<int>(i / 2)), column);
            }
            row += 10;
            Place(vehicleRearDamping_, x, RowY(y, row), column);
            Place(vehicle4WD_, x + column + gap, RowY(y, row), column);
            row += 2;

            using Type = wi::scene::RigidBodyPhysicsComponent::Vehicle::Type;
            if (s.type == Type::Motorcycle)
            {
                Place(vehicleMotoAngle_, x, RowY(y, row), column);
                Place(vehicleMotoFrontBrake_, x + column + gap, RowY(y, row), column);
                ++row;
                Place(vehicleMotoRearBrake_, x, RowY(y, row), column);
                Place(vehicleLean_, x + column + gap, RowY(y, row), column);
                row += 2;
            }
            Place(vehicleWheelEntity_[0], x, RowY(y, row), column);
            Place(vehicleWheelEntity_[1], x + column + gap, RowY(y, row), column);
            ++row;
            Place(vehicleWheelEntity_[2], x, RowY(y, row), column);
            Place(vehicleWheelEntity_[3], x + column + gap, RowY(y, row), column);
            pageContentHeight_ = RowY(0.0f, row + 4);
        }

        void LayoutRagdoll(const float x, const float y, const float width,
            const float column, const float gap)
        {
            RegisterAllControlsOnce();
            HideAllControls();
            Place(ragdollDisabled_, x, RowY(y, 0), column);
            Place(ragdollPhysics_, x + column + gap, RowY(y, 0), column);
            Place(ragdoll2D_, x, RowY(y, 1), column);
            Place(ragdollFatness_, x, RowY(y, 3), column);
            Place(ragdollHeadSize_, x + column + gap, RowY(y, 3), column);
            Place(ragdollActivateAll_, x, RowY(y, 5), width);
            pageContentHeight_ = RowY(0.0f, 8);
        }

        void LayoutSoftBody(const float x, const float y, const float width,
            const float column, const float gap)
        {
            RegisterAllControlsOnce();
            HideAllControls();
            Place(softAdd_, x, RowY(y, 0), column);
            Place(softRemove_, x + column + gap, RowY(y, 0), column);
            Place(softReset_, x, RowY(y, 1), column);
            Place(softActivate_, x + column + gap, RowY(y, 1), column);
            Place(softDetail_, x, RowY(y, 3), column);
            Place(softMass_, x + column + gap, RowY(y, 3), column);
            Place(softFriction_, x, RowY(y, 4), column);
            Place(softRestitution_, x + column + gap, RowY(y, 4), column);
            Place(softPressure_, x, RowY(y, 5), column);
            Place(softVertexRadius_, x + column + gap, RowY(y, 5), column);
            Place(softWind_, x, RowY(y, 7), width);
            pageContentHeight_ = RowY(0.0f, 10);
        }

        void LayoutWickedCollider(const float x, const float y, const float,
            const float column, const float gap)
        {
            RegisterAllControlsOnce();
            HideAllControls();
            Place(colliderAdd_, x, RowY(y, 0), column);
            Place(colliderRemove_, x + column + gap, RowY(y, 0), column);
            Place(colliderShape_, x, RowY(y, 2), column);
            Place(colliderCPU_, x + column + gap, RowY(y, 2), column);
            Place(colliderGPU_, x, RowY(y, 3), column);
            Place(colliderRadius_, x + column + gap, RowY(y, 3), column);
            Place(colliderOffsetX_, x, RowY(y, 5), column);
            Place(colliderOffsetY_, x + column + gap, RowY(y, 5), column);
            Place(colliderOffsetZ_, x, RowY(y, 6), column);
            Place(colliderTailX_, x, RowY(y, 8), column);
            Place(colliderTailY_, x + column + gap, RowY(y, 8), column);
            Place(colliderTailZ_, x, RowY(y, 9), column);
            pageContentHeight_ = RowY(0.0f, 12);
        }

        void RegisterAllControlsOnce()
        {
            if (!allControls_.empty())
                return;
            const auto add = [this](wi::gui::Widget& w) { allControls_.push_back(&w); };
            for (wi::gui::Widget* w : {
                &worldEnabled_, &worldSimulation_, &worldInterpolation_, &worldDebug_,
                &worldGravityX_, &worldGravityY_, &worldGravityZ_, &worldAccuracy_,
                &worldFrameRate_, &worldConstraintDebugSize_, &worldDebugDistance_,
                &worldCharacterTolerance_, &worldActivateAll_, &worldOptimize_, &worldReset_,
                &rigidShape_, &rigidDimX_, &rigidDimY_, &rigidDimZ_, &rigidMass_,
                &rigidFriction_, &rigidRestitution_, &rigidLinearDamping_, &rigidAngularDamping_,
                &rigidBuoyancy_, &rigidOffsetX_, &rigidOffsetY_, &rigidOffsetZ_, &rigidMeshLod_,
                &rigidKinematic_, &rigid2D_, &rigidNoSleep_, &rigidStartAsleep_, &rigidAdd_, &rigidRemove_,
                &constraintType_, &constraintBodyA_, &constraintBodyB_, &constraintNoSelfCollision_,
                &constraintBreak_, &constraintMin_, &constraintMax_, &constraintMotor1_, &constraintMotor2_,
                &constraintNormalCone_, &constraintPlaneCone_, &constraintAdd_, &constraintRemove_,
                &constraintRebind_, &characterEnabled_, &characterSlope_, &characterGravity_,
                &vehicleType_, &vehicleCollision_, &vehicleRearDamping_, &vehicleMotoAngle_,
                &vehicleMotoFrontBrake_, &vehicleMotoRearBrake_, &vehicle4WD_, &vehicleLean_,
                &ragdollDisabled_, &ragdollPhysics_, &ragdoll2D_, &ragdollFatness_, &ragdollHeadSize_,
                &ragdollActivateAll_, &softDetail_, &softMass_, &softFriction_, &softRestitution_,
                &softPressure_, &softVertexRadius_, &softWind_, &softAdd_, &softRemove_, &softReset_,
                &softActivate_, &colliderShape_, &colliderCPU_, &colliderGPU_, &colliderRadius_,
                &colliderOffsetX_, &colliderOffsetY_, &colliderOffsetZ_, &colliderTailX_, &colliderTailY_,
                &colliderTailZ_, &colliderAdd_, &colliderRemove_})
            {
                if (w != nullptr) add(*w);
            }
            for (auto& w : constraintAxis_) add(w);
            for (auto& w : constraintFix_) add(w);
            for (auto& w : vehicleNumeric_) add(w);
            for (auto& w : vehicleWheelEntity_) add(w);
        }

        XMFLOAT4 bounds_ = XMFLOAT4(320.0f, 98.0f, 1560.0f, 1020.0f);
        bridge::StudioSession* session_ = nullptr;
        wi::ecs::Entity selectedEntity_ = wi::ecs::INVALID_ENTITY;
        Page page_ = Page::World;
        bool created_ = false;
        bool active_ = false;
        bool pointerConsumed_ = false;
        float scrollY_ = 0.0f;
        float pageContentHeight_ = 0.0f;
        std::string status_ = "PHYSICS LAB // READY";
        bool statusIsError_ = false;
        std::vector<wi::gui::Widget*> activeControls_;
        std::vector<wi::gui::Widget*> allControls_;

        SceneInspectorCheckBox worldEnabled_;
        SceneInspectorCheckBox worldSimulation_;
        SceneInspectorCheckBox worldInterpolation_;
        SceneInspectorCheckBox worldDebug_;
        SceneInspectorSlider worldGravityX_;
        SceneInspectorSlider worldGravityY_;
        SceneInspectorSlider worldGravityZ_;
        SceneInspectorSlider worldAccuracy_;
        SceneInspectorSlider worldFrameRate_;
        SceneInspectorSlider worldConstraintDebugSize_;
        SceneInspectorSlider worldDebugDistance_;
        SceneInspectorSlider worldCharacterTolerance_;
        SceneInspectorButton worldActivateAll_;
        SceneInspectorButton worldOptimize_;
        SceneInspectorButton worldReset_;

        SceneInspectorComboBox rigidShape_;
        SceneInspectorSlider rigidDimX_;
        SceneInspectorSlider rigidDimY_;
        SceneInspectorSlider rigidDimZ_;
        SceneInspectorSlider rigidMass_;
        SceneInspectorSlider rigidFriction_;
        SceneInspectorSlider rigidRestitution_;
        SceneInspectorSlider rigidLinearDamping_;
        SceneInspectorSlider rigidAngularDamping_;
        SceneInspectorSlider rigidBuoyancy_;
        SceneInspectorSlider rigidOffsetX_;
        SceneInspectorSlider rigidOffsetY_;
        SceneInspectorSlider rigidOffsetZ_;
        SceneInspectorSlider rigidMeshLod_;
        SceneInspectorCheckBox rigidKinematic_;
        SceneInspectorCheckBox rigid2D_;
        SceneInspectorCheckBox rigidNoSleep_;
        SceneInspectorCheckBox rigidStartAsleep_;
        SceneInspectorButton rigidAdd_;
        SceneInspectorButton rigidRemove_;

        SceneInspectorComboBox constraintType_;
        SceneInspectorComboBox constraintBodyA_;
        SceneInspectorComboBox constraintBodyB_;
        SceneInspectorCheckBox constraintNoSelfCollision_;
        SceneInspectorSlider constraintBreak_;
        SceneInspectorSlider constraintMin_;
        SceneInspectorSlider constraintMax_;
        SceneInspectorSlider constraintMotor1_;
        SceneInspectorSlider constraintMotor2_;
        SceneInspectorSlider constraintNormalCone_;
        SceneInspectorSlider constraintPlaneCone_;
        std::array<SceneInspectorSlider, 12> constraintAxis_;
        std::array<std::string, 12> constraintAxisLabels_;
        std::array<SceneInspectorButton, 6> constraintFix_;
        SceneInspectorButton constraintAdd_;
        SceneInspectorButton constraintRemove_;
        SceneInspectorButton constraintRebind_;

        SceneInspectorCheckBox characterEnabled_;
        SceneInspectorSlider characterSlope_;
        SceneInspectorSlider characterGravity_;

        SceneInspectorComboBox vehicleType_;
        SceneInspectorComboBox vehicleCollision_;
        std::array<SceneInspectorSlider, 18> vehicleNumeric_;
        std::array<std::string, 18> vehicleNumericLabels_;
        SceneInspectorSlider vehicleRearDamping_;
        SceneInspectorSlider vehicleMotoAngle_;
        SceneInspectorSlider vehicleMotoFrontBrake_;
        SceneInspectorSlider vehicleMotoRearBrake_;
        SceneInspectorCheckBox vehicle4WD_;
        SceneInspectorCheckBox vehicleLean_;
        std::array<SceneInspectorComboBox, 4> vehicleWheelEntity_;

        SceneInspectorCheckBox ragdollDisabled_;
        SceneInspectorCheckBox ragdollPhysics_;
        SceneInspectorCheckBox ragdoll2D_;
        SceneInspectorSlider ragdollFatness_;
        SceneInspectorSlider ragdollHeadSize_;
        SceneInspectorButton ragdollActivateAll_;

        SceneInspectorSlider softDetail_;
        SceneInspectorSlider softMass_;
        SceneInspectorSlider softFriction_;
        SceneInspectorSlider softRestitution_;
        SceneInspectorSlider softPressure_;
        SceneInspectorSlider softVertexRadius_;
        SceneInspectorCheckBox softWind_;
        SceneInspectorButton softAdd_;
        SceneInspectorButton softRemove_;
        SceneInspectorButton softReset_;
        SceneInspectorButton softActivate_;

        SceneInspectorComboBox colliderShape_;
        SceneInspectorCheckBox colliderCPU_;
        SceneInspectorCheckBox colliderGPU_;
        SceneInspectorSlider colliderRadius_;
        SceneInspectorSlider colliderOffsetX_;
        SceneInspectorSlider colliderOffsetY_;
        SceneInspectorSlider colliderOffsetZ_;
        SceneInspectorSlider colliderTailX_;
        SceneInspectorSlider colliderTailY_;
        SceneInspectorSlider colliderTailZ_;
        SceneInspectorButton colliderAdd_;
        SceneInspectorButton colliderRemove_;
    };
}
