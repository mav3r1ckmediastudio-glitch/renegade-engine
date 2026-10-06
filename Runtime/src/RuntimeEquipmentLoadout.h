#pragma once

#include <algorithm>
#include "renegade/bridge/EquipmentAssetService.h"
#include "renegade/bridge/GameplayInputService.h"
#include "renegade/bridge/PlayerService.h"

namespace renegade::runtime
{
    // Runtime-owned resolved assets. Scene settings remain immutable authoring data.
    struct RuntimeEquipmentLoadout
    {
        bridge::EquipmentAssetDocument primary, offHand;
        bridge::EquipmentActionState actions;
        bool dispatched = false;
        bool chargePresentation = false, releasePresentation = false;
        bool offHandBlockPresentation = false;
        struct QueuedStrike {
            bool pending=false, released=false;
            unsigned direction=0;
            float seconds=0, age=0, gestureX=0, gestureY=0;
            std::string item;
        } queuedStrike;
        bool chainedChargeStarted=false, chainedReleasedCharge=false, chainInputWindow=false;
        unsigned chainedDirection=0, chainedCount=0;
        float chainedSeconds=0;
        bool authored = false;
        bool ready = false;
        std::string error;

        bool Load(const std::string& root, const bridge::StableId& project,
            const bridge::PlayerControllerSettings& settings)
        {
            *this = {};
            authored = !settings.primaryEquipmentAssetId.empty() ||
                !settings.offHandEquipmentAssetId.empty();
            if (!authored) { ready = true; return true; }
            if (!bridge::ValidateStartingEquipment(root, project,
                    settings.primaryEquipmentAssetId, settings.offHandEquipmentAssetId, error))
                return false;
            bridge::EquipmentAssetDocument p, o;
            if (!settings.primaryEquipmentAssetId.empty() &&
                !bridge::LoadEquipmentAsset(root, project, settings.primaryEquipmentAssetId, p, error))
                return false;
            if (!settings.offHandEquipmentAssetId.empty() &&
                !bridge::LoadEquipmentAsset(root, project, settings.offHandEquipmentAssetId, o, error))
                return false;
            primary = std::move(p);
            offHand = std::move(o);
            ready = true;
            return true;
        }

        bridge::StableId Presentation(const bridge::StableId& legacy) const
        {
            if (!authored) return legacy;
            return ready ? primary.equipment.presentationAssetId : bridge::StableId{};
        }

        bool Allows(bridge::EquipmentAction action, const char* semantic) const
        {
            if (!authored) return true; // Accepted legacy assemblies retain their path.
            if (!ready || primary.equipment.assetId.empty()) return false;
            for (const auto& definition : primary.equipment.actions)
            {
                if (definition.action != action) continue;
                // This adapter admits the immediate paired-animation subset.
                // Staged/held actions require the subsequent action-state adapter.
                return definition.animationAction == semantic &&
                    definition.prepareSeconds == 0 && definition.windupSeconds == 0 &&
                    definition.recoverySeconds == 0 && !definition.holdUntilRelease && !definition.activeWhileHeld;
            }
            return false;
        }

        bool HasChargeRelease() const
        {
            const bridge::EquipmentActionDefinition* charge = nullptr;
            const bridge::EquipmentActionDefinition* release = nullptr;
            for (const auto& action : primary.equipment.actions) {
                if (action.action == bridge::EquipmentAction::PrimaryUse) return false;
                if (action.action == bridge::EquipmentAction::Charge) charge = &action;
                if (action.action == bridge::EquipmentAction::Release) release = &action;
            }
            return charge && release && charge->animationAction == "Charge" &&
                charge->holdUntilRelease && !charge->activeWhileHeld && !release->activeWhileHeld && release->animationAction == "Release" &&
                release->prepareSeconds == 0 && release->windupSeconds == 0 &&
                !release->holdUntilRelease;
        }

        void RefreshChargePresentation()
        {
            chargePresentation = false;
            for (const auto& channel : actions.Channels())
                if (channel.ownerHand == bridge::EquipmentHand::Primary &&
                    channel.definition.action == bridge::EquipmentAction::Charge &&
                    channel.phase != bridge::EquipmentActionPhase::Ready &&
                    channel.phase != bridge::EquipmentActionPhase::Active &&
                    channel.phase != bridge::EquipmentActionPhase::Recovery)
                    chargePresentation = true;
        }

        void RefreshOffHandBlockPresentation()
        {
            offHandBlockPresentation = false;
            for(const auto& channel:actions.Channels())
                if(channel.ownerHand==bridge::EquipmentHand::OffHand &&
                    channel.definition.action==bridge::EquipmentAction::Block &&
                    channel.phase==bridge::EquipmentActionPhase::Active)
                    offHandBlockPresentation=true;
        }

        bridge::GameplayInputFrame RouteStaged(bridge::GameplayInputFrame input,
            bool equipped, bool nativeBusy, float dt, bool currentAim = false, bool chargePairsAvailable = false, bool offHandBlockAvailable = false, bool directionalMelee = false, bool chainWindow = false, unsigned direction = 0)
        {
            releasePresentation = false;
            chainedChargeStarted = false;
            RefreshChargePresentation();
            RefreshOffHandBlockPresentation();
            if (!authored) return input;
            auto output = Route(input, equipped);
            output.firePressed = output.reloadPressed = output.toggleEquipmentPressed = false;
            if (!ready || !std::isfinite(dt) || dt <= 0) return output;
            bool releaseOwned=false, recoveryOwned=false;
            for(const auto& c:actions.Channels())
                if(c.ownerHand==bridge::EquipmentHand::Primary && c.definition.action==bridge::EquipmentAction::Release &&
                   c.phase!=bridge::EquipmentActionPhase::Ready) {
                    releaseOwned=true;
                    recoveryOwned=c.phase==bridge::EquipmentActionPhase::Recovery || (dispatched && !nativeBusy);
                }
            chainInputWindow=directionalMelee && releaseOwned && (chainWindow || recoveryOwned);
            const bool chainsAvailable=directionalMelee && chargePairsAvailable && HasChargeRelease() && equipped;
            if(!chainsAvailable || (queuedStrike.pending && queuedStrike.item!=primary.equipment.assetId) ||
               input.cancelEquipmentPressed || input.reloadPressed || input.toggleEquipmentPressed)
                {queuedStrike={};chainedReleasedCharge=false;}
            if(chainsAvailable && releaseOwned && (chainWindow || recoveryOwned) &&
               input.firePressed && !queuedStrike.pending && !input.cancelEquipmentPressed &&
               !input.reloadPressed && !input.toggleEquipmentPressed) {
                queuedStrike={};queuedStrike.pending=true;
                queuedStrike.direction=std::min(direction,3u);queuedStrike.item=primary.equipment.assetId;
            }
            if(queuedStrike.pending) {
                if(!queuedStrike.released) {
                    queuedStrike.seconds=std::min(queuedStrike.seconds+dt,1.0f);
                    if(std::isfinite(input.player.lookYaw)&&std::isfinite(input.player.lookPitch)) {
                        queuedStrike.gestureX+=input.player.lookYaw;
                        queuedStrike.gestureY+=input.player.lookPitch;
                        if(std::max(std::abs(queuedStrike.gestureX),std::abs(queuedStrike.gestureY))>=0.025f) {
                            queuedStrike.direction=std::abs(queuedStrike.gestureX)>=std::abs(queuedStrike.gestureY)?
                                (queuedStrike.gestureX<0?0:1):(queuedStrike.gestureY>0?2:3);
                            queuedStrike.gestureX=queuedStrike.gestureY=0;
                        }
                    }
                    queuedStrike.released=!input.fireDown;
                }
                if(queuedStrike.released) {
                    queuedStrike.age+=dt;
                    if(queuedStrike.age>0.75f)queuedStrike={};
                }
            }
            if (dispatched && !nativeBusy) {
                actions.CompleteActive(primary.equipment.assetId, bridge::EquipmentHand::Primary);
                dispatched = false;
            }
            // Consume a press only when both gameplay and native presentation are free.
            // Existing native animation retains ammo, jump and paired completion rules.
            if (input.cancelEquipmentPressed) {
                actions.Cancel(primary.equipment.assetId, bridge::EquipmentHand::Primary);
                actions.Cancel(offHand.equipment.assetId, bridge::EquipmentHand::OffHand);
            }
            // Enable held block only for a loaded, compatible independent-hand presentation.
            if(offHandBlockAvailable && !input.cancelEquipmentPressed && input.offHandUsePressed &&
                !(actions.ReservedHands()&2) && !offHand.equipment.assetId.empty()) {
                for(const auto& definition:offHand.equipment.actions)
                    if(definition.action==bridge::EquipmentAction::Block &&
                        definition.animationAction=="Block" && definition.activeWhileHeld)
                        actions.Begin(offHand.equipment,bridge::EquipmentAction::Block,bridge::EquipmentHand::OffHand);
            }
            if(!input.offHandUseDown) actions.Release(offHand.equipment.assetId,bridge::EquipmentHand::OffHand);
            if (!input.cancelEquipmentPressed && !nativeBusy && !(actions.ReservedHands() & 1)) {
                const auto begin = [&](bridge::EquipmentAction action, const char* semantic) {
                    auto item = primary.equipment;
                    for (const auto& definition : item.actions)
                        if (definition.action == action && definition.animationAction == semantic && !definition.activeWhileHeld &&
                            (!definition.holdUntilRelease || action == bridge::EquipmentAction::PrimaryUse || action == bridge::EquipmentAction::Charge))
                            return actions.Begin(item, action, bridge::EquipmentHand::Primary);
                    return false;
                };
                if (input.toggleEquipmentPressed)
                    begin(equipped ? bridge::EquipmentAction::Unequip : bridge::EquipmentAction::Equip,
                        equipped ? "Unequip" : "Equip");
                else if (equipped && input.reloadPressed)
                    begin(bridge::EquipmentAction::Reload, "Reload");
                else if (equipped && (input.firePressed || queuedStrike.pending)) {
                    if (chargePairsAvailable && HasChargeRelease()) {
                        if(begin(bridge::EquipmentAction::Charge, "Charge")) {
                            chainedReleasedCharge=queuedStrike.pending && queuedStrike.released;
                            if(queuedStrike.pending) {
                            chainedChargeStarted=true;chainedDirection=queuedStrike.direction;
                            chainedSeconds=queuedStrike.seconds;++chainedCount;queuedStrike={};
                            }
                        }
                    }
                    else begin(bridge::EquipmentAction::PrimaryUse, "Attack");
                }
            }
            if (!input.fireDown) actions.Release(primary.equipment.assetId, bridge::EquipmentHand::Primary);
            if (actions.ReservedHands() & 1) output.aimDown = currentAim;
            if(offHandBlockAvailable && !offHand.equipment.assetId.empty()) output.aimDown=false;
            // A jump/aim pair that began during preparation must finish before dispatch.
            actions.Update(dt, true, nativeBusy && !dispatched ? 1 : 0);
            auto events = actions.TakeEvents();
            for (const auto& event : events)
                if (event.ownerHand == bridge::EquipmentHand::Primary &&
                    event.phase == bridge::EquipmentActionPhase::Active &&
                    event.action == bridge::EquipmentAction::Charge)
                    actions.RetargetActive(primary.equipment, bridge::EquipmentAction::Release, bridge::EquipmentHand::Primary);
            auto releaseEvents = actions.TakeEvents();
            events.insert(events.end(), releaseEvents.begin(), releaseEvents.end());
            for (const auto& event : events) {
                if (event.ownerHand != bridge::EquipmentHand::Primary ||
                    event.phase != bridge::EquipmentActionPhase::Active) continue;
                if (event.action == bridge::EquipmentAction::Charge) continue;
                dispatched = true;
                releasePresentation = event.action == bridge::EquipmentAction::Release;
                output.firePressed = event.action == bridge::EquipmentAction::PrimaryUse;
                output.reloadPressed = event.action == bridge::EquipmentAction::Reload;
                output.toggleEquipmentPressed = event.action == bridge::EquipmentAction::Equip ||
                    event.action == bridge::EquipmentAction::Unequip;
            }
            RefreshChargePresentation();
            RefreshOffHandBlockPresentation();
            return output;
        }

        bridge::GameplayInputFrame Route(bridge::GameplayInputFrame input, bool equipped) const
        {
            input.firePressed &= Allows(bridge::EquipmentAction::PrimaryUse, "Attack");
            input.reloadPressed &= Allows(bridge::EquipmentAction::Reload, "Reload");
            input.aimDown &= Allows(bridge::EquipmentAction::AlternateUse, "AimIn");
            input.toggleEquipmentPressed &= equipped
                ? Allows(bridge::EquipmentAction::Unequip, "Unequip")
                : Allows(bridge::EquipmentAction::Equip, "Equip");
            return input;
        }
    };
}
