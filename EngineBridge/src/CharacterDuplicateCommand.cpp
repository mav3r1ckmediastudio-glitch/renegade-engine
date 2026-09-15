#include "renegade/bridge/CommandService.h"

#include "renegade/bridge/CharacterProfileService.h"
#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/IdentityService.h"

#include <algorithm>
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
            duplicate_ = scene_->Entity_Duplicate(source_);
            if (duplicate_ == wi::ecs::INVALID_ENTITY)
                return false;

            if (auto* name = scene_->names.GetComponent(duplicate_))
                name->name += " Copy";

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
