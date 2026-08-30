#include "renegade/bridge/LightmapBakeService.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <xatlas.h>
#include <wiTextureHelper.h>

namespace renegade::bridge
{
    namespace
    {
        using wi::ecs::Entity;
        using wi::scene::MeshComponent;
        using wi::scene::ObjectComponent;
        using wi::scene::Scene;

        struct ObjectBakeSnapshot
        {
            Entity entity = wi::ecs::INVALID_ENTITY;
            std::uint32_t lightmapWidth = 0;
            std::uint32_t lightmapHeight = 0;
            bool lightmapDisableBlockCompression = false;
            std::vector<std::uint8_t> lightmapTextureData;
            std::vector<std::uint8_t> vertexAo;
        };

        struct MeshBakeSnapshot
        {
            Entity entity = wi::ecs::INVALID_ENTITY;
            wi::vector<std::uint32_t> indices;
            wi::vector<XMFLOAT3> positions;
            wi::vector<XMFLOAT2> atlas;
            wi::vector<XMFLOAT3> normals;
            wi::vector<std::uint8_t> windWeights;
            wi::vector<XMFLOAT4> tangents;
            wi::vector<XMFLOAT2> uv0;
            wi::vector<XMFLOAT2> uv1;
            wi::vector<std::uint32_t> colors;
            wi::vector<XMUINT4> boneIndices;
            wi::vector<XMFLOAT4> boneWeights;
            wi::vector<XMUINT4> boneIndices2;
            wi::vector<XMFLOAT4> boneWeights2;
        };

        struct BakeSnapshot
        {
            std::vector<ObjectBakeSnapshot> objects;
            std::vector<MeshBakeSnapshot> meshes;
        };

        struct AtlasDimensions
        {
            std::uint32_t width = 0;
            std::uint32_t height = 0;
        };

        [[nodiscard]] std::vector<Entity> NormalizeTargets(
            const std::vector<Entity>& targets)
        {
            std::vector<Entity> result;
            result.reserve(targets.size());
            for (const Entity entity : targets)
            {
                if (entity == wi::ecs::INVALID_ENTITY)
                    continue;
                if (std::find(result.begin(), result.end(), entity) == result.end())
                    result.push_back(entity);
            }
            return result;
        }

        [[nodiscard]] bool ValidateTarget(
            const Scene& scene,
            const Entity entity,
            std::string& error)
        {
            const ObjectComponent* object = scene.objects.GetComponent(entity);
            if (object == nullptr)
            {
                error = "Selected entity is not a renderable mesh object.";
                return false;
            }
            if (object->meshID == wi::ecs::INVALID_ENTITY)
            {
                error = "Selected object does not reference a mesh.";
                return false;
            }
            const MeshComponent* mesh = scene.meshes.GetComponent(object->meshID);
            if (mesh == nullptr)
            {
                error = "Selected object's mesh is unavailable.";
                return false;
            }
            if (object->IsDynamic() || mesh->IsDynamic())
            {
                error = "Lightmap and Vertex AO baking require static geometry.";
                return false;
            }
            if (mesh->IsSkinned())
            {
                error = "Skinned/deforming meshes are not eligible for static baking.";
                return false;
            }
            if (scene.softbodies.GetComponent(object->meshID) != nullptr)
            {
                error = "Soft-body meshes are not eligible for static baking.";
                return false;
            }
            if (mesh->vertex_positions.empty() || mesh->indices.empty())
            {
                error = "Selected mesh contains no bakeable triangles.";
                return false;
            }
            return true;
        }

        [[nodiscard]] bool ValidateTargets(
            const Scene& scene,
            const std::vector<Entity>& targets,
            std::string& error)
        {
            if (targets.empty())
            {
                error = "Select a static mesh object to bake.";
                return false;
            }
            for (const Entity entity : targets)
            {
                if (!ValidateTarget(scene, entity, error))
                    return false;
            }
            return true;
        }

        [[nodiscard]] ObjectBakeSnapshot CaptureObject(
            const Scene& scene,
            const Entity entity)
        {
            ObjectBakeSnapshot snapshot;
            snapshot.entity = entity;
            if (const ObjectComponent* object = scene.objects.GetComponent(entity))
            {
                snapshot.lightmapWidth = object->lightmapWidth;
                snapshot.lightmapHeight = object->lightmapHeight;
                snapshot.lightmapDisableBlockCompression =
                    object->IsLightmapDisableBlockCompression();
                snapshot.lightmapTextureData.assign(
                    object->lightmapTextureData.begin(),
                    object->lightmapTextureData.end());
                snapshot.vertexAo.assign(
                    object->vertex_ao.begin(),
                    object->vertex_ao.end());
            }
            return snapshot;
        }

        [[nodiscard]] MeshBakeSnapshot CaptureMesh(
            const Scene& scene,
            const Entity entity)
        {
            MeshBakeSnapshot snapshot;
            snapshot.entity = entity;
            if (const MeshComponent* mesh = scene.meshes.GetComponent(entity))
            {
                snapshot.indices = mesh->indices;
                snapshot.positions = mesh->vertex_positions;
                snapshot.atlas = mesh->vertex_atlas;
                snapshot.normals = mesh->vertex_normals;
                snapshot.windWeights = mesh->vertex_windweights;
                snapshot.tangents = mesh->vertex_tangents;
                snapshot.uv0 = mesh->vertex_uvset_0;
                snapshot.uv1 = mesh->vertex_uvset_1;
                snapshot.colors = mesh->vertex_colors;
                snapshot.boneIndices = mesh->vertex_boneindices;
                snapshot.boneWeights = mesh->vertex_boneweights;
                snapshot.boneIndices2 = mesh->vertex_boneindices2;
                snapshot.boneWeights2 = mesh->vertex_boneweights2;
            }
            return snapshot;
        }

        [[nodiscard]] BakeSnapshot CaptureSnapshot(
            const Scene& scene,
            const std::vector<Entity>& targets,
            const bool includeMeshes)
        {
            BakeSnapshot snapshot;
            snapshot.objects.reserve(targets.size());
            std::unordered_set<Entity> meshes;
            for (const Entity entity : targets)
            {
                snapshot.objects.push_back(CaptureObject(scene, entity));
                if (includeMeshes)
                {
                    if (const ObjectComponent* object = scene.objects.GetComponent(entity))
                        meshes.insert(object->meshID);
                }
            }
            snapshot.meshes.reserve(meshes.size());
            for (const Entity mesh : meshes)
                snapshot.meshes.push_back(CaptureMesh(scene, mesh));
            return snapshot;
        }

        [[nodiscard]] bool ApplySnapshot(
            Scene& scene,
            const BakeSnapshot& snapshot)
        {
            for (const MeshBakeSnapshot& state : snapshot.meshes)
            {
                MeshComponent* mesh = scene.meshes.GetComponent(state.entity);
                if (mesh == nullptr)
                    return false;
                mesh->indices = state.indices;
                mesh->vertex_positions = state.positions;
                mesh->vertex_atlas = state.atlas;
                mesh->vertex_normals = state.normals;
                mesh->vertex_windweights = state.windWeights;
                mesh->vertex_tangents = state.tangents;
                mesh->vertex_uvset_0 = state.uv0;
                mesh->vertex_uvset_1 = state.uv1;
                mesh->vertex_colors = state.colors;
                mesh->vertex_boneindices = state.boneIndices;
                mesh->vertex_boneweights = state.boneWeights;
                mesh->vertex_boneindices2 = state.boneIndices2;
                mesh->vertex_boneweights2 = state.boneWeights2;
                mesh->CreateRenderData();
            }

            for (const ObjectBakeSnapshot& state : snapshot.objects)
            {
                ObjectComponent* object = scene.objects.GetComponent(state.entity);
                if (object == nullptr)
                    return false;
                object->SetLightmapRenderRequest(false);
                object->ClearLightmap();
                object->lightmapWidth = state.lightmapWidth;
                object->lightmapHeight = state.lightmapHeight;
                object->SetLightmapDisableBlockCompression(
                    state.lightmapDisableBlockCompression);
                object->lightmapTextureData.assign(
                    state.lightmapTextureData.begin(),
                    state.lightmapTextureData.end());
                object->vertex_ao.assign(
                    state.vertexAo.begin(),
                    state.vertexAo.end());
                if (!object->lightmapTextureData.empty() &&
                    object->lightmapWidth > 0 && object->lightmapHeight > 0)
                {
                    const wi::graphics::Format format =
                        state.lightmapDisableBlockCompression
                            ? wi::graphics::Format::R11G11B10_FLOAT
                            : wi::graphics::Format::BC6H_UF16;
                    if (!wi::texturehelper::CreateTexture(
                            object->lightmap,
                            object->lightmapTextureData.data(),
                            object->lightmapWidth,
                            object->lightmapHeight,
                            format))
                    {
                        return false;
                    }
                    wi::graphics::GetDevice()->SetName(
                        &object->lightmap, "lightmap");
                }
                object->CreateRenderData();
            }
            scene.SetAccelerationStructureUpdateRequested(true);
            return true;
        }

        class RestoreBakeStateCommand final : public ICommand
        {
        public:
            RestoreBakeStateCommand(
                Scene& scene,
                BakeSnapshot before,
                BakeSnapshot after)
                : scene_(&scene)
                , before_(std::move(before))
                , after_(std::move(after))
            {
            }

            bool Execute() override
            {
                return scene_ != nullptr && ApplySnapshot(*scene_, after_);
            }

            void Undo() override
            {
                if (scene_ != nullptr)
                    (void)ApplySnapshot(*scene_, before_);
            }

        private:
            Scene* scene_ = nullptr;
            BakeSnapshot before_;
            BakeSnapshot after_;
        };

        [[nodiscard]] bool ValidateUvSource(
            const MeshComponent& mesh,
            const LightmapUvSource source,
            std::string& error)
        {
            const std::size_t count = mesh.vertex_positions.size();
            switch (source)
            {
            case LightmapUvSource::CopyUv0:
                if (mesh.vertex_uvset_0.size() != count)
                {
                    error = "Copy UV 0 requires a complete UV0 set on the selected mesh.";
                    return false;
                }
                break;
            case LightmapUvSource::CopyUv1:
                if (mesh.vertex_uvset_1.size() != count)
                {
                    error = "Copy UV 1 requires a complete UV1 set on the selected mesh.";
                    return false;
                }
                break;
            case LightmapUvSource::KeepAtlas:
                if (mesh.vertex_atlas.size() != count)
                {
                    error = "Keep Atlas requires an existing complete lightmap atlas channel.";
                    return false;
                }
                break;
            case LightmapUvSource::GenerateAtlas:
                if (!mesh.vertex_uvset_0.empty() && mesh.vertex_uvset_0.size() != count)
                {
                    error = "Generate Atlas cannot consume an incomplete UV0 set.";
                    return false;
                }
                break;
            }
            return true;
        }

        [[nodiscard]] bool GenerateAtlas(
            MeshComponent& meshcomponent,
            const std::uint32_t resolution,
            AtlasDimensions& dimensions,
            std::string& error)
        {
            xatlas::Atlas* atlas = xatlas::Create();
            if (atlas == nullptr)
            {
                error = "xatlas could not create an atlas context.";
                return false;
            }

            xatlas::MeshDecl mesh;
            mesh.vertexCount = static_cast<int>(meshcomponent.vertex_positions.size());
            mesh.vertexPositionData = meshcomponent.vertex_positions.data();
            mesh.vertexPositionStride = sizeof(float) * 3;
            if (!meshcomponent.vertex_normals.empty())
            {
                mesh.vertexNormalData = meshcomponent.vertex_normals.data();
                mesh.vertexNormalStride = sizeof(float) * 3;
            }
            if (!meshcomponent.vertex_uvset_0.empty())
            {
                mesh.vertexUvData = meshcomponent.vertex_uvset_0.data();
                mesh.vertexUvStride = sizeof(float) * 2;
            }
            mesh.indexCount = static_cast<int>(meshcomponent.indices.size());
            mesh.indexData = meshcomponent.indices.data();
            mesh.indexFormat = xatlas::IndexFormat::UInt32;

            const xatlas::AddMeshError addError = xatlas::AddMesh(atlas, mesh);
            if (addError != xatlas::AddMeshError::Success)
            {
                error = std::string("xatlas rejected the selected mesh: ") +
                    xatlas::StringForEnum(addError);
                xatlas::Destroy(atlas);
                return false;
            }

            xatlas::ChartOptions chartOptions;
            chartOptions.useInputMeshUvs = true;
            chartOptions.fixWinding = true;
            xatlas::PackOptions packOptions;
            packOptions.resolution = resolution;
            packOptions.blockAlign = true;
            packOptions.padding = 2;
            xatlas::Generate(atlas, chartOptions, packOptions);

            if (atlas->meshCount == 0 || atlas->meshes == nullptr ||
                atlas->width == 0 || atlas->height == 0)
            {
                error = "xatlas produced an empty lightmap atlas.";
                xatlas::Destroy(atlas);
                return false;
            }

            dimensions.width = atlas->width;
            dimensions.height = atlas->height;
            const xatlas::Mesh& generated = atlas->meshes[0];
            if (generated.vertexCount == 0 || generated.indexCount == 0)
            {
                error = "xatlas produced no bakeable vertices.";
                xatlas::Destroy(atlas);
                return false;
            }

            const auto sourcePositions = meshcomponent.vertex_positions;
            const auto sourceNormals = meshcomponent.vertex_normals;
            const auto sourceWind = meshcomponent.vertex_windweights;
            const auto sourceTangents = meshcomponent.vertex_tangents;
            const auto sourceUv0 = meshcomponent.vertex_uvset_0;
            const auto sourceUv1 = meshcomponent.vertex_uvset_1;
            const auto sourceColors = meshcomponent.vertex_colors;
            const auto sourceBoneIndices = meshcomponent.vertex_boneindices;
            const auto sourceBoneWeights = meshcomponent.vertex_boneweights;
            const auto sourceBoneIndices2 = meshcomponent.vertex_boneindices2;
            const auto sourceBoneWeights2 = meshcomponent.vertex_boneweights2;

            wi::vector<std::uint32_t> indices(generated.indexCount);
            wi::vector<XMFLOAT3> positions(generated.vertexCount);
            wi::vector<XMFLOAT2> atlasUv(generated.vertexCount);
            wi::vector<XMFLOAT3> normals;
            wi::vector<std::uint8_t> wind;
            wi::vector<XMFLOAT4> tangents;
            wi::vector<XMFLOAT2> uv0;
            wi::vector<XMFLOAT2> uv1;
            wi::vector<std::uint32_t> colors;
            wi::vector<XMUINT4> boneIndices;
            wi::vector<XMFLOAT4> boneWeights;
            wi::vector<XMUINT4> boneIndices2;
            wi::vector<XMFLOAT4> boneWeights2;

            if (!sourceNormals.empty()) normals.resize(generated.vertexCount);
            if (!sourceWind.empty()) wind.resize(generated.vertexCount);
            if (!sourceTangents.empty()) tangents.resize(generated.vertexCount);
            if (!sourceUv0.empty()) uv0.resize(generated.vertexCount);
            if (!sourceUv1.empty()) uv1.resize(generated.vertexCount);
            if (!sourceColors.empty()) colors.resize(generated.vertexCount);
            if (!sourceBoneIndices.empty()) boneIndices.resize(generated.vertexCount);
            if (!sourceBoneWeights.empty()) boneWeights.resize(generated.vertexCount);
            if (!sourceBoneIndices2.empty()) boneIndices2.resize(generated.vertexCount);
            if (!sourceBoneWeights2.empty()) boneWeights2.resize(generated.vertexCount);

            for (std::uint32_t j = 0; j < generated.indexCount; ++j)
            {
                const std::uint32_t generatedIndex = generated.indexArray[j];
                const xatlas::Vertex& vertex = generated.vertexArray[generatedIndex];
                if (vertex.xref >= sourcePositions.size())
                {
                    error = "xatlas returned an invalid source-vertex reference.";
                    xatlas::Destroy(atlas);
                    return false;
                }

                indices[j] = generatedIndex;
                atlasUv[generatedIndex].x = vertex.uv[0] / static_cast<float>(dimensions.width);
                atlasUv[generatedIndex].y = vertex.uv[1] / static_cast<float>(dimensions.height);
                positions[generatedIndex] = sourcePositions[vertex.xref];
                if (!normals.empty()) normals[generatedIndex] = sourceNormals[vertex.xref];
                if (!wind.empty()) wind[generatedIndex] = sourceWind[vertex.xref];
                if (!tangents.empty()) tangents[generatedIndex] = sourceTangents[vertex.xref];
                if (!uv0.empty()) uv0[generatedIndex] = sourceUv0[vertex.xref];
                if (!uv1.empty()) uv1[generatedIndex] = sourceUv1[vertex.xref];
                if (!colors.empty()) colors[generatedIndex] = sourceColors[vertex.xref];
                if (!boneIndices.empty()) boneIndices[generatedIndex] = sourceBoneIndices[vertex.xref];
                if (!boneWeights.empty()) boneWeights[generatedIndex] = sourceBoneWeights[vertex.xref];
                if (!boneIndices2.empty()) boneIndices2[generatedIndex] = sourceBoneIndices2[vertex.xref];
                if (!boneWeights2.empty()) boneWeights2[generatedIndex] = sourceBoneWeights2[vertex.xref];
            }

            meshcomponent.indices = std::move(indices);
            meshcomponent.vertex_positions = std::move(positions);
            meshcomponent.vertex_atlas = std::move(atlasUv);
            if (!normals.empty()) meshcomponent.vertex_normals = std::move(normals);
            if (!wind.empty()) meshcomponent.vertex_windweights = std::move(wind);
            if (!tangents.empty()) meshcomponent.vertex_tangents = std::move(tangents);
            if (!uv0.empty()) meshcomponent.vertex_uvset_0 = std::move(uv0);
            if (!uv1.empty()) meshcomponent.vertex_uvset_1 = std::move(uv1);
            if (!colors.empty()) meshcomponent.vertex_colors = std::move(colors);
            if (!boneIndices.empty()) meshcomponent.vertex_boneindices = std::move(boneIndices);
            if (!boneWeights.empty()) meshcomponent.vertex_boneweights = std::move(boneWeights);
            if (!boneIndices2.empty()) meshcomponent.vertex_boneindices2 = std::move(boneIndices2);
            if (!boneWeights2.empty()) meshcomponent.vertex_boneweights2 = std::move(boneWeights2);
            meshcomponent.CreateRenderData();

            xatlas::Destroy(atlas);
            return true;
        }

        [[nodiscard]] bool PrepareMeshes(
            Scene& scene,
            const std::vector<Entity>& targets,
            const LightmapBakeSettings& settings,
            std::unordered_map<Entity, AtlasDimensions>& dimensions,
            std::string& error)
        {
            std::unordered_set<Entity> prepared;
            for (const Entity entity : targets)
            {
                ObjectComponent* object = scene.objects.GetComponent(entity);
                if (object == nullptr)
                    return false;
                if (!prepared.insert(object->meshID).second)
                    continue;

                MeshComponent* mesh = scene.meshes.GetComponent(object->meshID);
                if (mesh == nullptr)
                {
                    error = "Bake target mesh disappeared during preparation.";
                    return false;
                }
                if (!ValidateUvSource(*mesh, settings.uvSource, error))
                    return false;

                AtlasDimensions dim;
                dim.width = settings.resolution;
                dim.height = settings.resolution;
                switch (settings.uvSource)
                {
                case LightmapUvSource::CopyUv0:
                    mesh->vertex_atlas = mesh->vertex_uvset_0;
                    mesh->CreateRenderData();
                    break;
                case LightmapUvSource::CopyUv1:
                    mesh->vertex_atlas = mesh->vertex_uvset_1;
                    mesh->CreateRenderData();
                    break;
                case LightmapUvSource::KeepAtlas:
                    break;
                case LightmapUvSource::GenerateAtlas:
                    if (!GenerateAtlas(*mesh, settings.resolution, dim, error))
                        return false;
                    break;
                }
                dimensions.emplace(object->meshID, dim);
            }
            return true;
        }

        [[nodiscard]] bool AnyLightmapData(
            const Scene& scene,
            const std::vector<Entity>& targets)
        {
            for (const Entity entity : targets)
            {
                if (const ObjectComponent* object = scene.objects.GetComponent(entity))
                {
                    if (!object->lightmapTextureData.empty() || object->lightmap.IsValid() ||
                        object->IsLightmapRenderRequested())
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        [[nodiscard]] bool AnyVertexAo(
            const Scene& scene,
            const std::vector<Entity>& targets)
        {
            for (const Entity entity : targets)
            {
                if (const ObjectComponent* object = scene.objects.GetComponent(entity))
                {
                    if (!object->vertex_ao.empty())
                        return true;
                }
            }
            return false;
        }

        [[nodiscard]] bool ComputeVertexAoForObject(
            Scene& scene,
            const Entity target,
            const VertexAoBakeSettings& settings,
            std::string& error)
        {
            ObjectComponent* object = scene.objects.GetComponent(target);
            if (object == nullptr)
                return false;
            const std::size_t objectIndex = scene.objects.GetIndex(target);
            MeshComponent* mesh = scene.meshes.GetComponent(object->meshID);
            if (mesh == nullptr)
                return false;
            if (mesh->vertex_normals.size() != mesh->vertex_positions.size())
            {
                error = "Vertex AO requires one normal per mesh vertex.";
                return false;
            }
            if (!mesh->bvh.IsValid())
                mesh->BuildBVH();

            object->vertex_ao.resize(mesh->vertex_positions.size());
            const std::uint32_t sampleCount = settings.rayCount;
            const float rayLength = settings.rayLength;
            const XMMATRIX world = XMLoadFloat4x4(&scene.matrix_objects[objectIndex]);

            wi::jobsystem::context context;
            const std::uint32_t groupSize = wi::jobsystem::DispatchGroupCount(
                static_cast<std::uint32_t>(object->vertex_ao.size()),
                wi::jobsystem::GetThreadCount());
            wi::jobsystem::Dispatch(
                context,
                static_cast<std::uint32_t>(object->vertex_ao.size()),
                groupSize,
                [&](wi::jobsystem::JobArgs args)
                {
                    XMFLOAT3 position = mesh->vertex_positions[args.jobIndex];
                    XMFLOAT3 normal = mesh->vertex_normals[args.jobIndex];
                    XMStoreFloat3(
                        &position,
                        XMVector3Transform(XMLoadFloat3(&position), world));
                    XMStoreFloat3(
                        &normal,
                        XMVector3Normalize(XMVector3TransformNormal(
                            XMLoadFloat3(&normal),
                            XMMatrixTranspose(XMMatrixInverse(nullptr, world)))));
                    const XMMATRIX tangentSpace = wi::math::GetTangentSpace(normal);
                    float accumulation = 0.0f;
                    const XMVECTOR rayOrigin = XMLoadFloat3(&position);

                    for (std::uint32_t sample = 0; sample < sampleCount; ++sample)
                    {
                        const XMFLOAT2 hammersley = wi::math::Hammersley2D(sample, sampleCount);
                        XMFLOAT3 hemisphere = wi::math::HemispherePoint_Cos(
                            hammersley.x, hammersley.y);
                        XMVECTOR rayDirection = XMVector3Normalize(
                            XMVector3TransformNormal(XMLoadFloat3(&hemisphere), tangentSpace));
                        XMStoreFloat3(&hemisphere, rayDirection);
                        wi::primitive::Ray ray(position, hemisphere, 0.0001f, rayLength);

                        bool hit = false;
                        for (std::size_t sceneObjectIndex = 0;
                             sceneObjectIndex < scene.aabb_objects.size() && !hit;
                             ++sceneObjectIndex)
                        {
                            const wi::primitive::AABB& aabb = scene.aabb_objects[sceneObjectIndex];
                            if (!ray.intersects(aabb))
                                continue;

                            const ObjectComponent& occluder = scene.objects[sceneObjectIndex];
                            if (occluder.meshID == wi::ecs::INVALID_ENTITY)
                                continue;
                            const MeshComponent* occluderMesh =
                                scene.meshes.GetComponent(occluder.meshID);
                            if (occluderMesh == nullptr)
                                continue;

                            const XMMATRIX objectMatrix =
                                XMLoadFloat4x4(&scene.matrix_objects[sceneObjectIndex]);
                            const XMMATRIX inverseObject = XMMatrixInverse(nullptr, objectMatrix);
                            const XMVECTOR localOrigin = XMVector3Transform(rayOrigin, inverseObject);
                            const XMVECTOR localDirection = XMVector3Normalize(
                                XMVector3TransformNormal(rayDirection, inverseObject));

                            const auto intersectsTriangle = [&](const std::uint32_t subsetIndex,
                                                               const std::uint32_t indexOffset,
                                                               const std::uint32_t triangleIndex)
                            {
                                const std::uint32_t i0 = occluderMesh->indices[indexOffset + triangleIndex * 3 + 0];
                                const std::uint32_t i1 = occluderMesh->indices[indexOffset + triangleIndex * 3 + 1];
                                const std::uint32_t i2 = occluderMesh->indices[indexOffset + triangleIndex * 3 + 2];
                                const XMVECTOR p0 = XMLoadFloat3(&occluderMesh->vertex_positions[i0]);
                                const XMVECTOR p1 = XMLoadFloat3(&occluderMesh->vertex_positions[i1]);
                                const XMVECTOR p2 = XMLoadFloat3(&occluderMesh->vertex_positions[i2]);
                                float distance = 0.0f;
                                XMFLOAT2 barycentric;
                                return wi::math::RayTriangleIntersects(
                                    localOrigin, localDirection, p0, p1, p2,
                                    distance, barycentric, ray.TMin, ray.TMax);
                            };

                            if (occluderMesh->bvh.IsValid())
                            {
                                wi::primitive::Ray localRay(localOrigin, localDirection);
                                hit = occluderMesh->bvh.IntersectsFirst(
                                    localRay,
                                    [&](const std::uint32_t index)
                                    {
                                        const wi::primitive::AABB& leaf =
                                            occluderMesh->bvh_leaf_aabbs[index];
                                        const std::uint32_t triangleIndex = leaf.layerMask;
                                        const std::uint32_t subsetIndex = leaf.userdata;
                                        const MeshComponent::MeshSubset& subset =
                                            occluderMesh->subsets[subsetIndex];
                                        if (subset.indexCount == 0)
                                            return false;
                                        const auto* material =
                                            scene.materials.GetComponent(subset.materialID);
                                        if (material != nullptr &&
                                            material->GetBlendMode() != wi::enums::BLENDMODE_OPAQUE)
                                        {
                                            return false;
                                        }
                                        return intersectsTriangle(
                                            subsetIndex,
                                            subset.indexOffset,
                                            triangleIndex);
                                    });
                            }
                            else
                            {
                                std::uint32_t firstSubset = 0;
                                std::uint32_t lastSubset = 0;
                                occluderMesh->GetLODSubsetRange(0, firstSubset, lastSubset);
                                for (std::uint32_t subsetIndex = firstSubset;
                                     subsetIndex < lastSubset && !hit;
                                     ++subsetIndex)
                                {
                                    const MeshComponent::MeshSubset& subset =
                                        occluderMesh->subsets[subsetIndex];
                                    if (subset.indexCount == 0)
                                        continue;
                                    const auto* material =
                                        scene.materials.GetComponent(subset.materialID);
                                    if (material != nullptr &&
                                        material->GetBlendMode() != wi::enums::BLENDMODE_OPAQUE)
                                    {
                                        continue;
                                    }
                                    const std::uint32_t triangleCount = subset.indexCount / 3;
                                    for (std::uint32_t triangle = 0;
                                         triangle < triangleCount && !hit;
                                         ++triangle)
                                    {
                                        hit = intersectsTriangle(
                                            subsetIndex,
                                            subset.indexOffset,
                                            triangle);
                                    }
                                }
                            }
                        }

                        if (!hit)
                            accumulation += 1.0f;
                    }

                    accumulation /= static_cast<float>(sampleCount);
                    object->vertex_ao[args.jobIndex] = static_cast<std::uint8_t>(
                        std::clamp(accumulation, 0.0f, 1.0f) * 255.0f);
                });
            wi::jobsystem::Wait(context);
            object->CreateRenderData();
            if (!mesh->IsBVHEnabled())
            {
                mesh->bvh = {};
                mesh->bvh_leaf_aabbs.clear();
            }
            return true;
        }
    }

    struct LightmapBakeSession::Impl
    {
        std::vector<Entity> targets;
        BakeSnapshot before;
        bool active = false;
    };

    LightmapBakeSession::LightmapBakeSession()
        : impl_(std::make_unique<Impl>())
    {
    }

    LightmapBakeSession::~LightmapBakeSession() = default;
    LightmapBakeSession::LightmapBakeSession(LightmapBakeSession&&) noexcept = default;
    LightmapBakeSession& LightmapBakeSession::operator=(LightmapBakeSession&&) noexcept = default;

    bool LightmapBakeSession::IsActive() const noexcept
    {
        return impl_ != nullptr && impl_->active;
    }

    const std::vector<Entity>& LightmapBakeSession::Targets() const noexcept
    {
        static const std::vector<Entity> empty;
        return impl_ == nullptr ? empty : impl_->targets;
    }

    bool LightmapBakeService::IsValidResolution(const std::uint32_t resolution) noexcept
    {
        return resolution >= 32 && resolution <= 8192 &&
            (resolution & (resolution - 1)) == 0;
    }

    bool LightmapBakeService::HydratePersistedLightmaps(
        Scene& scene,
        std::string& error)
    {
        for (std::size_t index = 0; index < scene.objects.GetCount(); ++index)
        {
            ObjectComponent& object = scene.objects[index];
            if (object.lightmapTextureData.empty())
                continue;
            if (object.lightmapWidth == 0 || object.lightmapHeight == 0)
            {
                error = "Serialized lightmap data has invalid zero dimensions.";
                return false;
            }
            if (object.lightmap.IsValid())
                continue;

            const wi::graphics::Format format =
                object.IsLightmapDisableBlockCompression()
                    ? wi::graphics::Format::R11G11B10_FLOAT
                    : wi::graphics::Format::BC6H_UF16;
            if (!wi::texturehelper::CreateTexture(
                    object.lightmap,
                    object.lightmapTextureData.data(),
                    object.lightmapWidth,
                    object.lightmapHeight,
                    format))
            {
                error = "Could not reconstruct a serialized native Wicked lightmap texture.";
                return false;
            }
            wi::graphics::GetDevice()->SetName(&object.lightmap, "lightmap");
        }
        error.clear();
        return true;
    }

    LightmapBakeStatus LightmapBakeService::CaptureStatus(
        const Scene& scene,
        const std::vector<Entity>& requestedTargets)
    {
        LightmapBakeStatus status;
        const std::vector<Entity> targets = NormalizeTargets(requestedTargets);
        status.targetCount = targets.size();
        std::string error;
        status.eligible = ValidateTargets(scene, targets, error);
        status.message = status.eligible ? "Ready to bake selected static geometry." : error;

        for (const Entity entity : targets)
        {
            const ObjectComponent* object = scene.objects.GetComponent(entity);
            if (object == nullptr)
                continue;
            if (!object->lightmapTextureData.empty() || object->lightmap.IsValid())
                ++status.bakedCount;
            if (object->IsLightmapRenderRequested())
                ++status.activeCount;
            if (!object->vertex_ao.empty())
                ++status.vertexAoCount;
            status.highestIteration = std::max(
                status.highestIteration,
                object->lightmapIterationCount);
        }
        return status;
    }

    bool LightmapBakeService::BeginBake(
        Scene& scene,
        const std::vector<Entity>& requestedTargets,
        const LightmapBakeSettings& settings,
        LightmapBakeSession& session,
        std::string& error)
    {
        if (session.impl_ == nullptr)
            session.impl_ = std::make_unique<LightmapBakeSession::Impl>();
        if (session.impl_->active)
        {
            error = "A lightmap bake is already active.";
            return false;
        }
        if (!IsValidResolution(settings.resolution))
        {
            error = "Lightmap resolution must be a power of two from 32 through 8192.";
            return false;
        }

        const std::vector<Entity> targets = NormalizeTargets(requestedTargets);
        if (!ValidateTargets(scene, targets, error))
            return false;

        const BakeSnapshot before = CaptureSnapshot(scene, targets, true);
        std::unordered_map<Entity, AtlasDimensions> dimensions;
        if (!PrepareMeshes(scene, targets, settings, dimensions, error))
        {
            (void)ApplySnapshot(scene, before);
            return false;
        }

        for (const Entity entity : targets)
        {
            ObjectComponent* object = scene.objects.GetComponent(entity);
            if (object == nullptr)
            {
                error = "Bake target disappeared during preparation.";
                (void)ApplySnapshot(scene, before);
                return false;
            }
            const auto found = dimensions.find(object->meshID);
            if (found == dimensions.end())
            {
                error = "Prepared lightmap dimensions are unavailable.";
                (void)ApplySnapshot(scene, before);
                return false;
            }

            object->SetLightmapRenderRequest(false);
            object->ClearLightmap();
            object->SetLightmapDisableBlockCompression(!settings.blockCompression);
            object->lightmapWidth = found->second.width;
            object->lightmapHeight = found->second.height;
            object->lightmapIterationCount = 0;
            object->SetLightmapRenderRequest(true);
        }

        scene.SetAccelerationStructureUpdateRequested(true);
        session.impl_->targets = targets;
        session.impl_->before = before;
        session.impl_->active = true;
        error.clear();
        return true;
    }

    bool LightmapBakeService::StopAndSave(
        Scene& scene,
        LightmapBakeSession& session,
        CommandService& commands,
        std::string& error)
    {
        if (session.impl_ == nullptr || !session.impl_->active)
        {
            error = "No lightmap bake is active.";
            return false;
        }

        for (const Entity entity : session.impl_->targets)
        {
            ObjectComponent* object = scene.objects.GetComponent(entity);
            if (object == nullptr)
            {
                error = "Bake target disappeared before completion.";
                Cancel(scene, session);
                return false;
            }
            if (!object->lightmap.IsValid() ||
                object->lightmapIterationCount == 0)
            {
                error = "The native lightmap bake has not produced a sample yet.";
                Cancel(scene, session);
                return false;
            }
            object->SetLightmapRenderRequest(false);
            object->SaveLightmap();
            if (object->lightmapTextureData.empty())
            {
                error = "Wicked did not return persisted lightmap data.";
                Cancel(scene, session);
                return false;
            }
        }

        const BakeSnapshot after = CaptureSnapshot(scene, session.impl_->targets, true);
        auto command = std::make_unique<RestoreBakeStateCommand>(
            scene,
            session.impl_->before,
            after);
        if (!commands.RecordExecuted(std::move(command)))
        {
            error = "The completed bake could not be recorded in Undo history.";
            (void)ApplySnapshot(scene, session.impl_->before);
            session.impl_->active = false;
            session.impl_->targets.clear();
            session.impl_->before = {};
            return false;
        }

        session.impl_->active = false;
        session.impl_->targets.clear();
        session.impl_->before = {};
        error.clear();
        return true;
    }

    void LightmapBakeService::Cancel(
        Scene& scene,
        LightmapBakeSession& session) noexcept
    {
        if (session.impl_ == nullptr || !session.impl_->active)
            return;
        (void)ApplySnapshot(scene, session.impl_->before);
        session.impl_->active = false;
        session.impl_->targets.clear();
        session.impl_->before = {};
    }

    bool LightmapBakeService::ClearLightmaps(
        Scene& scene,
        const std::vector<Entity>& requestedTargets,
        CommandService& commands,
        std::string& error)
    {
        const std::vector<Entity> targets = NormalizeTargets(requestedTargets);
        if (!ValidateTargets(scene, targets, error))
            return false;
        if (!AnyLightmapData(scene, targets))
        {
            error = "The selected object has no lightmap to clear.";
            return false;
        }

        BakeSnapshot before = CaptureSnapshot(scene, targets, false);
        for (const Entity entity : targets)
        {
            if (ObjectComponent* object = scene.objects.GetComponent(entity))
            {
                object->SetLightmapRenderRequest(false);
                object->ClearLightmap();
            }
        }
        BakeSnapshot after = CaptureSnapshot(scene, targets, false);
        if (!commands.RecordExecuted(std::make_unique<RestoreBakeStateCommand>(
                scene, std::move(before), std::move(after))))
        {
            error = "Lightmap clear could not be recorded in Undo history.";
            return false;
        }
        error.clear();
        return true;
    }

    bool LightmapBakeService::ComputeVertexAo(
        Scene& scene,
        const std::vector<Entity>& requestedTargets,
        const VertexAoBakeSettings& settings,
        CommandService& commands,
        std::string& error)
    {
        const std::vector<Entity> targets = NormalizeTargets(requestedTargets);
        if (!ValidateTargets(scene, targets, error))
            return false;
        if (settings.rayCount < 8 || settings.rayCount > 1024)
        {
            error = "Vertex AO ray count must be between 8 and 1024.";
            return false;
        }
        if (!std::isfinite(settings.rayLength) ||
            settings.rayLength < 0.0f || settings.rayLength > 1000.0f)
        {
            error = "Vertex AO ray length must be between 0 and 1000.";
            return false;
        }

        BakeSnapshot before = CaptureSnapshot(scene, targets, false);
        for (const Entity entity : targets)
        {
            if (!ComputeVertexAoForObject(scene, entity, settings, error))
            {
                (void)ApplySnapshot(scene, before);
                return false;
            }
        }
        BakeSnapshot after = CaptureSnapshot(scene, targets, false);
        if (!commands.RecordExecuted(std::make_unique<RestoreBakeStateCommand>(
                scene, std::move(before), std::move(after))))
        {
            error = "Vertex AO bake could not be recorded in Undo history.";
            return false;
        }
        error.clear();
        return true;
    }

    bool LightmapBakeService::ClearVertexAo(
        Scene& scene,
        const std::vector<Entity>& requestedTargets,
        CommandService& commands,
        std::string& error)
    {
        const std::vector<Entity> targets = NormalizeTargets(requestedTargets);
        if (!ValidateTargets(scene, targets, error))
            return false;
        if (!AnyVertexAo(scene, targets))
        {
            error = "The selected object has no baked Vertex AO to clear.";
            return false;
        }

        BakeSnapshot before = CaptureSnapshot(scene, targets, false);
        for (const Entity entity : targets)
        {
            if (ObjectComponent* object = scene.objects.GetComponent(entity))
            {
                object->vertex_ao.clear();
                object->CreateRenderData();
            }
        }
        BakeSnapshot after = CaptureSnapshot(scene, targets, false);
        if (!commands.RecordExecuted(std::make_unique<RestoreBakeStateCommand>(
                scene, std::move(before), std::move(after))))
        {
            error = "Vertex AO clear could not be recorded in Undo history.";
            return false;
        }
        error.clear();
        return true;
    }
}
