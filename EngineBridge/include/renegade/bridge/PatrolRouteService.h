#pragma once

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <WickedEngine.h>

namespace renegade::bridge
{
    inline constexpr const char* PatrolRouteMetadataKey =
        "renegade.ai.patrol_route";
    inline constexpr const char* PatrolRouteMetadataVersion = "1";
    inline constexpr const char* PatrolRouteModeMetadataKey =
        "renegade.ai.patrol_route.mode";
    inline constexpr const char* PatrolRouteWaitMetadataKey =
        "renegade.ai.patrol_route.wait_seconds";
    inline constexpr const char* PatrolRoutePointMetadataKey =
        "renegade.ai.patrol_point";
    inline constexpr const char* PatrolRoutePointMetadataVersion = "1";
    inline constexpr const char* PatrolRoutePointOrderMetadataKey =
        "renegade.ai.patrol_point.order";

    enum class PatrolRouteMode : std::int32_t
    {
        Loop = 0,
        PingPong = 1,
        Random = 2,
    };

    struct PatrolRouteSettings
    {
        PatrolRouteMode mode = PatrolRouteMode::Loop;
        float waitSeconds = 1.0f;
    };

    struct PatrolRoutePoint
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        StableId stableId;
        std::int32_t order = 0;
        XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    };

    struct PatrolRoute
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        StableId stableId;
        PatrolRouteSettings settings;
        std::vector<PatrolRoutePoint> points;
    };

    [[nodiscard]] inline PatrolRouteSettings SanitizePatrolRouteSettings(
        PatrolRouteSettings settings) noexcept
    {
        const auto rawMode = static_cast<std::int32_t>(settings.mode);
        if (rawMode < static_cast<std::int32_t>(PatrolRouteMode::Loop) ||
            rawMode > static_cast<std::int32_t>(PatrolRouteMode::Random))
        {
            settings.mode = PatrolRouteMode::Loop;
        }
        if (!std::isfinite(settings.waitSeconds))
            settings.waitSeconds = 1.0f;
        settings.waitSeconds = std::clamp(settings.waitSeconds, 0.0f, 60.0f);
        return settings;
    }

    [[nodiscard]] inline bool IsPatrolRoute(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata != nullptr &&
            metadata->string_values.has(PatrolRouteMetadataKey) &&
            metadata->string_values.get(PatrolRouteMetadataKey) ==
                PatrolRouteMetadataVersion;
    }

    [[nodiscard]] inline bool IsPatrolRoutePoint(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata != nullptr &&
            metadata->string_values.has(PatrolRoutePointMetadataKey) &&
            metadata->string_values.get(PatrolRoutePointMetadataKey) ==
                PatrolRoutePointMetadataVersion;
    }

    [[nodiscard]] inline PatrolRouteSettings CapturePatrolRouteSettings(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        PatrolRouteSettings settings;
        const auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata == nullptr)
            return settings;
        if (metadata->int_values.has(PatrolRouteModeMetadataKey))
        {
            settings.mode = static_cast<PatrolRouteMode>(
                metadata->int_values.get(PatrolRouteModeMetadataKey));
        }
        if (metadata->float_values.has(PatrolRouteWaitMetadataKey))
            settings.waitSeconds = metadata->float_values.get(PatrolRouteWaitMetadataKey);
        return SanitizePatrolRouteSettings(settings);
    }

    inline void ApplyPatrolRouteSettings(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const PatrolRouteSettings& requested)
    {
        auto& metadata = scene.metadatas.Contains(entity)
            ? *scene.metadatas.GetComponent(entity)
            : scene.metadatas.Create(entity);
        const auto settings = SanitizePatrolRouteSettings(requested);
        metadata.string_values.set(PatrolRouteMetadataKey, PatrolRouteMetadataVersion);
        metadata.int_values.set(
            PatrolRouteModeMetadataKey,
            static_cast<std::int32_t>(settings.mode));
        metadata.float_values.set(PatrolRouteWaitMetadataKey, settings.waitSeconds);
    }

    [[nodiscard]] inline bool CapturePatrolRoute(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity routeEntity,
        PatrolRoute& route,
        std::string& error)
    {
        route = {};
        if (!IsPatrolRoute(scene, routeEntity))
        {
            error = "The referenced entity is not a Renegade Patrol Route.";
            return false;
        }
        route.entity = routeEntity;
        route.stableId = PersistentEntityId(scene, routeEntity);
        if (!IsValidStableId(route.stableId))
        {
            error = "Patrol Route is missing a valid persistent identity.";
            return false;
        }
        route.settings = CapturePatrolRouteSettings(scene, routeEntity);

        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            const wi::ecs::Entity entity = scene.metadatas.GetEntity(index);
            if (!IsPatrolRoutePoint(scene, entity))
                continue;
            const auto* hierarchy = scene.hierarchy.GetComponent(entity);
            if (hierarchy == nullptr || hierarchy->parentID != routeEntity)
                continue;
            const auto* transform = scene.transforms.GetComponent(entity);
            const auto* metadata = scene.metadatas.GetComponent(entity);
            if (transform == nullptr || metadata == nullptr)
            {
                error = "Patrol Route contains a point without Transform/Metadata state.";
                return false;
            }
            PatrolRoutePoint point;
            point.entity = entity;
            point.stableId = PersistentEntityId(scene, entity);
            if (!IsValidStableId(point.stableId))
            {
                error = "Patrol Route contains a point without a valid persistent identity.";
                return false;
            }
            if (metadata->int_values.has(PatrolRoutePointOrderMetadataKey))
                point.order = metadata->int_values.get(PatrolRoutePointOrderMetadataKey);
            point.position = transform->GetPosition();
            route.points.push_back(std::move(point));
        }

        std::sort(route.points.begin(), route.points.end(),
            [](const PatrolRoutePoint& lhs, const PatrolRoutePoint& rhs)
            {
                if (lhs.order != rhs.order)
                    return lhs.order < rhs.order;
                return lhs.stableId < rhs.stableId;
            });
        for (std::size_t index = 0; index < route.points.size(); ++index)
        {
            if (route.points[index].order != static_cast<std::int32_t>(index))
            {
                error = "Patrol Route point ordering is malformed or contains gaps.";
                return false;
            }
        }
        error.clear();
        return true;
    }

    [[nodiscard]] inline std::vector<wi::ecs::Entity> CollectPatrolRoutes(
        const wi::scene::Scene& scene)
    {
        std::vector<std::pair<StableId, wi::ecs::Entity>> ordered;
        ordered.reserve(scene.metadatas.GetCount());
        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            const wi::ecs::Entity entity = scene.metadatas.GetEntity(index);
            if (!IsPatrolRoute(scene, entity))
                continue;
            ordered.emplace_back(PersistentEntityId(scene, entity), entity);
        }
        std::sort(ordered.begin(), ordered.end(),
            [](const auto& lhs, const auto& rhs)
            {
                if (lhs.first != rhs.first)
                    return lhs.first < rhs.first;
                return lhs.second < rhs.second;
            });
        std::vector<wi::ecs::Entity> result;
        result.reserve(ordered.size());
        for (const auto& [id, entity] : ordered)
            result.push_back(entity);
        return result;
    }

    [[nodiscard]] inline wi::ecs::Entity FindDefaultNavigationGrid(
        const wi::scene::Scene& scene) noexcept
    {
        wi::ecs::Entity found = wi::ecs::INVALID_ENTITY;
        StableId foundId;
        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            const wi::ecs::Entity entity = scene.metadatas.GetEntity(index);
            const auto* metadata = scene.metadatas.GetComponent(entity);
            if (metadata == nullptr ||
                !metadata->string_values.has("renegade.navigation.grid") ||
                metadata->string_values.get("renegade.navigation.grid") != "1")
            {
                continue;
            }
            const StableId id = PersistentEntityId(scene, entity);
            if (found == wi::ecs::INVALID_ENTITY || id < foundId)
            {
                found = entity;
                foundId = id;
            }
        }
        return found;
    }

    class CreatePatrolRouteCommand final : public ICommand
    {
    public:
        CreatePatrolRouteCommand(
            wi::scene::Scene& scene,
            const XMFLOAT3 position,
            PatrolRouteSettings settings = {})
            : scene_(&scene), position_(position), settings_(SanitizePatrolRouteSettings(settings))
        {
        }

        bool Execute() override
        {
            if (scene_ == nullptr)
                return false;
            if (routeStableId_.empty())
                routeStableId_ = GenerateStableId();
            if (pointStableId_.empty())
                pointStableId_ = GenerateStableId();
            if (!IsValidStableId(routeStableId_) || !IsValidStableId(pointStableId_))
                return false;

            routeEntity_ = scene_->Entity_CreateTransform("Patrol Route");
            if (routeEntity_ == wi::ecs::INVALID_ENTITY)
                return false;
            auto* routeTransform = scene_->transforms.GetComponent(routeEntity_);
            if (routeTransform == nullptr)
            {
                scene_->Entity_Remove(routeEntity_);
                routeEntity_ = wi::ecs::INVALID_ENTITY;
                return false;
            }
            routeTransform->Translate(position_);
            routeTransform->UpdateTransform();
            std::string error;
            if (!AssignPersistentEntityId(*scene_, routeEntity_, routeStableId_, error))
            {
                scene_->Entity_Remove(routeEntity_);
                routeEntity_ = wi::ecs::INVALID_ENTITY;
                return false;
            }
            ApplyPatrolRouteSettings(*scene_, routeEntity_, settings_);

            pointEntity_ = scene_->Entity_CreateTransform("Patrol Point 1");
            if (pointEntity_ == wi::ecs::INVALID_ENTITY)
            {
                scene_->Entity_Remove(routeEntity_);
                routeEntity_ = wi::ecs::INVALID_ENTITY;
                return false;
            }
            auto* pointTransform = scene_->transforms.GetComponent(pointEntity_);
            if (pointTransform == nullptr)
            {
                scene_->Entity_Remove(routeEntity_);
                routeEntity_ = wi::ecs::INVALID_ENTITY;
                pointEntity_ = wi::ecs::INVALID_ENTITY;
                return false;
            }
            pointTransform->Translate(position_);
            pointTransform->UpdateTransform();
            if (!AssignPersistentEntityId(*scene_, pointEntity_, pointStableId_, error))
            {
                scene_->Entity_Remove(routeEntity_);
                routeEntity_ = wi::ecs::INVALID_ENTITY;
                pointEntity_ = wi::ecs::INVALID_ENTITY;
                return false;
            }
            auto& pointMetadata = scene_->metadatas.Contains(pointEntity_)
                ? *scene_->metadatas.GetComponent(pointEntity_)
                : scene_->metadatas.Create(pointEntity_);
            pointMetadata.string_values.set(
                PatrolRoutePointMetadataKey, PatrolRoutePointMetadataVersion);
            pointMetadata.int_values.set(PatrolRoutePointOrderMetadataKey, 0);
            scene_->Component_Attach(pointEntity_, routeEntity_, false);
            return true;
        }

        void Undo() override
        {
            if (scene_ != nullptr && routeEntity_ != wi::ecs::INVALID_ENTITY)
                scene_->Entity_Remove(routeEntity_, true);
            routeEntity_ = wi::ecs::INVALID_ENTITY;
            pointEntity_ = wi::ecs::INVALID_ENTITY;
        }

        [[nodiscard]] wi::ecs::Entity CreatedEntity() const noexcept
        {
            return routeEntity_;
        }
        [[nodiscard]] const StableId& CreatedStableId() const noexcept
        {
            return routeStableId_;
        }

    private:
        wi::scene::Scene* scene_ = nullptr;
        XMFLOAT3 position_ = XMFLOAT3(0.0f, 0.0f, 0.0f);
        PatrolRouteSettings settings_;
        StableId routeStableId_;
        StableId pointStableId_;
        wi::ecs::Entity routeEntity_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity pointEntity_ = wi::ecs::INVALID_ENTITY;
    };

    class SetPatrolRouteSettingsCommand final : public ICommand
    {
    public:
        SetPatrolRouteSettingsCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity routeEntity,
            PatrolRouteSettings after)
            : scene_(&scene), entity_(routeEntity), after_(SanitizePatrolRouteSettings(after))
        {
        }

        bool Execute() override
        {
            if (scene_ == nullptr || !IsPatrolRoute(*scene_, entity_))
                return false;
            before_ = CapturePatrolRouteSettings(*scene_, entity_);
            if (before_.mode == after_.mode &&
                std::abs(before_.waitSeconds - after_.waitSeconds) < 0.0001f)
                return false;
            ApplyPatrolRouteSettings(*scene_, entity_, after_);
            return true;
        }

        void Undo() override
        {
            if (scene_ != nullptr && IsPatrolRoute(*scene_, entity_))
                ApplyPatrolRouteSettings(*scene_, entity_, before_);
        }

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        PatrolRouteSettings before_;
        PatrolRouteSettings after_;
    };

    class AddPatrolRoutePointCommand final : public ICommand
    {
    public:
        AddPatrolRoutePointCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity routeEntity,
            const XMFLOAT3 position)
            : scene_(&scene), routeEntity_(routeEntity), position_(position)
        {
        }

        bool Execute() override
        {
            if (scene_ == nullptr || !IsPatrolRoute(*scene_, routeEntity_))
                return false;
            PatrolRoute route;
            std::string error;
            if (!CapturePatrolRoute(*scene_, routeEntity_, route, error))
                return false;
            if (pointStableId_.empty())
                pointStableId_ = GenerateStableId();
            if (!IsValidStableId(pointStableId_))
                return false;
            order_ = static_cast<std::int32_t>(route.points.size());
            pointEntity_ = scene_->Entity_CreateTransform(
                "Patrol Point " + std::to_string(order_ + 1));
            if (pointEntity_ == wi::ecs::INVALID_ENTITY)
                return false;
            auto* transform = scene_->transforms.GetComponent(pointEntity_);
            if (transform == nullptr)
            {
                scene_->Entity_Remove(pointEntity_);
                pointEntity_ = wi::ecs::INVALID_ENTITY;
                return false;
            }
            transform->Translate(position_);
            transform->UpdateTransform();
            if (!AssignPersistentEntityId(*scene_, pointEntity_, pointStableId_, error))
            {
                scene_->Entity_Remove(pointEntity_);
                pointEntity_ = wi::ecs::INVALID_ENTITY;
                return false;
            }
            auto& metadata = scene_->metadatas.Contains(pointEntity_)
                ? *scene_->metadatas.GetComponent(pointEntity_)
                : scene_->metadatas.Create(pointEntity_);
            metadata.string_values.set(
                PatrolRoutePointMetadataKey, PatrolRoutePointMetadataVersion);
            metadata.int_values.set(PatrolRoutePointOrderMetadataKey, order_);
            scene_->Component_Attach(pointEntity_, routeEntity_, false);
            return true;
        }

        void Undo() override
        {
            if (scene_ != nullptr && pointEntity_ != wi::ecs::INVALID_ENTITY)
                scene_->Entity_Remove(pointEntity_);
            pointEntity_ = wi::ecs::INVALID_ENTITY;
        }

        [[nodiscard]] wi::ecs::Entity CreatedEntity() const noexcept
        {
            return pointEntity_;
        }

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity routeEntity_ = wi::ecs::INVALID_ENTITY;
        XMFLOAT3 position_ = XMFLOAT3(0.0f, 0.0f, 0.0f);
        StableId pointStableId_;
        std::int32_t order_ = 0;
        wi::ecs::Entity pointEntity_ = wi::ecs::INVALID_ENTITY;
    };
}
