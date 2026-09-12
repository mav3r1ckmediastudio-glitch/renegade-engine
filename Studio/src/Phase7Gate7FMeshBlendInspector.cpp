#include "Phase7Gate7FMeshBlendInspector.h"

#include "InspectorSectionFramework.h"
#include "Phase7Gate7AAnimationInspector.h"
#include "Phase7Gate7BHumanoidRetargetInspector.h"
#include "Phase7Gate7CCharacterControlsInspector.h"
#include "Phase7Gate7DNativeTimelineInspector.h"
#include "Phase7Gate7ESpecialistInspector.h"
#include "RenegadeStudioChrome.h"
#include "S4BScriptAttachmentInspector.h"
#include "S4DGlobalScriptInspector.h"
#include "StudioApplication.h"

#include "renegade/bridge/MaterialService.h"
#include "renegade/bridge/RenderSettingsService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace renegade::studio
{
    namespace
    {
        std::string MaterialName(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const std::size_t index)
        {
            if (const auto* name = scene.names.GetComponent(entity);
                name != nullptr && !name->name.empty())
            {
                return name->name;
            }
            return "MATERIAL " + std::to_string(index + 1);
        }

        class MeshBlendInspector;

        class MeshBlendSectionProvider final : public IInspectorSectionProvider
        {
        public:
            explicit MeshBlendSectionProvider(MeshBlendInspector& owner) noexcept
                : owner_(&owner)
            {
                descriptor_.id = Phase7MeshBlendSectionId;
                descriptor_.title = "MESH BLENDING";
                descriptor_.order = 39;
                descriptor_.defaultExpanded = false;
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
            MeshBlendInspector* owner_ = nullptr;
            InspectorSectionDescriptor descriptor_;
        };

        class MeshBlendInspector final
        {
        public:
            MeshBlendInspector(
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
                header_.Create("Phase 7F Mesh Blending Section Header");
                header_.SetTooltip(
                    "Wicked-native screen-space mesh blending for ordinary creator materials. This is not limited to Terrain Blended materials.");
                header_.OnClick([this](const wi::gui::EventArgs&)
                {
                    const bool opening = !registry_->IsExpanded(Phase7MeshBlendSectionId);
                    for (const char* sectionId : {
                        "transform", "rendering", "materials",
                        S4BActionSectionId, S4BScriptSectionId, S4DGlobalScriptSectionId,
                        Phase7AnimationSectionId, Phase7HumanoidRetargetSectionId,
                        Phase7CharacterControlsSectionId, Phase7NativeTimelineSectionId,
                        Phase7SpecialistSectionId, Phase7MeshBlendSectionId})
                    {
                        (void)registry_->SetExpanded(sectionId, false);
                    }
                    if (opening)
                        (void)registry_->SetExpanded(Phase7MeshBlendSectionId, true);
                    RequestRefresh();
                });
                panel_->AddWidget(&header_);

                status_.Create("Phase 7F Mesh Blending Status");
                status_.SetColor(wi::Color::Transparent());
                status_.SetFitTextEnabled(true);
                panel_->AddWidget(&status_);

                globalEnabled_.Create("Global mesh blending: ");
                globalEnabled_.SetTooltip(
                    "Enable or disable Wicked RenderPath3D mesh blending globally. The setting is persisted with the level and shared by Studio and Runtime.");
                globalEnabled_.OnClick([this](const wi::gui::EventArgs& args)
                {
                    CommitGlobal(args.bValue);
                });
                panel_->AddWidget(&globalEnabled_);

                materials_.Create("Mesh Blend Material");
                materials_.SetTooltip(
                    "Choose an editable material referenced by the selected mesh/object hierarchy.");
                materials_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    selectedMaterial_ = static_cast<wi::ecs::Entity>(args.userdata);
                    RefreshMaterial();
                    RequestRefresh();
                });
                panel_->AddWidget(&materials_);

                blend_.Create(0.0f, 2.0f, 0.0f, 2000.0f,
                    "Material Mesh Blend", "MESH BLEND // MATERIAL");
                blend_.SetTooltip(
                    "Native MaterialComponent mesh-blend falloff. Any value above zero marks the object for Wicked's mesh-blend pass, including ordinary PBR materials.");
                blend_.OnValueCommitted([this](const float value)
                {
                    CommitMaterial(value);
                });
                panel_->AddWidget(&blend_);

                info_.Create("Phase 7F Mesh Blending Info");
                info_.SetColor(wi::Color::Transparent());
                info_.SetFitTextEnabled(true);
                panel_->AddWidget(&info_);

                std::string error;
                if (!registry_->Register(std::make_shared<MeshBlendSectionProvider>(*this), error))
                    SetStatus("PHASE 7F // " + error);
            }

            [[nodiscard]] bool IsVisible(const InspectorSectionContext&) const
            {
                return bridge::StudioSession::Current() != nullptr;
            }

            [[nodiscard]] float Measure() const noexcept
            {
                return 190.0f;
            }

            void Refresh()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr)
                    return;

                const auto render = bridge::CaptureRenderSettings(session->Scenes().GetScene());
                globalEnabled_.SetCheck(render.meshBlendingEnabled);
                RefreshMaterialList();
                RefreshMaterial();
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
                header_.SetText(std::string(layout.expanded ? "▼  " : "▶  ") + "MESH BLENDING");
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

                place(status_, 34.0f);
                place(globalEnabled_);
                place(materials_);
                place(blend_);
                place(info_, 42.0f);
            }

        private:
            [[nodiscard]] std::vector<wi::gui::Widget*> Widgets()
            {
                return {&header_, &status_, &globalEnabled_, &materials_, &blend_, &info_};
            }

            void RefreshMaterialList()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr)
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto selected = session->Selection().HasSelection()
                    ? session->Selection().SelectedEntity()
                    : wi::ecs::INVALID_ENTITY;
                materialEntities_ = bridge::CollectEditableMaterialEntities(scene, selected);

                const bool keep = std::find(
                    materialEntities_.begin(), materialEntities_.end(), selectedMaterial_) !=
                    materialEntities_.end();
                if (!keep)
                    selectedMaterial_ = materialEntities_.empty()
                        ? wi::ecs::INVALID_ENTITY
                        : materialEntities_.front();

                materials_.ClearItems();
                for (std::size_t index = 0; index < materialEntities_.size(); ++index)
                {
                    materials_.AddItem(
                        MaterialName(scene, materialEntities_[index], index),
                        static_cast<std::uint64_t>(materialEntities_[index]));
                }
                materials_.SetEnabled(!materialEntities_.empty());
                if (selectedMaterial_ != wi::ecs::INVALID_ENTITY)
                {
                    materials_.SetSelectedByUserdataWithoutCallback(
                        static_cast<std::uint64_t>(selectedMaterial_));
                }
            }

            void RefreshMaterial()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr)
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto render = bridge::CaptureRenderSettings(scene);

                const auto* material = selectedMaterial_ == wi::ecs::INVALID_ENTITY
                    ? nullptr
                    : scene.materials.GetComponent(selectedMaterial_);
                const bool editable = material != nullptr &&
                    !bridge::IsTerrainOwnedMaterial(scene, selectedMaterial_);
                blend_.SetEnabled(editable);
                if (editable)
                {
                    const auto state = bridge::CaptureMaterial(*material);
                    blend_.SetValue(state.meshBlend);
                    status_.SetText(
                        std::string("Native mesh blend // global ") +
                        (render.meshBlendingEnabled ? "ON" : "OFF") +
                        " // material falloff " + std::to_string(state.meshBlend));
                    info_.SetText(
                        "Works on ordinary Wicked materials; Terrain Blended is not required. Values above zero opt the object into Wicked's mesh-blend pass.");
                }
                else
                {
                    blend_.SetValue(0.0f);
                    status_.SetText(
                        std::string("Native mesh blend // global ") +
                        (render.meshBlendingEnabled ? "ON" : "OFF"));
                    info_.SetText(
                        "Select a mesh or object with an editable material to author its mesh-blend falloff.");
                }
            }

            void CommitGlobal(const bool enabled)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || owner_ == nullptr)
                    return;
                auto& scene = session->Scenes().GetScene();
                auto state = bridge::CaptureRenderSettings(scene);
                state.meshBlendingEnabled = enabled;
                StudioRenderPath* stableOwner = owner_;
                const auto callback = [stableOwner](const bridge::RenderSettingsState& applied)
                {
                    if (stableOwner != nullptr)
                        stableOwner->setMeshBlendEnabled(applied.meshBlendingEnabled);
                };
                if (session->Commands().Execute(
                        std::make_unique<bridge::SetRenderSettingsCommand>(
                            scene, state, callback)))
                {
                    SetStatus(std::string("PHASE 7F // GLOBAL MESH BLENDING // ") +
                        (enabled ? "ON" : "OFF"));
                }
                Refresh();
                RequestRefresh();
            }

            void CommitMaterial(const float value)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || selectedMaterial_ == wi::ecs::INVALID_ENTITY)
                    return;
                auto& scene = session->Scenes().GetScene();
                const auto* material = scene.materials.GetComponent(selectedMaterial_);
                if (material == nullptr || bridge::IsTerrainOwnedMaterial(scene, selectedMaterial_))
                    return;
                auto state = bridge::CaptureMaterial(*material);
                state.meshBlend = value;
                if (session->Commands().Execute(
                        std::make_unique<bridge::SetMaterialCommand>(
                            scene, selectedMaterial_, state)))
                {
                    SetStatus("PHASE 7F // NATIVE MATERIAL MESH BLEND COMMITTED");
                }
                RefreshMaterial();
                RequestRefresh();
            }

            void RequestRefresh()
            {
                if (requestRefresh_)
                    requestRefresh_();
            }

            void SetStatus(std::string value)
            {
                if (setStatus_)
                    setStatus_(std::move(value));
            }

            StudioRenderPath* owner_ = nullptr;
            wi::gui::Window* panel_ = nullptr;
            InspectorSectionRegistry* registry_ = nullptr;
            std::function<void()> requestRefresh_;
            std::function<void(std::string)> setStatus_;
            wi::ecs::Entity selectedMaterial_ = wi::ecs::INVALID_ENTITY;
            std::vector<wi::ecs::Entity> materialEntities_;

            SceneInspectorButton header_;
            wi::gui::Label status_;
            SceneInspectorCheckBox globalEnabled_;
            SceneInspectorComboBox materials_;
            SceneInspectorSlider blend_;
            wi::gui::Label info_;
        };

        bool MeshBlendSectionProvider::IsVisible(const InspectorSectionContext& context) const
        {
            return owner_ != nullptr && owner_->IsVisible(context);
        }

        float MeshBlendSectionProvider::MeasureContentHeight(
            const InspectorSectionContext&,
            float) const
        {
            return owner_ != nullptr ? owner_->Measure() : 0.0f;
        }

        void MeshBlendSectionProvider::Refresh(const InspectorSectionContext&)
        {
            if (owner_ != nullptr)
                owner_->Refresh();
        }

        void MeshBlendSectionProvider::ApplyLayout(
            const InspectorSectionContext&,
            const InspectorSectionLayout& layout)
        {
            if (owner_ != nullptr)
                owner_->Layout(layout);
        }

        std::unique_ptr<MeshBlendInspector> activeInspector;
        StudioRenderPath* activeOwner = nullptr;
    }

    void RegisterPhase7Gate7FMeshBlendInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus)
    {
        activeInspector.reset();
        activeOwner = &owner;
        activeInspector = std::make_unique<MeshBlendInspector>(
            owner, inspectorPanel, registry, std::move(requestRefresh), std::move(setStatus));
        activeInspector->Register();
    }

    void PreparePhase7Gate7FMeshBlendInspector(StudioRenderPath& owner)
    {
        if (activeOwner == &owner && activeInspector)
            activeInspector->PrepareForLayout();
    }
}
