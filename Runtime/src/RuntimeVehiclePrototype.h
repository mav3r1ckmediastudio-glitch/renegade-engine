#pragma once

#include "renegade/bridge/FlightVehicleService.h"
#include "RuntimeFixedWing.h"
#include "RuntimeTrackedVehicle.h"
#include "RuntimeVtolThrusterEffects.h"
#include "renegade/bridge/GameplayInputService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/ProjectileEffectRuntime.h"
#include "renegade/bridge/TerrainPropCollisionWorld.h"
#include "renegade/bridge/VehiclePhysicsService.h"
#include "renegade/bridge/VehicleSteeringService.h"
#include "renegade/bridge/VehicleCockpitCamera.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace renegade::runtime
{
    struct VehicleSkidTrail
    {
        struct Sample
        {
            wi::ecs::Entity receiver = wi::ecs::INVALID_ENTITY;
            XMFLOAT3 position{};
        };
        static constexpr size_t Capacity = 320;
        std::array<Sample, 4> previous{};
        std::array<float, 4> slipIntensity{};
        std::array<wi::ecs::Entity, Capacity> segments{};
        size_t next = 0;
        float elapsed = 0;

        void BreakTracks()
        {
            previous = {};
            slipIntensity = {};
            elapsed = 0.f;
        }

        static const wi::graphics::Texture& Tread()
        {
            static wi::graphics::Texture texture;
            if (texture.IsValid() || wi::graphics::GetDevice() == nullptr)
                return texture;

            constexpr unsigned Size = 32;
            std::array<unsigned char, Size * Size * 4> pixels{};
            for (unsigned y = 0; y < Size; ++y)
            for (unsigned x = 0; x < Size; ++x)
            {
                const float lateral = (float(x) + .5f) / 16.f - 1.f;
                const float edge = std::clamp((1.f - std::abs(lateral)) * 4.f, 0.f, 1.f);
                const float grain = .78f + .22f * float((x * 17 + y * 41) % 13) / 12.f;
                const float groove = x == 8 || x == 16 || x == 23 ? .62f : 1.f;
                const size_t index = (y * Size + x) * 4;
                pixels[index] = pixels[index + 1] = pixels[index + 2] = 255;
                pixels[index + 3] = static_cast<unsigned char>(255.f * edge * grain * groove);
            }
            wi::graphics::TextureDesc desc;
            desc.width = desc.height = Size;
            desc.format = wi::graphics::Format::R8G8B8A8_UNORM;
            desc.bind_flags = wi::graphics::BindFlag::SHADER_RESOURCE;
            wi::graphics::SubresourceData data;
            data.data_ptr = pixels.data();
            data.row_pitch = Size * 4;
            data.slice_pitch = pixels.size();
            wi::graphics::GetDevice()->CreateTexture(&desc, &data, &texture);
            return texture;
        }

        void Add(wi::scene::Scene& scene, XMVECTOR center, XMVECTOR tangent, XMVECTOR normal,
                 float width, float length, float intensity)
        {
            const auto& tread = Tread();
            if (!tread.IsValid())
                return;

            auto& entry = segments[next++ % Capacity];
            if (entry == wi::ecs::INVALID_ENTITY || !scene.decals.Contains(entry))
                entry = scene.Entity_CreateDecal("Vehicle tyre mark", "", "");
            auto* transform = scene.transforms.GetComponent(entry);
            auto* material = scene.materials.GetComponent(entry);
            if (!transform || !material)
                return;

            material->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(tread);
            material->baseColor = {.055f, .049f, .044f, intensity};
            material->userBlendMode = wi::enums::BLENDMODE_ALPHA;
            material->SetRoughness(.94f);
            material->SetDirty();

            center += normal * .014f;
            const XMMATRIX facing = XMMatrixInverse(nullptr, XMMatrixLookToLH(XMVectorZero(), -normal, tangent));
            transform->ClearTransform();
            transform->MatrixTransform(
                XMMatrixScaling(width * .5f, length * .53f, .045f) * facing * XMMatrixTranslationFromVector(center));
            transform->UpdateTransform();
        }

        void Update(wi::scene::Scene& scene, wi::ecs::Entity chassis, float seconds, float speed,
                    float steering)
        {
            const auto* body = scene.rigidbodies.GetComponent(chassis);
            if (!body || body->vehicle.type != wi::scene::RigidBodyPhysicsComponent::Vehicle::Type::Car)
            {
                BreakTracks();
                return;
            }
            elapsed += std::max(0.f, seconds);
            if (elapsed < .055f)
                return;

            if (elapsed > .18f)
                previous = {};
            const float sampleTime = elapsed;
            elapsed = 0.f;
            const auto& wheels = body->vehicle;
            const std::array<wi::ecs::Entity, 4> wheelIds{
                wheels.wheel_entity_front_left, wheels.wheel_entity_front_right,
                wheels.wheel_entity_rear_left, wheels.wheel_entity_rear_right
            };
            for (size_t i = 0; i < wheelIds.size(); ++i)
            {
                auto& sample = previous[i];
                slipIntensity[i] = 0.f;
                const auto* wheel = scene.transforms.GetComponent(wheelIds[i]);
                if (!wheel)
                {
                    sample = {};
                    continue;
                }

                XMFLOAT3 origin = wheel->GetPosition();
                origin.y -= wheels.wheel_radius * .38f;
                const auto hit = wi::physics::Intersects(scene, wi::primitive::Ray(
                    origin, XMFLOAT3(0,-1,0), 0, wheels.wheel_radius * .62f + .105f));
                if (!hit.IsValid() || hit.entity == chassis || hit.normal.y < .2f)
                {
                    sample = {};
                    continue;
                }
                const Sample nextSample{hit.entity, hit.position};
                if (sample.receiver == hit.entity && std::abs(speed) > 8.f && std::abs(steering) > .25f)
                {
                    const XMVECTOR a = XMLoadFloat3(&sample.position);
                    const XMVECTOR b = XMLoadFloat3(&hit.position);
                    const XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&hit.normal));
                    XMVECTOR motion = b - a;
                    motion -= n * XMVectorGetX(XMVector3Dot(motion, n));
                    const float distance = XMVectorGetX(XMVector3Length(motion));

                    XMVECTOR axle = XMLoadFloat4x4(&wheel->world).r[0];
                    axle -= n * XMVectorGetX(XMVector3Dot(axle, n));
                    const float axleLength = XMVectorGetX(XMVector3Length(axle));
                    if (distance > .10f && distance < 4.5f && axleLength > .1f)
                    {
                        const float slip = std::abs(XMVectorGetX(XMVector3Dot(motion, axle / axleLength))) / sampleTime;
                        if (slip > 1.30f)
                        {
                            const float intensity = std::clamp(.24f + .09f * (slip - 1.3f), .24f, .70f);
                            slipIntensity[i] = intensity;
                            Add(scene, (a + b) * .5f, motion / distance, n,
                                std::clamp(wheels.wheel_width * .86f, .09f, .45f), distance, intensity);
                        }
                    }
                }
                sample = nextSample;
            }
        }
    };


    struct VanBrakeSignal
    {
        float lastSpeed = -1.f;
        float filteredDecel = 0.f;
        bool slowing = false;

        bool Update(float signedSpeed, float dt, bool brake)
        {
            const float speed = std::abs(signedSpeed);
            if (lastSpeed >= 0.f && dt > 0.f)
            {
                const float step = std::max(dt, .004f);
                const float decel = std::clamp((lastSpeed - speed) / step, -30.f, 30.f);
                filteredDecel += (decel - filteredDecel) * (1.f - std::exp(-step / .10f));
                slowing = speed > 1.f && filteredDecel > (slowing ? 1.6f : 2.8f);
            }
            lastSpeed = speed;
            return brake || slowing;
        }
    };

    struct VanBrakeLights
    {
        wi::ecs::Entity materialID = wi::ecs::INVALID_ENTITY;
        XMFLOAT4 originalColor{};
        VanBrakeSignal signal;
        bool active = false;

        void Begin(wi::scene::Scene& scene, wi::ecs::Entity chassis)
        {
            End(scene);
            for (size_t i = 0; i < scene.objects.GetCount(); ++i)
            {
                const auto object = scene.objects.GetEntity(i);
                const auto* name = scene.names.GetComponent(object);
                if (!name || name->name != "BrakeLights" || !scene.Entity_IsDescendant(object, chassis))
                    continue;
                const auto* mesh = scene.meshes.GetComponent(scene.objects[i].meshID);
                if (!mesh)
                    continue;
                for (const auto& subset : mesh->subsets)
                {
                    const auto* materialName = scene.names.GetComponent(subset.materialID);
                    const auto* material = scene.materials.GetComponent(subset.materialID);
                    if (material && materialName && materialName->name == "M_VehicleBrakeLights")
                    {
                        materialID = subset.materialID;
                        originalColor = material->emissiveColor;
                        return;
                    }
                }
            }
        }

        void End(wi::scene::Scene& scene)
        {
            Apply(scene, false);
            *this = {};
        }

        void Apply(wi::scene::Scene& scene, bool enabled)
        {
            if (active == enabled)
                return;
            if (auto* material = scene.materials.GetComponent(materialID))
            {
                material->SetEmissiveColor(enabled ? XMFLOAT4{1.f, .009f, .004f, 2.25f} : originalColor);
                active = enabled;
                wi::backlog::post(active ? "Vehicle brake lamps: on" : "Vehicle brake lamps: off");
            }
        }

        void Update(wi::scene::Scene& scene, float speed, float dt, bool brake)
        {
            Apply(scene, signal.Update(speed, dt, brake));
        }
    };

    struct VehicleSmoke
    {
        wi::ecs::Entity exhaust = wi::ecs::INVALID_ENTITY;
        std::array<wi::ecs::Entity,4> wheels{};

        static void UpdateEmitter(wi::scene::Scene& scene, wi::ecs::Entity& id,
                                  XMFLOAT3 position, float rate, bool tyre)
        {
            if (!scene.emitters.Contains(id))
            {
                if (rate <= 0.f) return;
                bridge::ProjectileEffectLayer layer;
                layer.kind = bridge::ProjectileEffectKind::Smoke;
                layer.sizeMetres = tyre ? .33f : .17f;
                layer.particleLifeSeconds = tyre ? 1.15f : .95f;
                layer.particlesPerSecond = rate;
                id = bridge::CreateProjectileEffectEmitter(scene, layer, position);
                auto& source = *scene.emitters.GetComponent(id);
                source.SetMaxParticleCount(96);
                source.random_factor = .28f;
                source.normal_factor = 0.f;
                source.scaleX = tyre ? 2.35f : 1.85f;
                source.scaleY = source.scaleX;
                source.SetOpacityCurveControl(.72f,.48f);
                auto& material = *scene.materials.GetComponent(id);
                material.SetBaseColor(tyre ? XMFLOAT4(.46f,.46f,.45f,.54f)
                                           : XMFLOAT4(.32f,.34f,.36f,.42f));
                if (auto* named = scene.names.GetComponent(id))
                    named->name = tyre ? "Vehicle tyre smoke" : "Vehicle exhaust";
            }
            auto* source = scene.emitters.GetComponent(id);
            auto* transform = scene.transforms.GetComponent(id);
            if (!source || !transform) return;
            transform->translation_local = position;
            transform->SetDirty();
            source->count = rate;
            source->velocity = {0,tyre ? .56f : .28f,0};
        }

        void Stop(wi::scene::Scene& scene)
        {
            if (auto* source = scene.emitters.GetComponent(exhaust)) source->count = 0.f;
            for (auto id : wheels)
                if (auto* source = scene.emitters.GetComponent(id)) source->count = 0.f;
        }

        void Update(wi::scene::Scene& scene, wi::ecs::Entity car, float throttle,
                    const std::array<float,4>& intensity)
        {
            const auto* pose = scene.transforms.GetComponent(car);
            const auto* body = scene.rigidbodies.GetComponent(car);
            if (!pose || !body) return;
            const auto radius = body->vehicle.wheel_radius;
            const std::array<wi::ecs::Entity,4> wheelIds{
                body->vehicle.wheel_entity_front_left, body->vehicle.wheel_entity_front_right,
                body->vehicle.wheel_entity_rear_left, body->vehicle.wheel_entity_rear_right};
            XMFLOAT3 pipe;
            XMStoreFloat3(&pipe, XMVector3TransformCoord(
                XMVectorSet(-.263f,-.43f,-1.58f,1), XMLoadFloat4x4(&pose->world)));
            UpdateEmitter(scene, exhaust, pipe, 3.f+std::abs(throttle)*8.f, false);
            for (size_t i=0; i<wheels.size(); ++i)
            {
                const auto* wheel = scene.transforms.GetComponent(wheelIds[i]);
                if (!wheel)
                {
                    if (auto* emitter=scene.emitters.GetComponent(wheels[i])) emitter->count=0.f;
                    continue;
                }
                auto position = wheel->GetPosition();
                position.y -= radius*.78f;
                UpdateEmitter(scene,wheels[i],position,intensity[i]*32.f,true);
            }
        }
    };

    struct RuntimeVehicleTrial
    {
        float altitudeTarget = 0.f;
        bridge::ScriptedFlightFrame scriptedFlight;
        wi::unordered_map<wi::ecs::Entity, RuntimeFixedWing> fixedWings;
        wi::unordered_map<wi::ecs::Entity, RuntimeTrackedVehicle> trackedVehicles;
        std::optional<BastionTurretShot> bastionShot;
        wi::ecs::Entity forceFlightPilotEye = wi::ecs::INVALID_ENTITY;
        std::array<wi::ecs::Entity, 3> rotorPivots{};
        std::array<XMFLOAT4, 3> rotorRest{};
        float rotorPhase = 0.f;
        float rotorCyclic = 0.f;

        void BindRotors(wi::scene::Scene& scene)
        {
            constexpr const char* names[] = {"Main_Rotor_Tilt", "Main_Rotor_Spin", "Tail_Rotor_Spin"};
            rotorPhase = rotorCyclic = 0.f;
            for (size_t i = 0; i < std::size(names); ++i)
            {
                rotorPivots[i] = scene.Entity_FindByName(names[i], current);
                if (const auto* pose = scene.transforms.GetComponent(rotorPivots[i]))
                    rotorRest[i] = pose->rotation_local;
            }
        }

        void UpdateRotors(wi::scene::Scene& scene, float cyclic, float dt, bool spinning)
        {
            const auto pitchAxis = XMVectorSet(1, 0, 0, 0);
            rotorPhase = spinning ? std::fmod(rotorPhase + dt * 25.f, XM_2PI) : 0.f;
            rotorCyclic = spinning
                ? rotorCyclic + (cyclic * .16f - rotorCyclic) * std::min(1.f, dt * 8.f)
                : 0.f;
            const XMVECTOR rotations[] = {
                XMQuaternionRotationAxis(pitchAxis, -rotorCyclic),
                XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), rotorPhase),
                XMQuaternionRotationAxis(pitchAxis, rotorPhase * 4.f),
            };
            for (size_t i = 0; i < rotorPivots.size(); ++i)
                if (auto* pose = scene.transforms.GetComponent(rotorPivots[i]))
                {
                    XMStoreFloat4(&pose->rotation_local, XMQuaternionMultiply(rotations[i], XMLoadFloat4(&rotorRest[i])));
                    pose->SetDirty();
                }
        }

        static bool IsReady(const wi::scene::Scene& scene, wi::ecs::Entity seat)
        {
            if (bridge::IsTrackedVehicle(scene, seat))
                return bridge::ReadTrackedVehicle(scene, seat).ready;
            return bridge::IsFlightVehicle(scene, seat)
                ? bridge::HasLiveFlightVehicle(scene, seat)
                : bridge::HasLiveVehiclePhysics(scene, seat);
        }

        static bool CanExit(wi::scene::Scene& scene, wi::ecs::Entity seat, float& speed)
        {
            if (bridge::IsTrackedVehicle(scene, seat))
            {
                const auto state = bridge::ReadTrackedVehicle(scene, seat);
                auto* body = scene.rigidbodies.GetComponent(seat);
                if (!state.ready || !body) return false;
                const auto velocity = wi::physics::GetVelocity(*body);
                speed = std::hypot(velocity.x, velocity.y, velocity.z);
                return state.contacts != 0;
            }
            if (!bridge::IsFlightVehicle(scene, seat))
                return bridge::GetVehicleForwardVelocity(scene, seat, speed);
            const auto* transform = scene.transforms.GetComponent(seat);
            auto* body = scene.rigidbodies.GetComponent(seat);
            if (!transform || !body || !body->physicsobject) return false;
            const auto velocity = wi::physics::GetVelocity(*body);
            speed = XMVectorGetX(XMVector3Length(XMLoadFloat3(&velocity)));
            if (bridge::IsFixedWing(scene, seat))
                return bridge::ReadFixedWing(scene, seat).groundContacts != 0;
            if (bridge::IsSpacecraft(scene, seat))
                return false;
            if (bridge::IsHelicopter(scene, seat))
                return bridge::IsHelicopterGrounded(scene, seat);
            if (bridge::IsPhysicalVtol(scene, seat))
                return bridge::IsPhysicalVtolGrounded(scene, seat);
            auto point = transform->GetPosition();
            point.x += 2.1f;
            const auto floor = wi::physics::Intersects(scene, wi::primitive::Ray(
                point, XMFLOAT3(0,-1,0), 0, .68f));
            return floor.IsValid() && floor.entity != seat && floor.normal.y > .6f;
        }

        void PilotFixedWing(wi::scene::Scene& scene, bridge::GameplayInputFrame& input, float dt)
        {
            auto& aircraft = fixedWings.at(current);
            const float step = std::clamp(dt, 0.f, .1f);
            const auto state = bridge::ReadFixedWing(scene, current);
            const auto throttleChange = input.player.moveForward * step * .52f;
            aircraft.throttle = std::clamp(aircraft.throttle + throttleChange, 0.f, 1.f);
            const auto rotation = wi::physics::GetRotation(*scene.rigidbodies.GetComponent(current));
            const auto controls = aircraft.SampleMouseControls(
                input.player.lookYaw, input.player.lookPitch, step, state.groundContacts > 0, rotation);
            const bool braking = input.aimDown || wi::input::Down(wi::input::KEYBOARD_BUTTON_SPACE);
            (void)bridge::CommandFixedWing(scene, current, controls, braking ? 1.f : 0.f);
            aircraft.UpdateVisuals(scene, current, controls, state, step);
            prompt = "W/S Power  Mouse Fly  RMB Brake  TAB " +
                std::string(CameraName()) + "  |  " + std::to_string(int(state.airspeed * 1.944f)) +
                " kt  " + std::to_string(int(aircraft.throttle * 100.f)) + "%";
            input.player = {};
            input.interactPressed = false;
        }

        void PilotFlight(wi::scene::Scene& scene, bridge::GameplayInputFrame& input, float dt)
        {
            const int lift = int(wi::input::Down(wi::input::KEYBOARD_BUTTON_SPACE));
            const int descend = int(wi::input::Down(static_cast<wi::input::BUTTON>('C')));
            const bridge::FlightVehicleInput flight{
                input.player.moveForward, input.player.moveRight, float(lift - descend)};
            if (bridge::IsSpacecraft(scene, current))
            {
                const auto axis = [](wi::input::BUTTON positive, wi::input::BUTTON negative) {
                    return float(int(wi::input::Down(positive)) - int(wi::input::Down(negative)));
                };
                const auto key = [](char value) { return static_cast<wi::input::BUTTON>(value); };
                const bridge::SpacecraftInput jets{flight.thrust, axis(key('X'), key('Z')), flight.climb,
                    axis(wi::input::KEYBOARD_BUTTON_UP, wi::input::KEYBOARD_BUTTON_DOWN),
                    flight.yaw, axis(key('R'), key('Q'))};
                (void)bridge::CommandSpacecraft(scene, current, jets);
                prompt = "W/S Thrust  Z/X Strafe  Space/C Lift  Arrows Pitch  A/D Yaw  Q/R Roll  TAB " +
                    std::string(CameraName());
                input.player = {};
                input.interactPressed = false;
                return;
            }
            if (bridge::IsHelicopter(scene, current))
            {
                (void)bridge::CommandHelicopter(scene, current, flight);
                UpdateRotors(scene, flight.thrust, std::clamp(dt, 0.f, .1f), true);
                prompt = "W/S Cyclic  A/D Pedals  Space/C Collective  E Exit when landed  TAB " +
                    std::string(CameraName());
                input.player = {};
                input.interactPressed = false;
                return;
            }
            if (bridge::GetPlayerVehicleMode(scene, current) == bridge::PlayerVehicleMode::ScriptedVTOL)
                scriptedFlight = {current, flight, dt};
            else if (bridge::IsPhysicalVtol(scene, current))
            {
                (void)bridge::CommandPhysicalVtol(scene, current, flight);
                auto& body = *scene.rigidbodies.GetComponent(current);
                const bool grounded = bridge::IsPhysicalVtolGrounded(scene, current);
                const float power = flight.climb > 0.f ? 1.f : grounded ? 0.f : flight.climb < 0.f ? .30f : .67f;
                vtolThrusters.Update(scene, wi::physics::GetVelocity(body), power, dt);
            }
            else
                bridge::DriveFlightVehicle(scene, current, flight, dt, altitudeTarget);

            prompt = "W/S Thrust   A/D Yaw   Space Rise   C Descend   E Exit when landed   TAB  " + std::string(CameraName());
            input.player = {};
            input.interactPressed = false;
        }

        std::uint64_t revision = 0;
        wi::ecs::Entity current = wi::ecs::INVALID_ENTITY;
        std::vector<wi::ecs::Entity> seats;
        enum class Camera { Chase, Cockpit, RearQuarter };
        Camera cameraMode = Camera::Chase;
        VehicleSkidTrail skidTrail;
        VanBrakeLights brakeLights;
        VehicleSmoke smoke;
        VtolThrusterEffects vtolThrusters;
        bridge::SteeringWheelSettings steeringWheel;
        XMFLOAT4 steeringWheelRest{0, 0, 0, 1};
        float steeringWheelAngle = 0.f;

        void BeginSteeringWheel(const wi::scene::Scene& scene)
        {
            steeringWheel = bridge::CaptureSteeringWheel(scene, current);
            steeringWheelAngle = 0.f;
            if (const auto* wheel = scene.transforms.GetComponent(steeringWheel.entity))
            {
                steeringWheelRest = wheel->rotation_local;
            }
        }

        void EndSteeringWheel(wi::scene::Scene& scene)
        {
            if (auto* wheel = scene.transforms.GetComponent(steeringWheel.entity))
            {
                wheel->rotation_local = steeringWheelRest;
                wheel->SetDirty();
            }
            steeringWheel = {};
        }

        void UpdateSteeringWheel(wi::scene::Scene& scene, const float dt)
        {
            if (steeringWheel.entity == wi::ecs::INVALID_ENTITY ||
                !bridge::CanBindSteeringWheel(scene, current, steeringWheel.entity))
                return;
            auto* wheel = scene.transforms.GetComponent(steeringWheel.entity);
            const auto* body = scene.rigidbodies.GetComponent(current);
            float angle = 0.f;
            if (!bridge::ReadFrontSteeringAngle(*body, angle))
                return;
            steeringWheelAngle = bridge::SmoothSteeringAngle(steeringWheelAngle, angle, dt);
            wheel->rotation_local = bridge::SteeringWheelRotation(
                steeringWheel, steeringWheelRest, steeringWheelAngle, body->vehicle.max_steering_angle);
            wheel->SetDirty();
        }
        std::vector<wi::ecs::Entity> cockpitGlass;

        void SetCockpitGlass(wi::scene::Scene& scene, bool hide)
        {
            for (auto entity : cockpitGlass)
                if (auto* object = scene.objects.GetComponent(entity))
                    object->SetRenderable(true);
            cockpitGlass.clear();
            if (!hide)
                return;

            cockpitGlass = bridge::FindCockpitGlass(scene, current);
            for (auto entity : cockpitGlass)
                scene.objects.GetComponent(entity)->SetRenderable(false);
        }
        std::string prompt;

        bool Active() const { return current != wi::ecs::INVALID_ENTITY; }

        void Sync(wi::scene::Scene& scene, std::uint64_t currentRevision)
        {
            if (revision == currentRevision)
                return;
            revision = currentRevision;
            current = wi::ecs::INVALID_ENTITY;
            seats.clear();
            fixedWings.clear();
            trackedVehicles.clear();
            cameraMode = Camera::Chase;
            skidTrail = {};
            brakeLights = {};
            smoke = {};
            vtolThrusters.Stop(scene);
            vtolThrusters = {};
            steeringWheel = {};
            cockpitGlass.clear();
            for (size_t i = 0; i < scene.metadatas.GetCount(); ++i)
            {
                const auto entity = scene.metadatas.GetEntity(i);
                const auto mode = bridge::GetPlayerVehicleMode(scene, entity);
                if (bridge::IsTrackedVehicle(scene, entity))
                {
                    RuntimeTrackedVehicle tracked;
                    if (tracked.Register(scene, entity))
                    {
                        trackedVehicles.emplace(entity, tracked);
                        seats.push_back(entity);
                    }
                    continue;
                }
                if (mode == bridge::PlayerVehicleMode::FixedWing)
                {
                    RuntimeFixedWing aircraft;
                    if (!aircraft.Register(scene, entity))
                    {
                        wi::backlog::post("Fixed Wing requires a contact marker or wheel for each gear position.",
                            wi::backlog::LogLevel::Warning);
                        continue;
                    }
                    fixedWings.emplace(entity, aircraft);
                }
                if (mode != bridge::PlayerVehicleMode::Disabled)
                    seats.push_back(entity);
            }
        }

        void Tick(wi::scene::Scene& scene, const bridge::RuntimePlayerState& player,
                  bridge::GameplayInputFrame& input, bool allowed, float dt)
        {
            prompt.clear();
            scriptedFlight = {};
            bastionShot.reset();
            if (Active() && !IsReady(scene, current))
            {
                SetCockpitGlass(scene, false);
                brakeLights.End(scene);
                smoke.Stop(scene);
                vtolThrusters.Stop(scene);
                EndSteeringWheel(scene);
                skidTrail.BreakTracks();
                current = wi::ecs::INVALID_ENTITY;
            }
            for (auto& [entity, tracked] : trackedVehicles)
                tracked.UpdateVisuals(scene, bridge::ReadTrackedVehicle(scene, entity), dt);
            if (!player.IsSpawned() || !allowed)
            {
                if (Active() && bridge::IsTrackedVehicle(scene, current))
                    (void)bridge::DriveTrackedVehicle(scene, current, 0.f, 0.f, 1.f);
                if (Active() && bridge::IsForceFlightVehicle(scene, current))
                {
                    if (bridge::IsSpacecraft(scene, current))
                        (void)bridge::CommandSpacecraft(scene, current, {});
                    else if (bridge::IsHelicopter(scene, current))
                        (void)bridge::CommandHelicopter(scene, current, {});
                    else
                        (void)bridge::CommandPhysicalVtol(scene, current, {});
                }
                return;
            }
            const auto* playerTransform = scene.transforms.GetComponent(player.entity);
            if (!playerTransform)
                return;
            if (!Active())
            {
                const XMFLOAT3 playerCoordinates = playerTransform->GetPosition();
                const XMVECTOR playerPosition = XMLoadFloat3(&playerCoordinates);
                auto nearest = wi::ecs::INVALID_ENTITY;
                float bestDistance = 16.0f;
                for (auto seat : seats)
                {
                    const auto* transform = scene.transforms.GetComponent(seat);
                    if (!transform || !IsReady(scene, seat))
                        continue;
                    const XMFLOAT3 position = transform->GetPosition();
                    const float distance = XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&position) - playerPosition));
                    if (distance < bestDistance)
                    {
                        bestDistance = distance;
                        nearest = seat;
                    }
                }
                if (nearest == wi::ecs::INVALID_ENTITY)
                    return;
                prompt = bridge::IsSpacecraft(scene, nearest) ? "E  Board spacecraft" :
                    bridge::IsHelicopter(scene, nearest) ? "E  Board helicopter" :
                    bridge::IsFixedWing(scene, nearest) ? "E  Board aircraft" :
                    bridge::IsFlightVehicle(scene, nearest) ? "E  Board VTOL" :
                    bridge::IsTrackedVehicle(scene, nearest) ? "E  Board tracked vehicle" : "E  Enter vehicle";
                if (!input.interactPressed)
                    return;
                if ((bridge::IsHelicopter(scene, nearest) && !bridge::CommandHelicopter(scene, nearest, {})) ||
                    (bridge::IsSpacecraft(scene, nearest) && !bridge::CommandSpacecraft(scene, nearest, {})))
                {
                    input.interactPressed = false;
                    prompt = "Missing flight rig markers";
                    return;
                }
                current = nearest;
                if (auto tracked = trackedVehicles.find(current); tracked != trackedVehicles.end())
                    tracked->second.turret.ResetAim();
                forceFlightPilotEye = wi::ecs::INVALID_ENTITY;
                if (bridge::IsForceFlightVehicle(scene, current))
                {
                    forceFlightPilotEye = scene.Entity_FindByName(
                        bridge::IsPhysicalVtol(scene, current) ? "REF_Pilot_Eye" : "REF_Eye_Pilot", current);
                    if (bridge::IsPhysicalVtol(scene, current)) vtolThrusters.Bind(scene, current);
                    if (bridge::IsHelicopter(scene, current)) BindRotors(scene);
                }
                if (bridge::IsFlightVehicle(scene, current))
                    altitudeTarget = scene.transforms.GetComponent(current)->GetPosition().y;
                else if (!bridge::IsTrackedVehicle(scene, current))
                {
                    brakeLights.Begin(scene, current);
                    BeginSteeringWheel(scene);
                }
                cameraMode = Camera::Chase;
                skidTrail.BreakTracks();
                input.interactPressed = false;
                wi::backlog::post("Vehicle Drive Test: player entered");
            }
            else if (input.interactPressed)
            {
                input.interactPressed = false;
                float speed = 0.0f;
                auto* playerBody = scene.rigidbodies.GetComponent(player.entity);
                auto* transform = scene.transforms.GetComponent(player.entity);
                const auto* chassis = scene.transforms.GetComponent(current);
                if (CanExit(scene, current, speed) &&
                    std::abs(speed) < 1.0f && playerBody && playerBody->physicsobject && transform && chassis)
                {
                    XMFLOAT3 position;
                    XMStoreFloat3(&position, XMVector3TransformCoord(XMVectorSet(-2.6f,0.35f,0,1), XMLoadFloat4x4(&chassis->world)));
                    if (bridge::IsForceFlightVehicle(scene, current))
                    {
                        const auto floor = wi::physics::Intersects(scene, wi::primitive::Ray(
                            position, XMFLOAT3(0, -1, 0), 0, 5.f));
                        if (floor.IsValid() && floor.entity != current && floor.normal.y > .6f)
                            position.y = floor.position.y + .37f;
                    }
                    std::string error;
                    if (bridge::RequireTerrainPropDestination(scene, player.entity, position, 1, error))
                    {
                        if (bridge::IsFixedWing(scene, current))
                        {
                            auto& aircraft = fixedWings.at(current);
                            aircraft.throttle = 0.f;
                            aircraft.pitchTarget = 0.f;
                            aircraft.mouseTurn = 0.f;
                            (void)bridge::CommandFixedWing(scene, current, {}, 1.f);
                            aircraft.UpdateVisuals(scene, current, {}, {}, 0.f);
                        }
                        else if (bridge::IsHelicopter(scene, current))
                        {
                            (void)bridge::CommandHelicopter(scene, current, {}, false);
                            UpdateRotors(scene, 0.f, 0.f, false);
                        }
                        else if (bridge::IsPhysicalVtol(scene, current))
                            (void)bridge::CommandPhysicalVtol(scene, current, {}, false);
                        else if (bridge::IsTrackedVehicle(scene, current))
                            (void)bridge::DriveTrackedVehicle(scene, current, 0.f, 0.f, 1.f);
                        else
                            (void)bridge::DrivePhysicsVehicle(scene, current, 0, 0, 1, 1);
                        transform->ClearTransform();
                        transform->Translate(position);
                        transform->UpdateTransform();
                        wi::physics::SetPosition(*playerBody, position);
                        current = wi::ecs::INVALID_ENTITY;
                        skidTrail.BreakTracks();
                        SetCockpitGlass(scene, false);
                        brakeLights.End(scene);
                        smoke.Stop(scene);
                        vtolThrusters.Stop(scene);
                        EndSteeringWheel(scene);
                        wi::backlog::post("Vehicle Drive Test: player exited");
                        return;
                    }
                }
            }

            float speed = 0.0f;
            (void)bridge::GetVehicleForwardVelocity(scene, current, speed);
            if (wi::input::Press(wi::input::KEYBOARD_BUTTON_TAB))
            {
                switch (cameraMode)
                {
                case Camera::Chase: cameraMode = Camera::Cockpit; break;
                case Camera::Cockpit: cameraMode = Camera::RearQuarter; break;
                case Camera::RearQuarter: cameraMode = Camera::Chase; break;
                }
                SetCockpitGlass(scene, cameraMode == Camera::Cockpit);
                wi::backlog::post(std::string("Vehicle camera: ") + CameraName());
            }
            if (bridge::IsFlightVehicle(scene, current))
            {
                if (bridge::IsFixedWing(scene, current))
                    PilotFixedWing(scene, input, dt);
                else
                    PilotFlight(scene, input, dt);
                return;
            }
            if (bridge::IsTrackedVehicle(scene, current))
            {
                const bool braking = wi::input::Down(wi::input::KEYBOARD_BUTTON_SPACE);
                const float forward = input.player.moveForward;
                const float turn = input.player.moveRight;
                const bool park = std::abs(forward) < .05f && std::abs(turn) < .05f;
                (void)bridge::DriveTrackedVehicle(scene, current, forward, turn, braking || park ? 1.f : 0.f);
                const auto velocity = wi::physics::GetVelocity(*scene.rigidbodies.GetComponent(current));
                const auto kph = static_cast<int>(std::hypot(velocity.x, velocity.z) * 3.6f);
                auto& turret = trackedVehicles.at(current).turret;
                if (turret.Bound())
                {
                    bastionShot = turret.Update(scene, current, input, dt);
                    prompt = std::string(turret.sight ? "Gunner  " : "") +
                        "WASD Drive  Mouse Aim  LMB Fire  RMB Sight  Tab View  |  " +
                        std::to_string(kph) + " km/h";
                    input.firePressed = input.fireDown = input.aimDown = false;
                }
                else
                    prompt = "W/S Drive   A/D Steer or pivot   Space Brake   E Exit   TAB " +
                        std::string(CameraName()) + "  |  " + std::to_string(kph) + " km/h";
                input.player = {};
                input.interactPressed = false;
                return;
            }
            skidTrail.Update(scene, current, dt, speed, input.player.moveRight);
            const bool braking = wi::input::Down(wi::input::KEYBOARD_BUTTON_SPACE);
            const float forward = input.player.moveForward;
            const bool brakingToReverse = speed > 1.0f && forward < 0.0f;
            const bool brakingToForward = speed < -1.0f && forward > 0.0f;
            const bool brake = braking || brakingToReverse || brakingToForward;
            brakeLights.Update(scene, speed, dt, brake);
            const float throttle = brake ? 0.f : forward;
            smoke.Update(scene, current, throttle, skidTrail.slipIntensity);
            (void)bridge::DrivePhysicsVehicle(scene, current, throttle,
                input.player.moveRight, brake ? 1.0f : 0.0f, 0.0f);
            UpdateSteeringWheel(scene, dt);
            prompt = "W/S  Drive    A/D  Steer    Space  Brake    E  Exit when stopped    " +
                std::to_string(static_cast<int>(std::abs(speed) * 3.6f)) + " km/h    TAB  " + CameraName();
            input.player = {};
            input.interactPressed = false;
        }

        const char* CameraName() const
        {
            switch (cameraMode)
            {
            case Camera::Cockpit: return "Cockpit";
            case Camera::RearQuarter: return "Rear quarter";
            case Camera::Chase: return "Chase";
            }
            return "Chase";
        }

        const RuntimeBastionTurret* ActiveTurret() const
        {
            const auto tracked = trackedVehicles.find(current);
            return tracked != trackedVehicles.end() && tracked->second.turret.aimInitialized
                ? &tracked->second.turret : nullptr;
        }

        struct PreviousProjection
        {
            wi::scene::CameraComponent* camera;
            float fov;
            bool ortho;
            bool custom;
            XMFLOAT4X4 projection;
        };
        std::optional<PreviousProjection> preCockpitProjection;

        void RestoreCockpitProjection(wi::scene::CameraComponent* camera)
        {
            if (preCockpitProjection && camera == preCockpitProjection->camera)
            {
                camera->fov = preCockpitProjection->fov;
                camera->SetOrtho(preCockpitProjection->ortho);
                camera->SetCustomProjectionEnabled(preCockpitProjection->custom);
                if (preCockpitProjection->custom) camera->Projection = preCockpitProjection->projection;
            }
            preCockpitProjection.reset();
        }

        bool ApplyCamera(const wi::scene::Scene& scene, wi::scene::CameraComponent& camera)
        {
            const auto* chassis = scene.transforms.GetComponent(current);
            if (!Active() || !chassis)
                return false;
            const XMMATRIX world = XMLoadFloat4x4(&chassis->world);
            const auto* turret = ActiveTurret();
            if (cameraMode == Camera::Cockpit && (!turret || !turret->sight))
            {
                const auto authored = bridge::ReadVehicleCockpitCamera(scene, current);
                wi::ecs::Entity marker = wi::ecs::INVALID_ENTITY;
                if (!authored)
                {
                    if (bridge::IsFixedWing(scene, current))
                        marker = fixedWings.at(current).parts[static_cast<size_t>(bridge::FixedWingPart::PilotEye)];
                    else if (bridge::IsForceFlightVehicle(scene, current))
                        marker = forceFlightPilotEye;
                    else if (bridge::IsTrackedVehicle(scene, current))
                        marker = trackedVehicles.at(current).driverView;
                }
                else
                {
                    preCockpitProjection = PreviousProjection{
                        &camera, camera.fov, camera.IsOrtho(), camera.IsCustomProjectionEnabled(), camera.Projection};
                    camera.SetOrtho(false);
                    camera.SetCustomProjectionEnabled(false);
                }
                const auto pose = authored ? *authored : bridge::DefaultVehicleCockpitCamera(
                    scene, current, marker, XMConvertToDegrees(camera.fov));
                bridge::ApplyVehicleCockpitCamera(camera, *chassis, pose, authored.has_value());
                return true;
            }
            XMVECTOR targetLocal = XMVectorSet(0,.75f,.60f,1);
            XMVECTOR eyeLocal = XMVectorSet(0,2.3f,-5.6f,1);
            XMVECTOR up = XMVectorSet(0,1,0,0);
            if (cameraMode == Camera::Cockpit)
                up = XMVector3Normalize(XMVector3TransformNormal(up,world));
            if (cameraMode == Camera::RearQuarter)
            {
                eyeLocal = XMVectorSet(3.6f,2.85f,-4.7f,1);
            }
            if (bridge::IsFixedWing(scene, current))
            {
                targetLocal = XMVectorSet(0, .5f, 2.2f, 1);
                eyeLocal = cameraMode == Camera::Chase ? XMVectorSet(0, 3.2f, -11.f, 1) :
                    XMVectorSet(5.8f, 4.f, -8.5f, 1);
            }
            if (bridge::IsForceFlightVehicle(scene, current))
            {
                targetLocal = XMVectorSet(0, .5f, 2.f, 1);
                eyeLocal = cameraMode == Camera::Chase ? XMVectorSet(0, 4.f, -14.f, 1) :
                    XMVectorSet(7.f, 4.f, -11.f, 1);
                if (bridge::IsSpacecraft(scene, current)) up = XMVector3Normalize(world.r[1]);
            }
            if (bridge::IsTrackedVehicle(scene, current))
            {
                eyeLocal = cameraMode == Camera::Chase ? XMVectorSet(0, 4.4f, -10.5f, 1) :
                    XMVectorSet(5.4f, 3.6f, -7.6f, 1);
                targetLocal = XMVectorSet(0, .35f, 1.7f, 1);
            }
            XMVECTOR target = XMVector3TransformCoord(targetLocal,world);
            XMVECTOR eye = XMVector3TransformCoord(eyeLocal,world);
            if (turret)
            {
                const auto focus = XMVector3TransformCoord(XMVectorSet(0, 1.85f, 0, 1), world);
                const auto aim = XMLoadFloat3(&turret->aimPoint);
                if (turret->sight)
                {
                    eye = focus;
                    target = aim;
                }
                else
                {
                    const auto direction = XMVector3Normalize(aim - focus);
                    const auto heading = XMVector3Normalize(XMVectorSetY(direction, 0));
                    const auto right = XMVector3Cross(up, heading);
                    const bool quarter = cameraMode == Camera::RearQuarter;
                    eye = focus - heading * (quarter ? 7.6f : 10.5f) +
                        up * (quarter ? 1.75f : 2.55f) + right * (quarter ? 5.4f : 0.f);
                    target = focus + direction * 8.f;
                }
            }
            wi::scene::TransformComponent pose;
            pose.MatrixTransform(XMMatrixInverse(nullptr, XMMatrixLookAtLH(eye,target,up)));
            pose.UpdateTransform();
            camera.TransformCamera(pose);
            camera.UpdateCamera();
            return true;
        }
    };
}
