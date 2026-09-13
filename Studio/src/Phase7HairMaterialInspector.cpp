#include "Phase7HairMaterialInspector.h"
#include "Phase7AsyncSceneGuard.h"

#include "InspectorSectionFramework.h"
#include "RenegadeStudioChrome.h"
#include "StudioApplication.h"

#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ResourceImportService.h"
#include "renegade/bridge/StudioSession.h"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace renegade::studio
{
    namespace
    {
        namespace fs = std::filesystem;

        class HairMaterialInspector;

        class HairMaterialSectionProvider final : public IInspectorSectionProvider
        {
        public:
            explicit HairMaterialSectionProvider(HairMaterialInspector& owner) noexcept
                : owner_(&owner)
            {
                descriptor_.id = Phase7HairMaterialSectionId;
                descriptor_.title = "HAIR / FUR // TEXTURE / ATLAS";
                descriptor_.order = 37;
                descriptor_.defaultExpanded = true;
                descriptor_.headerHeight = 28.0f;
                descriptor_.spacingAfter = 6.0f;
            }

            [[nodiscard]] const InspectorSectionDescriptor& Descriptor() const noexcept override
            {
                return descriptor_;
            }
            [[nodiscard]] bool IsVisible(const InspectorSectionContext& context) const override;
            [[nodiscard]] float MeasureContentHeight(const InspectorSectionContext&, float) const override;
            void Refresh(const InspectorSectionContext&) override;
            void ApplyLayout(const InspectorSectionContext&, const InspectorSectionLayout& layout) override;

        private:
            HairMaterialInspector* owner_ = nullptr;
            InspectorSectionDescriptor descriptor_;
        };

        class HairMaterialInspector final
        {
        public:
            HairMaterialInspector(
                StudioRenderPath& owner,
                wi::gui::Window& panel,
                InspectorSectionRegistry& registry,
                std::function<void()> requestRefresh,
                std::function<void(std::string)> setStatus)
                : owner_(&owner), panel_(&panel), registry_(&registry),
                  requestRefresh_(std::move(requestRefresh)), setStatus_(std::move(setStatus))
            {
            }

            void Register()
            {
                header_.Create("Phase 7 Hair Fur Material Header");
                header_.SetTooltip(
                    "Dedicated governed material/atlas texture for the selected Wicked HairParticleSystem.");
                header_.OnClick([this](const wi::gui::EventArgs&)
                {
                    const bool opening = !registry_->IsExpanded(Phase7HairMaterialSectionId);
                    (void)registry_->SetExpanded(Phase7HairMaterialSectionId, opening);
                    RequestRefresh();
                });
                panel_->AddWidget(&header_);

                status_.Create("Phase 7 Hair Fur Material Status");
                status_.SetColor(wi::Color::Transparent());
                status_.SetFitTextEnabled(true);
                panel_->AddWidget(&status_);

                textureInfo_.Create("Phase 7 Hair Fur Texture Info");
                textureInfo_.SetColor(wi::Color::Transparent());
                textureInfo_.SetFitTextEnabled(true);
                panel_->AddWidget(&textureInfo_);

                selectTexture_.Create("Phase 7 Hair Fur Select Texture");
                selectTexture_.SetText("TEXTURE / ATLAS // SELECT...");
                selectTexture_.SetTooltip(
                    "Choose PNG, TGA, DDS, JPG/JPEG, BMP or HDR from local storage. Renegade copies/imports it through the governed project texture pipeline and binds it as the Hair material Base Colour atlas.");
                selectTexture_.OnClick([this](const wi::gui::EventArgs&) { BrowseTexture(); });
                panel_->AddWidget(&selectTexture_);

                clearTexture_.Create("Phase 7 Hair Fur Clear Texture");
                clearTexture_.SetText("CLEAR TEXTURE");
                clearTexture_.SetTooltip(
                    "Clear the Hair/Fur Base Colour atlas binding. Undo restores the prior governed/native texture.");
                clearTexture_.OnClick([this](const wi::gui::EventArgs&) { ClearTexture(); });
                panel_->AddWidget(&clearTexture_);

                std::string error;
                if (!registry_->Register(std::make_shared<HairMaterialSectionProvider>(*this), error))
                    SetStatus("HAIR / FUR MATERIAL // " + error);
            }

            [[nodiscard]] bool IsVisible(const InspectorSectionContext& context) const
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !context.hasSelection ||
                    !session->Selection().HasSelection())
                {
                    return false;
                }
                return session->Scenes().GetScene().hairs.Contains(
                    session->Selection().SelectedEntity());
            }

            [[nodiscard]] float Measure() const noexcept
            {
                return 142.0f;
            }

            void Refresh()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !session->Selection().HasSelection())
                    return;

                auto& scene = session->Scenes().GetScene();
                const auto entity = session->Selection().SelectedEntity();
                const auto* hair = scene.hairs.GetComponent(entity);
                const auto* material = scene.materials.GetComponent(entity);
                const bool valid = hair != nullptr && material != nullptr;
                selectTexture_.SetEnabled(valid && session->Projects().HasProject());
                clearTexture_.SetEnabled(valid);

                if (!valid)
                {
                    status_.SetText("Hair/Fur requires its native MaterialComponent.");
                    textureInfo_.SetText("NO HAIR MATERIAL");
                    return;
                }

                status_.SetText(
                    "Hair/Fur atlas is the Base Colour texture on this HairParticleSystem material.");

                const auto* metadata = scene.metadatas.GetComponent(entity);
                const char* metadataKey = bridge::MaterialTextureSlotMetadataKey(
                    bridge::MaterialTextureSlot::BaseColor);
                if (metadata != nullptr && metadata->string_values.has(metadataKey))
                {
                    const auto assetId = metadata->string_values.get(metadataKey);
                    textureInfo_.SetText("GOVERNED ATLAS // " + assetId);
                    return;
                }

                const auto slot = bridge::WickedTextureSlot(
                    bridge::MaterialTextureSlot::BaseColor);
                const auto& texture = material->textures[slot];
                if (!texture.name.empty())
                {
                    textureInfo_.SetText(
                        "NATIVE ATLAS // " +
                        fs::u8path(texture.name).filename().generic_u8string());
                }
                else if (texture.resource.IsValid())
                {
                    textureInfo_.SetText("LIVE / EMBEDDED ATLAS");
                }
                else
                {
                    textureInfo_.SetText("NO TEXTURE / ATLAS ASSIGNED");
                }
            }

            void PrepareForLayout()
            {
                for (auto* widget : Widgets())
                    widget->SetVisible(false);
            }

            void Layout(const InspectorSectionLayout& layout)
            {
                header_.SetVisible(true);
                header_.SetPos(XMFLOAT2(12.0f, layout.top));
                header_.SetSize(XMFLOAT2(layout.width, layout.headerHeight));
                header_.SetText(std::string(layout.expanded ? "▼  " : "▶  ") +
                    "HAIR / FUR // TEXTURE / ATLAS");
                if (!layout.expanded)
                    return;

                const float x = 12.0f;
                const float width = layout.width;
                float y = layout.contentTop;
                auto place = [&](wi::gui::Widget& widget, const float height = 28.0f)
                {
                    widget.SetVisible(true);
                    widget.SetPos(XMFLOAT2(x, y));
                    widget.SetSize(XMFLOAT2(width, height));
                    y += height + 6.0f;
                };
                auto pair = [&](wi::gui::Widget& left, wi::gui::Widget& right)
                {
                    const float half = (width - 8.0f) * 0.5f;
                    left.SetVisible(true);
                    left.SetPos(XMFLOAT2(x, y));
                    left.SetSize(XMFLOAT2(half, 28.0f));
                    right.SetVisible(true);
                    right.SetPos(XMFLOAT2(x + half + 8.0f, y));
                    right.SetSize(XMFLOAT2(half, 28.0f));
                    y += 34.0f;
                };

                place(status_, 36.0f);
                place(textureInfo_, 36.0f);
                pair(selectTexture_, clearTexture_);
            }

        private:
            [[nodiscard]] std::vector<wi::gui::Widget*> Widgets()
            {
                return {&header_, &status_, &textureInfo_, &selectTexture_, &clearTexture_};
            }

            void BrowseTexture()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !session->Projects().HasProject() ||
                    !session->Selection().HasSelection())
                {
                    SetStatus("HAIR / FUR MATERIAL // ACTIVE PROJECT + HAIR SELECTION REQUIRED");
                    return;
                }

                const auto entity = session->Selection().SelectedEntity();
                auto& scene = session->Scenes().GetScene();
                if (!scene.hairs.Contains(entity) || !scene.materials.Contains(entity))
                    return;

                const auto guard = CapturePhase7AsyncSceneGuard();
                wi::helper::FileDialogParams params;
                params.type = wi::helper::FileDialogParams::OPEN;
                params.description = "Hair / Fur texture atlas";
                params.extensions = {"png", "tga", "dds", "jpg", "jpeg", "bmp", "hdr"};
                wi::helper::FileDialog(params,
                    [this, entity, guard](const std::string& sourcePath)
                    {
                        if (sourcePath.empty()) return;
                        wi::eventhandler::Subscribe_Once(
                            wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                            [this, entity, guard, sourcePath](std::uint64_t)
                            {
                                if (!MatchesPhase7AsyncSceneGuard(guard))
                                {
                                    SetStatus("HAIR / FUR MATERIAL // CANCELLED: PROJECT OR LEVEL CHANGED");
                                    RequestRefresh();
                                    return;
                                }

                                auto* current = bridge::StudioSession::Current();
                                if (current == nullptr || !current->Projects().HasProject())
                                    return;
                                auto& liveScene = current->Scenes().GetScene();
                                if (!liveScene.hairs.Contains(entity) ||
                                    !liveScene.materials.Contains(entity))
                                {
                                    SetStatus("HAIR / FUR MATERIAL // TARGET NO LONGER EXISTS");
                                    RequestRefresh();
                                    return;
                                }

                                const auto format = bridge::DetectResourceSourceFormat(sourcePath);
                                if (format == bridge::ResourceSourceFormat::Unknown ||
                                    bridge::ClassifyResourceSourceFormat(format) != bridge::ResourceClass::Texture)
                                {
                                    SetStatus("HAIR / FUR MATERIAL // UNSUPPORTED IMAGE FORMAT");
                                    wi::helper::messageBox(
                                        "Choose PNG, TGA, DDS, JPG/JPEG, BMP or HDR.",
                                        "Hair / Fur Texture");
                                    return;
                                }

                                const auto& project = current->Projects().CurrentProject();
                                bridge::CreatorTextureWorkflowService workflow;
                                const auto imported = workflow.ImportTexture(
                                    project.rootPath, project.projectId, sourcePath);
                                if (!imported.succeeded)
                                {
                                    SetStatus("HAIR / FUR MATERIAL // IMPORT FAILED // " + imported.error);
                                    wi::helper::messageBox(
                                        "Could not import the selected Hair/Fur texture.\n" + imported.error,
                                        "Hair / Fur Texture");
                                    RequestRefresh();
                                    return;
                                }

                                bridge::PreparedMaterialTextureAsset prepared;
                                std::string error;
                                if (!bridge::PrepareMaterialTextureAsset(
                                        project.rootPath,
                                        project.projectId,
                                        imported.assetId,
                                        prepared,
                                        error))
                                {
                                    SetStatus("HAIR / FUR MATERIAL // PREPARE FAILED // " + error);
                                    RequestRefresh();
                                    return;
                                }

                                if (!current->Commands().Execute(
                                        std::make_unique<bridge::SetMaterialTextureAssetCommand>(
                                            liveScene,
                                            entity,
                                            bridge::MaterialTextureSlot::BaseColor,
                                            std::move(prepared))))
                                {
                                    SetStatus("HAIR / FUR MATERIAL // ASSIGN FAILED");
                                    RequestRefresh();
                                    return;
                                }

                                SetStatus(
                                    "HAIR / FUR MATERIAL // GOVERNED + ASSIGNED // " +
                                    fs::u8path(sourcePath).filename().generic_u8string());
                                Refresh();
                                RequestRefresh();
                            });
                    });
            }

            void ClearTexture()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !session->Selection().HasSelection())
                    return;
                const auto entity = session->Selection().SelectedEntity();
                auto& scene = session->Scenes().GetScene();
                if (!scene.hairs.Contains(entity) || !scene.materials.Contains(entity))
                    return;

                if (session->Commands().Execute(
                        std::make_unique<bridge::ClearMaterialTextureAssetCommand>(
                            scene, entity, bridge::MaterialTextureSlot::BaseColor)))
                {
                    SetStatus("HAIR / FUR MATERIAL // TEXTURE CLEARED");
                    Refresh();
                    RequestRefresh();
                }
            }

            void RequestRefresh()
            {
                if (requestRefresh_) requestRefresh_();
            }
            void SetStatus(std::string text)
            {
                if (setStatus_) setStatus_(std::move(text));
            }

            StudioRenderPath* owner_ = nullptr;
            wi::gui::Window* panel_ = nullptr;
            InspectorSectionRegistry* registry_ = nullptr;
            std::function<void()> requestRefresh_;
            std::function<void(std::string)> setStatus_;

            SceneInspectorButton header_;
            wi::gui::Label status_;
            wi::gui::Label textureInfo_;
            SceneInspectorButton selectTexture_;
            SceneInspectorButton clearTexture_;
        };

        bool HairMaterialSectionProvider::IsVisible(const InspectorSectionContext& context) const
        {
            return owner_ != nullptr && owner_->IsVisible(context);
        }

        float HairMaterialSectionProvider::MeasureContentHeight(
            const InspectorSectionContext&, float) const
        {
            return owner_ != nullptr ? owner_->Measure() : 0.0f;
        }

        void HairMaterialSectionProvider::Refresh(const InspectorSectionContext&)
        {
            if (owner_ != nullptr) owner_->Refresh();
        }

        void HairMaterialSectionProvider::ApplyLayout(
            const InspectorSectionContext&, const InspectorSectionLayout& layout)
        {
            if (owner_ != nullptr) owner_->Layout(layout);
        }

        std::unique_ptr<HairMaterialInspector> activeInspector;
        StudioRenderPath* activeOwner = nullptr;
    }

    void RegisterPhase7HairMaterialInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus)
    {
        activeInspector.reset();
        activeOwner = &owner;
        activeInspector = std::make_unique<HairMaterialInspector>(
            owner,
            inspectorPanel,
            registry,
            std::move(requestRefresh),
            std::move(setStatus));
        activeInspector->Register();
    }

    void PreparePhase7HairMaterialInspector(StudioRenderPath& owner)
    {
        if (activeOwner == &owner && activeInspector)
            activeInspector->PrepareForLayout();
    }
}