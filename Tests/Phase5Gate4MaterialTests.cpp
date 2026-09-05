#include "renegade/bridge/MaterialService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace
{
    bool Near(const float left, const float right, const float epsilon = 0.001f)
    {
        return std::abs(left - right) <= epsilon;
    }

    [[noreturn]] void Fail(const std::string& message)
    {
        std::cerr << "PHASE5 GATE4 MATERIAL FAIL // " << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    wi::ecs::Entity CreateMaterial(wi::scene::Scene& scene, const char* name)
    {
        const auto entity = wi::ecs::CreateEntity();
        scene.names.Create(entity).name = name;
        scene.materials.Create(entity);
        return entity;
    }

    wi::ecs::Entity CreateMeshObject(
        wi::scene::Scene& scene,
        const std::vector<wi::ecs::Entity>& materials)
    {
        const auto meshEntity = wi::ecs::CreateEntity();
        auto& mesh = scene.meshes.Create(meshEntity);
        mesh.subsets.resize(materials.size());
        for (std::size_t i = 0; i < materials.size(); ++i)
            mesh.subsets[i].materialID = materials[i];

        const auto objectEntity = wi::ecs::CreateEntity();
        scene.objects.Create(objectEntity).meshID = meshEntity;
        return objectEntity;
    }
}

int main()
{
    using namespace renegade::bridge;
    using Material = wi::scene::MaterialComponent;

    wi::scene::Scene scene;
    const auto materialA = CreateMaterial(scene, "Body");
    const auto materialB = CreateMaterial(scene, "Trim");
    const auto materialC = CreateMaterial(scene, "Glass");

    const auto direct = CollectEditableMaterialEntities(scene, materialA);
    if (direct.size() != 1 || direct.front() != materialA)
        Fail("direct material selection did not resolve");

    const auto oneMaterialObject = CreateMeshObject(scene, {materialA});
    if (ResolveEditableMaterialEntity(scene, oneMaterialObject) != materialA)
        Fail("single-material object did not resolve uniquely");

    const auto multiMaterialObject = CreateMeshObject(scene, {materialA, materialB});
    const auto multi = CollectEditableMaterialEntities(scene, multiMaterialObject);
    if (multi.size() != 2 || multi[0] != materialA || multi[1] != materialB)
        Fail("multi-material mesh collection failed");
    if (ResolveEditableMaterialEntity(scene, multiMaterialObject) !=
        wi::ecs::INVALID_ENTITY)
    {
        Fail("ambiguous multi-material object incorrectly resolved as one material");
    }

    const auto root = wi::ecs::CreateEntity();
    const auto childA = CreateMeshObject(scene, {materialA, materialB});
    const auto childB = CreateMeshObject(scene, {materialB, materialC});
    scene.hierarchy.Create(childA).parentID = root;
    scene.hierarchy.Create(childB).parentID = root;
    const auto rootMaterials = CollectEditableMaterialEntities(scene, root);
    if (rootMaterials.size() != 3 || rootMaterials[0] != materialA ||
        rootMaterials[1] != materialB || rootMaterials[2] != materialC)
    {
        Fail("root descendant material collection/deduplication failed");
    }

    const auto terrainEntity = wi::ecs::CreateEntity();
    auto& terrain = scene.terrains.Create(terrainEntity);
    terrain.materialEntities.push_back(materialC);
    const auto filtered = CollectEditableMaterialEntities(scene, root);
    if (filtered.size() != 2 || filtered[0] != materialA || filtered[1] != materialB)
        Fail("terrain-owned material was not excluded");

    auto* material = scene.materials.GetComponent(materialA);
    if (material == nullptr)
        Fail("material fixture missing");
    material->SetCustomShaderID(7);

    MaterialState before = CaptureMaterial(*material);
    MaterialState edited = before;
    edited.shaderType = Material::SHADERTYPE_PBR_CLEARCOAT;
    edited.blendMode = wi::enums::BLENDMODE_ALPHA;
    edited.baseColor = XMFLOAT4(0.2f, 0.3f, 0.4f, 0.75f);
    edited.metalness = 0.8f;
    edited.roughness = 0.15f;
    edited.reflectance = 0.6f;
    edited.normalMapStrength = 2.25f;
    edited.alphaRef = 0.4f;
    edited.emissiveColor = XMFLOAT3(0.1f, 0.5f, 0.9f);
    edited.emissiveStrength = 6.0f;
    edited.receiveShadow = false;
    edited.castShadow = false;
    edited.useVertexColors = true;
    edited.doubleSided = true;
    edited.texMulAdd = XMFLOAT4(3.0f, 2.0f, 0.25f, -0.5f);
    edited.parallaxOcclusionMapping = 0.7f;
    edited.anisotropyStrength = 0.8f;
    edited.anisotropyRotationDegrees = 270.0f;
    edited.sheenColor = XMFLOAT3(0.7f, 0.4f, 0.2f);
    edited.sheenRoughness = 0.35f;
    edited.clearcoat = 0.9f;
    edited.clearcoatRoughness = 0.2f;
    edited.transmission = 0.45f;
    edited.refraction = 0.3f;
    edited.blendWithTerrainHeight = 1.25f;
    edited.meshBlend = 0.75f;
    edited.interiorMappingScale = XMFLOAT3(2.0f, 3.0f, 4.0f);
    edited.interiorMappingOffset = XMFLOAT3(0.1f, -0.2f, 0.3f);
    edited.interiorMappingRotationDegrees = 90.0f;

    CommandService commands;
    if (!commands.Execute(std::make_unique<SetMaterialCommand>(
            scene, materialA, before, edited)))
    {
        Fail("extended SetMaterialCommand did not execute");
    }

    const auto changed = CaptureMaterial(*material);
    if (changed.shaderType != Material::SHADERTYPE_PBR_CLEARCOAT ||
        changed.blendMode != wi::enums::BLENDMODE_ALPHA ||
        !Near(changed.baseColor.w, 0.75f) ||
        !Near(changed.normalMapStrength, 2.25f) ||
        !Near(changed.alphaRef, 0.4f) ||
        changed.receiveShadow || changed.castShadow ||
        !changed.useVertexColors || !changed.doubleSided ||
        !Near(changed.texMulAdd.x, 3.0f) ||
        !Near(changed.texMulAdd.z, 0.25f) ||
        !Near(changed.parallaxOcclusionMapping, 0.7f) ||
        !Near(changed.anisotropyRotationDegrees, 270.0f) ||
        !Near(changed.clearcoat, 0.9f) ||
        !Near(changed.transmission, 0.45f) ||
        !Near(changed.blendWithTerrainHeight, 1.25f) ||
        !Near(changed.interiorMappingRotationDegrees, 90.0f))
    {
        Fail("extended native material state was not applied");
    }
    if (material->GetCustomShaderID() != 7)
        Fail("unexposed custom shader identity was not preserved");

    if (!commands.Undo())
        Fail("extended material Undo failed");
    const auto undone = CaptureMaterial(*material);
    if (undone.shaderType != before.shaderType ||
        !Near(undone.baseColor.x, before.baseColor.x) ||
        undone.doubleSided != before.doubleSided)
    {
        Fail("extended material Undo did not restore prior state");
    }
    if (!commands.Redo())
        Fail("extended material Redo failed");

    MaterialState unsafe = edited;
    unsafe.shaderType = static_cast<Material::SHADERTYPE>(999);
    unsafe.blendMode = static_cast<wi::enums::BLENDMODE>(999);
    unsafe.normalMapStrength = 100.0f;
    unsafe.alphaRef = -4.0f;
    unsafe.texMulAdd = XMFLOAT4(0.0f, 100.0f, 5000.0f, -5000.0f);
    unsafe.anisotropyRotationDegrees = -90.0f;
    unsafe.clearcoat = 5.0f;
    unsafe.blendWithTerrainHeight = 9.0f;
    const auto safe = SanitizeMaterialState(unsafe);
    if (safe.shaderType != Material::SHADERTYPE_PBR ||
        safe.blendMode != wi::enums::BLENDMODE_OPAQUE ||
        !Near(safe.normalMapStrength, 4.0f) || !Near(safe.alphaRef, 0.0f) ||
        !Near(safe.texMulAdd.x, 0.01f) || !Near(safe.texMulAdd.y, 10.0f) ||
        !Near(safe.texMulAdd.z, 1000.0f) || !Near(safe.texMulAdd.w, -1000.0f) ||
        !Near(safe.anisotropyRotationDegrees, 270.0f) ||
        !Near(safe.clearcoat, 1.0f) ||
        !Near(safe.blendWithTerrainHeight, 2.0f))
    {
        Fail("extended material sanitization failed");
    }

    // Texture removal is independent of texture decoding: seed one native
    // slot plus governed metadata, clear it through CommandService, then Undo.
    const std::string governedId = "11111111-1111-4111-8111-111111111111";
    material->textures[Material::NORMALMAP].name = "normal.rasset";
    auto& metadata = scene.metadatas.Create(materialA);
    metadata.int_values.set(
        MaterialTextureAssetBindingVersionMetadataKey,
        MaterialTextureAssetBindingVersion);
    metadata.string_values.set(
        MaterialNormalTextureAssetIdMetadataKey,
        governedId);

    if (!commands.Execute(std::make_unique<ClearMaterialTextureAssetCommand>(
            scene, materialA, MaterialTextureSlot::Normal)))
    {
        Fail("texture clear command did not execute");
    }
    if (!material->textures[Material::NORMALMAP].name.empty())
        Fail("native texture slot was not cleared");
    const auto* clearedMetadata = scene.metadatas.GetComponent(materialA);
    if (clearedMetadata == nullptr ||
        clearedMetadata->string_values.has(MaterialNormalTextureAssetIdMetadataKey))
    {
        Fail("governed texture stable-ID metadata was not cleared");
    }
    if (!commands.Undo())
        Fail("texture clear Undo failed");
    if (material->textures[Material::NORMALMAP].name != "normal.rasset")
        Fail("texture clear Undo did not restore native slot");
    const auto* restoredMetadata = scene.metadatas.GetComponent(materialA);
    if (restoredMetadata == nullptr ||
        !restoredMetadata->string_values.has(MaterialNormalTextureAssetIdMetadataKey) ||
        restoredMetadata->string_values.get(MaterialNormalTextureAssetIdMetadataKey) !=
            governedId)
    {
        Fail("texture clear Undo did not restore governed metadata");
    }
    if (!commands.Redo())
        Fail("texture clear Redo failed");

    std::cout << "PHASE5 GATE4 MATERIAL PASS\n";
    return EXIT_SUCCESS;
}
