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

    // Renderer-independent description of one generated Proving Ground entity.
    //
    // Wicked's primitive factories and MeshComponent::CreateRenderData() both
    // require a live graphics device, so CreateProvingGround() itself cannot
    // run in a headless test process. Keeping the composition as plain data
    // lets RenegadeBridgeTests assert the generated structure - most
    // importantly that no internal helper entities are serialized - without a
    // renderer.
    struct ProvingGroundProp
    {
        enum class Kind
        {
            Environment,      // WeatherComponent carrier: sky, fog and mist
            Terrain,          // generated ground relief mesh
            Block,            // cube primitive
            Sphere,           // sphere primitive
            EnvironmentProbe,
            Light,
        };

        std::string name;
        Kind kind = Kind::Block;

        XMFLOAT3 translation = XMFLOAT3(0.0f, 0.0f, 0.0f);
        // Block: half extents. Sphere: x is the radius. Terrain: x is the
        // half-width of the generated ground.
        XMFLOAT3 halfExtents = XMFLOAT3(1.0f, 1.0f, 1.0f);
        XMFLOAT3 rotationPitchYawRoll = XMFLOAT3(0.0f, 0.0f, 0.0f);

        // Surface (Terrain, Block, Sphere).
        XMFLOAT4 baseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        XMFLOAT3 emissiveColor = XMFLOAT3(0.0f, 0.0f, 0.0f);
        float emissiveStrength = 0.0f;
        float metalness = 0.0f;
        float roughness = 0.5f;

        // Terrain.
        int terrainResolution = 0;
        float terrainFlatHalfWidth = 0.0f;
        float terrainBlendWidth = 1.0f;

        // Light.
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

    // The exact composition CreateProvingGround() instantiates, in order.
    [[nodiscard]] std::vector<ProvingGroundProp> ProvingGroundBlueprint();

    // Footprint of Wicked's renderer-owned grid helper in 3D mode: a 20x20
    // unit line grid centred on the world origin. The generated deck is sized
    // against this so the helper reads as the deck's own measurement grid.
    inline constexpr float GridHelperHalfExtent = 10.0f;

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
