#pragma once

#include "RuntimeCombatDamage.h"
#include "renegade/bridge/ProjectileSimulation.h"

#include <limits>

namespace renegade::runtime
{
    struct ProjectileOwnerBinding
    {
        std::string subjectId;
        wi::ecs::Entity root = wi::ecs::INVALID_ENTITY;
    };

    inline XMFLOAT3 ProjectileNativeVector(const bridge::ProjectileVector& value)
    {
        return {value.x, value.y, value.z};
    }

    inline bridge::ProjectileVector ProjectileBridgeVector(const XMFLOAT3& value)
    {
        return {value.x, value.y, value.z};
    }

    inline bridge::StableId ProjectileContactSubject(
        const wi::scene::Scene& scene,
        const RuntimeCharacterSystemState& characters,
        wi::ecs::Entity entity)
    {
        // Prefer the governed Character root over identities on imported children.
        for (const auto& character : characters.characters)
        {
            if (perception_detail::EntityBelongsToRoot(scene, entity, character.entity))
                return character.stableEntityId;
        }
        return bridge::PersistentEntityId(scene, entity);
    }

    inline std::string ProjectileContactSurface(
        const wi::scene::Scene& scene,
        const wi::scene::Scene::RayIntersectionResult& hit)
    {
        // Material identity is the current surface lookup seam. Do not infer
        // "metal"/"stone" from filenames, colour or a runtime entity number.
        const auto* object = scene.objects.GetComponent(hit.entity);
        const auto* mesh = object == nullptr ? nullptr : scene.meshes.GetComponent(object->meshID);
        if (mesh == nullptr || hit.subsetIndex < 0 ||
            static_cast<std::size_t>(hit.subsetIndex) >= mesh->subsets.size())
            return {};
        return bridge::PersistentEntityId(scene, mesh->subsets[hit.subsetIndex].materialID);
    }

    // Scene-query slice: mesh instances (including native skinning), terrain
    // objects and CPU scene colliders. Physics-only Jolt bodies are not covered.
    inline bridge::ProjectileQueryResult QueryProjectileSceneSegment(
        const wi::scene::Scene& scene,
        const RuntimeCharacterSystemState& characters,
        const ProjectileOwnerBinding& owner,
        const bridge::ProjectileRecord& projectile,
        const bridge::ProjectileVector& from,
        const bridge::ProjectileVector& to,
        const std::uint32_t layerMask = ~0u)
    {
        using bridge::ProjectileQueryStatus;
        bridge::ProjectileQueryResult result;
        if (owner.root == wi::ecs::INVALID_ENTITY ||
            !scene.transforms.Contains(owner.root) ||
            owner.subjectId != projectile.launch.source.ownerSubjectId ||
            !bridge::FiniteProjectileVector(from) || !bridge::FiniteProjectileVector(to))
        {
            result.status = ProjectileQueryStatus::Blocked;
            return result;
        }
        const XMFLOAT3 delta{to.x - from.x, to.y - from.y, to.z - from.z};
        const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        if (!std::isfinite(length))
        {
            result.status = ProjectileQueryStatus::Blocked;
            return result;
        }
        if (length <= 0.000001f)
            return result;
        const XMFLOAT3 direction{delta.x / length, delta.y / length, delta.z / length};
        const wi::primitive::Ray ray(ProjectileNativeVector(from), direction, 0, length);
        constexpr std::uint32_t filter =
            wi::enums::FILTER_OPAQUE | wi::enums::FILTER_TRANSPARENT |
            wi::enums::FILTER_WATER | wi::enums::FILTER_TERRAIN |
            wi::enums::FILTER_COLLIDER;
        // Detect an origin already inside a CPU collider or on a mesh surface.
        // Owner overlaps are ignored; a world overlap is contact at time zero.
        wi::vector<wi::scene::Scene::SphereIntersectionResult> overlaps;
        scene.IntersectsAll(overlaps,
            wi::primitive::Sphere(ProjectileNativeVector(from), 0.00001f), filter, layerMask);
        for (const auto& overlap : overlaps)
        {
            if (overlap.entity == wi::ecs::INVALID_ENTITY ||
                perception_detail::EntityBelongsToRoot(scene, overlap.entity, owner.root))
                continue;
            result.status = ProjectileQueryStatus::Hit;
            result.contact.position = from;
            // Coincident sphere centres have no unique outward normal in native
            // overlap math. Preserve the confirmed blocker with an incoming-facing
            // fallback, rather than letting the projectile leave the collider.
            const auto normal = ProjectileBridgeVector(overlap.normal);
            result.contact.normal = bridge::FiniteProjectileVector(normal) ? normal :
                bridge::ProjectileVector{-direction.x, -direction.y, -direction.z};
            result.contact.targetSubjectId = ProjectileContactSubject(scene, characters, overlap.entity);
            wi::scene::Scene::RayIntersectionResult surfaceHit;
            surfaceHit.entity = overlap.entity;
            surfaceHit.subsetIndex = overlap.subsetIndex;
            result.contact.surfaceId = ProjectileContactSurface(scene, surfaceHit);
            return result;
        }

        wi::vector<wi::scene::Scene::RayIntersectionResult> hits;
        scene.IntersectsAll(hits, ray, filter, layerMask);

        const wi::scene::Scene::RayIntersectionResult* nearest = nullptr;
        for (const auto& hit : hits)
        {
            if (hit.entity == wi::ecs::INVALID_ENTITY ||
                perception_detail::EntityBelongsToRoot(scene, hit.entity, owner.root))
                continue;
            // Recheck limits: native collider intersection details differ from meshes.
            if (!std::isfinite(hit.distance) ||
                !bridge::FiniteProjectileVector(ProjectileBridgeVector(hit.position)) ||
                !bridge::FiniteProjectileVector(ProjectileBridgeVector(hit.normal)))
            {
                result.status = ProjectileQueryStatus::Blocked;
                return result;
            }
            if (hit.distance < 0 || hit.distance > length)
                continue;
            if (nearest == nullptr || hit.distance < nearest->distance ||
                (hit.distance == nearest->distance && hit.entity < nearest->entity))
                nearest = &hit;
        }
        if (nearest == nullptr)
            return result;
        result.status = ProjectileQueryStatus::Hit;
        result.contact.fraction = std::clamp(nearest->distance / length, 0.0f, 1.0f);
        result.contact.position = ProjectileBridgeVector(nearest->position);
        result.contact.normal = ProjectileBridgeVector(nearest->normal);
        result.contact.targetSubjectId = ProjectileContactSubject(scene, characters, nearest->entity);
        result.contact.surfaceId = ProjectileContactSurface(scene, *nearest);
        return result;
    }

    struct ProjectileDamageResult
    {
        bool characterContact = false;
        bool damageApplied = false;
        std::string error;
    };

    // Consume each simulation-produced impact once in the Runtime update owner.
    // Static world impacts remain presentation contacts with no health mutation.
    inline ProjectileDamageResult ApplyProjectileCharacterImpact(
        wi::scene::Scene& scene,
        const RuntimeCharacterSystemState& characters,
        RuntimeCharacterPerceptionState& perception,
        RuntimeCombatState& combat,
        const bridge::ProjectileImpact& impact,
        const CombatEventEmitter& emitter)
    {
        ProjectileDamageResult result;
        const auto* target = FindRuntimeCharacter(characters, impact.contact.targetSubjectId);
        if (target == nullptr)
            return result;
        result.characterContact = true;
        if (impact.contact.targetSubjectId == impact.source.ownerSubjectId ||
            !bridge::IsValidStableId(impact.source.sourceAssetId))
        {
            result.error = "Projectile impact has invalid source identity or self target.";
            return result;
        }
        if (impact.damage == 0)
            return result;
        result.damageApplied = ApplyAttributedCombatDamage(
            scene, characters, perception, combat, target->stableEntityId,
            impact.source.ownerSubjectId, impact.source.factionId,
            ProjectileNativeVector(impact.source.knownPosition),
            ProjectileNativeVector(impact.source.knownVelocity),
            impact.damage, emitter, result.error);
        return result;
    }
}
