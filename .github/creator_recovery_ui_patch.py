from pathlib import Path

p = Path('Studio/src/StudioApplication.cpp')
s = p.read_text(encoding='utf-8')

def replace_once(old, new, label):
    global s
    if old not in s:
        raise SystemExit(f'{label}: marker not found')
    s = s.replace(old, new, 1)

replace_once(
'''#include "renegade/bridge/CreatorAssetWorkflowService.h"\n#include "renegade/bridge/ReusableAssetInstanceService.h"\n''',
'''#include "renegade/bridge/CreatorAssetWorkflowService.h"\n#include "renegade/bridge/CreatorModelImportRecipe.h"\n#include "renegade/bridge/CreatorModelMaterialPreparationService.h"\n#include "renegade/bridge/ReusableAssetInstanceService.h"\n''',
'includes')

replace_once(
'''        std::vector<wi::ecs::Entity> materialEntities;\n        std::vector<wi::ecs::Entity> animationEntities;\n        std::size_t selectedMaterial = 0;\n''',
'''        std::vector<wi::ecs::Entity> materialEntities;\n        std::vector<wi::ecs::Entity> animationEntities;\n        std::vector<renegade::bridge::CreatorMaterialSourceOverride> materialOverrides;\n        std::vector<renegade::bridge::CreatorAnimationImportRecipe> animationRecipe;\n        std::size_t selectedMaterial = 0;\n''',
'workspace recipe state')

replace_once(
'''    wi::gui::Label creatorImportMaterialLabel;\n    wi::gui::Label creatorImportMaterialReadout;\n    wi::gui::Label creatorImportAnimationLabel;\n''',
'''    wi::gui::Label creatorImportMaterialLabel;\n    wi::gui::Label creatorImportMaterialReadout;\n    wi::gui::Label creatorImportTextureHelp;\n    renegade::studio::RenegadeComboBox creatorImportTextureSlotCombo;\n    renegade::studio::RenegadeTextInputField creatorImportTexturePath;\n    renegade::studio::RenegadeButton creatorImportTextureBrowse;\n    renegade::studio::RenegadeButton creatorImportTextureClear;\n    std::size_t creatorImportTextureSlot = 0;\n    wi::gui::Label creatorImportAnimationLabel;\n    renegade::studio::RenegadeTextInputField creatorImportAnimationName;\n    renegade::studio::RenegadeTextInputField creatorImportAnimationStart;\n    renegade::studio::RenegadeTextInputField creatorImportAnimationEnd;\n    renegade::studio::RenegadeComboBox creatorImportAnimationEnabled;\n    renegade::studio::RenegadeButton creatorImportAnimationAdd;\n    renegade::studio::RenegadeButton creatorImportAnimationDelete;\n''',
'global importer controls')

helper_marker = '''    void RefreshCreatorImportMaterialReadout()\n    {\n'''
helper_code = r'''    renegade::bridge::CreatorMaterialSourceOverride& EnsureCreatorMaterialOverride()
    {
        const std::uint32_t materialIndex = static_cast<std::uint32_t>(
            creatorModelImporter.selectedMaterial);
        auto found = std::find_if(
            creatorModelImporter.materialOverrides.begin(),
            creatorModelImporter.materialOverrides.end(),
            [materialIndex](const renegade::bridge::CreatorMaterialSourceOverride& value)
            { return value.materialIndex == materialIndex; });
        if (found == creatorModelImporter.materialOverrides.end())
        {
            renegade::bridge::CreatorMaterialSourceOverride created;
            created.materialIndex = materialIndex;
            creatorModelImporter.materialOverrides.push_back(std::move(created));
            return creatorModelImporter.materialOverrides.back();
        }
        return *found;
    }

    renegade::bridge::CreatorTextureSourceChoice& SelectedCreatorTextureChoice()
    {
        auto& material = EnsureCreatorMaterialOverride();
        switch (creatorImportTextureSlot)
        {
        case 0: return material.baseColor;
        case 1: return material.normal;
        case 2: return material.surface;
        case 3: return material.roughness;
        case 4: return material.metalness;
        case 5: return material.occlusion;
        default: return material.emissive;
        }
    }

    void RefreshCreatorImportTextureEditor()
    {
        if (creatorModelImporter.materialEntities.empty())
        {
            creatorImportTexturePath.SetValue("");
            return;
        }
        const std::uint32_t materialIndex = static_cast<std::uint32_t>(
            creatorModelImporter.selectedMaterial);
        const auto found = std::find_if(
            creatorModelImporter.materialOverrides.begin(),
            creatorModelImporter.materialOverrides.end(),
            [materialIndex](const renegade::bridge::CreatorMaterialSourceOverride& value)
            { return value.materialIndex == materialIndex; });
        if (found == creatorModelImporter.materialOverrides.end())
        {
            creatorImportTexturePath.SetValue("<AUTO // imported binding or filename suffix>");
            return;
        }
        const renegade::bridge::CreatorTextureSourceChoice* choice = nullptr;
        switch (creatorImportTextureSlot)
        {
        case 0: choice = &found->baseColor; break;
        case 1: choice = &found->normal; break;
        case 2: choice = &found->surface; break;
        case 3: choice = &found->roughness; break;
        case 4: choice = &found->metalness; break;
        case 5: choice = &found->occlusion; break;
        default: choice = &found->emissive; break;
        }
        if (!choice->overridden)
            creatorImportTexturePath.SetValue("<AUTO // imported binding or filename suffix>");
        else if (choice->path.empty())
            creatorImportTexturePath.SetValue("<REMOVED>");
        else
            creatorImportTexturePath.SetValue(choice->path);
    }

    void ApplyCreatorPreviewTextureChoice(const std::string& path)
    {
        auto* session = renegade::bridge::StudioSession::Current();
        if (session == nullptr || creatorModelImporter.materialEntities.empty())
            return;
        auto& scene = session->Scenes().GetScene();
        const auto entity = creatorModelImporter.materialEntities[
            creatorModelImporter.selectedMaterial];
        auto* material = scene.materials.GetComponent(entity);
        if (material == nullptr)
            return;
        int wickedSlot = -1;
        switch (creatorImportTextureSlot)
        {
        case 0: wickedSlot = wi::scene::MaterialComponent::BASECOLORMAP; break;
        case 1: wickedSlot = wi::scene::MaterialComponent::NORMALMAP; break;
        case 2: wickedSlot = wi::scene::MaterialComponent::SURFACEMAP; break;
        case 5: wickedSlot = wi::scene::MaterialComponent::OCCLUSIONMAP; break;
        case 6: wickedSlot = wi::scene::MaterialComponent::EMISSIVEMAP; break;
        default: break;
        }
        if (wickedSlot < 0)
            return; // roughness/metalness are packed into Surface at commit.
        auto& texture = material->textures[wickedSlot];
        if (path.empty())
        {
            texture.name.clear();
            texture.resource = {};
            material->SetDirty();
            return;
        }
        wi::Resource resource = wi::resourcemanager::Load(path);
        if (!resource.IsValid())
            return;
        texture.name = path;
        texture.resource = std::move(resource);
        material->SetDirty();
    }

    void OpenCreatorImportTextureBrowser()
    {
        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::OPEN;
        params.description = "Texture source for the selected imported material slot";
        for (const char* extension : {"png", "jpg", "jpeg", "tga", "bmp", "dds", "hdr"})
            params.extensions.push_back(extension);
        wi::helper::FileDialog(params, [](const std::string& path)
        {
            wi::eventhandler::Subscribe_Once(
                wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                [path](std::uint64_t)
                {
                    if (path.empty() || !creatorModelImporter.active)
                        return;
                    auto& choice = SelectedCreatorTextureChoice();
                    choice.overridden = true;
                    choice.path = path;
                    ApplyCreatorPreviewTextureChoice(path);
                    RefreshCreatorImportTextureEditor();
                    RefreshCreatorImportMaterialReadout();
                });
        });
    }

    void RefreshCreatorImportAnimationEditor()
    {
        if (creatorModelImporter.animationRecipe.empty())
        {
            creatorImportAnimationName.SetValue("");
            creatorImportAnimationStart.SetValue(0.0f);
            creatorImportAnimationEnd.SetValue(0.0f);
            creatorImportAnimationEnabled.SetSelectedWithoutCallback(-1);
            creatorImportAnimationReadout.SetText("No animation actions detected.");
            return;
        }
        creatorModelImporter.selectedAnimation = std::min(
            creatorModelImporter.selectedAnimation,
            creatorModelImporter.animationRecipe.size() - 1);
        const auto& clip = creatorModelImporter.animationRecipe[
            creatorModelImporter.selectedAnimation];
        creatorImportAnimationName.SetValue(clip.name);
        creatorImportAnimationStart.SetValue(clip.start);
        creatorImportAnimationEnd.SetValue(clip.end);
        creatorImportAnimationEnabled.SetSelectedWithoutCallback(clip.enabled ? 0 : 1);
        std::ostringstream out;
        out.precision(3);
        out << std::fixed << "Source action " << clip.sourceAnimationIndex + 1
            << " // " << clip.start << " - " << clip.end
            << " // " << (clip.enabled ? "INCLUDED" : "EXCLUDED");
        creatorImportAnimationReadout.SetText(out.str());
    }

    void RebuildCreatorImportAnimationCombo()
    {
        creatorImportAnimationCombo.ClearItems();
        for (std::size_t index = 0; index < creatorModelImporter.animationRecipe.size(); ++index)
        {
            const auto& clip = creatorModelImporter.animationRecipe[index];
            creatorImportAnimationCombo.AddItem(
                clip.name.empty() ? "Animation " + std::to_string(index + 1) : clip.name,
                static_cast<std::uint64_t>(index));
        }
        if (!creatorModelImporter.animationRecipe.empty())
        {
            creatorModelImporter.selectedAnimation = std::min(
                creatorModelImporter.selectedAnimation,
                creatorModelImporter.animationRecipe.size() - 1);
            creatorImportAnimationCombo.SetSelectedWithoutCallback(
                static_cast<int>(creatorModelImporter.selectedAnimation));
        }
        RefreshCreatorImportAnimationEditor();
    }

'''
replace_once(helper_marker, helper_code + helper_marker, 'importer helper functions')

replace_once(
'''            RefreshCreatorImportMaterialReadout();\n        });\n        creatorImportMaterialReadout.Create("");\n        creatorImportMaterialReadout.SetFitTextEnabled(true);\n\n        creatorImportAnimationLabel.Create("ANIMATIONS // SOURCE ACTIONS");\n''',
'''            RefreshCreatorImportMaterialReadout();\n            RefreshCreatorImportTextureEditor();\n        });\n        creatorImportMaterialReadout.Create("");\n        creatorImportMaterialReadout.SetFitTextEnabled(true);\n        creatorImportTextureHelp.Create("TEXTURE SLOT // AUTO-DETECT, REPLACE OR REMOVE");\n        creatorImportTextureSlotCombo.Create("Texture Slot");\n        for (const char* name : {"BASE COLOR", "NORMAL", "SURFACE (PACKED)", "ROUGHNESS", "METALNESS", "AO", "EMISSIVE"})\n            creatorImportTextureSlotCombo.AddItem(name);\n        creatorImportTextureSlotCombo.SetSelectedWithoutCallback(0);\n        creatorImportTextureSlotCombo.OnSelect([](const wi::gui::EventArgs& args)\n        {\n            creatorImportTextureSlot = static_cast<std::size_t>(std::max(0, args.iValue));\n            RefreshCreatorImportTextureEditor();\n        });\n        creatorImportTexturePath.Create("Texture Source Path");\n        creatorImportTexturePath.SetPlaceholder("AUTO-DETECT");\n        creatorImportTexturePath.OnInputAccepted([](const wi::gui::EventArgs& args)\n        {\n            if (!creatorModelImporter.active) return;\n            auto& choice = SelectedCreatorTextureChoice();\n            choice.overridden = true;\n            choice.path = args.sValue;\n            ApplyCreatorPreviewTextureChoice(choice.path);\n            RefreshCreatorImportTextureEditor();\n            RefreshCreatorImportMaterialReadout();\n        });\n        creatorImportTextureBrowse.Create("Browse Imported Material Texture");\n        creatorImportTextureBrowse.SetText("BROWSE...");\n        creatorImportTextureBrowse.OnClick([](const wi::gui::EventArgs&)\n        {\n            OpenCreatorImportTextureBrowser();\n        });\n        creatorImportTextureClear.Create("Clear Imported Material Texture");\n        creatorImportTextureClear.SetText("REMOVE");\n        creatorImportTextureClear.OnClick([](const wi::gui::EventArgs&)\n        {\n            if (!creatorModelImporter.active) return;\n            auto& choice = SelectedCreatorTextureChoice();\n            choice.overridden = true;\n            choice.path.clear();\n            ApplyCreatorPreviewTextureChoice({});\n            RefreshCreatorImportTextureEditor();\n            RefreshCreatorImportMaterialReadout();\n        });\n\n        creatorImportAnimationLabel.Create("ANIMATIONS // EDITABLE CLIPS");\n''',
'material editing UI')

replace_once(
'''        creatorImportAnimationCombo.OnSelect([](const wi::gui::EventArgs& args)\n        {\n            creatorModelImporter.selectedAnimation =\n                static_cast<std::size_t>(args.userdata);\n            RefreshCreatorImportAnimationReadout();\n        });\n        creatorImportAnimationReadout.Create("");\n        creatorImportAnimationReadout.SetFitTextEnabled(true);\n\n        importScaleApplyButton_.Create("Import Model Commit");\n''',
'''        creatorImportAnimationCombo.OnSelect([](const wi::gui::EventArgs& args)\n        {\n            creatorModelImporter.selectedAnimation =\n                static_cast<std::size_t>(args.userdata);\n            RefreshCreatorImportAnimationEditor();\n        });\n        creatorImportAnimationName.Create("Animation Clip Name");\n        creatorImportAnimationName.SetPlaceholder("CLIP NAME");\n        creatorImportAnimationName.OnInputAccepted([](const wi::gui::EventArgs& args)\n        {\n            if (creatorModelImporter.animationRecipe.empty()) return;\n            creatorModelImporter.animationRecipe[creatorModelImporter.selectedAnimation].name = args.sValue;\n            RebuildCreatorImportAnimationCombo();\n        });\n        creatorImportAnimationStart.Create("Animation Start");\n        creatorImportAnimationStart.SetDescription("START: ");\n        creatorImportAnimationStart.OnInputAccepted([](const wi::gui::EventArgs& args)\n        {\n            if (creatorModelImporter.animationRecipe.empty()) return;\n            auto& clip = creatorModelImporter.animationRecipe[creatorModelImporter.selectedAnimation];\n            clip.start = std::min(args.fValue, clip.end);\n            RefreshCreatorImportAnimationEditor();\n        });\n        creatorImportAnimationEnd.Create("Animation End");\n        creatorImportAnimationEnd.SetDescription("END: ");\n        creatorImportAnimationEnd.OnInputAccepted([](const wi::gui::EventArgs& args)\n        {\n            if (creatorModelImporter.animationRecipe.empty()) return;\n            auto& clip = creatorModelImporter.animationRecipe[creatorModelImporter.selectedAnimation];\n            clip.end = std::max(args.fValue, clip.start);\n            RefreshCreatorImportAnimationEditor();\n        });\n        creatorImportAnimationEnabled.Create("Animation Included");\n        creatorImportAnimationEnabled.AddItem("INCLUDE");\n        creatorImportAnimationEnabled.AddItem("EXCLUDE");\n        creatorImportAnimationEnabled.OnSelect([](const wi::gui::EventArgs& args)\n        {\n            if (creatorModelImporter.animationRecipe.empty()) return;\n            creatorModelImporter.animationRecipe[creatorModelImporter.selectedAnimation].enabled = args.iValue == 0;\n            RefreshCreatorImportAnimationEditor();\n        });\n        creatorImportAnimationAdd.Create("Add Animation Clip");\n        creatorImportAnimationAdd.SetText("ADD CLIP");\n        creatorImportAnimationAdd.OnClick([](const wi::gui::EventArgs&)\n        {\n            if (creatorModelImporter.animationRecipe.empty()) return;\n            auto clip = creatorModelImporter.animationRecipe[creatorModelImporter.selectedAnimation];\n            clip.name = clip.name.empty()\n                ? "Clip " + std::to_string(creatorModelImporter.animationRecipe.size() + 1)\n                : clip.name + " Copy";\n            creatorModelImporter.animationRecipe.push_back(std::move(clip));\n            creatorModelImporter.selectedAnimation = creatorModelImporter.animationRecipe.size() - 1;\n            RebuildCreatorImportAnimationCombo();\n        });\n        creatorImportAnimationDelete.Create("Delete Animation Clip");\n        creatorImportAnimationDelete.SetText("DELETE CLIP");\n        creatorImportAnimationDelete.OnClick([](const wi::gui::EventArgs&)\n        {\n            if (creatorModelImporter.animationRecipe.empty()) return;\n            creatorModelImporter.animationRecipe.erase(\n                creatorModelImporter.animationRecipe.begin() +\n                static_cast<std::ptrdiff_t>(creatorModelImporter.selectedAnimation));\n            if (creatorModelImporter.selectedAnimation > 0)\n                --creatorModelImporter.selectedAnimation;\n            RebuildCreatorImportAnimationCombo();\n        });\n        creatorImportAnimationReadout.Create("");\n        creatorImportAnimationReadout.SetFitTextEnabled(true);\n\n        importScaleApplyButton_.Create("Import Model Commit");\n''',
'animation editing UI')

replace_once(
'''            static_cast<wi::gui::Widget*>(&creatorImportMaterialCombo),\n            static_cast<wi::gui::Widget*>(&creatorImportMaterialReadout),\n            static_cast<wi::gui::Widget*>(&creatorImportAnimationLabel),\n            static_cast<wi::gui::Widget*>(&creatorImportAnimationCombo),\n            static_cast<wi::gui::Widget*>(&creatorImportAnimationReadout),\n''',
'''            static_cast<wi::gui::Widget*>(&creatorImportMaterialCombo),\n            static_cast<wi::gui::Widget*>(&creatorImportMaterialReadout),\n            static_cast<wi::gui::Widget*>(&creatorImportTextureHelp),\n            static_cast<wi::gui::Widget*>(&creatorImportTextureSlotCombo),\n            static_cast<wi::gui::Widget*>(&creatorImportTexturePath),\n            static_cast<wi::gui::Widget*>(&creatorImportTextureBrowse),\n            static_cast<wi::gui::Widget*>(&creatorImportTextureClear),\n            static_cast<wi::gui::Widget*>(&creatorImportAnimationLabel),\n            static_cast<wi::gui::Widget*>(&creatorImportAnimationCombo),\n            static_cast<wi::gui::Widget*>(&creatorImportAnimationName),\n            static_cast<wi::gui::Widget*>(&creatorImportAnimationStart),\n            static_cast<wi::gui::Widget*>(&creatorImportAnimationEnd),\n            static_cast<wi::gui::Widget*>(&creatorImportAnimationEnabled),\n            static_cast<wi::gui::Widget*>(&creatorImportAnimationAdd),\n            static_cast<wi::gui::Widget*>(&creatorImportAnimationDelete),\n            static_cast<wi::gui::Widget*>(&creatorImportAnimationReadout),\n''',
'widget list')

replace_once(
'''        const float importScalePanelWidth = 390.0f;\n        const float importScalePanelHeight = 690.0f;\n''',
'''        const float importScalePanelWidth = 440.0f;\n        const float importScalePanelHeight = std::min(850.0f, std::max(690.0f, height - 36.0f));\n''',
'panel size')

replace_once(
'''        creatorImportMaterialReadout.SetPos(XMFLOAT2(12.0f, 360.0f));\n        creatorImportMaterialReadout.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 104.0f));\n        creatorImportAnimationLabel.SetPos(XMFLOAT2(12.0f, 478.0f));\n        creatorImportAnimationLabel.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 22.0f));\n        creatorImportAnimationCombo.SetPos(XMFLOAT2(12.0f, 504.0f));\n        creatorImportAnimationCombo.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));\n        creatorImportAnimationReadout.SetPos(XMFLOAT2(12.0f, 538.0f));\n        creatorImportAnimationReadout.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 70.0f));\n        importScaleApplyButton_.SetPos(XMFLOAT2(12.0f, 624.0f));\n''',
'''        creatorImportMaterialReadout.SetPos(XMFLOAT2(12.0f, 360.0f));\n        creatorImportMaterialReadout.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 94.0f));\n        creatorImportTextureHelp.SetPos(XMFLOAT2(12.0f, 458.0f));\n        creatorImportTextureHelp.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 20.0f));\n        creatorImportTextureSlotCombo.SetPos(XMFLOAT2(12.0f, 482.0f));\n        creatorImportTextureSlotCombo.SetSize(XMFLOAT2(150.0f, 28.0f));\n        creatorImportTexturePath.SetPos(XMFLOAT2(166.0f, 482.0f));\n        creatorImportTexturePath.SetSize(XMFLOAT2(importScalePanelWidth - 178.0f, 28.0f));\n        creatorImportTextureBrowse.SetPos(XMFLOAT2(12.0f, 514.0f));\n        creatorImportTextureBrowse.SetSize(XMFLOAT2(120.0f, 28.0f));\n        creatorImportTextureClear.SetPos(XMFLOAT2(136.0f, 514.0f));\n        creatorImportTextureClear.SetSize(XMFLOAT2(100.0f, 28.0f));\n        creatorImportAnimationLabel.SetPos(XMFLOAT2(12.0f, 554.0f));\n        creatorImportAnimationLabel.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 22.0f));\n        creatorImportAnimationCombo.SetPos(XMFLOAT2(12.0f, 580.0f));\n        creatorImportAnimationCombo.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));\n        creatorImportAnimationName.SetPos(XMFLOAT2(12.0f, 612.0f));\n        creatorImportAnimationName.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 28.0f));\n        creatorImportAnimationStart.SetPos(XMFLOAT2(12.0f, 644.0f));\n        creatorImportAnimationStart.SetSize(XMFLOAT2(120.0f, 28.0f));\n        creatorImportAnimationEnd.SetPos(XMFLOAT2(136.0f, 644.0f));\n        creatorImportAnimationEnd.SetSize(XMFLOAT2(120.0f, 28.0f));\n        creatorImportAnimationEnabled.SetPos(XMFLOAT2(260.0f, 644.0f));\n        creatorImportAnimationEnabled.SetSize(XMFLOAT2(importScalePanelWidth - 272.0f, 28.0f));\n        creatorImportAnimationAdd.SetPos(XMFLOAT2(12.0f, 676.0f));\n        creatorImportAnimationAdd.SetSize(XMFLOAT2(110.0f, 28.0f));\n        creatorImportAnimationDelete.SetPos(XMFLOAT2(126.0f, 676.0f));\n        creatorImportAnimationDelete.SetSize(XMFLOAT2(120.0f, 28.0f));\n        creatorImportAnimationReadout.SetPos(XMFLOAT2(12.0f, 708.0f));\n        creatorImportAnimationReadout.SetSize(XMFLOAT2(importScalePanelWidth - 24.0f, 42.0f));\n        const float commitY = std::min(importScalePanelHeight - 46.0f, 764.0f);\n        importScaleApplyButton_.SetPos(XMFLOAT2(12.0f, commitY));\n''',
'layout expanded controls')

replace_once(
'''            12.0f + (importScalePanelWidth - 24.0f - 8.0f) * 0.5f + 8.0f,\n            234.0f));\n''',
'''            20.0f + (importScalePanelWidth - 32.0f) * 0.64f,\n            commitY));\n''',
'legacy dismiss position if present') if False else None

# Current branch already has the new dismiss layout; only replace its Y literal.
replace_once(
'''        importScaleDismissButton_.SetPos(XMFLOAT2(20.0f + (importScalePanelWidth - 32.0f) * 0.64f, 624.0f));\n''',
'''        importScaleDismissButton_.SetPos(XMFLOAT2(20.0f + (importScalePanelWidth - 32.0f) * 0.64f, commitY));\n''',
'dismiss layout y')

replace_once(
'''                        for (std::size_t index = animationStart; index < liveScene.animations.GetCount(); ++index)\n                            creatorModelImporter.animationEntities.push_back(liveScene.animations.GetEntity(index));\n\n                        auto light = std::make_unique<bridge::CreateLightCommand>(\n''',
'''                        for (std::size_t index = animationStart; index < liveScene.animations.GetCount(); ++index)\n                        {\n                            const auto entity = liveScene.animations.GetEntity(index);\n                            creatorModelImporter.animationEntities.push_back(entity);\n                            const auto* animation = liveScene.animations.GetComponent(entity);\n                            if (animation != nullptr)\n                            {\n                                bridge::CreatorAnimationImportRecipe clip;\n                                clip.sourceAnimationIndex = static_cast<std::uint32_t>(index - animationStart);\n                                clip.name = CreatorImportEntityName(\n                                    liveScene, entity, "Animation " + std::to_string(index - animationStart + 1));\n                                clip.start = animation->start;\n                                clip.end = animation->end;\n                                clip.enabled = true;\n                                creatorModelImporter.animationRecipe.push_back(std::move(clip));\n                            }\n                        }\n\n                        auto light = std::make_unique<bridge::CreateLightCommand>(\n''',
'initialize animation recipe')

replace_once(
'''        RefreshCreatorImportMaterialReadout();\n\n        creatorImportAnimationCombo.ClearItems();\n        for (std::size_t index = 0; index < creatorModelImporter.animationEntities.size(); ++index)\n        {\n            const auto entityId = creatorModelImporter.animationEntities[index];\n            creatorImportAnimationCombo.AddItem(\n                CreatorImportEntityName(scene, entityId, "Animation " + std::to_string(index + 1)),\n                static_cast<std::uint64_t>(index));\n        }\n        if (!creatorModelImporter.animationEntities.empty())\n        {\n            creatorImportAnimationCombo.SetSelectedWithoutCallback(0);\n            creatorModelImporter.selectedAnimation = 0;\n        }\n        RefreshCreatorImportAnimationReadout();\n''',
'''        RefreshCreatorImportMaterialReadout();\n        creatorImportTextureSlot = 0;\n        creatorImportTextureSlotCombo.SetSelectedWithoutCallback(0);\n        RefreshCreatorImportTextureEditor();\n\n        creatorModelImporter.selectedAnimation = 0;\n        RebuildCreatorImportAnimationCombo();\n''',
'show editable animation recipe')

replace_once(
'''        const std::string sourcePath = creatorModelImporter.sourcePath;\n        const auto cameraBefore = creatorModelImporter.cameraBefore;\n        creatorModelImporter.committing = true;\n''',
'''        const std::string sourcePath = creatorModelImporter.sourcePath;\n        const auto cameraBefore = creatorModelImporter.cameraBefore;\n        const auto materialOverrides = creatorModelImporter.materialOverrides;\n        const auto animationRecipe = creatorModelImporter.animationRecipe;\n        creatorModelImporter.committing = true;\n''',
'capture creator edits')

replace_once(
'''            float scale = 1.0f;\n            bridge::CreatorModelImportResult imported;\n            bridge::PreparedReusableModelPlacement prepared;\n''',
'''            float scale = 1.0f;\n            std::vector<bridge::CreatorMaterialSourceOverride> materialOverrides;\n            std::vector<bridge::CreatorAnimationImportRecipe> animationRecipe;\n            std::string settingsJson = "{}";\n            bridge::CreatorModelImportResult imported;\n            bridge::PreparedReusableModelPlacement prepared;\n''',
'commit state recipe fields')

replace_once(
'''        state->rotation = preview.rotation;\n        state->scale = preview.scale.x;\n        studioChrome_.SetStatusText("IMPORT MODEL // COMMITTING GOVERNED ASSET");\n''',
'''        state->rotation = preview.rotation;\n        state->scale = preview.scale.x;\n        state->materialOverrides = materialOverrides;\n        state->animationRecipe = animationRecipe;\n        studioChrome_.SetStatusText("IMPORT MODEL // COMMITTING GOVERNED ASSET");\n''',
'copy recipe to worker state')

replace_once(
'''                bridge::CreatorAssetWorkflowService workflow;\n                state->imported = workflow.ImportModel(state->projectRoot, state->projectId, state->sourcePath);\n                if (state->imported.succeeded)\n''',
'''                const fs::path previewDirectory =\n                    fs::u8path(state->projectRoot) / "Intermediate" / "Imports";\n                std::error_code ec;\n                fs::create_directories(previewDirectory, ec);\n                bridge::ModelImportRequest modelRequest;\n                modelRequest.sourcePath = state->sourcePath;\n                modelRequest.assetPath = (previewDirectory / ".creator-commit.wiscene").generic_u8string();\n                modelRequest.expectedFormat = bridge::ImportService::ClassifyModelSourceFormat(state->sourcePath);\n                auto converted = bridge::ImportService().PrepareModelAsset(modelRequest);\n                if (converted.IsReady())\n                {\n                    bridge::CreatorModelMaterialPreparationRequest materialRequest;\n                    materialRequest.preparedScene = converted.PeekScene();\n                    materialRequest.projectRoot = state->projectRoot;\n                    materialRequest.projectId = state->projectId;\n                    materialRequest.modelSourcePath = state->sourcePath;\n                    materialRequest.overrides = state->materialOverrides;\n                    auto materials = bridge::PrepareCreatorModelMaterials(materialRequest);\n                    if (materials.succeeded)\n                    {\n                        materials.recipe.animations = state->animationRecipe;\n                        std::string recipeError;\n                        if (!bridge::SerializeCreatorModelImportOptions(\n                                materials.recipe, state->settingsJson, recipeError))\n                        {\n                            state->imported.error = recipeError;\n                        }\n                    }\n                    else\n                    {\n                        state->imported.error = materials.error;\n                    }\n                }\n                else\n                {\n                    state->imported.error = converted.Result().error;\n                }\n\n                bridge::CreatorAssetWorkflowService workflow;\n                if (state->imported.error.empty())\n                    state->imported = workflow.ImportModel(\n                        state->projectRoot, state->projectId, state->sourcePath, state->settingsJson);\n                if (state->imported.succeeded)\n''',
'commit governed recipe')

p.write_text(s, encoding='utf-8')
