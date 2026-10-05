#pragma once

#include "renegade/bridge/PlayerViewRig.h"
#include "renegade/bridge/FirearmSettings.h"

#include "renegade/bridge/AnimationService.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

namespace renegade::runtime
{
    struct RuntimePlayerViewAnimationClip
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        std::string name;
        std::string assemblyTrack;
    };

    struct RuntimePlayerViewAnimationState
    {
        std::array<std::vector<RuntimePlayerViewAnimationClip>, 16> clips;
        wi::ecs::Entity activeClip = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity outgoingClip = wi::ecs::INVALID_ENTITY;
        PlayerViewAction activeAction = PlayerViewAction::Idle;
        std::string resolvedClipName;
        float blendElapsed = 0.0f;
        float blendDuration = 0.20f;
        bool initialized = false;
        bool pairedAssembly = false;
        bool oneShotPlaying = false;
        bool aiming = false;
        bool equipped = true;
        bool groundKnown = false;
        bool wasGrounded = true;
        bool jumpCycleActive = false;
        bool takeoffPending = false;
        bool landingPending = false;
        bridge::FirearmSettings firearm;
        float shotCooldown = 0;
        // Loaded ammunition; reserve ammunition is not modelled yet.
        int loadedShells = 2;
        wi::ecs::Entity activeWeaponClip = wi::ecs::INVALID_ENTITY;
        float pairedTime = 0.0f;
        std::vector<wi::ecs::Entity> ownedAssemblyClips;
    };

    [[nodiscard]] inline std::size_t PlayerViewActionIndex(
        const PlayerViewAction action) noexcept
    {
        switch (action)
        {
        case PlayerViewAction::Charge: return 14;
        case PlayerViewAction::Release: return 15;
        case PlayerViewAction::Equip: return 9;
        case PlayerViewAction::Unequip: return 10;
        case PlayerViewAction::JumpStart: return 11;
        case PlayerViewAction::JumpLoop: return 12;
        case PlayerViewAction::JumpLand: return 13;
        case PlayerViewAction::AimIn: return 6;
        case PlayerViewAction::AimOut: return 7;
        case PlayerViewAction::AimAttack: return 8;
        case PlayerViewAction::Attack: return 3;
        case PlayerViewAction::Reload: return 4;
        case PlayerViewAction::ReloadPartial: return 5;
        case PlayerViewAction::Walk: return 1;
        case PlayerViewAction::Sprint: return 2;
        case PlayerViewAction::Idle:
        default:
            return 0;
        }
    }

    [[nodiscard]] inline std::string NormalizePlayerViewAnimationName(
        std::string value)
    {
        std::transform(
            value.begin(), value.end(), value.begin(),
            [](const unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        return value;
    }

    [[nodiscard]] inline bool PlayerViewAnimationNameContains(
        const std::string& value,
        const std::initializer_list<const char*> tokens) noexcept
    {
        for (const char* token : tokens)
        {
            if (value.find(token) != std::string::npos)
                return true;
        }
        return false;
    }

    [[nodiscard]] inline bool ResolvePlayerViewAnimationAction(
        const std::string& nativeName,
        PlayerViewAction& action)
    {
        const std::string name =
            NormalizePlayerViewAnimationName(nativeName);
        if (PlayerViewAnimationNameContains(
                name, {"run", "sprint", "jog"}))
        {
            action = PlayerViewAction::Sprint;
            return true;
        }
        if (PlayerViewAnimationNameContains(
                name, {"walk", "move", "locomotion", "strafe"}))
        {
            action = PlayerViewAction::Walk;
            return true;
        }
        if (PlayerViewAnimationNameContains(
                name, {"idle", "breath", "rest", "stand"}))
        {
            action = PlayerViewAction::Idle;
            return true;
        }
        return false;
    }

    [[nodiscard]] inline bool ResolveAuthoredPlayerViewAnimationAction(
        const std::string& authoredAction,
        PlayerViewAction& action) noexcept
    {
        if (authoredAction == "Equip" || authoredAction == "Unequip" ||
            authoredAction == "JumpStart" || authoredAction == "JumpLoop" || authoredAction == "JumpLand")
        {
            action = authoredAction == "Equip" ? PlayerViewAction::Equip :
                authoredAction == "Unequip" ? PlayerViewAction::Unequip :
                authoredAction == "JumpStart" ? PlayerViewAction::JumpStart :
                authoredAction == "JumpLoop" ? PlayerViewAction::JumpLoop : PlayerViewAction::JumpLand;
            return true;
        }
        if (authoredAction == "AimIn" || authoredAction == "AimOut" || authoredAction == "AimAttack")
        {
            action = authoredAction == "AimIn" ? PlayerViewAction::AimIn :
                authoredAction == "AimOut" ? PlayerViewAction::AimOut : PlayerViewAction::AimAttack;
            return true;
        }
        if (authoredAction == "Charge" || authoredAction == "Release")
        {
            action = authoredAction == "Charge" ? PlayerViewAction::Charge : PlayerViewAction::Release;
            return true;
        }
        if (authoredAction == "ReloadPartial")
        {
            action = PlayerViewAction::ReloadPartial;
            return true;
        }
        if (authoredAction == "Attack" || authoredAction == "Reload")
        {
            action = authoredAction == "Attack" ? PlayerViewAction::Attack : PlayerViewAction::Reload;
            return true;
        }
        if (authoredAction == "Idle")
        {
            action = PlayerViewAction::Idle;
            return true;
        }
        if (authoredAction == "Walk" ||
            authoredAction == "Locomotion")
        {
            action = PlayerViewAction::Walk;
            return true;
        }
        if (authoredAction == "Run" ||
            authoredAction == "Sprint")
        {
            action = PlayerViewAction::Sprint;
            return true;
        }
        return false;
    }

    [[nodiscard]] inline bool MatchingPlayerViewAnimationChannels(
        const wi::scene::AnimationComponent& first,
        const wi::scene::AnimationComponent& second) noexcept
    {
        if (first.channels.size() != second.channels.size())
            return false;

        for (const auto& channel : first.channels)
        {
            if (channel.GetPathDataType() ==
                wi::scene::AnimationComponent::AnimationChannel::
                    PathDataType::Event)
            {
                return false;
            }

            if (std::none_of(
                    second.channels.begin(),
                    second.channels.end(),
                    [&channel](const auto& other)
                    {
                        return other.target == channel.target &&
                            other.path == channel.path;
                    }))
            {
                return false;
            }
        }
        return true;
    }

    inline void ApplyPlayerViewAnimationBlend(
        wi::scene::Scene& scene,
        const RuntimePlayerViewAnimationState& state,
        const float activeWeight,
        const float outgoingWeight) noexcept
    {
        struct WeightedClip
        {
            wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
            float weight = 0.0f;
        };

        std::array<WeightedClip, 2> weighted = {{
            {state.activeClip, activeWeight},
            {state.outgoingClip, outgoingWeight},
        }};
        std::sort(
            weighted.begin(), weighted.end(),
            [&scene](const WeightedClip& left, const WeightedClip& right)
            {
                return scene.animations.GetIndex(left.entity) <
                    scene.animations.GetIndex(right.entity);
            });

        float cumulative = 0.0f;
        for (const auto& clip : weighted)
        {
            auto* animation = scene.animations.GetComponent(clip.entity);
            if (animation == nullptr)
                continue;

            cumulative += clip.weight;
            animation->amount =
                clip.weight > 0.0f && cumulative > 0.0f
                ? clip.weight / cumulative
                : 0.0f;
        }
    }

    inline void ResetRuntimePlayerViewAnimations(
        wi::scene::Scene& scene,
        RuntimePlayerViewAnimationState& state) noexcept
    {
        for (const auto& variants : state.clips)
        {
            for (const auto& clip : variants)
            {
                (void)bridge::StopAnimation(scene, clip.entity);
                if (auto* animation =
                        scene.animations.GetComponent(clip.entity))
                {
                    animation->amount = 0.0f;
                }
            }
        }
        for (const auto entity : state.ownedAssemblyClips)
        {
            if (auto* clip = scene.animations.GetComponent(entity))
            {
                clip->Pause(); clip->amount = 0.0f;
            }
        }
        state = {};
    }

    [[nodiscard]] inline bool InitializeRuntimePlayerViewAnimations(
        wi::scene::Scene& scene,
        const RuntimePlayerViewRigState& rig,
        RuntimePlayerViewAnimationState& state,
        std::string& error)
    {
        ResetRuntimePlayerViewAnimations(scene, state);
        error.clear();

        if (rig.viewModelRoot == wi::ecs::INVALID_ENTITY ||
            !scene.transforms.Contains(rig.viewModelRoot))
        {
            error =
                "Player View animation setup requires a loaded view-model root.";
            return false;
        }

        for (std::size_t i = 0; i < scene.metadatas.GetCount(); ++i)
        {
            const auto entity = scene.metadatas.GetEntity(i);
            const auto& metadata = scene.metadatas[i];
            if ((entity == rig.viewModelRoot || scene.Entity_IsDescendant(entity, rig.viewModelRoot)) &&
                metadata.bool_values.has("renegade.first_person.assembly") &&
                metadata.bool_values.get("renegade.first_person.assembly"))
            {
                if (state.pairedAssembly) { error = "Multiple first-person assembly roots."; return false; }
                if (!bridge::CaptureFirearmSettings(metadata, state.firearm, error)) return false;
                state.loadedShells = state.firearm.capacity;
                state.pairedAssembly = true;
            }
        }
        const auto available =
            bridge::CollectAnimationClips(scene, rig.viewModelRoot, true);
        const bool hasAuthoredActions = std::any_of(
            available.begin(), available.end(),
            [&scene](const auto& clip)
            {
                const auto* metadata =
                    scene.metadatas.GetComponent(clip.entity);
                return metadata != nullptr &&
                    metadata->string_values.has(
                        bridge::CreatorCharacterAnimationActionMetadataKey);
            });

        for (const auto& clip : available)
        {
            if (clip.entity == wi::ecs::INVALID_ENTITY)
                continue;

            std::string assemblyTrack;
            if (state.pairedAssembly)
            {
                auto* animation = scene.animations.GetComponent(clip.entity);
                if (animation == nullptr) continue;
                state.ownedAssemblyClips.push_back(clip.entity);
                animation->Pause(); animation->RootMotionOff(); animation->amount = 0.0f;
                const auto* metadata = scene.metadatas.GetComponent(clip.entity);
                if (metadata == nullptr || !metadata->string_values.has("renegade.first_person.assembly.track"))
                {
                    error = "Assembly animation is missing its explicit track role.";
                    ResetRuntimePlayerViewAnimations(scene, state); return false;
                }
                assemblyTrack = metadata->string_values.get("renegade.first_person.assembly.track");
                if ((assemblyTrack != "arms" && assemblyTrack != "weapon") ||
                    !std::isfinite(animation->start) || !std::isfinite(animation->end) ||
                    animation->end < animation->start ||
                    !std::isfinite(animation->end - animation->start))
                {
                    error = "Assembly animation has an invalid track role or timeline.";
                    ResetRuntimePlayerViewAnimations(scene, state); return false;
                }
            }
            PlayerViewAction action = PlayerViewAction::Idle;
            bool accepted = false;
            if (hasAuthoredActions)
            {
                const auto* metadata =
                    scene.metadatas.GetComponent(clip.entity);
                if (metadata != nullptr &&
                    metadata->string_values.has(
                        bridge::CreatorCharacterAnimationActionMetadataKey))
                {
                    accepted = ResolveAuthoredPlayerViewAnimationAction(
                        metadata->string_values.get(
                            bridge::CreatorCharacterAnimationActionMetadataKey),
                        action);
                }
            }
            else
            {
                accepted =
                    ResolvePlayerViewAnimationAction(clip.name, action);
            }

            if (!accepted)
                continue;

            auto* animation =
                scene.animations.GetComponent(clip.entity);
            if (animation == nullptr)
                continue;

            animation->RootMotionOff();
            animation->SetLooped(true);
            (void)bridge::StopAnimation(scene, clip.entity);
            animation->amount = 0.0f;

            state.clips[PlayerViewActionIndex(action)].push_back(
                {clip.entity, clip.name, assemblyTrack});
        }

        for (auto& variants : state.clips)
        {
            std::sort(
                variants.begin(), variants.end(),
                [](const RuntimePlayerViewAnimationClip& left,
                    const RuntimePlayerViewAnimationClip& right)
                {
                    if (left.name != right.name)
                        return left.name < right.name;
                    return left.entity < right.entity;
                });
        }

        if (state.pairedAssembly)
        {
            for (const auto& variants : state.clips)
            {
                if (variants.empty()) continue;
                const auto arms = std::count_if(variants.begin(), variants.end(),
                    [](const auto& clip) { return clip.assemblyTrack == "arms"; });
                const auto weapon = std::count_if(variants.begin(), variants.end(),
                    [](const auto& clip) { return clip.assemblyTrack == "weapon"; });
                if (variants.size() != 2 || arms != 1 || weapon != 1)
                {
                    error = "Each assembly action requires one arms and one weapon track.";
                    ResetRuntimePlayerViewAnimations(scene, state); return false;
                }
            }
            if (state.clips[0].empty())
            {
                error = "Runtime assembly requires an explicit Idle pair.";
                ResetRuntimePlayerViewAnimations(scene, state); return false;
            }
        }
        state.initialized = true;
        return true;
    }

    [[nodiscard]] inline const std::vector<RuntimePlayerViewAnimationClip>*
    ResolvePlayerViewAnimationVariants(
        const RuntimePlayerViewAnimationState& state,
        const PlayerViewAction requested) noexcept
    {
        const auto* variants =
            &state.clips[PlayerViewActionIndex(requested)];

        if (requested == PlayerViewAction::Sprint && variants->empty())
        {
            variants =
                &state.clips[PlayerViewActionIndex(PlayerViewAction::Walk)];
        }

        if (requested != PlayerViewAction::Idle && variants->empty())
        {
            variants =
                &state.clips[PlayerViewActionIndex(PlayerViewAction::Idle)];
        }
        return variants;
    }

    [[nodiscard]] inline bool RequestRuntimePlayerViewAnimation(
        wi::scene::Scene& scene,
        RuntimePlayerViewAnimationState& state,
        const PlayerViewAction requested) noexcept
    {
        if (!state.initialized)
            return false;

        const auto* variants =
            ResolvePlayerViewAnimationVariants(state, requested);
        if (variants == nullptr || variants->empty())
        {
            state.activeAction = requested;
            state.resolvedClipName.clear();
            return false;
        }

        if (state.pairedAssembly)
        {
            if (variants->size() != 2) return false;
            const auto arms = std::find_if(variants->begin(), variants->end(),
                [](const auto& clip) { return clip.assemblyTrack == "arms"; });
            const auto weapon = std::find_if(variants->begin(), variants->end(),
                [](const auto& clip) { return clip.assemblyTrack == "weapon"; });
            if (arms == variants->end() || weapon == variants->end() ||
                !scene.animations.Contains(arms->entity) || !scene.animations.Contains(weapon->entity))
                return false;
            const bool changed = state.activeClip != arms->entity || state.activeWeaponClip != weapon->entity;
            if (changed)
            {
                for (const auto entity : state.ownedAssemblyClips)
                    if (auto* clip = scene.animations.GetComponent(entity))
                    { clip->Pause(); clip->amount = 0.0f; }
                state.pairedTime = 0;
            }
            state.activeClip = arms->entity;
            state.activeWeaponClip = weapon->entity;
            state.activeAction = requested;
            state.resolvedClipName = arms->name + " + " + weapon->name;
            state.outgoingClip = wi::ecs::INVALID_ENTITY;
            state.blendDuration = 0;
            for (const auto entity : {state.activeClip, state.activeWeaponClip})
            {
                auto& clip = *scene.animations.GetComponent(entity);
                clip.Pause(); clip.RootMotionOff(); clip.SetLooped(false); clip.amount = 1.0f;
                if (changed) { clip.timer = clip.start; clip.last_update_time = -std::numeric_limits<float>::max(); }
            }
            return true;
        }
        const RuntimePlayerViewAnimationClip& next = variants->front();
        auto* nextAnimation =
            scene.animations.GetComponent(next.entity);
        if (nextAnimation == nullptr)
            return false;

        if (state.activeClip == next.entity)
        {
            state.activeAction = requested;
            state.resolvedClipName = next.name;
            if (!nextAnimation->IsPlaying())
            {
                nextAnimation->RootMotionOff();
                nextAnimation->SetLooped(true);
                nextAnimation->amount = 1.0f;
                return bridge::PlayAnimation(scene, next.entity, true);
            }
            return true;
        }

        wi::ecs::Entity outgoing = wi::ecs::INVALID_ENTITY;
        if (auto* active =
                scene.animations.GetComponent(state.activeClip))
        {
            if (active->IsPlaying() &&
                MatchingPlayerViewAnimationChannels(
                    *active, *nextAnimation))
            {
                outgoing = state.activeClip;
            }
            else
            {
                (void)bridge::StopAnimation(scene, state.activeClip);
                active->amount = 0.0f;
            }
        }

        if (state.outgoingClip != wi::ecs::INVALID_ENTITY &&
            state.outgoingClip != outgoing)
        {
            (void)bridge::StopAnimation(scene, state.outgoingClip);
            if (auto* prior =
                    scene.animations.GetComponent(state.outgoingClip))
            {
                prior->amount = 0.0f;
            }
        }

        state.outgoingClip = outgoing;
        state.activeClip = next.entity;
        state.activeAction = requested;
        state.resolvedClipName = next.name;
        state.blendElapsed = 0.0f;
        state.blendDuration =
            outgoing == wi::ecs::INVALID_ENTITY ? 0.0f : 0.20f;

        nextAnimation->RootMotionOff();
        nextAnimation->SetLooped(true);
        nextAnimation->amount =
            outgoing == wi::ecs::INVALID_ENTITY ? 1.0f : 0.0f;
        if (!bridge::PlayAnimation(scene, next.entity, true))
            return false;

        if (state.blendDuration > 0.0f)
        {
            ApplyPlayerViewAnimationBlend(
                scene, state, 0.0f, 1.0f);
        }
        return true;
    }

    inline void UpdateRuntimePlayerViewAnimations(
        wi::scene::Scene& scene,
        RuntimePlayerViewAnimationState& state,
        const PlayerViewAction requested,
        const float dt,
        const bool firePressed = false,
        const bool reloadPressed = false,
        const bool aimDown = false,
        const bool toggleEquipmentPressed = false,
        const bool grounded = true,
        const bool chargeHeld = false,
        const bool releasePressed = false) noexcept
    {
        if (!state.initialized)
            return;

        if (state.pairedAssembly)
        {
            // A discrete action owns both tracks until its shared duration finishes.
            // Ignore new presses while busy; missing action pairs never play Idle as a shot.
            const bool advancing = std::isfinite(dt) && dt > 0;
            if (advancing)
            {
                state.shotCooldown = std::max(0.0f, state.shotCooldown - dt);
                if (state.groundKnown)
                {
                    if (state.wasGrounded && !grounded)
                    { state.takeoffPending = true; state.jumpCycleActive = true; }
                    if (!state.wasGrounded && grounded && state.jumpCycleActive) state.landingPending = true;
                }
                state.wasGrounded = grounded;
                state.groundKnown = true;
            }
            PlayerViewAction next = !state.equipped ? PlayerViewAction::Unequip :
                state.aiming ? PlayerViewAction::AimIn : requested;
            const auto startAction = [&](const PlayerViewAction action) {
                if (state.clips[PlayerViewActionIndex(action)].empty()) return false;
                next = action;
                state.activeClip = wi::ecs::INVALID_ENTITY;
                state.oneShotPlaying = true;
                return true;
            };
            if (state.oneShotPlaying)
                next = state.activeAction;
            else if (advancing && state.equipped && releasePressed)
            {
                if (startAction(PlayerViewAction::Release)) state.aiming = false;
            }
            else if (advancing && toggleEquipmentPressed)
            {
                if (startAction(state.equipped ? PlayerViewAction::Unequip : PlayerViewAction::Equip))
                    state.aiming = false;
            }
            else if (advancing && state.equipped && state.landingPending)
            {
                state.landingPending = false;
                state.takeoffPending = false;
                if (startAction(PlayerViewAction::JumpLand)) state.aiming = false;
                else state.jumpCycleActive = false;
            }
            else if (advancing && state.equipped && state.takeoffPending)
            {
                state.takeoffPending = false;
                if (startAction(PlayerViewAction::JumpStart)) state.aiming = false;
            }
            else if (state.equipped && state.jumpCycleActive && !grounded && !state.clips[PlayerViewActionIndex(PlayerViewAction::JumpLoop)].empty())
            {
                next = PlayerViewAction::JumpLoop;
                state.aiming = false;
            }
            else if (state.equipped && chargeHeld && !state.clips[PlayerViewActionIndex(PlayerViewAction::Charge)].empty())
            {
                next = PlayerViewAction::Charge;
                state.aiming = false;
            }
            else if (advancing && state.equipped && (reloadPressed || firePressed))
            {
                const auto action = reloadPressed
                    ? (state.loadedShells > 0 && state.firearm.allowPartialReload ? PlayerViewAction::ReloadPartial : PlayerViewAction::Reload)
                    : (state.aiming ? PlayerViewAction::AimAttack : PlayerViewAction::Attack);
                const bool allowed = reloadPressed ? (state.loadedShells < state.firearm.capacity &&
                    (state.loadedShells == 0 || state.firearm.allowPartialReload)) :
                    (state.loadedShells > 0 && state.shotCooldown <= 0);
                // A missing partial pair may use the authored full reload, but
                // a missing fire pair must never consume ammunition.
                auto selected = action;
                if (selected == PlayerViewAction::AimAttack &&
                    state.clips[PlayerViewActionIndex(selected)].empty())
                    selected = PlayerViewAction::Attack;
                if (selected == PlayerViewAction::ReloadPartial &&
                    state.clips[PlayerViewActionIndex(selected)].empty())
                    selected = PlayerViewAction::Reload;
                if (allowed && !state.clips[PlayerViewActionIndex(selected)].empty())
                {
                    next = selected;
                    if (reloadPressed) state.aiming = false;
                    state.activeClip = wi::ecs::INVALID_ENTITY;
                    state.oneShotPlaying = true;
                }
            }
            // Reconcile hold/release after a busy action. Never interrupt paired tracks.
            if (!state.oneShotPlaying && advancing && state.equipped && !state.jumpCycleActive && !chargeHeld && aimDown != state.aiming)
            {
                const auto transition = aimDown ? PlayerViewAction::AimIn : PlayerViewAction::AimOut;
                if (!state.clips[PlayerViewActionIndex(transition)].empty())
                {
                    next = transition;
                    state.activeClip = wi::ecs::INVALID_ENTITY;
                    state.oneShotPlaying = true;
                }
                else if (!aimDown)
                {
                    state.aiming = false;
                    next = requested;
                }
            }
            const bool startingShot = state.oneShotPlaying &&
                state.activeClip == wi::ecs::INVALID_ENTITY &&
                (next == PlayerViewAction::Attack || next == PlayerViewAction::AimAttack);
            if (!RequestRuntimePlayerViewAnimation(scene, state, next))
            {
                state.oneShotPlaying = false;
                return;
            }
            if (startingShot) {
                --state.loadedShells;
                state.shotCooldown = state.firearm.minimumShotInterval;
            }
            auto* arms = scene.animations.GetComponent(state.activeClip);
            auto* weapon = scene.animations.GetComponent(state.activeWeaponClip);
            if (arms == nullptr || weapon == nullptr) return;
            const float duration = std::max(arms->end - arms->start, weapon->end - weapon->start);
            if (std::isfinite(dt) && dt > 0 && duration > 0)
            {
                if (state.oneShotPlaying)
                {
                    state.pairedTime = std::min(state.pairedTime + dt, duration);
                    if (state.pairedTime >= duration)
                    {
                        if (state.activeAction == PlayerViewAction::Reload ||
                            state.activeAction == PlayerViewAction::ReloadPartial)
                            state.loadedShells = state.firearm.capacity;
                        if (state.activeAction == PlayerViewAction::JumpLand)
                            state.jumpCycleActive = false;
                        if (state.activeAction == PlayerViewAction::Equip)
                        {
                            state.equipped = true;
                            state.takeoffPending = false;
                            state.landingPending = false;
                        }
                        else if (state.activeAction == PlayerViewAction::Unequip)
                        {
                            state.equipped = false;
                            state.takeoffPending = false;
                            state.landingPending = false;
                        }
                        if (state.activeAction == PlayerViewAction::AimIn)
                            state.aiming = true;
                        else if (state.activeAction == PlayerViewAction::AimOut)
                            state.aiming = false;
                        state.oneShotPlaying = false;
                    }
                }
                else if (chargeHeld && next == PlayerViewAction::Charge)
                    state.pairedTime = std::min(state.pairedTime + dt, duration);
                else if ((!state.equipped && next == PlayerViewAction::Unequip) ||
                    (state.aiming && next == PlayerViewAction::AimIn))
                    state.pairedTime = duration; // Hold the authored sight pose, do not loop aim-in.
                else
                    state.pairedTime = std::fmod(state.pairedTime + dt, duration);
            }
            else if (duration <= 0)
                state.oneShotPlaying = false;
            // Wicked evaluates native channels during the normal Scene update.
            // Paused tracks prevent a second timer advance. Short tracks hold.
            for (auto* clip : {arms, weapon})
            {
                clip->timer = std::clamp(clip->start + state.pairedTime, clip->start, clip->end);
                clip->last_update_time = -std::numeric_limits<float>::max();
            }
            return;
        }
        auto* active =
            scene.animations.GetComponent(state.activeClip);
        if (state.activeClip == wi::ecs::INVALID_ENTITY ||
            state.activeAction != requested ||
            active == nullptr || !active->IsPlaying())
        {
            (void)RequestRuntimePlayerViewAnimation(
                scene, state, requested);
        }

        if (state.blendDuration <= 0.0f ||
            state.outgoingClip == wi::ecs::INVALID_ENTITY)
        {
            return;
        }

        if (std::isfinite(dt) && dt > 0.0f)
            state.blendElapsed += dt;

        const float t = std::clamp(
            state.blendElapsed / state.blendDuration,
            0.0f, 1.0f);
        ApplyPlayerViewAnimationBlend(
            scene, state, t, 1.0f - t);

        if (t >= 1.0f)
        {
            (void)bridge::StopAnimation(
                scene, state.outgoingClip);
            if (auto* outgoingAnimation =
                    scene.animations.GetComponent(state.outgoingClip))
            {
                outgoingAnimation->amount = 0.0f;
            }
            state.outgoingClip = wi::ecs::INVALID_ENTITY;
            state.blendElapsed = 0.0f;
            state.blendDuration = 0.20f;
            if (auto* current =
                    scene.animations.GetComponent(state.activeClip))
            {
                current->amount = 1.0f;
            }
        }
    }
}
