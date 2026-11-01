#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/screen/ScreenRenderer.h"
#include "RenegadeScreenEditorAdvancedInspector.h"
#include "RenegadeScreenEditorWorkspace.h"
#include "StoryFlowScreenEditorHandoff.h"

namespace renegade::studio
{
    // First-class Screen Editor path. The inactive Story Flow and 3D Level
    // Editor paths do not tick or render while this path owns Studio. Gate 8D's
    // advanced Inspector is a sibling overlay; the accepted 8C workspace stays
    // intact underneath and remains available whenever ADVANCED is collapsed.
    class RenegadeScreenEditorRenderPath final : public wi::RenderPath2D
    {
    public:
        ~RenegadeScreenEditorRenderPath() override
        {
            if (loaded_)
            {
                renderer_.Reset(*this);
                GetGUI().RemoveWidget(&workspace_);
            }
        }

        void EnsureLoaded()
        {
            if (loaded_) return;

            advancedInspector_.Create();
            advancedInspector_.SetVisible(false);
            advancedInspector_.SetEnabled(false);

            workspace_.Create();
            workspace_.OnUiBudgetsRequested([this](){advancedInspector_.OpenUiBudgets();advancedExpanded_=true;workspace_.SetInspectorSuppressed(true);});
            workspace_.OnPortableLibraryRequested([this](bool publish){PortableLibrary(publish);});
            workspace_.OnGameplayAttributesRequested([this](){advancedInspector_.OpenGameplayAttributes();advancedExpanded_=true;workspace_.SetInspectorSuppressed(true);});
            workspace_.SetVisible(false);
            workspace_.SetEnabled(false);
            workspace_.OnDocumentChanged([this]()
            {
                previewDirty_ = true;
            });
            workspace_.OnAppearanceRequested([this]()
            {
                advancedInspector_.OpenAppearanceForSelection();
                advancedExpanded_ = true;
                workspace_.SetInspectorSuppressed(true);
            });
            workspace_.OnGraphicRequested([this](bool allSlots)
            {
                advancedInspector_.ChangeGraphicForSelection(allSlots);
            });
            workspace_.OnImageAssetRequested([this]()
            {
                advancedInspector_.OpenImageForSelection();
            });
            workspace_.OnProgressArtworkRequested([this](bool frame)
            {
                advancedInspector_.OpenProgressArtworkForSelection(frame);
            });
            workspace_.OnPreferencesChanged([this]()
            {
                if (projectRoot_.empty()) return;
                const auto settings = std::filesystem::u8path(projectRoot_) /
                    "Intermediate" / "Editor" / "ScreenWorkspace.txt";
                if (!workspace_.ExportPreferences().Save(settings))
                    wi::backlog::post("Screen Editor: could not save local workspace preferences",
                        wi::backlog::LogLevel::Warning);
            });
            workspace_.OnPanelLayoutChanged([this]()
            {
                // Resizing a Studio panel changes only preview geometry.
                // No ScreenDocument edit or expensive resource reload.
                renderer_.SetViewport(workspace_.PreviewViewport());
                renderer_.SetClipViewport(workspace_.PreviewBounds());
                renderer_.ApplyLayout(*this);
                advancedInspector_.SetPreferredPanelWidth(
                    workspace_.InspectorPanelWidth());
                advancedInspector_.SetLayout(
                    std::max(1.0f, GetLogicalWidth()),
                    std::max(1.0f, GetLogicalHeight()));
            });
            workspace_.OnReturnRequested([this]()
            {
                if (returnRequested_) returnRequested_();
            });

            // The workspace owns explicit Inspector overlay ordering inside
            // the existing GUI render pass. Register only one full-canvas root.
            workspace_.OnInspectorOverlay([this](
                const wi::Canvas& canvas, wi::graphics::CommandList cmd)
            {
                advancedInspector_.Render(canvas, cmd);
                if(advancedInspector_.IsSimulationActive()||advancedInspector_.AnimationPreview().active)
                {
                    wi::graphics::Rect clip={0,0,static_cast<int>(GetLogicalWidth()),static_cast<int>(GetLogicalHeight())};
                    wi::graphics::GetDevice()->BindScissorRects(1,&clip,cmd);
                    const auto bounds=workspace_.PreviewBounds();wi::font::Params label;
                    label.posX=bounds.x+8;label.posY=bounds.y+8;label.size=12;
                    label.color=wi::Color(238,128,38,255);
                    wi::font::Draw(advancedInspector_.IsSimulationActive()?
                        advancedInspector_.PreviewModeLabel():
                        "ANIMATION PREVIEW // CANVAS EDITING PAUSED",label,cmd);
                }
            });
            GetGUI().AddWidget(&workspace_);
            loaded_ = true;
            LayoutWorkspace();
        }

        void Load() override
        {
            EnsureLoaded();
        }

        void Start() override
        {
            EnsureLoaded();
            LayoutWorkspace();
            workspace_.SetVisible(open_);
            workspace_.SetEnabled(open_);
            advancedInspector_.SetVisible(open_);
            advancedInspector_.SetEnabled(open_);
            workspace_.SetInspectorSuppressed(open_ && advancedExpanded_);
        }

        void Stop() override
        {
            if (!loaded_) return;
            workspace_.SetInspectorSuppressed(false);
            workspace_.SetVisible(false);
            workspace_.SetEnabled(false);
            advancedInspector_.SetVisible(false);
            advancedInspector_.SetEnabled(false);
        }

        void ResizeLayout() override
        {
            wi::RenderPath2D::ResizeLayout();
            LayoutWorkspace();
            renderer_.ApplyLayout(*this);
        }

        void Update(const float dt) override
        {
            EnsureLoaded();
            LayoutWorkspace();
            SyncAdvancedInspectorMode();
            advancedInspector_.SetVisible(open_ && (!workspace_.IsFocusCanvas()||advancedInspector_.HasGameplayAttributeEditor()));
            advancedInspector_.SetEnabled(open_ && (!workspace_.IsFocusCanvas()||advancedInspector_.HasGameplayAttributeEditor()));
            if (previewDirty_)
            {
                previewDirty_ = false;
                std::string error;
                if (!ReloadPreview(error))
                {
                    bridge::ScreenDocument sourceCheck;
                    std::string sourceError;
                    if(!bridge::ResolveScreenTheme(projectRoot_,session_.Document(),sourceCheck,sourceError))
                    {
                        renderer_.Reset(*this);
                        previewSourceConflict_ = true;
                        workspace_.ReportPreviewConflict(sourceError);
                    }
                    wi::backlog::post(
                        "Renegade Screen Editor preview: " + error,
                        wi::backlog::LogLevel::Error);
                }
                else if(previewSourceConflict_)
                {
                    previewSourceConflict_ = false;
                    workspace_.ReportPreviewRecovered();
                }
            }
            advancedInspector_.ApplySimulationPreview(renderer_);
            advancedInspector_.TickAnimationPreview(dt);
            renderer_.SetAnimationPreview(advancedInspector_.AnimationPreview());
            renderer_.SetTransitionFrame(advancedInspector_.TransitionPreview());
            renderer_.ApplyLayout(*this);
            // A dismissal/selection click belongs to the popup for its whole
            // frame, including after its callback closes the popup.
            const bool workspacePopupWasOpen = workspace_.HasOpenPopup();
            const bool inspectorPopupWasOpen = advancedInspector_.HasOpenPopup();
            workspace_.SetOverlayInputBlocked(inspectorPopupWasOpen || advancedInspector_.AnimationPreview().active || advancedInspector_.IsSimulationActive());
            wi::RenderPath2D::Update(dt);
            if (!workspacePopupWasOpen && !workspace_.HasOpenPopup())
                advancedInspector_.Update(*this, dt);
            SyncAdvancedInspectorMode();
            // Observe both Basic and Advanced Inspector edits after GUI Update
            // so the quiet-period transaction contains settled values.
            if (open_) workspace_.TickAutosave(dt);
            // Dispatch editing shortcuts after native widgets finish updating.
            // Never steal Undo/Redo while a name, text or advanced field is active.
            if (open_ && !advancedInspector_.IsSimulationActive() && !advancedInspector_.AnimationPreview().active && !GetGUI().IsTyping() && !workspacePopupWasOpen &&
                !inspectorPopupWasOpen && !workspace_.HasOpenPopup() &&
                !advancedInspector_.HasOpenPopup())
            {
                workspace_.HandleCanvasNavigation();
                workspace_.HandleUndoRedoShortcuts();
                workspace_.HandleSelectionShortcuts();
                workspace_.HandleLayerOrderShortcuts();
                workspace_.HandleDeleteShortcut();
            }
        }

        void SyncCanvas(const wi::Canvas& canvas)
        {
            init(canvas);
            EnsureLoaded();
            LayoutWorkspace();
            renderer_.ApplyLayout(*this);
        }

        void SetLiveRuntimeProbe(std::function<std::string()> probe)
        {advancedInspector_.SetLiveRuntimeProbe(std::move(probe));}

        void SetBindingActorCatalogue(bridge::ScreenBindingActorCatalogue catalogue)
        { advancedInspector_.SetBindingActorCatalogue(std::move(catalogue)); }

        [[nodiscard]] bool OpenScreen(
            const StoryFlowScreenEditorHandoff& handoff,
            const std::string& projectRoot,
            const bridge::StableId& projectId,
            std::string& error)
        {
            EnsureLoaded();
            if (!handoff.ready || handoff.resolvedPath.empty() ||
                !bridge::IsValidStableId(handoff.screenDocumentId))
            {
                error = "Screen Editor received an incomplete Story Flow handoff.";
                return false;
            }

            bridge::ScreenAuthoringSession candidate;
            if (!candidate.Open(handoff.resolvedPath, projectId, error))
                return false;
            if (candidate.Document().envelope.documentId !=
                handoff.screenDocumentId)
            {
                error = "Screen Editor handoff identity does not match the opened document.";
                return false;
            }

            session_ = std::move(candidate);
            projectRoot_ = projectRoot;
            open_ = true;
            advancedExpanded_ = false;
            workspace_.Bind(&session_, &renderer_);
            ScreenEditorWorkspacePreferences prefs;
            const auto settings = std::filesystem::u8path(projectRoot_) /
                "Intermediate" / "Editor" / "ScreenWorkspace.txt";
            if (ScreenEditorWorkspacePreferences::Load(settings, prefs))
                workspace_.ImportPreferences(prefs);
            workspace_.SetInspectorSuppressed(false);
            advancedInspector_.Bind(
                &session_, &workspace_, projectRoot_,
                [this]() { previewDirty_ = true; });
            workspace_.SetVisible(true);
            workspace_.SetEnabled(true);
            advancedInspector_.SetVisible(true);
            advancedInspector_.SetEnabled(true);
            LayoutWorkspace();
            if (!ReloadPreview(error))
            {
                bridge::ScreenDocument sourceCheck;
                std::string sourceError;
                if (!bridge::ResolveScreenTheme(projectRoot_,session_.Document(),sourceCheck,sourceError))
                {
                    // The authored Screen is valid but its external source is
                    // unavailable or conflicted. Keep the authoring session,
                    // Layers and Inspector accessible so the creator can
                    // recover; never show a partial/stale Runtime preview.
                    renderer_.Reset(*this);
                    previewSourceConflict_ = true;
                    workspace_.ReportPreviewConflict(sourceError);
                    wi::backlog::post("Renegade Screen Editor repair mode: "+sourceError,
                        wi::backlog::LogLevel::Warning);
                    error.clear();
                    return true;
                }
                Clear();
                return false;
            }
            wi::backlog::post(
                "Renegade Screen Editor: creator authoring surface opened " +
                    handoff.resolvedPath,
                wi::backlog::LogLevel::Default);
            return true;
        }

        void Clear() noexcept
        {
            advancedInspector_.ResetAnimationPreview();
            if (loaded_) renderer_.Reset(*this);
            session_.Clear();
            projectRoot_.clear();
            advancedInspector_.Clear();
            workspace_.SetInspectorSuppressed(false);
            workspace_.Clear();
            if (loaded_)
            {
                workspace_.SetVisible(false);
                workspace_.SetEnabled(false);
                advancedInspector_.SetVisible(false);
                advancedInspector_.SetEnabled(false);
            }
            previewDirty_ = false;
            previewSourceConflict_ = false;
            open_ = false;
            advancedExpanded_ = false;
        }

        void OnReturnRequested(std::function<void()> callback)
        {
            returnRequested_ = std::move(callback);
        }

        void OnOpenLibraryDocument(std::function<void(const bridge::StableId&)> callback)
        {openLibraryRequested_=callback;workspace_.OnOpenLibraryDocument(std::move(callback));}
        void OnSetGameplayAttributes(RenegadeGameplayAttributeInspector::AttachCallback callback)
        {advancedInspector_.OnSetGameplayAttributes(std::move(callback));}
        void SetGameplayAttributeDocumentReference(std::string id,std::string hint)
        {advancedInspector_.SetGameplayAttributeDocumentReference(std::move(id),std::move(hint));}
        void OnSetProjectHud(RenegadeScreenEditorWorkspace::SetProjectHudCallback callback)
        {workspace_.OnSetProjectHud(std::move(callback));}
        void OnSetProjectInventory(RenegadeScreenEditorWorkspace::SetProjectInventoryCallback callback)
        {workspace_.OnSetProjectInventory(std::move(callback));}
        void OnReadProjectInventory(std::function<bridge::StableId()> callback)
        {workspace_.OnReadProjectInventory(std::move(callback));}
        void SetRouteContext(bridge::StableId context, std::function<bool(const bridge::StableId&)> choose)
        { workspace_.SetRouteContext(std::move(context),std::move(choose)); }
        void SetProjectDefaultHud(bridge::StableId id)
        {workspace_.SetProjectDefaultHud(std::move(id));}

        void OnOpenProjectScreen(
            std::function<void(const bridge::StableId&)> callback)
        {
            EnsureLoaded();
            workspace_.OnOpenProjectScreen(std::move(callback));
        }

        void OnOpenScreenRoute(
            RenegadeScreenEditorWorkspace::OpenScreenRouteCallback callback)
        {
            EnsureLoaded();
            workspace_.OnOpenScreenRoute(std::move(callback));
        }

        void OnRouteHistoryAction(
            RenegadeScreenEditorWorkspace::RouteHistoryCallback callback)
        {
            EnsureLoaded();
            workspace_.OnRouteHistoryAction(std::move(callback));
        }

        void OnCoordinatedSave(
            RenegadeScreenEditorWorkspace::CoordinatedSaveCallback callback)
        {
            EnsureLoaded();
            workspace_.OnCoordinatedSave(std::move(callback));
        }

        void OnExternalFlowDirty(std::function<bool()> callback)
        {
            EnsureLoaded();
            workspace_.OnExternalFlowDirty(std::move(callback));
        }

        void SetProjectScreenLibrary(
            std::vector<ScreenEditorLibraryScreen> screens)
        {
            EnsureLoaded();
            workspace_.SetProjectScreenLibrary(std::move(screens));
        }

        [[nodiscard]] bool IsOpen() const noexcept
        {
            return open_ && session_.IsLoaded();
        }

        [[nodiscard]] bool IsDirty() const noexcept
        {
            return session_.IsDirty();
        }

    private:
        std::function<void(const bridge::StableId&)> openLibraryRequested_;
        void PortableLibrary(bool publish)
        {
            if(!session_.IsLoaded()||projectRoot_.empty()||!workspace_.SavePendingBeforeNavigation())return;
            const auto projectId=session_.Document().envelope.projectId;
            const auto documentId=session_.Document().envelope.documentId;
            const auto root=projectRoot_;
            if(publish)
            {
                const auto length=GetEnvironmentVariableW(L"LOCALAPPDATA",nullptr,0);
                if(!length){workspace_.ReportLibraryStatus("PERSONAL UI LIBRARY LOCATION UNAVAILABLE");return;}
                std::wstring userRoot(length,L'\0');GetEnvironmentVariableW(L"LOCALAPPDATA",userRoot.data(),length);
                userRoot.resize(length-1);
                const auto library=std::filesystem::path(userRoot)/L"RenegadeStudio"/L"UILibrary";
                std::error_code ec;std::filesystem::create_directories(library,ec);
                if(ec){workspace_.ReportLibraryStatus("PERSONAL UI LIBRARY FOLDER UNAVAILABLE");return;}
                const auto destination=library/std::filesystem::u8path(bridge::GenerateStableId());
                std::string error;
                if(!bridge::PublishPortableScreenPackage(root,projectId,documentId,
                    session_.Document().envelope.pathHint,destination.generic_u8string(),error))
                {workspace_.ReportLibraryStatus("UI PACKAGE NOT PUBLISHED // "+error);return;}
                workspace_.ReportLibraryStatus("UI PACKAGE PUBLISHED // "+destination.filename().generic_u8string());
                wi::backlog::post("Personal UI package: "+destination.generic_u8string());
                return;
            }
            wi::helper::FileDialogParams dialog;dialog.type=wi::helper::FileDialogParams::OPEN;
            dialog.description="Adopt renegade-ui-package.json into this project";
            dialog.extensions={"json"};
            wi::helper::FileDialog(dialog,[this,root,projectId,documentId](const std::string& external)
            {
                if(external.empty())return;
                wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this,root,projectId,documentId,external](std::uint64_t)
                {
                    if(!session_.IsLoaded()||projectRoot_!=root||
                        session_.Document().envelope.projectId!=projectId||
                        session_.Document().envelope.documentId!=documentId)return;
                    if(!workspace_.SavePendingBeforeNavigation())return;
                    const auto manifest=std::filesystem::u8path(external);
                    if(manifest.filename()!="renegade-ui-package.json")
                    {workspace_.ReportLibraryStatus("CHOOSE renegade-ui-package.json");return;}
                    bridge::ScreenPortableAdoptionResult adopted;std::string error;
                    if(!bridge::AdoptPortableScreenPackage(root,projectId,manifest.parent_path().generic_u8string(),adopted,error))
                    {workspace_.ReportLibraryStatus("UI PACKAGE NOT ADOPTED // "+error);return;}
                    const auto status="UI PACKAGE ADOPTED // "+std::to_string(adopted.unresolvedBindings.size())+
                        " GAMEPLAY SOURCE(S) REQUIRE EXPLICIT REASSIGNMENT";
                    workspace_.ReportLibraryStatus(status);wi::backlog::post(status);
                    if(openLibraryRequested_)openLibraryRequested_(adopted.documentId);
                });
            });
        }
        void LayoutWorkspace()
        {
            if (!loaded_) return;
            const float width = std::max(1.0f, GetLogicalWidth());
            const float height = std::max(1.0f, GetLogicalHeight());
            if (std::abs(width - lastWidth_) < 0.01f &&
                std::abs(height - lastHeight_) < 0.01f)
                return;
            lastWidth_ = width;
            lastHeight_ = height;
            workspace_.SetPos(XMFLOAT2(0.0f, 0.0f));
            workspace_.SetLayout(width, height);
            advancedInspector_.SetPreferredPanelWidth(
                workspace_.InspectorPanelWidth());
            advancedInspector_.SetLayout(width, height);
            renderer_.SetViewport(workspace_.PreviewViewport());
                renderer_.SetClipViewport(workspace_.PreviewBounds());
        }

        void SyncAdvancedInspectorMode()
        {
            advancedExpanded_ = open_ && advancedInspector_.IsExpanded();
            workspace_.SetInspectorSuppressed(advancedExpanded_);
        }

        [[nodiscard]] bool ReloadPreview(std::string& error)
        {
            if (!open_ || !session_.IsLoaded())
            {
                error = "No Screen document is open for preview.";
                return false;
            }
            renderer_.SetViewport(workspace_.PreviewViewport());
                renderer_.SetClipViewport(workspace_.PreviewBounds());
            const bool loaded = renderer_.Load(
                session_.Document(), projectRoot_, *this,
                [this](const bridge::StableId& widgetId)
                {
                    workspace_.SelectWidget(widgetId);
                },
                error);
            if (loaded)
            {
                advancedInspector_.ResetSimulationPresentation();
                renderer_.SetInputEnabled(false);
                renderer_.ApplyLayout(*this);
            }
            return loaded;
        }

        bridge::ScreenAuthoringSession session_;
        screen::ScreenRenderer renderer_;
        RenegadeScreenEditorAdvancedInspector advancedInspector_;
        RenegadeScreenEditorWorkspace workspace_;
        std::function<void()> returnRequested_;
        std::string projectRoot_;
        bool loaded_ = false;
        bool open_ = false;
        bool previewDirty_ = false;
        bool previewSourceConflict_ = false;
        bool advancedExpanded_ = false;
        float lastWidth_ = -1.0f;
        float lastHeight_ = -1.0f;
    };
}
