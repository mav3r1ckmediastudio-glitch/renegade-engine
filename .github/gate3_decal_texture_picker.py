from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    file = Path(path)
    text = file.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one anchor, found {count}: {old[:80]!r}")
    file.write_text(text.replace(old, new, 1), encoding="utf-8")


cpp = "Studio/src/StudioApplication.cpp"
header = "Studio/src/StudioApplication.h"
contract = "Tests/Phase5Gate3SourceContract.cmake"

replace_once(
    cpp,
    '#include "renegade/bridge/CreatorModelMaterialPreparationService.h"\n#include "renegade/bridge/MaterialTextureAssetService.h"',
    '#include "renegade/bridge/CreatorModelMaterialPreparationService.h"\n#include "renegade/bridge/CreatorTextureWorkflowService.h"\n#include "renegade/bridge/MaterialTextureAssetService.h"',
)

replace_once(
    cpp,
    '''        createDecalMaterialSlider(\n            decalOpacity_, "Decal Material Opacity", "OPACITY", 3);\n\n        createSectionLabel(\n            environmentProbeLabel_,''',
    '''        createDecalMaterialSlider(\n            decalOpacity_, "Decal Material Opacity", "OPACITY", 3);\n\n        decalBaseColorTexture_.Create("Decal Base Color Texture");\n        decalBaseColorTexture_.SetText("SELECT DECAL TEXTURE...");\n        decalBaseColorTexture_.SetTooltip(\n            "Choose a local image, import it as a governed Renegade texture, and bind it to this projected decal's base-colour/alpha slot.");\n        decalBaseColorTexture_.OnClick([this](const wi::gui::EventArgs&)\n        {\n            ChooseSelectedDecalTexture();\n        });\n        inspectorPanel_.AddWidget(&decalBaseColorTexture_);\n\n        createSectionLabel(\n            environmentProbeLabel_,''',
)

replace_once(
    cpp,
    '''        positionEnvironmentWidget(decalBaseColorBlue_, 688.0f);\n        positionEnvironmentWidget(decalOpacity_, 722.0f);\n\n        positionEnvironmentWidget(environmentProbeLabel_, 506.0f, 20.0f);''',
    '''        positionEnvironmentWidget(decalBaseColorBlue_, 688.0f);\n        positionEnvironmentWidget(decalOpacity_, 722.0f);\n        positionEnvironmentWidget(decalBaseColorTexture_, 756.0f);\n\n        positionEnvironmentWidget(environmentProbeLabel_, 506.0f, 20.0f);''',
)

replace_once(
    cpp,
    '''        decalBaseColorBlue_.SetVisible(hasDecal && decalMaterial != nullptr);\n        decalOpacity_.SetVisible(hasDecal && decalMaterial != nullptr);\n        if (hasDecal)''',
    '''        decalBaseColorBlue_.SetVisible(hasDecal && decalMaterial != nullptr);\n        decalOpacity_.SetVisible(hasDecal && decalMaterial != nullptr);\n        decalBaseColorTexture_.SetVisible(hasDecal && decalMaterial != nullptr);\n        if (hasDecal && decalMaterial != nullptr)\n        {\n            decalBaseColorTexture_.SetText(\n                decalMaterial->textures[wi::scene::MaterialComponent::BASECOLORMAP]\n                        .resource.IsValid()\n                    ? "CHANGE DECAL TEXTURE..."\n                    : "SELECT DECAL TEXTURE...");\n        }\n        if (hasDecal)''',
)

texture_function = r'''    void StudioRenderPath::ChooseSelectedDecalTexture()
    {
        if (session_ == nullptr || !session_->Projects().HasProject() ||
            wi::jobsystem::IsBusy(decalTextureImportWorkload_))
        {
            return;
        }

        const wi::ecs::Entity decalEntity =
            session_->Selection().SelectedEntity();
        if (!session_->Scenes().GetScene().decals.Contains(decalEntity))
            return;

        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::OPEN;
        params.description = "Projected decal base-colour / alpha texture";
        params.extensions = {
            "png", "tga", "dds", "jpg", "jpeg", "bmp", "hdr"};
        wi::helper::FileDialog(
            params,
            [this, decalEntity](const std::string& sourcePath)
            {
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, decalEntity, sourcePath](std::uint64_t)
                    {
                        if (sourcePath.empty() || session_ == nullptr ||
                            !session_->Projects().HasProject() ||
                            wi::jobsystem::IsBusy(decalTextureImportWorkload_))
                        {
                            return;
                        }

                        auto& scene = session_->Scenes().GetScene();
                        if (!scene.decals.Contains(decalEntity))
                        {
                            studioChrome_.SetStatusText(
                                "DECAL TEXTURE // TARGET NO LONGER EXISTS");
                            return;
                        }

                        const bridge::ResourceSourceFormat format =
                            bridge::DetectResourceSourceFormat(sourcePath);
                        if (format == bridge::ResourceSourceFormat::Unknown ||
                            bridge::ClassifyResourceSourceFormat(format) !=
                                bridge::ResourceClass::Texture)
                        {
                            studioChrome_.SetStatusText(
                                "DECAL TEXTURE // UNSUPPORTED IMAGE FORMAT");
                            wi::helper::messageBox(
                                "Choose a supported image texture (PNG, TGA, DDS, JPG/JPEG, BMP or HDR).",
                                "Select Decal Texture");
                            return;
                        }

                        struct DecalTextureImportState
                        {
                            std::string projectRoot;
                            bridge::StableId projectId;
                            wi::ecs::Entity decalEntity = wi::ecs::INVALID_ENTITY;
                            std::string sourcePath;
                            bridge::CreatorTextureImportResult imported;
                        };

                        auto state = std::make_shared<DecalTextureImportState>();
                        const auto& project =
                            session_->Projects().CurrentProject();
                        state->projectRoot = project.rootPath;
                        state->projectId = project.projectId;
                        state->decalEntity = decalEntity;
                        state->sourcePath = sourcePath;
                        studioChrome_.SetStatusText(
                            "DECAL TEXTURE // IMPORTING + REGISTERING // " +
                            fs::u8path(sourcePath).filename().generic_u8string());

                        wi::jobsystem::Execute(
                            decalTextureImportWorkload_,
                            [this, state](wi::jobsystem::JobArgs)
                            {
                                bridge::CreatorTextureWorkflowService workflow;
                                state->imported = workflow.ImportTexture(
                                    state->projectRoot,
                                    state->projectId,
                                    state->sourcePath);

                                wi::eventhandler::Subscribe_Once(
                                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                                    [this, state](std::uint64_t)
                                    {
                                        if (!state->imported.succeeded)
                                        {
                                            const std::string prefix =
                                                state->imported.committed
                                                    ? "DECAL TEXTURE // IMPORT COMMITTED // VERIFY FAILED // "
                                                    : "DECAL TEXTURE // IMPORT FAILED // ";
                                            studioChrome_.SetStatusText(
                                                prefix + state->imported.error);
                                            wi::helper::messageBox(
                                                "Could not prepare the selected decal texture.\n\nReason: " +
                                                    state->imported.error,
                                                "Select Decal Texture");
                                            return;
                                        }

                                        if (session_ == nullptr ||
                                            !session_->Projects().HasProject() ||
                                            session_->Projects().CurrentProject().projectId !=
                                                state->projectId)
                                        {
                                            return;
                                        }

                                        auto& currentScene =
                                            session_->Scenes().GetScene();
                                        if (!currentScene.decals.Contains(
                                                state->decalEntity))
                                        {
                                            studioChrome_.SetStatusText(
                                                "DECAL TEXTURE // IMPORTED // TARGET NO LONGER EXISTS");
                                            RefreshAssetBrowser();
                                            return;
                                        }

                                        const wi::ecs::Entity materialEntity =
                                            bridge::ResolveEditableMaterialEntity(
                                                currentScene,
                                                state->decalEntity);
                                        if (materialEntity ==
                                            wi::ecs::INVALID_ENTITY)
                                        {
                                            studioChrome_.SetStatusText(
                                                "DECAL TEXTURE // IMPORTED // DECAL MATERIAL MISSING");
                                            RefreshAssetBrowser();
                                            return;
                                        }

                                        bridge::PreparedMaterialTextureAsset prepared;
                                        std::string error;
                                        if (!bridge::PrepareMaterialTextureAsset(
                                                state->projectRoot,
                                                state->projectId,
                                                state->imported.assetId,
                                                prepared,
                                                error))
                                        {
                                            studioChrome_.SetStatusText(
                                                "DECAL TEXTURE // IMPORTED // PREPARE FAILED // " +
                                                error);
                                            RefreshAssetBrowser();
                                            return;
                                        }

                                        auto command = std::make_unique<
                                            bridge::SetMaterialBaseColorTextureAssetCommand>(
                                                currentScene,
                                                materialEntity,
                                                std::move(prepared));
                                        if (!session_->Commands().Execute(
                                                std::move(command)))
                                        {
                                            studioChrome_.SetStatusText(
                                                "DECAL TEXTURE // IMPORTED // ASSIGN FAILED");
                                            RefreshAssetBrowser();
                                            return;
                                        }

                                        studioChrome_.SetSceneDirty(
                                            session_->Commands().IsDirty());
                                        RefreshAssetBrowser();
                                        RefreshInspector();
                                        RefreshStatus();
                                        studioChrome_.SetStatusText(
                                            "DECAL TEXTURE // GOVERNED + ASSIGNED // " +
                                            fs::u8path(state->sourcePath)
                                                .filename().generic_u8string());
                                    });
                            });
                    });
            });
    }

'''

replace_once(
    cpp,
    "    void StudioRenderPath::CreateEnvironmentProbeFromView()\n",
    texture_function + "    void StudioRenderPath::CreateEnvironmentProbeFromView()\n",
)

replace_once(
    header,
    '''        bool CommitSelectedDecal(const bridge::DecalState& state);\n        bool CommitSelectedEnvironmentProbe(''',
    '''        bool CommitSelectedDecal(const bridge::DecalState& state);\n        void ChooseSelectedDecalTexture();\n        bool CommitSelectedEnvironmentProbe(''',
)

replace_once(
    header,
    '''        SceneInspectorSlider decalBaseColorBlue_;\n        SceneInspectorSlider decalOpacity_;\n        wi::gui::Label environmentProbeLabel_;''',
    '''        SceneInspectorSlider decalBaseColorBlue_;\n        SceneInspectorSlider decalOpacity_;\n        SceneInspectorButton decalBaseColorTexture_;\n        wi::gui::Label environmentProbeLabel_;''',
)

replace_once(
    header,
    '''        wi::jobsystem::context modelImportWorkload_;\n        std::string openingScenePath_;''',
    '''        wi::jobsystem::context modelImportWorkload_;\n        wi::jobsystem::context decalTextureImportWorkload_;\n        std::string openingScenePath_;''',
)

replace_once(
    contract,
    '''    "HandleDecalProbeSceneIcons"\n    "RefreshEnvironmentProbe"\n    "SetMaterialCommand")''',
    '''    "HandleDecalProbeSceneIcons"\n    "RefreshEnvironmentProbe"\n    "SetMaterialCommand"\n    "ChooseSelectedDecalTexture"\n    "CreatorTextureWorkflowService"\n    "SetMaterialBaseColorTextureAssetCommand")''',
)

replace_once(
    contract,
    '''    "CreateEnvironmentProbe"\n    "DecalProbeService.h")''',
    '''    "CreateEnvironmentProbe"\n    "DecalProbeService.h"\n    "decalBaseColorTexture_")''',
)

print("Gate 3 governed decal texture picker patch applied")
