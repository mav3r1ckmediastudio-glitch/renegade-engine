#pragma once

#include "renegade/bridge/CharacterProfileService.h"
#include "renegade/bridge/FactionService.h"
#include "renegade/bridge/IdentityService.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace renegade::runtime
{
    struct ResolvedCharacterReferences
    {
        wi::ecs::Entity patrolRouteEntity = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity weaponEntity = wi::ecs::INVALID_ENTITY;
    };

    struct RuntimeCharacterRecord
    {
        bridge::StableId stableEntityId;
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        bridge::CharacterAuthoringSettings authoring;
        bridge::CharacterAdvancedOverrides advancedOverrides;
        bridge::CharacterTuning tuning;
        ResolvedCharacterReferences references;
    };

    struct RuntimeCharacterSystemState
    {
        // Known faction IDs are Runtime-transient registry state. Built-ins are
        // seeded automatically and creator-defined factions are registered from
        // authored Characters during initialization. No perception knowledge is
        // stored here.
        bridge::FactionRegistry factions;
        std::vector<RuntimeCharacterRecord> characters;
    };

    [[nodiscard]] inline bool ValidateRuntimeCharacterMarkers(
        const wi::scene::Scene& scene,
        std::string& error)
    {
        // AI-01's IsRenegadeCharacter() intentionally recognizes only the
        // current schema. Runtime must still notice an owned Character marker
        // whose schema is missing/unsupported; otherwise corrupt or future
        // authoring could be silently skipped and look like "no Character".
        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            const auto& metadata = scene.metadatas[index];
            if (!metadata.string_values.has(bridge::CharacterMetadataKey))
                continue;

            if (metadata.string_values.get(bridge::CharacterMetadataKey) != "1")
            {
                error = "Character metadata contains an unsupported marker value.";
                return false;
            }
            if (!metadata.int_values.has(bridge::CharacterSchemaVersionMetadataKey))
            {
                error = "Character metadata is missing its schema version.";
                return false;
            }
            if (metadata.int_values.get(bridge::CharacterSchemaVersionMetadataKey) !=
                bridge::CharacterSchemaVersion)
            {
                error = "Character metadata uses an unsupported schema version.";
                return false;
            }
        }
        error.clear();
        return true;
    }

    [[nodiscard]] inline bool InitializeRuntimeCharacterSystem(
        const wi::scene::Scene& scene,
        const bridge::CharacterRuntimeState& foundation,
        RuntimeCharacterSystemState& state,
        std::string& error)
    {
        // Build the candidate completely before publishing it so a malformed
        // faction/profile/reference never leaves a partial Runtime AI-02 state.
        RuntimeCharacterSystemState candidate;
        candidate.characters.reserve(foundation.characters.size());

        if (!ValidateRuntimeCharacterMarkers(scene, error))
        {
            state = {};
            return false;
        }

        bridge::EntityIdentityIndex identityIndex;
        if (!identityIndex.Build(scene, error))
        {
            state = {};
            return false;
        }

        for (const auto& foundationRecord : foundation.characters)
        {
            if (!bridge::IsValidStableId(foundationRecord.characterId))
            {
                state = {};
                error = "Runtime Character has an invalid stable identity.";
                return false;
            }
            // CaptureCharacterSettings deliberately supplies backwards-compatible
            // defaults for ordinary readers. Runtime AI must additionally inspect
            // the persisted profile fields so corrupt/unsupported enum metadata
            // cannot silently turn into a different Character profile.
            if (!bridge::ValidateCharacterProfileAuthoring(
                    scene, foundationRecord.entity, error))
            {
                state = {};
                return false;
            }
            if (!candidate.factions.Register(
                    foundationRecord.settings.factionId, error))
            {
                state = {};
                return false;
            }

            RuntimeCharacterRecord record;
            record.stableEntityId = foundationRecord.characterId;
            record.entity = foundationRecord.entity;
            record.authoring = foundationRecord.settings;

            if (!bridge::CaptureCharacterAdvancedOverrides(
                    scene, record.entity, record.advancedOverrides, error))
            {
                state = {};
                return false;
            }
            record.tuning = bridge::ResolveCharacterTuning(
                record.authoring, record.advancedOverrides);

            if (!record.authoring.patrolRouteEntityId.empty())
            {
                record.references.patrolRouteEntity = identityIndex.Resolve(
                    record.authoring.patrolRouteEntityId);
                if (record.references.patrolRouteEntity == wi::ecs::INVALID_ENTITY)
                {
                    state = {};
                    error = "Character '" + record.stableEntityId +
                        "' references a missing patrol route stable ID.";
                    return false;
                }
            }

            if (!record.authoring.weaponEntityId.empty())
            {
                record.references.weaponEntity = identityIndex.Resolve(
                    record.authoring.weaponEntityId);
                if (record.references.weaponEntity == wi::ecs::INVALID_ENTITY)
                {
                    state = {};
                    error = "Character '" + record.stableEntityId +
                        "' references a missing weapon stable ID.";
                    return false;
                }
            }

            candidate.characters.push_back(std::move(record));
        }

        std::sort(
            candidate.characters.begin(), candidate.characters.end(),
            [](const RuntimeCharacterRecord& lhs, const RuntimeCharacterRecord& rhs)
            {
                if (lhs.stableEntityId != rhs.stableEntityId)
                    return lhs.stableEntityId < rhs.stableEntityId;
                return lhs.entity < rhs.entity;
            });

        state = std::move(candidate);
        error.clear();
        return true;
    }

    inline void ResetRuntimeCharacterSystem(RuntimeCharacterSystemState& state) noexcept
    {
        state.characters.clear();
        state.factions.ResetToBuiltIns();
    }

    [[nodiscard]] inline const RuntimeCharacterRecord* FindRuntimeCharacter(
        const RuntimeCharacterSystemState& state,
        const bridge::StableId& stableEntityId) noexcept
    {
        const auto iterator = std::lower_bound(
            state.characters.begin(), state.characters.end(), stableEntityId,
            [](const RuntimeCharacterRecord& record, const bridge::StableId& id)
            {
                return record.stableEntityId < id;
            });
        return iterator != state.characters.end() && iterator->stableEntityId == stableEntityId
            ? &*iterator
            : nullptr;
    }

    [[nodiscard]] inline bridge::FactionRelationship RelationshipBetween(
        const RuntimeCharacterRecord& observer,
        const RuntimeCharacterRecord& subject) noexcept
    {
        // Relationship is semantic only. It deliberately does not imply that
        // observer has detected, perceived or remembered subject.
        return bridge::DefaultFactionRelationship(
            observer.authoring.factionId, subject.authoring.factionId);
    }
}
