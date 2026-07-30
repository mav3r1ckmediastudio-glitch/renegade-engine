#include "renegade/bridge/SceneService.h"

#include <algorithm>
#include <unordered_map>

namespace
{
    constexpr const char* InternalEntityPrefix = "__renegade_internal_";

    bool IsHierarchyEntryVisible(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
    {
        const auto* name = scene.names.GetComponent(entity);
        return name == nullptr ||
            name->name.rfind(InternalEntityPrefix, 0) != 0;
    }

    void SetTransform(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const XMFLOAT3& translation,
        const XMFLOAT3& scale)
    {
        auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
        {
            return;
        }

        transform->Scale(scale);
        transform->Translate(translation);
        transform->UpdateTransform();
    }

    void SetMaterial(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const XMFLOAT4& baseColor,
        const XMFLOAT3& emissiveColor,
        const float emissiveStrength,
        const float metalness,
        const float roughness)
    {
        auto* material = scene.materials.GetComponent(entity);
        if (material == nullptr)
        {
            return;
        }

        material->SetBaseColor(baseColor);
        material->SetEmissiveColor(
            XMFLOAT4(
                emissiveColor.x,
                emissiveColor.y,
                emissiveColor.z,
                emissiveStrength));
        material->SetMetalness(metalness);
        material->SetRoughness(roughness);
    }
}

namespace renegade::bridge
{
    void SceneService::NewScene()
    {
        scene_.Clear();
        currentPath_.clear();
        lastError_.clear();
    }

    void SceneService::CreateProvingGround()
    {
        NewScene();

        scene_.weather.horizon = XMFLOAT3(0.008f, 0.02f, 0.035f);
        scene_.weather.zenith = XMFLOAT3(0.001f, 0.004f, 0.012f);
        scene_.weather.ambient = XMFLOAT3(0.055f, 0.075f, 0.09f);
        scene_.weather.fogStart = 18.0f;
        scene_.weather.fogDensity = 0.012f;

        const auto ground = scene_.Entity_CreatePlane("Proving Ground");
        SetTransform(
            scene_,
            ground,
            XMFLOAT3(0.0f, -0.03f, 2.0f),
            XMFLOAT3(12.0f, 1.0f, 12.0f));
        SetMaterial(
            scene_,
            ground,
            XMFLOAT4(0.012f, 0.025f, 0.035f, 1.0f),
            XMFLOAT3(0.0f, 0.0f, 0.0f),
            0.0f,
            0.72f,
            0.34f);

        for (int offset = -10; offset <= 10; offset += 2)
        {
            const float coordinate = static_cast<float>(offset);
            const auto lineZ = scene_.Entity_CreateCube(
                "__renegade_internal_grid_z");
            SetTransform(
                scene_,
                lineZ,
                XMFLOAT3(coordinate, 0.002f, 2.0f),
                XMFLOAT3(0.012f, 0.008f, 10.0f));
            SetMaterial(
                scene_,
                lineZ,
                XMFLOAT4(0.0f, 0.18f, 0.24f, 1.0f),
                XMFLOAT3(0.0f, 0.7f, 1.0f),
                1.25f,
                0.35f,
                0.2f);

            const auto lineX = scene_.Entity_CreateCube(
                "__renegade_internal_grid_x");
            SetTransform(
                scene_,
                lineX,
                XMFLOAT3(0.0f, 0.002f, coordinate + 2.0f),
                XMFLOAT3(10.0f, 0.008f, 0.012f));
            SetMaterial(
                scene_,
                lineX,
                XMFLOAT4(0.0f, 0.18f, 0.24f, 1.0f),
                XMFLOAT3(0.0f, 0.7f, 1.0f),
                1.25f,
                0.35f,
                0.2f);
        }

        const auto pedestal = scene_.Entity_CreateCube("Hologram Pedestal");
        SetTransform(
            scene_,
            pedestal,
            XMFLOAT3(0.0f, 0.35f, 2.0f),
            XMFLOAT3(2.4f, 0.35f, 2.4f));
        SetMaterial(
            scene_,
            pedestal,
            XMFLOAT4(0.025f, 0.055f, 0.075f, 1.0f),
            XMFLOAT3(0.0f, 0.22f, 0.32f),
            0.55f,
            0.88f,
            0.18f);

        const auto core = scene_.Entity_CreateSphere("Renegade Hologram Core", 1.2f);
        SetTransform(
            scene_,
            core,
            XMFLOAT3(0.0f, 2.35f, 2.0f),
            XMFLOAT3(1.0f, 1.0f, 1.0f));
        SetMaterial(
            scene_,
            core,
            XMFLOAT4(0.015f, 0.2f, 0.3f, 1.0f),
            XMFLOAT3(0.0f, 0.75f, 1.0f),
            4.0f,
            0.55f,
            0.08f);

        constexpr XMFLOAT3 cyan = XMFLOAT3(0.0f, 0.72f, 1.0f);
        constexpr XMFLOAT3 frameColor = XMFLOAT3(0.01f, 0.08f, 0.11f);
        const auto leftPillar = scene_.Entity_CreateCube("Gateway Left");
        SetTransform(
            scene_,
            leftPillar,
            XMFLOAT3(-5.0f, 3.0f, 5.5f),
            XMFLOAT3(0.32f, 3.0f, 0.32f));
        SetMaterial(
            scene_,
            leftPillar,
            XMFLOAT4(frameColor.x, frameColor.y, frameColor.z, 1.0f),
            cyan,
            1.1f,
            0.85f,
            0.16f);

        const auto rightPillar = scene_.Entity_CreateCube("Gateway Right");
        SetTransform(
            scene_,
            rightPillar,
            XMFLOAT3(5.0f, 3.0f, 5.5f),
            XMFLOAT3(0.32f, 3.0f, 0.32f));
        SetMaterial(
            scene_,
            rightPillar,
            XMFLOAT4(frameColor.x, frameColor.y, frameColor.z, 1.0f),
            cyan,
            1.1f,
            0.85f,
            0.16f);

        const auto gatewayTop = scene_.Entity_CreateCube("Gateway Crown");
        SetTransform(
            scene_,
            gatewayTop,
            XMFLOAT3(0.0f, 6.0f, 5.5f),
            XMFLOAT3(5.32f, 0.32f, 0.32f));
        SetMaterial(
            scene_,
            gatewayTop,
            XMFLOAT4(frameColor.x, frameColor.y, frameColor.z, 1.0f),
            cyan,
            1.1f,
            0.85f,
            0.16f);

        for (const float side : {-1.0f, 1.0f})
        {
            const auto marker = scene_.Entity_CreateCube("Range Marker");
            SetTransform(
                scene_,
                marker,
                XMFLOAT3(side * 7.2f, 1.15f, 0.0f),
                XMFLOAT3(0.55f, 1.15f, 0.55f));
            SetMaterial(
                scene_,
                marker,
                XMFLOAT4(0.11f, 0.04f, 0.015f, 1.0f),
                XMFLOAT3(1.0f, 0.25f, 0.02f),
                1.5f,
                0.7f,
                0.25f);
        }

        const auto keyLight = scene_.Entity_CreateLight(
            "Key Light",
            XMFLOAT3(0.0f, 8.0f, -2.0f),
            XMFLOAT3(0.78f, 0.9f, 1.0f),
            4.5f,
            100.0f,
            wi::scene::LightComponent::DIRECTIONAL);
        if (auto* transform = scene_.transforms.GetComponent(keyLight))
        {
            transform->RotateRollPitchYaw(XMFLOAT3(-0.65f, 0.4f, 0.0f));
            transform->UpdateTransform();
        }
        if (auto* light = scene_.lights.GetComponent(keyLight))
        {
            light->SetCastShadow(true);
        }

        scene_.Entity_CreateLight(
            "Core Light",
            XMFLOAT3(0.0f, 3.0f, 1.0f),
            cyan,
            45.0f,
            12.0f,
            wi::scene::LightComponent::POINT);
        scene_.Entity_CreateLight(
            "Amber Marker Light",
            XMFLOAT3(-7.2f, 2.0f, 0.0f),
            XMFLOAT3(1.0f, 0.22f, 0.03f),
            20.0f,
            6.0f,
            wi::scene::LightComponent::POINT);

        scene_.Update(0.0f);
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

        {
            wi::Archive archive(filePath, false);
            if (!archive.IsOpen())
            {
                lastError_ = "Could not create scene file: " + filePath;
                return false;
            }

            archive.SetCompressionEnabled(true);
            scene_.Update(0.0f);
            scene_.Serialize(archive);

            // Archive writes on destruction. Calling Close() here would make
            // the destructor close the same archive a second time after its
            // data buffer has already been cleared.
        }

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
        wi::unordered_set<wi::ecs::Entity> visibleEntitySet;
        for (const auto entity : entitySet)
        {
            if (IsHierarchyEntryVisible(scene_, entity))
            {
                visibleEntitySet.insert(entity);
            }
        }

        std::vector<SceneEntity> entities;
        entities.reserve(visibleEntitySet.size());
        for (const auto entity : visibleEntitySet)
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
            if (parent == wi::ecs::INVALID_ENTITY ||
                visibleEntitySet.count(parent) == 0)
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

    bool SceneService::ContainsEntity(const wi::ecs::Entity entity) const
    {
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return false;
        }

        wi::unordered_set<wi::ecs::Entity> entities;
        scene_.FindAllEntities(entities);
        return entities.count(entity) != 0;
    }

    bool SceneService::IsHierarchyVisible(
        const wi::ecs::Entity entity) const
    {
        return ContainsEntity(entity) &&
            IsHierarchyEntryVisible(scene_, entity);
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
