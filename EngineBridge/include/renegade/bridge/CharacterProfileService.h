#pragma once

#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/FactionService.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

namespace renegade::bridge
{
    inline constexpr int CharacterProfileRegistryVersion = 1;
    inline constexpr int CharacterAdvancedSchemaVersion = 1;
    inline constexpr const char* CharacterAdvancedMetadataKey =
        CharacterAdvancedPayloadMetadataKey;

    struct CharacterTuning
    {
        float visionDistance = 35.0f;
        float peripheralVisionDistance = 15.0f;
        float horizontalFovDegrees = 100.0f;
        float verticalFovDegrees = 70.0f;
        float hearingSensitivity = 1.0f;
        float visualReactionSeconds = 0.35f;
        float audioReactionSeconds = 0.30f;

        float memorySeconds = 12.0f;
        float suspicionGain = 20.0f;
        float suspicionDecay = 4.0f;
        float alertThreshold = 35.0f;
        float combatThreshold = 70.0f;

        float aggression = 0.50f;
        float courage = 0.50f;
        float curiosity = 0.50f;
        float alertness = 0.50f;
        float loyalty = 0.50f;
        float accuracy = 0.50f;

        float preferredCombatRange = 15.0f;
        float minCombatRange = 3.0f;
        float maxCombatRange = 35.0f;

        float coverPreference = 0.50f;
        float retreatHealthThreshold = 0.25f;
        float pursuitSeconds = 10.0f;
        float searchSeconds = 12.0f;
        float communicationRange = 25.0f;
        float suppressionTolerance = 0.50f;
    };

    struct CharacterAdvancedOverrides
    {
        std::optional<float> visionDistance;
        std::optional<float> peripheralVisionDistance;
        std::optional<float> horizontalFovDegrees;
        std::optional<float> verticalFovDegrees;
        std::optional<float> hearingSensitivity;
        std::optional<float> visualReactionSeconds;
        std::optional<float> audioReactionSeconds;
        std::optional<float> memorySeconds;
        std::optional<float> suspicionGain;
        std::optional<float> suspicionDecay;
        std::optional<float> alertThreshold;
        std::optional<float> combatThreshold;
        std::optional<float> aggression;
        std::optional<float> courage;
        std::optional<float> curiosity;
        std::optional<float> alertness;
        std::optional<float> loyalty;
        std::optional<float> accuracy;
        std::optional<float> preferredCombatRange;
        std::optional<float> minCombatRange;
        std::optional<float> maxCombatRange;
        std::optional<float> coverPreference;
        std::optional<float> retreatHealthThreshold;
        std::optional<float> pursuitSeconds;
        std::optional<float> searchSeconds;
        std::optional<float> communicationRange;
        std::optional<float> suppressionTolerance;
    };

    [[nodiscard]] inline bool operator==(
        const CharacterAdvancedOverrides& lhs,
        const CharacterAdvancedOverrides& rhs) noexcept
    {
        return lhs.visionDistance == rhs.visionDistance &&
            lhs.peripheralVisionDistance == rhs.peripheralVisionDistance &&
            lhs.horizontalFovDegrees == rhs.horizontalFovDegrees &&
            lhs.verticalFovDegrees == rhs.verticalFovDegrees &&
            lhs.hearingSensitivity == rhs.hearingSensitivity &&
            lhs.visualReactionSeconds == rhs.visualReactionSeconds &&
            lhs.audioReactionSeconds == rhs.audioReactionSeconds &&
            lhs.memorySeconds == rhs.memorySeconds &&
            lhs.suspicionGain == rhs.suspicionGain &&
            lhs.suspicionDecay == rhs.suspicionDecay &&
            lhs.alertThreshold == rhs.alertThreshold &&
            lhs.combatThreshold == rhs.combatThreshold &&
            lhs.aggression == rhs.aggression && lhs.courage == rhs.courage &&
            lhs.curiosity == rhs.curiosity && lhs.alertness == rhs.alertness &&
            lhs.loyalty == rhs.loyalty && lhs.accuracy == rhs.accuracy &&
            lhs.preferredCombatRange == rhs.preferredCombatRange &&
            lhs.minCombatRange == rhs.minCombatRange &&
            lhs.maxCombatRange == rhs.maxCombatRange &&
            lhs.coverPreference == rhs.coverPreference &&
            lhs.retreatHealthThreshold == rhs.retreatHealthThreshold &&
            lhs.pursuitSeconds == rhs.pursuitSeconds &&
            lhs.searchSeconds == rhs.searchSeconds &&
            lhs.communicationRange == rhs.communicationRange &&
            lhs.suppressionTolerance == rhs.suppressionTolerance;
    }

    [[nodiscard]] inline bool operator!=(
        const CharacterAdvancedOverrides& lhs,
        const CharacterAdvancedOverrides& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    struct BehaviourProfileInfo
    {
        const char* id = "guard";
        const char* displayName = "Guard";
        CharacterRole role = CharacterRole::Guard;
    };

    [[nodiscard]] inline const std::array<BehaviourProfileInfo, 8>&
        BuiltInBehaviourProfiles() noexcept
    {
        static constexpr std::array<BehaviourProfileInfo, 8> profiles = {{
            {"guard", "Guard", CharacterRole::Guard},
            {"patrol_guard", "Patrol Guard", CharacterRole::PatrolGuard},
            {"soldier", "Soldier", CharacterRole::Soldier},
            {"civilian", "Civilian", CharacterRole::Civilian},
            {"companion", "Companion", CharacterRole::Companion},
            {"predator", "Predator", CharacterRole::Predator},
            {"passive", "Passive Creature", CharacterRole::Passive},
            {"custom", "Custom", CharacterRole::Custom},
        }};
        return profiles;
    }

    namespace detail
    {
        inline float Clamp01(const float value) noexcept
        {
            return std::clamp(value, 0.0f, 1.0f);
        }

        inline void SanitizeTuning(CharacterTuning& tuning) noexcept
        {
            tuning.visionDistance = std::clamp(tuning.visionDistance, 0.0f, 500.0f);
            tuning.peripheralVisionDistance = std::clamp(
                tuning.peripheralVisionDistance, 0.0f, tuning.visionDistance);
            tuning.horizontalFovDegrees = std::clamp(tuning.horizontalFovDegrees, 1.0f, 360.0f);
            tuning.verticalFovDegrees = std::clamp(tuning.verticalFovDegrees, 1.0f, 180.0f);
            tuning.hearingSensitivity = std::clamp(tuning.hearingSensitivity, 0.0f, 10.0f);
            tuning.visualReactionSeconds = std::clamp(tuning.visualReactionSeconds, 0.0f, 10.0f);
            tuning.audioReactionSeconds = std::clamp(tuning.audioReactionSeconds, 0.0f, 10.0f);
            tuning.memorySeconds = std::clamp(tuning.memorySeconds, 0.0f, 600.0f);
            tuning.suspicionGain = std::clamp(tuning.suspicionGain, 0.0f, 100.0f);
            tuning.suspicionDecay = std::clamp(tuning.suspicionDecay, 0.0f, 100.0f);
            tuning.alertThreshold = std::clamp(tuning.alertThreshold, 0.0f, 100.0f);
            tuning.combatThreshold = std::clamp(
                tuning.combatThreshold, tuning.alertThreshold, 100.0f);
            tuning.aggression = Clamp01(tuning.aggression);
            tuning.courage = Clamp01(tuning.courage);
            tuning.curiosity = Clamp01(tuning.curiosity);
            tuning.alertness = Clamp01(tuning.alertness);
            tuning.loyalty = Clamp01(tuning.loyalty);
            tuning.accuracy = Clamp01(tuning.accuracy);
            tuning.minCombatRange = std::clamp(tuning.minCombatRange, 0.0f, 500.0f);
            tuning.maxCombatRange = std::clamp(
                tuning.maxCombatRange, tuning.minCombatRange, 500.0f);
            tuning.preferredCombatRange = std::clamp(
                tuning.preferredCombatRange, tuning.minCombatRange, tuning.maxCombatRange);
            tuning.coverPreference = Clamp01(tuning.coverPreference);
            tuning.retreatHealthThreshold = Clamp01(tuning.retreatHealthThreshold);
            tuning.pursuitSeconds = std::clamp(tuning.pursuitSeconds, 0.0f, 600.0f);
            tuning.searchSeconds = std::clamp(tuning.searchSeconds, 0.0f, 600.0f);
            tuning.communicationRange = std::clamp(tuning.communicationRange, 0.0f, 1000.0f);
            tuning.suppressionTolerance = Clamp01(tuning.suppressionTolerance);
        }

        inline CharacterTuning TypeDefaults(const CharacterType type) noexcept
        {
            CharacterTuning tuning;
            switch (type)
            {
            case CharacterType::Creature:
                tuning.visionDistance = 30.0f;
                tuning.peripheralVisionDistance = 18.0f;
                tuning.horizontalFovDegrees = 120.0f;
                tuning.hearingSensitivity = 1.25f;
                tuning.visualReactionSeconds = 0.30f;
                tuning.audioReactionSeconds = 0.20f;
                tuning.memorySeconds = 10.0f;
                tuning.communicationRange = 0.0f;
                tuning.coverPreference = 0.10f;
                break;
            case CharacterType::Custom:
            case CharacterType::Human:
            default:
                break;
            }
            return tuning;
        }

        inline void ApplyRole(CharacterTuning& t, const CharacterRole role) noexcept
        {
            switch (role)
            {
            case CharacterRole::Guard:
                t.alertness += 0.12f;
                t.courage += 0.05f;
                t.coverPreference += 0.10f;
                t.searchSeconds += 3.0f;
                break;
            case CharacterRole::PatrolGuard:
                t.alertness += 0.10f;
                t.curiosity += 0.15f;
                t.pursuitSeconds += 2.0f;
                t.searchSeconds += 4.0f;
                break;
            case CharacterRole::Soldier:
                t.aggression += 0.15f;
                t.courage += 0.20f;
                t.loyalty += 0.20f;
                t.accuracy += 0.15f;
                t.coverPreference += 0.25f;
                t.communicationRange += 10.0f;
                t.suppressionTolerance += 0.20f;
                break;
            case CharacterRole::Civilian:
                t.aggression = 0.10f;
                t.courage = 0.18f;
                t.loyalty = 0.35f;
                t.accuracy = 0.10f;
                t.preferredCombatRange = 0.0f;
                t.minCombatRange = 0.0f;
                t.maxCombatRange = 0.0f;
                t.coverPreference = 0.35f;
                t.retreatHealthThreshold = 0.85f;
                t.pursuitSeconds = 0.0f;
                t.communicationRange = 18.0f;
                break;
            case CharacterRole::Companion:
                t.courage += 0.15f;
                t.loyalty = 0.90f;
                t.accuracy += 0.10f;
                t.coverPreference += 0.15f;
                t.communicationRange += 15.0f;
                break;
            case CharacterRole::Predator:
                t.aggression = 0.90f;
                t.courage = 0.80f;
                t.curiosity = 0.70f;
                t.loyalty = 0.15f;
                t.coverPreference = 0.05f;
                t.retreatHealthThreshold = 0.12f;
                t.pursuitSeconds = 18.0f;
                t.communicationRange = 0.0f;
                break;
            case CharacterRole::Passive:
                t.aggression = 0.05f;
                t.courage = 0.20f;
                t.curiosity = 0.40f;
                t.coverPreference = 0.05f;
                t.retreatHealthThreshold = 0.95f;
                t.pursuitSeconds = 0.0f;
                t.communicationRange = 0.0f;
                break;
            case CharacterRole::Custom:
            default:
                break;
            }
        }

        inline void ApplyPersonality(CharacterTuning& t, const PersonalityPreset preset) noexcept
        {
            switch (preset)
            {
            case PersonalityPreset::Cautious:
                t.aggression -= 0.18f;
                t.courage -= 0.08f;
                t.alertness += 0.15f;
                t.coverPreference += 0.20f;
                t.retreatHealthThreshold += 0.10f;
                break;
            case PersonalityPreset::Aggressive:
                t.aggression += 0.25f;
                t.courage += 0.12f;
                t.coverPreference -= 0.15f;
                t.pursuitSeconds += 4.0f;
                break;
            case PersonalityPreset::Timid:
                t.aggression -= 0.30f;
                t.courage -= 0.30f;
                t.curiosity -= 0.10f;
                t.retreatHealthThreshold += 0.30f;
                t.suppressionTolerance -= 0.20f;
                break;
            case PersonalityPreset::Veteran:
                t.courage += 0.20f;
                t.alertness += 0.12f;
                t.accuracy += 0.15f;
                t.coverPreference += 0.10f;
                t.suppressionTolerance += 0.20f;
                t.visualReactionSeconds *= 0.80f;
                t.audioReactionSeconds *= 0.80f;
                break;
            case PersonalityPreset::Reckless:
                t.aggression += 0.35f;
                t.courage += 0.25f;
                t.coverPreference -= 0.30f;
                t.retreatHealthThreshold -= 0.15f;
                t.suppressionTolerance += 0.10f;
                break;
            case PersonalityPreset::Balanced:
            case PersonalityPreset::Custom:
            default:
                break;
            }
        }

        inline void ApplySkill(CharacterTuning& t, const SkillPreset preset) noexcept
        {
            switch (preset)
            {
            case SkillPreset::Untrained:
                t.accuracy -= 0.30f;
                t.visualReactionSeconds *= 1.40f;
                break;
            case SkillPreset::Novice:
                t.accuracy -= 0.15f;
                t.visualReactionSeconds *= 1.20f;
                break;
            case SkillPreset::Veteran:
                t.accuracy += 0.18f;
                t.visualReactionSeconds *= 0.85f;
                break;
            case SkillPreset::Elite:
                t.accuracy += 0.30f;
                t.visualReactionSeconds *= 0.70f;
                t.audioReactionSeconds *= 0.80f;
                break;
            case SkillPreset::Trained:
            default:
                break;
            }
        }

        inline void ApplyAwareness(CharacterTuning& t, const AwarenessPreset preset) noexcept
        {
            switch (preset)
            {
            case AwarenessPreset::Relaxed:
                t.visionDistance *= 0.80f;
                t.peripheralVisionDistance *= 0.75f;
                t.hearingSensitivity *= 0.80f;
                t.alertness -= 0.15f;
                t.visualReactionSeconds *= 1.20f;
                t.audioReactionSeconds *= 1.20f;
                break;
            case AwarenessPreset::Alert:
                t.visionDistance *= 1.15f;
                t.peripheralVisionDistance *= 1.15f;
                t.hearingSensitivity *= 1.20f;
                t.alertness += 0.15f;
                t.visualReactionSeconds *= 0.85f;
                t.audioReactionSeconds *= 0.85f;
                break;
            case AwarenessPreset::Vigilant:
                t.visionDistance *= 1.30f;
                t.peripheralVisionDistance *= 1.30f;
                t.hearingSensitivity *= 1.40f;
                t.alertness += 0.30f;
                t.visualReactionSeconds *= 0.70f;
                t.audioReactionSeconds *= 0.70f;
                break;
            case AwarenessPreset::Normal:
            default:
                break;
            }
        }

        inline void ApplyOverrides(
            CharacterTuning& t,
            const CharacterAdvancedOverrides& o) noexcept
        {
#define RENEGADE_AI_APPLY_OVERRIDE(name) if (o.name) t.name = *o.name
            RENEGADE_AI_APPLY_OVERRIDE(visionDistance);
            RENEGADE_AI_APPLY_OVERRIDE(peripheralVisionDistance);
            RENEGADE_AI_APPLY_OVERRIDE(horizontalFovDegrees);
            RENEGADE_AI_APPLY_OVERRIDE(verticalFovDegrees);
            RENEGADE_AI_APPLY_OVERRIDE(hearingSensitivity);
            RENEGADE_AI_APPLY_OVERRIDE(visualReactionSeconds);
            RENEGADE_AI_APPLY_OVERRIDE(audioReactionSeconds);
            RENEGADE_AI_APPLY_OVERRIDE(memorySeconds);
            RENEGADE_AI_APPLY_OVERRIDE(suspicionGain);
            RENEGADE_AI_APPLY_OVERRIDE(suspicionDecay);
            RENEGADE_AI_APPLY_OVERRIDE(alertThreshold);
            RENEGADE_AI_APPLY_OVERRIDE(combatThreshold);
            RENEGADE_AI_APPLY_OVERRIDE(aggression);
            RENEGADE_AI_APPLY_OVERRIDE(courage);
            RENEGADE_AI_APPLY_OVERRIDE(curiosity);
            RENEGADE_AI_APPLY_OVERRIDE(alertness);
            RENEGADE_AI_APPLY_OVERRIDE(loyalty);
            RENEGADE_AI_APPLY_OVERRIDE(accuracy);
            RENEGADE_AI_APPLY_OVERRIDE(preferredCombatRange);
            RENEGADE_AI_APPLY_OVERRIDE(minCombatRange);
            RENEGADE_AI_APPLY_OVERRIDE(maxCombatRange);
            RENEGADE_AI_APPLY_OVERRIDE(coverPreference);
            RENEGADE_AI_APPLY_OVERRIDE(retreatHealthThreshold);
            RENEGADE_AI_APPLY_OVERRIDE(pursuitSeconds);
            RENEGADE_AI_APPLY_OVERRIDE(searchSeconds);
            RENEGADE_AI_APPLY_OVERRIDE(communicationRange);
            RENEGADE_AI_APPLY_OVERRIDE(suppressionTolerance);
#undef RENEGADE_AI_APPLY_OVERRIDE
        }

        inline bool FiniteInRange(
            const std::optional<float>& value,
            const float minimum,
            const float maximum) noexcept
        {
            return !value ||
                (std::isfinite(*value) && *value >= minimum && *value <= maximum);
        }

        inline void AppendOverride(
            std::ostringstream& stream,
            const char* key,
            const std::optional<float>& value)
        {
            if (value)
                stream << ';' << key << '=' << *value;
        }

        inline bool SetParsedOverride(
            CharacterAdvancedOverrides& overrides,
            const std::string& key,
            const float value)
        {
#define RENEGADE_AI_PARSE_OVERRIDE(name, text) \
            if (key == text) { overrides.name = value; return true; }
            RENEGADE_AI_PARSE_OVERRIDE(visionDistance, "vision")
            RENEGADE_AI_PARSE_OVERRIDE(peripheralVisionDistance, "peripheral")
            RENEGADE_AI_PARSE_OVERRIDE(horizontalFovDegrees, "hfov")
            RENEGADE_AI_PARSE_OVERRIDE(verticalFovDegrees, "vfov")
            RENEGADE_AI_PARSE_OVERRIDE(hearingSensitivity, "hearing")
            RENEGADE_AI_PARSE_OVERRIDE(visualReactionSeconds, "visual_reaction")
            RENEGADE_AI_PARSE_OVERRIDE(audioReactionSeconds, "audio_reaction")
            RENEGADE_AI_PARSE_OVERRIDE(memorySeconds, "memory")
            RENEGADE_AI_PARSE_OVERRIDE(suspicionGain, "suspicion_gain")
            RENEGADE_AI_PARSE_OVERRIDE(suspicionDecay, "suspicion_decay")
            RENEGADE_AI_PARSE_OVERRIDE(alertThreshold, "alert_threshold")
            RENEGADE_AI_PARSE_OVERRIDE(combatThreshold, "combat_threshold")
            RENEGADE_AI_PARSE_OVERRIDE(aggression, "aggression")
            RENEGADE_AI_PARSE_OVERRIDE(courage, "courage")
            RENEGADE_AI_PARSE_OVERRIDE(curiosity, "curiosity")
            RENEGADE_AI_PARSE_OVERRIDE(alertness, "alertness")
            RENEGADE_AI_PARSE_OVERRIDE(loyalty, "loyalty")
            RENEGADE_AI_PARSE_OVERRIDE(accuracy, "accuracy")
            RENEGADE_AI_PARSE_OVERRIDE(preferredCombatRange, "preferred_range")
            RENEGADE_AI_PARSE_OVERRIDE(minCombatRange, "min_range")
            RENEGADE_AI_PARSE_OVERRIDE(maxCombatRange, "max_range")
            RENEGADE_AI_PARSE_OVERRIDE(coverPreference, "cover")
            RENEGADE_AI_PARSE_OVERRIDE(retreatHealthThreshold, "retreat_health")
            RENEGADE_AI_PARSE_OVERRIDE(pursuitSeconds, "pursuit")
            RENEGADE_AI_PARSE_OVERRIDE(searchSeconds, "search")
            RENEGADE_AI_PARSE_OVERRIDE(communicationRange, "communication")
            RENEGADE_AI_PARSE_OVERRIDE(suppressionTolerance, "suppression")
#undef RENEGADE_AI_PARSE_OVERRIDE
            return false;
        }

        inline bool ValidateEnumMetadata(
            const wi::scene::MetadataComponent& metadata,
            const char* key,
            const int minimum,
            const int maximum,
            const char* label,
            std::string& error)
        {
            if (!metadata.int_values.has(key))
            {
                error = std::string("Character profile metadata is missing: ") + label + '.';
                return false;
            }
            const int value = metadata.int_values.get(key);
            if (value < minimum || value > maximum)
            {
                error = std::string("Character profile metadata has an invalid ") + label + '.';
                return false;
            }
            return true;
        }
    }

    [[nodiscard]] inline bool HasAnyCharacterAdvancedOverride(
        const CharacterAdvancedOverrides& o) noexcept
    {
        return o.visionDistance || o.peripheralVisionDistance || o.horizontalFovDegrees ||
            o.verticalFovDegrees || o.hearingSensitivity || o.visualReactionSeconds ||
            o.audioReactionSeconds || o.memorySeconds || o.suspicionGain ||
            o.suspicionDecay || o.alertThreshold || o.combatThreshold || o.aggression ||
            o.courage || o.curiosity || o.alertness || o.loyalty || o.accuracy ||
            o.preferredCombatRange || o.minCombatRange || o.maxCombatRange ||
            o.coverPreference || o.retreatHealthThreshold || o.pursuitSeconds ||
            o.searchSeconds || o.communicationRange || o.suppressionTolerance;
    }

    [[nodiscard]] inline bool ValidateCharacterAdvancedOverrides(
        const CharacterAdvancedOverrides& o,
        std::string& error) noexcept
    {
        const auto invalid = [&](const char* label)
        {
            error = std::string("Character advanced override is outside its supported range: ") +
                label + '.';
            return false;
        };
        if (!detail::FiniteInRange(o.visionDistance, 0.0f, 500.0f)) return invalid("vision distance");
        if (!detail::FiniteInRange(o.peripheralVisionDistance, 0.0f, 500.0f)) return invalid("peripheral distance");
        if (!detail::FiniteInRange(o.horizontalFovDegrees, 1.0f, 360.0f)) return invalid("horizontal FOV");
        if (!detail::FiniteInRange(o.verticalFovDegrees, 1.0f, 180.0f)) return invalid("vertical FOV");
        if (!detail::FiniteInRange(o.hearingSensitivity, 0.0f, 10.0f)) return invalid("hearing sensitivity");
        if (!detail::FiniteInRange(o.visualReactionSeconds, 0.0f, 10.0f)) return invalid("visual reaction");
        if (!detail::FiniteInRange(o.audioReactionSeconds, 0.0f, 10.0f)) return invalid("audio reaction");
        if (!detail::FiniteInRange(o.memorySeconds, 0.0f, 600.0f)) return invalid("memory duration");
        if (!detail::FiniteInRange(o.suspicionGain, 0.0f, 100.0f)) return invalid("suspicion gain");
        if (!detail::FiniteInRange(o.suspicionDecay, 0.0f, 100.0f)) return invalid("suspicion decay");
        if (!detail::FiniteInRange(o.alertThreshold, 0.0f, 100.0f)) return invalid("alert threshold");
        if (!detail::FiniteInRange(o.combatThreshold, 0.0f, 100.0f)) return invalid("combat threshold");
        for (const auto* entry : {&o.aggression, &o.courage, &o.curiosity, &o.alertness,
                                  &o.loyalty, &o.accuracy, &o.coverPreference,
                                  &o.retreatHealthThreshold, &o.suppressionTolerance})
        {
            if (!detail::FiniteInRange(*entry, 0.0f, 1.0f)) return invalid("normalized trait");
        }
        if (!detail::FiniteInRange(o.preferredCombatRange, 0.0f, 500.0f)) return invalid("preferred combat range");
        if (!detail::FiniteInRange(o.minCombatRange, 0.0f, 500.0f)) return invalid("minimum combat range");
        if (!detail::FiniteInRange(o.maxCombatRange, 0.0f, 500.0f)) return invalid("maximum combat range");
        if (!detail::FiniteInRange(o.pursuitSeconds, 0.0f, 600.0f)) return invalid("pursuit duration");
        if (!detail::FiniteInRange(o.searchSeconds, 0.0f, 600.0f)) return invalid("search duration");
        if (!detail::FiniteInRange(o.communicationRange, 0.0f, 1000.0f)) return invalid("communication range");

        if (o.visionDistance && o.peripheralVisionDistance &&
            *o.peripheralVisionDistance > *o.visionDistance)
        {
            error = "Character peripheral vision distance cannot exceed vision distance.";
            return false;
        }
        if (o.alertThreshold && o.combatThreshold &&
            *o.combatThreshold < *o.alertThreshold)
        {
            error = "Character combat threshold cannot be lower than alert threshold.";
            return false;
        }
        if (o.minCombatRange && o.maxCombatRange &&
            *o.maxCombatRange < *o.minCombatRange)
        {
            error = "Character maximum combat range cannot be lower than minimum combat range.";
            return false;
        }
        if (o.preferredCombatRange && o.minCombatRange &&
            *o.preferredCombatRange < *o.minCombatRange)
        {
            error = "Character preferred combat range cannot be lower than minimum combat range.";
            return false;
        }
        if (o.preferredCombatRange && o.maxCombatRange &&
            *o.preferredCombatRange > *o.maxCombatRange)
        {
            error = "Character preferred combat range cannot exceed maximum combat range.";
            return false;
        }
        error.clear();
        return true;
    }

    [[nodiscard]] inline bool ValidateCharacterProfileAuthoring(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        std::string& error)
    {
        if (!IsRenegadeCharacter(scene, entity))
        {
            error = "Selected entity is not a Renegade Character.";
            return false;
        }
        const auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata == nullptr)
        {
            error = "Character metadata is missing.";
            return false;
        }

        if (!detail::ValidateEnumMetadata(*metadata, CharacterTypeMetadataKey,
                static_cast<int>(CharacterType::Human), static_cast<int>(CharacterType::Custom),
                "character type", error) ||
            !detail::ValidateEnumMetadata(*metadata, CharacterRoleMetadataKey,
                static_cast<int>(CharacterRole::Guard), static_cast<int>(CharacterRole::Custom),
                "role", error) ||
            !detail::ValidateEnumMetadata(*metadata, CharacterPersonalityMetadataKey,
                static_cast<int>(PersonalityPreset::Cautious), static_cast<int>(PersonalityPreset::Custom),
                "personality", error) ||
            !detail::ValidateEnumMetadata(*metadata, CharacterCombatStyleMetadataKey,
                static_cast<int>(CombatStyle::None), static_cast<int>(CombatStyle::Custom),
                "combat style", error) ||
            !detail::ValidateEnumMetadata(*metadata, CharacterSkillMetadataKey,
                static_cast<int>(SkillPreset::Untrained), static_cast<int>(SkillPreset::Elite),
                "skill", error) ||
            !detail::ValidateEnumMetadata(*metadata, CharacterAwarenessMetadataKey,
                static_cast<int>(AwarenessPreset::Relaxed), static_cast<int>(AwarenessPreset::Vigilant),
                "awareness", error))
        {
            return false;
        }

        if (!metadata->string_values.has(CharacterFactionMetadataKey))
        {
            error = "Character profile metadata is missing: faction.";
            return false;
        }
        return ValidateFactionId(
            metadata->string_values.get(CharacterFactionMetadataKey), error);
    }

    [[nodiscard]] inline CharacterTuning ResolveCharacterTuning(
        const CharacterAuthoringSettings& settings,
        const CharacterAdvancedOverrides& overrides = {}) noexcept
    {
        CharacterTuning tuning = detail::TypeDefaults(settings.type);
        detail::ApplyRole(tuning, settings.role);
        detail::ApplyPersonality(tuning, settings.personality);
        detail::ApplySkill(tuning, settings.skill);
        detail::ApplyAwareness(tuning, settings.awareness);
        detail::ApplyOverrides(tuning, overrides);
        detail::SanitizeTuning(tuning);
        return tuning;
    }

    [[nodiscard]] inline std::string SerializeCharacterAdvancedOverrides(
        const CharacterAdvancedOverrides& o)
    {
        if (!HasAnyCharacterAdvancedOverride(o))
            return {};
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << std::setprecision(9) << 'v' << CharacterAdvancedSchemaVersion;
        detail::AppendOverride(stream, "vision", o.visionDistance);
        detail::AppendOverride(stream, "peripheral", o.peripheralVisionDistance);
        detail::AppendOverride(stream, "hfov", o.horizontalFovDegrees);
        detail::AppendOverride(stream, "vfov", o.verticalFovDegrees);
        detail::AppendOverride(stream, "hearing", o.hearingSensitivity);
        detail::AppendOverride(stream, "visual_reaction", o.visualReactionSeconds);
        detail::AppendOverride(stream, "audio_reaction", o.audioReactionSeconds);
        detail::AppendOverride(stream, "memory", o.memorySeconds);
        detail::AppendOverride(stream, "suspicion_gain", o.suspicionGain);
        detail::AppendOverride(stream, "suspicion_decay", o.suspicionDecay);
        detail::AppendOverride(stream, "alert_threshold", o.alertThreshold);
        detail::AppendOverride(stream, "combat_threshold", o.combatThreshold);
        detail::AppendOverride(stream, "aggression", o.aggression);
        detail::AppendOverride(stream, "courage", o.courage);
        detail::AppendOverride(stream, "curiosity", o.curiosity);
        detail::AppendOverride(stream, "alertness", o.alertness);
        detail::AppendOverride(stream, "loyalty", o.loyalty);
        detail::AppendOverride(stream, "accuracy", o.accuracy);
        detail::AppendOverride(stream, "preferred_range", o.preferredCombatRange);
        detail::AppendOverride(stream, "min_range", o.minCombatRange);
        detail::AppendOverride(stream, "max_range", o.maxCombatRange);
        detail::AppendOverride(stream, "cover", o.coverPreference);
        detail::AppendOverride(stream, "retreat_health", o.retreatHealthThreshold);
        detail::AppendOverride(stream, "pursuit", o.pursuitSeconds);
        detail::AppendOverride(stream, "search", o.searchSeconds);
        detail::AppendOverride(stream, "communication", o.communicationRange);
        detail::AppendOverride(stream, "suppression", o.suppressionTolerance);
        return stream.str();
    }

    [[nodiscard]] inline bool DeserializeCharacterAdvancedOverrides(
        const std::string& serialized,
        CharacterAdvancedOverrides& overrides,
        std::string& error)
    {
        overrides = {};
        if (serialized.empty())
        {
            error.clear();
            return true;
        }

        std::istringstream stream(serialized);
        stream.imbue(std::locale::classic());
        std::string token;
        if (!std::getline(stream, token, ';') || token != "v1")
        {
            error = "Character advanced overrides use an unsupported schema.";
            return false;
        }

        std::unordered_set<std::string> seenKeys;
        while (std::getline(stream, token, ';'))
        {
            const std::size_t equals = token.find('=');
            if (equals == std::string::npos || equals == 0 || equals + 1 >= token.size())
            {
                error = "Character advanced override payload is malformed.";
                return false;
            }
            const std::string key = token.substr(0, equals);
            if (!seenKeys.insert(key).second)
            {
                error = "Character advanced override contains a duplicate field: " + key + '.';
                return false;
            }

            const std::string valueText = token.substr(equals + 1);
            std::istringstream valueStream(valueText);
            valueStream.imbue(std::locale::classic());
            float value = 0.0f;
            valueStream >> value;
            if (!valueStream || !std::isfinite(value))
            {
                error = "Character advanced override contains an invalid number.";
                return false;
            }
            valueStream >> std::ws;
            if (!valueStream.eof())
            {
                error = "Character advanced override contains trailing characters.";
                return false;
            }
            if (!detail::SetParsedOverride(overrides, key, value))
            {
                error = "Character advanced override contains an unknown field: " + key + '.';
                return false;
            }
        }
        return ValidateCharacterAdvancedOverrides(overrides, error);
    }

    [[nodiscard]] inline bool CaptureCharacterAdvancedOverrides(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        CharacterAdvancedOverrides& overrides,
        std::string& error)
    {
        overrides = {};
        if (!IsRenegadeCharacter(scene, entity))
        {
            error = "Selected entity is not a Renegade Character.";
            return false;
        }
        const auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata == nullptr || !metadata->string_values.has(CharacterAdvancedMetadataKey))
        {
            error.clear();
            return true;
        }
        return DeserializeCharacterAdvancedOverrides(
            metadata->string_values.get(CharacterAdvancedMetadataKey), overrides, error);
    }

    [[nodiscard]] inline bool ApplyCharacterAdvancedOverrides(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const CharacterAdvancedOverrides& overrides,
        std::string& error)
    {
        if (!IsRenegadeCharacter(scene, entity))
        {
            error = "Selected entity is not a Renegade Character.";
            return false;
        }
        if (!ValidateCharacterAdvancedOverrides(overrides, error))
            return false;
        auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata == nullptr)
        {
            error = "Character metadata is missing.";
            return false;
        }
        const std::string serialized = SerializeCharacterAdvancedOverrides(overrides);
        if (serialized.empty())
            metadata->string_values.erase(CharacterAdvancedMetadataKey);
        else
            metadata->string_values.set(CharacterAdvancedMetadataKey, serialized);
        error.clear();
        return true;
    }

    inline void EraseCharacterAdvancedOverrides(
        wi::scene::MetadataComponent& metadata) noexcept
    {
        metadata.string_values.erase(CharacterAdvancedMetadataKey);
    }

    class SetCharacterAdvancedOverridesCommand final : public ICommand
    {
    public:
        SetCharacterAdvancedOverridesCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            CharacterAdvancedOverrides after)
            : scene_(&scene), entity_(entity), after_(std::move(after))
        {
            if (!IsRenegadeCharacter(scene, entity))
                return;
            auto* metadata = scene.metadatas.GetComponent(entity);
            if (metadata == nullptr)
                return;

            targetValid_ = true;
            if (metadata->string_values.has(CharacterAdvancedMetadataKey))
            {
                hadRawBefore_ = true;
                rawBefore_ = metadata->string_values.get(CharacterAdvancedMetadataKey);
                std::string ignored;
                beforeParsed_ = DeserializeCharacterAdvancedOverrides(
                    rawBefore_, before_, ignored);
            }
            else
            {
                beforeParsed_ = true;
                before_ = {};
            }
        }

        bool Execute() override
        {
            if (!targetValid_ || scene_ == nullptr)
                return false;
            if (beforeParsed_ && before_ == after_)
                return false;
            std::string error;
            return ApplyCharacterAdvancedOverrides(*scene_, entity_, after_, error);
        }

        void Undo() override
        {
            if (!targetValid_ || scene_ == nullptr ||
                !IsRenegadeCharacter(*scene_, entity_))
            {
                return;
            }
            auto* metadata = scene_->metadatas.GetComponent(entity_);
            if (metadata == nullptr)
                return;
            if (hadRawBefore_)
                metadata->string_values.set(CharacterAdvancedMetadataKey, rawBefore_);
            else
                metadata->string_values.erase(CharacterAdvancedMetadataKey);
        }

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        CharacterAdvancedOverrides before_;
        CharacterAdvancedOverrides after_;
        std::string rawBefore_;
        bool targetValid_ = false;
        bool hadRawBefore_ = false;
        bool beforeParsed_ = false;
    };
}
