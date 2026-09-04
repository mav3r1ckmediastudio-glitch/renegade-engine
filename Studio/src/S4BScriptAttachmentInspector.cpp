#include "S4BScriptAttachmentInspector.h"
#include "S4CScriptPropertyControls.h"

#include "InspectorSectionFramework.h"
#include "RenegadeStudioChrome.h"
#include "StudioApplication.h"

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScriptAuthoringService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace renegade::studio
{
    namespace
    {
        namespace fs = std::filesystem;
        using bridge::ScriptAttachment;
        using bridge::ScriptAuthoringSource;
        using bridge::ScriptPresentation;
        using bridge::StableId;

        class ScriptAttachmentInspector;

        class ScriptRoleProvider final : public IInspectorSectionProvider
        {
        public:
            ScriptRoleProvider(
                ScriptAttachmentInspector& owner,
                ScriptPresentation presentation,
                InspectorSectionDescriptor descriptor) noexcept
                : owner_(&owner)
                , presentation_(presentation)
                , descriptor_(std::move(descriptor))
            {
            }

            [[nodiscard]] const InspectorSectionDescriptor& Descriptor()
                const noexcept override
            {
                return descriptor_;
            }

            [[nodiscard]] bool IsVisible(
                const InspectorSectionContext& context) const override;
            [[nodiscard]] float MeasureContentHeight(
                const InspectorSectionContext& context,
                float availableWidth) const override;
            void Refresh(const InspectorSectionContext& context) override;
            void ApplyLayout(
                const InspectorSectionContext& context,
                const InspectorSectionLayout& layout) override;

        private:
            ScriptAttachmentInspector* owner_ = nullptr;
            ScriptPresentation presentation_ = ScriptPresentation::Action;
            InspectorSectionDescriptor descriptor_;
        };

        struct ScriptRowControls
        {
            wi::gui::Label source;
            SceneInspectorCheckBox enabled;
            SceneInspectorButton up;
            SceneInspectorButton down;
            SceneInspectorButton remove;
            StableId scriptInstanceId;
            std::uint32_t order = 0;
            std::unique_ptr<S4CScriptPropertyEditor> properties;
        };

        struct ScriptRoleControls
        {
            SceneInspectorButton header;
            wi::gui::Label status;
            SceneInspectorComboBox addSource;
            SceneInspectorButton add;
            SceneInspectorButton refresh;
            std::vector<std::unique_ptr<ScriptRowControls>> rows;
            std::vector<ScriptAuthoringSource> sources;
            std::vector<bridge::ScriptMetadataDiagnostic> diagnostics;
            std::size_t selectedSource = 0;
            std::uint64_t sourceSceneRevision =
                std::numeric_limits<std::uint64_t>::max();
            std::string sourceProjectRoot;
            StableId ownerEntityId;
            bool documentReady = false;
            std::string documentError;
        };

        InspectorSectionDescriptor MakeScriptSection(
            std::string id,
            std::string title,
            const int order)
        {
            InspectorSectionDescriptor descriptor;
            descriptor.id = std::move(id);
            descriptor.title = std::move(title);
            descriptor.order = order;
            descriptor.defaultExpanded = false;
            descriptor.headerHeight = 28.0f;
            descriptor.spacingAfter = 6.0f;
            return descriptor;
        }

        const char* RoleLabel(const ScriptPresentation presentation) noexcept
        {
            return presentation == ScriptPresentation::Action
                ? "ACTION"
                : "SCRIPT";
        }

        const char* RoleSectionId(const ScriptPresentation presentation) noexcept
        {
            return presentation == ScriptPresentation::Action
                ? S4BActionSectionId
                : S4BScriptSectionId;
        }

        class ScriptAttachmentInspector final
        {
        public:
            ScriptAttachmentInspector(
                StudioRenderPath& owner,
                wi::gui::Window& panel,
                InspectorSectionRegistry& registry,
                std::function<void()> requestRefresh,
                std::function<void(std::string)> setStatus)
                : owner_(&owner)
                , panel_(&panel)
                , registry_(&registry)
                , requestRefresh_(std::move(requestRefresh))
                , setStatus_(std::move(setStatus))
            {
            }

            void Register()
            {
                CreateRoleControls(ScriptPresentation::Action);
                CreateRoleControls(ScriptPresentation::Script);

                std::string error;
                auto action = std::make_shared<ScriptRoleProvider>(
                    *this,
                    ScriptPresentation::Action,
                    MakeScriptSection(S4BActionSectionId, "ACTION", 40));
                if (!registry_->Register(std::move(action), error))
                    SetStatus("S4B INSPECTOR // " + error);

                error.clear();
                auto script = std::make_shared<ScriptRoleProvider>(
                    *this,
                    ScriptPresentation::Script,
                    MakeScriptSection(S4BScriptSectionId, "SCRIPT", 50));
                if (!registry_->Register(std::move(script), error))
                    SetStatus("S4B INSPECTOR // " + error);
            }

            [[nodiscard]] bool RoleVisible(
                const ScriptPresentation presentation,
                const InspectorSectionContext& context) const
            {
                (void)presentation;
                if (!context.hasSelection)
                    return false;
                const auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !session->Selection().HasSelection())
                    return false;
                const auto selected = session->Selection().SelectedEntity();
                return selected != wi::ecs::INVALID_ENTITY &&
                    session->Scenes().ContainsEntity(selected);
            }

            [[nodiscard]] float MeasureRole(
                const ScriptPresentation presentation) const
            {
                const auto& controls = Role(presentation);
                const std::size_t count = VisibleAttachmentCount(presentation);
                float height = 60.0f;
                for (std::size_t index = 0; index < count && index < controls.rows.size(); ++index)
                {
                    height += 36.0f;
                    if (controls.rows[index]->properties)
                        height += controls.rows[index]->properties->MeasureHeight();
                }
                return height;
            }

            void RefreshRole(const ScriptPresentation presentation)
            {
                auto& controls = Role(presentation);
                controls.ownerEntityId.clear();
                controls.documentReady = false;
                controls.documentError.clear();

                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !session->Selection().HasSelection())
                    return;
                const auto selected = session->Selection().SelectedEntity();
                if (selected == wi::ecs::INVALID_ENTITY)
                    return;

                controls.ownerEntityId = bridge::PersistentEntityId(
                    session->Scenes().GetScene(), selected);
                if (!bridge::IsValidStableId(controls.ownerEntityId))
                {
                    controls.documentError =
                        "Selected entity has no persistent Renegade identity.";
                    RefreshStatusText(presentation);
                    HideUnusedRows(controls, 0);
                    return;
                }

                std::string error;
                if (!session->Scripts().EnsureCurrent(error))
                {
                    controls.documentError = std::move(error);
                    RefreshStatusText(presentation);
                    HideUnusedRows(controls, 0);
                    return;
                }
                controls.documentReady = true;

                if (registry_->IsExpanded(RoleSectionId(presentation)))
                    RefreshSourcesIfNeeded(presentation, false);

                const auto attachments = session->Scripts().EntityAttachments(
                    controls.ownerEntityId,
                    presentation);
                EnsureRows(presentation, attachments.size());
                for (std::size_t index = 0; index < attachments.size(); ++index)
                    RefreshRow(presentation, index, *attachments[index]);
                HideUnusedRows(controls, attachments.size());
                RefreshStatusText(presentation);
            }

            void LayoutRole(
                const ScriptPresentation presentation,
                const InspectorSectionLayout& layout)
            {
                auto& controls = Role(presentation);
                controls.header.SetVisible(true);
                controls.header.SetPos(XMFLOAT2(12.0f, layout.top));
                controls.header.SetSize(XMFLOAT2(layout.width, layout.headerHeight));
                controls.header.SetText(
                    std::string(layout.expanded ? "▼  " : "▶  ") +
                    RoleLabel(presentation));

                if (!layout.expanded)
                {
                    SetRoleContentVisible(controls, false);
                    return;
                }

                RefreshSourcesIfNeeded(presentation, false);
                const float x = 12.0f;
                const float width = layout.width;
                const float top = layout.contentTop;

                controls.status.SetVisible(true);
                controls.status.SetPos(XMFLOAT2(x, top));
                controls.status.SetSize(XMFLOAT2(width, 24.0f));

                const float refreshWidth = 64.0f;
                const float addWidth = 54.0f;
                const float gap = 6.0f;
                const float sourceWidth = std::max(
                    80.0f,
                    width - refreshWidth - addWidth - gap * 2.0f);
                controls.addSource.SetVisible(true);
                controls.addSource.SetPos(XMFLOAT2(x, top + 26.0f));
                controls.addSource.SetSize(XMFLOAT2(sourceWidth, 28.0f));
                controls.refresh.SetVisible(true);
                controls.refresh.SetPos(XMFLOAT2(
                    x + sourceWidth + gap,
                    top + 26.0f));
                controls.refresh.SetSize(XMFLOAT2(refreshWidth, 28.0f));
                controls.add.SetVisible(true);
                controls.add.SetPos(XMFLOAT2(
                    x + sourceWidth + gap + refreshWidth + gap,
                    top + 26.0f));
                controls.add.SetSize(XMFLOAT2(addWidth, 28.0f));
                controls.addSource.SetEnabled(
                    controls.documentReady && !controls.sources.empty());
                controls.add.SetEnabled(
                    controls.documentReady && !controls.sources.empty());

                auto* session = bridge::StudioSession::Current();
                std::vector<const ScriptAttachment*> attachments;
                if (session != nullptr && controls.documentReady)
                {
                    attachments = session->Scripts().EntityAttachments(
                        controls.ownerEntityId,
                        presentation);
                }
                float rowY = top + 60.0f;
                for (std::size_t index = 0; index < attachments.size(); ++index)
                {
                    auto& row = *controls.rows[index];
                    const float y = rowY;
                    const float enabledWidth = 62.0f;
                    const float buttonWidth = 42.0f;
                    const float removeWidth = 58.0f;
                    const float rowGap = 4.0f;
                    const float labelWidth = std::max(
                        70.0f,
                        width - enabledWidth - buttonWidth * 2.0f -
                            removeWidth - rowGap * 4.0f);

                    row.source.SetVisible(true);
                    row.source.SetPos(XMFLOAT2(x, y));
                    row.source.SetSize(XMFLOAT2(labelWidth, 28.0f));
                    row.enabled.SetVisible(true);
                    row.enabled.SetPos(XMFLOAT2(
                        x + labelWidth + rowGap, y));
                    row.enabled.SetSize(XMFLOAT2(enabledWidth, 28.0f));
                    row.up.SetVisible(true);
                    row.up.SetPos(XMFLOAT2(
                        x + labelWidth + rowGap + enabledWidth + rowGap, y));
                    row.up.SetSize(XMFLOAT2(buttonWidth, 28.0f));
                    row.down.SetVisible(true);
                    row.down.SetPos(XMFLOAT2(
                        x + labelWidth + rowGap + enabledWidth + rowGap +
                            buttonWidth + rowGap,
                        y));
                    row.down.SetSize(XMFLOAT2(buttonWidth, 28.0f));
                    row.remove.SetVisible(true);
                    row.remove.SetPos(XMFLOAT2(
                        x + width - removeWidth,
                        y));
                    row.remove.SetSize(XMFLOAT2(removeWidth, 28.0f));
                    row.up.SetEnabled(index > 0);
                    row.down.SetEnabled(index + 1 < attachments.size());
                    if (row.properties)
                    {
                        row.properties->SetVisible(true);
                        row.properties->Layout(
                            x + 12.0f,
                            y + 32.0f,
                            std::max(70.0f, width - 12.0f));
                        rowY += 36.0f + row.properties->MeasureHeight();
                    }
                    else
                    {
                        rowY += 36.0f;
                    }
                }
                HideUnusedRows(controls, attachments.size());
            }

            void PrepareForLayout()
            {
                for (const auto presentation : {
                    ScriptPresentation::Action,
                    ScriptPresentation::Script})
                {
                    auto& controls = Role(presentation);
                    controls.header.SetVisible(false);
                    SetRoleContentVisible(controls, false);
                }
            }

        private:
            ScriptRoleControls& Role(const ScriptPresentation presentation)
            {
                return presentation == ScriptPresentation::Action
                    ? action_
                    : script_;
            }

            const ScriptRoleControls& Role(
                const ScriptPresentation presentation) const
            {
                return presentation == ScriptPresentation::Action
                    ? action_
                    : script_;
            }

            void CreateRoleControls(const ScriptPresentation presentation)
            {
                auto& controls = Role(presentation);
                const std::string role = RoleLabel(presentation);

                controls.header.Create("S4B " + role + " Section Header");
                controls.header.SetTooltip(
                    "Expand or collapse creator " + role + " attachments.");
                controls.header.OnClick(
                    [this, presentation](const wi::gui::EventArgs&)
                    {
                        const char* id = RoleSectionId(presentation);
                        const bool opening = !registry_->IsExpanded(id);
                        for (const char* section : {
                            "transform", "rendering", "materials",
                            S4BActionSectionId, S4BScriptSectionId})
                        {
                            (void)registry_->SetExpanded(section, false);
                        }
                        if (opening)
                            (void)registry_->SetExpanded(id, true);
                        RequestRefresh();
                    });
                panel_->AddWidget(&controls.header);

                controls.status.Create("S4B " + role + " Status");
                controls.status.SetText("No attachments.");
                panel_->AddWidget(&controls.status);

                controls.addSource.Create("S4B " + role + " Source");
                controls.addSource.SetTooltip(
                    "Choose a governed Content/Scripts Lua source whose S4A metadata role is " +
                    role + ".");
                controls.addSource.OnSelect(
                    [this, presentation](const wi::gui::EventArgs& args)
                    {
                        Role(presentation).selectedSource =
                            static_cast<std::size_t>(args.userdata);
                    });
                panel_->AddWidget(&controls.addSource);

                controls.refresh.Create("REFRESH");
                controls.refresh.SetTooltip(
                    "Rescan Content/Scripts and re-evaluate restricted S4A metadata.");
                controls.refresh.OnClick(
                    [this, presentation](const wi::gui::EventArgs&)
                    {
                        RefreshSourcesIfNeeded(presentation, true);
                        RequestRefresh();
                    });
                panel_->AddWidget(&controls.refresh);

                controls.add.Create("ADD");
                controls.add.SetTooltip("Attach the selected governed source.");
                controls.add.OnClick(
                    [this, presentation](const wi::gui::EventArgs&)
                    {
                        AddSelectedSource(presentation);
                    });
                panel_->AddWidget(&controls.add);

                controls.header.SetVisible(false);
                SetRoleContentVisible(controls, false);
            }

            void EnsureRows(
                const ScriptPresentation presentation,
                const std::size_t count)
            {
                auto& controls = Role(presentation);
                while (controls.rows.size() < count)
                {
                    const std::size_t index = controls.rows.size();
                    auto row = std::make_unique<ScriptRowControls>();
                    row->properties = std::make_unique<S4CScriptPropertyEditor>(
                        *panel_,
                        [this]() { RequestRefresh(); },
                        [this](std::string status) { SetStatus(std::move(status)); });
                    row->source.Create(
                        "S4B " + std::string(RoleLabel(presentation)) +
                        " Source " + std::to_string(index));
                    panel_->AddWidget(&row->source);

                    row->enabled.Create("ON");
                    row->enabled.SetTooltip("Enable or disable this script instance.");
                    row->enabled.OnClick(
                        [this, presentation, index](const wi::gui::EventArgs& args)
                        {
                            SetEnabled(presentation, index, args.bValue);
                        });
                    panel_->AddWidget(&row->enabled);

                    row->up.Create("UP");
                    row->up.SetTooltip("Move this attachment earlier in the entity script execution order.");
                    row->up.OnClick(
                        [this, presentation, index](const wi::gui::EventArgs&)
                        {
                            Move(presentation, index, -1);
                        });
                    panel_->AddWidget(&row->up);

                    row->down.Create("DOWN");
                    row->down.SetTooltip("Move this attachment later in the entity script execution order.");
                    row->down.OnClick(
                        [this, presentation, index](const wi::gui::EventArgs&)
                        {
                            Move(presentation, index, 1);
                        });
                    panel_->AddWidget(&row->down);

                    row->remove.Create("REMOVE");
                    row->remove.SetTooltip("Detach this script instance from the entity.");
                    row->remove.OnClick(
                        [this, presentation, index](const wi::gui::EventArgs&)
                        {
                            Remove(presentation, index);
                        });
                    panel_->AddWidget(&row->remove);
                    controls.rows.push_back(std::move(row));
                }
            }

            void RefreshRow(
                const ScriptPresentation presentation,
                const std::size_t index,
                const ScriptAttachment& attachment)
            {
                auto& controls = Role(presentation);
                if (index >= controls.rows.size())
                    return;
                auto& row = *controls.rows[index];
                row.scriptInstanceId = attachment.scriptInstanceId;
                row.order = attachment.order;

                std::string display =
                    fs::u8path(attachment.sourcePath).filename().generic_u8string();
                const auto source = std::find_if(
                    controls.sources.begin(), controls.sources.end(),
                    [&](const ScriptAuthoringSource& candidate)
                    {
                        return candidate.sourcePath == attachment.sourcePath;
                    });
                const bridge::ScriptMetadataDescriptor* metadata = nullptr;
                if (source != controls.sources.end())
                {
                    display = source->metadata.name;
                    metadata = &source->metadata;
                }
                row.source.SetText(
                    "#" + std::to_string(attachment.order + 1) + "  " + display);
                row.source.SetTooltip(attachment.sourcePath);
                row.enabled.SetCheck(attachment.enabled);
                if (row.properties)
                {
                    row.properties->Refresh(
                        attachment.scriptInstanceId,
                        attachment,
                        metadata);
                }
            }

            void RefreshSourcesIfNeeded(
                const ScriptPresentation presentation,
                const bool force)
            {
                auto& controls = Role(presentation);
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !controls.documentReady ||
                    !session->Projects().HasProject())
                {
                    return;
                }
                const auto& project = session->Projects().CurrentProject();
                if (!force &&
                    controls.sourceSceneRevision == session->Scenes().Revision() &&
                    controls.sourceProjectRoot == project.rootPath)
                {
                    return;
                }

                controls.sources.clear();
                controls.diagnostics.clear();
                controls.addSource.ClearItems();
                controls.selectedSource = 0;

                std::string error;
                if (!session->Scripts().EnumerateProjectSources(
                        presentation,
                        controls.sources,
                        controls.diagnostics,
                        error))
                {
                    controls.documentError = std::move(error);
                    controls.sourceSceneRevision = session->Scenes().Revision();
                    controls.sourceProjectRoot = project.rootPath;
                    RefreshStatusText(presentation);
                    return;
                }

                for (std::size_t index = 0; index < controls.sources.size(); ++index)
                {
                    const auto& source = controls.sources[index];
                    std::string label = source.metadata.category.empty()
                        ? source.metadata.name
                        : source.metadata.category + " / " + source.metadata.name;
                    controls.addSource.AddItem(
                        label,
                        static_cast<std::uint64_t>(index));
                }
                if (!controls.sources.empty())
                    controls.addSource.SetSelectedWithoutCallback(0);
                controls.sourceSceneRevision = session->Scenes().Revision();
                controls.sourceProjectRoot = project.rootPath;
                controls.documentError.clear();
                RefreshStatusText(presentation);
            }

            void RefreshStatusText(const ScriptPresentation presentation)
            {
                auto& controls = Role(presentation);
                if (!controls.documentError.empty())
                {
                    controls.status.SetText(controls.documentError);
                    return;
                }
                const std::size_t count = VisibleAttachmentCount(presentation);
                std::string text = std::to_string(count) + " attached";
                if (controls.sources.empty())
                    text += " // no compatible sources";
                else
                    text += " // " + std::to_string(controls.sources.size()) + " sources";
                if (!controls.diagnostics.empty())
                    text += " // " + std::to_string(controls.diagnostics.size()) + " metadata issues";
                controls.status.SetText(text);
            }

            [[nodiscard]] std::size_t VisibleAttachmentCount(
                const ScriptPresentation presentation) const
            {
                const auto* session = bridge::StudioSession::Current();
                const auto& controls = Role(presentation);
                if (session == nullptr || !controls.documentReady ||
                    !bridge::IsValidStableId(controls.ownerEntityId))
                {
                    return 0;
                }
                return session->Scripts().EntityAttachments(
                    controls.ownerEntityId,
                    presentation).size();
            }

            void HideUnusedRows(
                ScriptRoleControls& controls,
                const std::size_t used)
            {
                for (std::size_t index = used; index < controls.rows.size(); ++index)
                    SetRowVisible(*controls.rows[index], false);
            }

            static void SetRowVisible(
                ScriptRowControls& row,
                const bool visible)
            {
                row.source.SetVisible(visible);
                row.enabled.SetVisible(visible);
                row.up.SetVisible(visible);
                row.down.SetVisible(visible);
                row.remove.SetVisible(visible);
                if (row.properties)
                    row.properties->SetVisible(visible);
            }

            static void SetRoleContentVisible(
                ScriptRoleControls& controls,
                const bool visible)
            {
                controls.status.SetVisible(visible);
                controls.addSource.SetVisible(visible);
                controls.add.SetVisible(visible);
                controls.refresh.SetVisible(visible);
                for (auto& row : controls.rows)
                    SetRowVisible(*row, visible);
            }

            void AddSelectedSource(const ScriptPresentation presentation)
            {
                auto& controls = Role(presentation);
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !controls.documentReady)
                    return;
                if (controls.sources.empty() ||
                    controls.selectedSource >= controls.sources.size())
                {
                    SetStatus("S4B SCRIPTING // no compatible source selected");
                    return;
                }

                StableId created;
                std::string error;
                if (!session->Scripts().AttachEntitySource(
                        controls.ownerEntityId,
                        controls.sources[controls.selectedSource],
                        created,
                        error))
                {
                    SetStatus("S4B SCRIPTING // " + error);
                    return;
                }
                SetStatus(
                    std::string("S4B SCRIPTING // ") + RoleLabel(presentation) +
                    " attached");
                RequestRefresh();
            }

            void SetEnabled(
                const ScriptPresentation presentation,
                const std::size_t index,
                const bool enabled)
            {
                auto& controls = Role(presentation);
                if (index >= controls.rows.size())
                    return;
                std::string error;
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr ||
                    !session->Scripts().SetAttachmentEnabled(
                        controls.rows[index]->scriptInstanceId,
                        enabled,
                        error))
                {
                    if (!error.empty())
                        SetStatus("S4B SCRIPTING // " + error);
                    RequestRefresh();
                    return;
                }
                RequestRefresh();
            }

            void Remove(
                const ScriptPresentation presentation,
                const std::size_t index)
            {
                auto& controls = Role(presentation);
                if (index >= controls.rows.size())
                    return;
                std::string error;
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr ||
                    !session->Scripts().RemoveAttachment(
                        controls.rows[index]->scriptInstanceId,
                        error))
                {
                    if (!error.empty())
                        SetStatus("S4B SCRIPTING // " + error);
                    return;
                }
                SetStatus("S4B SCRIPTING // attachment removed");
                RequestRefresh();
            }

            void Move(
                const ScriptPresentation presentation,
                const std::size_t index,
                const int direction)
            {
                auto& controls = Role(presentation);
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !controls.documentReady)
                    return;
                const auto attachments = session->Scripts().EntityAttachments(
                    controls.ownerEntityId,
                    presentation);
                if (index >= attachments.size())
                    return;
                if ((direction < 0 && index == 0) ||
                    (direction > 0 && index + 1 >= attachments.size()))
                {
                    return;
                }
                const std::size_t neighbour = direction < 0
                    ? index - 1
                    : index + 1;
                std::string error;
                if (!session->Scripts().MoveAttachment(
                        attachments[index]->scriptInstanceId,
                        attachments[neighbour]->order,
                        error))
                {
                    if (!error.empty())
                        SetStatus("S4B SCRIPTING // " + error);
                    return;
                }
                RequestRefresh();
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

            StudioRenderPath* owner_ = nullptr;
            wi::gui::Window* panel_ = nullptr;
            InspectorSectionRegistry* registry_ = nullptr;
            std::function<void()> requestRefresh_;
            std::function<void(std::string)> setStatus_;
            ScriptRoleControls action_;
            ScriptRoleControls script_;
        };

        bool ScriptRoleProvider::IsVisible(
            const InspectorSectionContext& context) const
        {
            return owner_ != nullptr && owner_->RoleVisible(presentation_, context);
        }

        float ScriptRoleProvider::MeasureContentHeight(
            const InspectorSectionContext&,
            const float) const
        {
            return owner_ == nullptr ? 0.0f : owner_->MeasureRole(presentation_);
        }

        void ScriptRoleProvider::Refresh(const InspectorSectionContext&)
        {
            if (owner_ != nullptr)
                owner_->RefreshRole(presentation_);
        }

        void ScriptRoleProvider::ApplyLayout(
            const InspectorSectionContext&,
            const InspectorSectionLayout& layout)
        {
            if (owner_ != nullptr)
                owner_->LayoutRole(presentation_, layout);
        }

        std::unique_ptr<ScriptAttachmentInspector> activeInspector;
        StudioRenderPath* activeOwner = nullptr;
    }

    void RegisterS4BScriptAttachmentInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus)
    {
        activeInspector.reset();
        activeOwner = &owner;
        activeInspector = std::make_unique<ScriptAttachmentInspector>(
            owner,
            inspectorPanel,
            registry,
            std::move(requestRefresh),
            std::move(setStatus));
        activeInspector->Register();
    }

    void PrepareS4BScriptAttachmentInspector(StudioRenderPath& owner)
    {
        if (activeOwner == &owner && activeInspector)
            activeInspector->PrepareForLayout();
    }
}
