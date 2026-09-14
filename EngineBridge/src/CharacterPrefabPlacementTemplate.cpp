#include "renegade/bridge/CharacterPrefabService.h"

#include <vector>

namespace renegade::bridge
{
    bool MarkCharacterPrefabPlacementTemplate(
        wi::scene::Scene& scene,
        const CharacterPrefabDocument& document,
        std::string& error)
    {
        std::string serialized;
        if (!SerializeCharacterPrefab(document, serialized, error))
            return false;

        std::vector<wi::ecs::Entity> roots;
        for (std::size_t index = 0; index < scene.transforms.GetCount(); ++index)
        {
            const wi::ecs::Entity entity = scene.transforms.GetEntity(index);
            const auto* hierarchy = scene.hierarchy.GetComponent(entity);
            if (hierarchy == nullptr || hierarchy->parentID == wi::ecs::INVALID_ENTITY)
                roots.push_back(entity);
        }
        if (roots.empty())
        {
            error = "Prepared Character Prefab base scene has no transform root.";
            return false;
        }

        // Multiple top-level transforms can exist in imported model scenes.
        // Stamp each root so whichever one the reusable placement boundary
        // chooses remains able to recover the portable prefab layer.
        for (const wi::ecs::Entity root : roots)
        {
            auto* metadata = scene.metadatas.GetComponent(root);
            if (metadata == nullptr)
                metadata = &scene.metadatas.Create(root);
            metadata->string_values.set(
                CharacterPrefabPlacementTemplateMetadataKey,
                serialized);
        }
        error.clear();
        return true;
    }

    bool ReadCharacterPrefabPlacementTemplate(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity hierarchyRoot,
        CharacterPrefabDocument& document,
        std::string& error)
    {
        document = {};
        if (hierarchyRoot == wi::ecs::INVALID_ENTITY)
        {
            error.clear();
            return false;
        }

        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            const wi::ecs::Entity entity = scene.metadatas.GetEntity(index);
            if (entity != hierarchyRoot &&
                !scene.Entity_IsDescendant(entity, hierarchyRoot))
            {
                continue;
            }
            const auto& metadata = scene.metadatas[index];
            if (!metadata.string_values.has(
                    CharacterPrefabPlacementTemplateMetadataKey))
            {
                continue;
            }
            const std::string serialized = metadata.string_values.get(
                CharacterPrefabPlacementTemplateMetadataKey);
            if (!DeserializeCharacterPrefab(serialized, document, error))
                return false;
            error.clear();
            return true;
        }
        error.clear();
        return false;
    }
}
