#include "renegade/bridge/SceneService.h"

#include <algorithm>
#include <unordered_map>

namespace renegade::bridge
{
    void SceneService::NewScene()
    {
        scene_.Clear();
        currentPath_.clear();
        lastError_.clear();
    }

    bool SceneService::LoadScene(const std::string& filePath)
    {
        if (!wi::helper::FileExists(filePath))
        {
            lastError_ = "Scene file does not exist: " + filePath;
            return false;
        }

        wi::scene::Scene candidate;
        wi::scene::LoadModel(candidate, filePath);

        wi::unordered_set<wi::ecs::Entity> entities;
        candidate.FindAllEntities(entities);
        if (entities.empty())
        {
            lastError_ = "Scene loaded no entities: " + filePath;
            return false;
        }

        scene_.Clear();
        scene_.Merge(candidate);
        currentPath_ = filePath;
        lastError_.clear();
        return true;
    }

    bool SceneService::SaveScene(const std::string& filePath)
    {
        if (filePath.empty())
        {
            lastError_ = "A scene path is required.";
            return false;
        }

        wi::Archive archive(filePath, false);
        if (!archive.IsOpen())
        {
            lastError_ = "Could not create scene file: " + filePath;
            return false;
        }

        archive.SetCompressionEnabled(true);
        scene_.Serialize(archive);
        archive.Close();

        if (!wi::helper::FileExists(filePath))
        {
            lastError_ = "Scene file was not written: " + filePath;
            return false;
        }

        currentPath_ = filePath;
        lastError_.clear();
        return true;
    }

    bool SceneService::ReloadScene()
    {
        if (currentPath_.empty())
        {
            lastError_ = "There is no saved scene to reopen.";
            return false;
        }

        const std::string path = currentPath_;
        return LoadScene(path);
    }

    wi::scene::Scene& SceneService::GetScene() noexcept
    {
        return scene_;
    }

    const wi::scene::Scene& SceneService::GetScene() const noexcept
    {
        return scene_;
    }

    std::size_t SceneService::EntityCount() const
    {
        wi::unordered_set<wi::ecs::Entity> entities;
        scene_.FindAllEntities(entities);
        return entities.size();
    }

    std::vector<SceneEntity> SceneService::ListEntities() const
    {
        wi::unordered_set<wi::ecs::Entity> entitySet;
        scene_.FindAllEntities(entitySet);

        std::vector<SceneEntity> entities;
        entities.reserve(entitySet.size());
        for (const auto entity : entitySet)
        {
            SceneEntity item;
            item.entity = entity;
            item.hasTransform = scene_.transforms.Contains(entity);

            if (const auto* hierarchy = scene_.hierarchy.GetComponent(entity))
            {
                item.parent = hierarchy->parentID;
            }

            if (const auto* name = scene_.names.GetComponent(entity);
                name != nullptr && !name->name.empty())
            {
                item.name = name->name;
            }
            else
            {
                item.name = "[entity " + std::to_string(entity) + "]";
            }

            entities.push_back(std::move(item));
        }

        std::sort(
            entities.begin(),
            entities.end(),
            [](const SceneEntity& left, const SceneEntity& right)
            {
                if (left.name == right.name)
                {
                    return left.entity < right.entity;
                }
                return left.name < right.name;
            });

        std::unordered_map<wi::ecs::Entity, std::vector<std::size_t>> children;
        std::vector<std::size_t> roots;
        for (std::size_t index = 0; index < entities.size(); ++index)
        {
            const auto parent = entities[index].parent;
            if (parent == wi::ecs::INVALID_ENTITY || entitySet.count(parent) == 0)
            {
                roots.push_back(index);
            }
            else
            {
                children[parent].push_back(index);
            }
        }

        std::vector<SceneEntity> ordered;
        ordered.reserve(entities.size());
        const auto append = [&](const auto& self, const std::size_t index, const int depth) -> void
        {
            auto item = entities[index];
            item.depth = depth;
            ordered.push_back(std::move(item));

            const auto found = children.find(entities[index].entity);
            if (found == children.end())
            {
                return;
            }
            for (const auto child : found->second)
            {
                self(self, child, depth + 1);
            }
        };

        for (const auto root : roots)
        {
            append(append, root, 0);
        }
        return ordered;
    }

    const std::string& SceneService::CurrentPath() const noexcept
    {
        return currentPath_;
    }

    const std::string& SceneService::LastError() const noexcept
    {
        return lastError_;
    }
}
