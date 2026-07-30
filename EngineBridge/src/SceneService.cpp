#include "renegade/bridge/SceneService.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace
{
    constexpr const char* InternalEntityPrefix = "__renegade_internal_";

    constexpr XMFLOAT3 Cyan = XMFLOAT3(0.0f, 0.72f, 1.0f);
    constexpr XMFLOAT3 Amber = XMFLOAT3(1.0f, 0.42f, 0.06f);
    constexpr XMFLOAT3 Unlit = XMFLOAT3(0.0f, 0.0f, 0.0f);

    // The deck is deliberately slightly larger than the renderer-owned grid
    // helper so the helper never overhangs the generated ground.
    constexpr float DeckHalfWidth = 11.0f;
    constexpr float DeckThickness = 0.16f;
    constexpr float GatewayZ = 9.0f;

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
        const XMFLOAT3& scale,
        const XMFLOAT3& rotationPitchYawRoll = XMFLOAT3(0.0f, 0.0f, 0.0f))
    {
        auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
        {
            return;
        }

        transform->Scale(scale);
        if (rotationPitchYawRoll.x != 0.0f ||
            rotationPitchYawRoll.y != 0.0f ||
            rotationPitchYawRoll.z != 0.0f)
        {
            transform->RotateRollPitchYaw(rotationPitchYawRoll);
        }
        transform->Translate(translation);
        transform->UpdateTransform();
    }

    void SetMaterial(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const renegade::bridge::ProvingGroundProp& prop)
    {
        auto* material = scene.materials.GetComponent(entity);
        if (material == nullptr)
        {
            return;
        }

        material->SetBaseColor(prop.baseColor);
        material->SetEmissiveColor(
            XMFLOAT4(
                prop.emissiveColor.x,
                prop.emissiveColor.y,
                prop.emissiveColor.z,
                prop.emissiveStrength));
        material->SetMetalness(prop.metalness);
        material->SetRoughness(prop.roughness);
        material->SetCastShadow(true);
        material->SetReceiveShadow(true);
    }

    // Deterministic, layered relief. No random source is used, so every
    // generated project produces byte-identical terrain geometry.
    float TerrainHeight(const float x, const float z)
    {
        const float broad =
            0.95f * std::sin(x * 0.043f) * std::cos(z * 0.037f);
        const float medium =
            0.42f * std::sin(x * 0.121f + 1.7f) * std::cos(z * 0.104f - 0.9f);
        const float fine =
            0.16f * std::sin((x + z) * 0.263f + 0.4f);
        return broad + medium + fine;
    }

    // Builds one ground mesh whose relief flattens to zero across the central
    // deck footprint. This is a generated ground treatment, not a terrain
    // authoring system: there is no editing surface, no chunking, no virtual
    // texturing and no streaming.
    wi::ecs::Entity CreateGroundRelief(
        wi::scene::Scene& scene,
        const renegade::bridge::ProvingGroundProp& prop)
    {
        const float extent = prop.halfExtents.x;
        const int resolution = std::max(1, prop.terrainResolution);
        const float flatHalfWidth = prop.terrainFlatHalfWidth;
        const float blendWidth = std::max(0.001f, prop.terrainBlendWidth);
        const float baseHeight = prop.translation.y;

        const int vertexSide = resolution + 1;
        const float step = (extent * 2.0f) / static_cast<float>(resolution);

        wi::vector<XMFLOAT3> positions;
        positions.reserve(
            static_cast<std::size_t>(vertexSide) *
            static_cast<std::size_t>(vertexSide));

        for (int row = 0; row < vertexSide; ++row)
        {
            const float z = -extent + step * static_cast<float>(row);
            for (int column = 0; column < vertexSide; ++column)
            {
                const float x = -extent + step * static_cast<float>(column);

                float mask =
                    (std::max(std::abs(x), std::abs(z)) - flatHalfWidth) /
                    blendWidth;
                mask = std::clamp(mask, 0.0f, 1.0f);
                mask = mask * mask * (3.0f - 2.0f * mask); // smoothstep

                positions.push_back(
                    XMFLOAT3(x, baseHeight + TerrainHeight(x, z) * mask, z));
            }
        }

        wi::vector<std::uint32_t> indices;
        indices.reserve(
            static_cast<std::size_t>(resolution) *
            static_cast<std::size_t>(resolution) * 6u);

        for (int row = 0; row < resolution; ++row)
        {
            for (int column = 0; column < resolution; ++column)
            {
                const auto topLeft =
                    static_cast<std::uint32_t>(row * vertexSide + column);
                const auto topRight = topLeft + 1u;
                const auto bottomLeft =
                    static_cast<std::uint32_t>(topLeft + vertexSide);
                const auto bottomRight = bottomLeft + 1u;

                // Wicked winds front faces so that
                // (v1 - v0) x (v2 - v0) points away from the visible side.
                // Entity_CreatePlane uses this order for its +Y quad.
                indices.push_back(topLeft);
                indices.push_back(topRight);
                indices.push_back(bottomLeft);

                indices.push_back(topRight);
                indices.push_back(bottomRight);
                indices.push_back(bottomLeft);
            }
        }

        return scene.Entity_CreateMeshFromData(
            prop.name,
            indices.size(),
            indices.data(),
            positions.size(),
            positions.data());
    }

    renegade::bridge::ProvingGroundProp MakeBlock(
        std::string name,
        const XMFLOAT3& translation,
        const XMFLOAT3& halfExtents,
        const XMFLOAT4& baseColor,
        const float metalness,
        const float roughness,
        const XMFLOAT3& emissiveColor = Unlit,
        const float emissiveStrength = 0.0f)
    {
        renegade::bridge::ProvingGroundProp prop;
        prop.name = std::move(name);
        prop.kind = renegade::bridge::ProvingGroundProp::Kind::Block;
        prop.translation = translation;
        prop.halfExtents = halfExtents;
        prop.baseColor = baseColor;
        prop.metalness = metalness;
        prop.roughness = roughness;
        prop.emissiveColor = emissiveColor;
        prop.emissiveStrength = emissiveStrength;
        return prop;
    }
}

namespace renegade::bridge
{
    std::vector<ProvingGroundProp> ProvingGroundBlueprint()
    {
        std::vector<ProvingGroundProp> props;
        props.reserve(40);

        // -- Environment -----------------------------------------------------
        {
            ProvingGroundProp environment;
            environment.name = "Environment";
            environment.kind = ProvingGroundProp::Kind::Environment;
            props.push_back(std::move(environment));
        }

        // -- Ground ----------------------------------------------------------
        {
            ProvingGroundProp terrain;
            terrain.name = "Proving Ground Terrain";
            terrain.kind = ProvingGroundProp::Kind::Terrain;
            terrain.translation = XMFLOAT3(0.0f, -DeckThickness * 2.0f, 0.0f);
            terrain.halfExtents = XMFLOAT3(90.0f, 0.0f, 90.0f);
            terrain.terrainResolution = 90;
            terrain.terrainFlatHalfWidth = 13.0f;
            terrain.terrainBlendWidth = 18.0f;
            terrain.baseColor = XMFLOAT4(0.030f, 0.034f, 0.039f, 1.0f);
            terrain.metalness = 0.02f;
            terrain.roughness = 0.92f;
            props.push_back(std::move(terrain));
        }

        props.push_back(MakeBlock(
            "Proving Ground Deck",
            XMFLOAT3(0.0f, -DeckThickness, 0.0f),
            XMFLOAT3(DeckHalfWidth, DeckThickness, DeckHalfWidth),
            XMFLOAT4(0.020f, 0.026f, 0.032f, 1.0f),
            0.15f,
            0.52f));

        // Projected deck edge. Ice-blue is the approved interaction colour and
        // is emissive geometry rather than a post effect, so it also survives
        // into the standalone runtime.
        const XMFLOAT3 deckEdges[4] = {
            XMFLOAT3(0.0f, 0.0f, DeckHalfWidth),
            XMFLOAT3(0.0f, 0.0f, -DeckHalfWidth),
            XMFLOAT3(DeckHalfWidth, 0.0f, 0.0f),
            XMFLOAT3(-DeckHalfWidth, 0.0f, 0.0f),
        };
        // Named rather than numbered: the hierarchy is a creator surface, and
        // four identical "Deck Edge" rows cannot be told apart.
        const char* const deckEdgeNames[4] = {
            "Deck Edge North",
            "Deck Edge South",
            "Deck Edge East",
            "Deck Edge West",
        };
        for (int index = 0; index < 4; ++index)
        {
            const bool alongX = index < 2;
            props.push_back(MakeBlock(
                deckEdgeNames[index],
                deckEdges[index],
                alongX
                    ? XMFLOAT3(DeckHalfWidth + 0.3f, 0.10f, 0.3f)
                    : XMFLOAT3(0.3f, 0.10f, DeckHalfWidth + 0.3f),
                XMFLOAT4(0.010f, 0.030f, 0.040f, 1.0f),
                0.25f,
                0.30f,
                Cyan,
                0.85f));
        }

        // -- Centre stage ----------------------------------------------------
        props.push_back(MakeBlock(
            "Alignment Pedestal",
            XMFLOAT3(0.0f, 0.35f, 0.0f),
            XMFLOAT3(2.4f, 0.35f, 2.4f),
            XMFLOAT4(0.025f, 0.055f, 0.075f, 1.0f),
            0.80f,
            0.28f,
            Cyan,
            0.40f));

        {
            ProvingGroundProp core;
            core.name = "Renegade Hologram Core";
            core.kind = ProvingGroundProp::Kind::Sphere;
            core.translation = XMFLOAT3(0.0f, 2.55f, 0.0f);
            core.halfExtents = XMFLOAT3(1.2f, 1.2f, 1.2f);
            core.baseColor = XMFLOAT4(0.015f, 0.200f, 0.300f, 1.0f);
            core.emissiveColor = Cyan;
            core.emissiveStrength = 4.0f;
            core.metalness = 0.0f;
            core.roughness = 0.10f;
            props.push_back(std::move(core));
        }

        // -- Gateway ---------------------------------------------------------
        const XMFLOAT4 gatewayColor = XMFLOAT4(0.014f, 0.030f, 0.038f, 1.0f);
        props.push_back(MakeBlock(
            "Gateway Left",
            XMFLOAT3(-5.0f, 3.0f, GatewayZ),
            XMFLOAT3(0.32f, 3.0f, 0.32f),
            gatewayColor,
            0.85f,
            0.22f,
            Cyan,
            0.95f));
        props.push_back(MakeBlock(
            "Gateway Right",
            XMFLOAT3(5.0f, 3.0f, GatewayZ),
            XMFLOAT3(0.32f, 3.0f, 0.32f),
            gatewayColor,
            0.85f,
            0.22f,
            Cyan,
            0.95f));
        props.push_back(MakeBlock(
            "Gateway Crown",
            XMFLOAT3(0.0f, 6.0f, GatewayZ),
            XMFLOAT3(5.32f, 0.32f, 0.32f),
            gatewayColor,
            0.85f,
            0.22f,
            Cyan,
            0.95f));

        // -- Range markers ---------------------------------------------------
        for (const float side : {-1.0f, 1.0f})
        {
            props.push_back(MakeBlock(
                side < 0.0f ? "Range Marker West" : "Range Marker East",
                XMFLOAT3(side * 7.2f, 1.15f, 0.0f),
                XMFLOAT3(0.55f, 1.15f, 0.55f),
                XMFLOAT4(0.110f, 0.045f, 0.018f, 1.0f),
                0.55f,
                0.35f,
                Amber,
                1.40f));
        }

        // -- Perimeter -------------------------------------------------------
        // Eight pylons just outside the deck enclose the viewport and give the
        // low sun something to throw long shadows from.
        constexpr float PylonRadius = 15.5f;
        constexpr float PylonDiagonal = PylonRadius * 0.7071f;
        const XMFLOAT2 pylonGround[8] = {
            XMFLOAT2(PylonRadius, 0.0f),
            XMFLOAT2(-PylonRadius, 0.0f),
            XMFLOAT2(0.0f, PylonRadius),
            XMFLOAT2(0.0f, -PylonRadius),
            XMFLOAT2(PylonDiagonal, PylonDiagonal),
            XMFLOAT2(-PylonDiagonal, PylonDiagonal),
            XMFLOAT2(PylonDiagonal, -PylonDiagonal),
            XMFLOAT2(-PylonDiagonal, -PylonDiagonal),
        };
        for (int index = 0; index < 8; ++index)
        {
            const float height = (index % 2 == 0) ? 5.4f : 4.1f;
            props.push_back(MakeBlock(
                "Perimeter Pylon " + std::to_string(index + 1),
                XMFLOAT3(pylonGround[index].x, height, pylonGround[index].y),
                XMFLOAT3(0.26f, height, 0.26f),
                XMFLOAT4(0.018f, 0.024f, 0.030f, 1.0f),
                0.70f,
                0.38f,
                Cyan,
                0.20f));
        }

        // -- Retaining terrace -------------------------------------------------
        for (int step = 0; step < 3; ++step)
        {
            const float depth = 19.0f + static_cast<float>(step) * 2.6f;
            const float height = 1.1f + static_cast<float>(step) * 0.9f;
            props.push_back(MakeBlock(
                "Retaining Terrace " + std::to_string(step + 1),
                XMFLOAT3(0.0f, height - 0.4f, -depth),
                XMFLOAT3(24.0f - static_cast<float>(step) * 1.5f, height, 1.3f),
                XMFLOAT4(0.026f, 0.028f, 0.031f, 1.0f),
                0.05f,
                0.85f));
        }

        // -- Equipment cluster --------------------------------------------------
        const XMFLOAT4 crates[4] = {
            XMFLOAT4(7.4f, 0.85f, -6.6f, 0.85f),
            XMFLOAT4(8.9f, 0.55f, -5.2f, 0.55f),
            XMFLOAT4(6.2f, 0.45f, -8.1f, 0.45f),
            XMFLOAT4(7.6f, 2.15f, -6.6f, 0.45f),
        };
        for (int index = 0; index < 4; ++index)
        {
            const auto& crate = crates[index];
            props.push_back(MakeBlock(
                "Equipment Crate " + std::to_string(index + 1),
                XMFLOAT3(crate.x, crate.y, crate.z),
                XMFLOAT3(crate.w, crate.w, crate.w),
                XMFLOAT4(0.035f, 0.038f, 0.042f, 1.0f),
                0.20f,
                0.62f));
        }

        // -- Background depth ---------------------------------------------------
        // Large, distant, deliberately plain masses. Their job is to give the
        // fog and aerial perspective something to fall off against.
        const XMFLOAT4 distantMasses[5] = {
            XMFLOAT4(-46.0f, 9.0f, 38.0f, 7.0f),
            XMFLOAT4(52.0f, 12.0f, 44.0f, 9.0f),
            XMFLOAT4(-8.0f, 7.0f, 68.0f, 11.0f),
            XMFLOAT4(70.0f, 6.0f, -18.0f, 8.0f),
            XMFLOAT4(-62.0f, 8.0f, -34.0f, 10.0f),
        };
        for (int index = 0; index < 5; ++index)
        {
            const auto& mass = distantMasses[index];
            props.push_back(MakeBlock(
                "Distant Structure " + std::to_string(index + 1),
                XMFLOAT3(mass.x, mass.y, mass.z),
                XMFLOAT3(mass.w, mass.y, mass.w * 0.7f),
                XMFLOAT4(0.024f, 0.027f, 0.032f, 1.0f),
                0.10f,
                0.75f));
        }

        {
            ProvingGroundProp probe;
            probe.name = "Environment Probe";
            probe.kind = ProvingGroundProp::Kind::EnvironmentProbe;
            probe.translation = XMFLOAT3(0.0f, 3.5f, 0.0f);
            props.push_back(std::move(probe));
        }

        // -- Lighting -----------------------------------------------------------
        {
            ProvingGroundProp sun;
            sun.name = "Sun";
            sun.kind = ProvingGroundProp::Kind::Light;
            sun.lightType = wi::scene::LightComponent::DIRECTIONAL;
            sun.translation = XMFLOAT3(0.0f, 24.0f, -6.0f);
            // A light's direction is its local +Y axis, so identity points
            // straight down. This pitch keeps the sun low enough to throw long
            // shadows across the deck and to drive the volumetrics.
            sun.rotationPitchYawRoll = XMFLOAT3(-1.0f, 0.75f, 0.0f);
            sun.lightColor = XMFLOAT3(1.0f, 0.84f, 0.68f);
            sun.lightIntensity = 5.5f;
            sun.lightRange = 1000.0f;
            sun.castShadow = true;
            sun.volumetrics = true;
            sun.volumetricBoost = 0.55f;
            props.push_back(std::move(sun));
        }

        {
            ProvingGroundProp coreLight;
            coreLight.name = "Core Light";
            coreLight.kind = ProvingGroundProp::Kind::Light;
            coreLight.lightType = wi::scene::LightComponent::POINT;
            coreLight.translation = XMFLOAT3(0.0f, 2.55f, 0.0f);
            coreLight.lightColor = Cyan;
            coreLight.lightIntensity = 60.0f;
            coreLight.lightRange = 15.0f;
            coreLight.volumetrics = true;
            coreLight.volumetricBoost = 1.30f;
            props.push_back(std::move(coreLight));
        }

        {
            ProvingGroundProp beam;
            beam.name = "Gateway Beam";
            beam.kind = ProvingGroundProp::Kind::Light;
            beam.lightType = wi::scene::LightComponent::SPOT;
            beam.translation = XMFLOAT3(0.0f, 5.6f, GatewayZ);
            beam.lightColor = XMFLOAT3(0.35f, 0.85f, 1.0f);
            beam.lightIntensity = 160.0f;
            beam.lightRange = 18.0f;
            beam.lightOuterConeAngle = 0.62f;
            beam.lightInnerConeAngle = 0.18f;
            beam.castShadow = true;
            beam.volumetrics = true;
            beam.volumetricBoost = 1.60f;
            props.push_back(std::move(beam));
        }

        for (const float side : {-1.0f, 1.0f})
        {
            ProvingGroundProp marker;
            marker.name = side < 0.0f
                ? "Marker Light West"
                : "Marker Light East";
            marker.kind = ProvingGroundProp::Kind::Light;
            marker.lightType = wi::scene::LightComponent::POINT;
            marker.translation = XMFLOAT3(side * 7.2f, 2.1f, 0.0f);
            marker.lightColor = Amber;
            marker.lightIntensity = 26.0f;
            marker.lightRange = 8.0f;
            marker.volumetrics = true;
            marker.volumetricBoost = 0.7f;
            props.push_back(std::move(marker));
        }

        {
            ProvingGroundProp fill;
            fill.name = "Deck Fill Light";
            fill.kind = ProvingGroundProp::Kind::Light;
            fill.lightType = wi::scene::LightComponent::POINT;
            fill.translation = XMFLOAT3(-8.0f, 5.0f, -7.0f);
            fill.lightColor = XMFLOAT3(0.45f, 0.62f, 0.80f);
            fill.lightIntensity = 34.0f;
            fill.lightRange = 26.0f;
            props.push_back(std::move(fill));
        }

        return props;
    }

    void SceneService::NewScene()
    {
        scene_.Clear();
        currentPath_.clear();
        lastError_.clear();
    }

    void SceneService::CreateProvingGround()
    {
        NewScene();

        for (const auto& prop : ProvingGroundBlueprint())
        {
            switch (prop.kind)
            {
            case ProvingGroundProp::Kind::Environment:
            {
                // Weather lives on a real entity so the sky, fog and mist
                // survive save and reopen. Scene::weather is only a resolved
                // runtime copy of weathers[0] and is not serialized by itself.
                const auto entity = wi::ecs::CreateEntity();
                scene_.names.Create(entity) = prop.name;
                auto& weather = scene_.weathers.Create(entity);
                weather.SetRealisticSky(true);
                weather.SetRealisticSkyAerialPerspective(true);
                weather.SetRealisticSkyReceiveShadow(true);
                weather.SetHeightFog(true);
                weather.skyExposure = 0.85f;
                weather.ambient = XMFLOAT3(0.034f, 0.046f, 0.060f);
                weather.horizon = XMFLOAT3(0.012f, 0.028f, 0.044f);
                weather.zenith = XMFLOAT3(0.002f, 0.006f, 0.016f);
                weather.fogStart = 14.0f;
                weather.fogDensity = 0.018f;
                weather.fogHeightStart = -0.8f;
                weather.fogHeightEnd = 3.4f;
                weather.windDirection = XMFLOAT3(0.05f, 0.0f, 0.02f);
                weather.windSpeed = 0.5f;
                weather.stars = 0.0f;
                scene_.weather = weather;
                break;
            }
            case ProvingGroundProp::Kind::Terrain:
            {
                const auto entity = CreateGroundRelief(scene_, prop);
                SetMaterial(scene_, entity, prop);
                break;
            }
            case ProvingGroundProp::Kind::Block:
            {
                const auto entity = scene_.Entity_CreateCube(prop.name);
                SetTransform(
                    scene_,
                    entity,
                    prop.translation,
                    prop.halfExtents,
                    prop.rotationPitchYawRoll);
                SetMaterial(scene_, entity, prop);
                break;
            }
            case ProvingGroundProp::Kind::Sphere:
            {
                const auto entity =
                    scene_.Entity_CreateSphere(prop.name, prop.halfExtents.x);
                SetTransform(
                    scene_,
                    entity,
                    prop.translation,
                    XMFLOAT3(1.0f, 1.0f, 1.0f),
                    prop.rotationPitchYawRoll);
                SetMaterial(scene_, entity, prop);
                break;
            }
            case ProvingGroundProp::Kind::EnvironmentProbe:
            {
                scene_.Entity_CreateEnvironmentProbe(
                    prop.name,
                    prop.translation);
                break;
            }
            case ProvingGroundProp::Kind::Light:
            {
                const auto entity = scene_.Entity_CreateLight(
                    prop.name,
                    prop.translation,
                    prop.lightColor,
                    prop.lightIntensity,
                    prop.lightRange,
                    prop.lightType,
                    prop.lightOuterConeAngle,
                    prop.lightInnerConeAngle);

                if (auto* transform = scene_.transforms.GetComponent(entity);
                    transform != nullptr &&
                    (prop.rotationPitchYawRoll.x != 0.0f ||
                        prop.rotationPitchYawRoll.y != 0.0f ||
                        prop.rotationPitchYawRoll.z != 0.0f))
                {
                    transform->RotateRollPitchYaw(prop.rotationPitchYawRoll);
                    transform->UpdateTransform();
                }

                if (auto* light = scene_.lights.GetComponent(entity))
                {
                    light->SetCastShadow(prop.castShadow);
                    light->SetVolumetricsEnabled(prop.volumetrics);
                    light->volumetric_boost = prop.volumetricBoost;
                    if (prop.lightType ==
                        wi::scene::LightComponent::DIRECTIONAL)
                    {
                        light->cascade_distances =
                            wi::vector<float>{ 14.0f, 55.0f, 180.0f };
                    }
                }
                break;
            }
            }
        }

        // Scene::Update() is renderer-dependent, and the render-capable Studio
        // frame loop owns scene advancement. Nothing here calls it.
        lastError_.clear();
    }

    bool SceneService::LoadScene(const std::string& filePath)
    {
        if (!wi::helper::FileExists(filePath))
        {
            lastError_ = "Scene file does not exist: " + filePath;
            return false;
        }

        // Deserialize the archive directly rather than through
        // wi::scene::LoadModel. LoadModel is an import path: it reparents every
        // unparented transform under a temporary root and calls the
        // renderer-dependent Scene::Update() before detaching again. Reading
        // the archive with Scene::Serialize is the exact inverse of SaveScene
        // and preserves the authored hierarchy.
        wi::scene::Scene candidate;
        {
            wi::Archive archive(filePath, true);
            if (!archive.IsOpen())
            {
                lastError_ = "Could not open scene file: " + filePath;
                return false;
            }

            candidate.Serialize(archive);
        }

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

            // Serialization writes local transforms only, and
            // TransformComponent recomputes its world matrix on read. Calling
            // the renderer-dependent Scene::Update() here would break headless
            // callers, and the Studio frame loop already owns advancement.
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
