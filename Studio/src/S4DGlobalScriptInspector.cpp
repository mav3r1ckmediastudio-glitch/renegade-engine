#include "S4DGlobalScriptInspector.h"

#include "InspectorSectionFramework.h"
#include "RenegadeStudioChrome.h"
#include "S4BScriptAttachmentInspector.h"
#include "S4CScriptPropertyControls.h"
#include "StudioApplication.h"

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

        class GlobalScriptInspector;

        class GlobalScriptProvider final : public IInspectorSectionProvider
        {
        public:
            GlobalScriptProvider(
                GlobalScriptInspector& owner,
                InspectorSectionDescriptor descriptor) noexcept
                : owner_(&owner)
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
            GlobalScriptInspector* owner_ = nullptr;
            InspectorSectionDescriptor descriptor_;
        };

        struct GlobalScriptRow
        {
            wi::gui::Label source;
            SceneInspectorCheckBox enabled;
            SceneInspectorButton up;
            SceneInspectorButton down;
            SceneInspectorButton remove;
            StableId scriptInstanceId;
            std::unique_ptr<S4CScriptPropertyEditor> properties;
        };

        class GlobalScriptInspector final
        {
        public:
            GlobalScriptInspector(
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
                header_.Create("S4D GLOBAL SCRIPT Section Header");
                header_.SetTooltip(
                    "Expand or collapse Level-wide GLOBAL SCRIPT attachments.");
                header_.OnClick([this](const wi::gui::EventArgs&)
                {
                    const bool opening =
                        !registry_->IsExpanded(S4DGlobalScriptSectionId);
                    for (const char* section : {
                        "transform", "rendering", "materials",
                        S4BActionSectionId, S4BScriptSectionId,
                        S4DGlobalScriptSectionId})
                    {
                        (void)registry_->SetExpanded(section, false);
                    }
                    if (opening)
                        (void)registry_->SetExpanded(
                            S4DGlobalScriptSectionId, true);
                    RequestRefresh();
                });
                panel_->AddWidget(&header_);

                status_.Create("S4D GLOBAL SCRIPT Status");
                status_.SetText("No GLOBAL SCRIPT attachments.");
                panel_->AddWidget(&status_);

                addSource_.Create("S4D GLOBAL SCRIPT Source");
                addSource_.SetTooltip(
                    "Choose a governed Content/Scripts Lua source whose S4A metadata role is GLOBAL SCRIPT.");
                addSource_.OnSelect([this](const wi::gui::EventArgs& args)
                {
                    selectedSource_ = static_cast<std::size_t>(args.userdata);
                });
                panel_->AddWidget(&addSource_);

                refresh_.Create("REFRESH");
                refresh_.SetTooltip(
                    "Rescan Content/Scripts and re-evaluate restricted S4A metadata.");
                refresh_.OnClick([this](const wi::gui::EventArgs&)
                {
                    RefreshSourcesIfNeeded(true);
                    RequestRefresh();
                });
                panel_->AddWidget(&refresh_);

                add_.Create("ADD");
                add_.SetTooltip("Attach the selected GLOBAL SCRIPT to this Level.");
                add_.OnClick([this](const wi::gui::EventArgs&)
                {
                    AddSelectedSource();
                });
                panel_->AddWidget(&add_);

                InspectorSectionDescriptor descriptor;
                descriptor.id = S4DGlobalScriptSectionId;
                descriptor.title = "GLOBAL SCRIPT";
                descriptor.order = 60;
                descriptor.defaultExpanded = false;
                descriptor.headerHeight = 28.0f;
                descriptor.spacingAfter = 6.0f;

                std::string error;
                auto provider = std::make_shared<GlobalScriptProvider>(
                    *this, std::move(descriptor));
                if (!registry_->Register(std::move(provider), error))
                    SetStatus("S4D GLOBAL SCRIPT // " + error);

                PrepareForLayout();
            }

            [[nodiscard]] bool IsVisible(
                const InspectorSectionContext&) const
            {
                const auto* session = bridge::StudioSession::Current();
                return session != nullptr &&
                    session->Projects().HasProject() &&
                    !session->Scenes().CurrentPath().empty();
            }

            [[nodiscard]] float Measure() const
            {
                const auto* session = bridge::StudioSession::Current();
                std::vector<const ScriptAttachment*> attachments;
                if (session != nullptr && documentReady_)
                {
                    attachments = session->Scripts().LevelAttachments(
                        ScriptPresentation::GlobalScript);
                }
                float height = 60.0f;
                for (std::size_t index = 0;
                    index < attachments.size() && index < rows_.size();
                    ++index)
                {
                    height += 36.0f;
                    if (rows_[index]->properties)
                        height += rows_[index]->properties->MeasureHeight();
                }
                return height;
            }

            void Refresh()
            {
                documentReady_ = false;
                documentError_.clear();
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr ||
                    !session->Projects().HasProject() ||
                    session->Scenes().CurrentPath().empty())
                {
                    HideUnusedRows(0);
                    return;
                }

                std::string error;
                if (!session->Scripts().EnsureCurrent(error))
                {
                    documentError_ = std::move(error);
                    RefreshStatusText();
                    HideUnusedRows(0);
                    return;
                }
                documentReady_ = true;

                if (registry_->IsExpanded(S4DGlobalScriptSectionId))
                    RefreshSourcesIfNeeded(false);

                const auto attachments = session->Scripts().LevelAttachments(
                    ScriptPresentation::GlobalScript);
                EnsureRows(attachments.size());
                for (std::size_t index = 0; index < attachments.size(); ++index)
                    RefreshRow(index, *attachments[index]);
                HideUnusedRows(attachments.size());
                RefreshStatusText();
            }

            void Layout(const InspectorSectionLayout& layout)
            {
                header_.SetVisible(true);
                header_.SetPos(XMFLOAT2(12.0f, layout.top));
                header_.SetSize(XMFLOAT2(layout.width, layout.headerHeight));
                header_.SetText(
                    std::string(layout.expanded ? "▼  " : "▶  ") +
                    "GLOBAL SCRIPT");

                if (!layout.expanded)
                {
                    SetContentVisible(false);
                    return;
                }

                RefreshSourcesIfNeeded(false);
                const float x = 12.0f;
                const float width = layout.width;
                const float top = layout.contentTop;

                status_.SetVisible(true);
                status_.SetPos(XMFLOAT2(x, top));
                status_.SetSize(XMFLOAT2(width, 24.0f));

                const float refreshWidth = 64.0f;
                const float addWidth = 54.0f;
                const float gap = 6.0f;
                const float sourceWidth = std::max(
                    80.0f,
                    width - refreshWidth - addWidth - gap * 2.0f);
                addSource_.SetVisible(true);
                addSource_.SetPos(XMFLOAT2(x, top + 26.0f));
                addSource_.SetSize(XMFLOAT2(sourceWidth, 28.0f));
                refresh_.SetVisible(true);
                refresh_.SetPos(XMFLOAT2(
                    x + sourceWidth + gap,
                    top + 26.0f));
                refresh_.SetSize(XMFLOAT2(refreshWidth, 28.0f));
                add_.SetVisible(true);
                add_.SetPos(XMFLOAT2(
                    x + sourceWidth + gap + refreshWidth + gap,
                    top + 26.0f));
                add_.SetSize(XMFLOAT2(addWidth, 28.0f));
                addSource_.SetEnabled(documentReady_ && !sources_.empty());
                add_.SetEnabled(documentReady_ && !sources_.empty());

                auto* session = bridge::StudioSession::Current();
                std::vector<const ScriptAttachment*> attachments;
                if (session != nullptr && documentReady_)
                {
                    attachments = session->Scripts().LevelAttachments(
                        ScriptPresentation::GlobalScript);
                }

                float rowY = top + 60.0f;
                for (std::size_t index = 0; index < attachments.size(); ++index)
                {
                    auto& row = *rows_[index];
                    const float enabledWidth = 62.0f;
                    const float buttonWidth = 42.0f;
                    const float removeWidth = 58.0f;
                    const float rowGap = 4.0f;
                    const float labelWidth = std::max(
                        70.0f,
                        width - enabledWidth - buttonWidth * 2.0f -
                            removeWidth - rowGap * 4.0f);

                    row.source.SetVisible(true);
                    row.source.SetPos(XMFLOAT2(x, rowY));
                    row.source.SetSize(XMFLOAT2(labelWidth, 28.0f));
                    row.enabled.SetVisible(true);
                    row.enabled.SetPos(XMFLOAT2(
                        x + labelWidth + rowGap, rowY));
                    row.enabled.SetSize(XMFLOAT2(enabledWidth, 28.0f));
                    row.up.SetVisible(true);
                    row.up.SetPos(XMFLOAT2(
                        x + labelWidth + rowGap + enabledWidth + rowGap,
                        rowY));
                    row.up.SetSize(XMFLOAT2(buttonWidth, 28.0f));
                    row.down.SetVisible(true);
                    row.down.SetPos(XMFLOAT2(
                        x + labelWidth + rowGap + enabledWidth + rowGap +
                            buttonWidth + rowGap,
                        rowY));
                    row.down.SetSize(XMFLOAT2(buttonWidth, 28.0f));
                    row.remove.SetVisible(true);
                    row.remove.SetPos(XMFLOAT2(x + width - removeWidth, rowY));
                    row.remove.SetSize(XMFLOAT2(removeWidth, 28.0f));
                    row.up.SetEnabled(index > 0);
                    row.down.SetEnabled(index + 1 < attachments.size());

                    if (row.properties)
                    {
                        row.properties->SetVisible(true);
                        row.properties->Layout(
                            x + 12.0f,
                            rowY + 32.0f,
                            std::max(70.0f, width - 12.0f));
                        rowY += 36.0f + row.properties->MeasureHeight();
                    }
                    else
                    {
                        rowY += 36.0f;
                    }
                }
                HideUnusedRows(attachments.size());
            }

            void PrepareForLayout()
            {
                header_.SetVisible(false);
                SetContentVisible(false);
            }

        private:
            void EnsureRows(const std::size_t count)
            {
                while (rows_.size() < count)
                {
                    const std::size_t index = rows_.size();
                    auto row = std::make_unique<GlobalScriptRow>();
                    row->properties = std::make_unique<S4CScriptPropertyEditor>(
                        *panel_,
                        [this]() { RequestRefresh(); },
                        [this](std::string status)
                        {
                            SetStatus(std::move(status));
                        });

                    row->source.Create(
                        "S4D GLOBAL SCRIPT Source " + std::to_string(index));
                    panel_->AddWidget(&row->source);

                    row->enabled.Create("ON");
                    row->enabled.SetTooltip(
                        "Enable or disable this Level-wide script instance.");
                    row->enabled.OnClick(
                        [this, index](const wi::gui::EventArgs& args)
                        {
                            SetEnabled(index, args.bValue);
                        });
                    panel_->AddWidget(&row->enabled);

                    row->up.Create("UP");
                    row->up.SetTooltip(
                        "Move this GLOBAL SCRIPT earlier in Level execution order.");
                    row->up.OnClick([this, index](const wi::gui::EventArgs&)
                    {
                        Move(index, -1);
                    });
                    panel_->AddWidget(&row->up);

                    row->down.Create("DOWN");
                    row->down.SetTooltip(
                        "Move this GLOBAL SCRIPT later in Level execution order.");
                    row->down.OnClick([this, index](const wi::gui::EventArgs&)
                    {
                        Move(index, 1);
                    });
                    panel_->AddWidget(&row->down);

                    row->remove.Create("REMOVE");
                    row->remove.SetTooltip(
                        "Detach this GLOBAL SCRIPT from the Level.");
                    row->remove.OnClick([this, index](const wi::gui::EventArgs&)
                    {
                        Remove(index);
                    });
                    panel_->AddWidget(&row->remove);
                    rows_.push_back(std::move(row));
                }
            }

            void RefreshRow(
                const std::size_t index,
                const ScriptAttachment& attachment)
            {
                if (index >= rows_.size())
                    return;
                auto& row = *rows_[index];
                row.scriptInstanceId = attachment.scriptInstanceId;

                std::string display =
                    fs::u8path(attachment.sourcePath).filename().generic_u8string();
                const auto source = std::find_if(
                    sources_.begin(), sources_.end(),
                    [&](const ScriptAuthoringSource& candidate)
                    {
                        return candidate.sourcePath == attachment.sourcePath;
                    });
                const bridge::ScriptMetadataDescriptor* metadata = nullptr;
                if (source != sources_.end())
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

            void RefreshSourcesIfNeeded(const bool force)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !documentReady_ ||
                    !session->Projects().HasProject())
                {
                    return;
                }
                const auto& project = session->Projects().CurrentProject();
                if (!force &&
                    sourceSceneRevision_ == session->Scenes().Revision() &&
                    sourceProjectRoot_ == project.rootPath)
                {
                    return;
                }

                sources_.clear();
                diagnostics_.clear();
                addSource_.ClearItems();
                selectedSource_ = 0;

                std::string error;
                if (!session->Scripts().EnumerateProjectSources(
                        ScriptPresentation::GlobalScript,
                        sources_,
                        diagnostics_,
                        error))
                {
                    documentError_ = std::move(error);
                    sourceSceneRevision_ = session->Scenes().Revision();
                    sourceProjectRoot_ = project.rootPath;
                    RefreshStatusText();
                    return;
                }

                for (std::size_t index = 0; index < sources_.size(); ++index)
                {
                    const auto& source = sources_[index];
                    const std::string label = source.metadata.category.empty()
                        ? source.metadata.name
                        : source.metadata.category + " / " + source.metadata.name;
                    addSource_.AddItem(
                        label,
                        static_cast<std::uint64_t>(index));
                }
                if (!sources_.empty())
                    addSource_.SetSelectedWithoutCallback(0);
                sourceSceneRevision_ = session->Scenes().Revision();
                sourceProjectRoot_ = project.rootPath;
                documentError_.clear();
                RefreshStatusText();
            }

            void RefreshStatusText()
            {
                if (!documentError_.empty())
                {
                    status_.SetText(documentError_);
                    return;
                }
                const auto* session = bridge::StudioSession::Current();
                const std::size_t count = session != nullptr && documentReady_
                    ? session->Scripts().LevelAttachments(
                        ScriptPresentation::GlobalScript).size()
                    : 0;
                std::string text = std::to_string(count) + " attached";
                if (sources_.empty())
                    text += " // no compatible sources";
                else
                    text += " // " + std::to_string(sources_.size()) + " sources";
                if (!diagnostics_.empty())
                    text += " // " + std::to_string(diagnostics_.size()) +
                        " metadata issues";
                status_.SetText(text);
            }

            void AddSelectedSource()
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !documentReady_)
                    return;
                if (sources_.empty() || selectedSource_ >= sources_.size())
                {
                    SetStatus("S4D GLOBAL SCRIPT // no compatible source selected");
                    return;
                }

                StableId created;
                std::string error;
                if (!session->Scripts().AttachLevelSource(
                        sources_[selectedSource_],
                        created,
                        error))
                {
                    SetStatus("S4D GLOBAL SCRIPT // " + error);
                    return;
                }
                SetStatus("S4D GLOBAL SCRIPT // attached");
                RequestRefresh();
            }

            void SetEnabled(const std::size_t index, const bool enabled)
            {
                if (index >= rows_.size())
                    return;
                std::string error;
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr ||
                    !session->Scripts().SetAttachmentEnabled(
                        rows_[index]->scriptInstanceId,
                        enabled,
                        error))
                {
                    if (!error.empty())
                        SetStatus("S4D GLOBAL SCRIPT // " + error);
                    RequestRefresh();
                    return;
                }
                RequestRefresh();
            }

            void Remove(const std::size_t index)
            {
                if (index >= rows_.size())
                    return;
                std::string error;
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr ||
                    !session->Scripts().RemoveAttachment(
                        rows_[index]->scriptInstanceId,
                        error))
                {
                    if (!error.empty())
                        SetStatus("S4D GLOBAL SCRIPT // " + error);
                    return;
                }
                SetStatus("S4D GLOBAL SCRIPT // attachment removed");
                RequestRefresh();
            }

            void Move(const std::size_t index, const int direction)
            {
                auto* session = bridge::StudioSession::Current();
                if (session == nullptr || !documentReady_)
                    return;
                const auto attachments = session->Scripts().LevelAttachments(
                    ScriptPresentation::GlobalScript);
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
                        SetStatus("S4D GLOBAL SCRIPT // " + error);
                    return;
                }
                RequestRefresh();
            }

            void HideUnusedRows(const std::size_t used)
            {
                for (std::size_t index = used; index < rows_.size(); ++index)
                    SetRowVisible(*rows_[index], false);
            }

            static void SetRowVisible(GlobalScriptRow& row, const bool visible)
            {
                row.source.SetVisible(visible);
                row.enabled.SetVisible(visible);
                row.up.SetVisible(visible);
                row.down.SetVisible(visible);
                row.remove.SetVisible(visible);
                if (row.properties)
                    row.properties->SetVisible(visible);
            }

            void SetContentVisible(const bool visible)
            {
                status_.SetVisible(visible);
                addSource_.SetVisible(visible);
                refresh_.SetVisible(visible);
                add_.SetVisible(visible);
                for (auto& row : rows_)
                    SetRowVisible(*row, visible);
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

            SceneInspectorButton header_;
            wi::gui::Label status_;
            SceneInspectorComboBox addSource_;
            SceneInspectorButton refresh_;
            SceneInspectorButton add_;
            std::vector<std::unique_ptr<GlobalScriptRow>> rows_;
            std::vector<ScriptAuthoringSource> sources_;
            std::vector<bridge::ScriptMetadataDiagnostic> diagnostics_;
            std::size_t selectedSource_ = 0;
            std::uint64_t sourceSceneRevision_ =
                std::numeric_limits<std::uint64_t>::max();
            std::string sourceProjectRoot_;
            bool documentReady_ = false;
            std::string documentError_;
        };

        bool GlobalScriptProvider::IsVisible(
            const InspectorSectionContext& context) const
        {
            return owner_ != nullptr && owner_->IsVisible(context);
        }

        float GlobalScriptProvider::MeasureContentHeight(
            const InspectorSectionContext&,
            const float) const
        {
            return owner_ == nullptr ? 0.0f : owner_->Measure();
        }

        void GlobalScriptProvider::Refresh(const InspectorSectionContext&)
        {
            if (owner_ != nullptr)
                owner_->Refresh();
        }

        void GlobalScriptProvider::ApplyLayout(
            const InspectorSectionContext&,
            const InspectorSectionLayout& layout)
        {
            if (owner_ != nullptr)
                owner_->Layout(layout);
        }

        std::unique_ptr<GlobalScriptInspector> activeGlobalInspector;
        StudioRenderPath* activeGlobalOwner = nullptr;
    }

    void RegisterS4DGlobalScriptInspector(
        StudioRenderPath& owner,
        wi::gui::Window& inspectorPanel,
        InspectorSectionRegistry& registry,
        std::function<void()> requestRefresh,
        std::function<void(std::string)> setStatus)
    {
        activeGlobalInspector.reset();
        activeGlobalOwner = &owner;
        activeGlobalInspector = std::make_unique<GlobalScriptInspector>(
            owner,
            inspectorPanel,
            registry,
            std::move(requestRefresh),
            std::move(setStatus));
        activeGlobalInspector->Register();
    }

    void PrepareS4DGlobalScriptInspector(StudioRenderPath& owner)
    {
        if (activeGlobalOwner == &owner && activeGlobalInspector)
            activeGlobalInspector->PrepareForLayout();
    }
}
