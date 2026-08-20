#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <WickedEngine.h>

namespace renegade::bridge
{
    class SceneDocumentService;

    enum class SceneEntityCategory
    {
        Lights,
        Models,
        Characters,
        Cameras,
        Terrain,
        Effects,
        Audio,
        Other,
    };

    struct SceneEntity
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity parent = wi::ecs::INVALID_ENTITY;
        std::string name;
        int depth = 0;
        bool hasTransform = false;
        SceneEntityCategory category = SceneEntityCategory::Other;
    };

    struct ProvingGroundProp
    {
        enum class Kind
        {
            Environment,
            Terrain,
            Block,
            Sphere,
            EnvironmentProbe,
            Light,
        };

        std::string name;
        Kind kind = Kind::Block;
        XMFLOAT3 translation = XMFLOAT3(0.0f, 0.0f, 0.0f);
        XMFLOAT3 halfExtents = XMFLOAT3(1.0f, 1.0f, 1.0f);
        XMFLOAT3 rotationPitchYawRoll = XMFLOAT3(0.0f, 0.0f, 0.0f);
        XMFLOAT4 baseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        XMFLOAT3 emissiveColor = XMFLOAT3(0.0f, 0.0f, 0.0f);
        float emissiveStrength = 0.0f;
        float metalness = 0.0f;
        float roughness = 0.5f;
        int terrainResolution = 0;
        float terrainFlatHalfWidth = 0.0f;
        float terrainBlendWidth = 1.0f;
        wi::scene::LightComponent::LightType lightType =
            wi::scene::LightComponent::POINT;
        XMFLOAT3 lightColor = XMFLOAT3(1.0f, 1.0f, 1.0f);
        float lightIntensity = 0.0f;
        float lightRange = 0.0f;
        float lightOuterConeAngle = 0.0f;
        float lightInnerConeAngle = 0.0f;
        bool castShadow = false;
        bool volumetrics = false;
        float volumetricBoost = 0.0f;
    };

    [[nodiscard]] std::vector<ProvingGroundProp> ProvingGroundBlueprint();
    inline constexpr float GridHelperHalfExtent = 10.0f;

    class SceneService
    {
    public:
        void NewScene();
        void CreateProvingGround();
        bool LoadScene(const std::string& filePath);

        [[nodiscard]] wi::scene::Scene& GetScene() noexcept;
        [[nodiscard]] const wi::scene::Scene& GetScene() const noexcept;
        [[nodiscard]] std::size_t EntityCount() const;
        [[nodiscard]] std::vector<SceneEntity> ListEntities() const;
        [[nodiscard]] bool ContainsEntity(wi::ecs::Entity entity) const;
        [[nodiscard]] wi::ecs::Entity WeatherEntity() const;
        [[nodiscard]] bool IsHierarchyVisible(
            wi::ecs::Entity entity) const;
        [[nodiscard]] const std::string& CurrentPath() const noexcept;
        [[nodiscard]] const std::string& LastError() const noexcept;

        void SetLastError(std::string error)
        {
            lastError_ = std::move(error);
        }

    private:
        friend class SceneDocumentService;

        wi::scene::Scene scene_;
        std::string currentPath_;
        std::string lastError_;
    };
}
