#include "renegade/bridge/SceneService.h"

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

    const std::string& SceneService::CurrentPath() const noexcept
    {
        return currentPath_;
    }

    const std::string& SceneService::LastError() const noexcept
    {
        return lastError_;
    }
}
