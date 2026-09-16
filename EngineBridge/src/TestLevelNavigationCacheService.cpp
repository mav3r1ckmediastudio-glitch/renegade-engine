#include "renegade/bridge/TestLevelNavigationCacheService.h"

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/NavigationService.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <exception>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    constexpr std::uint32_t CacheFormatVersion = 1;
    constexpr const char* CacheMagic = "RENEGADE_TEST_LEVEL_NAV_CACHE";
    constexpr std::uint64_t FingerprintSeed = 1469598103934665603ull;
    constexpr std::uint64_t FingerprintPrime = 1099511628211ull;

    struct PreparedNavigationGeometry
    {
        wi::primitive::AABB bounds;
        bool hasBounds = false;
        std::uint64_t fingerprint = FingerprintSeed;
        std::size_t navigationObjects = 0;
        std::size_t cpuColliders = 0;
    };

    void HashBytes(
        std::uint64_t& hash,
        const void* bytes,
        const std::size_t count) noexcept
    {
        const auto* data = static_cast<const unsigned char*>(bytes);
        for (std::size_t index = 0; index < count; ++index)
        {
            hash ^= data[index];
            hash *= FingerprintPrime;
        }
    }

    template<typename Value>
    void HashValue(std::uint64_t& hash, const Value& value) noexcept
    {
        HashBytes(hash, &value, sizeof(value));
    }

    void HashString(
        std::uint64_t& hash,
        const std::string& value) noexcept
    {
        HashBytes(hash, value.data(), value.size());
        const unsigned char terminator = 0;
        HashBytes(hash, &terminator, 1);
    }

    std::string HexHash(const std::uint64_t hash)
    {
        std::ostringstream text;
        text << std::hex << std::setfill('0') << std::setw(16) << hash;
        return text.str();
    }

    std::uint64_t HashText(const std::string& text) noexcept
    {
        std::uint64_t hash = FingerprintSeed;
        HashString(hash, text);
        return hash;
    }

    bool ResolveHierarchyWorldMatrix(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        XMMATRIX& world,
        wi::unordered_set<wi::ecs::Entity>& visiting) noexcept
    {
        const auto* transform = scene.transforms.GetComponent(entity);
        world = transform != nullptr
            ? transform->GetLocalMatrix()
            : XMMatrixIdentity();

        const auto* hierarchy = scene.hierarchy.GetComponent(entity);
        if (hierarchy == nullptr ||
            hierarchy->parentID == wi::ecs::INVALID_ENTITY)
        {
            return true;
        }
        if (!visiting.insert(entity).second)
            return false;

        XMMATRIX parentWorld;
        if (!ResolveHierarchyWorldMatrix(
                scene, hierarchy->parentID, parentWorld, visiting))
        {
            visiting.erase(entity);
            return false;
        }
        visiting.erase(entity);
        world = world * parentWorld;
        return true;
    }

    std::uint32_t EffectiveLayerMask(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept
    {
        std::uint32_t mask = ~0u;
        wi::unordered_set<wi::ecs::Entity> visiting;
        while (entity != wi::ecs::INVALID_ENTITY)
        {
            if (!visiting.insert(entity).second)
                break;

            if (const auto* layer = scene.layers.GetComponent(entity);
                layer != nullptr)
            {
                mask &= layer->layerMask;
            }

            const auto* hierarchy = scene.hierarchy.GetComponent(entity);
            if (hierarchy == nullptr ||
                hierarchy->parentID == wi::ecs::INVALID_ENTITY ||
                hierarchy->parentID == entity)
            {
                break;
            }
            entity = hierarchy->parentID;
        }
        return mask;
    }

    void MergeBounds(
        PreparedNavigationGeometry& geometry,
        const wi::primitive::AABB& bounds)
    {
        if (!bounds.IsValid())
            return;
        if (!geometry.hasBounds)
        {
            geometry.bounds = bounds;
            geometry.hasBounds = true;
        }
        else
        {
            geometry.bounds =
                wi::primitive::AABB::Merge(geometry.bounds, bounds);
        }
    }

    void HashMesh(
        std::uint64_t& fingerprint,
        const wi::scene::MeshComponent& mesh)
    {
        HashValue(fingerprint, mesh.vertex_positions.size());
        for (const auto& position : mesh.vertex_positions)
            HashValue(fingerprint, position);

        HashValue(fingerprint, mesh.indices.size());
        for (const auto index : mesh.indices)
            HashValue(fingerprint, index);

        HashValue(fingerprint, mesh.subsets.size());
        HashValue(fingerprint, mesh.subsets_per_lod);
        for (const auto& subset : mesh.subsets)
        {
            HashValue(fingerprint, subset.indexOffset);
            HashValue(fingerprint, subset.indexCount);
        }
    }

    bool PrepareGeometryStreams(
        wi::scene::Scene& scene,
        PreparedNavigationGeometry& geometry,
        std::string& error)
    {
        geometry = {};
        (void)renegade::bridge::PrepareRigidBodyNavigationGeometry(scene);

        scene.matrix_objects.resize(scene.objects.GetCount());
        scene.matrix_objects_prev.resize(scene.objects.GetCount());
        scene.aabb_objects.resize(scene.objects.GetCount());

        for (std::size_t objectIndex = 0;
            objectIndex < scene.objects.GetCount(); ++objectIndex)
        {
            const wi::ecs::Entity entity =
                scene.objects.GetEntity(objectIndex);
            const auto& object = scene.objects[objectIndex];

            wi::unordered_set<wi::ecs::Entity> visiting;
            XMMATRIX world;
            if (!ResolveHierarchyWorldMatrix(
                    scene, entity, world, visiting))
            {
                error =
                    "Navigation signature found a cyclic transform hierarchy.";
                return false;
            }

            XMFLOAT4X4 worldFloat;
            XMStoreFloat4x4(&worldFloat, world);
            scene.matrix_objects[objectIndex] = worldFloat;
            scene.matrix_objects_prev[objectIndex] = worldFloat;

            wi::primitive::AABB objectBounds;
            if (const auto* mesh =
                    scene.meshes.GetComponent(object.meshID);
                mesh != nullptr && !mesh->vertex_positions.empty())
            {
                for (const auto& position : mesh->vertex_positions)
                {
                    objectBounds.AddPoint(
                        XMVector3TransformCoord(
                            XMLoadFloat3(&position), world));
                }
            }
            objectBounds.layerMask = EffectiveLayerMask(scene, entity);
            objectBounds.userdata =
                static_cast<std::uint32_t>(objectIndex);
            scene.aabb_objects[objectIndex] = objectBounds;

            if ((object.filterMask &
                    wi::enums::FILTER_NAVIGATION_MESH) == 0u ||
                !objectBounds.IsValid())
            {
                continue;
            }

            ++geometry.navigationObjects;
            const std::uint32_t marker = 0x4f424a31u; // OBJ1
            HashValue(geometry.fingerprint, marker);
            HashValue(geometry.fingerprint, object.filterMask);
            HashValue(geometry.fingerprint, objectBounds.layerMask);
            HashValue(geometry.fingerprint, worldFloat);

            const auto* mesh =
                scene.meshes.GetComponent(object.meshID);
            if (mesh != nullptr)
                HashMesh(geometry.fingerprint, *mesh);
            MergeBounds(geometry, objectBounds);
        }

        for (std::size_t colliderIndex = 0;
            colliderIndex < scene.colliders.GetCount(); ++colliderIndex)
        {
            const wi::ecs::Entity entity =
                scene.colliders.GetEntity(colliderIndex);
            const auto& collider = scene.colliders[colliderIndex];
            if (!collider.IsCPUEnabled())
                continue;

            wi::unordered_set<wi::ecs::Entity> visiting;
            XMMATRIX world;
            if (!ResolveHierarchyWorldMatrix(
                    scene, entity, world, visiting))
            {
                error =
                    "Navigation signature found a cyclic collider hierarchy.";
                return false;
            }

            XMFLOAT4X4 worldFloat;
            XMStoreFloat4x4(&worldFloat, world);

            const std::uint32_t layerMask =
                EffectiveLayerMask(scene, entity);
            ++geometry.cpuColliders;
            const std::uint32_t marker = 0x434f4c31u; // COL1
            HashValue(geometry.fingerprint, marker);
            HashValue(geometry.fingerprint, collider._flags);
            const auto shape =
                static_cast<std::uint32_t>(collider.shape);
            HashValue(geometry.fingerprint, shape);
            HashValue(geometry.fingerprint, collider.radius);
            HashValue(geometry.fingerprint, collider.offset);
            HashValue(geometry.fingerprint, collider.tail);
            HashValue(geometry.fingerprint, layerMask);
            HashValue(geometry.fingerprint, worldFloat);

            const float scaleX =
                XMVectorGetX(XMVector3Length(world.r[0]));
            const float scaleY =
                XMVectorGetX(XMVector3Length(world.r[1]));
            const float scaleZ =
                XMVectorGetX(XMVector3Length(world.r[2]));
            const float maximumScale =
                std::max(scaleX, std::max(scaleY, scaleZ));

            XMVECTOR offset =
                XMVector3Transform(
                    XMLoadFloat3(&collider.offset), world);
            XMVECTOR tail =
                XMVector3Transform(
                    XMLoadFloat3(&collider.tail), world);

            if (collider.shape ==
                wi::scene::ColliderComponent::Shape::Sphere)
            {
                wi::primitive::Sphere sphere;
                XMStoreFloat3(&sphere.center, offset);
                sphere.radius =
                    collider.radius *
                    maximumScale;
                wi::primitive::AABB bounds;
                bounds.createFromHalfWidth(
                    sphere.center,
                    XMFLOAT3(
                        sphere.radius,
                        sphere.radius,
                        sphere.radius));
                bounds.layerMask = layerMask;
                MergeBounds(geometry, bounds);
            }
            else if (collider.shape ==
                wi::scene::ColliderComponent::Shape::Capsule)
            {
                const float radius =
                    collider.radius *
                    maximumScale;
                XMVECTOR direction = offset - tail;
                if (XMVectorGetX(XMVector3LengthSq(direction)) < 0.000001f)
                    direction = XMVectorSet(0, 1, 0, 0);
                else
                    direction = XMVector3Normalize(direction);
                offset += direction * radius;
                tail -= direction * radius;

                wi::primitive::Capsule capsule;
                XMStoreFloat3(&capsule.base, offset);
                XMStoreFloat3(&capsule.tip, tail);
                capsule.radius = radius;
                auto bounds = capsule.getAABB();
                bounds.layerMask = layerMask;
                MergeBounds(geometry, bounds);
            }
            else
            {
                wi::primitive::AABB bounds;
                bounds.createFromHalfWidth(
                    XMFLOAT3(0, 0, 0),
                    XMFLOAT3(1, 1, 1));
                XMMATRIX plane =
                    XMMatrixScaling(
                        collider.radius, 1.0f, collider.radius);
                plane =
                    plane *
                    XMMatrixTranslationFromVector(
                        XMLoadFloat3(&collider.offset));
                plane = plane * world;
                bounds = bounds.transform(plane);
                bounds.layerMask = layerMask;
                MergeBounds(geometry, bounds);
            }
        }

        HashValue(
            geometry.fingerprint,
            geometry.navigationObjects);
        HashValue(
            geometry.fingerprint,
            geometry.cpuColliders);

        if (geometry.hasBounds)
            scene.bounds = geometry.bounds;

        error.clear();
        return true;
    }

    void SyncGridTransform(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const wi::VoxelGrid& grid) noexcept
    {
        auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
            return;
        transform->translation_local = grid.center;
        transform->scale_local = grid.voxelSize;
        transform->SetDirty();
        transform->UpdateTransform();
    }

    wi::ecs::Entity CreateCachedGridEntity(
        wi::scene::Scene& scene,
        const wi::VoxelGrid& grid,
        std::string& error)
    {
        using namespace renegade::bridge;

        const wi::ecs::Entity entity = wi::ecs::CreateEntity();
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            error =
                "Wicked could not allocate the automatic TestGame navigation grid.";
            return wi::ecs::INVALID_ENTITY;
        }

        scene.names.Create(entity) = "Automatic Navigation Grid";
        scene.transforms.Create(entity);
        scene.voxel_grids.Create(entity) = grid;
        auto& metadata = scene.metadatas.Create(entity);
        metadata.string_values.set(
            NavigationGridMetadataKey,
            NavigationGridMetadataVersion);

        if (!AssignPersistentEntityId(
                scene, entity, GenerateStableId(), error))
        {
            scene.Entity_Remove(entity);
            return wi::ecs::INVALID_ENTITY;
        }

        SyncGridTransform(scene, entity, grid);
        error.clear();
        return entity;
    }

    bool ApplyCachedGrid(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const wi::VoxelGrid& grid,
        std::string& error)
    {
        auto* target = scene.voxel_grids.GetComponent(entity);
        if (target == nullptr)
        {
            error =
                "TestGame navigation grid lost its native Wicked VoxelGrid component.";
            return false;
        }
        *target = grid;
        SyncGridTransform(scene, entity, grid);
        if (!target->IsValid())
        {
            error =
                "Cached TestGame navigation grid was not valid after restore.";
            return false;
        }
        error.clear();
        return true;
    }

    void InjectCpuColliders(
        const wi::scene::Scene& scene,
        const renegade::bridge::NavigationGridSettings& settings,
        wi::VoxelGrid& grid)
    {
        if ((settings.filterMask &
                wi::enums::FILTER_COLLIDER) == 0u)
        {
            return;
        }

        for (std::size_t colliderIndex = 0;
            colliderIndex < scene.colliders.GetCount(); ++colliderIndex)
        {
            const wi::ecs::Entity entity =
                scene.colliders.GetEntity(colliderIndex);
            const auto& collider = scene.colliders[colliderIndex];
            if (!collider.IsCPUEnabled())
                continue;

            const std::uint32_t layerMask =
                EffectiveLayerMask(scene, entity);
            if ((settings.layerMask & layerMask) == 0u)
                continue;

            wi::unordered_set<wi::ecs::Entity> visiting;
            XMMATRIX world;
            if (!ResolveHierarchyWorldMatrix(
                    scene, entity, world, visiting))
            {
                continue;
            }

            const float scaleX =
                XMVectorGetX(XMVector3Length(world.r[0]));
            const float scaleY =
                XMVectorGetX(XMVector3Length(world.r[1]));
            const float scaleZ =
                XMVectorGetX(XMVector3Length(world.r[2]));
            const float radius =
                collider.radius *
                std::max(scaleX, std::max(scaleY, scaleZ));

            XMVECTOR offset =
                XMVector3Transform(
                    XMLoadFloat3(&collider.offset), world);
            XMVECTOR tail =
                XMVector3Transform(
                    XMLoadFloat3(&collider.tail), world);

            switch (collider.shape)
            {
            default:
            case wi::scene::ColliderComponent::Shape::Sphere:
            {
                wi::primitive::Sphere sphere;
                XMStoreFloat3(&sphere.center, offset);
                sphere.radius = radius;
                grid.inject_sphere(sphere, false);
            }
            break;

            case wi::scene::ColliderComponent::Shape::Capsule:
            {
                XMVECTOR direction = offset - tail;
                if (XMVectorGetX(XMVector3LengthSq(direction)) < 0.000001f)
                    direction = XMVectorSet(0, 1, 0, 0);
                else
                    direction = XMVector3Normalize(direction);
                offset += direction * radius;
                tail -= direction * radius;
                wi::primitive::Capsule capsule;
                XMStoreFloat3(&capsule.base, offset);
                XMStoreFloat3(&capsule.tip, tail);
                capsule.radius = radius;
                grid.inject_capsule(capsule, false);
            }
            break;

            case wi::scene::ColliderComponent::Shape::Plane:
            {
                XMMATRIX plane =
                    XMMatrixScaling(
                        collider.radius, 1.0f, collider.radius);
                plane =
                    plane *
                    XMMatrixTranslationFromVector(
                        XMLoadFloat3(&collider.offset));
                plane = plane * world;
                const XMVECTOR p0 =
                    XMVector3Transform(
                        XMVectorSet(-1, 0, -1, 1), plane);
                const XMVECTOR p1 =
                    XMVector3Transform(
                        XMVectorSet(1, 0, -1, 1), plane);
                const XMVECTOR p2 =
                    XMVector3Transform(
                        XMVectorSet(1, 0, 1, 1), plane);
                const XMVECTOR p3 =
                    XMVector3Transform(
                        XMVectorSet(-1, 0, 1, 1), plane);
                grid.inject_triangle(p0, p1, p2, false);
                grid.inject_triangle(p0, p2, p3, false);
            }
            break;
            }
        }
    }

    bool BakeGrid(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const renegade::bridge::NavigationGridSettings& settings,
        const bool haveNavigationBounds,
        std::string& error)
    {
        using namespace renegade::bridge;

        if (!ValidateNavigationGridSettings(settings, error))
            return false;

        auto* grid = scene.voxel_grids.GetComponent(entity);
        if (grid == nullptr)
        {
            error =
                "TestGame navigation grid lost its native Wicked VoxelGrid component.";
            return false;
        }

        grid->init(
            settings.resolutionX,
            settings.resolutionY,
            settings.resolutionZ);

        if (settings.fitToSceneBounds && haveNavigationBounds)
        {
            grid->from_aabb(scene.bounds);
        }
        else
        {
            grid->center = settings.center;
            grid->set_voxelsize(settings.voxelSize);
        }

        grid->cleardata();
        const std::uint32_t objectFilter =
            settings.filterMask & ~wi::enums::FILTER_COLLIDER;
        if (objectFilter != 0u)
        {
            scene.VoxelizeScene(
                *grid,
                false,
                objectFilter,
                settings.layerMask,
                settings.lod);
        }
        InjectCpuColliders(scene, settings, *grid);
        SyncGridTransform(scene, entity, *grid);

        if (!grid->IsValid())
        {
            error =
                "Wicked did not produce a valid automatic TestGame navigation grid.";
            return false;
        }

        error.clear();
        return true;
    }

    std::string GridSignature(
        const std::uint64_t geometryFingerprint,
        const std::string& sourceSceneIdentity,
        const renegade::bridge::NavigationGridSettings& settings)
    {
        std::uint64_t hash = FingerprintSeed;
        HashValue(hash, CacheFormatVersion);
        HashValue(hash, geometryFingerprint);
        HashString(hash, sourceSceneIdentity);
        HashValue(hash, settings.resolutionX);
        HashValue(hash, settings.resolutionY);
        HashValue(hash, settings.resolutionZ);
        HashValue(hash, settings.center);
        HashValue(hash, settings.voxelSize);
        HashValue(hash, settings.fitToSceneBounds);
        HashValue(hash, settings.filterMask);
        HashValue(hash, settings.layerMask);
        HashValue(hash, settings.lod);
        return HexHash(hash);
    }

    fs::path CacheDirectory(
        const std::string& projectRoot,
        const std::string& sourceSceneIdentity)
    {
        const std::string identity =
            sourceSceneIdentity.empty()
                ? std::string("<unsaved>")
                : sourceSceneIdentity;
        return fs::u8path(projectRoot) /
            "Intermediate" /
            "NavigationCache" /
            HexHash(HashText(identity));
    }

    fs::path CachePath(
        const fs::path& directory,
        const std::string& gridIdentity)
    {
        return directory /
            fs::u8path(
                HexHash(HashText(gridIdentity)) +
                ".rnavcache");
    }

    bool TryLoadCache(
        const fs::path& path,
        const std::string& expectedSignature,
        wi::VoxelGrid& grid)
    {
        std::error_code ec;
        if (!fs::is_regular_file(path, ec) || ec)
            return false;

        wi::Archive archive(
            path.generic_u8string(),
            true,
            false);
        if (!archive.IsOpen())
            return false;

        try
        {
            std::string magic;
            unsigned int version = 0;
            std::string signature;
            archive >> magic;
            archive >> version;
            archive >> signature;
            if (magic != CacheMagic ||
                version != CacheFormatVersion ||
                signature != expectedSignature)
            {
                return false;
            }

            wi::ecs::EntitySerializer serializer;
            serializer.allow_remap = false;
            grid.Serialize(archive, serializer);
            return grid.IsValid() &&
                archive.GetPos() == archive.GetSize();
        }
        catch (...)
        {
            return false;
        }
    }

    bool SaveCache(
        const fs::path& path,
        const std::string& signature,
        const wi::VoxelGrid& sourceGrid,
        std::string& error)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
        {
            error =
                "Could not create TestGame navigation cache directory: " +
                ec.message();
            return false;
        }

        const fs::path temporary =
            fs::u8path(
                path.generic_u8string() +
                ".tmp-" +
                renegade::bridge::GenerateStableId());

        try
        {
            wi::Archive archive;
            archive << std::string(CacheMagic);
            archive << static_cast<unsigned int>(CacheFormatVersion);
            archive << signature;

            wi::VoxelGrid grid = sourceGrid;
            wi::ecs::EntitySerializer serializer;
            serializer.allow_remap = false;
            grid.Serialize(archive, serializer);

            if (!archive.SaveFile(temporary.generic_u8string()))
            {
                error =
                    "Could not write TestGame navigation cache: " +
                    temporary.generic_u8string();
                return false;
            }
            archive = wi::Archive();
        }
        catch (const std::exception& exception)
        {
            fs::remove(temporary, ec);
            error =
                std::string(
                    "Could not serialize TestGame navigation cache: ") +
                exception.what();
            return false;
        }
        catch (...)
        {
            fs::remove(temporary, ec);
            error =
                "Could not serialize TestGame navigation cache.";
            return false;
        }

        ec.clear();
        if (fs::exists(path, ec) && !ec)
            fs::remove(path, ec);
        if (ec)
        {
            const std::string replaceError = ec.message();
            std::error_code ignored;
            fs::remove(temporary, ignored);
            error =
                "Could not replace previous TestGame navigation cache: " +
                replaceError;
            return false;
        }

        fs::rename(temporary, path, ec);
        if (ec)
        {
            std::error_code ignored;
            fs::remove(temporary, ignored);
            error =
                "Could not install TestGame navigation cache: " +
                ec.message();
            return false;
        }

        error.clear();
        return true;
    }
}

namespace renegade::bridge
{
    bool PrepareTestLevelNavigationCache(
        const std::string& projectRoot,
        const std::string& sourceSceneIdentity,
        wi::scene::Scene& scene,
        TestLevelNavigationPreparation& result,
        std::string& error)
    {
        result = {};
        error.clear();

        if (projectRoot.empty())
        {
            error =
                "A project root is required for TestGame navigation caching.";
            return false;
        }

        PreparedNavigationGeometry geometry;
        if (!PrepareGeometryStreams(scene, geometry, error))
            return false;

        std::vector<wi::ecs::Entity> authoredGrids;
        for (std::size_t index = 0;
            index < scene.voxel_grids.GetCount(); ++index)
        {
            const wi::ecs::Entity entity =
                scene.voxel_grids.GetEntity(index);
            if (IsRenegadeNavigationGrid(scene, entity))
                authoredGrids.push_back(entity);
        }

        const fs::path cacheDirectory =
            CacheDirectory(projectRoot, sourceSceneIdentity);
        result.cacheDirectory =
            cacheDirectory.generic_u8string();

        const bool automaticGrid = authoredGrids.empty();
        if (automaticGrid)
            authoredGrids.push_back(wi::ecs::INVALID_ENTITY);

        std::uint64_t combinedSignature = FingerprintSeed;
        for (wi::ecs::Entity entity : authoredGrids)
        {
            NavigationGridSettings settings;
            std::string gridIdentity;

            if (entity == wi::ecs::INVALID_ENTITY)
            {
                settings.fitToSceneBounds = geometry.hasBounds;
                gridIdentity = "automatic-testgame-grid";
            }
            else
            {
                settings =
                    CaptureNavigationGridSettings(scene, entity);
                gridIdentity = PersistentEntityId(scene, entity);
                if (gridIdentity.empty())
                {
                    gridIdentity =
                        "entity-" +
                        std::to_string(
                            static_cast<std::uint64_t>(entity));
                }
            }

            const std::string signature =
                GridSignature(
                    geometry.fingerprint,
                    sourceSceneIdentity,
                    settings);
            HashString(combinedSignature, signature);

            const fs::path cachePath =
                CachePath(cacheDirectory, gridIdentity);
            wi::VoxelGrid cached;
            const bool reused =
                TryLoadCache(cachePath, signature, cached);

            if (reused)
            {
                if (entity == wi::ecs::INVALID_ENTITY)
                {
                    entity =
                        CreateCachedGridEntity(
                            scene, cached, error);
                    if (entity == wi::ecs::INVALID_ENTITY)
                        return false;
                }
                else if (!ApplyCachedGrid(
                        scene, entity, cached, error))
                {
                    return false;
                }
                result.reusedCache = true;
            }
            else
            {
                if (entity == wi::ecs::INVALID_ENTITY)
                {
                    entity = wi::ecs::CreateEntity();
                    if (entity == wi::ecs::INVALID_ENTITY)
                    {
                        error =
                            "Wicked could not allocate the automatic TestGame navigation grid.";
                        return false;
                    }

                    scene.names.Create(entity) =
                        "Automatic Navigation Grid";
                    scene.transforms.Create(entity);
                    scene.voxel_grids.Create(entity);
                    auto& metadata =
                        scene.metadatas.Create(entity);
                    metadata.string_values.set(
                        NavigationGridMetadataKey,
                        NavigationGridMetadataVersion);
                    if (!AssignPersistentEntityId(
                            scene,
                            entity,
                            GenerateStableId(),
                            error))
                    {
                        scene.Entity_Remove(entity);
                        return false;
                    }
                }

                if (!BakeGrid(
                        scene,
                        entity,
                        settings,
                        geometry.hasBounds,
                        error))
                {
                    return false;
                }

                const auto* grid =
                    scene.voxel_grids.GetComponent(entity);
                if (grid == nullptr ||
                    !SaveCache(
                        cachePath,
                        signature,
                        *grid,
                        error))
                {
                    if (error.empty())
                    {
                        error =
                            "Automatic TestGame navigation bake could not be cached.";
                    }
                    return false;
                }
                result.rebuiltCache = true;
            }

            ++result.gridCount;
        }

        result.signature = HexHash(combinedSignature);
        error.clear();
        return true;
    }
}
