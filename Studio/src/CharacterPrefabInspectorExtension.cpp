#include "AICharacterInspector.h"

#include "CharacterPrefabStudioIntegration.h"
#include "InspectorSectionFramework.h"
#include "RenegadeStudioChrome.h"
#include "StudioApplication.h"

#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/StudioSession.h"

#include <memory>
#include <string>
#include <utility>

namespace renegade::studio
{
    // AICharacterInspector.cpp is compiled with these two exported symbols
    // renamed. CW-05 wraps that accepted implementation rather than copying or
    // reopening the large AI-02 inspector surface.
    void RegisterAICharacterInspectorLegacy(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus);
    void PrepareAICharacterInspectorLegacy(StudioRenderPath& owner);

    namespace
    {
        inline constexpr const char* CharacterPrefabSectionId =
            "character_prefab";

        class CharacterPrefabInspectorExtension;

        class CharacterPrefabSectionProvider final :
            public IInspectorSectionProvider
        {
        public:
            explicit CharacterPrefabSectionProvider(
                CharacterPrefabInspectorExtension& owner) noexcept
                : owner_(&owner)
            {
                descriptor_.id = CharacterPrefabSectionId;
                descriptor_.title = "CHARACTER PREFAB";
                descriptor_.order = 18;
                descriptor_.defaultExpanded = true;
                descriptor_.headerHeight = 28.0f;
                descriptor_.spacingAfter = 6.0f;
            }

            [[nodiscard]] const InspectorSectionDescriptor& Descriptor()
                const noexcept override
            {
                return descriptor_;
            }
            [[nodiscard]] bool IsVisible(
                const InspectorSectionContext& context) const override;
            [[nodiscard]] float MeasureContentHeight(
                const InspectorSectionContext&, float) const override;
            void Refresh(const InspectorSectionContext&) override;
            void ApplyLayout(
                const InspectorSectionContext&,
                const InspectorSectionLayout& layout) override;

        private:
            CharacterPrefabInspectorExtension* owner_ = nullptr;
            InspectorSectionDescriptor descriptor_;
        };

        class CharacterPrefabInspectorExtension final
        {
        public:
            CharacterPrefabInspectorExtension(
                wi::gui::Window& panel,
                InspectorSectionRegistry& registry,
                std::function<void()> requestRefresh,
                std::function<void(std::string)> setStatus)
                : panel_(&panel)
                , registry_(&registry)
                , requestRefresh_(std::move(requestRefresh))
                , setStatus_(std::move(setStatus))
            {
            }

            void Register()
            {
                header_.Create("Character Prefab Section Header");
                header_.SetTooltip(
                    "Save the configured Character as a reusable gameplay prefab while retaining the prepared base Character Asset separately.");
                header_.OnClick([this](const wi::gui::EventArgs&)
                {
                    (void)registry_->ToggleExpanded(CharacterPrefabSectionId);
                    RequestRefresh();
                });
                panel_->AddWidget(&header_);

                save_.Create("Save Character Prefab");
                save_.SetText("SAVE CHARACTER PREFAB");
                save_.SetTooltip(
                    "Create a reusable Character Prefab containing this Character's gameplay, AI and Action/Script setup. New placements receive fresh Scene and script identities.");
                save_.OnClick([this](const wi::gui::EventArgs&)
                {
                    ResolveSelection();
                    std::string status;
                    (void)SaveSelectedCharacterPrefab(selected_, status);
                    SetStatus(std::move(status));
                    RequestRefresh();
                });
                panel_->AddWidget(&save_);

                std::string error;
                if (!registry_->Register(
                        std::make_shared<CharacterPrefabSectionProvider>(*this),
                        error))
                {
                    SetStatus("CW-05 // " + error);
                }
            }

            void PrepareForLayout()
            {
                header_.SetVisible(false);
                save_.SetVisible(false);
            }

            [[nodiscard]] bool IsVisible(
                const InspectorSectionContext& context)
            {
                if (!context.hasSelection)
                    return false;
                ResolveSelection();
                auto* session = bridge::StudioSession::Current();
                return session != nullptr &&
                    selected_ != wi::ecs::INVALID_ENTITY &&
                    bridge::IsRenegadeCharacter(
                        session->Scenes().GetScene(), selected_);
            }

            [[nodiscard]] float Measure() const noexcept
            {
                return 34.0f;
            }

            void Refresh()
            {
                ResolveSelection();
                auto* session = bridge::StudioSession::Current();
                save_.SetEnabled(
                    session != nullptr &&
                    selected_ != wi::ecs::INVALID_ENTITY &&
                    bridge::IsRenegadeCharacter(
                        session->Scenes().GetScene(), selected_));
            }

            void Layout(const InspectorSectionLayout& layout)
            {
                header_.SetVisible(true);
                header_.SetPos(XMFLOAT2(12.0f, layout.top));
                header_.SetSize(XMFLOAT2(layout.width, layout.headerHeight));
                header_.SetText(
                    std::string(layout.expanded ? "▼  " : "▶  ") +
                    "CHARACTER PREFAB");
                if (!layout.expanded)
                    return;

                save_.SetVisible(true);
                save_.SetPos(XMFLOAT2(12.0f, layout.contentTop));
                save_.SetSize(XMFLOAT2(layout.width, 28.0f));
            }

        private:
            void ResolveSelection()
            {
                auto* session = bridge::StudioSession::Current();
                selected_ = session != nullptr &&
                        session->Selection().HasSelection()
                    ? session->Selection().SelectedEntity()
                    : wi::ecs::INVALID_ENTITY;
            }

            void RequestRefresh()
            {
                if (requestRefresh_)
                    requestRefresh_();
            }

            void SetStatus(std::string status)
            {
                if (setStatus_)
                    setStatus_(std::move(status));
            }

            wi::gui::Window* panel_ = nullptr;
            InspectorSectionRegistry* registry_ = nullptr;
            std::function<void()> requestRefresh_;
            std::function<void(std::string)> setStatus_;
            wi::ecs::Entity selected_ = wi::ecs::INVALID_ENTITY;
            SceneInspectorButton header_;
            SceneInspectorButton save_;
        };

        bool CharacterPrefabSectionProvider::IsVisible(
            const InspectorSectionContext& context) const
        {
            return owner_ != nullptr && owner_->IsVisible(context);
        }

        float CharacterPrefabSectionProvider::MeasureContentHeight(
            const InspectorSectionContext&,
            float) const
        {
            return owner_ != nullptr ? owner_->Measure() : 0.0f;
        }

        void CharacterPrefabSectionProvider::Refresh(
            const InspectorSectionContext&)
        {
            if (owner_ != nullptr)
                owner_->Refresh();
        }

        void CharacterPrefabSectionProvider::ApplyLayout(
            const InspectorSectionContext&,
            const InspectorSectionLayout& layout)
        {
            if (owner_ != nullptr)
                owner_->Layout(layout);
        }

        std::unique_ptr<CharacterPrefabInspectorExtension> activePrefabInspector;
        StudioRenderPath* activePrefabOwner = nullptr;
    }

    void RegisterAICharacterInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus)
    {
        InstallCharacterPrefabStudioIntegration();

        // Preserve the accepted Character + Advanced AI surfaces first.
        RegisterAICharacterInspectorLegacy(
            owner,
            inspectorPanel,
            registry,
            requestRefresh,
            setStatus);

        activePrefabInspector.reset();
        activePrefabOwner = &owner;
        activePrefabInspector =
            std::make_unique<CharacterPrefabInspectorExtension>(
                inspectorPanel,
                registry,
                std::move(requestRefresh),
                std::move(setStatus));
        activePrefabInspector->Register();
    }

    void PrepareAICharacterInspector(StudioRenderPath& owner)
    {
        PrepareAICharacterInspectorLegacy(owner);
        if (activePrefabOwner == &owner && activePrefabInspector)
            activePrefabInspector->PrepareForLayout();
    }
}
