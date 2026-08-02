#include <cmath>
#include <iostream>
#include <memory>

#include "renegade/bridge/MaterialService.h"

namespace
{
    bool NearlyEqual(const float left, const float right)
    {
        return std::abs(left - right) < 0.001f;
    }

    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }
}

int main()
{
    wi::scene::Scene scene;
    const auto entity = wi::ecs::CreateEntity();
    auto& material = scene.materials.Create(entity);
    material.baseColor = XMFLOAT4(0.75f, 0.70f, 0.65f, 0.42f);
    material.metalness = 0.1f;
    material.roughness = 0.8f;
    material.reflectance = 0.2f;
    material.emissiveColor = XMFLOAT4(0.1f, 0.2f, 0.3f, 1.5f);

    // Sentinels prove a curated edit does not become a whole-component copy.
    material.SetCastShadow(false);
    material.SetReceiveShadow(false);
    material.normalMapStrength = 2.25f;
    material.transmission = 0.33f;
    material.alphaRef = 0.71f;
    material.shaderType = wi::scene::MaterialComponent::SHADERTYPE_PBR_CLEARCOAT;
    material.customShaderID = 27;
    material.textures[wi::scene::MaterialComponent::BASECOLORMAP].name =
        "authored/albedo.png";
    material.textures[wi::scene::MaterialComponent::BASECOLORMAP].uvset = 1;
    material.SetDirty(false);

    const auto before = renegade::bridge::CaptureMaterial(material);
    auto after = before;
    after.baseColor = XMFLOAT4(0.02f, 0.03f, 0.04f, before.baseColor.w);
    after.metalness = 0.75f;
    after.roughness = 0.25f;
    after.reflectance = 0.55f;
    after.emissiveColor = XMFLOAT3(0.0f, 0.75f, 1.0f);
    after.emissiveStrength = 2.5f;

    renegade::bridge::CommandService commands;
    if (!commands.Execute(
            std::make_unique<renegade::bridge::SetMaterialCommand>(
                scene,
                entity,
                before,
                after)))
    {
        return Fail("material command did not execute");
    }
    if (!NearlyEqual(material.baseColor.x, 0.02f) ||
        !NearlyEqual(material.baseColor.w, 0.42f) ||
        !NearlyEqual(material.metalness, 0.75f) ||
        !NearlyEqual(material.roughness, 0.25f) ||
        !NearlyEqual(material.reflectance, 0.55f) ||
        !NearlyEqual(material.emissiveColor.z, 1.0f) ||
        !NearlyEqual(material.GetEmissiveStrength(), 2.5f) ||
        !material.IsDirty())
    {
        return Fail("curated material state did not reach the native component");
    }
    if (material.IsCastingShadow() || material.IsReceiveShadow() ||
        !NearlyEqual(material.normalMapStrength, 2.25f) ||
        !NearlyEqual(material.transmission, 0.33f) ||
        !NearlyEqual(material.alphaRef, 0.71f) ||
        material.shaderType !=
            wi::scene::MaterialComponent::SHADERTYPE_PBR_CLEARCOAT ||
        material.customShaderID != 27 ||
        material.textures[
            wi::scene::MaterialComponent::BASECOLORMAP].name !=
            "authored/albedo.png" ||
        material.textures[
            wi::scene::MaterialComponent::BASECOLORMAP].uvset != 1)
    {
        return Fail("material edit overwrote an unexposed Wicked field");
    }

    if (!commands.Undo() ||
        !NearlyEqual(material.baseColor.x, 0.75f) ||
        !NearlyEqual(material.baseColor.w, 0.42f) ||
        !NearlyEqual(material.GetEmissiveStrength(), 1.5f))
    {
        return Fail("material Undo did not restore the curated state");
    }
    if (!commands.Redo() ||
        !NearlyEqual(material.baseColor.x, 0.02f) ||
        !NearlyEqual(material.GetEmissiveStrength(), 2.5f))
    {
        return Fail("material Redo did not restore the edited state");
    }

    material.SetDirty(false);
    const auto current = renegade::bridge::CaptureMaterial(material);
    renegade::bridge::CommandService noOpCommands;
    if (noOpCommands.Execute(
            std::make_unique<renegade::bridge::SetMaterialCommand>(
                scene,
                entity,
                current,
                current)) ||
        material.IsDirty() || noOpCommands.UndoCount() != 0 ||
        noOpCommands.IsDirty())
    {
        return Fail("identical material state dirtied data or history");
    }

    // Direct and one-shared-subset targets are editable.
    if (renegade::bridge::ResolveEditableMaterialEntity(scene, entity) !=
        entity)
    {
        return Fail("direct material target did not resolve");
    }
    const auto objectEntity = wi::ecs::CreateEntity();
    const auto meshEntity = wi::ecs::CreateEntity();
    scene.objects.Create(objectEntity).meshID = meshEntity;
    auto& mesh = scene.meshes.Create(meshEntity);
    mesh.subsets.emplace_back().materialID = entity;
    mesh.subsets.emplace_back().materialID = entity;
    if (renegade::bridge::ResolveEditableMaterialEntity(
            scene,
            objectEntity) != entity)
    {
        return Fail("one shared subset material did not resolve");
    }

    // Multiple distinct subset materials are deliberately ambiguous.
    const auto secondMaterial = wi::ecs::CreateEntity();
    scene.materials.Create(secondMaterial);
    mesh.subsets.back().materialID = secondMaterial;
    if (renegade::bridge::ResolveEditableMaterialEntity(
            scene,
            objectEntity) != wi::ecs::INVALID_ENTITY)
    {
        return Fail("multi-material object silently selected one subset");
    }

    // Terrain ownership is rejected by both resolution and the command.
    const auto terrainEntity = wi::ecs::CreateEntity();
    auto& terrain = scene.terrains.Create(terrainEntity);
    terrain.terrainEntity = terrainEntity;
    const auto terrainMaterial = wi::ecs::CreateEntity();
    auto& terrainSurface = scene.materials.Create(terrainMaterial);
    terrain.materialEntities.push_back(terrainMaterial);
    const auto terrainBefore =
        renegade::bridge::CaptureMaterial(terrainSurface);
    auto terrainAfter = terrainBefore;
    terrainAfter.roughness = 0.1f;
    renegade::bridge::CommandService terrainCommands;
    if (!renegade::bridge::IsTerrainOwnedMaterial(
            scene,
            terrainMaterial) ||
        renegade::bridge::ResolveEditableMaterialEntity(
            scene,
            terrainMaterial) != wi::ecs::INVALID_ENTITY ||
        terrainCommands.Execute(
            std::make_unique<renegade::bridge::SetMaterialCommand>(
                scene,
                terrainMaterial,
                terrainBefore,
                terrainAfter)) ||
        terrainSurface.IsDirty() || terrainCommands.UndoCount() != 0)
    {
        return Fail("generic material path accepted terrain-owned material");
    }

    auto removedAfter = current;
    removedAfter.roughness = 0.9f;
    auto removed = std::make_unique<renegade::bridge::SetMaterialCommand>(
        scene,
        entity,
        current,
        removedAfter);
    scene.materials.Remove(entity);
    renegade::bridge::CommandService removedCommands;
    if (removedCommands.Execute(std::move(removed)) ||
        removedCommands.UndoCount() != 0)
    {
        return Fail("removed material produced a command-history entry");
    }

    std::cout << "PASS: material resolution, terrain safety, preservation and Undo/Redo\n";
    return 0;
}
