#pragma once

#include <WickedEngine.h>

#include <algorithm>
#include <cmath>
#include <memory>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    struct SoftBodyPhysicsState
    {
        float detail = 1.0f;
        float mass = 1.0f;
        float friction = 0.5f;
        float restitution = 0.0f;
        float pressure = 0.0f;
        float vertexRadius = 0.2f;
        bool windEnabled = false;
    };

    namespace softbody_physics_detail
    {
        inline constexpr float Epsilon = 0.00001f;

        inline float FiniteOr(const float value, const float fallback) noexcept
        {
            return std::isfinite(value) ? value : fallback;
        }

        inline bool NearlyEqual(const float left, const float right) noexcept
        {
            return std::abs(left - right) <= Epsilon;
        }

        inline void RefreshMeshRenderData(wi::scene::MeshComponent& mesh)
        {
            // The editor has a graphics device and must immediately rebuild the
            // mesh after soft-body-only bone streams change. JP01 unit tests are
            // intentionally GPU-free, so keep the data cleanup testable without
            // manufacturing a renderer just to exercise this command path.
            if (wi::graphics::GetDevice() != nullptr)
                mesh.CreateRenderData();
        }
    }

    // Soft-body physics is stored on the Mesh entity. Accept either the mesh
    // itself or an Object whose meshID points at it.
    [[nodiscard]] inline wi::ecs::Entity ResolveSoftBodyMeshEntity(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        if (entity == wi::ecs::INVALID_ENTITY)
            return wi::ecs::INVALID_ENTITY;
        if (scene.meshes.Contains(entity))
            return entity;
        const auto* object = scene.objects.GetComponent(entity);
        if (object != nullptr && scene.meshes.Contains(object->meshID))
            return object->meshID;
        return wi::ecs::INVALID_ENTITY;
    }

    enum class SoftBodyCreationBlock
    {
        None,
        MissingMesh,
        AlreadyExists,
        RigidBody,
        SharedMesh,
    };

    [[nodiscard]] inline SoftBodyCreationBlock GetSoftBodyCreationBlock(
        const wi::scene::Scene& scene, const wi::ecs::Entity selectedEntity) noexcept
    {
        const auto mesh = ResolveSoftBodyMeshEntity(scene, selectedEntity);
        if (mesh == wi::ecs::INVALID_ENTITY)
            return SoftBodyCreationBlock::MissingMesh;
        if (scene.softbodies.Contains(mesh))
            return SoftBodyCreationBlock::AlreadyExists;

        bool foundInstance = false;
        for (size_t i = 0; i < scene.objects.GetCount(); ++i)
        {
            if (scene.objects[i].meshID != mesh)
                continue;
            // Wicked stores both the deformation and the world pose on the mesh.
            if (foundInstance)
                return SoftBodyCreationBlock::SharedMesh;
            foundInstance = true;
            auto entity = scene.objects.GetEntity(i);
            while (entity != wi::ecs::INVALID_ENTITY)
            {
                // Intact destructibles can own the rigid collider on a parent.
                if (scene.rigidbodies.Contains(entity))
                    return SoftBodyCreationBlock::RigidBody;
                const auto* hierarchy = scene.hierarchy.GetComponent(entity);
                entity = hierarchy ? hierarchy->parentID : wi::ecs::INVALID_ENTITY;
            }
        }
        return SoftBodyCreationBlock::None;
    }

    [[nodiscard]] inline SoftBodyPhysicsState CaptureSoftBodyPhysics(
        const wi::scene::SoftBodyPhysicsComponent& body) noexcept
    {
        SoftBodyPhysicsState state;
        state.detail = body.detail;
        state.mass = body.mass;
        state.friction = body.friction;
        state.restitution = body.restitution;
        state.pressure = body.pressure;
        state.vertexRadius = body.vertex_radius;
        state.windEnabled = body.IsWindEnabled();
        return state;
    }

    [[nodiscard]] inline SoftBodyPhysicsState SanitizeSoftBodyPhysicsState(
        const SoftBodyPhysicsState& state) noexcept
    {
        SoftBodyPhysicsState result = state;
        result.detail = std::clamp(
            softbody_physics_detail::FiniteOr(result.detail, 1.0f),
            0.001f,
            1.0f);
        result.mass = std::clamp(
            softbody_physics_detail::FiniteOr(result.mass, 1.0f),
            0.0f,
            100.0f);
        result.friction = std::clamp(
            softbody_physics_detail::FiniteOr(result.friction, 0.5f),
            0.0f,
            1.0f);
        result.restitution = std::clamp(
            softbody_physics_detail::FiniteOr(result.restitution, 0.0f),
            0.0f,
            1.0f);
        result.pressure = std::clamp(
            softbody_physics_detail::FiniteOr(result.pressure, 0.0f),
            0.0f,
            100000.0f);
        result.vertexRadius = std::clamp(
            softbody_physics_detail::FiniteOr(result.vertexRadius, 0.2f),
            0.0f,
            1.0f);
        return result;
    }

    [[nodiscard]] inline bool HasSoftBodyPhysicsStateChange(
        const SoftBodyPhysicsState& before,
        const SoftBodyPhysicsState& after) noexcept
    {
        const auto left = SanitizeSoftBodyPhysicsState(before);
        const auto right = SanitizeSoftBodyPhysicsState(after);
        return !softbody_physics_detail::NearlyEqual(left.detail, right.detail) ||
            !softbody_physics_detail::NearlyEqual(left.mass, right.mass) ||
            !softbody_physics_detail::NearlyEqual(left.friction, right.friction) ||
            !softbody_physics_detail::NearlyEqual(
                left.restitution,
                right.restitution) ||
            !softbody_physics_detail::NearlyEqual(left.pressure, right.pressure) ||
            !softbody_physics_detail::NearlyEqual(
                left.vertexRadius,
                right.vertexRadius) ||
            left.windEnabled != right.windEnabled;
    }

    inline void ApplySoftBodyPhysics(
        wi::scene::SoftBodyPhysicsComponent& body,
        const SoftBodyPhysicsState& state) noexcept
    {
        const auto before = CaptureSoftBodyPhysics(body);
        const auto safe = SanitizeSoftBodyPhysicsState(state);

        if (!softbody_physics_detail::NearlyEqual(before.detail, safe.detail))
            body.SetDetail(safe.detail);
        else
            body.detail = safe.detail;

        if (!softbody_physics_detail::NearlyEqual(before.mass, safe.mass) ||
            !softbody_physics_detail::NearlyEqual(before.pressure, safe.pressure) ||
            !softbody_physics_detail::NearlyEqual(
                before.vertexRadius,
                safe.vertexRadius))
        {
            body.physicsobject.reset();
        }

        body.mass = safe.mass;
        body.friction = safe.friction;
        body.restitution = safe.restitution;
        body.pressure = safe.pressure;
        body.vertex_radius = safe.vertexRadius;
        body.SetWindEnabled(safe.windEnabled);
    }

    class CreateSoftBodyPhysicsCommand final : public ICommand
    {
    public:
        CreateSoftBodyPhysicsCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity selectedEntity,
            const SoftBodyPhysicsState& initial = {})
            : scene_(&scene)
            , selectedEntity_(selectedEntity)
            , initial_(SanitizeSoftBodyPhysicsState(initial))
        {
        }

        bool Execute() override
        {
            if (scene_ == nullptr ||
                GetSoftBodyCreationBlock(*scene_, selectedEntity_) != SoftBodyCreationBlock::None)
            {
                return false;
            }
            meshEntity_ = ResolveSoftBodyMeshEntity(*scene_, selectedEntity_);
            auto& body = scene_->softbodies.Create(meshEntity_);
            ApplySoftBodyPhysics(body, initial_);
            return true;
        }

        void Undo() override
        {
            if (scene_ != nullptr && meshEntity_ != wi::ecs::INVALID_ENTITY)
                scene_->softbodies.Remove(meshEntity_);
        }

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity selectedEntity_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity meshEntity_ = wi::ecs::INVALID_ENTITY;
        SoftBodyPhysicsState initial_;
    };

    class SetSoftBodyPhysicsCommand final : public ICommand
    {
    public:
        SetSoftBodyPhysicsCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity selectedEntity,
            const SoftBodyPhysicsState& state)
            : scene_(&scene)
            , selectedEntity_(selectedEntity)
            , after_(SanitizeSoftBodyPhysicsState(state))
        {
            meshEntity_ = ResolveSoftBodyMeshEntity(scene, selectedEntity);
            if (const auto* body = scene.softbodies.GetComponent(meshEntity_))
                before_ = CaptureSoftBodyPhysics(*body);
        }

        SetSoftBodyPhysicsCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity selectedEntity,
            const SoftBodyPhysicsState& before,
            const SoftBodyPhysicsState& after)
            : scene_(&scene)
            , selectedEntity_(selectedEntity)
            , meshEntity_(ResolveSoftBodyMeshEntity(scene, selectedEntity))
            , before_(SanitizeSoftBodyPhysicsState(before))
            , after_(SanitizeSoftBodyPhysicsState(after))
        {
        }

        bool Execute() override
        {
            return HasSoftBodyPhysicsStateChange(before_, after_) && Apply(after_);
        }

        void Undo() override
        {
            Apply(before_);
        }

    private:
        bool Apply(const SoftBodyPhysicsState& state) noexcept
        {
            if (scene_ == nullptr || meshEntity_ == wi::ecs::INVALID_ENTITY)
                return false;
            auto* body = scene_->softbodies.GetComponent(meshEntity_);
            if (body == nullptr)
                return false;
            ApplySoftBodyPhysics(*body, state);
            return true;
        }

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity selectedEntity_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity meshEntity_ = wi::ecs::INVALID_ENTITY;
        SoftBodyPhysicsState before_;
        SoftBodyPhysicsState after_;
    };

    class RemoveSoftBodyPhysicsCommand final : public ICommand
    {
    public:
        RemoveSoftBodyPhysicsCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity selectedEntity)
            : scene_(&scene)
            , selectedEntity_(selectedEntity)
        {
        }

        bool Execute() override
        {
            if (scene_ == nullptr)
                return false;
            meshEntity_ = ResolveSoftBodyMeshEntity(*scene_, selectedEntity_);
            const auto* existing = scene_->softbodies.GetComponent(meshEntity_);
            if (existing == nullptr)
                return false;

            removedNative_ = *existing;
            removedNative_.physicsobject.reset();
            hasRemoved_ = true;
            scene_->softbodies.Remove(meshEntity_);

            // Soft-body simulation uses temporary bone streams on an unrigged
            // mesh. Removing the component must clear those streams just like
            // the editor does, otherwise stale soft-body skinning data remains
            // on a now-static mesh. Capture it so Renegade Undo is lossless.
            meshCleanupApplied_ = false;
            auto* mesh = scene_->meshes.GetComponent(meshEntity_);
            if (mesh != nullptr && mesh->armatureID == wi::ecs::INVALID_ENTITY)
            {
                removedBoneIndices_ = mesh->vertex_boneindices;
                removedBoneWeights_ = mesh->vertex_boneweights;
                removedBoneIndices2_ = mesh->vertex_boneindices2;
                removedBoneWeights2_ = mesh->vertex_boneweights2;

                mesh->vertex_boneindices.clear();
                mesh->vertex_boneweights.clear();
                mesh->vertex_boneindices2.clear();
                mesh->vertex_boneweights2.clear();
                softbody_physics_detail::RefreshMeshRenderData(*mesh);
                meshCleanupApplied_ = true;
            }
            return true;
        }

        void Undo() override
        {
            if (scene_ == nullptr ||
                !hasRemoved_ ||
                meshEntity_ == wi::ecs::INVALID_ENTITY ||
                scene_->softbodies.Contains(meshEntity_))
            {
                return;
            }

            if (meshCleanupApplied_)
            {
                if (auto* mesh = scene_->meshes.GetComponent(meshEntity_))
                {
                    mesh->vertex_boneindices = removedBoneIndices_;
                    mesh->vertex_boneweights = removedBoneWeights_;
                    mesh->vertex_boneindices2 = removedBoneIndices2_;
                    mesh->vertex_boneweights2 = removedBoneWeights2_;
                    softbody_physics_detail::RefreshMeshRenderData(*mesh);
                }
            }

            auto& body = scene_->softbodies.Create(meshEntity_);
            body = removedNative_;
            body.physicsobject.reset();
        }

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity selectedEntity_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity meshEntity_ = wi::ecs::INVALID_ENTITY;
        wi::scene::SoftBodyPhysicsComponent removedNative_;
        wi::vector<XMUINT4> removedBoneIndices_;
        wi::vector<XMFLOAT4> removedBoneWeights_;
        wi::vector<XMUINT4> removedBoneIndices2_;
        wi::vector<XMFLOAT4> removedBoneWeights2_;
        bool hasRemoved_ = false;
        bool meshCleanupApplied_ = false;
    };

    // Pin indices are zero-based graphics vertices. Wicked maps these weights
    // into the simulation mesh when the native body is rebuilt.
    [[nodiscard]] inline bool GetSoftBodyPinnedVertices(
        const wi::scene::Scene& scene, wi::ecs::Entity selectedEntity,
        wi::vector<uint32_t>& pins)
    {
        pins.clear();
        const auto entity = ResolveSoftBodyMeshEntity(scene, selectedEntity);
        const auto* mesh = scene.meshes.GetComponent(entity);
        const auto* body = scene.softbodies.GetComponent(entity);
        if (!mesh || !body)
            return false;
        const auto count = std::min(mesh->vertex_positions.size(), body->weights.size());
        for (size_t index = 0; index < count; ++index)
            if (body->weights[index] == 0.0f)
                pins.push_back(static_cast<uint32_t>(index));
        return true;
    }

    class SetSoftBodyPinsCommand final : public ICommand
    {
    public:
        SetSoftBodyPinsCommand(wi::scene::Scene& scene, wi::ecs::Entity selectedEntity,
            wi::vector<uint32_t> pins)
            : scene_(scene), entity_(ResolveSoftBodyMeshEntity(scene, selectedEntity)),
              pins_(std::move(pins))
        {
        }

        bool Execute() override
        {
            auto* body = scene_.softbodies.GetComponent(entity_);
            const auto* mesh = scene_.meshes.GetComponent(entity_);
            if (!body || !mesh)
                return false;
            const auto count = mesh->vertex_positions.size();
            for (auto index : pins_)
                if (index >= count)
                    return false;
            auto weights = body->weights;
            weights.resize(count, 1.0f);
            for (auto& weight : weights)
                if (weight == 0.0f)
                    weight = 1.0f;
            for (auto index : pins_)
                weights[index] = 0.0f;

            wi::vector<uint32_t> currentPins;
            if (!GetSoftBodyPinnedVertices(scene_, entity_, currentPins))
                return false;
            std::sort(pins_.begin(), pins_.end());
            pins_.erase(std::unique(pins_.begin(), pins_.end()), pins_.end());
            if (currentPins == pins_)
                return false;

            before_ = std::move(body->weights);
            vertexCount_ = count;
            body->weights = std::move(weights);
            body->Reset();
            return true;
        }

        void Undo() override
        {
            auto* body = scene_.softbodies.GetComponent(entity_);
            const auto* mesh = scene_.meshes.GetComponent(entity_);
            if (!body || !mesh || mesh->vertex_positions.size() != vertexCount_)
                return;
            body->weights = before_;
            body->Reset();
        }

    private:
        wi::scene::Scene& scene_;
        wi::ecs::Entity entity_;
        wi::vector<uint32_t> pins_;
        wi::vector<float> before_;
        size_t vertexCount_ = 0;
    };

    [[nodiscard]] inline bool ResetSoftBodyPhysics(
        wi::scene::Scene& scene,
        const wi::ecs::Entity selectedEntity) noexcept
    {
        const auto meshEntity = ResolveSoftBodyMeshEntity(scene, selectedEntity);
        auto* body = scene.softbodies.GetComponent(meshEntity);
        if (body == nullptr)
            return false;
        body->Reset();
        return true;
    }

    [[nodiscard]] inline bool HasLiveSoftBodyPhysics(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity selectedEntity) noexcept
    {
        const auto meshEntity = ResolveSoftBodyMeshEntity(scene, selectedEntity);
        const auto* body = scene.softbodies.GetComponent(meshEntity);
        return body != nullptr && body->physicsobject != nullptr;
    }

    [[nodiscard]] inline bool SetSoftBodyActive(
        wi::scene::Scene& scene,
        const wi::ecs::Entity selectedEntity,
        const bool active) noexcept
    {
        const auto meshEntity = ResolveSoftBodyMeshEntity(scene, selectedEntity);
        auto* body = scene.softbodies.GetComponent(meshEntity);
        if (body == nullptr || body->physicsobject == nullptr)
            return false;
        wi::physics::SetActivationState(
            *body,
            active ? wi::physics::ActivationState::Active
                   : wi::physics::ActivationState::Inactive);
        return true;
    }

    [[nodiscard]] inline bool GetSoftBodyNodePosition(
        wi::scene::Scene& scene,
        const wi::ecs::Entity selectedEntity,
        const uint32_t physicsIndex,
        XMFLOAT3& position) noexcept
    {
        const auto meshEntity = ResolveSoftBodyMeshEntity(scene, selectedEntity);
        auto* body = scene.softbodies.GetComponent(meshEntity);
        if (body == nullptr ||
            body->physicsobject == nullptr ||
            physicsIndex >= body->physicsIndices.size())
        {
            return false;
        }
        position = wi::physics::GetSoftBodyNodePosition(*body, physicsIndex);
        return true;
    }
}
