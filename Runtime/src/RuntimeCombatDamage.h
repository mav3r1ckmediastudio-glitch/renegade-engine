#pragma once

#include "RuntimeCombatService.h"

#include <cmath>
#include <string>

namespace renegade::runtime
{
    // Reusable external-damage seam for later player weapons/scripts/Smart
    // Objects. Health and DamagedBy cognition are updated together from the
    // legitimate source information supplied by the damage producer; this
    // helper never looks up a hidden attacker transform.
    [[nodiscard]] inline bool ApplyAttributedCombatDamage(
        wi::scene::Scene& scene,
        const RuntimeCharacterSystemState& characters,
        RuntimeCharacterPerceptionState& perception,
        RuntimeCombatState& combatState,
        const bridge::StableId& damagedCharacterId,
        const std::string& sourceSubjectId,
        const std::string& sourceFactionId,
        const XMFLOAT3& legitimateKnownPosition,
        const XMFLOAT3& legitimateKnownVelocity,
        const float damageAmount,
        const CombatEventEmitter& emitter,
        std::string& error)
    {
        if (!std::isfinite(damageAmount) || damageAmount <= 0.0f)
        {
            error = "Attributed combat damage must be a finite positive amount.";
            return false;
        }
        const auto* character = FindRuntimeCharacter(characters, damagedCharacterId);
        auto* combat = FindCharacterCombat(combatState, damagedCharacterId);
        if (character == nullptr || combat == nullptr || combat->dead)
        {
            error = "Attributed combat damage target is not an active Runtime Character.";
            return false;
        }

        // ReportDamageStimulus validates source identity/faction/known vectors
        // before mutation, so a malformed damage producer cannot change health.
        std::string perceptionError;
        if (!ReportDamageStimulus(
                characters,
                perception,
                damagedCharacterId,
                sourceSubjectId,
                sourceFactionId,
                legitimateKnownPosition,
                legitimateKnownVelocity,
                damageAmount,
                perceptionError))
        {
            error = perceptionError;
            return false;
        }

        bool died = false;
        if (!ApplyCombatDamage(
                scene,
                combatState,
                damagedCharacterId,
                damageAmount,
                died))
        {
            error = "Attributed combat damage could not update target health.";
            return false;
        }

        const std::string payload =
            "damage=" + std::to_string(damageAmount) +
            ";health=" + std::to_string(combat->health) +
            ";dead=" + std::string(died ? "1" : "0");
        (void)EmitCombatGameplayEvent(
            combatState,
            emitter,
            {0, "ai.damage", payload, sourceSubjectId, damagedCharacterId});
        error.clear();
        return true;
    }
}
