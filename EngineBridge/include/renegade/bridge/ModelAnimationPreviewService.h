#pragma once
#include "renegade/bridge/AnimationService.h"
#include <limits>

namespace renegade::bridge
{
    inline constexpr const char* ModelImportStartsPausedMetadataKey =
        "renegade.model_import.starts_paused";
    inline bool ModelImportStartsPaused(const wi::scene::Scene& scene,
        wi::ecs::Entity root = wi::ecs::INVALID_ENTITY)
    {
        for (size_t i = 0; i < scene.metadatas.GetCount(); ++i)
        {
            const auto& metadata = scene.metadatas[i];
            if (metadata.bool_values.has(ModelImportStartsPausedMetadataKey) &&
                metadata.bool_values.get(ModelImportStartsPausedMetadataKey) &&
                (root == wi::ecs::INVALID_ENTITY ||
                 animation_detail::IsAncestorOrSelf(scene, root, scene.metadatas.GetEntity(i))))
                return true;
        }
        return false;
    }

    inline void PauseImportedModelAnimations(wi::scene::Scene& scene)
    {
        for (size_t i = 0; i < scene.animations.GetCount(); ++i)
        {
            auto& clip = scene.animations[i];
            clip.Pause();
            clip.timer = clip.start;
            clip.last_update_time = clip.timer;
            clip.RootMotionOff();
        }
    }

    // Scene must have completed Update(0). Native armature AABBs surround
    // joints, so fit the actual deformed vertices instead of those helper bounds.
    inline bool ComputeVisibleModelBounds(const wi::scene::Scene& scene,
        wi::primitive::AABB& bounds)
    {
        bounds = {};
        if (scene.matrix_objects.size() < scene.objects.GetCount()) return false;
        for (size_t i = 0; i < scene.objects.GetCount(); ++i)
        {
            const auto* mesh = scene.meshes.GetComponent(scene.objects[i].meshID);
            if (!mesh) continue;
            const auto* armature = scene.armatures.GetComponent(mesh->armatureID);
            const auto world = XMLoadFloat4x4(&scene.matrix_objects[i]);
            for (size_t vertex = 0; vertex < mesh->vertex_positions.size(); ++vertex)
            {
                const auto position = mesh->IsSkinned() && armature ?
                    wi::scene::SkinVertex(*mesh, *armature, (uint32_t)vertex) :
                    XMLoadFloat3(&mesh->vertex_positions[vertex]);
                const auto point = XMVector3TransformCoord(position, world);
                if (XMVector3IsNaN(point) || XMVector3IsInfinite(point)) return false;
                bounds.AddPoint(point);
            }
        }
        return bounds.IsValid();
    }

    // Transient controller for an isolated scene copy. It never edits the
    // candidate, authored scene, creator recipe or gameplay action assignments.
    class ModelAnimationPreviewService
    {
    public:
        void Prepare(wi::scene::Scene& scene)
        {
            scene_ = &scene;
            selected_ = wi::ecs::INVALID_ENTITY;
            PauseImportedModelAnimations(scene);
            transforms_.clear();
            for (size_t i = 0; i < scene.transforms.GetCount(); ++i)
                transforms_.push_back({scene.transforms.GetEntity(i), scene.transforms[i]});
        }
        std::vector<AnimationClipInfo> Clips() const
        {
            return scene_ ? CollectAnimationClips(*scene_, wi::ecs::INVALID_ENTITY, false)
                          : std::vector<AnimationClipInfo>{};
        }
        bool Select(int index)
        {
            if (!scene_ || index < -1 || index >= int(scene_->animations.GetCount())) return false;
            PauseImportedModelAnimations(*scene_);
            for (size_t i = 0; i < scene_->animations.GetCount(); ++i)
                scene_->animations[i].amount = 0;
            for (const auto& saved : transforms_)
                if (auto* transform = scene_->transforms.GetComponent(saved.first))
                    *transform = saved.second;
            selected_ = index < 0 ? wi::ecs::INVALID_ENTITY : scene_->animations.GetEntity(index);
            if (auto* clip = Selected())
            {
                clip->amount = 1;
                clip->last_update_time = -std::numeric_limits<float>::max();
            }
            return true;
        }
        bool PlayPause()
        {
            auto* clip = Selected();
            if (!clip) return false;
            if (clip->IsPlaying()) clip->Pause(); else clip->Play();
            return true;
        }
        bool Scrub(float time)
        {
            auto* clip = Selected();
            if (!clip || !std::isfinite(time)) return false;
            clip->Pause();
            clip->timer = std::clamp(time, clip->start, clip->end);
            // Wicked skips paused clips whose last_update_time equals timer.
            clip->last_update_time = -std::numeric_limits<float>::max();
            return true;
        }
        bool Speed(float multiplier)
        {
            auto* clip = Selected();
            if (!clip || !std::isfinite(multiplier)) return false;
            clip->speed = std::clamp(multiplier, 0.1f, 4.0f);
            return true;
        }
        bool IsPlaying() const { const auto* clip = Selected(); return clip && clip->IsPlaying(); }
        float Time() const { const auto* clip = Selected(); return clip ? clip->timer : 0; }
        bool HasSelection() const { return Selected() != nullptr; }
    private:
        wi::scene::AnimationComponent* Selected() const
        {
            return scene_ ? scene_->animations.GetComponent(selected_) : nullptr;
        }
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity selected_ = wi::ecs::INVALID_ENTITY;
        std::vector<std::pair<wi::ecs::Entity, wi::scene::TransformComponent>> transforms_;
    };
}
