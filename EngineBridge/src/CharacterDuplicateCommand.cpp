#include "renegade/bridge/CommandService.h"

#include "renegade/bridge/CharacterProfileService.h"
#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/IdentityService.h"

#include <algorithm>
#include <array>
#include <vector>
#include <utility>

namespace
{
    renegade::bridge::DuplicateEntityCompanionFactory duplicateCompanionFactory;

    bool EntityExists(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
    {
        if (entity == wi::ecs::INVALID_ENTITY)
            return false;
        wi::unordered_set<wi::ecs::Entity> entities;
        scene.FindAllEntities(entities);
        return entities.count(entity) != 0;
    }
}

namespace renegade::bridge
{
    void SetDuplicateEntityCompanionFactory(
        DuplicateEntityCompanionFactory factory)
    {
        duplicateCompanionFactory = std::move(factory);
    }

    void ClearDuplicateEntityCompanionFactory() noexcept
    {
        duplicateCompanionFactory = {};
    }

    DuplicateEntityCommand::DuplicateEntityCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity source)
        : scene_(&scene)
        , source_(source)
    {
    }

    bool DuplicateEntityCommand::Execute()
    {
        if (!hasSnapshot_)
        {
            if (scene_ == nullptr || !EntityExists(*scene_, source_))
                return false;

            const bool characterSource = IsRenegadeCharacter(*scene_, source_);
            CharacterAuthoringSettings characterSettings;
            CharacterAdvancedOverrides advancedOverrides;
            if (characterSource)
            {
                characterSettings = CaptureCharacterSettings(*scene_, source_);
                std::string advancedError;
                if (!CaptureCharacterAdvancedOverrides(
                        *scene_, source_, advancedOverrides, advancedError))
                    return false;
            }

            const auto entitiesBefore = EnumeratePersistentSceneEntities(*scene_);
            // Use the native duplication algorithm while retaining its entity
            // remap, so live material resources can follow every copied child.
            wi::Archive duplicateArchive;
            wi::ecs::EntitySerializer duplicateSerializer;
            scene_->Entity_Serialize(duplicateArchive, duplicateSerializer, source_,
                wi::scene::Scene::EntitySerializeFlags::RECURSIVE);
            duplicateArchive.SetReadModeAndResetPos(true);
            duplicate_ = scene_->Entity_Serialize(duplicateArchive, duplicateSerializer,
                wi::ecs::INVALID_ENTITY,
                wi::scene::Scene::EntitySerializeFlags::RECURSIVE |
                wi::scene::Scene::EntitySerializeFlags::KEEP_INTERNAL_ENTITY_REFERENCES);
            wi::jobsystem::Wait(duplicateSerializer.ctx);
            if (duplicate_ == wi::ecs::INVALID_ENTITY)
                return false;

            struct LiveMaterialBindings
            {
                wi::ecs::Entity entity;
                std::array<wi::scene::MaterialComponent::TextureMap,
                    wi::scene::MaterialComponent::TEXTURESLOT_COUNT> textures;
            };
            std::vector<LiveMaterialBindings> liveBindings;
            for (const auto& mapping : duplicateSerializer.remap)
            {
                // KEEP_INTERNAL_ENTITY_REFERENCES also mentions shared external
                // objects. Only newly created materials belong to this command.
                if (std::binary_search(entitiesBefore.begin(), entitiesBefore.end(),
                        mapping.second))
                    continue;
                const auto* original = scene_->materials.GetComponent(mapping.first);
                auto* copy = scene_->materials.GetComponent(mapping.second);
                if (original == nullptr || copy == nullptr)
                    continue;
                LiveMaterialBindings binding;
                binding.entity = mapping.second;
                bool hasLiveResource = false;
                for (size_t slot = 0; slot < binding.textures.size(); ++slot)
                {
                    binding.textures[slot] = original->textures[slot];
                    hasLiveResource |= original->textures[slot].resource.IsValid();
                }
                if (!hasLiveResource)
                    continue;
                for (size_t slot = 0; slot < binding.textures.size(); ++slot)
                    copy->textures[slot] = binding.textures[slot];
                copy->SetDirty();
                liveBindings.push_back(std::move(binding));
            }

            if (auto* name = scene_->names.GetComponent(duplicate_)) {
                std::string base=name->name;
                // Remove legacy accumulated Copy suffixes, then an existing numeric suffix.
                while(base.size()>5&&base.compare(base.size()-5,5," Copy")==0)base.resize(base.size()-5);
                const auto space=base.find_last_of(' ');
                if(space!=std::string::npos && space+1<base.size() &&
                   std::all_of(base.begin()+space+1,base.end(),[](unsigned char c){return c>='0'&&c<='9';}))
                    base.resize(space);
                if(base.empty())base="Entity";
                std::string candidate;unsigned suffix=2;bool clash;
                do {
                    candidate=base+" "+std::to_string(suffix++);clash=false;
                    for(size_t i=0;i<scene_->names.GetCount();++i)
                        if(scene_->names.GetEntity(i)!=duplicate_&&scene_->names[i].name==candidate){clash=true;break;}
                }while(clash);
                name->name=candidate;
            }

            // Wicked duplicates MetadataComponent values verbatim. Replace every
            // copied persistent ID in the new hierarchy before any Character or
            // companion authoring is rebuilt, so nothing can accidentally adopt
            // the source actor's stable identity.
            const auto entitiesAfter = EnumeratePersistentSceneEntities(*scene_);
            std::string identityError;
            for (const auto entity : entitiesAfter)
            {
                if (!std::binary_search(
                        entitiesBefore.begin(), entitiesBefore.end(), entity) &&
                    !AssignNewPersistentEntityId(
                        *scene_, entity, identityError))
                {
                    scene_->Entity_Remove(duplicate_);
                    duplicate_ = wi::ecs::INVALID_ENTITY;
                    return false;
                }
            }

            if (characterSource)
            {
                if (!IsRenegadeCharacter(*scene_, duplicate_))
                {
                    scene_->Entity_Remove(duplicate_);
                    duplicate_ = wi::ecs::INVALID_ENTITY;
                    return false;
                }

                // A raw Wicked duplicate would copy CharacterComponent runtime
                // internals. Strip the copied Character semantic/controller and
                // rebuild it through Renegade's accepted Character foundation,
                // then restore only authored settings and advanced overrides.
                RemoveCharacterCommand remove(*scene_, duplicate_);
                if (!remove.Execute())
                {
                    scene_->Entity_Remove(duplicate_);
                    duplicate_ = wi::ecs::INVALID_ENTITY;
                    return false;
                }
                // An imported/manual Character can have adopted an existing
                // controller rather than owning it. Duplication must still start
                // with fresh native controller state, so never retain that copied
                // controller instance on the duplicate.
                if (scene_->characters.Contains(duplicate_))
                    scene_->characters.Remove(duplicate_);

                MakeCharacterCommand make(
                    *scene_, duplicate_, characterSettings);
                std::string advancedError;
                if (!make.Execute() ||
                    !ApplyCharacterAdvancedOverrides(
                        *scene_, duplicate_, advancedOverrides, advancedError))
                {
                    scene_->Entity_Remove(duplicate_);
                    duplicate_ = wi::ecs::INVALID_ENTITY;
                    return false;
                }
            }

            companion_ = {};
            companionActive_ = false;
            if (duplicateCompanionFactory)
            {
                std::string companionError;
                if (!duplicateCompanionFactory(
                        *scene_, source_, duplicate_, companion_, companionError))
                {
                    if (companion_.undo)
                        companion_.undo();
                    scene_->Entity_Remove(duplicate_);
                    duplicate_ = wi::ecs::INVALID_ENTITY;
                    return false;
                }
                companionActive_ =
                    static_cast<bool>(companion_.undo) ||
                    static_cast<bool>(companion_.redo);
            }

            // Managed textures deliberately have no loose filename. Their
            // metadata survives native serialization, but the live resource does
            // not. Retain it for Redo as well as the first duplicate, composing
            // with existing governed script attachment callbacks.
            if (!liveBindings.empty())
            {
                auto companionRedo = std::move(companion_.redo);
                companion_.redo = [scene = scene_, bindings = std::move(liveBindings),
                    companionRedo = std::move(companionRedo)]() mutable
                {
                    if (companionRedo && !companionRedo())
                        return false;
                    for (const auto& binding : bindings)
                    {
                        auto* material = scene->materials.GetComponent(binding.entity);
                        if (material == nullptr)
                            return false;
                        for (size_t slot = 0; slot < binding.textures.size(); ++slot)
                            material->textures[slot] = binding.textures[slot];
                        material->SetDirty();
                    }
                    return true;
                };
                companionActive_ = true;
            }

            snapshot_.SetReadModeAndResetPos(false);
            wi::ecs::EntitySerializer serializer;
            scene_->Entity_Serialize(snapshot_, serializer, duplicate_);
            hasSnapshot_ = true;
            return true;
        }

        if (scene_ == nullptr || EntityExists(*scene_, duplicate_))
            return false;

        snapshot_.SetReadModeAndResetPos(true);
        wi::ecs::EntitySerializer serializer;
        serializer.allow_remap = false;
        const auto restored = scene_->Entity_Serialize(snapshot_, serializer);
        if (restored != duplicate_)
            return false;

        if (companionActive_ && companion_.redo && !companion_.redo())
        {
            scene_->Entity_Remove(duplicate_);
            return false;
        }
        return true;
    }

    void DuplicateEntityCommand::Undo()
    {
        if (companionActive_ && companion_.undo)
            companion_.undo();
        if (scene_ != nullptr && EntityExists(*scene_, duplicate_))
            scene_->Entity_Remove(duplicate_);
    }

    wi::ecs::Entity DuplicateEntityCommand::DuplicatedEntity() const noexcept
    {
        return duplicate_;
    }
}
