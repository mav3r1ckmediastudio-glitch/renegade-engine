#include "renegade/bridge/HumanoidRetargetService.h"
#include "ModelImporter.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <limits>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace fs = std::filesystem;

namespace
{
    using Bone = renegade::bridge::HumanoidBone;
    using Entity = wi::ecs::Entity;

    struct BoneRule
    {
        Bone bone;
        const char* name;
        std::vector<const char*> candidates;
    };

    const std::vector<BoneRule>& BoneRules()
    {
        static const std::vector<BoneRule> rules = {
            {Bone::Hips, "Hips", {"Hips", "pelvis"}},
            {Bone::Spine, "Spine", {"Spine", "spine_01"}},
            {Bone::Chest, "Chest", {"Chest", "Spine1", "spine_02"}},
            {Bone::UpperChest, "UpperChest", {"UpperChest", "Spine2", "spine_03"}},
            {Bone::Neck, "Neck", {"Neck"}},
            {Bone::Head, "Head", {"Head"}},
            {Bone::LeftEye, "LeftEye", {"LeftEye", "eye_l"}},
            {Bone::RightEye, "RightEye", {"RightEye", "eye_r"}},
            {Bone::Jaw, "Jaw", {"Jaw"}},
            {Bone::LeftUpperLeg, "LeftUpperLeg", {"LeftUpperLeg", "LeftUpLeg", "thigh_l"}},
            {Bone::LeftLowerLeg, "LeftLowerLeg", {"LeftLowerLeg", "LeftLeg", "calf_l"}},
            {Bone::LeftFoot, "LeftFoot", {"LeftFoot", "foot_l"}},
            {Bone::LeftToes, "LeftToes", {"LeftToe", "LeftToes", "ball_l"}},
            {Bone::RightUpperLeg, "RightUpperLeg", {"RightUpperLeg", "RightUpLeg", "thigh_r"}},
            {Bone::RightLowerLeg, "RightLowerLeg", {"RightLowerLeg", "RightLeg", "calf_r"}},
            {Bone::RightFoot, "RightFoot", {"RightFoot", "foot_r"}},
            {Bone::RightToes, "RightToes", {"RightToe", "RightToes", "ball_r"}},
            {Bone::LeftShoulder, "LeftShoulder", {"LeftShoulder", "clavicle_l"}},
            {Bone::LeftUpperArm, "LeftUpperArm", {"LeftUpperArm", "LeftArm", "upperarm_l"}},
            {Bone::LeftLowerArm, "LeftLowerArm", {"LeftLowerArm", "LeftForeArm", "lowerarm_l"}},
            {Bone::LeftHand, "LeftHand", {"LeftHand", "hand_l"}},
            {Bone::RightShoulder, "RightShoulder", {"RightShoulder", "clavicle_r"}},
            {Bone::RightUpperArm, "RightUpperArm", {"RightUpperArm", "RightArm", "upperarm_r"}},
            {Bone::RightLowerArm, "RightLowerArm", {"RightLowerArm", "RightForeArm", "lowerarm_r"}},
            {Bone::RightHand, "RightHand", {"RightHand", "hand_r"}},
            {Bone::LeftThumbMetacarpal, "LeftThumbMetacarpal", {"LeftThumbMetacarpal", "LeftHandThumb1", "thumb_01_l"}},
            {Bone::LeftThumbProximal, "LeftThumbProximal", {"LeftThumbProximal", "LeftHandThumb2", "thumb_02_l"}},
            {Bone::LeftThumbDistal, "LeftThumbDistal", {"LeftThumbDistal", "LeftHandThumb3", "thumb_03_l"}},
            {Bone::LeftIndexProximal, "LeftIndexProximal", {"LeftIndexProximal", "LeftHandIndex1", "index_01_l"}},
            {Bone::LeftIndexIntermediate, "LeftIndexIntermediate", {"LeftIndexIntermediate", "LeftHandIndex2", "index_02_l"}},
            {Bone::LeftIndexDistal, "LeftIndexDistal", {"LeftIndexDistal", "LeftHandIndex3", "index_03_l"}},
            {Bone::LeftMiddleProximal, "LeftMiddleProximal", {"LeftMiddleProximal", "LeftHandMiddle1", "middle_01_l"}},
            {Bone::LeftMiddleIntermediate, "LeftMiddleIntermediate", {"LeftMiddleIntermediate", "LeftHandMiddle2", "middle_02_l"}},
            {Bone::LeftMiddleDistal, "LeftMiddleDistal", {"LeftMiddleDistal", "LeftHandMiddle3", "middle_03_l"}},
            {Bone::LeftRingProximal, "LeftRingProximal", {"LeftRingProximal", "LeftHandRing1", "ring_01_l"}},
            {Bone::LeftRingIntermediate, "LeftRingIntermediate", {"LeftRingIntermediate", "LeftHandRing2", "ring_02_l"}},
            {Bone::LeftRingDistal, "LeftRingDistal", {"LeftRingDistal", "LeftHandRing3", "ring_03_l"}},
            {Bone::LeftLittleProximal, "LeftLittleProximal", {"LeftLittleProximal", "LeftHandPinky1", "pinky_01_l"}},
            {Bone::LeftLittleIntermediate, "LeftLittleIntermediate", {"LeftLittleIntermediate", "LeftHandPinky2", "pinky_02_l"}},
            {Bone::LeftLittleDistal, "LeftLittleDistal", {"LeftLittleDistal", "LeftHandPinky3", "pinky_03_l"}},
            {Bone::RightThumbMetacarpal, "RightThumbMetacarpal", {"RightThumbMetacarpal", "RightHandThumb1", "thumb_01_r"}},
            {Bone::RightThumbProximal, "RightThumbProximal", {"RightThumbProximal", "RightHandThumb2", "thumb_02_r"}},
            {Bone::RightThumbDistal, "RightThumbDistal", {"RightThumbDistal", "RightHandThumb3", "thumb_03_r"}},
            {Bone::RightIndexProximal, "RightIndexProximal", {"RightIndexProximal", "RightHandIndex1", "index_01_r"}},
            {Bone::RightIndexIntermediate, "RightIndexIntermediate", {"RightIndexIntermediate", "RightHandIndex2", "index_02_r"}},
            {Bone::RightIndexDistal, "RightIndexDistal", {"RightIndexDistal", "RightHandIndex3", "index_03_r"}},
            {Bone::RightMiddleProximal, "RightMiddleProximal", {"RightMiddleProximal", "RightHandMiddle1", "middle_01_r"}},
            {Bone::RightMiddleIntermediate, "RightMiddleIntermediate", {"RightMiddleIntermediate", "RightHandMiddle2", "middle_02_r"}},
            {Bone::RightMiddleDistal, "RightMiddleDistal", {"RightMiddleDistal", "RightHandMiddle3", "middle_03_r"}},
            {Bone::RightRingProximal, "RightRingProximal", {"RightRingProximal", "RightHandRing1", "ring_01_r"}},
            {Bone::RightRingIntermediate, "RightRingIntermediate", {"RightRingIntermediate", "RightHandRing2", "ring_02_r"}},
            {Bone::RightRingDistal, "RightRingDistal", {"RightRingDistal", "RightHandRing3", "ring_03_r"}},
            {Bone::RightLittleProximal, "RightLittleProximal", {"RightLittleProximal", "RightHandPinky1", "pinky_01_r"}},
            {Bone::RightLittleIntermediate, "RightLittleIntermediate", {"RightLittleIntermediate", "RightHandPinky2", "pinky_02_r"}},
            {Bone::RightLittleDistal, "RightLittleDistal", {"RightLittleDistal", "RightHandPinky3", "pinky_03_r"}},
        };
        return rules;
    }

    std::string Normalize(std::string_view value)
    {
        std::string result;
        result.reserve(value.size());
        for (const unsigned char c : value)
        {
            if (std::isalnum(c))
                result.push_back(static_cast<char>(std::toupper(c)));
        }
        return result;
    }

    int MatchScore(const std::string& entityName, const char* candidate)
    {
        const std::string name = Normalize(entityName);
        const std::string token = Normalize(candidate == nullptr ? "" : candidate);
        if (name.empty() || token.empty())
            return -1;
        if (name == token)
            return 30000 + static_cast<int>(token.size());
        if (name.size() >= token.size() &&
            name.compare(name.size() - token.size(), token.size(), token) == 0)
        {
            return 20000 + static_cast<int>(token.size());
        }
        if (name.find(token) != std::string::npos)
            return 10000 + static_cast<int>(token.size());
        return -1;
    }

    bool EntityExists(const wi::scene::Scene& scene, const Entity entity)
    {
        if (entity == wi::ecs::INVALID_ENTITY)
            return false;
        wi::unordered_set<Entity> entities;
        scene.FindAllEntities(entities);
        return entities.count(entity) != 0;
    }

    bool MappingEquals(
        const renegade::bridge::HumanoidMappingState& a,
        const renegade::bridge::HumanoidMappingState& b) noexcept
    {
        return a.componentExists == b.componentExists && a.bones == b.bones;
    }

    std::string LowerExtension(const std::string& path)
    {
        std::string extension = fs::u8path(path).extension().u8string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return extension;
    }
}

namespace renegade::bridge
{
    const char* HumanoidBoneName(const HumanoidBone bone) noexcept
    {
        const auto& rules = BoneRules();
        const auto it = std::find_if(rules.begin(), rules.end(),
            [bone](const BoneRule& rule) { return rule.bone == bone; });
        return it == rules.end() ? "Unknown" : it->name;
    }

    wi::ecs::Entity FindHumanoidRigEntity(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity selected) noexcept
    {
        if (selected == wi::ecs::INVALID_ENTITY)
            return wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity cursor = selected;
        for (std::size_t depth = 0; depth < 512; ++depth)
        {
            if (scene.humanoids.Contains(cursor) || scene.armatures.Contains(cursor))
                return cursor;
            const auto* hierarchy = scene.hierarchy.GetComponent(cursor);
            if (hierarchy == nullptr || hierarchy->parentID == wi::ecs::INVALID_ENTITY ||
                hierarchy->parentID == cursor)
            {
                break;
            }
            cursor = hierarchy->parentID;
        }
        for (std::size_t i = 0; i < scene.armatures.GetCount(); ++i)
        {
            const auto entity = scene.armatures.GetEntity(i);
            if (entity == selected || scene.Entity_IsDescendant(entity, selected))
                return entity;
        }
        for (std::size_t i = 0; i < scene.humanoids.GetCount(); ++i)
        {
            const auto entity = scene.humanoids.GetEntity(i);
            if (entity == selected || scene.Entity_IsDescendant(entity, selected))
                return entity;
        }
        return wi::ecs::INVALID_ENTITY;
    }

    std::vector<wi::ecs::Entity> CollectArmatureBones(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity rigEntity)
    {
        std::vector<wi::ecs::Entity> result;
        if (const auto* armature = scene.armatures.GetComponent(rigEntity))
        {
            result.reserve(armature->boneCollection.size());
            for (const auto bone : armature->boneCollection)
            {
                if (bone != wi::ecs::INVALID_ENTITY && EntityExists(scene, bone))
                    result.push_back(bone);
            }
        }
        return result;
    }

    HumanoidMappingState CaptureHumanoidMapping(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity rigEntity) noexcept
    {
        HumanoidMappingState result;
        const auto* humanoid = scene.humanoids.GetComponent(rigEntity);
        if (humanoid == nullptr)
            return result;
        result.componentExists = true;
        for (std::size_t i = 0; i < HumanoidBoneCount; ++i)
            result.bones[i] = humanoid->bones[i];
        return result;
    }

    bool IsHumanoidMappingValid(const HumanoidMappingState& mapping) noexcept
    {
        if (!mapping.componentExists)
            return false;
        wi::scene::HumanoidComponent probe;
        for (std::size_t i = 0; i < HumanoidBoneCount; ++i)
            probe.bones[i] = mapping.bones[i];
        return probe.IsValid();
    }

    HumanoidAutoMapResult BuildAutoHumanoidMapping(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity rigEntity)
    {
        HumanoidAutoMapResult result;
        result.mapping.componentExists = true;
        const auto* armature = scene.armatures.GetComponent(rigEntity);
        if (armature == nullptr)
        {
            result.error = "Selected character has no native Wicked ArmatureComponent.";
            return result;
        }
        struct NamedBone
        {
            Entity entity = wi::ecs::INVALID_ENTITY;
            std::string name;
        };
        std::vector<NamedBone> namedBones;
        namedBones.reserve(armature->boneCollection.size());
        for (const auto entity : armature->boneCollection)
        {
            const auto* name = scene.names.GetComponent(entity);
            if (entity != wi::ecs::INVALID_ENTITY && name != nullptr && !name->name.empty())
                namedBones.push_back({entity, name->name});
        }
        std::unordered_set<Entity> used;
        for (const auto& rule : BoneRules())
        {
            int bestScore = -1;
            Entity bestEntity = wi::ecs::INVALID_ENTITY;
            for (const auto& named : namedBones)
            {
                if (used.count(named.entity) != 0)
                    continue;
                for (const auto* candidate : rule.candidates)
                {
                    const int score = MatchScore(named.name, candidate);
                    if (score > bestScore)
                    {
                        bestScore = score;
                        bestEntity = named.entity;
                    }
                }
            }
            if (bestEntity != wi::ecs::INVALID_ENTITY)
            {
                result.mapping.bones[static_cast<std::size_t>(rule.bone)] = bestEntity;
                used.insert(bestEntity);
                ++result.mappedBones;
            }
        }
        result.valid = IsHumanoidMappingValid(result.mapping);
        if (result.mappedBones == 0)
            result.error = "No VRM, Mixamo or supported creator bone names matched this armature.";
        return result;
    }

    SetHumanoidMappingCommand::SetHumanoidMappingCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity rigEntity,
        const HumanoidMappingState& after)
        : scene_(&scene), rigEntity_(rigEntity),
          before_(CaptureHumanoidMapping(scene, rigEntity)), after_(after)
    {
    }

    bool SetHumanoidMappingCommand::Apply(const HumanoidMappingState& mapping) noexcept
    {
        if (scene_ == nullptr || rigEntity_ == wi::ecs::INVALID_ENTITY ||
            !EntityExists(*scene_, rigEntity_))
        {
            return false;
        }
        if (!mapping.componentExists)
        {
            scene_->humanoids.Remove(rigEntity_);
            return true;
        }
        auto* humanoid = scene_->humanoids.GetComponent(rigEntity_);
        if (humanoid == nullptr)
            humanoid = &scene_->humanoids.Create(rigEntity_);
        for (std::size_t i = 0; i < HumanoidBoneCount; ++i)
            humanoid->bones[i] = mapping.bones[i];

        // Native ragdoll state contains cached bodies/joints for the old bone map.
        // Invalidate it whenever the mapping changes; the existing ragdoll service
        // rebuilds it from the authored component state on next use.
        humanoid->ragdoll = {};
        return true;
    }

    bool SetHumanoidMappingCommand::Execute()
    {
        return !MappingEquals(before_, after_) && Apply(after_);
    }

    void SetHumanoidMappingCommand::Undo()
    {
        (void)Apply(before_);
    }

    SetHumanoidBoneCommand::SetHumanoidBoneCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity rigEntity,
        const HumanoidBone bone,
        const wi::ecs::Entity mappedEntity)
        : mappingCommand_(scene, rigEntity, [&]()
        {
            auto after = CaptureHumanoidMapping(scene, rigEntity);
            after.componentExists = true;
            const auto index = static_cast<std::size_t>(bone);
            if (index < HumanoidBoneCount)
                after.bones[index] = mappedEntity;
            return after;
        }())
    {
    }

    bool SetHumanoidBoneCommand::Execute()
    {
        return mappingCommand_.Execute();
    }

    void SetHumanoidBoneCommand::Undo()
    {
        mappingCommand_.Undo();
    }

    HumanoidAnimationSourceFormat ClassifyHumanoidAnimationSource(
        const std::string& path) noexcept
    {
        try
        {
            const auto ext = LowerExtension(path);
            if (ext == ".wiscene") return HumanoidAnimationSourceFormat::Wiscene;
            if (ext == ".fbx") return HumanoidAnimationSourceFormat::Fbx;
            if (ext == ".gltf") return HumanoidAnimationSourceFormat::Gltf;
            if (ext == ".glb") return HumanoidAnimationSourceFormat::Glb;
            if (ext == ".vrm") return HumanoidAnimationSourceFormat::Vrm;
            if (ext == ".vrma") return HumanoidAnimationSourceFormat::Vrma;
        }
        catch (...)
        {
        }
        return HumanoidAnimationSourceFormat::Unknown;
    }

    const char* HumanoidAnimationSourceFormatName(
        const HumanoidAnimationSourceFormat format) noexcept
    {
        switch (format)
        {
        case HumanoidAnimationSourceFormat::Wiscene: return "WISCENE";
        case HumanoidAnimationSourceFormat::Fbx: return "FBX";
        case HumanoidAnimationSourceFormat::Gltf: return "glTF";
        case HumanoidAnimationSourceFormat::Glb: return "GLB";
        case HumanoidAnimationSourceFormat::Vrm: return "VRM";
        case HumanoidAnimationSourceFormat::Vrma: return "VRMA";
        default: return "unknown";
        }
    }

    RetargetHumanoidAnimationsCommand::RetargetHumanoidAnimationsCommand(
        wi::scene::Scene& destinationScene,
        const wi::ecs::Entity destinationHumanoid,
        std::string sourcePath)
        : scene_(&destinationScene), destinationHumanoid_(destinationHumanoid),
          sourcePath_(std::move(sourcePath))
    {
    }

    bool RetargetHumanoidAnimationsCommand::LoadSource(wi::scene::Scene& sourceScene)
    {
        result_.sourceFormat = ClassifyHumanoidAnimationSource(sourcePath_);
        if (result_.sourceFormat == HumanoidAnimationSourceFormat::Unknown)
        {
            result_.error = "Unsupported humanoid animation source: " + sourcePath_;
            return false;
        }
        std::error_code ec;
        if (!fs::is_regular_file(fs::u8path(sourcePath_), ec) || ec)
        {
            result_.error = "Humanoid animation source does not exist: " + sourcePath_;
            return false;
        }
        try
        {
            switch (result_.sourceFormat)
            {
            case HumanoidAnimationSourceFormat::Wiscene:
                wi::scene::LoadModel(sourceScene, sourcePath_);
                break;
            case HumanoidAnimationSourceFormat::Fbx:
                ImportModel_FBX(sourcePath_, sourceScene);
                break;
            case HumanoidAnimationSourceFormat::Gltf:
            case HumanoidAnimationSourceFormat::Glb:
            case HumanoidAnimationSourceFormat::Vrm:
            case HumanoidAnimationSourceFormat::Vrma:
                ImportModel_GLTF(sourcePath_, sourceScene);
                break;
            default:
                result_.error = "Internal humanoid animation import dispatch error.";
                return false;
            }
        }
        catch (const std::exception& exception)
        {
            result_.error = std::string("Animation source import failed: ") + exception.what();
            return false;
        }
        catch (...)
        {
            result_.error = "Animation source import failed with an unknown error.";
            return false;
        }
        if (sourceScene.animations.GetCount() == 0)
        {
            result_.error = std::string(HumanoidAnimationSourceFormatName(result_.sourceFormat)) +
                " source contains no native Wicked animation clips.";
            return false;
        }
        return true;
    }

    bool RetargetHumanoidAnimationsCommand::CapturePreparedResult()
    {
        if (scene_ == nullptr || result_.createdAnimations.empty())
            return false;

        animationSnapshots_.clear();
        dataSnapshots_.clear();
        std::unordered_set<wi::ecs::Entity> capturedData;

        for (const auto entity : result_.createdAnimations)
        {
            const auto* animation = scene_->animations.GetComponent(entity);
            if (animation == nullptr)
                return false;

            RetargetAnimationSnapshot snapshot;
            snapshot.entity = entity;
            snapshot.animation = *animation;
            if (const auto* name = scene_->names.GetComponent(entity))
                snapshot.name = name->name;
            animationSnapshots_.push_back(std::move(snapshot));

            for (const auto& sampler : animation->samplers)
            {
                if (sampler.data == wi::ecs::INVALID_ENTITY ||
                    !capturedData.insert(sampler.data).second)
                {
                    continue;
                }
                const auto* data = scene_->animation_datas.GetComponent(sampler.data);
                if (data == nullptr)
                    return false;
                dataSnapshots_.push_back({sampler.data, *data});
            }
        }

        prepared_ = !animationSnapshots_.empty();
        return prepared_;
    }

    bool RetargetHumanoidAnimationsCommand::RestorePreparedResult()
    {
        if (!prepared_ || scene_ == nullptr || destinationHumanoid_ == wi::ecs::INVALID_ENTITY)
            return false;
        if (scene_->humanoids.GetComponent(destinationHumanoid_) == nullptr)
            return false;

        for (const auto& snapshot : dataSnapshots_)
        {
            if (scene_->animation_datas.Contains(snapshot.entity))
            {
                result_.error = "Retarget redo could not restore baked data because an entity ID was reused.";
                return false;
            }
        }
        for (const auto& snapshot : animationSnapshots_)
        {
            if (EntityExists(*scene_, snapshot.entity))
            {
                result_.error = "Retarget redo could not restore an animation because an entity ID was reused.";
                return false;
            }
        }

        for (const auto& snapshot : dataSnapshots_)
            scene_->animation_datas.Create(snapshot.entity) = snapshot.data;

        result_ = {};
        result_.sourcePath = sourcePath_;
        result_.sourceFormat = ClassifyHumanoidAnimationSource(sourcePath_);
        for (const auto& snapshot : animationSnapshots_)
        {
            scene_->animations.Create(snapshot.entity) = snapshot.animation;
            if (!snapshot.name.empty())
                scene_->names.Create(snapshot.entity).name = snapshot.name;
            result_.createdAnimations.push_back(snapshot.entity);
        }
        result_.succeeded = !result_.createdAnimations.empty();
        scene_->ResetPose(destinationHumanoid_);
        return result_.succeeded;
    }

    bool RetargetHumanoidAnimationsCommand::Execute()
    {
        if (prepared_)
            return RestorePreparedResult();

        RemoveCreated();
        result_ = {};
        result_.sourcePath = sourcePath_;
        result_.sourceFormat = ClassifyHumanoidAnimationSource(sourcePath_);
        if (scene_ == nullptr || destinationHumanoid_ == wi::ecs::INVALID_ENTITY)
        {
            result_.error = "Retarget destination is invalid.";
            return false;
        }
        const auto* destination = scene_->humanoids.GetComponent(destinationHumanoid_);
        if (destination == nullptr || !destination->IsValid())
        {
            result_.error = "Destination humanoid mapping is incomplete; auto-map or correct required bones first.";
            return false;
        }

        wi::scene::Scene sourceScene;
        if (!LoadSource(sourceScene))
            return false;

        scene_->ResetPose(destinationHumanoid_);
        for (std::size_t i = 0; i < sourceScene.animations.GetCount(); ++i)
        {
            const auto sourceAnimation = sourceScene.animations.GetEntity(i);
            const auto created = scene_->RetargetAnimation(
                destinationHumanoid_, sourceAnimation, true, &sourceScene);
            if (created == wi::ecs::INVALID_ENTITY)
                continue;
            const auto* sourceName = sourceScene.names.GetComponent(sourceAnimation);
            auto& destinationName = scene_->names.Create(created);
            destinationName.name = sourceName != nullptr && !sourceName->name.empty()
                ? sourceName->name
                : "Retargeted Animation " + std::to_string(result_.createdAnimations.size() + 1);
            result_.createdAnimations.push_back(created);
        }
        if (result_.createdAnimations.empty())
        {
            result_.error =
                "Wicked could not retarget any clips. The source must contain a compatible native humanoid mapping.";
            return false;
        }
        if (!CapturePreparedResult())
        {
            const std::string failure =
                "Retarget succeeded but the baked result could not be captured for deterministic Undo/Redo.";
            RemoveCreated();
            result_.error = failure;
            return false;
        }
        result_.succeeded = true;
        return true;
    }

    void RetargetHumanoidAnimationsCommand::RemoveCreated() noexcept
    {
        if (scene_ == nullptr)
            return;

        for (const auto entity : result_.createdAnimations)
        {
            if (entity != wi::ecs::INVALID_ENTITY && EntityExists(*scene_, entity))
                scene_->Entity_Remove(entity, true);
        }
        for (const auto& snapshot : dataSnapshots_)
        {
            if (snapshot.entity != wi::ecs::INVALID_ENTITY)
                scene_->animation_datas.Remove(snapshot.entity);
        }
        result_.createdAnimations.clear();
    }

    void RetargetHumanoidAnimationsCommand::Undo()
    {
        RemoveCreated();
        if (scene_ != nullptr && destinationHumanoid_ != wi::ecs::INVALID_ENTITY)
            scene_->ResetPose(destinationHumanoid_);
    }
}
