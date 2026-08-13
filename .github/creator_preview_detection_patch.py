from pathlib import Path
p=Path('Studio/src/StudioApplication.cpp')
s=p.read_text(encoding='utf-8')
def r(old,new,label):
    global s
    if old not in s: raise SystemExit(label+' marker missing')
    s=s.replace(old,new,1)

marker='''    void RefreshCreatorImportTextureEditor()\n    {\n'''
helper=r'''    void ApplyDetectedCreatorPreviewMaterials()
    {
        auto* session = renegade::bridge::StudioSession::Current();
        if (session == nullptr)
            return;
        auto& scene = session->Scenes().GetScene();
        for (const auto& detected : creatorModelImporter.materialOverrides)
        {
            if (detected.materialIndex >= creatorModelImporter.materialEntities.size())
                continue;
            auto* material = scene.materials.GetComponent(
                creatorModelImporter.materialEntities[detected.materialIndex]);
            if (material == nullptr)
                continue;
            const auto apply = [material](
                const renegade::bridge::CreatorTextureSourceChoice& choice,
                const int slot)
            {
                if (!choice.overridden || choice.path.empty())
                    return;
                wi::Resource resource = wi::resourcemanager::Load(choice.path);
                if (!resource.IsValid())
                    return;
                auto& texture = material->textures[slot];
                texture.name = choice.path;
                texture.resource = std::move(resource);
                material->SetDirty();
            };
            apply(detected.baseColor, wi::scene::MaterialComponent::BASECOLORMAP);
            apply(detected.normal, wi::scene::MaterialComponent::NORMALMAP);
            apply(detected.surface, wi::scene::MaterialComponent::SURFACEMAP);
            apply(detected.occlusion, wi::scene::MaterialComponent::OCCLUSIONMAP);
            apply(detected.emissive, wi::scene::MaterialComponent::EMISSIVEMAP);
        }
    }

'''
r(marker,helper+marker,'preview apply helper')

old='''                        creatorModelImporter.automaticScale = bridge::ImportService::ResolveScaleFactor(\n                            bridge::ModelScaleMode::Automatic, *isolated);\n\n                        const std::size_t materialStart = liveScene.materials.GetCount();\n'''
new='''                        creatorModelImporter.automaticScale = bridge::ImportService::ResolveScaleFactor(\n                            bridge::ModelScaleMode::Automatic, *isolated);\n                        const auto detectedMaterials = bridge::DetectCreatorModelMaterials(\n                            *isolated, state->sourcePath);\n                        if (detectedMaterials.succeeded)\n                            creatorModelImporter.materialOverrides = detectedMaterials.materials;\n\n                        const std::size_t materialStart = liveScene.materials.GetCount();\n'''
r(old,new,'detect before preview')

old='''                        for (std::size_t index = materialStart; index < liveScene.materials.GetCount(); ++index)\n                            creatorModelImporter.materialEntities.push_back(liveScene.materials.GetEntity(index));\n                        for (std::size_t index = animationStart; index < liveScene.animations.GetCount(); ++index)\n'''
new='''                        for (std::size_t index = materialStart; index < liveScene.materials.GetCount(); ++index)\n                            creatorModelImporter.materialEntities.push_back(liveScene.materials.GetEntity(index));\n                        ApplyDetectedCreatorPreviewMaterials();\n                        for (std::size_t index = animationStart; index < liveScene.animations.GetCount(); ++index)\n'''
r(old,new,'apply detected after placement')

p.write_text(s,encoding='utf-8')
