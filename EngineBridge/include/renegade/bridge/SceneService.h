#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <WickedEngine.h>

namespace renegade::bridge
{
    struct SceneEntity
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity parent = wi::ecs::INVALID_ENTITY;
        std::string name;
        int depth = 0;
        bool hasTransform = false;
    };

    class SceneService
    {
    public:
        void NewScene();
        void CreateProvingGround();
        bool LoadScene(const std::string& filePath);
        bool SaveScene(const std::string& filePath);
        bool ReloadScene();

        [[nodiscard]] wi::scene::Scene& GetScene() noexcept;
        [[nodiscard]] const wi::scene::Scene& GetScene() const noexcept;
        [[nodiscard]] std::size_t EntityCount() const;
        [[nodiscard]] std::vector<SceneEntity> ListEntities() const;
        [[nodiscard]] bool ContainsEntity(wi::ecs::Entity entity) const;
        [[nodiscard]] bool IsHierarchyVisible(
            wi::ecs::Entity entity) const;
        [[nodiscard]] const std::string& CurrentPath() const noexcept;
        [[nodiscard]] const std::string& LastError() const noexcept;

    private:
        wi::scene::Scene scene_;
        std::string currentPath_;
        std::string lastError_;
    };
}
