#pragma once

#include <WickedEngine.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace renegade::bridge
{
    // Runtime/private-preview state only. Never add generated mask clips to an
    // authored asset. Mutate at the caller's normal scene thread-safe point.
    struct PlayerViewBonePartition
    {
        std::vector<wi::ecs::Entity> base;
        std::vector<wi::ecs::Entity> primary;
        std::vector<wi::ecs::Entity> offHand;
    };

    [[nodiscard]] inline bool CollectPlayerViewBonePartition(
        const wi::scene::Scene& scene, wi::ecs::Entity armatureEntity,
        wi::ecs::Entity primaryRoot, wi::ecs::Entity offHandRoot,
        PlayerViewBonePartition& result, std::string& error)
    {
        error.clear();
        const auto* armature = scene.armatures.GetComponent(armatureEntity);
        if (!armature || primaryRoot == offHandRoot ||
            !scene.transforms.Contains(primaryRoot) || !scene.transforms.Contains(offHandRoot))
        {
            error = "Hand masks require two distinct skeletal subtree roots.";
            return false;
        }
        const auto& bones = armature->boneCollection;
        const auto contains = [&](wi::ecs::Entity entity) {
            return std::find(bones.begin(), bones.end(), entity) != bones.end();
        };
        if (!contains(primaryRoot) || !contains(offHandRoot))
        {
            error = "Both hand roots must belong to the same armature.";
            return false;
        }
        // Walk bounded parent chains rather than accepting a malformed cycle.
        const auto below = [&](wi::ecs::Entity bone, wi::ecs::Entity root, bool& valid) {
            for (size_t depth = 0; depth <= scene.hierarchy.GetCount(); ++depth)
            {
                if (bone == root) return true;
                const auto* parent = scene.hierarchy.GetComponent(bone);
                if (!parent || parent->parentID == wi::ecs::INVALID_ENTITY) return false;
                bone = parent->parentID;
            }
            valid = false;
            return false;
        };
        PlayerViewBonePartition prepared;
        for (const auto bone : bones)
        {
            bool valid = scene.transforms.Contains(bone);
            const bool primary = below(bone, primaryRoot, valid);
            const bool offHand = below(bone, offHandRoot, valid);
            if (!valid || (primary && offHand))
            {
                error = "Hand bone subtrees overlap or contain invalid hierarchy.";
                return false;
            }
            (primary ? prepared.primary : offHand ? prepared.offHand : prepared.base).push_back(bone);
        }
        result = std::move(prepared);
        return true;
    }

    [[nodiscard]] inline bool CreatePlayerViewMaskedAnimationClip(
        wi::scene::Scene& scene, wi::ecs::Entity sourceEntity,
        const std::vector<wi::ecs::Entity>& targets,
        wi::ecs::Entity& result, std::string& error)
    {
        error.clear();
        const auto* source = scene.animations.GetComponent(sourceEntity);
        if (!source || targets.empty() || !source->retargets.empty() ||
            !std::isfinite(source->start) || !std::isfinite(source->end) ||
            source->end < source->start || !std::isfinite(source->end - source->start) ||
            !std::isfinite(source->speed) || source->speed <= 0)
        {
            error = "A hand mask requires a native local clip and nonempty bone targets.";
            return false;
        }
        for (const auto target : targets)
            if (!scene.transforms.Contains(target))
            {
                error = "Animation mask contains a missing transform.";
                return false;
            }
        // Build a separate native component before creating entities; manager
        // growth must not invalidate the source pointer. Keyframe data is shared.
        wi::scene::AnimationComponent prepared;
        prepared.start = source->start;
        prepared.end = source->end;
        prepared.timer = source->start;
        prepared.speed = source->speed;
        prepared.amount = 0;
        prepared.Pause();
        prepared.RootMotionOff();
        prepared.SetRootMotionBone(wi::ecs::INVALID_ENTITY);
        prepared.SetLooped(source->IsLooped());
        prepared.last_update_time = -std::numeric_limits<float>::max();
        using Path = wi::scene::AnimationComponent::AnimationChannel::Path;
        for (const auto& channel : source->channels)
        {
            if (std::find(targets.begin(), targets.end(), channel.target) == targets.end()) continue;
            if ((channel.path != Path::TRANSLATION && channel.path != Path::ROTATION &&
                    channel.path != Path::SCALE) || channel.retargetIndex >= 0 ||
                channel.samplerIndex < 0 || size_t(channel.samplerIndex) >= source->samplers.size())
            {
                error = "Hand masks support only local native transform channels.";
                return false;
            }
            const auto& sampler = source->samplers[channel.samplerIndex];
            if (sampler.scene || !scene.animation_datas.Contains(sampler.data))
            {
                error = "Hand mask keyframes must be retained in the same scene.";
                return false;
            }
            auto copied = channel;
            copied.samplerIndex = int(prepared.samplers.size());
            prepared.samplers.push_back(sampler);
            prepared.channels.push_back(copied);
        }
        if (prepared.channels.empty())
        {
            error = "Source clip has no channels for the requested hand mask.";
            return false;
        }
        const auto entity = wi::ecs::CreateEntity();
        scene.animations.Create(entity) = std::move(prepared);
        result = entity;
        return true;
    }
}
