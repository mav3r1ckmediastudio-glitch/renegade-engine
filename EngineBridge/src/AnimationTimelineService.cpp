#include "renegade/bridge/AnimationTimelineService.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_set>
#include <utility>

namespace
{
    using namespace renegade::bridge;

    constexpr float KeyTimeEpsilon = 0.000001f;

    bool EntityExists(const wi::scene::Scene& scene, const wi::ecs::Entity entity)
    {
        if (entity == wi::ecs::INVALID_ENTITY)
            return false;
        wi::unordered_set<wi::ecs::Entity> entities;
        scene.FindAllEntities(entities);
        return entities.count(entity) != 0;
    }

    bool ContainsEntity(
        const std::vector<TimelineDataSnapshot>& snapshots,
        const wi::ecs::Entity entity) noexcept
    {
        return std::any_of(snapshots.begin(), snapshots.end(),
            [entity](const TimelineDataSnapshot& snapshot)
            {
                return snapshot.entity == entity;
            });
    }

    bool ApplySnapshot(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity,
        const TimelineGraphSnapshot& snapshot,
        const std::vector<wi::ecs::Entity>* createdDataEntities = nullptr,
        const bool removeCreated = false) noexcept
    {
        auto* animation = scene.animations.GetComponent(animationEntity);
        if (animation == nullptr)
            return false;

        animation->channels.clear();
        animation->channels.insert(
            animation->channels.end(), snapshot.channels.begin(), snapshot.channels.end());
        animation->samplers.clear();
        animation->samplers.insert(
            animation->samplers.end(), snapshot.samplers.begin(), snapshot.samplers.end());

        for (const auto& dataSnapshot : snapshot.data)
        {
            auto* data = scene.animation_datas.GetComponent(dataSnapshot.entity);
            if (data == nullptr)
                data = &scene.animation_datas.Create(dataSnapshot.entity);
            *data = dataSnapshot.data;

            if (dataSnapshot.hasHierarchy)
                scene.hierarchy.Create(dataSnapshot.entity) = dataSnapshot.hierarchy;
            else
                scene.hierarchy.Remove(dataSnapshot.entity);
        }

        if (removeCreated && createdDataEntities != nullptr)
        {
            for (const auto entity : *createdDataEntities)
            {
                if (!ContainsEntity(snapshot.data, entity) && EntityExists(scene, entity))
                    scene.Entity_Remove(entity, true);
            }
        }
        for (auto& channel : animation->channels)
            channel.next_event = 0;
        return true;
    }

    const wi::scene::MeshComponent* ResolveMorphMesh(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity target) noexcept
    {
        if (const auto* mesh = scene.meshes.GetComponent(target))
            return mesh;
        if (const auto* object = scene.objects.GetComponent(target))
            return scene.meshes.GetComponent(object->meshID);
        return nullptr;
    }

    bool CapturePathValue(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity target,
        const AnimationPath path,
        std::vector<float>& value,
        std::string* reason)
    {
        value.clear();
        auto fail = [reason](const char* message)
        {
            if (reason != nullptr)
                *reason = message;
            return false;
        };

        switch (path)
        {
        case AnimationPath::TRANSLATION:
        {
            const auto* transform = scene.transforms.GetComponent(target);
            if (transform == nullptr)
                return fail("target has no TransformComponent");
            value = {
                transform->translation_local.x,
                transform->translation_local.y,
                transform->translation_local.z,
            };
            return true;
        }
        case AnimationPath::ROTATION:
        {
            const auto* transform = scene.transforms.GetComponent(target);
            if (transform == nullptr)
                return fail("target has no TransformComponent");
            value = {
                transform->rotation_local.x,
                transform->rotation_local.y,
                transform->rotation_local.z,
                transform->rotation_local.w,
            };
            return true;
        }
        case AnimationPath::SCALE:
        {
            const auto* transform = scene.transforms.GetComponent(target);
            if (transform == nullptr)
                return fail("target has no TransformComponent");
            value = {
                transform->scale_local.x,
                transform->scale_local.y,
                transform->scale_local.z,
            };
            return true;
        }
        case AnimationPath::WEIGHTS:
        {
            const auto* mesh = ResolveMorphMesh(scene, target);
            if (mesh == nullptr || mesh->morph_targets.empty())
                return fail("target has no morph targets");
            value.reserve(mesh->morph_targets.size());
            for (const auto& morph : mesh->morph_targets)
                value.push_back(morph.weight);
            return true;
        }
        case AnimationPath::LIGHT_COLOR:
        {
            const auto* light = scene.lights.GetComponent(target);
            if (light == nullptr)
                return fail("target has no LightComponent");
            value = {light->color.x, light->color.y, light->color.z};
            return true;
        }
        case AnimationPath::LIGHT_INTENSITY:
        case AnimationPath::LIGHT_RANGE:
        case AnimationPath::LIGHT_INNERCONE:
        case AnimationPath::LIGHT_OUTERCONE:
        {
            const auto* light = scene.lights.GetComponent(target);
            if (light == nullptr)
                return fail("target has no LightComponent");
            if (path == AnimationPath::LIGHT_INTENSITY)
                value.push_back(light->intensity);
            else if (path == AnimationPath::LIGHT_RANGE)
                value.push_back(light->range);
            else if (path == AnimationPath::LIGHT_INNERCONE)
                value.push_back(light->innerConeAngle);
            else
                value.push_back(light->outerConeAngle);
            return true;
        }
        case AnimationPath::SOUND_PLAY:
        case AnimationPath::SOUND_STOP:
            return scene.sounds.GetComponent(target) != nullptr
                ? true
                : fail("target has no SoundComponent");
        case AnimationPath::SOUND_VOLUME:
        {
            const auto* sound = scene.sounds.GetComponent(target);
            if (sound == nullptr)
                return fail("target has no SoundComponent");
            value.push_back(sound->volume);
            return true;
        }
        case AnimationPath::EMITTER_EMITCOUNT:
        {
            const auto* emitter = scene.emitters.GetComponent(target);
            if (emitter == nullptr)
                return fail("target has no particle emitter");
            value.push_back(emitter->count);
            return true;
        }
        case AnimationPath::CAMERA_FOV:
        case AnimationPath::CAMERA_FOCAL_LENGTH:
        case AnimationPath::CAMERA_APERTURE_SIZE:
        case AnimationPath::CAMERA_APERTURE_SHAPE:
        {
            const auto* camera = scene.cameras.GetComponent(target);
            if (camera == nullptr)
                return fail("target has no CameraComponent");
            if (path == AnimationPath::CAMERA_FOV)
                value.push_back(camera->fov);
            else if (path == AnimationPath::CAMERA_FOCAL_LENGTH)
                value.push_back(camera->focal_length);
            else if (path == AnimationPath::CAMERA_APERTURE_SIZE)
                value.push_back(camera->aperture_size);
            else
                value = {camera->aperture_shape.x, camera->aperture_shape.y};
            return true;
        }
        case AnimationPath::SCRIPT_PLAY:
        case AnimationPath::SCRIPT_STOP:
            return fail(
                "Renegade creator scripts use the governed .rscripts runtime, not Wicked ScriptComponent events");
        case AnimationPath::MATERIAL_COLOR:
        case AnimationPath::MATERIAL_EMISSIVE:
        case AnimationPath::MATERIAL_ROUGHNESS:
        case AnimationPath::MATERIAL_METALNESS:
        case AnimationPath::MATERIAL_REFLECTANCE:
        case AnimationPath::MATERIAL_TEXMULADD:
        {
            const auto* material = scene.materials.GetComponent(target);
            if (material == nullptr)
                return fail("target has no MaterialComponent");
            if (path == AnimationPath::MATERIAL_COLOR)
            {
                value = {
                    material->baseColor.x, material->baseColor.y,
                    material->baseColor.z, material->baseColor.w,
                };
            }
            else if (path == AnimationPath::MATERIAL_EMISSIVE)
            {
                value = {
                    material->emissiveColor.x, material->emissiveColor.y,
                    material->emissiveColor.z, material->emissiveColor.w,
                };
            }
            else if (path == AnimationPath::MATERIAL_ROUGHNESS)
                value.push_back(material->roughness);
            else if (path == AnimationPath::MATERIAL_METALNESS)
                value.push_back(material->metalness);
            else if (path == AnimationPath::MATERIAL_REFLECTANCE)
                value.push_back(material->reflectance);
            else
            {
                value = {
                    material->texMulAdd.x, material->texMulAdd.y,
                    material->texMulAdd.z, material->texMulAdd.w,
                };
            }
            return true;
        }
        default:
            return fail("unsupported native animation path");
        }
    }

    int FindChannelIndex(
        const wi::scene::AnimationComponent& animation,
        const wi::ecs::Entity target,
        const AnimationPath path) noexcept
    {
        for (std::size_t i = 0; i < animation.channels.size(); ++i)
        {
            const auto& channel = animation.channels[i];
            if (channel.target == target && channel.path == path)
                return static_cast<int>(i);
        }
        return -1;
    }

    bool ResolveData(
        wi::scene::Scene& scene,
        wi::scene::AnimationComponent& animation,
        const std::size_t channelIndex,
        wi::scene::AnimationDataComponent*& data,
        std::string& error) noexcept
    {
        if (channelIndex >= animation.channels.size())
        {
            error = "timeline channel index is out of range";
            return false;
        }
        const auto& channel = animation.channels[channelIndex];
        if (channel.samplerIndex < 0 ||
            static_cast<std::size_t>(channel.samplerIndex) >= animation.samplers.size())
        {
            error = "timeline channel has an invalid native sampler";
            return false;
        }
        const auto& sampler = animation.samplers[static_cast<std::size_t>(channel.samplerIndex)];
        data = scene.animation_datas.GetComponent(sampler.data);
        if (data == nullptr)
        {
            error = "timeline sampler has no AnimationDataComponent";
            return false;
        }
        return true;
    }

    bool ReorderKeys(
        wi::scene::AnimationDataComponent& data,
        std::string& error)
    {
        const std::size_t keyCount = data.keyframe_times.size();
        if (keyCount < 2)
            return true;
        if (!data.keyframe_data.empty() && data.keyframe_data.size() % keyCount != 0)
        {
            error = "animation key data is malformed and cannot be reordered safely";
            return false;
        }

        const std::size_t stride = data.keyframe_data.empty()
            ? 0
            : data.keyframe_data.size() / keyCount;
        std::vector<std::size_t> order(keyCount);
        std::iota(order.begin(), order.end(), 0);
        std::stable_sort(order.begin(), order.end(),
            [&data](const std::size_t left, const std::size_t right)
            {
                return data.keyframe_times[left] < data.keyframe_times[right];
            });

        wi::vector<float> times;
        wi::vector<float> values;
        times.reserve(keyCount);
        values.reserve(data.keyframe_data.size());
        for (const auto oldIndex : order)
        {
            times.push_back(data.keyframe_times[oldIndex]);
            for (std::size_t component = 0; component < stride; ++component)
                values.push_back(data.keyframe_data[oldIndex * stride + component]);
        }
        data.keyframe_times = std::move(times);
        data.keyframe_data = std::move(values);
        return true;
    }

    bool InsertOrReplaceKey(
        wi::scene::AnimationDataComponent& data,
        const std::vector<float>& value,
        const float time,
        std::string& error,
        bool& changed)
    {
        changed = false;
        if (!std::isfinite(time))
        {
            error = "timeline key time must be finite";
            return false;
        }

        const auto timesBefore = data.keyframe_times;
        if (!ReorderKeys(data, error))
            return false;
        changed = timesBefore != data.keyframe_times;

        const std::size_t keyCount = data.keyframe_times.size();
        const std::size_t stride = value.size();
        if (keyCount == 0)
        {
            if (!data.keyframe_data.empty())
            {
                error = "animation data contains payload values without key times";
                return false;
            }
        }
        else if (data.keyframe_data.size() != keyCount * stride)
        {
            error = "animation key payload width does not match the native channel path";
            return false;
        }

        auto lower = std::lower_bound(data.keyframe_times.begin(), data.keyframe_times.end(), time);
        std::size_t index = static_cast<std::size_t>(
            std::distance(data.keyframe_times.begin(), lower));

        auto replaceAt = [&](const std::size_t keyIndex)
        {
            if (stride == 0)
                return;
            bool payloadChanged = false;
            for (std::size_t component = 0; component < stride; ++component)
            {
                if (std::abs(data.keyframe_data[keyIndex * stride + component] - value[component]) >
                    KeyTimeEpsilon)
                {
                    payloadChanged = true;
                    break;
                }
            }
            if (!payloadChanged)
                return;
            for (std::size_t component = 0; component < stride; ++component)
                data.keyframe_data[keyIndex * stride + component] = value[component];
            changed = true;
        };

        if (lower != data.keyframe_times.end() && std::abs(*lower - time) <= KeyTimeEpsilon)
        {
            replaceAt(index);
            return true;
        }
        if (index > 0 && std::abs(data.keyframe_times[index - 1] - time) <= KeyTimeEpsilon)
        {
            replaceAt(index - 1);
            return true;
        }

        data.keyframe_times.insert(lower, time);
        if (stride > 0)
        {
            data.keyframe_data.insert(
                data.keyframe_data.begin() + static_cast<std::ptrdiff_t>(index * stride),
                value.begin(), value.end());
        }
        changed = true;
        return true;
    }
}

namespace renegade::bridge
{
    bool IsTimelineEventPath(const AnimationPath path) noexcept
    {
        return path == AnimationPath::SOUND_PLAY ||
            path == AnimationPath::SOUND_STOP ||
            path == AnimationPath::SCRIPT_PLAY ||
            path == AnimationPath::SCRIPT_STOP;
    }

    const char* TimelineRecordPresetLabel(const TimelineRecordPreset preset) noexcept
    {
        switch (preset)
        {
        case TimelineRecordPreset::Transform: return "TRANSFORM";
        case TimelineRecordPreset::Translation: return "POSITION";
        case TimelineRecordPreset::Rotation: return "ROTATION";
        case TimelineRecordPreset::Scale: return "SCALE";
        case TimelineRecordPreset::MorphWeights: return "MORPH WEIGHTS";
        case TimelineRecordPreset::LightColor: return "LIGHT // COLOR";
        case TimelineRecordPreset::LightIntensity: return "LIGHT // INTENSITY";
        case TimelineRecordPreset::LightRange: return "LIGHT // RANGE";
        case TimelineRecordPreset::LightInnerCone: return "LIGHT // INNER CONE";
        case TimelineRecordPreset::LightOuterCone: return "LIGHT // OUTER CONE";
        case TimelineRecordPreset::SoundPlay: return "SOUND // PLAY";
        case TimelineRecordPreset::SoundStop: return "SOUND // STOP";
        case TimelineRecordPreset::SoundVolume: return "SOUND // VOLUME";
        case TimelineRecordPreset::EmitterEmitCount: return "EMITTER // EMIT COUNT";
        case TimelineRecordPreset::CameraFov: return "CAMERA // FOV";
        case TimelineRecordPreset::CameraFocalLength: return "CAMERA // FOCAL LENGTH";
        case TimelineRecordPreset::CameraApertureSize: return "CAMERA // APERTURE SIZE";
        case TimelineRecordPreset::CameraApertureShape: return "CAMERA // APERTURE SHAPE";
        case TimelineRecordPreset::ScriptPlay: return "SCRIPT // PLAY (RESERVED)";
        case TimelineRecordPreset::ScriptStop: return "SCRIPT // STOP (RESERVED)";
        case TimelineRecordPreset::MaterialColor: return "MATERIAL // COLOR";
        case TimelineRecordPreset::MaterialEmissive: return "MATERIAL // EMISSIVE";
        case TimelineRecordPreset::MaterialRoughness: return "MATERIAL // ROUGHNESS";
        case TimelineRecordPreset::MaterialMetalness: return "MATERIAL // METALNESS";
        case TimelineRecordPreset::MaterialReflectance: return "MATERIAL // REFLECTANCE";
        case TimelineRecordPreset::MaterialTexMulAdd: return "MATERIAL // TEX MUL/ADD";
        default: return "UNKNOWN";
        }
    }

    const char* TimelinePathLabel(const AnimationPath path) noexcept
    {
        switch (path)
        {
        case AnimationPath::TRANSLATION: return "POSITION";
        case AnimationPath::ROTATION: return "ROTATION";
        case AnimationPath::SCALE: return "SCALE";
        case AnimationPath::WEIGHTS: return "MORPH WEIGHTS";
        case AnimationPath::LIGHT_COLOR: return "LIGHT COLOR";
        case AnimationPath::LIGHT_INTENSITY: return "LIGHT INTENSITY";
        case AnimationPath::LIGHT_RANGE: return "LIGHT RANGE";
        case AnimationPath::LIGHT_INNERCONE: return "LIGHT INNER CONE";
        case AnimationPath::LIGHT_OUTERCONE: return "LIGHT OUTER CONE";
        case AnimationPath::SOUND_PLAY: return "SOUND PLAY";
        case AnimationPath::SOUND_STOP: return "SOUND STOP";
        case AnimationPath::SOUND_VOLUME: return "SOUND VOLUME";
        case AnimationPath::EMITTER_EMITCOUNT: return "EMITTER COUNT";
        case AnimationPath::CAMERA_FOV: return "CAMERA FOV";
        case AnimationPath::CAMERA_FOCAL_LENGTH: return "CAMERA FOCAL LENGTH";
        case AnimationPath::CAMERA_APERTURE_SIZE: return "CAMERA APERTURE SIZE";
        case AnimationPath::CAMERA_APERTURE_SHAPE: return "CAMERA APERTURE SHAPE";
        case AnimationPath::SCRIPT_PLAY: return "SCRIPT PLAY (LEGACY WICKED ONLY)";
        case AnimationPath::SCRIPT_STOP: return "SCRIPT STOP (LEGACY WICKED ONLY)";
        case AnimationPath::MATERIAL_COLOR: return "MATERIAL COLOR";
        case AnimationPath::MATERIAL_EMISSIVE: return "MATERIAL EMISSIVE";
        case AnimationPath::MATERIAL_ROUGHNESS: return "MATERIAL ROUGHNESS";
        case AnimationPath::MATERIAL_METALNESS: return "MATERIAL METALNESS";
        case AnimationPath::MATERIAL_REFLECTANCE: return "MATERIAL REFLECTANCE";
        case AnimationPath::MATERIAL_TEXMULADD: return "MATERIAL TEX MUL/ADD";
        default: return "UNKNOWN";
        }
    }

    std::vector<AnimationPath> TimelineRecordPresetPaths(const TimelineRecordPreset preset)
    {
        switch (preset)
        {
        case TimelineRecordPreset::Transform:
            return {AnimationPath::TRANSLATION, AnimationPath::ROTATION, AnimationPath::SCALE};
        case TimelineRecordPreset::Translation: return {AnimationPath::TRANSLATION};
        case TimelineRecordPreset::Rotation: return {AnimationPath::ROTATION};
        case TimelineRecordPreset::Scale: return {AnimationPath::SCALE};
        case TimelineRecordPreset::MorphWeights: return {AnimationPath::WEIGHTS};
        case TimelineRecordPreset::LightColor: return {AnimationPath::LIGHT_COLOR};
        case TimelineRecordPreset::LightIntensity: return {AnimationPath::LIGHT_INTENSITY};
        case TimelineRecordPreset::LightRange: return {AnimationPath::LIGHT_RANGE};
        case TimelineRecordPreset::LightInnerCone: return {AnimationPath::LIGHT_INNERCONE};
        case TimelineRecordPreset::LightOuterCone: return {AnimationPath::LIGHT_OUTERCONE};
        case TimelineRecordPreset::SoundPlay: return {AnimationPath::SOUND_PLAY};
        case TimelineRecordPreset::SoundStop: return {AnimationPath::SOUND_STOP};
        case TimelineRecordPreset::SoundVolume: return {AnimationPath::SOUND_VOLUME};
        case TimelineRecordPreset::EmitterEmitCount: return {AnimationPath::EMITTER_EMITCOUNT};
        case TimelineRecordPreset::CameraFov: return {AnimationPath::CAMERA_FOV};
        case TimelineRecordPreset::CameraFocalLength: return {AnimationPath::CAMERA_FOCAL_LENGTH};
        case TimelineRecordPreset::CameraApertureSize: return {AnimationPath::CAMERA_APERTURE_SIZE};
        case TimelineRecordPreset::CameraApertureShape: return {AnimationPath::CAMERA_APERTURE_SHAPE};
        case TimelineRecordPreset::ScriptPlay:
        case TimelineRecordPreset::ScriptStop:
            return {};
        case TimelineRecordPreset::MaterialColor: return {AnimationPath::MATERIAL_COLOR};
        case TimelineRecordPreset::MaterialEmissive: return {AnimationPath::MATERIAL_EMISSIVE};
        case TimelineRecordPreset::MaterialRoughness: return {AnimationPath::MATERIAL_ROUGHNESS};
        case TimelineRecordPreset::MaterialMetalness: return {AnimationPath::MATERIAL_METALNESS};
        case TimelineRecordPreset::MaterialReflectance: return {AnimationPath::MATERIAL_REFLECTANCE};
        case TimelineRecordPreset::MaterialTexMulAdd: return {AnimationPath::MATERIAL_TEXMULADD};
        default: return {};
        }
    }

    std::vector<TimelineChannelInfo> CollectTimelineChannels(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity)
    {
        std::vector<TimelineChannelInfo> result;
        const auto* animation = scene.animations.GetComponent(animationEntity);
        if (animation == nullptr)
            return result;

        result.reserve(animation->channels.size());
        for (std::size_t index = 0; index < animation->channels.size(); ++index)
        {
            const auto& channel = animation->channels[index];
            TimelineChannelInfo info;
            info.index = index;
            info.target = channel.target;
            info.path = channel.path;
            info.samplerIndex = channel.samplerIndex;
            if (channel.samplerIndex >= 0 &&
                static_cast<std::size_t>(channel.samplerIndex) < animation->samplers.size())
            {
                const auto& sampler = animation->samplers[static_cast<std::size_t>(channel.samplerIndex)];
                info.dataEntity = sampler.data;
                info.mode = sampler.mode;
                if (const auto* data = scene.animation_datas.GetComponent(sampler.data))
                    info.keyframeCount = data->keyframe_times.size();
            }
            result.push_back(info);
        }
        return result;
    }

    std::vector<TimelineKeyInfo> CollectTimelineKeys(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity,
        const std::size_t channelIndex)
    {
        std::vector<TimelineKeyInfo> result;
        const auto* animation = scene.animations.GetComponent(animationEntity);
        if (animation == nullptr || channelIndex >= animation->channels.size())
            return result;
        const auto& channel = animation->channels[channelIndex];
        if (channel.samplerIndex < 0 ||
            static_cast<std::size_t>(channel.samplerIndex) >= animation->samplers.size())
            return result;
        const auto* data = scene.animation_datas.GetComponent(
            animation->samplers[static_cast<std::size_t>(channel.samplerIndex)].data);
        if (data == nullptr)
            return result;
        result.reserve(data->keyframe_times.size());
        for (std::size_t i = 0; i < data->keyframe_times.size(); ++i)
            result.push_back({i, data->keyframe_times[i]});
        return result;
    }

    std::size_t TimelineValueWidth(
        const wi::scene::Scene& scene,
        const wi::scene::AnimationComponent::AnimationChannel& channel) noexcept
    {
        switch (channel.GetPathDataType())
        {
        case wi::scene::AnimationComponent::AnimationChannel::PathDataType::Event: return 0;
        case wi::scene::AnimationComponent::AnimationChannel::PathDataType::Float: return 1;
        case wi::scene::AnimationComponent::AnimationChannel::PathDataType::Float2: return 2;
        case wi::scene::AnimationComponent::AnimationChannel::PathDataType::Float3: return 3;
        case wi::scene::AnimationComponent::AnimationChannel::PathDataType::Float4: return 4;
        case wi::scene::AnimationComponent::AnimationChannel::PathDataType::Weights:
        {
            const auto* mesh = ResolveMorphMesh(scene, channel.target);
            return mesh == nullptr ? 0 : mesh->morph_targets.size();
        }
        default: return 0;
        }
    }

    bool CanRecordTimelinePreset(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity target,
        const TimelineRecordPreset preset,
        std::string* reason)
    {
        if (preset == TimelineRecordPreset::ScriptPlay || preset == TimelineRecordPreset::ScriptStop)
        {
            if (reason != nullptr)
                *reason = "Renegade governed script events are not native Wicked ScriptComponent timeline paths";
            return false;
        }
        if (target == wi::ecs::INVALID_ENTITY)
        {
            if (reason != nullptr)
                *reason = "no target entity selected";
            return false;
        }
        std::vector<float> value;
        for (const auto path : TimelineRecordPresetPaths(preset))
        {
            if (!CapturePathValue(scene, target, path, value, reason))
                return false;
        }
        return !TimelineRecordPresetPaths(preset).empty();
    }

    TimelineGraphSnapshot CaptureTimelineGraphSnapshot(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity)
    {
        TimelineGraphSnapshot snapshot;
        const auto* animation = scene.animations.GetComponent(animationEntity);
        if (animation == nullptr)
            return snapshot;
        snapshot.channels.assign(animation->channels.begin(), animation->channels.end());
        snapshot.samplers.assign(animation->samplers.begin(), animation->samplers.end());

        std::unordered_set<wi::ecs::Entity> seen;
        for (const auto& sampler : animation->samplers)
        {
            if (sampler.data == wi::ecs::INVALID_ENTITY || !seen.insert(sampler.data).second)
                continue;
            if (const auto* data = scene.animation_datas.GetComponent(sampler.data))
            {
                TimelineDataSnapshot dataSnapshot;
                dataSnapshot.entity = sampler.data;
                dataSnapshot.data = *data;
                if (const auto* hierarchy = scene.hierarchy.GetComponent(sampler.data))
                {
                    dataSnapshot.hasHierarchy = true;
                    dataSnapshot.hierarchy = *hierarchy;
                }
                snapshot.data.push_back(std::move(dataSnapshot));
            }
        }
        return snapshot;
    }

    CreateTimelineAnimationCommand::CreateTimelineAnimationCommand(
        wi::scene::Scene& scene,
        std::string name)
        : scene_(&scene), name_(std::move(name))
    {
        if (name_.empty())
            name_ = "Animation";
    }

    bool CreateTimelineAnimationCommand::Execute()
    {
        if (scene_ == nullptr)
            return false;
        if (entity_ == wi::ecs::INVALID_ENTITY)
            entity_ = wi::ecs::CreateEntity();
        if (EntityExists(*scene_, entity_))
            return false;

        scene_->names.Create(entity_).name = name_;
        auto& animation = scene_->animations.Create(entity_);
        animation.start = 0.0f;
        animation.end = 1.0f;
        animation.timer = 0.0f;
        animation.last_update_time = 0.0f;
        animation.speed = 1.0f;
        animation.amount = 1.0f;
        animation.SetLooped(true);
        return true;
    }

    void CreateTimelineAnimationCommand::Undo()
    {
        if (scene_ != nullptr && EntityExists(*scene_, entity_))
            scene_->Entity_Remove(entity_, true);
    }

    RecordTimelineKeyCommand::RecordTimelineKeyCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity,
        const wi::ecs::Entity target,
        const TimelineRecordPreset preset,
        const float time)
        : scene_(&scene), animationEntity_(animationEntity), target_(target), preset_(preset), time_(time)
    {
    }

    bool RecordTimelineKeyCommand::Execute()
    {
        if (scene_ == nullptr || animationEntity_ == wi::ecs::INVALID_ENTITY ||
            target_ == wi::ecs::INVALID_ENTITY || !std::isfinite(time_))
            return false;
        if (prepared_)
            return ApplySnapshot(*scene_, animationEntity_, after_);

        auto* animation = scene_->animations.GetComponent(animationEntity_);
        if (animation == nullptr)
        {
            error_ = "native AnimationComponent is missing";
            return false;
        }

        const auto paths = TimelineRecordPresetPaths(preset_);
        if (paths.empty())
        {
            error_ = preset_ == TimelineRecordPreset::ScriptPlay || preset_ == TimelineRecordPreset::ScriptStop
                ? "Renegade governed script events are not exposed as Wicked ScriptComponent timeline paths"
                : "record preset has no native paths";
            return false;
        }

        before_ = CaptureTimelineGraphSnapshot(*scene_, animationEntity_);
        std::vector<float> value;
        recordedPathCount_ = 0;

        for (const auto path : paths)
        {
            if (!CapturePathValue(*scene_, target_, path, value, &error_))
            {
                (void)ApplySnapshot(*scene_, animationEntity_, before_, &createdDataEntities_, true);
                return false;
            }

            int channelIndex = FindChannelIndex(*animation, target_, path);
            if (channelIndex < 0)
            {
                auto& channel = animation->channels.emplace_back();
                channel.samplerIndex = static_cast<int>(animation->samplers.size());
                channel.target = target_;
                channel.path = path;
                auto& sampler = animation->samplers.emplace_back();
                const auto dataEntity = wi::ecs::CreateEntity();
                scene_->animation_datas.Create(dataEntity);
                scene_->Component_Attach(dataEntity, animationEntity_);
                sampler.data = dataEntity;
                createdDataEntities_.push_back(dataEntity);
                channelIndex = static_cast<int>(animation->channels.size() - 1);
            }

            auto& channel = animation->channels[static_cast<std::size_t>(channelIndex)];
            if (channel.samplerIndex < 0 ||
                static_cast<std::size_t>(channel.samplerIndex) >= animation->samplers.size())
            {
                error_ = "native animation channel has an invalid sampler";
                (void)ApplySnapshot(*scene_, animationEntity_, before_, &createdDataEntities_, true);
                return false;
            }
            auto& sampler = animation->samplers[static_cast<std::size_t>(channel.samplerIndex)];
            if (sampler.mode == wi::scene::AnimationComponent::AnimationSampler::CUBICSPLINE)
            {
                error_ = "cubic spline channels must be converted to linear or step before recording";
                (void)ApplySnapshot(*scene_, animationEntity_, before_, &createdDataEntities_, true);
                return false;
            }
            auto* data = scene_->animation_datas.GetComponent(sampler.data);
            if (data == nullptr)
            {
                error_ = "native animation sampler has no data component";
                (void)ApplySnapshot(*scene_, animationEntity_, before_, &createdDataEntities_, true);
                return false;
            }

            bool changed = false;
            if (!InsertOrReplaceKey(*data, value, time_, error_, changed))
            {
                (void)ApplySnapshot(*scene_, animationEntity_, before_, &createdDataEntities_, true);
                return false;
            }
            if (changed)
                ++recordedPathCount_;
            if (IsTimelineEventPath(path))
                channel.next_event = 0;
        }

        if (recordedPathCount_ == 0)
            return false;

        after_ = CaptureTimelineGraphSnapshot(*scene_, animationEntity_);
        prepared_ = true;
        return true;
    }

    void RecordTimelineKeyCommand::Undo()
    {
        if (scene_ != nullptr && prepared_)
            (void)ApplySnapshot(*scene_, animationEntity_, before_, &createdDataEntities_, true);
    }

    CloseTimelineLoopCommand::CloseTimelineLoopCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity,
        const float time)
        : scene_(&scene), animationEntity_(animationEntity), time_(time)
    {
    }

    bool CloseTimelineLoopCommand::Execute()
    {
        if (scene_ == nullptr || animationEntity_ == wi::ecs::INVALID_ENTITY || !std::isfinite(time_))
            return false;
        if (prepared_)
            return ApplySnapshot(*scene_, animationEntity_, after_);

        auto* animation = scene_->animations.GetComponent(animationEntity_);
        if (animation == nullptr)
        {
            error_ = "native AnimationComponent is missing";
            return false;
        }
        before_ = CaptureTimelineGraphSnapshot(*scene_, animationEntity_);
        closedChannelCount_ = 0;

        for (std::size_t channelIndex = 0; channelIndex < animation->channels.size(); ++channelIndex)
        {
            auto& channel = animation->channels[channelIndex];
            if (channel.GetPathDataType() ==
                wi::scene::AnimationComponent::AnimationChannel::PathDataType::Event)
            {
                // A loop seam is a value-continuity operation. Duplicating SOUND/SCRIPT
                // events would create a new gameplay action at the seam.
                continue;
            }

            wi::scene::AnimationDataComponent* data = nullptr;
            if (!ResolveData(*scene_, *animation, channelIndex, data, error_))
            {
                (void)ApplySnapshot(*scene_, animationEntity_, before_);
                return false;
            }
            if (data->keyframe_times.empty())
                continue;
            if (!ReorderKeys(*data, error_))
            {
                (void)ApplySnapshot(*scene_, animationEntity_, before_);
                return false;
            }

            const std::size_t keyCount = data->keyframe_times.size();
            if (!data->keyframe_data.empty() && data->keyframe_data.size() % keyCount != 0)
            {
                error_ = "animation key data is malformed and cannot close the loop safely";
                (void)ApplySnapshot(*scene_, animationEntity_, before_);
                return false;
            }
            const std::size_t stride = data->keyframe_data.empty()
                ? 0
                : data->keyframe_data.size() / keyCount;
            std::vector<float> firstValue;
            firstValue.reserve(stride);
            for (std::size_t component = 0; component < stride; ++component)
                firstValue.push_back(data->keyframe_data[component]);

            bool changed = false;
            if (!InsertOrReplaceKey(*data, firstValue, time_, error_, changed))
            {
                (void)ApplySnapshot(*scene_, animationEntity_, before_);
                return false;
            }
            if (changed)
                ++closedChannelCount_;
        }

        if (closedChannelCount_ == 0)
        {
            error_ = "timeline has no value channels requiring a loop-closing key at this time";
            (void)ApplySnapshot(*scene_, animationEntity_, before_);
            return false;
        }
        after_ = CaptureTimelineGraphSnapshot(*scene_, animationEntity_);
        prepared_ = true;
        return true;
    }

    void CloseTimelineLoopCommand::Undo()
    {
        if (scene_ != nullptr && prepared_)
            (void)ApplySnapshot(*scene_, animationEntity_, before_);
    }

    MoveTimelineKeyCommand::MoveTimelineKeyCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity,
        const std::size_t channelIndex,
        const std::size_t keyIndex,
        const float time)
        : scene_(&scene), animationEntity_(animationEntity), channelIndex_(channelIndex),
          keyIndex_(keyIndex), time_(time)
    {
    }

    bool MoveTimelineKeyCommand::Execute()
    {
        if (scene_ == nullptr || !std::isfinite(time_))
            return false;
        if (prepared_)
            return ApplySnapshot(*scene_, animationEntity_, after_);
        auto* animation = scene_->animations.GetComponent(animationEntity_);
        if (animation == nullptr)
        {
            error_ = "native AnimationComponent is missing";
            return false;
        }
        wi::scene::AnimationDataComponent* data = nullptr;
        if (!ResolveData(*scene_, *animation, channelIndex_, data, error_))
            return false;
        if (keyIndex_ >= data->keyframe_times.size())
        {
            error_ = "timeline key index is out of range";
            return false;
        }
        if (std::abs(data->keyframe_times[keyIndex_] - time_) <= KeyTimeEpsilon)
            return false;

        const std::size_t keyCount = data->keyframe_times.size();
        if (!data->keyframe_data.empty() && data->keyframe_data.size() % keyCount != 0)
        {
            error_ = "animation key data is malformed and cannot be moved safely";
            return false;
        }
        const std::size_t stride = data->keyframe_data.empty()
            ? 0
            : data->keyframe_data.size() / keyCount;
        std::vector<float> value;
        value.reserve(stride);
        for (std::size_t component = 0; component < stride; ++component)
            value.push_back(data->keyframe_data[keyIndex_ * stride + component]);

        before_ = CaptureTimelineGraphSnapshot(*scene_, animationEntity_);
        data->keyframe_times.erase(
            data->keyframe_times.begin() + static_cast<std::ptrdiff_t>(keyIndex_));
        if (stride > 0)
        {
            const auto first = data->keyframe_data.begin() +
                static_cast<std::ptrdiff_t>(keyIndex_ * stride);
            data->keyframe_data.erase(first, first + static_cast<std::ptrdiff_t>(stride));
        }
        bool changed = false;
        if (!InsertOrReplaceKey(*data, value, time_, error_, changed))
        {
            (void)ApplySnapshot(*scene_, animationEntity_, before_);
            return false;
        }
        if (IsTimelineEventPath(animation->channels[channelIndex_].path))
            animation->channels[channelIndex_].next_event = 0;
        after_ = CaptureTimelineGraphSnapshot(*scene_, animationEntity_);
        prepared_ = true;
        return true;
    }

    void MoveTimelineKeyCommand::Undo()
    {
        if (scene_ != nullptr && prepared_)
            (void)ApplySnapshot(*scene_, animationEntity_, before_);
    }

    DeleteTimelineKeyCommand::DeleteTimelineKeyCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity,
        const std::size_t channelIndex,
        const std::size_t keyIndex)
        : scene_(&scene), animationEntity_(animationEntity), channelIndex_(channelIndex), keyIndex_(keyIndex)
    {
    }

    bool DeleteTimelineKeyCommand::Execute()
    {
        if (scene_ == nullptr)
            return false;
        if (prepared_)
            return ApplySnapshot(*scene_, animationEntity_, after_);
        auto* animation = scene_->animations.GetComponent(animationEntity_);
        if (animation == nullptr)
        {
            error_ = "native AnimationComponent is missing";
            return false;
        }
        wi::scene::AnimationDataComponent* data = nullptr;
        if (!ResolveData(*scene_, *animation, channelIndex_, data, error_))
            return false;
        const std::size_t keyCount = data->keyframe_times.size();
        if (keyIndex_ >= keyCount)
        {
            error_ = "timeline key index is out of range";
            return false;
        }
        if (!data->keyframe_data.empty() && data->keyframe_data.size() % keyCount != 0)
        {
            error_ = "animation key data is malformed and cannot delete safely";
            return false;
        }

        before_ = CaptureTimelineGraphSnapshot(*scene_, animationEntity_);
        const std::size_t stride = data->keyframe_data.empty()
            ? 0
            : data->keyframe_data.size() / keyCount;
        data->keyframe_times.erase(data->keyframe_times.begin() + static_cast<std::ptrdiff_t>(keyIndex_));
        if (stride > 0)
        {
            const auto first = data->keyframe_data.begin() + static_cast<std::ptrdiff_t>(keyIndex_ * stride);
            data->keyframe_data.erase(first, first + static_cast<std::ptrdiff_t>(stride));
        }
        if (IsTimelineEventPath(animation->channels[channelIndex_].path))
            animation->channels[channelIndex_].next_event = 0;
        after_ = CaptureTimelineGraphSnapshot(*scene_, animationEntity_);
        prepared_ = true;
        return true;
    }

    void DeleteTimelineKeyCommand::Undo()
    {
        if (scene_ != nullptr && prepared_)
            (void)ApplySnapshot(*scene_, animationEntity_, before_);
    }

    SetTimelineSamplerModeCommand::SetTimelineSamplerModeCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity animationEntity,
        const std::size_t channelIndex,
        const AnimationSamplerMode mode)
        : scene_(&scene), animationEntity_(animationEntity), channelIndex_(channelIndex), mode_(mode)
    {
    }

    bool SetTimelineSamplerModeCommand::Execute()
    {
        if (scene_ == nullptr)
            return false;
        if (prepared_)
            return ApplySnapshot(*scene_, animationEntity_, after_);
        auto* animation = scene_->animations.GetComponent(animationEntity_);
        if (animation == nullptr || channelIndex_ >= animation->channels.size())
        {
            error_ = "native animation channel is missing";
            return false;
        }
        auto& channel = animation->channels[channelIndex_];
        if (channel.samplerIndex < 0 ||
            static_cast<std::size_t>(channel.samplerIndex) >= animation->samplers.size())
        {
            error_ = "native animation channel has an invalid sampler";
            return false;
        }
        auto& sampler = animation->samplers[static_cast<std::size_t>(channel.samplerIndex)];
        if (sampler.mode == mode_)
            return false;
        if (mode_ == wi::scene::AnimationComponent::AnimationSampler::CUBICSPLINE &&
            sampler.mode != wi::scene::AnimationComponent::AnimationSampler::CUBICSPLINE)
        {
            error_ = "cubic spline creation is not supported; imported cubic data is preserved read-only";
            return false;
        }

        before_ = CaptureTimelineGraphSnapshot(*scene_, animationEntity_);
        if (sampler.mode == wi::scene::AnimationComponent::AnimationSampler::CUBICSPLINE &&
            mode_ != wi::scene::AnimationComponent::AnimationSampler::CUBICSPLINE)
        {
            auto* data = scene_->animation_datas.GetComponent(sampler.data);
            if (data == nullptr)
            {
                error_ = "native animation sampler has no data component";
                return false;
            }
            const std::size_t keyCount = data->keyframe_times.size();
            const std::size_t width = TimelineValueWidth(*scene_, channel);
            if (keyCount > 0 && width > 0)
            {
                const std::size_t cubicStride = width * 3;
                if (data->keyframe_data.size() != keyCount * cubicStride)
                {
                    error_ = "imported cubic animation data has an unsupported layout";
                    return false;
                }
                wi::vector<float> linear;
                linear.reserve(keyCount * width);
                for (std::size_t key = 0; key < keyCount; ++key)
                {
                    const std::size_t valueOffset = key * cubicStride + width;
                    for (std::size_t component = 0; component < width; ++component)
                        linear.push_back(data->keyframe_data[valueOffset + component]);
                }
                data->keyframe_data = std::move(linear);
            }
        }
        sampler.mode = mode_;
        channel.next_event = 0;
        after_ = CaptureTimelineGraphSnapshot(*scene_, animationEntity_);
        prepared_ = true;
        return true;
    }

    void SetTimelineSamplerModeCommand::Undo()
    {
        if (scene_ != nullptr && prepared_)
            (void)ApplySnapshot(*scene_, animationEntity_, before_);
    }
}
