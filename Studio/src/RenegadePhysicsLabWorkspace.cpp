#include "RenegadePhysicsLabWorkspace.h"

#include "RenegadeStudioChrome.h"
#include "renegade/bridge/FlightVehicleService.h"
#include "renegade/bridge/FixedWingTuning.h"
#include "renegade/bridge/TrackedVehicleTuning.h"
#include "renegade/bridge/FixedWingRig.h"
#include "renegade/bridge/CharacterPhysicsService.h"
#include "renegade/bridge/PhysicsGameplayService.h"
#include "renegade/bridge/CollisionService.h"
#include "renegade/bridge/CompoundColliderService.h"
#include "renegade/bridge/ConstraintService.h"
#include "renegade/bridge/PhysicsService.h"
#include "renegade/bridge/PhysicsDiagnosticsService.h"
#include "renegade/bridge/RagdollPhysicsService.h"
#include "renegade/bridge/SoftBodyPhysicsService.h"
#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/VehiclePhysicsService.h"
#include "renegade/bridge/VehicleSteeringService.h"
#include "renegade/bridge/WickedColliderService.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using renegade::studio::RenegadeButton;
    using renegade::studio::SceneInspectorCheckBox;
    using renegade::studio::SceneInspectorComboBox;
    using renegade::studio::SceneInspectorSlider;

    constexpr float HeaderHeight = 62.0f;
    constexpr float TabsHeight = 38.0f;
    constexpr float ReadoutHeight = 52.0f;
    constexpr float StatusHeight = 30.0f;
    constexpr float RowHeight = 34.0f;
    constexpr float RowGap = 8.0f;
    constexpr float ScrollStep = 54.0f;

    constexpr wi::Color Surface0 = wi::Color(7, 11, 14, 255);
    constexpr wi::Color Surface1 = wi::Color(11, 17, 21, 255);
    constexpr wi::Color Surface2 = wi::Color(16, 23, 28, 255);
    constexpr wi::Color Border = wi::Color(38, 52, 61, 255);
    constexpr wi::Color BorderSoft = wi::Color(25, 36, 43, 255);
    constexpr wi::Color TextStrong = wi::Color(244, 244, 244, 255);
    constexpr wi::Color TextSecondary = wi::Color(214, 222, 226, 255);
    constexpr wi::Color Muted = wi::Color(139, 151, 158, 255);
    constexpr wi::Color Forge = wi::Color(210, 91, 29, 255);
    constexpr wi::Color Success = wi::Color(76, 195, 138, 255);
    constexpr wi::Color Warning = wi::Color(229, 165, 82, 255);
    constexpr wi::Color Error = wi::Color(229, 92, 92, 255);

    template <typename Enum>
    constexpr std::uint64_t Userdata(const Enum value) noexcept
    {
        return static_cast<std::uint64_t>(value);
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
        DrawRect(x + 1.0f, y + 1.0f, width - 2.0f, height - 2.0f, fill, cmd);
    }

    void DrawText(
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
            x,
            y,
            size,
            wi::font::WIFALIGN_LEFT,
            wi::font::WIFALIGN_TOP,
            color,
            wi::Color::Transparent());
        params.spacingX = spacing;
        params.bolden = bolden;
        wi::font::Draw(value, params, cmd);
    }

    bool NearlyEqual(const XMFLOAT3& left, const XMFLOAT3& right) noexcept
    {
        constexpr float Epsilon = 0.00001f;
        return std::abs(left.x - right.x) <= Epsilon &&
            std::abs(left.y - right.y) <= Epsilon &&
            std::abs(left.z - right.z) <= Epsilon;
    }

    class SetPlayerVehicleModeCommand final : public renegade::bridge::ICommand
    {
    public:
        SetPlayerVehicleModeCommand(wi::scene::Scene& scene, wi::ecs::Entity entity,
            renegade::bridge::PlayerVehicleMode mode)
            : scene_(scene), entity_(entity),
              before_(renegade::bridge::GetPlayerVehicleMode(scene, entity)), after_(mode)
        {
        }

        bool Execute() override
        {
            return renegade::bridge::SetPlayerVehicleMode(scene_, entity_, after_);
        }

        void Undo() override
        {
            renegade::bridge::SetPlayerVehicleMode(scene_, entity_, before_);
        }

    private:
        wi::scene::Scene& scene_;
        wi::ecs::Entity entity_;
        renegade::bridge::PlayerVehicleMode before_;
        renegade::bridge::PlayerVehicleMode after_;
    };

    class SetAircraftTuningCommand final : public renegade::bridge::ICommand
    {
    public:
        SetAircraftTuningCommand(wi::scene::Scene& scene, wi::ecs::Entity entity,
            const renegade::bridge::FixedWingTuningField& field, float value)
            : scene_(scene), entity_(entity), field_(field),
              before_(renegade::bridge::ReadFixedWingTuningValue(scene, entity, field)),
              after_(renegade::bridge::SanitizeFixedWingTuningValue(field, value)) {}

        bool Execute() override
        {
            return before_.value_or(renegade::bridge::DefaultFixedWingTuning.*field_.member) != after_ &&
                renegade::bridge::WriteFixedWingTuningValue(scene_, entity_, field_, after_);
        }

        void Undo() override
        {
            renegade::bridge::WriteFixedWingTuningValue(scene_, entity_, field_, before_);
        }

    private:
        wi::scene::Scene& scene_;
        wi::ecs::Entity entity_;
        renegade::bridge::FixedWingTuningField field_;
        std::optional<float> before_;
        float after_;
    };

    class SetTrackedTuningCommand final : public renegade::bridge::ICommand
    {
    public:
        SetTrackedTuningCommand(wi::scene::Scene& scene, wi::ecs::Entity entity,
            const renegade::bridge::TrackedTuningField& field, float value)
            : scene_(scene), entity_(entity), field_(field),
              before_(renegade::bridge::ReadTrackedTuningValue(scene, entity, field)),
              after_(renegade::bridge::SanitizeTrackedTuningValue(field, value)) {}

        bool Execute() override
        {
            return before_.value_or(renegade::bridge::DefaultTrackedVehicleTuning.*field_.member) != after_ &&
                renegade::bridge::WriteTrackedTuningValue(scene_, entity_, field_, after_);
        }

        void Undo() override
        {
            renegade::bridge::WriteTrackedTuningValue(scene_, entity_, field_, before_);
        }

    private:
        wi::scene::Scene& scene_;
        wi::ecs::Entity entity_;
        renegade::bridge::TrackedTuningField field_;
        std::optional<float> before_;
        float after_;
    };

    class SetAircraftBindingCommand final : public renegade::bridge::ICommand
    {
    public:
        SetAircraftBindingCommand(wi::scene::Scene& scene, wi::ecs::Entity chassis,
            size_t part, std::vector<wi::ecs::Entity> before, wi::ecs::Entity after)
            : scene_(scene), chassis_(chassis), part_(part), before_(std::move(before)), after_(after) {}

        bool Execute() override
        {
            if (!renegade::bridge::IsFixedWing(scene_, chassis_) ||
                !scene_.rigidbodies.Contains(chassis_))
                return false;
            XMMATRIX local;
            if (after_ != wi::ecs::INVALID_ENTITY &&
                (after_ == chassis_ || !renegade::bridge::FixedWingPartTransform(scene_, after_, chassis_, local)))
                return false;
            if (before_.empty() ? after_ == wi::ecs::INVALID_ENTITY
                                : before_.size() == 1 && before_.front() == after_)
                return false;
            for (const auto entity : before_)
                renegade::bridge::SetFixedWingPartBinding(scene_, entity, part_, false);
            if (after_ != wi::ecs::INVALID_ENTITY)
                renegade::bridge::SetFixedWingPartBinding(scene_, after_, part_, true);
            return true;
        }

        void Undo() override
        {
            if (after_ != wi::ecs::INVALID_ENTITY)
                renegade::bridge::SetFixedWingPartBinding(scene_, after_, part_, false);
            for (const auto entity : before_)
                renegade::bridge::SetFixedWingPartBinding(scene_, entity, part_, true);
        }

    private:
        wi::scene::Scene& scene_;
        wi::ecs::Entity chassis_;
        size_t part_;
        std::vector<wi::ecs::Entity> before_;
        wi::ecs::Entity after_;
    };

    class SetPhysicsGravityCommand final : public renegade::bridge::ICommand
    {
    public:
        SetPhysicsGravityCommand(
            wi::scene::Scene& scene,
            const XMFLOAT3& before,
            const XMFLOAT3& after)
            : scene_(&scene)
            , before_(renegade::bridge::SanitizePhysicsGravity(before))
            , after_(renegade::bridge::SanitizePhysicsGravity(after))
        {
        }

        bool Execute() override
        {
            if (scene_ == nullptr || NearlyEqual(before_, after_))
                return false;
            renegade::bridge::SetPhysicsGravity(*scene_, after_);
            return true;
        }

        void Undo() override
        {
            if (scene_ != nullptr)
                renegade::bridge::SetPhysicsGravity(*scene_, before_);
        }

    private:
        wi::scene::Scene* scene_ = nullptr;
        XMFLOAT3 before_ = {};
        XMFLOAT3 after_ = {};
    };

    const char* CollisionTargetStatusText(
        const renegade::bridge::CollisionTargetStatus status) noexcept
    {
        using Status = renegade::bridge::CollisionTargetStatus;
        switch (status)
        {
        case Status::Supported: return "SUPPORTED";
        case Status::InvalidEntity: return "INVALID ENTITY";
        case Status::MissingTransform: return "REQUIRES A TRANSFORM";
        case Status::MissingMesh: return "REQUIRES A MESH";
        case Status::InvalidMeshData: return "MESH DATA IS NOT VALID FOR THIS SHAPE";
        case Status::InvalidHeightFieldGrid: return "HEIGHT FIELD REQUIRES A VALID GRID";
        default: return "UNSUPPORTED TARGET";
        }
    }
}

namespace renegade::studio
{
    struct RenegadePhysicsLabWorkspace::Impl
    {
        using Page = RenegadePhysicsLabWorkspace::Page;

        bool created = false;
        bool active = false;
        bool pointerConsumed = false;
        std::uint64_t commandRevision = 0;
        bool refreshPending = false;
        XMFLOAT4 bounds = {};
        bridge::StudioSession* session = nullptr;
        wi::ecs::Entity selectedEntity = wi::ecs::INVALID_ENTITY;
        Page page = Page::World;
        float scrollY = 0.0f;
        wi::gui::Widget contentClip;
        float pageContentHeight = 0.0f;
        float compactCursor = 0.f;
        struct Caption { const char* text; float x; float y; };
        std::vector<Caption> captions;
        std::string status;
        bool statusError = false;
        std::vector<wi::gui::Widget*> allControls;
        std::vector<wi::gui::Widget*> activeControls;

        SceneInspectorCheckBox worldEnabled;
        SceneInspectorCheckBox worldSimulation;
        SceneInspectorCheckBox worldInterpolation;
        SceneInspectorCheckBox worldDebug;
        SceneInspectorSlider worldGravityX;
        SceneInspectorSlider worldGravityY;
        SceneInspectorSlider worldGravityZ;
        SceneInspectorSlider worldAccuracy;
        SceneInspectorSlider worldFrameRate;
        SceneInspectorSlider worldConstraintDebugSize;
        SceneInspectorSlider worldDebugDistance;
        SceneInspectorSlider worldCharacterTolerance;
        bridge::PhysicsDiagnostics worldDiagnostics;
        RenegadeButton worldStep;
        RenegadeButton worldActivateAll;
        RenegadeButton worldOptimize;
        RenegadeButton worldReset;

        RenegadeButton rigidAdd;
        SceneInspectorComboBox compoundShape;
        RenegadeButton compoundAdd;
        RenegadeButton rigidRemove;
        SceneInspectorComboBox rigidShape;
        SceneInspectorSlider rigidBoxX;
        SceneInspectorSlider rigidBoxY;
        SceneInspectorSlider rigidBoxZ;
        SceneInspectorSlider rigidSphereRadius;
        SceneInspectorSlider rigidCapsuleHeight;
        SceneInspectorSlider rigidCapsuleRadius;
        SceneInspectorSlider rigidMass;
        SceneInspectorSlider rigidFriction;
        SceneInspectorSlider rigidRestitution;
        SceneInspectorSlider rigidLinearDamping;
        SceneInspectorSlider rigidAngularDamping;
        SceneInspectorSlider rigidBuoyancy;
        SceneInspectorSlider rigidMeshLod;
        SceneInspectorSlider rigidOffsetX;
        SceneInspectorSlider rigidOffsetY;
        SceneInspectorSlider rigidOffsetZ;
        SceneInspectorCheckBox rigidSensor;
        SceneInspectorSlider rigidCollisionLayer;
        SceneInspectorSlider rigidBreakSpeed, breakHitPoints, breakBlastSpeed;
        SceneInspectorSlider breakDebrisStay, breakDebrisFade;
        RenegadeButton breakUseIntactMesh;
        std::array<SceneInspectorCheckBox, bridge::BodyCollisionLayerCount> rigidCollisionMask;
        SceneInspectorCheckBox rigidKinematic;
        SceneInspectorCheckBox rigid2D;
        SceneInspectorCheckBox rigidDisableDeactivation;
        SceneInspectorCheckBox rigidStartDeactivated;

        RenegadeButton constraintAdd;
        RenegadeButton constraintRemove;
        RenegadeButton constraintRebind;
        SceneInspectorComboBox constraintType;
        SceneInspectorComboBox constraintBodyA;
        SceneInspectorComboBox constraintBodyB;
        SceneInspectorCheckBox constraintNoSelfCollision;
        SceneInspectorSlider constraintBreak;
        RenegadeButton constraintDisableBreaking;
        SceneInspectorSlider distanceMin;
        SceneInspectorSlider distanceMax;
        SceneInspectorSlider hingeMin;
        SceneInspectorSlider hingeMax;
        SceneInspectorSlider hingeVelocity;
        SceneInspectorSlider coneHalfAngle;
        SceneInspectorSlider swingNormalCone;
        SceneInspectorSlider swingPlaneCone;
        SceneInspectorSlider swingMinTwist;
        SceneInspectorSlider swingMaxTwist;
        SceneInspectorSlider sliderMin;
        SceneInspectorSlider sliderMax;
        SceneInspectorSlider sliderVelocity;
        SceneInspectorSlider sliderMaxForce;
        std::array<SceneInspectorSlider, 12> sixDof;
        std::array<RenegadeButton, 6> sixDofFix;

        SceneInspectorCheckBox characterEnabled;
        SceneInspectorSlider characterSlope;
        SceneInspectorSlider characterGravity;

        SceneInspectorCheckBox vehiclePhysicsPreview;
        RenegadeButton fixedWingAdvanced;
        RenegadeButton fixedWingBindings;
        std::array<SceneInspectorSlider, std::size(bridge::FixedWingTuningFields)> fixedWingTuning;
        std::array<SceneInspectorComboBox, bridge::FixedWingParts.size()> fixedWingParts;
        RenegadeButton trackedAdvanced;
        std::array<SceneInspectorSlider, std::size(bridge::TrackedTuningFields)> trackedTuning;
        bool showTrackedAdvanced = false;
        bool showFixedWingAdvanced = false;
        bool showFixedWingBindings = false;
        bool showVehiclePhysics = false;
        SceneInspectorComboBox playerVehicleMode;
        SceneInspectorComboBox steeringWheel;
        std::array<SceneInspectorSlider, 3> steeringWheelAxis;
        SceneInspectorSlider steeringWheelTravel;
        SceneInspectorComboBox vehicleType;
        SceneInspectorComboBox vehicleCollision;
        SceneInspectorSlider vehicleChassisWidth;
        SceneInspectorSlider vehicleChassisHeight;
        SceneInspectorSlider vehicleChassisLength;
        SceneInspectorSlider vehicleFrontOffset;
        SceneInspectorSlider vehicleRearOffset;
        SceneInspectorSlider vehicleWheelRadius;
        SceneInspectorSlider vehicleWheelWidth;
        SceneInspectorSlider vehicleEngineTorque;
        SceneInspectorSlider vehicleClutch;
        SceneInspectorSlider vehicleRoll;
        SceneInspectorSlider vehicleSteering;
        SceneInspectorSlider vehicleFrontSuspMin;
        SceneInspectorSlider vehicleFrontSuspMax;
        SceneInspectorSlider vehicleFrontSuspFrequency;
        SceneInspectorSlider vehicleFrontSuspDamping;
        SceneInspectorSlider vehicleRearSuspMin;
        SceneInspectorSlider vehicleRearSuspMax;
        SceneInspectorSlider vehicleRearSuspFrequency;
        SceneInspectorSlider vehicleRearSuspDamping;
        SceneInspectorCheckBox vehicle4WD;
        SceneInspectorSlider vehicleMotoAngle;
        SceneInspectorSlider vehicleMotoFrontBrake;
        SceneInspectorSlider vehicleMotoRearBrake;
        SceneInspectorCheckBox vehicleMotoLean;
        std::array<SceneInspectorComboBox, 4> vehicleWheels;
        SceneInspectorSlider vehicleTestThrottle;
        SceneInspectorSlider vehicleTestSteer;
        SceneInspectorSlider vehicleTestBrake;
        SceneInspectorSlider vehicleTestHandbrake;
        RenegadeButton vehicleSendTestInput;
        RenegadeButton vehicleUpdateWheels;

        SceneInspectorCheckBox ragdollDisabled;
        SceneInspectorCheckBox ragdollPhysics;
        SceneInspectorCheckBox ragdoll2D;
        SceneInspectorSlider ragdollFatness;
        SceneInspectorSlider ragdollHeadSize;
        RenegadeButton ragdollActivateAll;

        RenegadeButton softAdd;
        RenegadeButton softRemove;
        RenegadeButton softPinTop;
        RenegadeButton softClearPins;
        RenegadeButton softReset;
        RenegadeButton softWake;
        SceneInspectorSlider softDetail;
        SceneInspectorSlider softMass;
        SceneInspectorSlider softFriction;
        SceneInspectorSlider softRestitution;
        SceneInspectorSlider softPressure;
        SceneInspectorSlider softVertexRadius;
        SceneInspectorCheckBox softWind;

        RenegadeButton colliderAdd;
        RenegadeButton colliderRemove;
        SceneInspectorComboBox colliderShape;
        SceneInspectorCheckBox colliderCPU;
        SceneInspectorCheckBox colliderGPU;
        SceneInspectorSlider colliderRadius;
        SceneInspectorSlider colliderOffsetX;
        SceneInspectorSlider colliderOffsetY;
        SceneInspectorSlider colliderOffsetZ;
        SceneInspectorSlider colliderTailX;
        SceneInspectorSlider colliderTailY;
        SceneInspectorSlider colliderTailZ;

        [[nodiscard]] wi::scene::Scene* Scene() const noexcept
        {
            return session != nullptr ? &session->Scenes().GetScene() : nullptr;
        }

        [[nodiscard]] bool HasSelection() const noexcept
        {
            return session != nullptr && selectedEntity != wi::ecs::INVALID_ENTITY;
        }

        [[nodiscard]] std::string EntityLabel(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity entity) const
        {
            if (entity == wi::ecs::INVALID_ENTITY)
                return "<NONE>";
            const auto* name = scene.names.GetComponent(entity);
            const std::string prefix = name != nullptr && !name->name.empty()
                ? name->name : "ENTITY";
            return prefix + " [" +
                std::to_string(static_cast<unsigned long long>(entity)) + "]";
        }

        [[nodiscard]] wi::ecs::Entity BreakableRoot() const
        {
            const auto* scene = Scene();
            if (!scene || !HasSelection()) return wi::ecs::INVALID_ENTITY;
            auto entity = selectedEntity;
            for (size_t depth = 0; entity != wi::ecs::INVALID_ENTITY && depth <= scene->hierarchy.GetCount(); ++depth)
            {
                if (const auto* body = scene->rigidbodies.GetComponent(entity))
                    return bridge::SupportsDestructible(*body) && !bridge::IsBodySensor(*scene, entity)
                        ? entity : wi::ecs::INVALID_ENTITY;
                const auto* parent = scene->hierarchy.GetComponent(entity);
                entity = parent ? parent->parentID : wi::ecs::INVALID_ENTITY;
            }
            return wi::ecs::INVALID_ENTITY;
        }

        [[nodiscard]] std::string SelectedEntityLabel() const
        {
            const auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
                return "SELECTION // NONE";
            if (page == Page::Breakables)
                return "BREAKABLE ROOT // " + EntityLabel(*scene, BreakableRoot());
            return "SELECTION // " + EntityLabel(*scene, selectedEntity);
        }

        void SetStatus(std::string value, const bool error = false)
        {
            status = std::move(value);
            statusError = error;
        }

        void RequestRefresh() noexcept
        {
            refreshPending = true;
        }

        template <typename Command, typename... Args>
        bool ExecuteCommand(Args&&... args)
        {
            if (session == nullptr)
                return false;
            const bool changed = session->Commands().Execute(
                std::make_unique<Command>(std::forward<Args>(args)...));
            if (changed)
                RequestRefresh();
            return changed;
        }

        template <typename Callback>
        void MakeSlider(
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

        void MakeButton(
            RenegadeButton& button,
            const char* name,
            const char* text,
            std::function<void()> callback)
        {
            button.Create(name);
            button.SetText(text);
            button.SetRenderTextSize(10);
            button.OnClick([callback = std::move(callback)](const wi::gui::EventArgs&)
            {
                callback();
            });
        }

        template <typename Fn>
        void EditCollision(Fn&& edit)
        {
            auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
                return;
            const auto* body = scene->rigidbodies.GetComponent(selectedEntity);
            if (body == nullptr)
            {
                SetStatus("RIGID BODY // ADD A BODY BEFORE EDITING", true);
                return;
            }
            const auto before = bridge::CaptureCollision(*body);
            auto after = before;
            edit(after);
            after = bridge::SanitizeCollisionState(after);
            const auto target = bridge::CheckCollisionTarget(*scene, selectedEntity, after.shape);
            if (target != bridge::CollisionTargetStatus::Supported)
            {
                SetStatus(std::string("RIGID BODY // ") + CollisionTargetStatusText(target), true);
                RequestRefresh();
                return;
            }
            if (ExecuteCommand<bridge::SetCollisionCommand>(*scene, selectedEntity, before, after))
                SetStatus("RIGID BODY // APPLIED");
        }

        template <typename Fn>
        void EditConstraint(Fn&& edit)
        {
            auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
                return;
            const auto* component = scene->constraints.GetComponent(selectedEntity);
            if (component == nullptr)
            {
                SetStatus("CONSTRAINT // ADD A CONSTRAINT BEFORE EDITING", true);
                return;
            }
            const auto before = bridge::CaptureConstraint(*component);
            auto after = before;
            edit(after);
            if (ExecuteCommand<bridge::SetConstraintCommand>(*scene, selectedEntity, before, after))
                SetStatus("CONSTRAINT // APPLIED");
        }

        template <typename Fn>
        void EditCharacter(Fn&& edit)
        {
            auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
                return;
            const auto* body = scene->rigidbodies.GetComponent(selectedEntity);
            if (body == nullptr)
            {
                SetStatus("CHARACTER // REQUIRES A RIGID BODY", true);
                return;
            }
            const auto before = bridge::CaptureCharacterPhysics(*body);
            auto after = before;
            edit(after);
            if (ExecuteCommand<bridge::SetCharacterPhysicsCommand>(*scene, selectedEntity, before, after))
                SetStatus("CHARACTER PHYSICS // APPLIED");
        }

        template <typename Fn>
        void EditVehicle(Fn&& edit)
        {
            auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
                return;
            const auto* body = scene->rigidbodies.GetComponent(selectedEntity);
            if (body == nullptr)
            {
                SetStatus("VEHICLE // REQUIRES A RIGID BODY", true);
                return;
            }
            const auto before = bridge::CaptureVehiclePhysics(*body);
            auto after = before;
            edit(after);
            if (ExecuteCommand<bridge::SetVehiclePhysicsCommand>(*scene, selectedEntity, before, after))
                SetStatus("VEHICLE PHYSICS // APPLIED");
        }

        template <typename Fn>
        void EditRagdoll(Fn&& edit)
        {
            auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
                return;
            const auto* humanoid = scene->humanoids.GetComponent(selectedEntity);
            if (humanoid == nullptr)
            {
                SetStatus("RAGDOLL // SELECT A HUMANOID ENTITY", true);
                return;
            }
            const auto before = bridge::CaptureRagdollPhysics(*humanoid);
            auto after = before;
            edit(after);
            if (ExecuteCommand<bridge::SetRagdollPhysicsCommand>(*scene, selectedEntity, before, after))
                SetStatus("RAGDOLL PHYSICS // APPLIED");
        }

        template <typename Fn>
        void EditSoftBody(Fn&& edit)
        {
            auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
                return;
            const auto mesh = bridge::ResolveSoftBodyMeshEntity(*scene, selectedEntity);
            const auto* body = scene->softbodies.GetComponent(mesh);
            if (body == nullptr)
            {
                SetStatus("SOFT BODY // ADD SOFT BODY TO THE SELECTED MESH", true);
                return;
            }
            const auto before = bridge::CaptureSoftBodyPhysics(*body);
            auto after = before;
            edit(after);
            if (ExecuteCommand<bridge::SetSoftBodyPhysicsCommand>(*scene, selectedEntity, before, after))
                SetStatus("SOFT BODY // APPLIED");
        }

        template <typename Fn>
        void EditCollider(Fn&& edit)
        {
            auto* scene = Scene();
            if (scene == nullptr || !HasSelection())
                return;
            const auto* collider = scene->colliders.GetComponent(selectedEntity);
            if (collider == nullptr)
            {
                SetStatus("COLLIDER // ADD A COLLIDER BEFORE EDITING", true);
                return;
            }
            const auto before = bridge::CaptureWickedCollider(*collider);
            auto after = before;
            edit(after);
            if (ExecuteCommand<bridge::SetWickedColliderCommand>(*scene, selectedEntity, before, after))
                SetStatus("COLLIDER // APPLIED");
        }

        void Create()
        {
            if (created)
                return;
            created = true;
            CreateWorld();
            CreateRigidBody();
            CreateConstraint();
            CreateCharacter();
            CreateVehicle();
            CreateRagdoll();
            CreateSoftBody();
            CreateCollider();
            RegisterAllControls();
            session = bridge::StudioSession::Current();
            if (session != nullptr && session->Selection().HasSelection())
                selectedEntity = session->Selection().SelectedEntity();
            RefreshNow();
        }

        void CreateWorld()
        {
            worldEnabled.Create("PHYSICS ENGINE ENABLED");
            worldEnabled.OnClick([this](const wi::gui::EventArgs& args)
            {
                auto state = bridge::CapturePhysicsWorldState();
                state.enabled = args.bValue;
                bridge::ApplyPhysicsWorldState(state);
                SetStatus(args.bValue ? "WORLD // PHYSICS ENGINE ENABLED" : "WORLD // PHYSICS ENGINE DISABLED");
                RequestRefresh();
            });
            worldSimulation.Create("SIMULATION ENABLED");
            worldSimulation.OnClick([this](const wi::gui::EventArgs& args)
            {
                auto state = bridge::CapturePhysicsWorldState();
                state.simulationEnabled = args.bValue;
                bridge::ApplyPhysicsWorldState(state);
                SetStatus(args.bValue ? "WORLD // SIMULATION RUNNING" : "WORLD // SIMULATION PAUSED");
                RequestRefresh();
            });
            worldInterpolation.Create("INTERPOLATION");
            worldInterpolation.OnClick([this](const wi::gui::EventArgs& args)
            {
                auto state = bridge::CapturePhysicsWorldState();
                state.interpolationEnabled = args.bValue;
                bridge::ApplyPhysicsWorldState(state);
                RequestRefresh();
            });
            worldDebug.Create("PHYSICS VISUALIZER");
            worldDebug.OnClick([this](const wi::gui::EventArgs& args)
            {
                auto state = bridge::CapturePhysicsWorldState();
                state.debugDrawEnabled = args.bValue;
                bridge::ApplyPhysicsWorldState(state);
                SetStatus(args.bValue ? "WORLD // PHYSICS VISUALIZER ON" : "WORLD // PHYSICS VISUALIZER OFF");
                RequestRefresh();
            });

            MakeSlider(worldGravityX, -100.0f, 100.0f, 0.0f, 20000, "Physics gravity X", "GRAVITY X", [this](const float value) { CommitGravity(0, value); });
            MakeSlider(worldGravityY, -100.0f, 100.0f, -10.0f, 20000, "Physics gravity Y", "GRAVITY Y", [this](const float value) { CommitGravity(1, value); });
            MakeSlider(worldGravityZ, -100.0f, 100.0f, 0.0f, 20000, "Physics gravity Z", "GRAVITY Z", [this](const float value) { CommitGravity(2, value); });
            MakeSlider(worldAccuracy, 1.0f, 16.0f, 4.0f, 15, "Physics accuracy", "SOLVER ACCURACY", [this](const float value) { CommitWorldNumber(0, value); });
            MakeSlider(worldFrameRate, 1.0f, 1000.0f, 60.0f, 999, "Physics frame rate", "PHYSICS FRAME RATE", [this](const float value) { CommitWorldNumber(1, value); });
            MakeSlider(worldConstraintDebugSize, 0.0f, 10.0f, 1.0f, 1000, "Constraint debug size", "CONSTRAINT DEBUG SIZE", [this](const float value) { CommitWorldNumber(2, value); });
            MakeSlider(worldDebugDistance, 0.0f, 2000.0f, 500.0f, 2000, "Physics debug distance", "PHYSICS DRAW DISTANCE", [this](const float value) { CommitWorldNumber(3, value); });
            MakeSlider(worldCharacterTolerance, 0.0f, 1.0f, 0.05f, 10000, "Character collision tolerance", "CHARACTER COLLISION TOLERANCE", [this](const float value) { CommitWorldNumber(4, value); });

            MakeButton(worldStep, "Advance one native physics tick", "STEP ONCE", [this]()
            {
                auto* scene = Scene();
                const bool queued = scene && bridge::RequestPhysicsStep(*scene);
                SetStatus(queued ? "WORLD // ONE FIXED TICK QUEUED" : "WORLD // PAUSE SIMULATION FIRST", !queued);
            });

            MakeButton(worldActivateAll, "Physics activate all", "ACTIVATE ALL RIGID BODIES", [this]()
            {
                auto* scene = Scene();
                const bool ok = scene != nullptr && bridge::ActivateAllRigidBodies(*scene);
                SetStatus(ok ? "WORLD // ALL RIGID BODIES ACTIVATED" : "WORLD // LIVE PHYSICS SCENE NOT READY", !ok);
            });
            MakeButton(worldOptimize, "Physics optimize broadphase", "OPTIMIZE BROADPHASE", [this]()
            {
                auto* scene = Scene();
                const bool ok = scene != nullptr && bridge::OptimizePhysicsBroadPhase(*scene);
                SetStatus(ok ? "WORLD // BROADPHASE OPTIMIZED" : "WORLD // LIVE PHYSICS SCENE NOT READY", !ok);
            });
            MakeButton(worldReset, "Physics reset objects", "RESET PHYSICS OBJECTS", [this]()
            {
                auto* scene = Scene();
                const bool ok = scene != nullptr && bridge::ResetPhysicsObjects(*scene);
                SetStatus(ok ? "WORLD // PHYSICS OBJECTS RESET" : "WORLD // LIVE PHYSICS SCENE NOT READY", !ok);
            });
        }

        wi::ecs::Entity CompoundRoot() const
        {
            const auto* scene = Scene();
            if (!scene || !HasSelection())
                return wi::ecs::INVALID_ENTITY;
            const auto parent = bridge::GetCompoundPartParent(*scene, selectedEntity);
            return parent == wi::ecs::INVALID_ENTITY ? selectedEntity : parent;
        }

        void CreateRigidBody()
        {
            MakeButton(rigidAdd, "Add rigid body", "ADD RIGID BODY", [this]()
            {
                auto* scene = Scene();
                if (scene == nullptr || !HasSelection())
                    return;
                bridge::CollisionState initial;
                const bool ok = ExecuteCommand<bridge::CreateCollisionCommand>(*scene, selectedEntity, initial);
                SetStatus(ok ? "RIGID BODY // ADDED" : "RIGID BODY // COULD NOT ADD COMPONENT", !ok);
            });
            MakeButton(rigidRemove, "Remove rigid body", "REMOVE RIGID BODY", [this]()
            {
                auto* scene = Scene();
                if (scene == nullptr || !HasSelection())
                    return;
                const bool ok = ExecuteCommand<bridge::RemoveCollisionCommand>(*scene, selectedEntity);
                SetStatus(ok ? "RIGID BODY // REMOVED" : "RIGID BODY // NOTHING TO REMOVE", !ok);
            });

            compoundShape.Create("Compound collider part shape");
            compoundShape.AddItem("BOX");
            compoundShape.AddItem("SPHERE");
            compoundShape.AddItem("CAPSULE");
            compoundShape.SetSelectedWithoutCallback(0);
            MakeButton(compoundAdd, "Add a collider part to this body", "ADD COLLIDER PART", [this]()
            {
                auto* scene = Scene();
                if (!scene || !HasSelection())
                    return;
                const auto shape = static_cast<bridge::CompoundPartShape>(compoundShape.GetSelected() + 1);
                auto command = std::make_unique<bridge::AddCompoundPartCommand>(*scene, CompoundRoot(), shape);
                const auto part = command->PartEntity();
                const bool ok = session->Commands().Execute(std::move(command));
                if (ok)
                {
                    session->Selection().Select(part);
                    RequestRefresh();
                }
                SetStatus(ok ? "COLLIDER PART // MOVE, ROTATE OR SCALE IN VIEWPORT" :
                    "COLLIDER PART // COULD NOT ADD PART (LIMIT 8)", !ok);
            });

            using Shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape;
            rigidShape.Create("Rigid body collision shape");
            rigidShape.AddItem("BOX", Userdata(Shape::BOX));
            rigidShape.AddItem("SPHERE", Userdata(Shape::SPHERE));
            rigidShape.AddItem("CAPSULE", Userdata(Shape::CAPSULE));
            rigidShape.AddItem("CYLINDER", Userdata(Shape::CYLINDER));
            rigidShape.AddItem("CONVEX HULL", Userdata(Shape::CONVEX_HULL));
            rigidShape.AddItem("TRIANGLE MESH", Userdata(Shape::TRIANGLE_MESH));
            rigidShape.AddItem("HEIGHT FIELD", Userdata(Shape::HEIGHTFIELD));
            rigidShape.OnSelect([this](const wi::gui::EventArgs& args)
            {
                EditCollision([&](bridge::CollisionState& state)
                {
                    state.shape = static_cast<Shape>(args.userdata);
                });
            });

            MakeSlider(rigidBoxX, 0.01f, 10.0f, 1.0f, 10000, "Box half extent X", "BOX HALF EXTENT X", [this](const float value) { EditCollision([&](auto& state) { state.boxHalfExtents.x = value; }); });
            MakeSlider(rigidBoxY, 0.01f, 10.0f, 1.0f, 10000, "Box half extent Y", "BOX HALF EXTENT Y", [this](const float value) { EditCollision([&](auto& state) { state.boxHalfExtents.y = value; }); });
            MakeSlider(rigidBoxZ, 0.01f, 10.0f, 1.0f, 10000, "Box half extent Z", "BOX HALF EXTENT Z", [this](const float value) { EditCollision([&](auto& state) { state.boxHalfExtents.z = value; }); });
            MakeSlider(rigidSphereRadius, 0.01f, 10.0f, 1.0f, 10000, "Sphere radius", "SPHERE RADIUS", [this](const float value) { EditCollision([&](auto& state) { state.sphereRadius = value; }); });
            MakeSlider(rigidCapsuleHeight, 0.01f, 10.0f, 1.0f, 10000, "Capsule height", "HEIGHT", [this](const float value) { EditCollision([&](auto& state) { state.capsuleHeight = value; }); });
            MakeSlider(rigidCapsuleRadius, 0.01f, 10.0f, 1.0f, 10000, "Capsule radius", "RADIUS", [this](const float value) { EditCollision([&](auto& state) { state.capsuleRadius = value; }); });
            MakeSlider(rigidMass, 0.0f, 10000.0f, 1.0f, 100000, "Rigid body mass", "MASS", [this](const float value) { EditCollision([&](auto& state) { state.mass = value; }); });
            MakeSlider(rigidFriction, 0.0f, 1.0f, 0.2f, 10000, "Rigid body friction", "FRICTION", [this](const float value) { EditCollision([&](auto& state) { state.friction = value; }); });
            MakeSlider(rigidRestitution, 0.0f, 1.0f, 0.1f, 10000, "Rigid body restitution", "RESTITUTION", [this](const float value) { EditCollision([&](auto& state) { state.restitution = value; }); });
            MakeSlider(rigidLinearDamping, 0.0f, 1.0f, 0.05f, 10000, "Rigid body linear damping", "LINEAR DAMPING", [this](const float value) { EditCollision([&](auto& state) { state.dampingLinear = value; }); });
            MakeSlider(rigidAngularDamping, 0.0f, 1.0f, 0.05f, 10000, "Rigid body angular damping", "ANGULAR DAMPING", [this](const float value) { EditCollision([&](auto& state) { state.dampingAngular = value; }); });
            MakeSlider(rigidBuoyancy, 0.0f, 2.0f, 1.2f, 2000, "Rigid body buoyancy", "BUOYANCY", [this](const float value) { EditCollision([&](auto& state) { state.buoyancy = value; }); });
            MakeSlider(rigidMeshLod, 0.0f, 6.0f, 0.0f, 6, "Rigid body mesh LOD", "USE MESH LOD", [this](const float value) { EditCollision([&](auto& state) { state.meshLod = static_cast<std::uint32_t>(std::clamp(std::lround(value), 0l, 6l)); }); });
            MakeSlider(rigidOffsetX, -10.0f, 10.0f, 0.0f, 20000, "Rigid local offset X", "LOCAL OFFSET X", [this](const float value) { EditCollision([&](auto& state) { state.localOffset.x = value; }); });
            MakeSlider(rigidOffsetY, -10.0f, 10.0f, 0.0f, 20000, "Rigid local offset Y", "LOCAL OFFSET Y", [this](const float value) { EditCollision([&](auto& state) { state.localOffset.y = value; }); });
            MakeSlider(rigidOffsetZ, -10.0f, 10.0f, 0.0f, 20000, "Rigid local offset Z", "LOCAL OFFSET Z", [this](const float value) { EditCollision([&](auto& state) { state.localOffset.z = value; }); });

            rigidSensor.Create("SENSOR");
            rigidSensor.OnClick([this](const wi::gui::EventArgs& args)
            {
                if (auto* scene = Scene(); scene && HasSelection())
                    ExecuteCommand<bridge::SetBodySensorCommand>(*scene, selectedEntity, args.bValue);
            });
            MakeSlider(rigidBreakSpeed, 0, 100, 0, 1000, "Destructible impact speed", "BREAK SPEED (M/S)",
                [this](float value)
                {
                    const auto root = BreakableRoot();
                    if (auto* scene = Scene(); scene && root != wi::ecs::INVALID_ENTITY)
                        ExecuteCommand<bridge::SetDestructibleImpactSpeedCommand>(*scene, root, value);
                });
            MakeSlider(breakHitPoints, 0, 10000, 0, 10000, "Breakable weapon hit points", "HIT POINTS",
                [this](float value)
                {
                    const auto root = BreakableRoot();
                    if (auto* scene = Scene(); scene && root != wi::ecs::INVALID_ENTITY)
                        ExecuteCommand<bridge::SetDestructibleHitPointsCommand>(*scene, root, value);
                });
            MakeSlider(breakBlastSpeed, 0, bridge::MaxDestructibleBlastSpeed, 0, 240,
                "Explosive debris launch speed. Zero disables the blast.", "BLAST SPEED (M/S)",
                [this](float value)
                {
                    const auto root = BreakableRoot();
                    if (auto* scene = Scene(); scene && root != wi::ecs::INVALID_ENTITY)
                        ExecuteCommand<bridge::SetDestructibleBlastSpeedCommand>(*scene, root, value);
                });
            MakeSlider(breakDebrisStay, 0, bridge::MaxDestructibleDebrisStay, 0, 1200,
                "Seconds after breaking before debris starts to fade. Zero keeps debris.", "DEBRIS STAY (S)",
                [this](float seconds)
                {
                    const auto root = BreakableRoot();
                    if (auto* scene = Scene(); scene && root != wi::ecs::INVALID_ENTITY)
                    {
                        auto timing = bridge::GetDestructibleDebrisTiming(*scene, root);
                        timing.staySeconds = seconds;
                        ExecuteCommand<bridge::SetDestructibleDebrisTimingCommand>(*scene, root, timing);
                    }
                });
            MakeSlider(breakDebrisFade, 0, bridge::MaxDestructibleDebrisFade, 1, 100,
                "Time to fade after Debris Stay. Zero removes debris immediately.", "DEBRIS FADE (S)",
                [this](float seconds)
                {
                    const auto root = BreakableRoot();
                    if (auto* scene = Scene(); scene && root != wi::ecs::INVALID_ENTITY)
                    {
                        auto timing = bridge::GetDestructibleDebrisTiming(*scene, root);
                        timing.fadeSeconds = seconds;
                        ExecuteCommand<bridge::SetDestructibleDebrisTimingCommand>(*scene, root, timing);
                    }
                });
            MakeButton(breakUseIntactMesh, "Set selected mesh as intact", "SET AS INTACT MESH", [this]()
            {
                const auto root = BreakableRoot();
                if (auto* scene = Scene(); scene && root != wi::ecs::INVALID_ENTITY)
                {
                    const bool changed = ExecuteCommand<bridge::SetDestructibleIntactMeshCommand>(*scene, root, selectedEntity);
                    SetStatus(changed ? "INTACT MESH SET. DEBRIS HIDDEN." :
                        "NEEDS AN INTACT MESH AND 2-16 PIECES", !changed);
                }
            });
            MakeSlider(rigidCollisionLayer, 0, bridge::BodyCollisionLayerCount - 1, 0,
                bridge::BodyCollisionLayerCount - 1, "Body simulation layer", "COLLISION LAYER",
                [this](float value)
                {
                    if (auto* scene = Scene(); scene && HasSelection())
                    {
                        auto filter = bridge::GetBodyCollisionFilter(*scene, selectedEntity);
                        filter.layer = static_cast<int>(std::lround(value));
                        ExecuteCommand<bridge::SetBodyCollisionFilterCommand>(*scene, selectedEntity, filter);
                    }
                });
            for (int i = 0; i < bridge::BodyCollisionLayerCount; ++i)
            {
                const auto label = "COLLIDE WITH " + std::to_string(i);
                rigidCollisionMask[i].Create(label);
                rigidCollisionMask[i].OnClick([this, i](const wi::gui::EventArgs& args)
                {
                    if (auto* scene = Scene(); scene && HasSelection())
                    {
                        auto filter = bridge::GetBodyCollisionFilter(*scene, selectedEntity);
                        const int bit = 1 << i;
                        filter.mask = args.bValue ? filter.mask | bit : filter.mask & ~bit;
                        ExecuteCommand<bridge::SetBodyCollisionFilterCommand>(*scene, selectedEntity, filter);
                    }
                });
            }
            rigidKinematic.Create("KINEMATIC");
            rigidKinematic.OnClick([this](const wi::gui::EventArgs& args) { EditCollision([&](auto& state) { state.kinematic = args.bValue; }); });
            rigid2D.Create("2D LOCK");
            rigid2D.OnClick([this](const wi::gui::EventArgs& args) { EditCollision([&](auto& state) { state.locked2D = args.bValue; }); });
            rigidDisableDeactivation.Create("DISABLE DEACTIVATION");
            rigidDisableDeactivation.OnClick([this](const wi::gui::EventArgs& args) { EditCollision([&](auto& state) { state.disableDeactivation = args.bValue; }); });
            rigidStartDeactivated.Create("START DEACTIVATED");
            rigidStartDeactivated.OnClick([this](const wi::gui::EventArgs& args) { EditCollision([&](auto& state) { state.startDeactivated = args.bValue; }); });
        }

        void CreateConstraint()
        {
            MakeButton(constraintAdd, "Add constraint", "ADD CONSTRAINT", [this]()
            {
                auto* scene = Scene();
                if (scene == nullptr || !HasSelection())
                    return;
                const bool ok = ExecuteCommand<bridge::CreateConstraintCommand>(*scene, selectedEntity, bridge::ConstraintState{});
                SetStatus(ok ? "CONSTRAINT // ADDED" : "CONSTRAINT // REQUIRES A TRANSFORM AND NO EXISTING CONSTRAINT", !ok);
            });
            MakeButton(constraintRemove, "Remove constraint", "REMOVE CONSTRAINT", [this]()
            {
                auto* scene = Scene();
                if (scene == nullptr || !HasSelection())
                    return;
                const bool ok = ExecuteCommand<bridge::RemoveConstraintCommand>(*scene, selectedEntity);
                SetStatus(ok ? "CONSTRAINT // REMOVED" : "CONSTRAINT // NOTHING TO REMOVE", !ok);
            });
            MakeButton(constraintRebind, "Rebind constraint", "REBIND CONSTRAINT", [this]()
            {
                auto* scene = Scene();
                const bool ok = scene != nullptr && HasSelection() && bridge::RebindConstraint(*scene, selectedEntity);
                SetStatus(ok ? "CONSTRAINT // BINDING RESET" : "CONSTRAINT // REBIND FAILED", !ok);
                RequestRefresh();
            });

            using Type = wi::scene::PhysicsConstraintComponent::Type;
            constraintType.Create("Constraint type");
            constraintType.AddItem("FIXED", Userdata(Type::Fixed));
            constraintType.AddItem("POINT", Userdata(Type::Point));
            constraintType.AddItem("DISTANCE", Userdata(Type::Distance));
            constraintType.AddItem("HINGE", Userdata(Type::Hinge));
            constraintType.AddItem("CONE", Userdata(Type::Cone));
            constraintType.AddItem("SIX DOF", Userdata(Type::SixDOF));
            constraintType.AddItem("SWING TWIST", Userdata(Type::SwingTwist));
            constraintType.AddItem("SLIDER", Userdata(Type::Slider));
            constraintType.OnSelect([this](const wi::gui::EventArgs& args) { EditConstraint([&](auto& state) { state.type = static_cast<Type>(args.userdata); }); });

            constraintBodyA.Create("Constraint Body A");
            constraintBodyA.SetMaxVisibleItemCount(5);
            constraintBodyB.Create("Constraint Body B");
            constraintBodyB.SetMaxVisibleItemCount(5);
            constraintBodyA.OnSelect([this](const wi::gui::EventArgs& args) { EditConstraint([&](auto& state) { state.bodyA = static_cast<wi::ecs::Entity>(args.userdata); }); });
            constraintBodyB.OnSelect([this](const wi::gui::EventArgs& args) { EditConstraint([&](auto& state) { state.bodyB = static_cast<wi::ecs::Entity>(args.userdata); }); });
            constraintNoSelfCollision.Create("DISABLE SELF COLLISION");
            constraintNoSelfCollision.OnClick([this](const wi::gui::EventArgs& args) { EditConstraint([&](auto& state) { state.disableSelfCollision = args.bValue; }); });
            MakeSlider(constraintBreak, 0.0f, 1000.0f, 10.0f, 10000, "Constraint break distance", "BREAK DISTANCE", [this](const float value) { EditConstraint([&](auto& state) { state.breakDistance = value; }); });
            MakeButton(constraintDisableBreaking, "Disable constraint breaking", "DISABLE BREAKING", [this]() { EditConstraint([](auto& state) { state.breakDistance = FLT_MAX; }); });

            MakeSlider(distanceMin, 0.0f, 100.0f, 0.0f, 10000, "Distance constraint minimum", "MIN DISTANCE", [this](const float value) { EditConstraint([&](auto& state) { state.distanceMin = value; }); });
            MakeSlider(distanceMax, 0.0f, 100.0f, 0.0f, 10000, "Distance constraint maximum", "MAX DISTANCE", [this](const float value) { EditConstraint([&](auto& state) { state.distanceMax = value; }); });
            MakeSlider(hingeMin, -180.0f, 180.0f, -180.0f, 3600, "Hinge minimum angle", "MIN ANGLE", [this](const float value) { EditConstraint([&](auto& state) { state.hingeMinAngle = wi::math::DegreesToRadians(value); }); });
            MakeSlider(hingeMax, -180.0f, 180.0f, 180.0f, 3600, "Hinge maximum angle", "MAX ANGLE", [this](const float value) { EditConstraint([&](auto& state) { state.hingeMaxAngle = wi::math::DegreesToRadians(value); }); });
            MakeSlider(hingeVelocity, -100.0f, 100.0f, 0.0f, 20000, "Hinge target angular velocity", "TARGET ANGULAR VELOCITY", [this](const float value) { EditConstraint([&](auto& state) { state.hingeTargetAngularVelocity = value; }); });
            MakeSlider(coneHalfAngle, 0.0f, 90.0f, 0.0f, 900, "Cone half angle", "HALF CONE ANGLE", [this](const float value) { EditConstraint([&](auto& state) { state.coneHalfAngle = wi::math::DegreesToRadians(value); }); });
            MakeSlider(swingNormalCone, 0.0f, 90.0f, 0.0f, 900, "Swing twist normal cone", "NORMAL HALF CONE", [this](const float value) { EditConstraint([&](auto& state) { state.swingTwistNormalHalfConeAngle = wi::math::DegreesToRadians(value); }); });
            MakeSlider(swingPlaneCone, 0.0f, 90.0f, 0.0f, 900, "Swing twist plane cone", "PLANE HALF CONE", [this](const float value) { EditConstraint([&](auto& state) { state.swingTwistPlaneHalfConeAngle = wi::math::DegreesToRadians(value); }); });
            MakeSlider(swingMinTwist, -180.0f, 180.0f, 0.0f, 3600, "Swing twist minimum", "MIN TWIST", [this](const float value) { EditConstraint([&](auto& state) { state.swingTwistMinAngle = wi::math::DegreesToRadians(value); }); });
            MakeSlider(swingMaxTwist, -180.0f, 180.0f, 0.0f, 3600, "Swing twist maximum", "MAX TWIST", [this](const float value) { EditConstraint([&](auto& state) { state.swingTwistMaxAngle = wi::math::DegreesToRadians(value); }); });
            MakeSlider(sliderMin, -100.0f, 100.0f, -10.0f, 20000, "Slider minimum", "MIN LIMIT", [this](const float value) { EditConstraint([&](auto& state) { state.sliderMinLimit = value; }); });
            MakeSlider(sliderMax, -100.0f, 100.0f, 10.0f, 20000, "Slider maximum", "MAX LIMIT", [this](const float value) { EditConstraint([&](auto& state) { state.sliderMaxLimit = value; }); });
            MakeSlider(sliderVelocity, -100.0f, 100.0f, 0.0f, 20000, "Slider target velocity", "TARGET VELOCITY", [this](const float value) { EditConstraint([&](auto& state) { state.sliderTargetVelocity = value; }); });
            MakeSlider(sliderMaxForce, 0.0f, 100000.0f, 0.0f, 100000, "Slider maximum force", "MAX FORCE", [this](const float value) { EditConstraint([&](auto& state) { state.sliderMaxForce = value; }); });

            constexpr std::array<const char*, 12> sixLabels = {
                "MIN TRANSLATION X", "MAX TRANSLATION X", "MIN TRANSLATION Y", "MAX TRANSLATION Y",
                "MIN TRANSLATION Z", "MAX TRANSLATION Z", "MIN ROTATION X", "MAX ROTATION X",
                "MIN ROTATION Y", "MAX ROTATION Y", "MIN ROTATION Z", "MAX ROTATION Z"};
            for (std::size_t i = 0; i < sixDof.size(); ++i)
            {
                const bool rotation = i >= 6;
                MakeSlider(sixDof[i], rotation ? -180.0f : -100.0f, rotation ? 180.0f : 100.0f, 0.0f,
                    rotation ? 3600.0f : 20000.0f, sixLabels[i], sixLabels[i], [this, i](const float value) { CommitSixDof(i, value); });
            }
            constexpr std::array<const char*, 6> fixLabels = {"FIX X", "FIX Y", "FIX Z", "FIX ROT X", "FIX ROT Y", "FIX ROT Z"};
            for (std::size_t i = 0; i < sixDofFix.size(); ++i)
                MakeButton(sixDofFix[i], fixLabels[i], fixLabels[i], [this, i]() { FixSixDof(i); });
        }

        void CreateCharacter()
        {
            characterEnabled.Create("CHARACTER PHYSICS");
            characterEnabled.OnClick([this](const wi::gui::EventArgs& args) { EditCharacter([&](auto& state) { state.enabled = args.bValue; }); });
            MakeSlider(characterSlope, 0.0f, 90.0f, 50.0f, 900, "Character max slope", "MAX SLOPE ANGLE", [this](const float value) { EditCharacter([&](auto& state) { state.maxSlopeAngle = wi::math::DegreesToRadians(value); }); });
            MakeSlider(characterGravity, 0.0f, 4.0f, 1.0f, 400, "Character gravity factor", "GRAVITY FACTOR", [this](const float value) { EditCharacter([&](auto& state) { state.gravityFactor = value; }); });
        }

        void CreateVehicle()
        {
            using Type = wi::scene::RigidBodyPhysicsComponent::Vehicle::Type;
            using Mode = wi::scene::RigidBodyPhysicsComponent::Vehicle::CollisionMode;
            using PlayerMode = bridge::PlayerVehicleMode;
            MakeButton(trackedAdvanced, "Tracked drivetrain", "ADVANCED", [this]()
            {
                showTrackedAdvanced = !showTrackedAdvanced;
                if (!showTrackedAdvanced) scrollY = 0.f;
                RequestRefresh();
            });
            for (size_t i = 0; i < trackedTuning.size(); ++i)
            {
                const auto field = bridge::TrackedTuningFields[i];
                MakeSlider(trackedTuning[i], field.minimum, field.maximum,
                    bridge::DefaultTrackedVehicleTuning.*field.member, 10000, field.label, field.label,
                    [this, field](float value)
                    {
                        if (TrackedSelected() && ExecuteCommand<SetTrackedTuningCommand>(
                            *Scene(), selectedEntity, field, value))
                            SetStatus("TRACKED TUNING APPLIED");
                    });
            }
            MakeButton(fixedWingAdvanced, "Fixed wing advanced", "ADVANCED", [this]()
            {
                showFixedWingAdvanced = !showFixedWingAdvanced;
                if (!showFixedWingAdvanced) scrollY = 0.f;
                RequestRefresh();
            });
            MakeButton(fixedWingBindings, "Fixed wing part bindings", "PART BINDINGS", [this]()
            {
                showFixedWingBindings = !showFixedWingBindings;
                scrollY = 0.f;
                RequestRefresh();
            });
            for (size_t i = 0; i < fixedWingTuning.size(); ++i)
            {
                const auto field = bridge::FixedWingTuningFields[i];
                MakeSlider(fixedWingTuning[i], field.minimum, field.maximum,
                    bridge::DefaultFixedWingTuning.*field.member, 10000, field.label, field.label,
                    [this, field](float value)
                    {
                        if (FixedWingSelected() && ExecuteCommand<SetAircraftTuningCommand>(
                            *Scene(), selectedEntity, field, value))
                            SetStatus("AIRCRAFT TUNING APPLIED");
                    });
            }
            for (size_t i = 0; i < fixedWingParts.size(); ++i)
            {
                auto& combo = fixedWingParts[i];
                combo.Create(bridge::FixedWingParts[i].label);
                combo.SetMaxVisibleItemCount(10);
                combo.OnSelect([this, i](const wi::gui::EventArgs& args)
                {
                    if (!FixedWingSelected()) return;
                    auto& scene = *Scene();
                    const auto rig = bridge::ReadFixedWingRig(scene, selectedEntity);
                    if (ExecuteCommand<SetAircraftBindingCommand>(scene, selectedEntity, i,
                        rig.assigned[i], static_cast<wi::ecs::Entity>(args.userdata)))
                        SetStatus("AIRCRAFT PART APPLIED");
                });
            }
            vehiclePhysicsPreview.Create("SHOW VEHICLE PHYSICS");
            vehiclePhysicsPreview.OnClick([this](const wi::gui::EventArgs& args) { showVehiclePhysics = args.bValue; });
            steeringWheel.Create("Cockpit steering wheel");
            steeringWheel.SetMaxVisibleItemCount(12);
            steeringWheel.OnSelect([this](const wi::gui::EventArgs& args) {
                EditSteeringWheel([&](auto& settings) { settings.entity = static_cast<wi::ecs::Entity>(args.userdata); });
            });
            constexpr const char* axes[] = {"AXIS X", "AXIS Y", "AXIS Z"};
            constexpr float XMFLOAT3::*components[] = {&XMFLOAT3::x, &XMFLOAT3::y, &XMFLOAT3::z};
            for (size_t i = 0; i < steeringWheelAxis.size(); ++i)
                MakeSlider(steeringWheelAxis[i], -1.0f, 1.0f, i == 2 ? 1.0f : 0.0f, 2000,
                    axes[i], axes[i], [this, component = components[i]](float value) {
                        EditSteeringWheel([&](auto& settings) { settings.axis.*component = value; });
                    });
            MakeSlider(steeringWheelTravel, 0.0f, 1080.0f, 180.0f, 1080,
                "Steering wheel degrees from centre to lock", "TURN TO LOCK (DEG)", [this](float value) {
                    EditSteeringWheel([&](auto& settings) { settings.degrees = value; });
                });
            playerVehicleMode.Create("Player controls");
            playerVehicleMode.AddItem("DISABLED", Userdata(PlayerMode::Disabled));
            playerVehicleMode.AddItem("GROUND", Userdata(PlayerMode::Ground));
            playerVehicleMode.AddItem("VTOL", Userdata(PlayerMode::VTOL));
            playerVehicleMode.AddItem("SCRIPTED VTOL", Userdata(PlayerMode::ScriptedVTOL));
            playerVehicleMode.AddItem("FIXED WING", Userdata(PlayerMode::FixedWing));
            playerVehicleMode.AddItem("PHYSICAL VTOL", Userdata(PlayerMode::PhysicalVTOL));
            playerVehicleMode.AddItem("TRACKED", Userdata(PlayerMode::Tracked));
            playerVehicleMode.AddItem("HELICOPTER", Userdata(PlayerMode::Helicopter));
            playerVehicleMode.AddItem("SPACECRAFT", Userdata(PlayerMode::Spacecraft));
            playerVehicleMode.OnSelect([this](const wi::gui::EventArgs& args) {
                auto* scene = Scene();
                auto* body = scene && HasSelection() ? scene->rigidbodies.GetComponent(selectedEntity) : nullptr;
                if (!body)
                    return;
                const auto mode = static_cast<PlayerMode>(args.userdata);
                const bool dynamic = !body->IsKinematic() && body->mass > 0.f;
                const bool flight = mode == PlayerMode::VTOL || mode == PlayerMode::ScriptedVTOL ||
                    mode == PlayerMode::FixedWing || mode == PlayerMode::PhysicalVTOL ||
                    mode == PlayerMode::Helicopter || mode == PlayerMode::Spacecraft;
                if ((mode == PlayerMode::Ground && (!dynamic || body->vehicle.type == Type::None)) ||
                    ((flight || mode == PlayerMode::Tracked) &&
                     (!dynamic || body->IsCharacterPhysics() || body->vehicle.type != Type::None)))
                {
                    SetStatus(mode == PlayerMode::Ground
                        ? "GROUND NEEDS A DYNAMIC CAR OR MOTORCYCLE"
                        : "SELECT A DYNAMIC BODY WITH VEHICLE TYPE NONE");
                    playerVehicleMode.SetSelectedByUserdataWithoutCallback(
                        Userdata(bridge::GetPlayerVehicleMode(*scene, selectedEntity)));
                    return;
                }
                if (ExecuteCommand<SetPlayerVehicleModeCommand>(*scene, selectedEntity, mode))
                    SetStatus("PLAYER CONTROLS APPLIED");
            });
            vehicleType.Create("Vehicle physics type");
            vehicleType.AddItem("NONE", Userdata(Type::None));
            vehicleType.AddItem("CAR", Userdata(Type::Car));
            vehicleType.AddItem("MOTORCYCLE", Userdata(Type::Motorcycle));
            vehicleType.OnSelect([this](const wi::gui::EventArgs& args) { EditVehicle([&](auto& state) { state.type = static_cast<Type>(args.userdata); }); });
            vehicleCollision.Create("Vehicle collision mode");
            vehicleCollision.AddItem("RAY", Userdata(Mode::Ray));
            vehicleCollision.AddItem("SPHERE", Userdata(Mode::Sphere));
            vehicleCollision.AddItem("CYLINDER", Userdata(Mode::Cylinder));
            vehicleCollision.OnSelect([this](const wi::gui::EventArgs& args) { EditVehicle([&](auto& state) { state.collisionMode = static_cast<Mode>(args.userdata); }); });

            MakeSlider(vehicleChassisWidth, 0.0f, 10.0f, 0.9f, 10000, "Vehicle chassis half width", "CHASSIS HALF WIDTH", [this](const float value) { EditVehicle([&](auto& state) { state.chassisHalfWidth = value; }); });
            MakeSlider(vehicleChassisHeight, -10.0f, 10.0f, 0.2f, 20000, "Vehicle chassis half height", "CHASSIS HALF HEIGHT", [this](const float value) { EditVehicle([&](auto& state) { state.chassisHalfHeight = value; }); });
            MakeSlider(vehicleChassisLength, 0.0f, 10.0f, 2.0f, 10000, "Vehicle chassis half length", "CHASSIS HALF LENGTH", [this](const float value) { EditVehicle([&](auto& state) { state.chassisHalfLength = value; }); });
            MakeSlider(vehicleFrontOffset, -10.0f, 10.0f, 0.0f, 20000, "Vehicle front wheel offset", "FRONT WHEEL OFFSET", [this](const float value) { EditVehicle([&](auto& state) { state.frontWheelOffset = value; }); });
            MakeSlider(vehicleRearOffset, -10.0f, 10.0f, 0.0f, 20000, "Vehicle rear wheel offset", "REAR WHEEL OFFSET", [this](const float value) { EditVehicle([&](auto& state) { state.rearWheelOffset = value; }); });
            MakeSlider(vehicleWheelRadius, 0.001f, 10.0f, 0.3f, 10000, "Vehicle wheel radius", "WHEEL RADIUS", [this](const float value) { EditVehicle([&](auto& state) { state.wheelRadius = value; }); });
            MakeSlider(vehicleWheelWidth, 0.001f, 10.0f, 0.1f, 10000, "Vehicle wheel width", "WHEEL WIDTH", [this](const float value) { EditVehicle([&](auto& state) { state.wheelWidth = value; }); });
            MakeSlider(vehicleEngineTorque, 0.0f, 1000.0f, 500.0f, 10000, "Vehicle engine torque", "MAX ENGINE TORQUE", [this](const float value) { EditVehicle([&](auto& state) { state.maxEngineTorque = value; }); });
            MakeSlider(vehicleClutch, 0.0f, 100.0f, 10.0f, 10000, "Vehicle clutch strength", "CLUTCH STRENGTH", [this](const float value) { EditVehicle([&](auto& state) { state.clutchStrength = value; }); });
            MakeSlider(vehicleRoll, 0.0f, 180.0f, 60.0f, 1800, "Vehicle max roll", "MAX ROLL ANGLE", [this](const float value) { EditVehicle([&](auto& state) { state.maxRollAngle = wi::math::DegreesToRadians(value); }); });
            MakeSlider(vehicleSteering, 0.0f, 90.0f, 30.0f, 900, "Vehicle max steering", "MAX STEERING ANGLE", [this](const float value) { EditVehicle([&](auto& state) { state.maxSteeringAngle = wi::math::DegreesToRadians(value); }); });
            MakeSlider(vehicleFrontSuspMin, 0.0f, 2.0f, 0.3f, 2000, "Front suspension minimum", "FRONT SUSPENSION MIN", [this](const float value) { EditVehicle([&](auto& state) { state.frontSuspensionMinLength = value; }); });
            MakeSlider(vehicleFrontSuspMax, 0.0f, 2.0f, 0.5f, 2000, "Front suspension maximum", "FRONT SUSPENSION MAX", [this](const float value) { EditVehicle([&](auto& state) { state.frontSuspensionMaxLength = value; }); });
            MakeSlider(vehicleFrontSuspFrequency, 0.0f, 2.0f, 1.5f, 2000, "Front suspension frequency", "FRONT SUSPENSION FREQUENCY", [this](const float value) { EditVehicle([&](auto& state) { state.frontSuspensionFrequency = value; }); });
            MakeSlider(vehicleFrontSuspDamping, 0.0f, 2.0f, 0.5f, 2000, "Front suspension damping", "FRONT SUSPENSION DAMPING", [this](const float value) { EditVehicle([&](auto& state) { state.frontSuspensionDamping = value; }); });
            MakeSlider(vehicleRearSuspMin, 0.0f, 2.0f, 0.3f, 2000, "Rear suspension minimum", "REAR SUSPENSION MIN", [this](const float value) { EditVehicle([&](auto& state) { state.rearSuspensionMinLength = value; }); });
            MakeSlider(vehicleRearSuspMax, 0.0f, 2.0f, 0.5f, 2000, "Rear suspension maximum", "REAR SUSPENSION MAX", [this](const float value) { EditVehicle([&](auto& state) { state.rearSuspensionMaxLength = value; }); });
            MakeSlider(vehicleRearSuspFrequency, 0.0f, 2.0f, 1.5f, 2000, "Rear suspension frequency", "REAR SUSPENSION FREQUENCY", [this](const float value) { EditVehicle([&](auto& state) { state.rearSuspensionFrequency = value; }); });
            MakeSlider(vehicleRearSuspDamping, 0.0f, 2.0f, 0.5f, 2000, "Rear suspension damping", "REAR SUSPENSION DAMPING", [this](const float value) { EditVehicle([&](auto& state) { state.rearSuspensionDamping = value; }); });
            vehicle4WD.Create("FOUR WHEEL DRIVE");
            vehicle4WD.OnClick([this](const wi::gui::EventArgs& args) { EditVehicle([&](auto& state) { state.fourWheelDrive = args.bValue; }); });
            MakeSlider(vehicleMotoAngle, 0.0f, 90.0f, 30.0f, 900, "Motorcycle suspension angle", "FRONT SUSPENSION ANGLE", [this](const float value) { EditVehicle([&](auto& state) { state.motorcycleFrontSuspensionAngle = wi::math::DegreesToRadians(value); }); });
            MakeSlider(vehicleMotoFrontBrake, 0.0f, 2000.0f, 500.0f, 20000, "Motorcycle front brake torque", "FRONT BRAKE TORQUE", [this](const float value) { EditVehicle([&](auto& state) { state.motorcycleFrontBrakeTorque = value; }); });
            MakeSlider(vehicleMotoRearBrake, 0.0f, 2000.0f, 250.0f, 20000, "Motorcycle rear brake torque", "REAR BRAKE TORQUE", [this](const float value) { EditVehicle([&](auto& state) { state.motorcycleRearBrakeTorque = value; }); });
            vehicleMotoLean.Create("LEAN CONTROL");
            vehicleMotoLean.OnClick([this](const wi::gui::EventArgs& args) { EditVehicle([&](auto& state) { state.motorcycleLeanControl = args.bValue; }); });

            constexpr std::array<const char*, 4> wheelNames = {"FRONT LEFT WHEEL", "FRONT RIGHT WHEEL", "REAR LEFT WHEEL", "REAR RIGHT WHEEL"};
            for (std::size_t i = 0; i < vehicleWheels.size(); ++i)
            {
                vehicleWheels[i].Create(wheelNames[i]);
                vehicleWheels[i].SetMaxVisibleItemCount(12);
                vehicleWheels[i].OnSelect([this, i](const wi::gui::EventArgs& args)
                {
                    EditVehicle([&](auto& state)
                    {
                        const auto entity = static_cast<wi::ecs::Entity>(args.userdata);
                        if (i == 0) state.wheelEntityFrontLeft = entity;
                        else if (i == 1) state.wheelEntityFrontRight = entity;
                        else if (i == 2) state.wheelEntityRearLeft = entity;
                        else state.wheelEntityRearRight = entity;
                    });
                });
            }

            MakeSlider(vehicleTestThrottle, -1.0f, 1.0f, 0.0f, 200, "Vehicle test throttle", "TEST THROTTLE", [](float) {});
            MakeSlider(vehicleTestSteer, -1.0f, 1.0f, 0.0f, 200, "Vehicle test steer", "TEST STEER", [](float) {});
            MakeSlider(vehicleTestBrake, 0.0f, 1.0f, 0.0f, 100, "Vehicle test brake", "TEST BRAKE", [](float) {});
            MakeSlider(vehicleTestHandbrake, 0.0f, 1.0f, 0.0f, 100, "Vehicle test handbrake", "TEST HANDBRAKE", [](float) {});
            MakeButton(vehicleSendTestInput, "Vehicle send test input", "SEND TEST INPUT", [this]()
            {
                auto* scene = Scene();
                const bool ok = scene != nullptr && HasSelection() && bridge::DrivePhysicsVehicle(
                    *scene, selectedEntity, vehicleTestThrottle.GetValue(), vehicleTestSteer.GetValue(),
                    vehicleTestBrake.GetValue(), vehicleTestHandbrake.GetValue());
                SetStatus(ok ? "VEHICLE // TEST INPUT SENT" : "VEHICLE // LIVE VEHICLE NOT READY", !ok);
            });
            MakeButton(vehicleUpdateWheels, "Vehicle update wheel transforms", "UPDATE WHEEL TRANSFORMS", [this]()
            {
                auto* scene = Scene();
                const bool ok = scene != nullptr && bridge::UpdateVehicleWheelTransforms(*scene);
                SetStatus(ok ? "VEHICLE // WHEEL TRANSFORMS UPDATED" : "VEHICLE // LIVE SIMULATION NOT READY", !ok);
            });
        }

        void CreateRagdoll()
        {
            ragdollDisabled.Create("RAGDOLL DISABLED");
            ragdollDisabled.OnClick([this](const wi::gui::EventArgs& args) { EditRagdoll([&](auto& state) { state.disabled = args.bValue; }); });
            ragdollPhysics.Create("RAGDOLL PHYSICS ENABLED");
            ragdollPhysics.OnClick([this](const wi::gui::EventArgs& args) { EditRagdoll([&](auto& state) { state.physicsEnabled = args.bValue; }); });
            ragdoll2D.Create("2D LOCK");
            ragdoll2D.OnClick([this](const wi::gui::EventArgs& args) { EditRagdoll([&](auto& state) { state.locked2D = args.bValue; }); });
            MakeSlider(ragdollFatness, 0.5f, 2.0f, 1.0f, 1500, "Ragdoll fatness", "FATNESS", [this](const float value) { EditRagdoll([&](auto& state) { state.fatness = value; }); });
            MakeSlider(ragdollHeadSize, 0.5f, 2.0f, 1.0f, 1500, "Ragdoll head size", "HEAD SIZE", [this](const float value) { EditRagdoll([&](auto& state) { state.headSize = value; }); });
            MakeButton(ragdollActivateAll, "Activate all ragdolls", "ACTIVATE ALL RAGDOLLS", [this]()
            {
                auto* scene = Scene();
                if (scene == nullptr)
                    return;
                bridge::ActivateAllRagdolls(*scene);
                SetStatus("RAGDOLL // ALL HUMANOID RAGDOLLS ENABLED");
                RequestRefresh();
            });
        }

        void CreateSoftBody()
        {
            MakeButton(softAdd, "Add soft body", "ADD SOFT BODY", [this]()
            {
                auto* scene = Scene();
                if (scene == nullptr || !HasSelection())
                    return;
                const auto block = bridge::GetSoftBodyCreationBlock(*scene, selectedEntity);
                if (block == bridge::SoftBodyCreationBlock::RigidBody)
                {
                    SetStatus("SOFT BODY // OBJECT OR PARENT HAS RIGID PHYSICS", true);
                    return;
                }
                if (block == bridge::SoftBodyCreationBlock::SharedMesh)
                {
                    SetStatus("SOFT BODY // REQUIRES A UNIQUE MESH", true);
                    return;
                }
                const bool ok = ExecuteCommand<bridge::CreateSoftBodyPhysicsCommand>(*scene, selectedEntity, bridge::SoftBodyPhysicsState{});
                SetStatus(ok ? "SOFT BODY // ADDED" : "SOFT BODY // COULD NOT ADD TO SELECTION", !ok);
            });
            MakeButton(softRemove, "Remove soft body", "REMOVE SOFT BODY", [this]()
            {
                auto* scene = Scene();
                if (scene == nullptr || !HasSelection())
                    return;
                const bool ok = ExecuteCommand<bridge::RemoveSoftBodyPhysicsCommand>(*scene, selectedEntity);
                SetStatus(ok ? "SOFT BODY // REMOVED" : "SOFT BODY // NOTHING TO REMOVE", !ok);
            });
            MakeButton(softPinTop, "Pin the highest mesh vertices in local Y", "PIN TOP EDGE", [this]()
            {
                auto* scene = Scene();
                if (!scene || !HasSelection())
                    return;
                const auto entity = bridge::ResolveSoftBodyMeshEntity(*scene, selectedEntity);
                const auto* mesh = scene->meshes.GetComponent(entity);
                wi::vector<uint32_t> pins;
                if (!mesh || mesh->vertex_positions.empty() ||
                    !bridge::GetSoftBodyPinnedVertices(*scene, selectedEntity, pins))
                    return;

                const auto& vertices = mesh->vertex_positions;
                const auto [lowest, highest] = std::minmax_element(vertices.begin(), vertices.end(),
                    [](const XMFLOAT3& a, const XMFLOAT3& b) { return a.y < b.y; });
                const float height = highest->y - lowest->y;
                if (!(height > 0.0f) || !std::isfinite(height))
                {
                    SetStatus("SOFT BODY // MESH NEEDS A VERTICAL EDGE", true);
                    return;
                }
                const float threshold = highest->y - height * 0.0001f;
                for (uint32_t i = 0; i < vertices.size(); ++i)
                    if (vertices[i].y >= threshold)
                        pins.push_back(i);

                if (ExecuteCommand<bridge::SetSoftBodyPinsCommand>(*scene, selectedEntity, std::move(pins)))
                    SetStatus("SOFT BODY // TOP EDGE PINNED");
            });
            MakeButton(softClearPins, "Unpin every vertex on this mesh", "CLEAR PINS", [this]()
            {
                auto* scene = Scene();
                if (scene && HasSelection() &&
                    ExecuteCommand<bridge::SetSoftBodyPinsCommand>(*scene, selectedEntity, wi::vector<uint32_t>{}))
                    SetStatus("SOFT BODY // PINS CLEARED");
            });
            MakeButton(softReset, "Reset soft body", "RESET SOFT BODY", [this]()
            {
                auto* scene = Scene();
                const bool ok = scene != nullptr && HasSelection() && bridge::ResetSoftBodyPhysics(*scene, selectedEntity);
                SetStatus(ok ? "SOFT BODY // RESET" : "SOFT BODY // COMPONENT NOT AVAILABLE", !ok);
                RequestRefresh();
            });
            MakeButton(softWake, "Wake soft body", "WAKE SOFT BODY", [this]()
            {
                auto* scene = Scene();
                const bool ok = scene != nullptr && HasSelection() && bridge::SetSoftBodyActive(*scene, selectedEntity, true);
                SetStatus(ok ? "SOFT BODY // ACTIVATED" : "SOFT BODY // LIVE BODY NOT READY", !ok);
            });
            MakeSlider(softDetail, 0.001f, 1.0f, 1.0f, 1000, "Soft body detail", "DETAIL / LOD", [this](const float value) { EditSoftBody([&](auto& state) { state.detail = value; }); });
            MakeSlider(softMass, 0.0f, 100.0f, 1.0f, 10000, "Soft body mass", "MASS", [this](const float value) { EditSoftBody([&](auto& state) { state.mass = value; }); });
            MakeSlider(softFriction, 0.0f, 1.0f, 0.5f, 1000, "Soft body friction", "FRICTION", [this](const float value) { EditSoftBody([&](auto& state) { state.friction = value; }); });
            MakeSlider(softRestitution, 0.0f, 1.0f, 0.0f, 1000, "Soft body restitution", "RESTITUTION", [this](const float value) { EditSoftBody([&](auto& state) { state.restitution = value; }); });
            MakeSlider(softPressure, 0.0f, 100000.0f, 0.0f, 100000, "Soft body pressure", "PRESSURE", [this](const float value) { EditSoftBody([&](auto& state) { state.pressure = value; }); });
            MakeSlider(softVertexRadius, 0.0f, 1.0f, 0.2f, 1000, "Soft body vertex radius", "VERTEX RADIUS", [this](const float value) { EditSoftBody([&](auto& state) { state.vertexRadius = value; }); });
            softWind.Create("WIND ENABLED");
            softWind.OnClick([this](const wi::gui::EventArgs& args) { EditSoftBody([&](auto& state) { state.windEnabled = args.bValue; }); });
        }

        void CreateCollider()
        {
            MakeButton(colliderAdd, "Add collider", "ADD COLLIDER", [this]()
            {
                auto* scene = Scene();
                if (scene == nullptr || !HasSelection())
                    return;
                const bool ok = ExecuteCommand<bridge::CreateWickedColliderCommand>(*scene, selectedEntity, bridge::WickedColliderState{});
                SetStatus(ok ? "COLLIDER // ADDED" : "COLLIDER // COULD NOT ADD COMPONENT", !ok);
            });
            MakeButton(colliderRemove, "Remove collider", "REMOVE COLLIDER", [this]()
            {
                auto* scene = Scene();
                if (scene == nullptr || !HasSelection())
                    return;
                const bool ok = ExecuteCommand<bridge::RemoveWickedColliderCommand>(*scene, selectedEntity);
                SetStatus(ok ? "COLLIDER // REMOVED" : "COLLIDER // NOTHING TO REMOVE", !ok);
            });
            using Shape = wi::scene::ColliderComponent::Shape;
            colliderShape.Create("Collider shape");
            colliderShape.AddItem("SPHERE", Userdata(Shape::Sphere));
            colliderShape.AddItem("CAPSULE", Userdata(Shape::Capsule));
            colliderShape.AddItem("PLANE", Userdata(Shape::Plane));
            colliderShape.OnSelect([this](const wi::gui::EventArgs& args) { EditCollider([&](auto& state) { state.shape = static_cast<Shape>(args.userdata); }); });
            colliderCPU.Create("CPU COLLISION");
            colliderCPU.OnClick([this](const wi::gui::EventArgs& args) { EditCollider([&](auto& state) { state.cpuEnabled = args.bValue; }); });
            colliderGPU.Create("GPU COLLISION");
            colliderGPU.OnClick([this](const wi::gui::EventArgs& args) { EditCollider([&](auto& state) { state.gpuEnabled = args.bValue; }); });
            MakeSlider(colliderRadius, 0.0f, 10.0f, 1.0f, 10000, "Collider radius", "RADIUS", [this](const float value) { EditCollider([&](auto& state) { state.radius = value; }); });
            MakeSlider(colliderOffsetX, -10.0f, 10.0f, 0.0f, 20000, "Collider offset X", "OFFSET X", [this](const float value) { EditCollider([&](auto& state) { state.offset.x = value; }); });
            MakeSlider(colliderOffsetY, -10.0f, 10.0f, 0.0f, 20000, "Collider offset Y", "OFFSET Y", [this](const float value) { EditCollider([&](auto& state) { state.offset.y = value; }); });
            MakeSlider(colliderOffsetZ, -10.0f, 10.0f, 0.0f, 20000, "Collider offset Z", "OFFSET Z", [this](const float value) { EditCollider([&](auto& state) { state.offset.z = value; }); });
            MakeSlider(colliderTailX, -10.0f, 10.0f, 0.0f, 20000, "Collider tail X", "TAIL X", [this](const float value) { EditCollider([&](auto& state) { state.tail.x = value; }); });
            MakeSlider(colliderTailY, -10.0f, 10.0f, 1.0f, 20000, "Collider tail Y", "TAIL Y", [this](const float value) { EditCollider([&](auto& state) { state.tail.y = value; }); });
            MakeSlider(colliderTailZ, -10.0f, 10.0f, 0.0f, 20000, "Collider tail Z", "TAIL Z", [this](const float value) { EditCollider([&](auto& state) { state.tail.z = value; }); });
        }

        void RegisterAllControls()
        {
            auto add = [this](wi::gui::Widget& widget)
            {
                allControls.push_back(&widget);
            };

            add(worldEnabled); add(worldSimulation); add(worldInterpolation); add(worldDebug);
            add(worldGravityX); add(worldGravityY); add(worldGravityZ); add(worldAccuracy);
            add(worldFrameRate); add(worldConstraintDebugSize); add(worldDebugDistance);
            add(worldCharacterTolerance); add(worldStep); add(worldActivateAll); add(worldOptimize); add(worldReset);

            add(rigidAdd); add(rigidRemove); add(compoundShape); add(compoundAdd); add(rigidShape); add(rigidBoxX); add(rigidBoxY); add(rigidBoxZ);
            add(rigidSphereRadius); add(rigidCapsuleHeight); add(rigidCapsuleRadius); add(rigidMass);
            add(rigidFriction); add(rigidRestitution); add(rigidLinearDamping); add(rigidAngularDamping);
            add(rigidBuoyancy); add(rigidMeshLod); add(rigidOffsetX); add(rigidOffsetY); add(rigidOffsetZ);
            add(rigidSensor);
            add(rigidCollisionLayer);
            add(rigidBreakSpeed); add(breakHitPoints); add(breakBlastSpeed);
            add(breakDebrisStay); add(breakDebrisFade); add(breakUseIntactMesh);
            for (auto& check : rigidCollisionMask) add(check);
            add(rigidKinematic); add(rigid2D); add(rigidDisableDeactivation); add(rigidStartDeactivated);

            add(constraintAdd); add(constraintRemove); add(constraintRebind); add(constraintType);
            add(constraintBodyA); add(constraintBodyB); add(constraintNoSelfCollision); add(constraintBreak);
            add(constraintDisableBreaking); add(distanceMin); add(distanceMax); add(hingeMin); add(hingeMax);
            add(hingeVelocity); add(coneHalfAngle); add(swingNormalCone); add(swingPlaneCone);
            add(swingMinTwist); add(swingMaxTwist); add(sliderMin); add(sliderMax);
            add(sliderVelocity); add(sliderMaxForce);
            for (auto& widget : sixDof) add(widget);
            for (auto& widget : sixDofFix) add(widget);

            add(characterEnabled); add(characterSlope); add(characterGravity);

            add(steeringWheel); add(steeringWheelTravel);
            for (auto& axis : steeringWheelAxis) add(axis);
            add(fixedWingAdvanced); add(fixedWingBindings); add(trackedAdvanced);
            for (auto& control : trackedTuning) add(control);
            for (auto& control : fixedWingTuning) add(control);
            for (auto& control : fixedWingParts) add(control);
            add(vehiclePhysicsPreview); add(playerVehicleMode); add(vehicleType); add(vehicleCollision); add(vehicleChassisWidth); add(vehicleChassisHeight);
            add(vehicleChassisLength); add(vehicleFrontOffset); add(vehicleRearOffset); add(vehicleWheelRadius);
            add(vehicleWheelWidth); add(vehicleEngineTorque); add(vehicleClutch); add(vehicleRoll);
            add(vehicleSteering); add(vehicleFrontSuspMin); add(vehicleFrontSuspMax); add(vehicleFrontSuspFrequency);
            add(vehicleFrontSuspDamping); add(vehicleRearSuspMin); add(vehicleRearSuspMax);
            add(vehicleRearSuspFrequency); add(vehicleRearSuspDamping); add(vehicle4WD); add(vehicleMotoAngle);
            add(vehicleMotoFrontBrake); add(vehicleMotoRearBrake); add(vehicleMotoLean);
            for (auto& widget : vehicleWheels) add(widget);
            add(vehicleTestThrottle); add(vehicleTestSteer); add(vehicleTestBrake); add(vehicleTestHandbrake);
            add(vehicleSendTestInput); add(vehicleUpdateWheels);

            add(ragdollDisabled); add(ragdollPhysics); add(ragdoll2D); add(ragdollFatness);
            add(ragdollHeadSize); add(ragdollActivateAll);

            add(softPinTop); add(softClearPins);
            add(softAdd); add(softRemove); add(softReset); add(softWake); add(softDetail); add(softMass);
            add(softFriction); add(softRestitution); add(softPressure); add(softVertexRadius); add(softWind);

            add(colliderAdd); add(colliderRemove); add(colliderShape); add(colliderCPU); add(colliderGPU);
            add(colliderRadius); add(colliderOffsetX); add(colliderOffsetY); add(colliderOffsetZ);
            add(colliderTailX); add(colliderTailY); add(colliderTailZ);
            HideAll();
        }

        void HideAll()
        {
            for (auto* widget : allControls)
                if (widget != nullptr) widget->SetVisible(false);
        }

        [[nodiscard]] wi::ecs::Entity SelectionTarget() const
        {
            if (session == nullptr || !session->Selection().HasSelection())
                return wi::ecs::INVALID_ENTITY;
            const auto selected = session->Selection().SelectedEntity();
            const auto* scene = Scene();
            if (scene && page == Page::RigidBody && bridge::CompoundPartKind(*scene, selected))
                return selected;
            const auto aircraft = scene != nullptr
                ? bridge::ResolveAircraftChassis(*scene, selected) : wi::ecs::INVALID_ENTITY;
            return aircraft == wi::ecs::INVALID_ENTITY ? selected : aircraft;
        }

        [[nodiscard]] bool TrackedSelected() const
        {
            const auto* scene = Scene();
            return scene && bridge::IsTrackedVehicle(*scene, selectedEntity);
        }

        [[nodiscard]] bool ForceFlightSelected() const
        {
            const auto* scene = Scene();
            return scene && bridge::IsForceFlightVehicle(*scene, selectedEntity);
        }

        [[nodiscard]] bool FixedWingSelected() const
        {
            const auto* scene = Scene();
            return scene != nullptr && bridge::IsFixedWing(*scene, selectedEntity) &&
                scene->rigidbodies.Contains(selectedEntity);
        }

        void SetActive(const bool value)
        {
            if (active == value)
                return;
            active = value;
            scrollY = 0.0f;
            pointerConsumed = false;
            if (active)
            {
                session = bridge::StudioSession::Current();
                selectedEntity = SelectionTarget();
                RefreshNow();
            }
            else
            {
                HideAll();
                activeControls.clear();
            }
        }

        void SetBounds(const XMFLOAT4& value)
        {
            if (bounds.x == value.x && bounds.y == value.y && bounds.z == value.z && bounds.w == value.w)
                return;
            bounds = value;
            Layout();
        }

        [[nodiscard]] bool ContainsPointer(const XMFLOAT4& pointer) const noexcept
        {
            return active && pointer.x >= bounds.x && pointer.x < bounds.z &&
                pointer.y >= bounds.y && pointer.y < bounds.w;
        }

        void Update(const wi::Canvas& canvas, const float dt)
        {
            pointerConsumed = false;
            if (!active || !created)
                return;

            auto* currentSession = bridge::StudioSession::Current();
            if (currentSession != session)
            {
                session = currentSession;
                selectedEntity = wi::ecs::INVALID_ENTITY;
                RequestRefresh();
            }
            if (session != nullptr)
            {
                const auto revision = session->Commands().Revision();
                if (commandRevision != revision)
                {
                    commandRevision = revision;
                    RequestRefresh();
                }
                const auto currentSelection = SelectionTarget();
                if (currentSelection != selectedEntity)
                {
                    selectedEntity = currentSelection;
                    scrollY = 0.0f;
                    RequestRefresh();
                }
            }

            if (refreshPending)
                RefreshNow();

            const XMFLOAT4 pointer = wi::input::GetPointer();
            if (ContainsPointer(pointer))
            {
                pointerConsumed = true;
                const float tabsY = bounds.y + HeaderHeight;
                if (pointer.y >= tabsY && pointer.y < tabsY + TabBandHeight() &&
                    wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
                {
                    const float width = std::max(1.0f, bounds.z - bounds.x - 32.0f);
                    const int columns = TabColumns();
                    const float tabWidth=width/columns;
                    const int col=int((pointer.x-bounds.x-16)/tabWidth);
                    const int row=int((pointer.y-tabsY)/TabsHeight);
                    const int index=row*columns+col;
                    if (pointer.x>=bounds.x+16 && col>=0 && col<columns && index < static_cast<int>(Page::Count))
                    {
                        page = static_cast<Page>(index);
                        scrollY = 0.0f;
                        RequestRefresh();
                    }
                }
                const float contentTop = bounds.y + HeaderHeight + TabBandHeight() + ReadoutHeight;
                if (pointer.y >= contentTop && std::abs(pointer.z) > 0.1f)
                {
                    scrollY += pointer.z > 0.0f ? -ScrollStep : ScrollStep;
                    ClampScroll();
                    Layout();
                }
            }

            for (auto* widget : activeControls)
                if (widget != nullptr && widget->IsVisible()) widget->Update(canvas, dt);

            if (refreshPending)
                RefreshNow();
            if (page == Page::World)
            {
                auto* scene = Scene();
                worldDiagnostics = scene ? bridge::SamplePhysicsDiagnostics(*scene) : bridge::PhysicsDiagnostics{};
                const auto world = bridge::CapturePhysicsWorldState();
                worldStep.SetEnabled(worldDiagnostics.ready && world.enabled && !world.simulationEnabled);
            }
            DrawVehiclePhysicsPreview();
            DrawConstraintFrame();
            DrawCompoundCollider();
        }

        void DrawConstraintFrame() const
        {
            if (page != Page::Constraint || !HasSelection())
                return;
            const auto* scene = Scene();
            const auto* joint = scene ? scene->constraints.GetComponent(selectedEntity) : nullptr;
            const auto* transform = scene ? scene->transforms.GetComponent(selectedEntity) : nullptr;
            if (!joint || !transform)
                return;

            const auto pivot = transform->GetPositionV();
            const XMFLOAT3 rightValue = transform->GetRight();
            const XMFLOAT3 upValue = transform->GetUp();
            const auto right = XMVector3Normalize(XMLoadFloat3(&rightValue));
            const auto up = XMVector3Normalize(XMLoadFloat3(&upValue));
            const auto draw = [](FXMVECTOR start, FXMVECTOR end, const XMFLOAT4& color)
            {
                wi::renderer::RenderableLine segment;
                XMStoreFloat3(&segment.start, start);
                XMStoreFloat3(&segment.end, end);
                segment.color_start = segment.color_end = color;
                wi::renderer::DrawLine(segment);
            };

            constexpr XMFLOAT4 pivotColor{1.f, .58f, .16f, 1.f};
            constexpr XMFLOAT4 axisColor{.17f, .92f, 1.f, 1.f};
            constexpr XMFLOAT4 linkColor{.58f, .69f, .82f, 1.f};
            for (const auto axis : {XMVectorSet(1, 0, 0, 0), XMVectorSet(0, 1, 0, 0),
                                   XMVectorSet(0, 0, 1, 0)})
                draw(pivot - axis * .10f, pivot + axis * .10f, pivotColor);

            const auto drawAxis = [&](FXMVECTOR axis, FXMVECTOR side)
            {
                const auto tip = pivot + axis * .78f;
                draw(pivot - axis * .32f, tip, axisColor);
                draw(tip, tip - axis * .17f + side * .065f, axisColor);
                draw(tip, tip - axis * .17f - side * .065f, axisColor);
            };
            using Type = wi::scene::PhysicsConstraintComponent::Type;
            if (joint->type == Type::Hinge)
                drawAxis(up, right);
            else if (joint->type == Type::Slider || joint->type == Type::Cone || joint->type == Type::SwingTwist)
                drawAxis(right, up);
            else if (joint->type == Type::SixDOF)
            {
                drawAxis(right, up);
                drawAxis(up, right);
                drawAxis(XMVector3Normalize(XMVector3Cross(right, up)), up);
            }
            for (const auto body : {joint->bodyA, joint->bodyB})
            {
                if (const auto* bodyTransform = scene->transforms.GetComponent(body))
                    draw(pivot, bodyTransform->GetPositionV(), linkColor);
            }
        }

        void DrawCompoundCollider() const
        {
            if (page != Page::RigidBody || !HasSelection())
                return;
            const auto* scene = Scene();
            const auto root = CompoundRoot();
            const auto* body = scene->rigidbodies.GetComponent(root);
            const auto* transform = scene->transforms.GetComponent(root);
            if (!body || !transform || !bridge::SupportsCompoundCollider(*body))
                return;
            const auto parts = bridge::GetCompoundParts(*scene, root);
            if (!parts.valid)
                return;

            constexpr int segments = 32;
            for (size_t i = 0; i < parts.count; ++i)
            {
                const auto& part = parts.parts[i];
                const auto pose = XMMatrixRotationQuaternion(XMLoadFloat4(&part.orientation)) *
                    XMMatrixTranslationFromVector(XMLoadFloat3(&part.position)) * XMLoadFloat4x4(&transform->world);
                const auto& half = part.halfExtents;
                const XMFLOAT4 color = part.entity == selectedEntity
                    ? XMFLOAT4{1.f, .58f, .16f, 1.f} : XMFLOAT4{.17f, .92f, 1.f, 1.f};
                if (part.shape == bridge::CompoundPartShape::Box)
                {
                    wi::renderer::DrawBox(XMMatrixScaling(half.x, half.y, half.z) * pose, color, false);
                    continue;
                }

                const auto draw = [&](FXMVECTOR start, FXMVECTOR end)
                {
                    wi::renderer::RenderableLine line;
                    XMStoreFloat3(&line.start, XMVector3TransformCoord(start, pose));
                    XMStoreFloat3(&line.end, XMVector3TransformCoord(end, pose));
                    line.color_start = line.color_end = color;
                    wi::renderer::DrawLine(line);
                };
                const auto arc = [&](FXMVECTOR u, FXMVECTOR v, FXMVECTOR center, float angle, int steps)
                {
                    for (int n = 0; n < steps; ++n)
                    {
                        const float a = angle * static_cast<float>(n) / steps;
                        const float b = angle * static_cast<float>(n + 1) / steps;
                        draw(center + u * std::cos(a) + v * std::sin(a),
                            center + u * std::cos(b) + v * std::sin(b));
                    }
                };
                const auto x = XMVectorSet(half.x, 0.f, 0.f, 0.f);
                const auto y = XMVectorSet(0.f, half.x, 0.f, 0.f);
                const auto z = XMVectorSet(0.f, 0.f, half.x, 0.f);
                const auto zero = XMVectorZero();
                if (part.shape == bridge::CompoundPartShape::Sphere)
                {
                    arc(x, y, zero, XM_2PI, segments);
                    arc(x, z, zero, XM_2PI, segments);
                    arc(z, y, zero, XM_2PI, segments);
                }
                else
                {
                    const auto top = XMVectorSet(0.f, half.y, 0.f, 0.f);
                    const auto bottom = -top;
                    arc(x, z, top, XM_2PI, segments);
                    arc(x, z, bottom, XM_2PI, segments);
                    for (const auto side : {x, z})
                    {
                        arc(side, y, top, XM_PI, segments / 2);
                        arc(side, -y, bottom, XM_PI, segments / 2);
                        draw(top + side, bottom + side);
                        draw(top - side, bottom - side);
                    }
                }
            }
        }

        void DrawVehiclePhysicsPreview() const
        {
            if (page != Page::Vehicle || !showVehiclePhysics || !HasSelection())
                return;
            const auto* scene = Scene();
            const auto* body = scene ? scene->rigidbodies.GetComponent(selectedEntity) : nullptr;
            const auto* transform = scene ? scene->transforms.GetComponent(selectedEntity) : nullptr;
            if (!body || !transform || !body->IsVehicle())
                return;

            const auto vehicle = bridge::CaptureVehiclePhysics(*body);
            const auto collision = bridge::CaptureCollision(*body);
            const auto rotation = XMMatrixRotationQuaternion(transform->GetRotationV());
            const auto pose = rotation * XMMatrixTranslationFromVector(
                transform->GetPositionV() + XMLoadFloat3(&collision.localOffset));

            if (collision.shape == wi::scene::RigidBodyPhysicsComponent::CollisionShape::BOX)
            {
                const auto& half = collision.boxHalfExtents;
                const auto& scale = transform->scale_local;
                wi::renderer::DrawBox(
                    XMMatrixScaling(half.x * scale.x, half.y * scale.y, half.z * scale.z) * pose,
                    {1.0f, 0.47f, 0.12f, 1.0f}, false);
            }

            const auto value = [](const SceneInspectorSlider& slider, const float committed)
            {
                return slider.IsDragging() ? slider.GetValue() : committed;
            };
            const auto world = [&](XMVECTOR local)
            {
                XMFLOAT3 point;
                XMStoreFloat3(&point, XMVector3TransformCoord(local, pose));
                return point;
            };
            const auto line = [&](XMVECTOR a, XMVECTOR b, const XMFLOAT4& color)
            {
                wi::renderer::RenderableLine segment;
                segment.start = world(a);
                segment.end = world(b);
                segment.color_start = segment.color_end = color;
                wi::renderer::DrawLine(segment);
            };

            const bool motorcycle = body->IsMotorcycle();
            const float width = value(vehicleWheelWidth, vehicle.wheelWidth);
            const float radius = value(vehicleWheelRadius, vehicle.wheelRadius);
            const float halfWidth = motorcycle ? 0.0f : value(vehicleChassisWidth, vehicle.chassisHalfWidth);
            const float height = value(vehicleChassisHeight, vehicle.chassisHalfHeight);
            const float halfLength = value(vehicleChassisLength, vehicle.chassisHalfLength);
            const XMFLOAT4 wheelColor{0.10f, 0.88f, 1.0f, 1.0f};
            const XMFLOAT4 travelColor{1.0f, 0.92f, 0.26f, 1.0f};
            for (int wheel = 0; wheel < (motorcycle ? 2 : 4); ++wheel)
            {
                const bool front = motorcycle ? wheel == 0 : wheel < 2;
                const float z = front ? halfLength + value(vehicleFrontOffset, vehicle.frontWheelOffset)
                                      : -halfLength + value(vehicleRearOffset, vehicle.rearWheelOffset);
                const auto mount = XMVectorSet(wheel % 2 == 0 ? -halfWidth : halfWidth, height, z, 0.0f);
                const float caster = motorcycle && front
                    ? std::tan(wi::math::DegreesToRadians(value(vehicleMotoAngle,
                        wi::math::RadiansToDegrees(vehicle.motorcycleFrontSuspensionAngle)))) : 0.0f;
                const auto direction = XMVector3Normalize(XMVectorSet(0.0f, -1.0f, caster, 0.0f));
                const float minimum = front ? value(vehicleFrontSuspMin, vehicle.frontSuspensionMinLength)
                                            : value(vehicleRearSuspMin, vehicle.rearSuspensionMinLength);
                const float maximum = std::max(minimum, front
                    ? value(vehicleFrontSuspMax, vehicle.frontSuspensionMaxLength)
                    : value(vehicleRearSuspMax, vehicle.rearSuspensionMaxLength));

                const auto compressed = mount + direction * minimum;
                const auto extended = mount + direction * maximum;
                const auto axle = XMVectorSet(width * 0.5f, 0.0f, 0.0f, 0.0f);
                line(mount, compressed, wheelColor);
                line(compressed, extended, travelColor);
                line(compressed - axle, compressed + axle, travelColor);
                line(extended - axle, extended + axle, travelColor);

                using Mode = wi::scene::RigidBodyPhysicsComponent::Vehicle::CollisionMode;
                if (vehicle.collisionMode == Mode::Ray)
                    line(extended, extended + direction * radius, travelColor);
                else if (vehicle.collisionMode == Mode::Sphere)
                {
                    const float castRadius = 0.5f * (motorcycle ? radius : width);
                    const auto castEnd = mount + direction * (maximum + radius - castRadius);
                    wi::renderer::DrawSphere(wi::primitive::Sphere(world(castEnd), castRadius), travelColor, false);
                }

                // An authored wheel outline at full extension makes its fit visible in every contact mode.
                constexpr int segments = 20;
                const auto rim = [&](int index)
                {
                    const float angle = DirectX::XM_2PI * index / segments;
                    return extended + XMVectorSet(0.0f, std::cos(angle) * radius, std::sin(angle) * radius, 0.0f);
                };
                for (int i = 0; i < segments; ++i)
                {
                    const auto a = rim(i);
                    const auto b = rim(i + 1);
                    line(a - axle, b - axle, wheelColor);
                    line(a + axle, b + axle, wheelColor);
                    if (i % 5 == 0)
                        line(a - axle, a + axle, wheelColor);
                }
            }
        }

        void Render(const wi::Canvas& canvas, const wi::graphics::CommandList cmd) const
        {
            if (!active || !created)
                return;
            const float width = std::max(1.0f, bounds.z - bounds.x);
            const float height = std::max(1.0f, bounds.w - bounds.y);
            DrawRect(bounds.x, bounds.y, width, height, Surface0, cmd);
            DrawRect(bounds.x, bounds.y, width, 1.0f, Border, cmd);
            DrawRect(bounds.x, bounds.w - 1.0f, width, 1.0f, Border, cmd);
            DrawText("PHYSICS LAB", bounds.x + 22.0f, bounds.y + 14.0f, 17, TextStrong, cmd, 1.15f, 0.18f);
            const auto selection = SelectedEntityLabel();
            DrawText(selection, bounds.x + 22.0f, bounds.y + 40.0f, 9, Muted, cmd);
            RenderTabs(cmd);
            RenderReadout(cmd);

            const auto clipControls = [&] {
                const float top = bounds.y + HeaderHeight + TabBandHeight() + ReadoutHeight;
                contentClip.ApplyScissor(canvas,
                    {int32_t(bounds.x + 14), int32_t(top), int32_t(bounds.z - 14),
                     int32_t(std::max(top, bounds.w - StatusHeight - 5))}, cmd, false);
            };
            clipControls();
            for (const auto& caption : captions)
                DrawText(caption.text, caption.x + 1.f, caption.y, 9, TextSecondary, cmd);
            for (const auto* widget : activeControls)
            {
                if (widget == nullptr || !widget->IsVisible() ||
                    dynamic_cast<const wi::gui::ComboBox*>(widget) != nullptr)
                    continue;
                clipControls();
                widget->Render(canvas, cmd);
            }
            for (auto it = activeControls.rbegin(); it != activeControls.rend(); ++it)
            {
                const auto* widget = *it;
                if (widget != nullptr && widget->IsVisible() &&
                    dynamic_cast<const wi::gui::ComboBox*>(widget) != nullptr)
                {
                    clipControls();
                    widget->Render(canvas, cmd);
                }
            }

            contentClip.ApplyScissor(canvas,{int32_t(bounds.x),int32_t(bounds.y),int32_t(bounds.z),int32_t(bounds.w)},cmd,false);
            if (!status.empty())
            {
                DrawBorderedRect(bounds.x + 14.0f, bounds.w - StatusHeight,
                    width - 28.0f, StatusHeight - 5.0f, Surface1, BorderSoft, cmd);
                DrawText(status, bounds.x + 24.0f, bounds.w - StatusHeight + 6.0f,
                    9, statusError ? Error : Muted, cmd, 0.15f, 0.14f);
            }
        }

        bool Compact() const { return bounds.z - bounds.x < 600.f; }
        int TabColumns() const { return bounds.z-bounds.x<600.f?2:bounds.z-bounds.x<1000.f?4:8; }
        float TabBandHeight() const
        {
            const int columns = TabColumns();
            return TabsHeight * ((static_cast<int>(Page::Count) + columns - 1) / columns);
        }

        void RenderTabs(const wi::graphics::CommandList cmd) const
        {
            constexpr std::array<const char*, static_cast<size_t>(Page::Count)> labels = {
                "WORLD", "RIGID BODY", "CONSTRAINT", "CHARACTER",
                "VEHICLE", "RAGDOLL", "SOFT BODY", "COLLIDER", "BREAKABLES"};
            const float available = std::max(1.0f, bounds.z - bounds.x - 32.0f);
            const float tabWidth = available / TabColumns();
            const int textSize = 11;
            float x = bounds.x + 16.0f;
            float y = bounds.y + HeaderHeight;
            for (std::size_t i = 0; i < labels.size(); ++i)
            {
                x=bounds.x+16+float(i%TabColumns())*tabWidth;
                y=bounds.y+HeaderHeight+float(i/TabColumns())*TabsHeight;
                const bool selected = static_cast<std::size_t>(page) == i;
                DrawRect(x, y, tabWidth - 4.0f, TabsHeight - 2.0f,
                    selected ? Surface2 : Surface1, cmd);
                if (selected)
                    DrawRect(x, y + TabsHeight - 4.0f, tabWidth - 4.0f, 2.0f, Forge, cmd);
                DrawText(labels[i], x + 7.0f, y + 12.0f, textSize,
                    selected ? TextStrong : TextSecondary, cmd, 0.15f, selected ? 0.17f : 0.12f);
                x += tabWidth;
            }
        }

        void RenderReadout(const wi::graphics::CommandList cmd) const
        {
            const float top = bounds.y + HeaderHeight + TabBandHeight() + 7.0f;
            DrawRect(bounds.x + 14.0f, top, bounds.z - bounds.x - 28.0f,
                bounds.w - top - StatusHeight - 6.0f, Surface1, cmd);
            const float x = bounds.x + 30.0f;
            const float y = top + 11.0f;
            const char* title = "WORLD";
            switch (page)
            {
            case Page::RigidBody: title = "RIGID BODY"; break;
            case Page::Constraint: title = "CONSTRAINT"; break;
            case Page::Character: title = "CHARACTER"; break;
            case Page::Vehicle: title = FixedWingSelected() ? "FIXED WING" :
                Scene() && bridge::IsHelicopter(*Scene(), selectedEntity) ? "HELICOPTER" :
                Scene() && bridge::IsSpacecraft(*Scene(), selectedEntity) ? "SPACECRAFT" :
                ForceFlightSelected() ? "PHYSICAL VTOL" : TrackedSelected() ? "TRACKED" : "VEHICLE"; break;
            case Page::Ragdoll: title = "RAGDOLL"; break;
            case Page::SoftBody: title = "SOFT BODY"; break;
            case Page::WickedCollider: title = "SECONDARY COLLIDER"; break;
            case Page::Breakables: title = "BREAKABLES"; break;
            default: break;
            }
            DrawText(title, x, y, 12, TextStrong, cmd, 0.7f, 0.18f);
            if (page != Page::World && !Compact()) DrawText(PageHint(), x, y + 21.0f, 9, Muted, cmd);
            RenderLiveReadout(x, y + (Compact() || page == Page::World ? 21.0f : 38.0f), cmd);
        }

        [[nodiscard]] std::string PageHint() const
        {
            switch (page)
            {
            case Page::World: return "Scene gravity, simulation, solver and visualisation settings.";
            case Page::RigidBody: return "Rigid-body collision, material response, damping, buoyancy and body behaviour.";
            case Page::Constraint: return "Use Move for the pivot and Rotate for the axis.";
            case Page::Character: return "Character movement physics. Capsule collision is recommended; mass and friction come from the rigid body.";
            case Page::Vehicle:
                if (ForceFlightSelected()) return "Flight controls run in Test Game. Set the chassis shape in Rigid Body.";
                if (TrackedSelected()) return "Tracked suspension and drivetrain run in Test Game.";
                return FixedWingSelected()
                ? "Aircraft mass and damping. Flight controls run in Test Game."
                : "Car and motorcycle authoring, wheel mapping, suspension and live editor drive testing.";
            case Page::Ragdoll: return "Humanoid ragdoll behaviour, 2D locking, body proportions and activation.";
            case Page::SoftBody: return "Mesh-backed soft-body simulation with mass, friction, pressure, vertex radius and wind.";
            case Page::WickedCollider: return "Lightweight CPU/GPU collision for particles, hair and other secondary systems.";
            case Page::Breakables: return "Select the prop or its intact mesh. Debris Stay at zero keeps debris.";
            default: return {};
            }
        }

        void RenderLiveReadout(const float x, const float y, const wi::graphics::CommandList cmd) const
        {
            auto* scene = Scene();
            if (page == Page::World)
            {
                const auto& stats = worldDiagnostics;
                if (!stats.ready)
                {
                    DrawText("PHYSICS SCENE NOT READY", x, y, 9, Muted, cmd);
                    return;
                }
                char values[128];
                if (stats.lastUpdateSteps > 0)
                    std::snprintf(values, sizeof(values), "PHYSICS %.2f MS   TICKS %llu   CONTACTS %u",
                        stats.lastUpdateMilliseconds, static_cast<unsigned long long>(stats.totalFixedSteps),
                        stats.lastStepContactManifolds);
                else
                    std::snprintf(values, sizeof(values), "WAITING FOR A PHYSICS TICK");
                DrawText(values, x, y, 9, TextStrong, cmd);
                std::snprintf(values, sizeof(values), "AWAKE %u / %u BODIES   NATIVE CONSTRAINTS %u",
                    stats.awakeRigidBodies + stats.awakeSoftBodies, stats.totalBodies, stats.constraints);
                DrawText(values, x, y + 14.f, 9, Muted, cmd);
                return;
            }
            if (scene == nullptr || !HasSelection())
            {
                DrawText("SELECT AN ENTITY IN THE SCENE HIERARCHY", x, y, 9, Muted, cmd);
                return;
            }
            if (page == Page::RigidBody)
            {
                if (bridge::CompoundPartKind(*scene, selectedEntity))
                {
                    DrawText("COMPOUND COLLIDER PART", x, y, 9, Success, cmd);
                    return;
                }
                const auto* body = scene->rigidbodies.GetComponent(selectedEntity);
                DrawText(body == nullptr ? "NO RIGID BODY COMPONENT" :
                    (bridge::HasLivePhysicsBody(*scene, selectedEntity) ? "LIVE BODY // READY" : "AUTHORED BODY // PENDING"),
                    x, y, 9, body != nullptr && bridge::HasLivePhysicsBody(*scene, selectedEntity) ? Success : Muted, cmd);
            }
            else if (page == Page::Breakables)
            {
                const auto root = BreakableRoot();
                if (root == wi::ecs::INVALID_ENTITY)
                    DrawText("ADD A SOLID STATIC BODY TO THE PARENT", x, y, 9, Muted, cmd);
                else if (bridge::IsDestructibleIntactVisual(*scene, selectedEntity))
                    DrawText("SELECTED MESH IS THE INTACT VISUAL", x, y, 9, Success, cmd);
            }
            else if (page == Page::Constraint)
            {
                if (scene->constraints.Contains(selectedEntity))
                    DrawText("MOVE PIVOT  /  ROTATE AXIS", x, y, 9, Muted, cmd);
            }
            else if (page == Page::Character)
            {
                if (bridge::HasCharacterVehiclePhysicsConflict(*scene, selectedEntity))
                    DrawText("CONFLICT // CHARACTER AND VEHICLE MODES ARE BOTH ENABLED", x, y, 9, Error, cmd);
                else
                {
                    bridge::CharacterGroundInfo ground;
                    if (bridge::GetCharacterGroundInfo(*scene, selectedEntity, ground))
                    {
                        const char* state = ground.state == bridge::CharacterGroundState::OnGround ? "ON GROUND" :
                            ground.state == bridge::CharacterGroundState::OnSteepGround ? "STEEP GROUND" :
                            ground.state == bridge::CharacterGroundState::InAir ? "IN AIR" : "NOT SUPPORTED";
                        DrawText(std::string("LIVE GROUND STATE // ") + state, x, y, 9, Success, cmd);
                    }
                }
            }
            else if (page == Page::Vehicle)
            {
                if (FixedWingSelected())
                {
                    DrawText("FLY AND TEST THE GEAR IN TEST GAME", x, y, 9, Muted, cmd);
                    return;
                }
                float velocity = 0.0f;
                if (bridge::GetVehicleForwardVelocity(*scene, selectedEntity, velocity))
                    DrawText("LIVE FORWARD VELOCITY // " + std::to_string(velocity), x, y, 9, Success, cmd);
                else if (bridge::HasCharacterVehiclePhysicsConflict(*scene, selectedEntity))
                    DrawText("CONFLICT // DISABLE CHARACTER PHYSICS BEFORE DRIVING", x, y, 9, Error, cmd);
            }
            else if (page == Page::Ragdoll)
            {
                if (scene->humanoids.Contains(selectedEntity))
                    DrawText(bridge::HasLiveRagdollPhysics(*scene, selectedEntity) ? "LIVE RAGDOLL // READY" : "RAGDOLL AUTHORED // PENDING",
                        x, y, 9, bridge::HasLiveRagdollPhysics(*scene, selectedEntity) ? Success : Muted, cmd);
            }
            else if (page == Page::SoftBody)
            {
                const auto mesh = bridge::ResolveSoftBodyMeshEntity(*scene, selectedEntity);
                if (mesh != wi::ecs::INVALID_ENTITY)
                    DrawText("RESOLVED MESH // " + std::to_string(static_cast<unsigned long long>(mesh)) +
                        (bridge::HasLiveSoftBodyPhysics(*scene, selectedEntity) ? " // LIVE" : " // AUTHORED"),
                        x, y, 9, bridge::HasLiveSoftBodyPhysics(*scene, selectedEntity) ? Success : Muted, cmd);
            }
        }

        void RefreshNow()
        {
            refreshPending = false;
            if (!created)
                return;
            RefreshWorld();
            RefreshRigidBody();
            if (page == Page::Breakables)
                RefreshBreakables();
            RefreshConstraint();
            RefreshCharacter();
            RefreshVehicle();
            RefreshRagdoll();
            RefreshSoftBody();
            RefreshCollider();
            Layout();
        }

        void RefreshWorld()
        {
            const auto state = bridge::CapturePhysicsWorldState();
            worldEnabled.SetCheck(state.enabled);
            worldSimulation.SetCheck(state.simulationEnabled);
            worldInterpolation.SetCheck(state.interpolationEnabled);
            worldDebug.SetCheck(state.debugDrawEnabled);
            worldAccuracy.SetValue(static_cast<float>(state.accuracy));
            worldFrameRate.SetValue(state.frameRate);
            worldConstraintDebugSize.SetValue(state.constraintDebugSize);
            worldDebugDistance.SetValue(std::clamp(state.debugDrawMaxDistance, 0.0f, 2000.0f));
            worldCharacterTolerance.SetValue(std::clamp(state.characterCollisionTolerance, 0.0f, 1.0f));
            if (const auto* scene = Scene())
            {
                const auto gravity = bridge::GetPhysicsGravity(*scene);
                worldGravityX.SetValue(std::clamp(gravity.x, -100.0f, 100.0f));
                worldGravityY.SetValue(std::clamp(gravity.y, -100.0f, 100.0f));
                worldGravityZ.SetValue(std::clamp(gravity.z, -100.0f, 100.0f));
            }
        }

        void RefreshBreakables()
        {
            const auto* scene = Scene();
            const auto root = BreakableRoot();
            const bool enabled = scene && root != wi::ecs::INVALID_ENTITY;
            rigidBreakSpeed.SetEnabled(enabled);
            rigidBreakSpeed.SetValue(enabled ? bridge::GetDestructibleImpactSpeed(*scene, root) : 0.f);
            breakHitPoints.SetEnabled(enabled);
            breakHitPoints.SetValue(enabled ? bridge::GetDestructibleHitPoints(*scene, root) : 0.f);
            breakBlastSpeed.SetEnabled(enabled);
            breakBlastSpeed.SetValue(enabled ? bridge::GetDestructibleBlastSpeed(*scene, root) : 0.f);
            const auto timing = enabled ? bridge::GetDestructibleDebrisTiming(*scene, root) : bridge::DestructibleDebrisTiming{};
            breakDebrisStay.SetEnabled(enabled);
            breakDebrisStay.SetValue(timing.staySeconds);
            breakDebrisFade.SetEnabled(enabled && timing.staySeconds > 0.f);
            breakDebrisFade.SetValue(timing.fadeSeconds);
            breakUseIntactMesh.SetEnabled(enabled && selectedEntity != root && !scene->objects.Contains(root) &&
                scene->objects.Contains(selectedEntity) && !bridge::IsDestructibleIntactVisual(*scene, selectedEntity));
        }

        void RefreshRigidBody()
        {
            auto* scene = Scene();
            const auto* body = scene != nullptr && HasSelection() ? scene->rigidbodies.GetComponent(selectedEntity) : nullptr;
            const int partKind = scene && HasSelection() ? bridge::CompoundPartKind(*scene, selectedEntity) : 0;
            rigidAdd.SetEnabled(body == nullptr && HasSelection() && partKind == 0);
            rigidRemove.SetEnabled(body != nullptr);
            const auto compoundRoot = CompoundRoot();
            const auto* rootBody = scene ? scene->rigidbodies.GetComponent(compoundRoot) : nullptr;
            const bool supportsCompound = rootBody && bridge::SupportsCompoundCollider(*rootBody);
            const auto parts = supportsCompound ? bridge::GetCompoundParts(*scene, compoundRoot) : bridge::CompoundParts{};
            const bool addPart = supportsCompound && parts.valid && parts.count < bridge::MaxCompoundParts;
            compoundShape.SetEnabled(addPart);
            compoundAdd.SetEnabled(addPart);
            const bool enabled = body != nullptr;
            rigidShape.SetEnabled(enabled);
            rigidBoxX.SetEnabled(enabled); rigidBoxY.SetEnabled(enabled); rigidBoxZ.SetEnabled(enabled);
            rigidSphereRadius.SetEnabled(enabled); rigidCapsuleHeight.SetEnabled(enabled); rigidCapsuleRadius.SetEnabled(enabled);
            rigidMass.SetEnabled(enabled); rigidFriction.SetEnabled(enabled); rigidRestitution.SetEnabled(enabled);
            rigidLinearDamping.SetEnabled(enabled); rigidAngularDamping.SetEnabled(enabled); rigidBuoyancy.SetEnabled(enabled);
            rigidMeshLod.SetEnabled(enabled); rigidOffsetX.SetEnabled(enabled); rigidOffsetY.SetEnabled(enabled); rigidOffsetZ.SetEnabled(enabled);
            const bool sensor = enabled && bridge::IsBodySensor(*scene, selectedEntity);
            rigidSensor.SetEnabled(enabled && (sensor || bridge::SupportsBodySensor(*body)));
            rigidSensor.SetCheck(sensor);
            const bool supportsFilter = enabled && bridge::SupportsBodyCollisionFilter(*body);
            const auto filter = supportsFilter ? bridge::GetBodyCollisionFilter(*scene, selectedEntity) : bridge::BodyCollisionFilter{};
            rigidCollisionLayer.SetEnabled(supportsFilter);
            rigidCollisionLayer.SetValue(static_cast<float>(filter.layer));
            for (int i = 0; i < bridge::BodyCollisionLayerCount; ++i)
            {
                rigidCollisionMask[i].SetEnabled(supportsFilter);
                rigidCollisionMask[i].SetCheck((filter.mask & (1 << i)) != 0);
            }
            rigidKinematic.SetEnabled(enabled); rigid2D.SetEnabled(enabled); rigidDisableDeactivation.SetEnabled(enabled);
            rigidStartDeactivated.SetEnabled(enabled);
            if (body == nullptr)
                return;
            const auto state = bridge::CaptureCollision(*body);
            rigidShape.SetSelectedByUserdataWithoutCallback(Userdata(state.shape));
            rigidBoxX.SetValue(state.boxHalfExtents.x);
            rigidBoxY.SetValue(state.boxHalfExtents.y);
            rigidBoxZ.SetValue(state.boxHalfExtents.z);
            rigidSphereRadius.SetValue(state.sphereRadius);
            rigidCapsuleHeight.SetValue(state.capsuleHeight);
            rigidCapsuleRadius.SetValue(state.capsuleRadius);
            rigidMass.SetValue(state.mass);
            rigidFriction.SetValue(state.friction);
            rigidRestitution.SetValue(state.restitution);
            rigidLinearDamping.SetValue(state.dampingLinear);
            rigidAngularDamping.SetValue(state.dampingAngular);
            rigidBuoyancy.SetValue(state.buoyancy);
            rigidMeshLod.SetValue(static_cast<float>(state.meshLod));
            rigidOffsetX.SetValue(state.localOffset.x);
            rigidOffsetY.SetValue(state.localOffset.y);
            rigidOffsetZ.SetValue(state.localOffset.z);
            rigidKinematic.SetCheck(state.kinematic);
            rigid2D.SetCheck(state.locked2D);
            rigidDisableDeactivation.SetCheck(state.disableDeactivation);
            rigidStartDeactivated.SetCheck(state.startDeactivated);
        }

        void RefreshConstraint()
        {
            auto* scene = Scene();
            const auto* component = scene != nullptr && HasSelection() ? scene->constraints.GetComponent(selectedEntity) : nullptr;
            const bool enabled = component != nullptr;
            constraintAdd.SetEnabled(!enabled && HasSelection() && scene != nullptr && scene->transforms.Contains(selectedEntity));
            constraintRemove.SetEnabled(enabled);
            constraintRebind.SetEnabled(enabled);
            constraintType.SetEnabled(enabled); constraintBodyA.SetEnabled(enabled); constraintBodyB.SetEnabled(enabled);
            constraintNoSelfCollision.SetEnabled(enabled); constraintBreak.SetEnabled(enabled); constraintDisableBreaking.SetEnabled(enabled);
            distanceMin.SetEnabled(enabled); distanceMax.SetEnabled(enabled); hingeMin.SetEnabled(enabled); hingeMax.SetEnabled(enabled);
            hingeVelocity.SetEnabled(enabled); coneHalfAngle.SetEnabled(enabled); swingNormalCone.SetEnabled(enabled);
            swingPlaneCone.SetEnabled(enabled); swingMinTwist.SetEnabled(enabled); swingMaxTwist.SetEnabled(enabled);
            sliderMin.SetEnabled(enabled); sliderMax.SetEnabled(enabled); sliderVelocity.SetEnabled(enabled); sliderMaxForce.SetEnabled(enabled);
            for (auto& widget : sixDof) widget.SetEnabled(enabled);
            for (auto& widget : sixDofFix) widget.SetEnabled(enabled);
            RebuildConstraintBodies();
            if (component == nullptr)
                return;
            const auto state = bridge::CaptureConstraint(*component);
            constraintType.SetSelectedByUserdataWithoutCallback(Userdata(state.type));
            SelectEntity(constraintBodyA, state.bodyA);
            SelectEntity(constraintBodyB, state.bodyB);
            constraintNoSelfCollision.SetCheck(state.disableSelfCollision);
            constraintBreak.SetValue(std::isfinite(state.breakDistance) ? std::clamp(state.breakDistance, 0.0f, 1000.0f) : 1000.0f);
            distanceMin.SetValue(state.distanceMin);
            distanceMax.SetValue(state.distanceMax);
            hingeMin.SetValue(wi::math::RadiansToDegrees(state.hingeMinAngle));
            hingeMax.SetValue(wi::math::RadiansToDegrees(state.hingeMaxAngle));
            hingeVelocity.SetValue(state.hingeTargetAngularVelocity);
            coneHalfAngle.SetValue(wi::math::RadiansToDegrees(state.coneHalfAngle));
            swingNormalCone.SetValue(wi::math::RadiansToDegrees(state.swingTwistNormalHalfConeAngle));
            swingPlaneCone.SetValue(wi::math::RadiansToDegrees(state.swingTwistPlaneHalfConeAngle));
            swingMinTwist.SetValue(wi::math::RadiansToDegrees(state.swingTwistMinAngle));
            swingMaxTwist.SetValue(wi::math::RadiansToDegrees(state.swingTwistMaxAngle));
            sliderMin.SetValue(std::isfinite(state.sliderMinLimit) ? std::clamp(state.sliderMinLimit, -100.0f, 100.0f) : -100.0f);
            sliderMax.SetValue(std::isfinite(state.sliderMaxLimit) ? std::clamp(state.sliderMaxLimit, -100.0f, 100.0f) : 100.0f);
            sliderVelocity.SetValue(state.sliderTargetVelocity);
            sliderMaxForce.SetValue(std::clamp(state.sliderMaxForce, 0.0f, 100000.0f));
            const std::array<float, 12> values = {
                state.sixDofMinTranslation.x, state.sixDofMaxTranslation.x,
                state.sixDofMinTranslation.y, state.sixDofMaxTranslation.y,
                state.sixDofMinTranslation.z, state.sixDofMaxTranslation.z,
                wi::math::RadiansToDegrees(state.sixDofMinRotation.x), wi::math::RadiansToDegrees(state.sixDofMaxRotation.x),
                wi::math::RadiansToDegrees(state.sixDofMinRotation.y), wi::math::RadiansToDegrees(state.sixDofMaxRotation.y),
                wi::math::RadiansToDegrees(state.sixDofMinRotation.z), wi::math::RadiansToDegrees(state.sixDofMaxRotation.z)};
            for (std::size_t i = 0; i < values.size(); ++i)
            {
                const float fallback = i >= 6 ? ((i % 2) == 0 ? -180.0f : 180.0f) : ((i % 2) == 0 ? -100.0f : 100.0f);
                sixDof[i].SetValue(std::isfinite(values[i]) ? values[i] : fallback);
            }
        }

        void RefreshCharacter()
        {
            auto* scene = Scene();
            const auto* body = scene != nullptr && HasSelection() ? scene->rigidbodies.GetComponent(selectedEntity) : nullptr;
            const bool enabled = body != nullptr;
            characterEnabled.SetEnabled(enabled);
            characterSlope.SetEnabled(enabled);
            characterGravity.SetEnabled(enabled);
            if (!enabled)
            {
                characterEnabled.SetCheck(false);
                return;
            }
            const auto state = bridge::CaptureCharacterPhysics(*body);
            characterEnabled.SetCheck(state.enabled);
            characterSlope.SetValue(wi::math::RadiansToDegrees(state.maxSlopeAngle));
            characterGravity.SetValue(state.gravityFactor);
        }

        template <typename Fn>
        void EditSteeringWheel(Fn&& edit)
        {
            auto* scene = Scene();
            if (!scene || !HasSelection())
                return;
            auto settings = bridge::CaptureSteeringWheel(*scene, selectedEntity);
            edit(settings);
            if (ExecuteCommand<bridge::SetSteeringWheelCommand>(*scene, selectedEntity, settings))
                SetStatus("COCKPIT STEERING // APPLIED");
        }

        void RefreshSteeringWheel(const wi::scene::Scene* scene)
        {
            steeringWheel.ClearItems();
            steeringWheel.AddItem("<NONE>", wi::ecs::INVALID_ENTITY);
            const auto* body = scene ? scene->rigidbodies.GetComponent(selectedEntity) : nullptr;
            const bool car = body && body->IsCar();
            steeringWheel.SetEnabled(car);
            bridge::SteeringWheelSettings settings;
            if (car)
            {
                settings = bridge::CaptureSteeringWheel(*scene, selectedEntity);
                for (size_t i = 0; i < scene->objects.GetCount(); ++i)
                {
                    const auto entity = scene->objects.GetEntity(i);
                    if (bridge::CanBindSteeringWheel(*scene, selectedEntity, entity))
                        steeringWheel.AddItem(EntityLabel(*scene, entity), entity);
                }
            }
            SelectEntity(steeringWheel, settings.entity);
            const std::array axis{settings.axis.x, settings.axis.y, settings.axis.z};
            const bool bound = settings.entity != wi::ecs::INVALID_ENTITY;
            for (size_t i = 0; i < steeringWheelAxis.size(); ++i)
            {
                steeringWheelAxis[i].SetEnabled(bound);
                steeringWheelAxis[i].SetValue(axis[i]);
            }
            steeringWheelTravel.SetEnabled(bound);
            steeringWheelTravel.SetValue(settings.degrees);
        }

        void RefreshFixedWing(const wi::scene::Scene& scene)
        {
            if (!showFixedWingAdvanced || !FixedWingSelected() || page != Page::Vehicle)
                return;
            if (!showFixedWingBindings)
            {
                const auto values = bridge::ReadFixedWingTuning(scene, selectedEntity);
                for (size_t i = 0; i < fixedWingTuning.size(); ++i)
                    fixedWingTuning[i].SetValue(values.*bridge::FixedWingTuningFields[i].member);
                return;
            }

            const auto rig = bridge::ReadFixedWingRig(scene, selectedEntity);
            for (size_t i = 0; i < fixedWingParts.size(); ++i)
            {
                auto& combo = fixedWingParts[i];
                combo.ClearItems();
                const auto automatic = rig.automatic[i];
                const auto label = automatic == wi::ecs::INVALID_ENTITY ? "AUTO"
                    : "AUTO - " + EntityLabel(scene, automatic);
                combo.AddItem(label, Userdata(wi::ecs::INVALID_ENTITY));
                for (const auto entity : rig.candidates)
                    combo.AddItem(EntityLabel(scene, entity), Userdata(entity));

                const auto& assigned = rig.assigned[i];
                if (assigned.size() > 1)
                    combo.AddItem("MULTIPLE BINDINGS - CHOOSE ONE", Userdata(wi::ecs::INVALID_ENTITY));
                combo.SetSelectedByUserdataWithoutCallback(
                    Userdata(assigned.empty() ? wi::ecs::INVALID_ENTITY : assigned.front()));
            }
        }

        void RefreshVehicle()
        {
            auto* scene = Scene();
            const auto* body = scene != nullptr && HasSelection() ? scene->rigidbodies.GetComponent(selectedEntity) : nullptr;
            RebuildVehicleWheels();
            RefreshSteeringWheel(scene);
            const bool enabled = body != nullptr;
            vehiclePhysicsPreview.SetEnabled(body != nullptr && body->IsVehicle());
            vehiclePhysicsPreview.SetCheck(showVehiclePhysics);
            for (auto* widget : VehicleAuthoringControls()) widget->SetEnabled(enabled);
            vehicleTestThrottle.SetEnabled(enabled); vehicleTestSteer.SetEnabled(enabled);
            vehicleTestBrake.SetEnabled(enabled); vehicleTestHandbrake.SetEnabled(enabled);
            if (!enabled)
            {
                vehicleSendTestInput.SetEnabled(false);
                vehicleUpdateWheels.SetEnabled(false);
                return;
            }
            RefreshFixedWing(*scene);
            if (TrackedSelected() && page == Page::Vehicle)
            {
                const auto values = bridge::ReadTrackedTuning(*scene, selectedEntity);
                for (size_t i = 0; i < trackedTuning.size(); ++i)
                    trackedTuning[i].SetValue(values.*bridge::TrackedTuningFields[i].member);
            }
            const auto state = bridge::CaptureVehiclePhysics(*body);
            playerVehicleMode.SetSelectedByUserdataWithoutCallback(Userdata(bridge::GetPlayerVehicleMode(*scene, selectedEntity)));
            vehicleType.SetSelectedByUserdataWithoutCallback(Userdata(state.type));
            vehicleCollision.SetSelectedByUserdataWithoutCallback(Userdata(state.collisionMode));
            vehicleChassisWidth.SetValue(state.chassisHalfWidth);
            vehicleChassisHeight.SetValue(state.chassisHalfHeight);
            vehicleChassisLength.SetValue(state.chassisHalfLength);
            vehicleFrontOffset.SetValue(state.frontWheelOffset);
            vehicleRearOffset.SetValue(state.rearWheelOffset);
            vehicleWheelRadius.SetValue(state.wheelRadius);
            vehicleWheelWidth.SetValue(state.wheelWidth);
            vehicleEngineTorque.SetValue(state.maxEngineTorque);
            vehicleClutch.SetValue(state.clutchStrength);
            vehicleRoll.SetValue(wi::math::RadiansToDegrees(state.maxRollAngle));
            vehicleSteering.SetValue(wi::math::RadiansToDegrees(state.maxSteeringAngle));
            vehicleFrontSuspMin.SetValue(state.frontSuspensionMinLength);
            vehicleFrontSuspMax.SetValue(state.frontSuspensionMaxLength);
            vehicleFrontSuspFrequency.SetValue(state.frontSuspensionFrequency);
            vehicleFrontSuspDamping.SetValue(state.frontSuspensionDamping);
            vehicleRearSuspMin.SetValue(state.rearSuspensionMinLength);
            vehicleRearSuspMax.SetValue(state.rearSuspensionMaxLength);
            vehicleRearSuspFrequency.SetValue(state.rearSuspensionFrequency);
            vehicleRearSuspDamping.SetValue(state.rearSuspensionDamping);
            vehicle4WD.SetCheck(state.fourWheelDrive);
            vehicleMotoAngle.SetValue(wi::math::RadiansToDegrees(state.motorcycleFrontSuspensionAngle));
            vehicleMotoFrontBrake.SetValue(state.motorcycleFrontBrakeTorque);
            vehicleMotoRearBrake.SetValue(state.motorcycleRearBrakeTorque);
            vehicleMotoLean.SetCheck(state.motorcycleLeanControl);
            SelectEntity(vehicleWheels[0], state.wheelEntityFrontLeft);
            SelectEntity(vehicleWheels[1], state.wheelEntityFrontRight);
            SelectEntity(vehicleWheels[2], state.wheelEntityRearLeft);
            SelectEntity(vehicleWheels[3], state.wheelEntityRearRight);
            const bool live = bridge::HasLiveVehiclePhysics(*scene, selectedEntity);
            vehicleSendTestInput.SetEnabled(live);
            vehicleUpdateWheels.SetEnabled(scene->physics_scene != nullptr && wi::physics::IsSimulationEnabled());
        }

        void RefreshRagdoll()
        {
            auto* scene = Scene();
            const auto* humanoid = scene != nullptr && HasSelection() ? scene->humanoids.GetComponent(selectedEntity) : nullptr;
            const bool enabled = humanoid != nullptr;
            ragdollDisabled.SetEnabled(enabled);
            ragdollPhysics.SetEnabled(enabled);
            ragdoll2D.SetEnabled(enabled);
            ragdollFatness.SetEnabled(enabled);
            ragdollHeadSize.SetEnabled(enabled);
            if (!enabled)
                return;
            const auto state = bridge::CaptureRagdollPhysics(*humanoid);
            ragdollDisabled.SetCheck(state.disabled);
            ragdollPhysics.SetCheck(state.physicsEnabled);
            ragdoll2D.SetCheck(state.locked2D);
            ragdollFatness.SetValue(state.fatness);
            ragdollHeadSize.SetValue(state.headSize);
        }

        void RefreshSoftBody()
        {
            auto* scene = Scene();
            const auto mesh = scene != nullptr && HasSelection() ? bridge::ResolveSoftBodyMeshEntity(*scene, selectedEntity) : wi::ecs::INVALID_ENTITY;
            const auto* body = scene != nullptr ? scene->softbodies.GetComponent(mesh) : nullptr;
            const bool enabled = body != nullptr;
            softAdd.SetEnabled(mesh != wi::ecs::INVALID_ENTITY && !enabled);
            softRemove.SetEnabled(enabled);
            softPinTop.SetEnabled(enabled); softClearPins.SetEnabled(enabled);
            softReset.SetEnabled(enabled);
            softWake.SetEnabled(enabled && scene != nullptr && bridge::HasLiveSoftBodyPhysics(*scene, selectedEntity));
            softDetail.SetEnabled(enabled);
            softMass.SetEnabled(enabled);
            softFriction.SetEnabled(enabled);
            softRestitution.SetEnabled(enabled);
            softPressure.SetEnabled(enabled);
            softVertexRadius.SetEnabled(enabled);
            softWind.SetEnabled(enabled);
            if (!enabled)
                return;
            const auto state = bridge::CaptureSoftBodyPhysics(*body);
            softDetail.SetValue(state.detail);
            softMass.SetValue(state.mass);
            softFriction.SetValue(state.friction);
            softRestitution.SetValue(state.restitution);
            softPressure.SetValue(state.pressure);
            softVertexRadius.SetValue(state.vertexRadius);
            softWind.SetCheck(state.windEnabled);
        }

        void RefreshCollider()
        {
            auto* scene = Scene();
            const auto* collider = scene != nullptr && HasSelection() ? scene->colliders.GetComponent(selectedEntity) : nullptr;
            const bool enabled = collider != nullptr;
            colliderAdd.SetEnabled(!enabled && HasSelection());
            colliderRemove.SetEnabled(enabled);
            colliderShape.SetEnabled(enabled);
            colliderCPU.SetEnabled(enabled);
            colliderGPU.SetEnabled(enabled);
            colliderRadius.SetEnabled(enabled);
            colliderOffsetX.SetEnabled(enabled);
            colliderOffsetY.SetEnabled(enabled);
            colliderOffsetZ.SetEnabled(enabled);
            colliderTailX.SetEnabled(enabled);
            colliderTailY.SetEnabled(enabled);
            colliderTailZ.SetEnabled(enabled);
            if (!enabled)
                return;
            const auto state = bridge::CaptureWickedCollider(*collider);
            colliderShape.SetSelectedByUserdataWithoutCallback(Userdata(state.shape));
            colliderCPU.SetCheck(state.cpuEnabled);
            colliderGPU.SetCheck(state.gpuEnabled);
            colliderRadius.SetValue(state.radius);
            colliderOffsetX.SetValue(state.offset.x);
            colliderOffsetY.SetValue(state.offset.y);
            colliderOffsetZ.SetValue(state.offset.z);
            colliderTailX.SetValue(state.tail.x);
            colliderTailY.SetValue(state.tail.y);
            colliderTailZ.SetValue(state.tail.z);
        }

        std::vector<wi::gui::Widget*> VehicleAuthoringControls()
        {
            std::vector<wi::gui::Widget*> controls = {
                &playerVehicleMode, &vehicleType, &vehicleCollision, &vehicleChassisWidth, &vehicleChassisHeight, &vehicleChassisLength,
                &vehicleFrontOffset, &vehicleRearOffset, &vehicleWheelRadius, &vehicleWheelWidth, &vehicleEngineTorque,
                &vehicleClutch, &vehicleRoll, &vehicleSteering, &vehicleFrontSuspMin, &vehicleFrontSuspMax,
                &vehicleFrontSuspFrequency, &vehicleFrontSuspDamping, &vehicleRearSuspMin, &vehicleRearSuspMax,
                &vehicleRearSuspFrequency, &vehicleRearSuspDamping, &vehicle4WD, &vehicleMotoAngle,
                &vehicleMotoFrontBrake, &vehicleMotoRearBrake, &vehicleMotoLean};
            for (auto& combo : vehicleWheels) controls.push_back(&combo);
            return controls;
        }

        void RebuildConstraintBodies()
        {
            auto* scene = Scene();
            constraintBodyA.ClearItems();
            constraintBodyB.ClearItems();
            constraintBodyA.AddItem("<NONE>", static_cast<std::uint64_t>(wi::ecs::INVALID_ENTITY));
            constraintBodyB.AddItem("<NONE>", static_cast<std::uint64_t>(wi::ecs::INVALID_ENTITY));
            if (scene == nullptr)
                return;
            for (std::size_t i = 0; i < scene->rigidbodies.GetCount(); ++i)
            {
                const auto entity = scene->rigidbodies.GetEntity(i);
                const auto label = EntityLabel(*scene, entity);
                constraintBodyA.AddItem(label, static_cast<std::uint64_t>(entity));
                constraintBodyB.AddItem(label, static_cast<std::uint64_t>(entity));
            }
        }

        void RebuildVehicleWheels()
        {
            auto* scene = Scene();
            for (auto& combo : vehicleWheels)
            {
                combo.ClearItems();
                combo.AddItem("<NONE>", static_cast<std::uint64_t>(wi::ecs::INVALID_ENTITY));
                if (scene == nullptr)
                    continue;
                for (std::size_t i = 0; i < scene->transforms.GetCount(); ++i)
                {
                    const auto entity = scene->transforms.GetEntity(i);
                    combo.AddItem(EntityLabel(*scene, entity), static_cast<std::uint64_t>(entity));
                }
            }
        }

        static void SelectEntity(SceneInspectorComboBox& combo, const wi::ecs::Entity entity)
        {
            int selected = 0;
            for (std::size_t i = 0; i < combo.GetItemCount(); ++i)
            {
                if (static_cast<wi::ecs::Entity>(combo.GetItemUserData(static_cast<int>(i))) == entity)
                {
                    selected = static_cast<int>(i);
                    break;
                }
            }
            combo.SetSelectedWithoutCallback(selected);
        }

        void CommitGravity(const int axis, const float value)
        {
            auto* scene = Scene();
            if (scene == nullptr)
                return;
            const auto before = bridge::GetPhysicsGravity(*scene);
            auto after = before;
            if (axis == 0) after.x = value;
            else if (axis == 1) after.y = value;
            else after.z = value;
            if (ExecuteCommand<SetPhysicsGravityCommand>(*scene, before, after))
                SetStatus("WORLD // GRAVITY UPDATED // UNDO AVAILABLE");
        }

        void CommitWorldNumber(const int field, const float value)
        {
            auto state = bridge::CapturePhysicsWorldState();
            if (field == 0) state.accuracy = static_cast<int>(std::lround(value));
            else if (field == 1) state.frameRate = value;
            else if (field == 2) state.constraintDebugSize = value;
            else if (field == 3) state.debugDrawMaxDistance = value;
            else state.characterCollisionTolerance = value;
            bridge::ApplyPhysicsWorldState(state);
            SetStatus("WORLD // LIVE PHYSICS SETTING UPDATED");
            RequestRefresh();
        }

        void CommitSixDof(const std::size_t index, const float value)
        {
            EditConstraint([&](auto& state)
            {
                const bool rotation = index >= 6;
                const float stored = rotation ? wi::math::DegreesToRadians(value) : value;
                XMFLOAT3* vector = nullptr;
                if (index < 6)
                    vector = (index % 2) == 0 ? &state.sixDofMinTranslation : &state.sixDofMaxTranslation;
                else
                    vector = (index % 2) == 0 ? &state.sixDofMinRotation : &state.sixDofMaxRotation;
                const std::size_t axis = (index % 6) / 2;
                if (axis == 0) vector->x = stored;
                else if (axis == 1) vector->y = stored;
                else vector->z = stored;
            });
        }

        void FixSixDof(const std::size_t index)
        {
            EditConstraint([&](auto& state)
            {
                XMFLOAT3* minimum = index < 3 ? &state.sixDofMinTranslation : &state.sixDofMinRotation;
                XMFLOAT3* maximum = index < 3 ? &state.sixDofMaxTranslation : &state.sixDofMaxRotation;
                const std::size_t axis = index % 3;
                if (axis == 0) minimum->x = maximum->x = 0.0f;
                else if (axis == 1) minimum->y = maximum->y = 0.0f;
                else minimum->z = maximum->z = 0.0f;
            });
        }

        void Layout()
        {
            activeControls.clear();
            captions.clear();
            HideAll();
            if (!active || bounds.z <= bounds.x || bounds.w <= bounds.y)
                return;
            const float contentTop = bounds.y + HeaderHeight + TabBandHeight() + ReadoutHeight;
            contentClip.scissorRect={int32_t(bounds.x+14),int32_t(contentTop),int32_t(bounds.z-14),int32_t(bounds.w-StatusHeight-5)};
            contentClip.active_area=wi::primitive::Hitbox2D({bounds.x+14,contentTop},{bounds.z-bounds.x-28,std::max(0.f,bounds.w-StatusHeight-5-contentTop)});
            const float left = bounds.x + 30.0f;
            const float right = bounds.z - 30.0f;
            const float width = std::max(80.0f, right - left);
            const float gap = 18.0f;
            const float column = std::max(60.0f, (width - gap) * 0.5f);
            const float y = contentTop - scrollY;
            compactCursor = y;
            switch (page)
            {
            case Page::World: LayoutWorld(left, y, width, column, gap); break;
            case Page::RigidBody: LayoutRigid(left, y, width, column, gap); break;
            case Page::Constraint: LayoutConstraint(left, y, width, column, gap); break;
            case Page::Character: LayoutCharacter(left, y, width, column, gap); break;
            case Page::Vehicle: LayoutVehicle(left, y, width, column, gap); break;
            case Page::Ragdoll: LayoutRagdoll(left, y, width, column, gap); break;
            case Page::SoftBody: LayoutSoft(left, y, width, column, gap); break;
            case Page::WickedCollider: LayoutCollider(left, y, width, column, gap); break;
            case Page::Breakables:
                Place(rigidBreakSpeed, left, Row(y, 0), width);
                Place(breakHitPoints, left, Row(y, 1), width);
                Place(breakBlastSpeed, left, Row(y, 2), width);
                Place(breakDebrisStay, left, Row(y, 3), width);
                Place(breakDebrisFade, left, Row(y, 4), width);
                Place(breakUseIntactMesh, left, Row(y, 6), width);
                break;
            default: break;
            }
            ClampScroll();
        }

        void ClampScroll()
        {
            const float visible = std::max(100.0f,
                bounds.w - (bounds.y + HeaderHeight + TabBandHeight() + ReadoutHeight + StatusHeight + 8.0f));
            scrollY = std::clamp(scrollY, 0.0f, std::max(0.0f, pageContentHeight - visible));
        }

        template <typename Widget>
        void Place(Widget& widget, float x, float y, float width, const float height = RowHeight,
                   const char* caption = nullptr)
        {
            if (Compact())
            {
                x = bounds.x + 24.f;
                width = std::max(60.f, bounds.z - bounds.x - 48.f);
                y = compactCursor;
                if (caption) y += 14.f;
                compactCursor = y + height + RowGap;
            }
            if (caption) captions.push_back({caption, x, y - 14.f});
            widget.AttachTo(&contentClip);
            widget.SetPos(XMFLOAT2(x, y));
            widget.SetSize(XMFLOAT2(width, height));
            const float visibleTop = bounds.y + HeaderHeight + TabBandHeight() + ReadoutHeight - 4.0f;
            const float visibleBottom = bounds.w - StatusHeight - 5.0f;
            widget.SetVisible(y + height >= visibleTop && y <= visibleBottom);
            activeControls.push_back(&widget);
        }

        static float Row(const float y, const int row) noexcept
        {
            return y + static_cast<float>(row) * (RowHeight + RowGap);
        }

        void SetRows(const int rows) noexcept
        {
            const float top = bounds.y + HeaderHeight + TabBandHeight() + ReadoutHeight;
            pageContentHeight = Compact() ? std::max(0.f, compactCursor + scrollY - top) + 8.f
                : static_cast<float>(rows) * (RowHeight + RowGap) + 8.f;
        }

        void LayoutWorld(const float x, const float y, const float width, const float column, const float gap)
        {
            Place(worldEnabled, x, Row(y, 0), column);
            Place(worldSimulation, x + column + gap, Row(y, 0), column);
            Place(worldInterpolation, x, Row(y, 1), column);
            Place(worldDebug, x + column + gap, Row(y, 1), column);
            Place(worldStep, x, Row(y, 2), width);
            Place(worldGravityX, x, Row(y, 3), column);
            Place(worldGravityY, x + column + gap, Row(y, 3), column);
            Place(worldGravityZ, x, Row(y, 4), column);
            Place(worldAccuracy, x + column + gap, Row(y, 4), column);
            Place(worldFrameRate, x, Row(y, 5), column);
            Place(worldConstraintDebugSize, x + column + gap, Row(y, 5), column);
            Place(worldDebugDistance, x, Row(y, 6), column);
            Place(worldCharacterTolerance, x + column + gap, Row(y, 6), column);
            Place(worldActivateAll, x, Row(y, 8), column);
            Place(worldOptimize, x + column + gap, Row(y, 8), column);
            Place(worldReset, x, Row(y, 9), width);
            SetRows(11);
        }

        void LayoutRigid(const float x, const float y, const float width, const float column, const float gap)
        {
            auto* scene = Scene();
            const auto* body = scene != nullptr && HasSelection() ? scene->rigidbodies.GetComponent(selectedEntity) : nullptr;
            if (scene && HasSelection() && bridge::CompoundPartKind(*scene, selectedEntity))
            {
                captions.push_back({"MOVE, ROTATE OR SCALE IN VIEWPORT", x, Row(y, 0)});
                captions.push_back({"DELETE PART FROM THE OBJECT HIERARCHY", x, Row(y, 1)});
                if (Compact())
                    compactCursor += RowHeight * 2.0f;
                Place(compoundShape, x, Row(y, 3), width, RowHeight, "COLLIDER PART SHAPE");
                Place(compoundAdd, x, Row(y, 4), width);
                SetRows(6);
                return;
            }
            Place(rigidAdd, x, Row(y, 0), column);
            Place(rigidRemove, x + column + gap, Row(y, 0), column);
            if (body == nullptr) { SetRows(3); return; }
            const auto state = bridge::CaptureCollision(*body);
            int row = 2;
            bool activeCompound = false;
            if (bridge::SupportsCompoundCollider(*body))
            {
                const auto parts = bridge::GetCompoundParts(*scene, selectedEntity);
                activeCompound = parts.valid && parts.count > 0;
                Place(compoundShape, x, Row(y, row++), width, RowHeight, "COLLIDER PART SHAPE");
                Place(compoundAdd, x, Row(y, row++), width);
                ++row;
            }
            Place(rigidShape, x, Row(y, row++), width, RowHeight,
                activeCompound ? "FALLBACK SHAPE (NO PARTS)" : "COLLISION SHAPE");
            using Shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape;
            if (state.shape == Shape::BOX)
            {
                Place(rigidBoxX, x, Row(y, row), column); Place(rigidBoxY, x + column + gap, Row(y, row++), column);
                Place(rigidBoxZ, x, Row(y, row++), column);
            }
            else if (state.shape == Shape::SPHERE)
            {
                Place(rigidSphereRadius, x, Row(y, row++), width);
            }
            else if (state.shape == Shape::CAPSULE || state.shape == Shape::CYLINDER)
            {
                Place(rigidCapsuleHeight, x, Row(y, row), column); Place(rigidCapsuleRadius, x + column + gap, Row(y, row++), column);
            }
            else if (state.shape == Shape::TRIANGLE_MESH)
            {
                Place(rigidMeshLod, x, Row(y, row++), width);
            }
            row++;
            Place(rigidMass, x, Row(y, row), column); Place(rigidFriction, x + column + gap, Row(y, row++), column);
            Place(rigidRestitution, x, Row(y, row), column); Place(rigidBuoyancy, x + column + gap, Row(y, row++), column);
            Place(rigidLinearDamping, x, Row(y, row), column); Place(rigidAngularDamping, x + column + gap, Row(y, row++), column);
            Place(rigidSensor, x, Row(y, row++), width);
            Place(rigidCollisionLayer, x, Row(y, row++), width);
            for (int i = 0; i < bridge::BodyCollisionLayerCount; i += 2)
            {
                Place(rigidCollisionMask[i], x, Row(y, row), column);
                Place(rigidCollisionMask[i + 1], x + column + gap, Row(y, row++), column);
            }
            Place(rigidKinematic, x, Row(y, row), column); Place(rigid2D, x + column + gap, Row(y, row++), column);
            Place(rigidDisableDeactivation, x, Row(y, row), column); Place(rigidStartDeactivated, x + column + gap, Row(y, row++), column);
            row++;
            Place(rigidOffsetX, x, Row(y, row), column); Place(rigidOffsetY, x + column + gap, Row(y, row++), column);
            Place(rigidOffsetZ, x, Row(y, row++), column);
            SetRows(row + 2);
        }

        void LayoutConstraint(const float x, const float y, const float width, const float column, const float gap)
        {
            auto* scene = Scene();
            const auto* component = scene != nullptr && HasSelection() ? scene->constraints.GetComponent(selectedEntity) : nullptr;
            Place(constraintAdd, x, Row(y, 0), column);
            Place(constraintRemove, x + column + gap, Row(y, 0), column);
            if (component == nullptr) { SetRows(3); return; }
            const auto state = bridge::CaptureConstraint(*component);
            Place(constraintType, x, Row(y, 1), width);
            Place(constraintBodyA, x, Row(y, 2), column); Place(constraintBodyB, x + column + gap, Row(y, 2), column);
            Place(constraintRebind, x, Row(y, 4), width);
            Place(constraintNoSelfCollision, x, Row(y, 5), column);
            Place(constraintBreak, x + column + gap, Row(y, 5), column);
            Place(constraintDisableBreaking, x, Row(y, 6), width);
            int row = 8;
            using Type = wi::scene::PhysicsConstraintComponent::Type;
            if (state.type == Type::Distance)
            {
                Place(distanceMin, x, Row(y, row), column); Place(distanceMax, x + column + gap, Row(y, row++), column);
            }
            else if (state.type == Type::Hinge)
            {
                Place(hingeMin, x, Row(y, row), column); Place(hingeMax, x + column + gap, Row(y, row++), column);
                Place(hingeVelocity, x, Row(y, row++), width);
            }
            else if (state.type == Type::Cone)
            {
                Place(coneHalfAngle, x, Row(y, row++), width);
            }
            else if (state.type == Type::SwingTwist)
            {
                Place(swingNormalCone, x, Row(y, row), column); Place(swingPlaneCone, x + column + gap, Row(y, row++), column);
                Place(swingMinTwist, x, Row(y, row), column); Place(swingMaxTwist, x + column + gap, Row(y, row++), column);
            }
            else if (state.type == Type::Slider)
            {
                Place(sliderMin, x, Row(y, row), column); Place(sliderMax, x + column + gap, Row(y, row++), column);
                Place(sliderVelocity, x, Row(y, row), column); Place(sliderMaxForce, x + column + gap, Row(y, row++), column);
            }
            else if (state.type == Type::SixDOF)
            {
                for (std::size_t i = 0; i < sixDofFix.size(); i += 2)
                {
                    Place(sixDofFix[i], x, Row(y, row), column);
                    Place(sixDofFix[i + 1], x + column + gap, Row(y, row++), column);
                }
                row++;
                for (std::size_t i = 0; i < sixDof.size(); i += 2)
                {
                    Place(sixDof[i], x, Row(y, row), column);
                    Place(sixDof[i + 1], x + column + gap, Row(y, row++), column);
                }
            }
            SetRows(row + 2);
        }

        void LayoutCharacter(const float x, const float y, const float width, const float column, const float gap)
        {
            Place(characterEnabled, x, Row(y, 0), width);
            Place(characterSlope, x, Row(y, 2), column);
            Place(characterGravity, x + column + gap, Row(y, 2), column);
            SetRows(5);
        }

        void LayoutVehicle(const float x, const float y, const float width, const float column, const float gap)
        {
            int row = 0;
            if (FixedWingSelected())
            {
                Place(playerVehicleMode, x, Row(y, row++), width, RowHeight, "PLAYER CONTROLS");
                Place(rigidMass, x, Row(y, row++), width);
                Place(rigidLinearDamping, x, Row(y, row++), width);
                Place(rigidAngularDamping, x, Row(y, row++), width);
                fixedWingAdvanced.SetText(showFixedWingAdvanced ? "HIDE ADVANCED" : "ADVANCED");
                Place(fixedWingAdvanced, x, Row(y, row++), width);
                if (showFixedWingAdvanced)
                {
                    fixedWingBindings.SetText(showFixedWingBindings ? "FLIGHT TUNING" : "PART BINDINGS");
                    Place(fixedWingBindings, x, Row(y, row++), width);
                    if (showFixedWingBindings)
                    {
                        constexpr auto gearStart = static_cast<size_t>(bridge::FixedWingPart::NoseWheel);
                        constexpr auto forceStart = static_cast<size_t>(bridge::FixedWingPart::PortLift);
                        const auto placePart = [&](size_t i)
                        {
                            Place(fixedWingParts[i], x, Row(y, row++), width, RowHeight,
                                bridge::FixedWingParts[i].label);
                        };
                        for (size_t i = gearStart; i < forceStart; ++i) placePart(i);
                        for (size_t i = 0; i < gearStart; ++i) placePart(i);
                        for (size_t i = forceStart; i < fixedWingParts.size(); ++i) placePart(i);
                    }
                    else
                    {
                        for (auto& slider : fixedWingTuning)
                            Place(slider, x, Row(y, row++), width);
                    }
                }
                SetRows(row + 2);
                return;
            }
            if (TrackedSelected())
            {
                Place(playerVehicleMode, x, Row(y, row++), width, RowHeight, "PLAYER CONTROLS");
                Place(rigidMass, x, Row(y, row++), width);
                Place(vehicleCollision, x, Row(y, row++), width, RowHeight, "CONTACT TEST");
                for (size_t i = 0; i < trackedTuning.size(); ++i)
                    if (!bridge::TrackedTuningFields[i].advanced)
                        Place(trackedTuning[i], x, Row(y, row++), width);
                trackedAdvanced.SetText(showTrackedAdvanced ? "HIDE ADVANCED" : "ADVANCED");
                Place(trackedAdvanced, x, Row(y, row++), width);
                if (showTrackedAdvanced)
                    for (size_t i = 0; i < trackedTuning.size(); ++i)
                        if (bridge::TrackedTuningFields[i].advanced)
                            Place(trackedTuning[i], x, Row(y, row++), width);
                SetRows(row + 2);
                return;
            }
            if (ForceFlightSelected())
            {
                Place(playerVehicleMode, x, Row(y, row++), width, RowHeight, "PLAYER CONTROLS");
                Place(rigidMass, x, Row(y, row++), width);
                Place(rigidLinearDamping, x, Row(y, row++), width);
                Place(rigidAngularDamping, x, Row(y, row++), width);
                SetRows(row + 2);
                return;
            }
            Place(vehiclePhysicsPreview, x, Row(y, row++), width);
            Place(playerVehicleMode, x, Row(y, row++), width, RowHeight, "PLAYER CONTROLS");
            Place(vehicleType, x, Row(y, row), column); Place(vehicleCollision, x + column + gap, Row(y, row++), column);
            ++row;
            constexpr std::array<const char*, 4> wheelCaptions = {
                "FRONT LEFT WHEEL", "FRONT RIGHT WHEEL", "REAR LEFT WHEEL", "REAR RIGHT WHEEL"};
            for (std::size_t i = 0; i < vehicleWheels.size(); i += 2)
            {
                Place(vehicleWheels[i], x, Row(y, row), column, RowHeight, wheelCaptions[i]);
                Place(vehicleWheels[i + 1], x + column + gap, Row(y, row), column, RowHeight, wheelCaptions[i + 1]);
                row += 2;
            }
            row++;
            Place(vehicleChassisWidth, x, Row(y, row), column); Place(vehicleChassisHeight, x + column + gap, Row(y, row++), column);
            Place(vehicleChassisLength, x, Row(y, row), column); Place(vehicleWheelRadius, x + column + gap, Row(y, row++), column);
            Place(vehicleWheelWidth, x, Row(y, row), column); Place(vehicleEngineTorque, x + column + gap, Row(y, row++), column);
            Place(vehicleClutch, x, Row(y, row), column); Place(vehicleRoll, x + column + gap, Row(y, row++), column);
            Place(vehicleSteering, x, Row(y, row), column); Place(vehicleFrontOffset, x + column + gap, Row(y, row++), column);
            Place(vehicleRearOffset, x, Row(y, row++), column);
            Place(steeringWheel, x, Row(y, row++), width, RowHeight, "STEERING WHEEL");
            for (auto& axis : steeringWheelAxis) Place(axis, x, Row(y, row++), width);
            Place(steeringWheelTravel, x, Row(y, row++), width);
            row++;
            Place(vehicleFrontSuspMin, x, Row(y, row), column); Place(vehicleFrontSuspMax, x + column + gap, Row(y, row++), column);
            Place(vehicleFrontSuspFrequency, x, Row(y, row), column); Place(vehicleFrontSuspDamping, x + column + gap, Row(y, row++), column);
            Place(vehicleRearSuspMin, x, Row(y, row), column); Place(vehicleRearSuspMax, x + column + gap, Row(y, row++), column);
            Place(vehicleRearSuspFrequency, x, Row(y, row), column); Place(vehicleRearSuspDamping, x + column + gap, Row(y, row++), column);
            Place(vehicle4WD, x, Row(y, row++), width);
            row++;
            Place(vehicleMotoAngle, x, Row(y, row), column); Place(vehicleMotoFrontBrake, x + column + gap, Row(y, row++), column);
            Place(vehicleMotoRearBrake, x, Row(y, row), column); Place(vehicleMotoLean, x + column + gap, Row(y, row++), column);
            row++;
            row++;
            Place(vehicleTestThrottle, x, Row(y, row), column); Place(vehicleTestSteer, x + column + gap, Row(y, row++), column);
            Place(vehicleTestBrake, x, Row(y, row), column); Place(vehicleTestHandbrake, x + column + gap, Row(y, row++), column);
            Place(vehicleSendTestInput, x, Row(y, row), column); Place(vehicleUpdateWheels, x + column + gap, Row(y, row++), column);
            SetRows(row + 2);
        }

        void LayoutRagdoll(const float x, const float y, const float width, const float column, const float gap)
        {
            Place(ragdollDisabled, x, Row(y, 0), column); Place(ragdollPhysics, x + column + gap, Row(y, 0), column);
            Place(ragdoll2D, x, Row(y, 1), width);
            Place(ragdollFatness, x, Row(y, 3), column); Place(ragdollHeadSize, x + column + gap, Row(y, 3), column);
            Place(ragdollActivateAll, x, Row(y, 5), width);
            SetRows(7);
        }

        void LayoutSoft(const float x, const float y, const float width, const float column, const float gap)
        {
            Place(softAdd, x, Row(y, 0), column); Place(softRemove, x + column + gap, Row(y, 0), column);
            Place(softReset, x, Row(y, 1), column); Place(softWake, x + column + gap, Row(y, 1), column);
            Place(softPinTop, x, Row(y, 2), column); Place(softClearPins, x + column + gap, Row(y, 2), column);
            Place(softDetail, x, Row(y, 3), column); Place(softMass, x + column + gap, Row(y, 3), column);
            Place(softFriction, x, Row(y, 4), column); Place(softRestitution, x + column + gap, Row(y, 4), column);
            Place(softPressure, x, Row(y, 5), column); Place(softVertexRadius, x + column + gap, Row(y, 5), column);
            Place(softWind, x, Row(y, 6), width);
            SetRows(8);
        }

        void LayoutCollider(const float x, const float y, const float width, const float column, const float gap)
        {
            Place(colliderAdd, x, Row(y, 0), column); Place(colliderRemove, x + column + gap, Row(y, 0), column);
            Place(colliderShape, x, Row(y, 2), width);
            Place(colliderCPU, x, Row(y, 3), column); Place(colliderGPU, x + column + gap, Row(y, 3), column);
            Place(colliderRadius, x, Row(y, 5), width);
            Place(colliderOffsetX, x, Row(y, 6), column); Place(colliderOffsetY, x + column + gap, Row(y, 6), column);
            Place(colliderOffsetZ, x, Row(y, 7), column);
            Place(colliderTailX, x, Row(y, 9), column); Place(colliderTailY, x + column + gap, Row(y, 9), column);
            Place(colliderTailZ, x, Row(y, 10), column);
            SetRows(12);
        }
    };

    RenegadePhysicsLabWorkspace::RenegadePhysicsLabWorkspace()
        : impl_(std::make_unique<Impl>())
    {
    }

    RenegadePhysicsLabWorkspace::~RenegadePhysicsLabWorkspace() = default;

    void RenegadePhysicsLabWorkspace::Create()
    {
        impl_->Create();
    }

    void RenegadePhysicsLabWorkspace::SetActive(const bool active)
    {
        impl_->SetActive(active);
    }

    bool RenegadePhysicsLabWorkspace::IsActive() const noexcept
    {
        return impl_->active;
    }

    void RenegadePhysicsLabWorkspace::SetPage(Page page)
    {
        if (page >= Page::Count || impl_->page == page)
            return;
        impl_->page = page;
        impl_->scrollY = 0.f;
        impl_->RequestRefresh();
    }

    void RenegadePhysicsLabWorkspace::SetBounds(const XMFLOAT4& bounds)
    {
        impl_->SetBounds(bounds);
    }

    bool RenegadePhysicsLabWorkspace::ContainsPointer(const XMFLOAT4& pointer) const noexcept
    {
        return impl_->ContainsPointer(pointer);
    }

    bool RenegadePhysicsLabWorkspace::ConsumedPointerThisFrame() const noexcept
    {
        return impl_->pointerConsumed;
    }

    void RenegadePhysicsLabWorkspace::Update(const wi::Canvas& canvas, const float dt)
    {
        impl_->Update(canvas, dt);
    }

    void RenegadePhysicsLabWorkspace::Render(
        const wi::Canvas& canvas,
        const wi::graphics::CommandList cmd) const
    {
        impl_->Render(canvas, cmd);
    }

    RenegadePhysicsLabWorkspace::Page RenegadePhysicsLabWorkspace::ActivePage() const noexcept
    {
        return impl_->page;
    }
}
