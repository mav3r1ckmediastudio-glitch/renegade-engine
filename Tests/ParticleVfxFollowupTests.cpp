#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ParticleBlendModeService.h"
#include "renegade/bridge/ParticleEffectLibraryService.h"
#include "renegade/bridge/ParticleEffectService.h"

#include <WickedEngine.h>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    int Fail(const std::string& message)
    {
        std::cerr << "Particle VFX follow-up test failed: " << message << '\n';
        return 1;
    }

    bool Near(const float left, const float right)
    {
        return std::abs(left - right) < 0.0001f;
    }

    bool Exists(const wi::scene::Scene& scene, const wi::ecs::Entity entity)
    {
        wi::unordered_set<wi::ecs::Entity> all;
        scene.FindAllEntities(all);
        return all.count(entity) != 0;
    }
}

int main()
{
    using namespace renegade::bridge;
    namespace fs = std::filesystem;

    wi::scene::Scene scene;
    CreateParticleEffectCommand create(
        scene, XMFLOAT3(4.0f, 5.0f, 6.0f), "Bonfire");
    if (!create.Execute())
        return Fail("could not create Particle Effect root and first native emitter");

    const auto root = create.RootEntity();
    const auto first = create.FirstLayerEntity();
    if (!IsParticleEffectRoot(scene, root) || !IsParticleEffectLayer(scene, first))
        return Fail("effect/layer metadata contract was not authored");
    if (ParticleEffectRootForLayer(scene, first) != root)
        return Fail("first layer did not resolve its effect root");
    const auto* firstHierarchy = scene.hierarchy.GetComponent(first);
    if (firstHierarchy == nullptr || firstHierarchy->parentID != root)
        return Fail("first native Wicked emitter was not parented under the effect root");

    const auto rootId = ReadPersistentEntityId(scene, root);
    const auto firstId = ReadPersistentEntityId(scene, first);
    if (rootId.empty() || firstId.empty() || rootId == firstId)
        return Fail("effect root/layer did not receive distinct persistent identities");

    SetParticleBlendModeCommand additive(
        scene, first, wi::enums::BLENDMODE_ADDITIVE);
    if (!additive.Execute() ||
        CaptureParticleBlendMode(scene, first) != wi::enums::BLENDMODE_ADDITIVE)
        return Fail("native Wicked additive blend mode did not apply");
    additive.Undo();
    if (CaptureParticleBlendMode(scene, first) == wi::enums::BLENDMODE_ADDITIVE)
        return Fail("blend-mode Undo did not restore the previous material state");
    if (!additive.Execute())
        return Fail("blend-mode Redo could not reapply additive blending");

    ParticleEmitterState smoke = MakeNewParticleEmitterState();
    smoke.framesX = 8;
    smoke.framesY = 8;
    smoke.frameCount = 64;
    smoke.frameRate = 24.0f;
    smoke.life = 4.0f;
    TransformState smokeLocal;
    smokeLocal.translation = XMFLOAT3(0.25f, 1.5f, -0.1f);

    AddParticleEffectLayerCommand addSmoke(scene, root, smoke, smokeLocal);
    if (!addSmoke.Execute())
        return Fail("could not add second native Wicked emitter layer");
    const auto second = addSmoke.CreatedLayer();
    if (!IsParticleEffectLayer(scene, second) ||
        ParticleEffectRootForLayer(scene, second) != root)
        return Fail("second layer was not governed by the effect root");
    const auto* secondTransform = scene.transforms.GetComponent(second);
    if (secondTransform == nullptr ||
        !Near(secondTransform->translation_local.x, smokeLocal.translation.x) ||
        !Near(secondTransform->translation_local.y, smokeLocal.translation.y) ||
        !Near(secondTransform->translation_local.z, smokeLocal.translation.z))
        return Fail("second layer exact local offset was not preserved");
    const auto secondId = ReadPersistentEntityId(scene, second);
    if (secondId.empty() || secondId == firstId || secondId == rootId)
        return Fail("second layer did not receive fresh persistent identity");

    auto layers = CollectParticleEffectLayers(scene, root);
    if (layers.size() != 2)
        return Fail("effect did not enumerate both emitter layers");

    addSmoke.Undo();
    if (Exists(scene, second))
        return Fail("layer Undo did not remove the added emitter");
    if (!addSmoke.Execute() || !Exists(scene, second) ||
        ReadPersistentEntityId(scene, second) != secondId)
        return Fail("layer Redo did not restore the exact emitter identity");

    const fs::path libraryRoot =
        fs::temp_directory_path() / "renegade_particle_vfx_followup_test";
    std::error_code cleanupError;
    fs::remove_all(libraryRoot, cleanupError);

    const auto saved = SaveParticleEffectToLibrary(
        scene,
        root,
        "unused-project-root",
        "00000000-0000-4000-8000-000000000001",
        libraryRoot.generic_u8string(),
        "Bonfire Reusable");
    if (!saved.succeeded || saved.layerCount != 2 || saved.packagePath.empty())
        return Fail("multi-emitter effect could not be saved as a reusable preset: " + saved.error);

    ParticleEffectPreset preset;
    std::string loadError;
    if (!LoadParticleEffectPreset(saved.packagePath, preset, loadError))
        return Fail("saved reusable effect could not be loaded: " + loadError);
    if (preset.name != "Bonfire Reusable" || preset.layers.size() != 2)
        return Fail("reusable effect manifest lost its name/layer count");

    bool foundAnimatedSmoke = false;
    bool foundAdditiveLayer = false;
    for (const auto& layer : preset.layers)
    {
        foundAnimatedSmoke = foundAnimatedSmoke ||
            (layer.emitter.framesX == 8 && layer.emitter.framesY == 8 &&
                layer.emitter.frameCount == 64 && Near(layer.emitter.frameRate, 24.0f));
        foundAdditiveLayer = foundAdditiveLayer ||
            layer.blendMode == wi::enums::BLENDMODE_ADDITIVE;
    }
    if (!foundAnimatedSmoke)
        return Fail("preset did not preserve native Wicked spritesheet settings");
    if (!foundAdditiveLayer)
        return Fail("preset did not preserve native Wicked blend mode");

    std::string warning;
    const auto entries = ListParticleEffectLibrary(
        libraryRoot.generic_u8string(), warning);
    if (entries.size() != 1 || entries.front().name != "Bonfire Reusable" ||
        entries.front().layerCount != 2)
        return Fail("user particle-effect library did not enumerate saved preset");

    fs::remove_all(libraryRoot, cleanupError);

    create.Undo();
    if (Exists(scene, root) || Exists(scene, first) || Exists(scene, second))
        return Fail("effect Undo did not remove the root hierarchy");
    if (!create.Execute() || !Exists(scene, root) || !Exists(scene, first))
        return Fail("effect Redo did not restore root/first layer");

    return 0;
}
