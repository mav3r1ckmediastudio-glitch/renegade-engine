#pragma once

#include "RuntimeCharacterSystem.h"
#include "renegade/bridge/PhysicsService.h"
#include "renegade/bridge/PlayerService.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace renegade::runtime
{
    inline constexpr const char* RuntimePlayerKnowledgeId = "$runtime.player";
    inline constexpr float CharacterCognitionIntervalSeconds = 0.20f; // 5 Hz
    inline constexpr std::size_t MaxCharacterMemories = 16;
    inline constexpr std::size_t MaxSoundStimuli = 128;
    inline constexpr float MinimumMemoryConfidence = 0.01f;

    enum class KnowledgeSource : std::int32_t
    {
        None = 0,
        Seen,
        Heard,
        DamagedBy,
        AllyReport,
        Script,
    };

    enum class AwarenessState : std::int32_t
    {
        Unaware = 0,
        Interested,
        Suspicious,
        Alerted,
        Combat,
        Searching,
    };

    [[nodiscard]] inline const char* ToString(const KnowledgeSource source) noexcept
    {
        switch (source)
        {
        case KnowledgeSource::Seen: return "Seen";
        case KnowledgeSource::Heard: return "Heard";
        case KnowledgeSource::DamagedBy: return "DamagedBy";
        case KnowledgeSource::AllyReport: return "AllyReport";
        case KnowledgeSource::Script: return "Script";
        case KnowledgeSource::None:
        default:
            return "None";
        }
    }

    [[nodiscard]] inline const char* ToString(const AwarenessState state) noexcept
    {
        switch (state)
        {
        case AwarenessState::Interested: return "Interested";
        case AwarenessState::Suspicious: return "Suspicious";
        case AwarenessState::Alerted: return "Alerted";
        case AwarenessState::Combat: return "Combat";
        case AwarenessState::Searching: return "Searching";
        case AwarenessState::Unaware:
        default:
            return "Unaware";
        }
    }

    struct CharacterMemoryRecord
    {
        std::string subjectId;
        std::string subjectFactionId;
        KnowledgeSource source = KnowledgeSource::None;
        XMFLOAT3 lastKnownPosition = XMFLOAT3(0, 0, 0);
        XMFLOAT3 lastKnownVelocity = XMFLOAT3(0, 0, 0);
        float confidence = 0.0f;
        float threat = 0.0f;
        float ageSeconds = 0.0f;
        float secondsSinceSeen = std::numeric_limits<float>::infinity();
        float memoryLifetimeSeconds = 0.0f;
        bool hasPosition = false;
        bool hostile = false;
        bool subjectDead = false;
        bool directSight = false;
    };

    struct CharacterCognitionRecord
    {
        bridge::StableId characterId;
        float suspicion = 0.0f;
        AwarenessState awareness = AwarenessState::Unaware;
        float nextCognitionTime = 0.0f;
        std::uint64_t cognitionTicks = 0;

        std::string pendingVisualSubjectId;
        float pendingVisualSeconds = 0.0f;
        std::uint64_t pendingSoundSequence = 0;
        float pendingSoundReadyTime = 0.0f;
        std::uint64_t lastHeardSoundSequence = 0;

        std::vector<CharacterMemoryRecord> memories;
    };

    struct SoundStimulus
    {
        std::uint64_t sequence = 0;
        std::string sourceSubjectId;
        std::string sourceFactionId;
        XMFLOAT3 position = XMFLOAT3(0, 0, 0);
        XMFLOAT3 velocity = XMFLOAT3(0, 0, 0);
        float radius = 0.0f;
        float intensity = 1.0f;
        float createdTime = 0.0f;
        float expiresTime = 0.0f;
    };

    struct VisualObservation
    {
        bool visible = false;
        bool peripheral = false;
        XMFLOAT3 position = XMFLOAT3(0, 0, 0);
        XMFLOAT3 velocity = XMFLOAT3(0, 0, 0);
    };

    struct RuntimeCharacterPerceptionState
    {
        std::vector<CharacterCognitionRecord> characters;
        std::vector<SoundStimulus> sounds;
        float elapsedSeconds = 0.0f;
        float nextPlayerFootstepTime = 0.0f;
        std::uint64_t nextSoundSequence = 1;
        std::uint64_t droppedSounds = 0;
        std::uint64_t lineOfSightQueries = 0;
        std::uint64_t visualDetections = 0;
        std::uint64_t heardStimuli = 0;
        std::uint64_t damageStimuli = 0;
    };

    namespace perception_detail
    {
        inline float Clamp01(const float value) noexcept
        {
            return std::clamp(value, 0.0f, 1.0f);
        }

        inline bool FiniteVector(const XMFLOAT3& value) noexcept
        {
            return std::isfinite(value.x) &&
                std::isfinite(value.y) &&
                std::isfinite(value.z);
        }

        inline bool ValidSubjectId(const std::string& value) noexcept
        {
            if (value.empty() || value.size() > 128)
                return false;
            for (const unsigned char c : value)
            {
                if (c < 32 || c == 127)
                    return false;
            }
            return true;
        }

        inline XMFLOAT3 AddScaled(
            const XMFLOAT3& a,
            const XMFLOAT3& direction,
            const float scale) noexcept
        {
            return XMFLOAT3(
                a.x + direction.x * scale,
                a.y + direction.y * scale,
                a.z + direction.z * scale);
        }

        inline XMFLOAT3 Subtract(const XMFLOAT3& a, const XMFLOAT3& b) noexcept
        {
            return XMFLOAT3(a.x - b.x, a.y - b.y, a.z - b.z);
        }

        inline float LengthSquared(const XMFLOAT3& value) noexcept
        {
            return value.x * value.x + value.y * value.y + value.z * value.z;
        }

        inline float Length(const XMFLOAT3& value) noexcept
        {
            return std::sqrt(LengthSquared(value));
        }

        inline XMFLOAT3 Normalize(const XMFLOAT3& value) noexcept
        {
            const float length = Length(value);
            if (!(length > 0.00001f) || !std::isfinite(length))
                return XMFLOAT3(0, 0, 1);
            return XMFLOAT3(value.x / length, value.y / length, value.z / length);
        }

        inline float HorizontalAngleDegrees(
            const XMFLOAT3& facing,
            const XMFLOAT3& direction) noexcept
        {
            XMFLOAT3 a(facing.x, 0.0f, facing.z);
            XMFLOAT3 b(direction.x, 0.0f, direction.z);
            a = Normalize(a);
            b = Normalize(b);
            const float dot = std::clamp(a.x * b.x + a.z * b.z, -1.0f, 1.0f);
            return wi::math::RadiansToDegrees(std::acos(dot));
        }

        inline float VerticalAngleDegrees(const XMFLOAT3& direction) noexcept
        {
            const float horizontal = std::sqrt(
                direction.x * direction.x + direction.z * direction.z);
            return wi::math::RadiansToDegrees(
                std::abs(std::atan2(direction.y, std::max(0.00001f, horizontal))));
        }

        inline std::uint32_t StableHash(const std::string& value) noexcept
        {
            std::uint32_t hash = 2166136261u;
            for (const unsigned char c : value)
            {
                hash ^= c;
                hash *= 16777619u;
            }
            return hash;
        }

        inline float RelationshipThreat(const bridge::FactionRelationship relationship) noexcept
        {
            switch (relationship)
            {
            case bridge::FactionRelationship::Hostile: return 1.0f;
            case bridge::FactionRelationship::Suspicious: return 0.45f;
            case bridge::FactionRelationship::Neutral: return 0.15f;
            case bridge::FactionRelationship::Friendly: return 0.05f;
            case bridge::FactionRelationship::Ally:
            default:
                return 0.0f;
            }
        }

        inline bool RelationshipHostile(const bridge::FactionRelationship relationship) noexcept
        {
            return relationship == bridge::FactionRelationship::Hostile;
        }

        inline CharacterMemoryRecord* FindMemory(
            CharacterCognitionRecord& cognition,
            const std::string& subjectId) noexcept
        {
            const auto iterator = std::lower_bound(
                cognition.memories.begin(), cognition.memories.end(), subjectId,
                [](const CharacterMemoryRecord& memory, const std::string& id)
                {
                    return memory.subjectId < id;
                });
            return iterator != cognition.memories.end() && iterator->subjectId == subjectId
                ? &*iterator
                : nullptr;
        }

        inline const CharacterMemoryRecord* FindMemory(
            const CharacterCognitionRecord& cognition,
            const std::string& subjectId) noexcept
        {
            const auto iterator = std::lower_bound(
                cognition.memories.begin(), cognition.memories.end(), subjectId,
                [](const CharacterMemoryRecord& memory, const std::string& id)
                {
                    return memory.subjectId < id;
                });
            return iterator != cognition.memories.end() && iterator->subjectId == subjectId
                ? &*iterator
                : nullptr;
        }

        inline CharacterMemoryRecord& FindOrCreateMemory(
            CharacterCognitionRecord& cognition,
            const std::string& subjectId)
        {
            auto iterator = std::lower_bound(
                cognition.memories.begin(), cognition.memories.end(), subjectId,
                [](const CharacterMemoryRecord& memory, const std::string& id)
                {
                    return memory.subjectId < id;
                });
            if (iterator != cognition.memories.end() && iterator->subjectId == subjectId)
                return *iterator;

            if (cognition.memories.size() >= MaxCharacterMemories)
            {
                auto weakest = std::min_element(
                    cognition.memories.begin(), cognition.memories.end(),
                    [](const CharacterMemoryRecord& lhs, const CharacterMemoryRecord& rhs)
                    {
                        if (lhs.confidence != rhs.confidence)
                            return lhs.confidence < rhs.confidence;
                        return lhs.ageSeconds > rhs.ageSeconds;
                    });
                if (weakest != cognition.memories.end())
                    cognition.memories.erase(weakest);
                iterator = std::lower_bound(
                    cognition.memories.begin(), cognition.memories.end(), subjectId,
                    [](const CharacterMemoryRecord& memory, const std::string& id)
                    {
                        return memory.subjectId < id;
                    });
            }

            CharacterMemoryRecord memory;
            memory.subjectId = subjectId;
            return *cognition.memories.insert(iterator, std::move(memory));
        }

        inline bool HasHostileMemory(const CharacterCognitionRecord& cognition) noexcept
        {
            for (const auto& memory : cognition.memories)
            {
                if (memory.hostile && memory.confidence >= MinimumMemoryConfidence)
                    return true;
            }
            return false;
        }

        inline bool HasDirectHostileSight(const CharacterCognitionRecord& cognition) noexcept
        {
            for (const auto& memory : cognition.memories)
            {
                if (memory.hostile && memory.directSight &&
                    memory.confidence >= MinimumMemoryConfidence)
                {
                    return true;
                }
            }
            return false;
        }

        inline void ReevaluateAwareness(
            CharacterCognitionRecord& cognition,
            const bridge::CharacterTuning& tuning) noexcept
        {
            const bool hostileMemory = HasHostileMemory(cognition);
            const bool directHostileSight = HasDirectHostileSight(cognition);
            const float alertThreshold = std::max(0.01f, tuning.alertThreshold);
            const float combatThreshold = std::max(alertThreshold, tuning.combatThreshold);

            if (hostileMemory && directHostileSight &&
                cognition.suspicion >= combatThreshold)
            {
                cognition.awareness = AwarenessState::Combat;
            }
            else if (hostileMemory && !directHostileSight &&
                (cognition.awareness == AwarenessState::Combat ||
                 cognition.awareness == AwarenessState::Searching) &&
                cognition.suspicion >= alertThreshold * 0.5f)
            {
                cognition.awareness = AwarenessState::Searching;
            }
            else if (cognition.suspicion >= alertThreshold)
            {
                cognition.awareness = AwarenessState::Alerted;
            }
            else if (cognition.suspicion >= alertThreshold * 0.5f)
            {
                cognition.awareness = AwarenessState::Suspicious;
            }
            else if (cognition.suspicion > 0.01f)
            {
                cognition.awareness = AwarenessState::Interested;
            }
            else
            {
                cognition.awareness = AwarenessState::Unaware;
            }
        }

        inline void DecayCharacterState(
            CharacterCognitionRecord& cognition,
            const bridge::CharacterTuning& tuning,
            const float dt) noexcept
        {
            const float safeDt = std::max(0.0f, std::isfinite(dt) ? dt : 0.0f);
            cognition.suspicion = std::max(
                0.0f,
                cognition.suspicion - tuning.suspicionDecay * safeDt);

            for (auto& memory : cognition.memories)
            {
                memory.ageSeconds += safeDt;
                if (std::isfinite(memory.secondsSinceSeen))
                    memory.secondsSinceSeen += safeDt;
                // directSight is owned by the 5 Hz perception sample. Do not
                // clear it on render-frame decay or diagnostics would oscillate
                // between visible/hidden between cognition ticks.

                if (memory.memoryLifetimeSeconds <= 0.0f)
                {
                    memory.confidence = 0.0f;
                }
                else
                {
                    memory.confidence = std::max(
                        0.0f,
                        memory.confidence - safeDt / memory.memoryLifetimeSeconds);
                }
            }
            cognition.memories.erase(
                std::remove_if(
                    cognition.memories.begin(), cognition.memories.end(),
                    [](const CharacterMemoryRecord& memory)
                    {
                        return memory.confidence < MinimumMemoryConfidence;
                    }),
                cognition.memories.end());
            ReevaluateAwareness(cognition, tuning);
        }

        inline void MarkSubjectSightLost(
            CharacterCognitionRecord& cognition,
            const std::string& subjectId) noexcept
        {
            if (auto* memory = FindMemory(cognition, subjectId))
                memory->directSight = false;
        }

        inline bool EntityBelongsToRoot(
            const wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const wi::ecs::Entity root) noexcept
        {
            for (std::size_t depth = 0;
                 depth < 64 && entity != wi::ecs::INVALID_ENTITY;
                 ++depth)
            {
                if (entity == root)
                    return true;
                const auto* hierarchy = scene.hierarchy.GetComponent(entity);
                if (hierarchy == nullptr)
                    return false;
                entity = hierarchy->parentID;
            }
            return false;
        }

        inline bool LineOfSightBlocked(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity observerRoot,
            const XMFLOAT3& origin,
            const XMFLOAT3& direction,
            const float maxDistance) noexcept
        {
            constexpr std::uint32_t filter =
                wi::enums::FILTER_OPAQUE |
                wi::enums::FILTER_TRANSPARENT |
                wi::enums::FILTER_WATER |
                wi::enums::FILTER_TERRAIN |
                wi::enums::FILTER_COLLIDER;
            XMFLOAT3 rayOrigin = origin;
            float remaining = maxDistance;
            for (int selfSkip = 0; selfSkip < 6 && remaining > 0.05f; ++selfSkip)
            {
                const wi::primitive::Ray ray(rayOrigin, direction, 0.05f, remaining);
                const auto hit = scene.Intersects(ray, filter);
                if (hit.entity == wi::ecs::INVALID_ENTITY)
                    return false;
                if (!EntityBelongsToRoot(scene, hit.entity, observerRoot))
                    return true;

                const float advance = Length(Subtract(hit.position, rayOrigin)) + 0.08f;
                if (!(advance > 0.0f) || advance >= remaining)
                    return false;
                rayOrigin = AddScaled(hit.position, direction, 0.08f);
                remaining -= advance;
            }
            // More than six self-hierarchy hits is abnormal. Fail closed rather
            // than seeing through malformed imported geometry.
            return remaining > 0.05f;
        }
    }

    [[nodiscard]] inline CharacterCognitionRecord* FindCharacterCognition(
        RuntimeCharacterPerceptionState& state,
        const bridge::StableId& characterId) noexcept
    {
        const auto iterator = std::lower_bound(
            state.characters.begin(), state.characters.end(), characterId,
            [](const CharacterCognitionRecord& cognition, const bridge::StableId& id)
            {
                return cognition.characterId < id;
            });
        return iterator != state.characters.end() && iterator->characterId == characterId
            ? &*iterator
            : nullptr;
    }

    [[nodiscard]] inline const CharacterCognitionRecord* FindCharacterCognition(
        const RuntimeCharacterPerceptionState& state,
        const bridge::StableId& characterId) noexcept
    {
        const auto iterator = std::lower_bound(
            state.characters.begin(), state.characters.end(), characterId,
            [](const CharacterCognitionRecord& cognition, const bridge::StableId& id)
            {
                return cognition.characterId < id;
            });
        return iterator != state.characters.end() && iterator->characterId == characterId
            ? &*iterator
            : nullptr;
    }

    [[nodiscard]] inline const CharacterMemoryRecord* FindCharacterMemory(
        const CharacterCognitionRecord& cognition,
        const std::string& subjectId) noexcept
    {
        return perception_detail::FindMemory(cognition, subjectId);
    }

    [[nodiscard]] inline bool InitializeRuntimeCharacterPerception(
        const RuntimeCharacterSystemState& characterSystem,
        RuntimeCharacterPerceptionState& state,
        std::string& error)
    {
        RuntimeCharacterPerceptionState candidate;
        candidate.characters.reserve(characterSystem.characters.size());
        for (const auto& character : characterSystem.characters)
        {
            if (!bridge::IsValidStableId(character.stableEntityId))
            {
                state = {};
                error = "AI-03 perception requires valid Character stable identities.";
                return false;
            }
            CharacterCognitionRecord cognition;
            cognition.characterId = character.stableEntityId;
            const std::uint32_t bucket =
                perception_detail::StableHash(character.stableEntityId) % 20u;
            cognition.nextCognitionTime =
                CharacterCognitionIntervalSeconds * (static_cast<float>(bucket) / 20.0f);
            candidate.characters.push_back(std::move(cognition));
        }
        std::sort(
            candidate.characters.begin(), candidate.characters.end(),
            [](const CharacterCognitionRecord& lhs, const CharacterCognitionRecord& rhs)
            {
                return lhs.characterId < rhs.characterId;
            });
        for (std::size_t index = 1; index < candidate.characters.size(); ++index)
        {
            if (candidate.characters[index - 1].characterId ==
                candidate.characters[index].characterId)
            {
                state = {};
                error = "AI-03 perception received duplicate Character stable identities.";
                return false;
            }
        }
        state = std::move(candidate);
        error.clear();
        return true;
    }

    inline void ResetRuntimeCharacterPerception(
        RuntimeCharacterPerceptionState& state) noexcept
    {
        state = {};
    }

    inline void ApplyVisualObservation(
        CharacterCognitionRecord& cognition,
        const bridge::CharacterTuning& tuning,
        const std::string& subjectId,
        const std::string& subjectFactionId,
        const bridge::FactionRelationship relationship,
        const VisualObservation& observation,
        const float dt) noexcept
    {
        if (!observation.visible)
        {
            // Critical anti-cheat boundary: an invisible observation never
            // consumes its position/velocity fields. Last-known information is
            // therefore preserved from the last legitimate stimulus.
            perception_detail::MarkSubjectSightLost(cognition, subjectId);
            cognition.pendingVisualSubjectId.clear();
            cognition.pendingVisualSeconds = 0.0f;
            perception_detail::ReevaluateAwareness(cognition, tuning);
            return;
        }
        if (!perception_detail::FiniteVector(observation.position) ||
            !perception_detail::FiniteVector(observation.velocity) ||
            !perception_detail::ValidSubjectId(subjectId))
        {
            return;
        }

        if (cognition.pendingVisualSubjectId != subjectId)
        {
            cognition.pendingVisualSubjectId = subjectId;
            cognition.pendingVisualSeconds = 0.0f;
        }
        cognition.pendingVisualSeconds += std::max(0.0f, dt);
        const float reaction = std::max(0.0f, tuning.visualReactionSeconds) *
            (observation.peripheral ? 1.35f : 1.0f);
        if (cognition.pendingVisualSeconds + 0.0001f < reaction)
            return;

        auto& memory = perception_detail::FindOrCreateMemory(cognition, subjectId);
        memory.subjectFactionId = subjectFactionId;
        memory.source = KnowledgeSource::Seen;
        memory.lastKnownPosition = observation.position;
        memory.lastKnownVelocity = observation.velocity;
        memory.confidence = observation.peripheral ? 0.75f : 1.0f;
        memory.threat = perception_detail::RelationshipThreat(relationship);
        memory.ageSeconds = 0.0f;
        memory.secondsSinceSeen = 0.0f;
        memory.memoryLifetimeSeconds = std::max(0.25f, tuning.memorySeconds);
        memory.hasPosition = true;
        memory.hostile = perception_detail::RelationshipHostile(relationship);
        memory.directSight = true;

        const float gain = tuning.suspicionGain * std::max(0.0f, dt) *
            (0.75f + tuning.alertness * 0.75f) *
            (observation.peripheral ? 0.65f : 1.0f) *
            std::max(0.15f, memory.threat);
        cognition.suspicion = std::clamp(cognition.suspicion + gain, 0.0f, 100.0f);
        perception_detail::ReevaluateAwareness(cognition, tuning);
    }

    inline void ApplyHeardStimulus(
        CharacterCognitionRecord& cognition,
        const bridge::CharacterTuning& tuning,
        const SoundStimulus& stimulus,
        const bridge::FactionRelationship relationship) noexcept
    {
        if (!perception_detail::ValidSubjectId(stimulus.sourceSubjectId) ||
            !perception_detail::FiniteVector(stimulus.position) ||
            !perception_detail::FiniteVector(stimulus.velocity))
        {
            return;
        }
        auto& memory = perception_detail::FindOrCreateMemory(
            cognition, stimulus.sourceSubjectId);
        memory.subjectFactionId = stimulus.sourceFactionId;
        memory.source = KnowledgeSource::Heard;
        memory.lastKnownPosition = stimulus.position;
        memory.lastKnownVelocity = stimulus.velocity;
        memory.confidence = perception_detail::Clamp01(0.45f + stimulus.intensity * 0.45f);
        memory.threat = perception_detail::RelationshipThreat(relationship);
        memory.ageSeconds = 0.0f;
        memory.memoryLifetimeSeconds = std::max(0.25f, tuning.memorySeconds);
        memory.hasPosition = true;
        memory.hostile = perception_detail::RelationshipHostile(relationship);
        memory.directSight = false;

        const float gain = tuning.suspicionGain *
            std::clamp(stimulus.intensity, 0.05f, 2.0f) *
            (0.35f + tuning.curiosity * 0.45f + tuning.alertness * 0.20f) *
            std::max(0.20f, memory.threat);
        cognition.suspicion = std::clamp(cognition.suspicion + gain, 0.0f, 100.0f);
        perception_detail::ReevaluateAwareness(cognition, tuning);
    }

    [[nodiscard]] inline bool ReportDamageStimulus(
        const RuntimeCharacterSystemState& characterSystem,
        RuntimeCharacterPerceptionState& state,
        const bridge::StableId& damagedCharacterId,
        const std::string& sourceSubjectId,
        const std::string& sourceFactionId,
        const XMFLOAT3& legitimateKnownPosition,
        const XMFLOAT3& legitimateKnownVelocity,
        const float damageAmount,
        std::string& error)
    {
        const auto* character = FindRuntimeCharacter(characterSystem, damagedCharacterId);
        auto* cognition = FindCharacterCognition(state, damagedCharacterId);
        if (character == nullptr || cognition == nullptr)
        {
            error = "Damage perception target is not an active Runtime Character.";
            return false;
        }
        if (!perception_detail::ValidSubjectId(sourceSubjectId) ||
            !perception_detail::FiniteVector(legitimateKnownPosition) ||
            !perception_detail::FiniteVector(legitimateKnownVelocity))
        {
            error = "Damage perception received invalid source information.";
            return false;
        }
        if (!sourceFactionId.empty())
        {
            std::string factionError;
            if (!bridge::ValidateFactionId(sourceFactionId, factionError))
            {
                error = "Damage perception received an invalid source faction.";
                return false;
            }
        }

        const float damage = std::max(
            0.0f,
            std::isfinite(damageAmount) ? damageAmount : 0.0f);
        const auto relationship = bridge::DefaultFactionRelationship(
            character->authoring.factionId, sourceFactionId);
        auto& memory = perception_detail::FindOrCreateMemory(*cognition, sourceSubjectId);
        memory.subjectFactionId = sourceFactionId;
        memory.source = KnowledgeSource::DamagedBy;
        memory.lastKnownPosition = legitimateKnownPosition;
        memory.lastKnownVelocity = legitimateKnownVelocity;
        memory.confidence = 1.0f;
        memory.threat = std::max(0.75f, perception_detail::RelationshipThreat(relationship));
        memory.ageSeconds = 0.0f;
        memory.memoryLifetimeSeconds = std::max(0.25f, character->tuning.memorySeconds);
        memory.hasPosition = true;
        memory.hostile = relationship == bridge::FactionRelationship::Hostile ||
            (damage > 0.0f &&
             relationship != bridge::FactionRelationship::Ally &&
             relationship != bridge::FactionRelationship::Friendly);
        memory.directSight = false;

        cognition->suspicion = std::clamp(
            std::max(cognition->suspicion, character->tuning.alertThreshold) +
                std::min(30.0f, damage),
            0.0f,
            100.0f);
        ++state.damageStimuli;
        perception_detail::ReevaluateAwareness(*cognition, character->tuning);
        error.clear();
        return true;
    }

    [[nodiscard]] inline std::uint64_t EmitSoundStimulus(
        RuntimeCharacterPerceptionState& state,
        std::string sourceSubjectId,
        std::string sourceFactionId,
        const XMFLOAT3& position,
        const XMFLOAT3& velocity,
        const float radius,
        const float intensity,
        const float lifetimeSeconds = 3.0f)
    {
        if (!perception_detail::ValidSubjectId(sourceSubjectId) ||
            !perception_detail::FiniteVector(position) ||
            !perception_detail::FiniteVector(velocity) ||
            !std::isfinite(radius) || radius <= 0.0f ||
            !std::isfinite(intensity) || intensity <= 0.0f)
        {
            return 0;
        }
        if (!sourceFactionId.empty())
        {
            std::string ignored;
            if (!bridge::ValidateFactionId(sourceFactionId, ignored))
                return 0;
        }
        if (state.sounds.size() >= MaxSoundStimuli)
        {
            state.sounds.erase(state.sounds.begin());
            ++state.droppedSounds;
        }
        SoundStimulus stimulus;
        stimulus.sequence = state.nextSoundSequence++;
        stimulus.sourceSubjectId = std::move(sourceSubjectId);
        stimulus.sourceFactionId = std::move(sourceFactionId);
        stimulus.position = position;
        stimulus.velocity = velocity;
        stimulus.radius = std::max(0.01f, radius);
        stimulus.intensity = std::clamp(intensity, 0.05f, 2.0f);
        stimulus.createdTime = state.elapsedSeconds;
        stimulus.expiresTime = state.elapsedSeconds +
            std::max(0.25f, std::isfinite(lifetimeSeconds) ? lifetimeSeconds : 3.0f);
        state.sounds.push_back(std::move(stimulus));
        return state.sounds.back().sequence;
    }

    [[nodiscard]] inline bool SamplePlayerVisualObservation(
        wi::scene::Scene& scene,
        const RuntimeCharacterRecord& observer,
        const bridge::RuntimePlayerState& player,
        const bridge::PlayerControllerSettings& playerSettings,
        VisualObservation& observation,
        std::uint64_t& lineOfSightQueries) noexcept
    {
        observation = {};
        if (!player.IsSpawned())
            return false;
        auto* nativeCharacter = scene.characters.GetComponent(observer.entity);
        if (nativeCharacter == nullptr || !nativeCharacter->IsActive())
            return false;

        XMFLOAT3 playerPosition;
        if (!bridge::GetPhysicsPosition(scene, player.entity, playerPosition))
            return false;
        XMFLOAT3 playerVelocity(0, 0, 0);
        (void)bridge::GetLinearVelocity(scene, player.entity, playerVelocity);

        XMFLOAT3 observerEye = nativeCharacter->GetPositionInterpolated();
        observerEye.y += std::max(
            0.25f,
            nativeCharacter->height * nativeCharacter->scale * 0.80f);
        XMFLOAT3 targetPoint = playerPosition;
        targetPoint.y += bridge::SanitizePlayerControllerSettings(playerSettings).eyeHeight * 0.75f;
        const XMFLOAT3 offset = perception_detail::Subtract(targetPoint, observerEye);
        const float distance = perception_detail::Length(offset);
        if (!(distance > 0.001f) || !std::isfinite(distance))
            return false;

        const XMFLOAT3 direction = perception_detail::Normalize(offset);
        const XMFLOAT3 facing = nativeCharacter->GetFacingSmoothed();
        const float horizontalAngle = perception_detail::HorizontalAngleDegrees(facing, direction);
        const float verticalAngle = perception_detail::VerticalAngleDegrees(direction);

        const bool central =
            distance <= observer.tuning.visionDistance &&
            horizontalAngle <= observer.tuning.horizontalFovDegrees * 0.5f &&
            verticalAngle <= observer.tuning.verticalFovDegrees * 0.5f;
        const bool peripheral =
            !central &&
            distance <= observer.tuning.peripheralVisionDistance &&
            horizontalAngle <= std::min(
                160.0f,
                observer.tuning.horizontalFovDegrees * 0.85f + 55.0f) &&
            verticalAngle <= std::min(
                80.0f,
                observer.tuning.verticalFovDegrees * 0.75f + 25.0f);
        if (!central && !peripheral)
            return false;

        ++lineOfSightQueries;
        const float rayMax = std::max(0.0f, distance - 0.15f);
        if (rayMax > 0.05f &&
            perception_detail::LineOfSightBlocked(
                scene,
                observer.entity,
                observerEye,
                direction,
                rayMax))
        {
            return false;
        }

        observation.visible = true;
        observation.peripheral = peripheral;
        observation.position = playerPosition;
        observation.velocity = playerVelocity;
        return true;
    }

    inline void ExpireSoundStimuli(RuntimeCharacterPerceptionState& state)
    {
        state.sounds.erase(
            std::remove_if(
                state.sounds.begin(), state.sounds.end(),
                [&](const SoundStimulus& sound)
                {
                    return sound.expiresTime < state.elapsedSeconds;
                }),
            state.sounds.end());
    }

    inline void EmitPlayerFootstepStimulus(
        wi::scene::Scene& scene,
        const bridge::RuntimePlayerState& player,
        const bridge::PlayerControllerSettings& playerSettings,
        RuntimeCharacterPerceptionState& state)
    {
        if (!player.IsSpawned() || state.elapsedSeconds < state.nextPlayerFootstepTime)
            return;
        XMFLOAT3 position;
        XMFLOAT3 velocity;
        if (!bridge::GetPhysicsPosition(scene, player.entity, position) ||
            !bridge::GetLinearVelocity(scene, player.entity, velocity))
        {
            return;
        }
        const float horizontalSpeed = std::sqrt(
            velocity.x * velocity.x + velocity.z * velocity.z);
        if (horizontalSpeed < 0.35f)
            return;

        const auto safe = bridge::SanitizePlayerControllerSettings(playerSettings);
        const bool sprinting = horizontalSpeed > safe.walkSpeed * 1.20f;
        const float radius = sprinting ? 14.0f : 8.0f;
        const float intensity = sprinting ? 1.0f : 0.65f;
        (void)EmitSoundStimulus(
            state,
            RuntimePlayerKnowledgeId,
            "Player",
            position,
            velocity,
            radius,
            intensity,
            2.0f);
        state.nextPlayerFootstepTime =
            state.elapsedSeconds + (sprinting ? 0.32f : 0.46f);
    }

    inline void ProcessAudibleSound(
        const RuntimeCharacterRecord& character,
        CharacterCognitionRecord& cognition,
        const XMFLOAT3& listenerPosition,
        RuntimeCharacterPerceptionState& state)
    {
        const auto audibleStrength = [&](const SoundStimulus& sound, float& strength)
        {
            if (sound.sequence <= cognition.lastHeardSoundSequence ||
                sound.expiresTime < state.elapsedSeconds)
            {
                return false;
            }
            const float distance = perception_detail::Length(
                perception_detail::Subtract(sound.position, listenerPosition));
            const float audibleRadius = sound.radius *
                std::max(0.0f, character.tuning.hearingSensitivity);
            if (!(audibleRadius > 0.0f) || distance > audibleRadius)
                return false;
            strength = sound.intensity *
                (1.0f - std::clamp(distance / audibleRadius, 0.0f, 1.0f));
            return strength > 0.0f;
        };

        const SoundStimulus* best = nullptr;
        float bestStrength = 0.0f;

        // Once a Character has started reacting to a sound, keep that stimulus
        // authoritative until the reaction completes or the stimulus is no
        // longer audible/available. Repeated footsteps or gunshots therefore
        // cannot continually restart the reaction timer and starve hearing.
        if (cognition.pendingSoundSequence != 0)
        {
            for (const auto& sound : state.sounds)
            {
                if (sound.sequence != cognition.pendingSoundSequence)
                    continue;
                if (audibleStrength(sound, bestStrength))
                    best = &sound;
                break;
            }
            if (best == nullptr)
            {
                cognition.pendingSoundSequence = 0;
                cognition.pendingSoundReadyTime = 0.0f;
            }
        }

        if (cognition.pendingSoundSequence == 0)
        {
            best = nullptr;
            bestStrength = 0.0f;
            for (const auto& sound : state.sounds)
            {
                float strength = 0.0f;
                if (!audibleStrength(sound, strength))
                    continue;
                if (best == nullptr || strength > bestStrength ||
                    (strength == bestStrength && sound.sequence > best->sequence))
                {
                    best = &sound;
                    bestStrength = strength;
                }
            }
            if (best != nullptr)
            {
                cognition.pendingSoundSequence = best->sequence;
                cognition.pendingSoundReadyTime = state.elapsedSeconds +
                    std::max(0.0f, character.tuning.audioReactionSeconds);
            }
        }

        if (best == nullptr)
            return;
        if (state.elapsedSeconds + 0.0001f < cognition.pendingSoundReadyTime)
            return;

        SoundStimulus heard = *best;
        heard.intensity = std::clamp(bestStrength, 0.05f, 2.0f);
        const auto relationship = bridge::DefaultFactionRelationship(
            character.authoring.factionId, heard.sourceFactionId);
        ApplyHeardStimulus(cognition, character.tuning, heard, relationship);
        cognition.lastHeardSoundSequence = std::max(
            cognition.lastHeardSoundSequence, best->sequence);
        cognition.pendingSoundSequence = 0;
        cognition.pendingSoundReadyTime = 0.0f;
        ++state.heardStimuli;
    }

    inline void UpdateRuntimeCharacterPerception(
        wi::scene::Scene& scene,
        const RuntimeCharacterSystemState& characterSystem,
        RuntimeCharacterPerceptionState& state,
        const bridge::RuntimePlayerState& player,
        const bridge::PlayerControllerSettings& playerSettings,
        const float dt)
    {
        const float safeDt = std::clamp(
            std::isfinite(dt) ? dt : 0.0f,
            0.0f,
            0.25f);
        state.elapsedSeconds += safeDt;
        ExpireSoundStimuli(state);
        EmitPlayerFootstepStimulus(scene, player, playerSettings, state);

        for (auto& cognition : state.characters)
        {
            const auto* character = FindRuntimeCharacter(
                characterSystem, cognition.characterId);
            if (character == nullptr)
                continue;

            perception_detail::DecayCharacterState(cognition, character->tuning, safeDt);
            if (state.elapsedSeconds + 0.0001f < cognition.nextCognitionTime)
                continue;
            cognition.nextCognitionTime =
                state.elapsedSeconds + CharacterCognitionIntervalSeconds;
            ++cognition.cognitionTicks;

            auto* nativeCharacter = scene.characters.GetComponent(character->entity);
            if (nativeCharacter == nullptr || !nativeCharacter->IsActive())
                continue;
            const XMFLOAT3 listenerPosition = nativeCharacter->GetPositionInterpolated();
            ProcessAudibleSound(*character, cognition, listenerPosition, state);

            VisualObservation observation;
            const bool sampledVisible = SamplePlayerVisualObservation(
                scene,
                *character,
                player,
                playerSettings,
                observation,
                state.lineOfSightQueries);
            if (!sampledVisible)
                observation.visible = false;
            const auto relationship = bridge::DefaultFactionRelationship(
                character->authoring.factionId, "Player");
            const bool hadDirectSight = [&]()
            {
                const auto* memory = FindCharacterMemory(
                    cognition, RuntimePlayerKnowledgeId);
                return memory != nullptr && memory->directSight;
            }();
            ApplyVisualObservation(
                cognition,
                character->tuning,
                RuntimePlayerKnowledgeId,
                "Player",
                relationship,
                observation,
                CharacterCognitionIntervalSeconds);
            const auto* playerMemory = FindCharacterMemory(
                cognition, RuntimePlayerKnowledgeId);
            if (playerMemory != nullptr && playerMemory->directSight && !hadDirectSight)
                ++state.visualDetections;
        }
    }
}
