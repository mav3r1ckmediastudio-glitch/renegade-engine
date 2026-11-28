#include "renegade/bridge/ScreenAmmoCounter.h"
#include "renegade/bridge/WorldLocationService.h"
#include "ScreenNavigationEditor.h"
#include "ScreenMinimapEditor.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/ScreenClickSound.h"
#include "renegade/bridge/DialogueAssetService.h"
#include "RenegadeScreenEditorWorkspace.h"
#include "ScreenHintEditor.h"
#include "ScreenCarouselEditor.h"
#include "ScreenLiquidEditor.h"
#include "renegade/bridge/ScreenPrefabService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include "renegade/bridge/CreatorVideoWorkflowService.h"
#include "renegade/bridge/VideoAssetService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ScreenBackgroundPanel.h"
#include <cstdio>
#include "StudioTheme.h"
#include "StoryFlowOverviewPolicy.h"
#include "ScreenCanvasGrid.h"
#include "ScreenElementSnap.h"
#include "renegade/bridge/ScreenWidgetPresets.h"
#include "renegade/bridge/DialogueDisplayService.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>
#include <string_view>

namespace
{
    constexpr float HeaderHeight = 64.0f;
    constexpr float FooterHeight = 44.0f;
    constexpr float PanelGap = 14.0f;
    constexpr float HierarchyRowHeight = 32.0f;
    constexpr float HierarchyRowsTop = HeaderHeight + 160.0f;
    constexpr float ResizeHandleSize = 12.0f;

    constexpr wi::Color Background=renegade::studio::theme::Canvas;
    constexpr wi::Color Surface=renegade::studio::theme::Panel;
    constexpr wi::Color Raised=renegade::studio::theme::Control;
    constexpr wi::Color Border=renegade::studio::theme::Border;
    constexpr wi::Color Text(244, 244, 244, 255);
    constexpr wi::Color Muted(135, 151, 159, 255);
    constexpr wi::Color Cyan=renegade::studio::theme::Accent;
    constexpr wi::Color Orange(222, 91, 29, 255);

    void Rect(
        const float x, const float y, const float width, const float height,
        const wi::Color color, const wi::graphics::CommandList cmd)
    {
        if (width <= 0.0f || height <= 0.0f) return;
        wi::image::Params params(x, y, width, height, color);
        params.blendFlag = wi::enums::BLENDMODE_ALPHA;
        wi::image::Draw(nullptr, params, cmd);
    }

    void Outline(
        const float x, const float y, const float width, const float height,
        const wi::Color color, const float thickness,
        const wi::graphics::CommandList cmd)
    {
        Rect(x, y, width, thickness, color, cmd);
        Rect(x, y + height - thickness, width, thickness, color, cmd);
        Rect(x, y, thickness, height, color, cmd);
        Rect(x + width - thickness, y, thickness, height, color, cmd);
    }

    void Label(
        const std::string& value, const float x, const float y,
        const int size, const wi::Color color,
        const wi::graphics::CommandList cmd)
    {
        wi::font::Params params(
            x, y, size, wi::font::WIFALIGN_LEFT,
            wi::font::WIFALIGN_TOP, color, wi::Color::Transparent());
        params.bolden = 0.12f;
        wi::font::Draw(value, params, cmd);
    }

    bool Contains(
        const renegade::bridge::ScreenRect& bounds,
        const XMFLOAT4& pointer) noexcept
    {
        return pointer.x >= bounds.x && pointer.y >= bounds.y &&
            pointer.x < bounds.x + bounds.width &&
            pointer.y < bounds.y + bounds.height;
    }

    std::string Trim(std::string value)
    {
        const auto whitespace = [](const unsigned char character)
        {
            return std::isspace(character) != 0;
        };
        while (!value.empty() && whitespace(value.front()))
            value.erase(value.begin());
        while (!value.empty() && whitespace(value.back()))
            value.pop_back();
        return value;
    }


    std::string Shorten(std::string value, const std::size_t limit)
    {
        if (value.size() <= limit) return value;
        value.resize(limit > 3 ? limit - 3 : limit);
        if (limit > 3) value += "...";
        return value;
    }
}

namespace renegade::studio
{
    void RenegadeScreenEditorWorkspace::Create()
    {
        carouselEditor_=std::make_shared<ScreenCarouselEditor>();
        carouselEditor_->Create();
        liquidEditor_=std::make_shared<ScreenLiquidEditor>();liquidEditor_->Create();
        navigationEditor_=std::make_shared<ScreenNavigationEditor>();navigationEditor_->Create();
        minimapEditor_=std::make_shared<ScreenMinimapEditor>();minimapEditor_->Create();
        SetName("Renegade Screen Editor workspace");
        SetShadowRadius(0.0f);
        CreateControls();
        SetLayout(width_, height_);
    }

    void RenegadeScreenEditorWorkspace::SetLayout(
        const float width,
        const float height)
    {
        width_ = std::max(1.0f, width);
        height_ = std::max(1.0f, height);
        // Create() is called before the first real Studio canvas arrives.
        // Preserve design defaults until a usable viewport is established.
        if (!focusCanvas_ && !leftCollapsed_ && !rightCollapsed_ && width_ >= ScreenEditorPanelLayout::MinLeft +
                ScreenEditorPanelLayout::MinRight +
                2.0f * ScreenEditorPanelLayout::Gap + 1.0f)
            panels_.Clamp(width_);
        SetSize(XMFLOAT2(width_, height_));
        LayoutControls();
    }

    void RenegadeScreenEditorWorkspace::Bind(
        bridge::ScreenAuthoringSession* session,
        screen::ScreenRenderer* renderer)
    {
        session_ = session;
        renderer_ = renderer;
        selectedWidgetId_.clear();
        selectedWidgetIds_.clear();
        paletteButtonPending_ = false;
        stateOwner_.clear();
        inventoryElementMode_=false;
        camera_.Reset();
        cameraPanning_ = false;
        EnsureSelection();
        LoadProjectGrid();
        timeline_.Reset(session_ ? session_->UndoCount() : 0,
            session_ ? session_->RedoCount() : 0, session_ ? session_->HistoryMutation() : 0);
        autosave_.Reset();
        status_ = session_ && session_->IsLoaded()
            ? "SCREEN READY // SHARED RUNTIME PREVIEW"
            : "NO SCREEN OPEN";
        RefreshInspector();
    }

    void RenegadeScreenEditorWorkspace::Clear() noexcept
    {
        if(carouselEditor_)carouselEditor_->Close();
        if(liquidEditor_)liquidEditor_->Close();liquidEditorOpen_=false;
        if(navigationEditor_)navigationEditor_->Close();navigationEditorOpen_=false;
        if(minimapEditor_)minimapEditor_->Close();minimapEditorOpen_=false;
        carouselEditorOpen_=false;
        session_ = nullptr;
        renderer_ = nullptr;
        selectedWidgetId_.clear();
        selectedWidgetIds_.clear();
        hierarchyScroll_ = 0;
        basicActionPicker_.Close();
        destinationPicker_.Close();
        layerDragId_.clear();
        layerDropTargetId_.clear();
        layerDragActive_ = false;
        timeline_.Reset(0, 0);
        autosave_.Reset();
        panelDrag_ = PanelDrag::None;
        status_ = "NO SCREEN OPEN";
    }

    void RenegadeScreenEditorWorkspace::SelectWidget(
        const bridge::StableId& widgetId, const bool additive)
    {
        if (!session_ || !session_->FindWidget(widgetId)) return;
        if (!additive) selectedWidgetIds_.clear();
        inventoryElementMode_=false;
        if (additive && selectedWidgetIds_.count(widgetId))
        {
            selectedWidgetIds_.erase(widgetId);
            selectedWidgetId_ = selectedWidgetIds_.empty() ? bridge::StableId{} : *selectedWidgetIds_.begin();
        }
        else
        {
            selectedWidgetIds_.insert(widgetId);
            selectedWidgetId_ = widgetId;
        }
        basicActionPicker_.Close();
        destinationPicker_.Close();
        RefreshInspector();
        SetStatus(std::to_string(selectedWidgetIds_.size()) + " SELECTED // CTRL+CLICK TO EXTEND; CTRL+C/V/D; CTRL+G TO GROUP");
    }

    void RenegadeScreenEditorWorkspace::OnPreferencesChanged(
        std::function<void()> callback)
    {
        preferencesChanged_ = std::move(callback);
    }

    void RenegadeScreenEditorWorkspace::OnExternalFlowDirty(
        std::function<bool()> callback)
    {
        externalFlowDirty_ = std::move(callback);
    }

    ScreenEditorWorkspacePreferences
        RenegadeScreenEditorWorkspace::ExportPreferences() const
    {
        ScreenEditorWorkspacePreferences prefs;
        const auto& physical = focusCanvas_ ? savedPanels_ : panels_;
        prefs.left = leftCollapsed_ ? preferredLeft_ : physical.left;
        prefs.leftCollapsed=leftCollapsed_;prefs.rightCollapsed=rightCollapsed_;
        prefs.right = rightCollapsed_ ? preferredRight_ : physical.right;
        prefs.tab = static_cast<int>(toolboxTab_);
        prefs.spacing = gridSpacing_;
        prefs.grid = gridVisible_;
        prefs.snap = gridSnapEnabled_;
        return prefs;
    }

    void RenegadeScreenEditorWorkspace::ImportPreferences(
        const ScreenEditorWorkspacePreferences& prefs)
    {
        if (!prefs.Valid()) return;
        focusCanvas_ = false;
        panels_.left = prefs.left;
        panels_.right = prefs.right;
        preferredLeft_=prefs.left;preferredRight_=prefs.right;
        leftCollapsed_=prefs.leftCollapsed;rightCollapsed_=prefs.rightCollapsed;
        if(leftCollapsed_)panels_.left=0;if(rightCollapsed_)panels_.right=0;
        savedPanels_ = panels_;
        toolboxTab_ = static_cast<ScreenToolboxTab>(prefs.tab);
        if(!session_||!session_->IsLoaded()){gridSpacing_=16;gridVisible_=gridSnapEnabled_=true;}
        gridVisibleButton_.SetText(gridVisible_ ? "GRID ON" : "GRID OFF");
        gridSnapButton_.SetText(gridSnapEnabled_ ? "SNAP ON" : "SNAP OFF");
        gridSpacingInput_.SetValue(std::to_string(gridSpacing_));
        focusButton_.SetText("FOCUS");
        SetLayout(width_, height_);
        if (panelLayoutChanged_) panelLayoutChanged_();
    }

    void RenegadeScreenEditorWorkspace::NotifyPreferencesChanged()
    {
        if (preferencesChanged_) preferencesChanged_();
    }

    void RenegadeScreenEditorWorkspace::ToggleFocusCanvas()
    {
        if (directManipulationMode_ != DirectManipulationMode::None ||
            panelDrag_ != PanelDrag::None || !session_ || !session_->IsLoaded())
            return;
        if (focusCanvas_)
        {
            panels_ = savedPanels_;
            focusCanvas_ = false;
            if(!leftCollapsed_&&!rightCollapsed_)panels_.Clamp(width_);
            focusButton_.SetText("FOCUS");
            SetStatus("CANVAS FOCUS OFF // PANELS RESTORED");
        }
        else
        {
            savedPanels_ = panels_;
            focusCanvas_ = true;
            panels_.left = 0.0f;
            panels_.right = 0.0f;
            basicActionPicker_.Close();
            destinationPicker_.Close();
            focusButton_.SetText("PANELS");
            SetStatus("CANVAS FOCUS // PRESS PANELS TO RETURN");
        }
        LayoutControls();
        if (panelLayoutChanged_) panelLayoutChanged_();
        NotifyPreferencesChanged();
    }

    void RenegadeScreenEditorWorkspace::OnDocumentChanged(
        std::function<void()> callback)
    {
        documentChanged_ = std::move(callback);
    }

    void RenegadeScreenEditorWorkspace::OnPanelLayoutChanged(
        std::function<void()> callback)
    {
        panelLayoutChanged_ = std::move(callback);
    }

    void RenegadeScreenEditorWorkspace::OnOpenLibraryDocument(std::function<void(const bridge::StableId&)> callback)
    {openLibraryDocument_=std::move(callback);}
    void RenegadeScreenEditorWorkspace::OnSetProjectHud(SetProjectHudCallback callback)
    {setProjectHud_=std::move(callback);}
    void RenegadeScreenEditorWorkspace::OnSetProjectInventory(SetProjectInventoryCallback callback)
    {setProjectInventory_=std::move(callback);}
    void RenegadeScreenEditorWorkspace::OnReadProjectInventory(std::function<bridge::StableId()> callback)
    {readProjectInventory_=std::move(callback);}
    void RenegadeScreenEditorWorkspace::OpenProjectInventoryPicker()
    {
        if(!setProjectInventory_){SetStatus("INVENTORY SCREEN SETTINGS UNAVAILABLE");return;}
        const bridge::StableId current=readProjectInventory_?readProjectInventory_():bridge::StableId{};
        std::vector<RenegadeScreenChoicePicker::Choice> choices={{"Use basic inventory (no custom Screen)",""}};
        std::unordered_set<std::string> seen;
        for(const auto& screen:projectScreens_)
        {
            if(!screen.available || screen.enabledInGame || screen.category!="OVERLAYS" ||
               screen.documentId.empty() || !seen.insert(screen.documentId).second) continue;
            // Screen-level eligibility is checked again by the project-owning
            // service. The picker only lists disabled authored Overlays.
            choices.push_back({screen.name,screen.documentId});
        }
        screenSettingsPicker_.Open("PROJECT INVENTORY SCREEN",std::move(choices),current,
            [this](const std::string& id)
            {
                std::string path;
                if(!id.empty())
                {
                    const auto found=std::find_if(projectScreens_.begin(),projectScreens_.end(),
                        [&](const auto& screen) {
                            return screen.documentId==id && screen.available &&
                                !screen.enabledInGame && screen.category=="OVERLAYS";
                        });
                    if(found==projectScreens_.end())
                    {SetStatus("INVENTORY SCREEN UNAVAILABLE // REFRESH SCREEN LIBRARY");return;}
                    path=found->pathHint;
                }
                std::string error;
                if(!setProjectInventory_(id,path,error))
                {SetStatus("INVENTORY SCREEN NOT CHANGED // "+error);return;}
                SetStatus(id.empty()?"BASIC INVENTORY SCREEN RESTORED":
                    "PROJECT INVENTORY SCREEN SAVED // INCLUDED IN GAME BUILDS");
            });
    }

    void RenegadeScreenEditorWorkspace::OpenProjectHudPicker()
    {
        if(!setProjectHud_){SetStatus("PROJECT HUD SETTINGS UNAVAILABLE");return;}
        std::vector<RenegadeScreenChoicePicker::Choice> choices={{"No project HUD",""}};
        std::unordered_set<std::string> seen;
        for(const auto& screen:projectScreens_)
            if(screen.available && screen.enabledInGame && screen.category=="HUDS" &&
                !screen.documentId.empty() && seen.insert(screen.documentId).second)
                choices.push_back({screen.name,screen.documentId});
        screenSettingsPicker_.Open("PROJECT HUD DEFAULT",std::move(choices),projectDefaultHud_,
            [this](const std::string& id)
            {
                std::string path;
                if(!id.empty())
                {
                    const auto found=std::find_if(projectScreens_.begin(),projectScreens_.end(),
                        [&](const auto& screen){return screen.documentId==id && screen.available && screen.enabledInGame;});
                    if(found==projectScreens_.end()){SetStatus("HUD IS NO LONGER AVAILABLE // REFRESH THE LIBRARY");return;}
                    path=found->pathHint;
                }
                std::string error;
                if(!setProjectHud_(id,path,error)){SetStatus("PROJECT HUD SAVE FAILED // "+error);return;}
                projectDefaultHud_=id;SetStatus(id.empty()?"PROJECT HUD CLEARED":"PROJECT HUD DEFAULT SAVED");
            });
    }

    void RenegadeScreenEditorWorkspace::OnOpenProjectScreen(
        std::function<void(const bridge::StableId&)> callback)
    {
        openProjectScreen_ = std::move(callback);
    }

    void RenegadeScreenEditorWorkspace::OnOpenScreenRoute(
        OpenScreenRouteCallback callback)
    {
        openScreenRoute_ = std::move(callback);
    }

    void RenegadeScreenEditorWorkspace::OnRouteHistoryAction(
        RouteHistoryCallback callback)
    {
        routeHistoryAction_ = std::move(callback);
    }

    void RenegadeScreenEditorWorkspace::OnAppearanceRequested(
        std::function<void()> callback)
    {
        appearanceRequested_ = std::move(callback);
    }

    void RenegadeScreenEditorWorkspace::OnImageAssetRequested(
        std::function<void()> callback)
    {
        imageAssetRequested_ = std::move(callback);
    }

    void RenegadeScreenEditorWorkspace::OnCoordinatedSave(
        CoordinatedSaveCallback callback)
    {
        coordinatedSave_ = std::move(callback);
    }

    void RenegadeScreenEditorWorkspace::SetProjectScreenLibrary(
        std::vector<ScreenEditorLibraryScreen> screens)
    {
        projectScreens_ = std::move(screens);
        toolboxScroll_ = 0;
    }

    void RenegadeScreenEditorWorkspace::OnReturnRequested(
        std::function<void()> callback)
    {
        returnRequested_ = std::move(callback);
    }

    bridge::ScreenRect RenegadeScreenEditorWorkspace::PreviewBounds() const noexcept
    {
        // UI-11: fit the authored screen into the available editor canvas
        // without stretching it when panels or Studio's window resize.
        const float left = panels_.left + PanelGap;
        const float top = HeaderHeight + PanelGap;
        const float availableW = std::max(1.0f, width_ - left - panels_.right - PanelGap);
        const float availableH = std::max(1.0f, height_ - top - FooterHeight - PanelGap);
        float aspect = 16.0f / 9.0f;
        if (session_ && session_->IsLoaded())
        {
            const auto& screen = session_->Document();
            if (std::isfinite(screen.designWidth) && std::isfinite(screen.designHeight) &&
                screen.designWidth > 0.0f && screen.designHeight > 0.0f)
                aspect = screen.designWidth / screen.designHeight;
        }
        const float fittedW = std::min(availableW, availableH * aspect);
        const float fittedH = std::min(availableH, availableW / aspect);
        return {left + (availableW - fittedW) * 0.5f,
                top + (availableH - fittedH) * 0.5f, fittedW, fittedH};
    }

    bridge::ScreenRect RenegadeScreenEditorWorkspace::PreviewViewport() const noexcept
    {
        const auto bounds = PreviewBounds();
        return {bounds.x + camera_.panX, bounds.y + camera_.panY,
            bounds.width * camera_.zoom, bounds.height * camera_.zoom};
    }

    bridge::ScreenRect RenegadeScreenEditorWorkspace::MinimapBounds() const noexcept
    {
        const auto bounds = PreviewBounds();
        const float w = std::min(140.0f, bounds.width * 0.25f);
        const float h = w * bounds.height / bounds.width;
        return {bounds.x + bounds.width - w - 8.0f, bounds.y + 8.0f, w, h};
    }

    void RenegadeScreenEditorWorkspace::FitCanvas()
    {
        camera_.Reset();
        cameraPanning_ = false;
        if (panelLayoutChanged_) panelLayoutChanged_();
    }

    void RenegadeScreenEditorWorkspace::HandleCanvasNavigation()
    {
        if (!session_ || !session_->IsLoaded() || basicActionPicker_.IsOpen() ||
            destinationPicker_.IsOpen() || screenSettingsPicker_.IsOpen() || panelDrag_ != PanelDrag::None ||
            !layerDragId_.empty() || directManipulationMode_ != DirectManipulationMode::None)
            return;
        const auto pointer = wi::input::GetPointer();
        const auto bounds = PreviewBounds();
        const bool middle = wi::input::Down(wi::input::MOUSE_BUTTON_MIDDLE);
        if (cameraPanning_)
        {
            if (!middle) { cameraPanning_ = false; return; }
            camera_.PanBy(pointer.x - cameraLastPointer_.x,
                pointer.y - cameraLastPointer_.y);
            cameraLastPointer_ = {pointer.x, pointer.y};
            if (panelLayoutChanged_) panelLayoutChanged_();
            return;
        }
        if (!Contains(bounds, pointer)) return;
        if (wi::input::Press(wi::input::MOUSE_BUTTON_MIDDLE))
        {
            cameraPanning_ = true;
            cameraLastPointer_ = {pointer.x, pointer.y};
            return;
        }
        if (std::isfinite(pointer.z) && std::abs(pointer.z) > 0.001f)
        {
            camera_.ZoomAt(std::pow(1.15f, std::clamp(pointer.z, -8.0f, 8.0f)),
                pointer.x - bounds.x, pointer.y - bounds.y);
            if (panelLayoutChanged_) panelLayoutChanged_();
        }
    }

    void RenegadeScreenEditorWorkspace::CreateControls()
    {
        screenSettingsButton_.Create("Screen Usage and Game Availability");
        screenSettingsButton_.SetTooltip("Declare a menu, passive HUD, overlay or modal. Disabled Screens remain editable.");
        screenSettingsButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            if (!session_ || !session_->IsLoaded()) return;
            basicActionPicker_.Close();
            destinationPicker_.Close();
            const auto documentId = session_->Document().envelope.documentId;
            screenSettingsPicker_.Open("SCREEN SETTINGS",
                {{"Save current grid as project defaults", "grid-default"},
                 {"Use project grid defaults for this Screen", "grid-inherit"},
                 {"Menu (navigation required)", "menu"},
                 {"HUD (passive; empty allowed)", "hud"},
                 {"Overlay (empty allowed)", "overlay"},
                 {"Modal (navigation required)", "modal"},
                 {session_->Document().enabledInGame ? "Disable in Game" : "Enable in Game", "enabled"}},
                bridge::ScreenUsageName(session_->Document().usage),
                [this, documentId](const std::string& value)
                {
                    if (!session_ || !session_->IsLoaded() ||
                        session_->Document().envelope.documentId != documentId) return;
                    if(value=="grid-default" || value=="grid-inherit"){
                        if(!gridSettingsValid_){SetStatus("GRID SETTINGS UNAVAILABLE");return;}
                        auto candidate=projectGrid_;
                        if(value=="grid-default")candidate.defaults={gridSpacing_,gridVisible_,gridSnapEnabled_};
                        candidate.screens.erase(documentId);std::string error;
                        if(!candidate.Save(gridProjectRoot_,session_->ProjectId(),error)){SetStatus("GRID SAVE FAILED // "+error);return;}
                        LoadProjectGrid();return;
                    }
                    auto usage = session_->Document().usage;
                    bool enabled = session_->Document().enabledInGame;
                    if (value == "enabled") enabled = !enabled;
                    else if (value == "menu") usage = bridge::ScreenUsage::Menu;
                    else if (value == "hud") usage = bridge::ScreenUsage::Hud;
                    else if (value == "overlay") usage = bridge::ScreenUsage::Overlay;
                    else if (value == "modal") usage = bridge::ScreenUsage::Modal;
                    else return;
                    std::string error;
                    if (!session_->UpdateScreenSettings(usage, enabled, error))
                        SetStatus("SCREEN SETTINGS REJECTED // " + error);
                    else RefreshAfterMutation("SCREEN SETTINGS UPDATED");
                });
        });
        previewButton_.Create("Preview screen");
        previewButton_.SetText("PREVIEW");
        previewButton_.OnClick([this](const wi::gui::EventArgs&){if(previewRequested_)previewRequested_();});
        cameraBackgroundButton_.Create("Background source");
        cameraBackgroundButton_.SetTooltip("Choose an image or a saved level camera.");
        cameraBackgroundButton_.OnClick([this](const wi::gui::EventArgs&){ChooseBackgroundCamera();});
        videoLoop_.Create("Loop video");
        videoMute_.Create("Mute video");
        const auto toggleVideo=[this](bool loop) {
            const auto* widget=session_?session_->FindWidget(selectedWidgetId_):nullptr;
            if (!widget || !widget->video) return;
            auto video=*widget->video;
            if (loop) video.loop=!video.loop; else video.mute=!video.mute;
            std::string error;
            if (session_->SetVideoPanel(widget->id,video,error)) RefreshAfterMutation("Video playback updated");
            else SetStatus(error);
        };
        videoLoop_.OnClick([toggleVideo](const wi::gui::EventArgs&){toggleVideo(true);});
        videoMute_.OnClick([toggleVideo](const wi::gui::EventArgs&){toggleVideo(false);});
        panBackgroundButton_.Create("Camera slow pan");
        panBackgroundButton_.OnClick([this](const wi::gui::EventArgs&){
            if(!session_)return;
            const auto* widget=session_->FindWidget(selectedWidgetId_);
            if(!widget || !widget->cameraSource)return;
            auto source=*widget->cameraSource;
            source.slowPan=!source.slowPan;
            std::string error;
            if(session_->SetBackgroundCamera(widget->id,source,error)) RefreshAfterMutation("Camera pan updated");
            else SetStatus(error);
        });
        copyBackgroundButton_.Create("Copy background to other screens");
        copyBackgroundButton_.SetText("Copy to other screens...");
        copyBackgroundButton_.OnClick([this](const wi::gui::EventArgs&){ChooseBackgroundDestinations();});
        fitCanvasButton_.Create("Screen Canvas Fit");
        fitCanvasButton_.SetText("FIT");
        fitCanvasButton_.SetTooltip("Fit the design canvas. Wheel zooms at the cursor; middle mouse pans; minimap click/drag recentres; double-click returns to StoryFlow.");
        fitCanvasButton_.OnClick([this](const wi::gui::EventArgs&) { FitCanvas(); });
        returnButton_.Create("Screen Editor Return");
        returnButton_.SetText("< BACK");
        returnButton_.SetTooltip("Autosave the current Screen and StoryFlow before returning; block if saving fails.");
        returnButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ReturnToStoryFlow();
        });

        returnButton_.SetIcon("return");returnButton_.SetTooltip("Back to Story Flow");
        fitCanvasButton_.SetIcon("fit");focusButton_.SetIcon("focus");
        saveButton_.Create("Screen Editor Save");
        saveButton_.SetText("SAVE");
        saveButton_.SetTooltip("Optional Save Now. Changes autosave after 1.5 seconds idle or before navigation.");
        saveButton_.OnClick([this](const wi::gui::EventArgs&) { SaveScreen(); });

        undoButton_.Create("Screen Editor Undo");
        undoButton_.SetText("UNDO");
        undoButton_.OnClick([this](const wi::gui::EventArgs&) { UndoScreen(); });

        redoButton_.Create("Screen Editor Redo");
        redoButton_.SetText("REDO");
        redoButton_.OnClick([this](const wi::gui::EventArgs&) { RedoScreen(); });
        saveButton_.SetIcon("save");undoButton_.SetIcon("undo");redoButton_.SetIcon("redo");
        undoButton_.SetTooltip("Undo");redoButton_.SetTooltip("Redo");

        gridVisibleButton_.Create("Screen Grid Visibility");
        gridVisibleButton_.SetText("GRID ON");
        gridVisibleButton_.SetTooltip("Toggle the editor-only grid. Snapping is independent.");
        gridVisibleButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            gridVisible_ = !gridVisible_;
            if(!SaveProjectGrid())return;
            gridVisibleButton_.SetText(gridVisible_ ? "GRID ON" : "GRID OFF");
            NotifyPreferencesChanged();
        });

        gridSnapButton_.Create("Screen Grid Snapping");
        gridSnapButton_.SetText("SNAP ON");
        gridSnapButton_.SetTooltip("Snap moving and resizing to the grid. Hold Alt to bypass.");
        gridSnapButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            gridSnapEnabled_ = !gridSnapEnabled_;
            if(!SaveProjectGrid())return;
            gridSnapButton_.SetText(gridSnapEnabled_ ? "SNAP ON" : "SNAP OFF");
            NotifyPreferencesChanged();
        });

        gridSpacingInput_.Create("Screen Grid Spacing");
        gridSpacingInput_.SetPlaceholder("16 PX");
        gridSpacingInput_.SetValue("16");
        gridSpacingInput_.SetShadowRadius(0.0f);
        gridSpacingInput_.SetTooltip("Editor grid spacing in design pixels: 4 to 128.");
        gridSpacingInput_.SetCancelInputEnabled(false);
        gridSpacingInput_.OnInputAccepted([this](const wi::gui::EventArgs&)
        {
            SetGridSpacingFromInput();
        });

        toolboxSearchInput_.Create("Screen UI Library Search");
        toolboxSearchInput_.SetValue("");
        toolboxSearchInput_.SetPlaceholder("SEARCH ELEMENTS");
        toolboxSearchInput_.SetCancelInputEnabled(false);
        toolboxSearchInput_.SetShadowRadius(0.0f);

        focusButton_.Create("Screen Editor Canvas Focus");
        focusButton_.SetText("FOCUS");
        focusButton_.SetTooltip("Toggle full-width canvas focus and restore the panels.");
        focusButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ToggleFocusCanvas();
        });

        addTextButton_.Create("Screen Editor Add Text");
        addTextButton_.SetText("+ TEXT");
        addTextButton_.SetTooltip("Create an editable Text element.");
        addTextButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            CreateElement(bridge::ScreenWidgetKind::Text, false);
        });

        addButtonButton_.Create("Screen Editor Add Button");
        addButtonButton_.SetText("+ BUTTON");
        addButtonButton_.SetTooltip("Create an editable Button bound to an existing Screen action.");
        addButtonButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            CreateElement(bridge::ScreenWidgetKind::Button, false);
        });

        addImageButton_.Create("Screen Editor Add Image");
        addImageButton_.SetText("+ IMAGE");
        addImageButton_.SetTooltip("Create an editable Image element. Resource assignment remains explicit.");
        addImageButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            CreateElement(bridge::ScreenWidgetKind::Image, false);
        });

        addBackgroundButton_.Create("Screen Editor Add Background");
        addBackgroundButton_.SetText("+ BACKGROUND");
        addBackgroundButton_.SetTooltip("Create a full-canvas Image at the back of the authored layer stack.");
        addBackgroundButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            CreateElement(bridge::ScreenWidgetKind::Image, true);
        });

        duplicateButton_.Create("Screen Editor Duplicate Element");
        duplicateButton_.SetText("DUP");
        duplicateButton_.SetTooltip("Duplicate the selected element with a new stable ID.");
        duplicateButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            DuplicateSelectedWidget();
        });

        deleteButton_.Create("Screen Editor Delete Element");
        deleteButton_.SetText("DELETE");
        deleteButton_.SetTooltip("Delete the selected element when the complete Screen remains valid.");
        deleteButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            DeleteSelectedWidget();
        });

        backButton_.Create("Screen Editor Send To Back");
        backButton_.SetText("TO BACK");
        backButton_.SetTooltip("Send the selected element behind its siblings.");
        backButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            MoveSelectedWidgetToBack();
        });

        backwardOneButton_.Create("Screen Editor Send Back One Layer");
        backwardOneButton_.SetText("BACK 1");
        backwardOneButton_.SetTooltip("Move the selected element back one sibling layer.");
        backwardOneButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            MoveSelectedWidgetBackward();
        });

        forwardOneButton_.Create("Screen Editor Bring Forward One Layer");
        forwardOneButton_.SetText("FORWARD 1");
        forwardOneButton_.SetTooltip("Move the selected element forward one sibling layer.");
        forwardOneButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            MoveSelectedWidgetForward();
        });

        frontButton_.Create("Screen Editor Bring To Front");
        frontButton_.SetText("TO FRONT");
        frontButton_.SetTooltip("Bring the selected element in front of its siblings.");
        frontButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            MoveSelectedWidgetToFront();
        });

        inventoryInspector_.Create();
        inventoryButton_.Create("Inventory layout Inspector");
        inventoryButton_.OnClick([this](const wi::gui::EventArgs&){
            inventoryElementMode_=false;basicActionPicker_.Close();destinationPicker_.Close();
        });
        const char* labels[] = {"Normal", "Hover", "Pressed"};
        for (unsigned i = 0; i < 3; ++i)
        {
            stateTabs_[i].Create(std::string("Button appearance ") + labels[i]);
            stateTabs_[i].SetText(labels[i]);
            stateTabs_[i].OnClick([this,i](const wi::gui::EventArgs&) { basicState_ = i; });
        }
        stateImage_.Create("Button state artwork");
        stateImage_.OnClick([this](const wi::gui::EventArgs&) { if (editState_) editState_(basicState_, false); });
        clickSound_.Create("Button click sound");
        clickSound_.OnClick([this](const wi::gui::EventArgs&){ ChooseClickSound(); });
        previewSound_.Create("Preview click sound"); previewSound_.SetText("Preview");
        previewSound_.OnClick([this](const wi::gui::EventArgs&){ UpdateClickSound("preview"); });
        defaultSound_.Create("Use click sound as default for all buttons");
        defaultSound_.SetText("Use as default for all buttons");
        defaultSound_.OnClick([this](const wi::gui::EventArgs&){ UpdateClickSound("promote"); });
        stateColour_.Create("Button state colour");
        stateColour_.OnClick([this](const wi::gui::EventArgs&) { if (editState_) editState_(basicState_, true); });
        appearanceButton_.Create("Screen Editor Appearance Inspector");
        appearanceButton_.SetText("STYLE / FONT");
        appearanceButton_.SetTooltip(
            "Edit colours, visual states, typography and resource bindings.");
        appearanceButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            if (appearanceRequested_) appearanceRequested_();
        });
        imageAssetButton_.Create("Screen Image Asset Browser");
        imageAssetButton_.SetText("CHANGE GRAPHIC");
        imageAssetButton_.SetAccentOutlineStyle(true);
        imageAssetButton_.SetRenderTextSize(12);
        imageAssetButton_.SetTooltip(
            "Choose an imported project image or browse this computer.");
        imageAssetButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            const auto* selected=session_ ? session_->FindWidget(selectedWidgetId_) : nullptr;
            if(selected&&selected->navigation){OpenNavigationEditor();return;}
            if(FindSelectedLiquid()){OpenLiquidEditor();return;}
            if (selected && selected->carousel)
            {
                const auto id=selected->id,document=session_->Document().envelope.documentId,root=gridProjectRoot_;
                carouselEditor_->Open(*selected->carousel,root,session_->ProjectId(),
                    [this,id,document,root](bridge::ScreenCarousel deck,std::string& error){
                        if(!session_||!session_->IsLoaded()||gridProjectRoot_!=root||
                            session_->Document().envelope.documentId!=document)
                        {error="The original screen is no longer open.";return false;}
                        if(!session_->SetCarousel(id,std::move(deck),error))return false;
                        RefreshAfterMutation("Carousel saved");return true;
                    },[this]{carouselEditorOpen_=false;});
                carouselEditorOpen_=true;
            }
            else if (selected && selected->kind==bridge::ScreenWidgetKind::Text) ChooseHintList();
            else if (selected && (selected->video || bridge::IsScreenBackground(session_->Document(),*selected))) ChooseScreenMedia();
            else if (graphicRequested_) graphicRequested_(false);
        });
        progressFrameButton_.Create("Progress Frame Image");
        progressFrameButton_.SetAccentOutlineStyle(true);
        progressFrameButton_.SetText("FRAME IMAGE // CHOOSE");
        progressFrameButton_.OnClick([this](const wi::gui::EventArgs&)
        { if (progressArtworkRequested_) progressArtworkRequested_(true); });
        progressFillButton_.Create("Progress Fill Image");
        progressFillButton_.SetAccentOutlineStyle(true);
        progressFillButton_.SetText("FILL IMAGE // CHOOSE");
        progressFillButton_.OnClick([this](const wi::gui::EventArgs&)
        { if (progressArtworkRequested_) progressArtworkRequested_(false); });

        destinationButton_.Create("Basic Screen Destination");
        destinationButton_.SetText("ON CLICK: OPEN SCREEN...");
        destinationButton_.SetTooltip("Choose an existing StoryFlow Screen destination for this Button.");
        destinationButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            OpenDestinationPicker();
        });

        basicActionButton_.Create("Basic Screen Button Action");
        basicActionButton_.SetText("ON CLICK: CHOOSE ACTION");
        basicActionButton_.SetTooltip("Select a governed symbolic button action; destinations remain owned by StoryFlow.");
        basicActionButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            if(session_&&session_->IsLoaded()&&std::any_of(session_->Document().widgets.begin(),session_->Document().widgets.end(),
                [](const auto& w){return !w.dialogueRole.empty();})){
                const auto* selected=session_->FindWidget(selectedWidgetId_);if(!selected)return;
                const auto id=selectedWidgetId_;
                std::vector<RenegadeScreenChoicePicker::Choice> choices={{"Decoration / no dialogue role",""}};
                if(selected->kind==bridge::ScreenWidgetKind::Text){choices.push_back({"Speaker name","speaker"});choices.push_back({"Dialogue text","text"});}
                else for(const auto* role:{"reply1","reply2","reply3","reply4","continue","previous","next","close"})choices.push_back({role,role});
                basicActionPicker_.Open("DIALOGUE ELEMENT ROLE",std::move(choices),selected->dialogueRole,[this,id](const std::string& role){
                    std::string error;if(!session_->UpdateDialogueRole(id,role,error)){SetStatus(error);return;}
                    RefreshAfterMutation("DIALOGUE ROLE UPDATED // STYLE AND LAYOUT PRESERVED");
                });return;
            }
            OpenBasicActionPicker();
        });

        const auto configureInput = [](RenegadeTextInputField& input,
                                       const char* name,
                                       const char* placeholder)
        {
            input.Create(name);
            input.SetPlaceholder(placeholder);
            input.SetCancelInputEnabled(false);
            input.SetShadowRadius(0.0f);
        };
        nameInput_.SetLiteral(true);textInput_.SetLiteral(true);
        configureInput(nameInput_, "Screen Element Name", "Element name");
        configureInput(textInput_, "Screen Element Text", "Displayed text");
        configureInput(fillInput_, "Screen Progress Fill", "Fill 0 to 1");
        fillInput_.OnInputAccepted([this](const wi::gui::EventArgs&)
        {
            if (!inspectorSuppressed_) ApplySelectedWidget();
        });
        fillDirectionButton_.Create("Screen Progress Fill Direction");
        fillDirectionButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            const auto* widget = session_ ? session_->FindWidget(selectedWidgetId_) : nullptr;
            if (!widget || widget->kind != bridge::ScreenWidgetKind::Progress) return;
            const auto direction = static_cast<bridge::ScreenFillDirection>(
                (static_cast<int>(widget->fillDirection) + 1) % 4);
            std::string error;
            if (!session_->UpdateWidgetFill(widget->id, widget->fill, direction, error))
            { SetStatus("FILL DIRECTION REJECTED // " + error); return; }
            RefreshAfterMutation("FILL DIRECTION UPDATED // UNSAVED SCREEN CHANGE");
        });
        configureInput(positionXInput_, "Screen Element X", "X px");
        configureInput(positionYInput_, "Screen Element Y", "Y px");
        configureInput(sizeWidthInput_, "Screen Element Width", "Width px");
        configureInput(sizeHeightInput_, "Screen Element Height", "Height px");
        for (RenegadeTextInputField* input : std::initializer_list<RenegadeTextInputField*>{&nameInput_, &positionXInput_, &positionYInput_,
                            &sizeWidthInput_, &sizeHeightInput_})
        {
            input->OnInputAccepted([this](const wi::gui::EventArgs&)
            {
                if (!inspectorSuppressed_) ApplySelectedWidget();
            });
        }

        applyButton_.Create("Screen Editor Apply Element");
        applyButton_.SetText("APPLY ELEMENT");
        applyButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ApplySelectedWidget();
        });
        textInput_.OnInputAccepted([this](const wi::gui::EventArgs&)
        {
            ApplySelectedWidget();
        });

        visibleButton_.Create("Screen Editor Visible Toggle");
        visibleButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ToggleVisible();
        });
        enabledButton_.Create("Screen Editor Enabled Toggle");
        enabledButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ToggleEnabled();
        });

        for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&returnButton_),
                static_cast<wi::gui::Widget*>(&saveButton_),
                static_cast<wi::gui::Widget*>(&undoButton_),
                static_cast<wi::gui::Widget*>(&redoButton_),
                static_cast<wi::gui::Widget*>(&focusButton_),
                static_cast<wi::gui::Widget*>(&screenSettingsButton_),
                static_cast<wi::gui::Widget*>(&fitCanvasButton_),
                static_cast<wi::gui::Widget*>(&previewButton_),
                static_cast<wi::gui::Widget*>(&copyBackgroundButton_),
                static_cast<wi::gui::Widget*>(&cameraBackgroundButton_),
                static_cast<wi::gui::Widget*>(&panBackgroundButton_),
                static_cast<wi::gui::Widget*>(&videoLoop_),
                static_cast<wi::gui::Widget*>(&videoMute_),
                static_cast<wi::gui::Widget*>(&gridVisibleButton_),
                static_cast<wi::gui::Widget*>(&gridSnapButton_),
                static_cast<wi::gui::Widget*>(&gridSpacingInput_),
                static_cast<wi::gui::Widget*>(&toolboxSearchInput_),
                static_cast<wi::gui::Widget*>(&stateTabs_[0]),
                static_cast<wi::gui::Widget*>(&stateTabs_[1]),
                static_cast<wi::gui::Widget*>(&stateTabs_[2]),
                static_cast<wi::gui::Widget*>(&stateImage_),
                static_cast<wi::gui::Widget*>(&stateColour_),
                static_cast<wi::gui::Widget*>(&clickSound_),
                static_cast<wi::gui::Widget*>(&previewSound_),
                static_cast<wi::gui::Widget*>(&defaultSound_),
                static_cast<wi::gui::Widget*>(&appearanceButton_),
                static_cast<wi::gui::Widget*>(&imageAssetButton_),
                static_cast<wi::gui::Widget*>(&progressFrameButton_),
                static_cast<wi::gui::Widget*>(&progressFillButton_),
                static_cast<wi::gui::Widget*>(&basicActionButton_),
                static_cast<wi::gui::Widget*>(&destinationButton_),
                static_cast<wi::gui::Widget*>(&addTextButton_),
                static_cast<wi::gui::Widget*>(&addButtonButton_),
                static_cast<wi::gui::Widget*>(&addImageButton_),
                static_cast<wi::gui::Widget*>(&addBackgroundButton_),
                static_cast<wi::gui::Widget*>(&duplicateButton_),
                static_cast<wi::gui::Widget*>(&deleteButton_),
                static_cast<wi::gui::Widget*>(&backButton_),
                static_cast<wi::gui::Widget*>(&backwardOneButton_),
                static_cast<wi::gui::Widget*>(&forwardOneButton_),
                static_cast<wi::gui::Widget*>(&frontButton_),
                static_cast<wi::gui::Widget*>(&nameInput_),
                static_cast<wi::gui::Widget*>(&textInput_),
                static_cast<wi::gui::Widget*>(&fillInput_),
                static_cast<wi::gui::Widget*>(&fillDirectionButton_),
                static_cast<wi::gui::Widget*>(&positionXInput_),
                static_cast<wi::gui::Widget*>(&positionYInput_),
                static_cast<wi::gui::Widget*>(&sizeWidthInput_),
                static_cast<wi::gui::Widget*>(&sizeHeightInput_),
                static_cast<wi::gui::Widget*>(&applyButton_),
                static_cast<wi::gui::Widget*>(&visibleButton_),
                static_cast<wi::gui::Widget*>(&enabledButton_)})
        {
            widget->SetShadowRadius(0.0f);
            widget->SetVisible(false);
        }
    }

    void RenegadeScreenEditorWorkspace::LayoutControls()
    {
        float x = 14.0f;
        const auto header = [&x](wi::gui::Widget& widget, const float width)
        {
            widget.SetPos(XMFLOAT2(x, 14.0f));
            widget.SetSize(XMFLOAT2(width, 32.0f));
            x += width + 7.0f;
        };
        header(returnButton_, 32.0f);
        header(undoButton_, 32.0f);
        header(redoButton_, 32.0f);
        header(saveButton_, 32.0f);

        // Header actions stay fixed when either side panel is dragged.
        // Align grid controls after Save instead of letting a wide left panel
        // push them under the right Inspector / Advanced toggle.
        const float gridToolbarX = x + 6.0f;
        gridVisibleButton_.SetPos(XMFLOAT2(gridToolbarX, 14.0f));
        gridVisibleButton_.SetSize(XMFLOAT2(84.0f, 32.0f));
        gridSnapButton_.SetPos(XMFLOAT2(gridToolbarX + 90.0f, 14.0f));
        gridSnapButton_.SetSize(XMFLOAT2(88.0f, 32.0f));
        gridSpacingInput_.SetPos(XMFLOAT2(gridToolbarX + 184.0f, 14.0f));
        gridSpacingInput_.SetSize(XMFLOAT2(64.0f, 32.0f));
        focusButton_.SetPos(XMFLOAT2(gridToolbarX + 260.0f, 14.0f));
        focusButton_.SetSize(XMFLOAT2(94.0f, 32.0f));
        previewButton_.SetPos(XMFLOAT2(gridToolbarX+544,14));
        previewButton_.SetSize(XMFLOAT2(78,32));
        fitCanvasButton_.SetPos(XMFLOAT2(gridToolbarX + 360.0f, 14.0f));
        fitCanvasButton_.SetSize(XMFLOAT2(50.0f, 32.0f));
        screenSettingsButton_.SetPos(XMFLOAT2(gridToolbarX + 416.0f, 14.0f));
        screenSettingsButton_.SetSize(XMFLOAT2(120.0f, 32.0f));
        screenSettingsPicker_.SetLayout(std::min(gridToolbarX + 416.0f, width_ - 290.0f),
            HeaderHeight, 280.0f, 214.0f);

        toolboxSearchInput_.SetPos(XMFLOAT2(12.0f, HeaderHeight + 76.0f));
        toolboxSearchInput_.SetSize(XMFLOAT2(
            std::max(150.0f, panels_.left - 24.0f), 28.0f));

        float creatorX = 10.0f;
        const float creatorTop = HeaderHeight + 42.0f;
        const auto creator = [&creatorX, creatorTop](
            wi::gui::Widget& widget,
            const float width,
            const float row)
        {
            widget.SetPos(XMFLOAT2(creatorX, creatorTop + row * 34.0f));
            widget.SetSize(XMFLOAT2(width, 28.0f));
            creatorX += width + 4.0f;
        };
        const float columnWidth = std::max(
            70.0f, (panels_.left - 24.0f) * 0.5f);
        creator(addTextButton_, columnWidth, 0.0f);
        creator(addButtonButton_, columnWidth, 0.0f);
        creatorX = 10.0f;
        creator(addImageButton_, columnWidth, 1.0f);
        creator(addBackgroundButton_, columnWidth, 1.0f);
        creatorX = 10.0f;
        creator(duplicateButton_, columnWidth, 2.0f);
        creator(deleteButton_, columnWidth, 2.0f);

        // Layer controls are always below the canvas, never buried among
        // creation controls. The footer keeps the same proportions at 720p.
        const auto preview = PreviewBounds();
        float layerX = preview.x + 8.0f;
        const float layerY = height_ - FooterHeight + 7.0f;
        const auto layer = [&layerX, layerY](wi::gui::Widget& widget, float w)
        {
            widget.SetPos(XMFLOAT2(layerX, layerY));
            widget.SetSize(XMFLOAT2(w, 30.0f));
            layerX += w + 6.0f;
        };
        layer(backButton_, 78.0f);
        layer(backwardOneButton_, 78.0f);
        layer(forwardOneButton_, 96.0f);
        layer(frontButton_, 84.0f);

        const float inspectorX = width_ - panels_.right + 14.0f;
        const float fieldWidth = panels_.right - 28.0f;
        const auto full = [&](wi::gui::Widget& widget, float y, float h = 28.0f)
        {
            widget.SetPos(XMFLOAT2(inspectorX, y));
            widget.SetSize(XMFLOAT2(fieldWidth, h));
        };
        const auto pair = [&](wi::gui::Widget& a, wi::gui::Widget& b, float y)
        {
            a.SetPos(XMFLOAT2(inspectorX, y));
            a.SetSize(XMFLOAT2((fieldWidth - 6.0f) * 0.5f, 28.0f));
            b.SetPos(XMFLOAT2(inspectorX + (fieldWidth + 6.0f) * 0.5f, y));
            b.SetSize(XMFLOAT2((fieldWidth - 6.0f) * 0.5f, 28.0f));
        };
        full(nameInput_, HeaderHeight + (session_ && session_->FindWidget(selectedWidgetId_) && session_->FindWidget(selectedWidgetId_)->kind == bridge::ScreenWidgetKind::Image ? 141.0f : 77.0f));
        full(textInput_, HeaderHeight + 137.0f);
        full(fillInput_, HeaderHeight + 137.0f);
        full(fillDirectionButton_, HeaderHeight + 423.0f, 32.0f);
        pair(positionXInput_, positionYInput_, HeaderHeight + 197.0f);
        pair(sizeWidthInput_, sizeHeightInput_, HeaderHeight + 249.0f);
        pair(visibleButton_, enabledButton_, HeaderHeight + 305.0f);
        full(applyButton_, HeaderHeight + 348.0f, 32.0f);
        full(basicActionButton_, HeaderHeight + 423.0f, 32.0f);
        const auto* graphicTarget=session_?session_->FindWidget(selectedWidgetId_):nullptr;
        full(cameraBackgroundButton_,HeaderHeight+423,32);
        full(panBackgroundButton_,HeaderHeight+461,32);
        pair(videoLoop_,videoMute_,HeaderHeight+461);
        full(copyBackgroundButton_,HeaderHeight+385,32);
        full(imageAssetButton_,HeaderHeight+
            (graphicTarget&&graphicTarget->kind==bridge::ScreenWidgetKind::Image?69.0f:385.0f),
            graphicTarget&&graphicTarget->kind==bridge::ScreenWidgetKind::Image?43.0f:30.0f);
        full(progressFrameButton_, HeaderHeight + 467.0f, 37.0f);
        full(progressFillButton_, HeaderHeight + 514.0f, 37.0f);
        appearanceButton_.SetPos(XMFLOAT2(
            inspectorX + std::max(0.0f, fieldWidth - 112.0f),
            HeaderHeight + 37.0f));
        appearanceButton_.SetSize(XMFLOAT2(112.0f, 25.0f));
        const float tabWidth = (fieldWidth - 8.0f)/3;
        for (unsigned i = 0; i < 3; ++i)
        {
            stateTabs_[i].SetPos({inspectorX+i*(tabWidth+4), HeaderHeight+383});
            stateTabs_[i].SetSize({tabWidth, 27});
        }
        pair(stateImage_,stateColour_,HeaderHeight+414);
        if (graphicTarget && graphicTarget->kind == bridge::ScreenWidgetKind::Button)
            full(basicActionButton_,HeaderHeight+449,30);
        full(destinationButton_, HeaderHeight + 493.0f, 32.0f);
        full(deleteButton_, HeaderHeight + 560.0f, 31.0f);
        full(duplicateButton_, HeaderHeight + 602.0f, 31.0f);
        if (graphicTarget && graphicTarget->kind==bridge::ScreenWidgetKind::Button)
        {
            const float previewWidth=65;
            full(clickSound_,HeaderHeight+530,28);
            clickSound_.SetSize({fieldWidth-previewWidth-5,28});
            previewSound_.SetPos({inspectorX+fieldWidth-previewWidth,HeaderHeight+530});
            previewSound_.SetSize({previewWidth,28});
            full(defaultSound_,HeaderHeight+562,28);
            pair(deleteButton_,duplicateButton_,HeaderHeight+602);
        }
        const float pickerHeight = std::min(240.0f,
            std::max(160.0f, height_ - HeaderHeight - FooterHeight - 300.0f));
        const float pickerLastY = std::max(HeaderHeight,
            height_ - FooterHeight - pickerHeight);
        destinationPicker_.SetLayout(inspectorX,
            std::min(HeaderHeight + 531.0f, pickerLastY), fieldWidth, pickerHeight);
        basicActionPicker_.SetLayout(inspectorX,
            std::min(HeaderHeight + 462.0f, pickerLastY), fieldWidth, pickerHeight);
    }

    bool RenegadeScreenEditorWorkspace::InventoryAvailable() const {
        if(!session_||!session_->IsLoaded())return false;
        return std::any_of(session_->Document().widgets.begin(),session_->Document().widgets.end(),
            [](const auto& w){std::size_t p;std::uint32_t i;
                return bridge::ParseInventoryPagedGridSlotAction(w.actionId,p,i);});
    }
    bool RenegadeScreenEditorWorkspace::InventoryInspectorActive() const {
        return InventoryAvailable()&&!inventoryElementMode_&&!inspectorSuppressed_&&!focusCanvas_&&!rightCollapsed_;
    }
    void RenegadeScreenEditorWorkspace::UpdateControls(
        const wi::Canvas& canvas,
        const float dt)
    {
        LayoutControls();
        const bool loaded = session_ && session_->IsLoaded();
        const auto* selected = loaded
            ? session_->FindWidget(selectedWidgetId_) : nullptr;

        returnButton_.SetVisible(loaded);
        saveButton_.SetVisible(loaded);
        undoButton_.SetVisible(loaded);
        redoButton_.SetVisible(loaded);
        focusButton_.SetVisible(loaded);
        screenSettingsButton_.SetVisible(loaded);
        screenSettingsButton_.SetEnabled(loaded);
        if (loaded)
            screenSettingsButton_.SetText(std::string(bridge::ScreenUsageName(
                session_->Document().usage)) +
                (session_->Document().enabledInGame ? " // ON" : " // OFF"));
        previewButton_.SetVisible(loaded);
        fitCanvasButton_.SetVisible(loaded);
        fitCanvasButton_.SetEnabled(loaded);
        gridVisibleButton_.SetVisible(loaded);
        gridSnapButton_.SetVisible(loaded);
        gridSpacingInput_.SetVisible(loaded);
        toolboxSearchInput_.SetVisible(loaded && !focusCanvas_ && !leftCollapsed_);
        addTextButton_.SetVisible(false);
        addButtonButton_.SetVisible(false);
        addImageButton_.SetVisible(false);
        addBackgroundButton_.SetVisible(false);
        const bool flowDirty = loaded && externalFlowDirty_ &&
            externalFlowDirty_();
        saveButton_.SetEnabled(loaded && (session_->IsDirty() || flowDirty));
        if (loaded)
            timeline_.ObserveScreen(session_->UndoCount(), session_->RedoCount(), session_->HistoryMutation());
        undoButton_.SetEnabled(loaded &&
            timeline_.NextUndo() != ScreenEditorUndoTimeline::Kind::None);
        redoButton_.SetEnabled(loaded &&
            timeline_.NextRedo() != ScreenEditorUndoTimeline::Kind::None);

        const bool selectedReady = selected != nullptr;
        const bool dialogue = loaded && std::any_of(session_->Document().widgets.begin(),session_->Document().widgets.end(),
            [](const auto& w){return !w.dialogueRole.empty();});
        const bool actionReady = selectedReady &&
            (selected->kind == bridge::ScreenWidgetKind::Button || (dialogue && selected->kind == bridge::ScreenWidgetKind::Text)) &&
            !inspectorSuppressed_;
        basicActionButton_.SetVisible(actionReady && !focusCanvas_ && !rightCollapsed_);
        const bool button = selectedReady && selected->kind == bridge::ScreenWidgetKind::Button;
        const bool showStates = button && !inspectorSuppressed_ && !focusCanvas_ && !rightCollapsed_ && !InventoryInspectorActive();
        if (stateOwner_ != selectedWidgetId_) { stateOwner_ = selectedWidgetId_; basicState_ = 0; }
        for (unsigned i=0; i<3; ++i)
        {
            stateTabs_[i].SetVisible(showStates);
            stateTabs_[i].SetAccentOutlineStyle(i==basicState_);
        }
        stateImage_.SetVisible(showStates);
        stateColour_.SetVisible(showStates);
        clickSound_.SetVisible(showStates);
        previewSound_.SetVisible(showStates);
        defaultSound_.SetVisible(showStates);
        if(showStates && !gridProjectRoot_.empty())
        {
            const auto& sounds=bridge::ProjectClickSounds(gridProjectRoot_);
            const bridge::ScreenClickSounds::Key key={session_->Document().envelope.documentId,selectedWidgetId_};
            const auto clip=sounds.Resolve(key);
            const auto it=sounds.buttons.find(key);
            const auto name=std::filesystem::u8path(clip).filename().u8string();
            const auto label=it==sounds.buttons.end() ? (clip.empty() ? std::string("Add sound...") : "Default: "+name) :
                (clip.empty() ? std::string("Silent") : name);
            clickSound_.SetText("Click sound: "+label);
            clickSound_.SetTooltip("Choose a project clip, import audio, follow the project default, or silence this button.");
            previewSound_.SetEnabled(!clip.empty());
            defaultSound_.SetEnabled(it!=sounds.buttons.end() && !clip.empty());
        }
        if (showStates)
        {
            const auto& v = basicState_ == 0 ? selected->style.normal : basicState_ == 1 ? selected->style.hover : selected->style.pressed;
            stateImage_.SetText(v.imageResourcePath.empty() ? "Choose image..." : "Change image...");
            stateColour_.SetText("Colour");
        }
        const bool background = selectedReady && bridge::IsScreenBackground(session_->Document(),*selected);
        copyBackgroundButton_.SetVisible(background && !focusCanvas_ && !rightCollapsed_ && !inspectorSuppressed_);
        cameraBackgroundButton_.SetVisible(copyBackgroundButton_.IsVisible());
        panBackgroundButton_.SetVisible(copyBackgroundButton_.IsVisible() && selected->cameraSource.has_value());
        const bool showVideo=selectedReady && selected->video && !focusCanvas_ && !rightCollapsed_ && !inspectorSuppressed_;
        videoLoop_.SetVisible(showVideo);
        videoMute_.SetVisible(showVideo);
        if (showVideo)
        {
            videoLoop_.SetText(selected->video->loop ? "Loop: on" : "Loop: off");
            videoMute_.SetText(selected->video->mute ? "Mute: on" : "Mute: off");
        }
        if(background)
        {
            const auto& source=selected->cameraSource;
            cameraBackgroundButton_.SetText(source ? "Live camera: "+source->cameraName : selected->video ? "Source: video / choose camera..." : "Source: image / choose camera...");
            if(source)panBackgroundButton_.SetText(source->slowPan ? "Slow pan: on" : "Slow pan: off");
        }
        imageAssetButton_.SetText(selectedReady && selected->kind==bridge::ScreenWidgetKind::Text ? "Hint list..." : selectedReady && selected->carousel ? "Edit carousel..." : background ? "Add file..." : selectedReady && selected->video ? "Choose video..." : "CHANGE GRAPHIC");
        if(FindSelectedLiquid()){imageAssetButton_.SetText("Fill settings...");imageAssetButton_.SetTooltip("Edit this Progress layer: mask, fill direction, colour, animation, sounds and gameplay binding.");}
        else imageAssetButton_.SetTooltip("Choose content or settings for this selected layer.");
        if(selectedReady&&selected->navigation){imageAssetButton_.SetText("Navigation settings...");imageAssetButton_.SetTooltip("Edit heading, visible span, world targets and appearance. Scripts can control the same component.");}
        imageAssetButton_.SetVisible(!button && selectedReady && !selected->carouselCaption && !focusCanvas_ && !rightCollapsed_ &&
            !inspectorSuppressed_);
        const bool progressArtworkReady = selectedReady &&
            selected->kind == bridge::ScreenWidgetKind::Progress && !selected->liquid &&
            !focusCanvas_ && !rightCollapsed_ && !inspectorSuppressed_;
        progressFrameButton_.SetVisible(progressArtworkReady);
        progressFillButton_.SetVisible(progressArtworkReady);
        if (progressArtworkReady)
        {
            progressFrameButton_.SetText(selected->progressFrameResourcePath.empty() ?
                "FRAME IMAGE // CHOOSE" : "FRAME IMAGE // CHANGE");
            progressFillButton_.SetText(selected->progressFillResourcePath.empty() ?
                "FILL IMAGE // CHOOSE" : "FILL IMAGE // CHANGE");
        }
        appearanceButton_.SetVisible(
            selectedReady && !focusCanvas_ && !rightCollapsed_ && !inspectorSuppressed_);
        destinationButton_.SetVisible(!dialogue && actionReady && !focusCanvas_ && !rightCollapsed_ &&
            !projectScreens_.empty() &&
            static_cast<bool>(openScreenRoute_));
        const bool hasContext = loaded && std::any_of(projectScreens_.begin(),projectScreens_.end(),[&](const auto& e){return e.available&&!e.nodeId.empty()&&e.documentId==session_->Document().envelope.documentId;});
        destinationButton_.SetEnabled(actionReady&&hasContext);
        const auto destination = hasContext && actionReady && destinationName_ ? destinationName_(selected->actionId) : std::string{};
        destinationButton_.SetText(!hasContext ? "No flow context" : destination.empty() ? "Choose destination..." : destination);
        destinationButton_.SetTooltip(hasContext?"Choose the destination in this Flow context.":"Standalone Screen: routes are disabled. Open or add an explicit Screen node in Journey first.");
        if (actionReady)
            basicActionButton_.SetText(dialogue ? "ROLE: " + (selected->dialogueRole.empty()?std::string("DECORATION"):selected->dialogueRole) : "On click: " + selected->actionId);
        const bool inspectorAvailable = selectedReady && !focusCanvas_ && !rightCollapsed_ &&
            !inspectorSuppressed_;
        deleteButton_.SetVisible(inspectorAvailable &&
            height_ >= HeaderHeight + 560.0f + 34.0f + FooterHeight);
        duplicateButton_.SetVisible(inspectorAvailable &&
            height_ >= HeaderHeight + 602.0f + 34.0f + FooterHeight);
        backButton_.SetVisible(selectedReady);
        backwardOneButton_.SetVisible(selectedReady);
        forwardOneButton_.SetVisible(selectedReady);
        frontButton_.SetVisible(selectedReady);

        // The layer actions are scoped to siblings, not the whole document.
        bool canBack = false;
        bool canFront = false;
        if (selectedReady)
        {
            bool seenSelected = false;
            for (const auto& widget : session_->Document().widgets)
            {
                if (widget.parentId != selected->parentId) continue;
                if (widget.id == selectedWidgetId_)
                {
                    seenSelected = true;
                    continue;
                }
                if (seenSelected) canFront = true;
                else canBack = true;
            }
        }
        backButton_.SetEnabled(canBack);
        backwardOneButton_.SetEnabled(canBack);
        forwardOneButton_.SetEnabled(canFront);
        frontButton_.SetEnabled(canFront);
        for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&nameInput_),
                static_cast<wi::gui::Widget*>(&textInput_),
                static_cast<wi::gui::Widget*>(&fillInput_),
                static_cast<wi::gui::Widget*>(&fillDirectionButton_),
                static_cast<wi::gui::Widget*>(&positionXInput_),
                static_cast<wi::gui::Widget*>(&positionYInput_),
                static_cast<wi::gui::Widget*>(&sizeWidthInput_),
                static_cast<wi::gui::Widget*>(&sizeHeightInput_),
                static_cast<wi::gui::Widget*>(&applyButton_),
                static_cast<wi::gui::Widget*>(&visibleButton_),
                static_cast<wi::gui::Widget*>(&enabledButton_)})
        {
            widget->SetVisible(selectedReady && !focusCanvas_ && !rightCollapsed_ &&
                !inspectorSuppressed_);
        }
        for(auto* input:{&positionXInput_,&positionYInput_,&sizeWidthInput_,&sizeHeightInput_})
            input->SetEnabled(selectedReady&&!inspectorSuppressed_);
        const bool progressReady = selectedReady && selected->kind == bridge::ScreenWidgetKind::Progress;
        textInput_.SetVisible(selectedReady && !progressReady && (selected->kind == bridge::ScreenWidgetKind::Text || bridge::IsScreenInteractive(selected->kind)) && !focusCanvas_ && !rightCollapsed_ && !inspectorSuppressed_);
        textInput_.SetEnabled(selectedReady &&
            (selected->kind == bridge::ScreenWidgetKind::Text || bridge::IsScreenInteractive(selected->kind)));
        fillInput_.SetVisible(progressReady && !focusCanvas_ && !rightCollapsed_ && !inspectorSuppressed_);
        fillDirectionButton_.SetVisible(progressReady && !focusCanvas_ && !rightCollapsed_ && !inspectorSuppressed_);
        enabledButton_.SetEnabled(selectedReady && bridge::IsScreenInteractive(selected->kind));

        for (wi::gui::Widget* widget : {
                static_cast<wi::gui::Widget*>(&returnButton_),
                static_cast<wi::gui::Widget*>(&saveButton_),
                static_cast<wi::gui::Widget*>(&undoButton_),
                static_cast<wi::gui::Widget*>(&redoButton_),
                static_cast<wi::gui::Widget*>(&focusButton_),
                static_cast<wi::gui::Widget*>(&screenSettingsButton_),
                static_cast<wi::gui::Widget*>(&fitCanvasButton_),
                static_cast<wi::gui::Widget*>(&previewButton_),
                static_cast<wi::gui::Widget*>(&copyBackgroundButton_),
                static_cast<wi::gui::Widget*>(&cameraBackgroundButton_),
                static_cast<wi::gui::Widget*>(&panBackgroundButton_),
                static_cast<wi::gui::Widget*>(&videoLoop_),
                static_cast<wi::gui::Widget*>(&videoMute_),
                static_cast<wi::gui::Widget*>(&gridVisibleButton_),
                static_cast<wi::gui::Widget*>(&gridSnapButton_),
                static_cast<wi::gui::Widget*>(&gridSpacingInput_),
                static_cast<wi::gui::Widget*>(&toolboxSearchInput_),
                static_cast<wi::gui::Widget*>(&stateTabs_[0]),
                static_cast<wi::gui::Widget*>(&stateTabs_[1]),
                static_cast<wi::gui::Widget*>(&stateTabs_[2]),
                static_cast<wi::gui::Widget*>(&stateImage_),
                static_cast<wi::gui::Widget*>(&stateColour_),
                static_cast<wi::gui::Widget*>(&clickSound_),
                static_cast<wi::gui::Widget*>(&previewSound_),
                static_cast<wi::gui::Widget*>(&defaultSound_),
                static_cast<wi::gui::Widget*>(&appearanceButton_),
                static_cast<wi::gui::Widget*>(&imageAssetButton_),
                static_cast<wi::gui::Widget*>(&progressFrameButton_),
                static_cast<wi::gui::Widget*>(&progressFillButton_),
                static_cast<wi::gui::Widget*>(&basicActionButton_),
                static_cast<wi::gui::Widget*>(&destinationButton_),
                static_cast<wi::gui::Widget*>(&addTextButton_),
                static_cast<wi::gui::Widget*>(&addButtonButton_),
                static_cast<wi::gui::Widget*>(&addImageButton_),
                static_cast<wi::gui::Widget*>(&addBackgroundButton_),
                static_cast<wi::gui::Widget*>(&duplicateButton_),
                static_cast<wi::gui::Widget*>(&deleteButton_),
                static_cast<wi::gui::Widget*>(&backButton_),
                static_cast<wi::gui::Widget*>(&backwardOneButton_),
                static_cast<wi::gui::Widget*>(&forwardOneButton_),
                static_cast<wi::gui::Widget*>(&frontButton_),
                static_cast<wi::gui::Widget*>(&nameInput_),
                static_cast<wi::gui::Widget*>(&textInput_),
                static_cast<wi::gui::Widget*>(&fillInput_),
                static_cast<wi::gui::Widget*>(&fillDirectionButton_),
                static_cast<wi::gui::Widget*>(&positionXInput_),
                static_cast<wi::gui::Widget*>(&positionYInput_),
                static_cast<wi::gui::Widget*>(&sizeWidthInput_),
                static_cast<wi::gui::Widget*>(&sizeHeightInput_),
                static_cast<wi::gui::Widget*>(&applyButton_),
                static_cast<wi::gui::Widget*>(&visibleButton_),
                static_cast<wi::gui::Widget*>(&enabledButton_)})
        {
            if(InventoryInspectorActive() &&
                (widget==&appearanceButton_ || widget==&imageAssetButton_ ||
                 widget==&progressFrameButton_ || widget==&progressFillButton_ ||
                 widget==&basicActionButton_ || widget==&destinationButton_ ||
                 widget==&duplicateButton_ || widget==&deleteButton_ ||
                 widget==&nameInput_ || widget==&textInput_ || widget==&fillInput_ ||
                 widget==&fillDirectionButton_ || widget==&positionXInput_ || widget==&positionYInput_ ||
                 widget==&sizeWidthInput_ || widget==&sizeHeightInput_ || widget==&applyButton_ ||
                 widget==&visibleButton_ || widget==&enabledButton_))
                widget->SetVisible(false);
            if (widget->IsVisible()) widget->Update(canvas, dt);
        }
        const bool inventoryVisible=InventoryAvailable()&&!inspectorSuppressed_&&!focusCanvas_&&!rightCollapsed_;
        inventoryButton_.SetVisible(inventoryVisible);
        inventoryButton_.SetPos(XMFLOAT2(width_-panels_.right+14,HeaderHeight+37));
        inventoryButton_.SetSize(XMFLOAT2(panels_.right-28-(inventoryElementMode_?118:0),25));
        inventoryButton_.SetText(inventoryElementMode_?"INVENTORY LAYOUT":"INVENTORY");
        if(inventoryVisible)inventoryButton_.Update(canvas,dt);
        if(InventoryInspectorActive()) {
            inventoryInspector_.Bind(session_,selectedWidgetId_,[this]{
                RefreshAfterMutation("INVENTORY UPDATED");
            },[this](std::string name){
                if(!name.empty()) {
                    for(const auto& w:session_->Document().widgets)if(w.name==name){SelectWidget(w.id);break;}
                }
                inventoryElementMode_=true;
            },[this](bool allSlots){
                if(allSlots) {
                    for(const auto& w:session_->Document().widgets) {
                        std::size_t p;std::uint32_t i;
                        if(w.kind==bridge::ScreenWidgetKind::Button&&
                           bridge::ParseInventoryPagedGridSlotAction(w.actionId,p,i)) {
                            SelectWidget(w.id);break;
                        }
                    }
                }
                if(graphicRequested_)graphicRequested_(allSlots);
            });
            inventoryInspector_.Update(canvas,dt,width_-panels_.right+14,HeaderHeight+77,
                panels_.right-28,height_-HeaderHeight-FooterHeight-88);
        }
    }

    void RenegadeScreenEditorWorkspace::RenderControls(
        const wi::Canvas& canvas,
        const wi::graphics::CommandList cmd) const
    {
        for (const wi::gui::Widget* widget : {
                static_cast<const wi::gui::Widget*>(&returnButton_),
                static_cast<const wi::gui::Widget*>(&saveButton_),
                static_cast<const wi::gui::Widget*>(&undoButton_),
                static_cast<const wi::gui::Widget*>(&redoButton_),
                static_cast<const wi::gui::Widget*>(&focusButton_),
                static_cast<const wi::gui::Widget*>(&screenSettingsButton_),
                static_cast<const wi::gui::Widget*>(&fitCanvasButton_),
                static_cast<const wi::gui::Widget*>(&previewButton_),
                static_cast<const wi::gui::Widget*>(&copyBackgroundButton_),
                static_cast<const wi::gui::Widget*>(&cameraBackgroundButton_),
                static_cast<const wi::gui::Widget*>(&panBackgroundButton_),
                static_cast<const wi::gui::Widget*>(&videoLoop_),
                static_cast<const wi::gui::Widget*>(&videoMute_),
                static_cast<const wi::gui::Widget*>(&gridVisibleButton_),
                static_cast<const wi::gui::Widget*>(&gridSnapButton_),
                static_cast<const wi::gui::Widget*>(&gridSpacingInput_),
                static_cast<const wi::gui::Widget*>(&toolboxSearchInput_),
                static_cast<const wi::gui::Widget*>(&stateTabs_[0]),
                static_cast<const wi::gui::Widget*>(&stateTabs_[1]),
                static_cast<const wi::gui::Widget*>(&stateTabs_[2]),
                static_cast<const wi::gui::Widget*>(&stateImage_),
                static_cast<const wi::gui::Widget*>(&stateColour_),
                static_cast<const wi::gui::Widget*>(&clickSound_),
                static_cast<const wi::gui::Widget*>(&previewSound_),
                static_cast<const wi::gui::Widget*>(&defaultSound_),
                static_cast<const wi::gui::Widget*>(&appearanceButton_),
                static_cast<const wi::gui::Widget*>(&imageAssetButton_),
                static_cast<const wi::gui::Widget*>(&progressFrameButton_),
                static_cast<const wi::gui::Widget*>(&progressFillButton_),
                static_cast<const wi::gui::Widget*>(&basicActionButton_),
                static_cast<const wi::gui::Widget*>(&destinationButton_),
                static_cast<const wi::gui::Widget*>(&addTextButton_),
                static_cast<const wi::gui::Widget*>(&addButtonButton_),
                static_cast<const wi::gui::Widget*>(&addImageButton_),
                static_cast<const wi::gui::Widget*>(&addBackgroundButton_),
                static_cast<const wi::gui::Widget*>(&duplicateButton_),
                static_cast<const wi::gui::Widget*>(&deleteButton_),
                static_cast<const wi::gui::Widget*>(&backButton_),
                static_cast<const wi::gui::Widget*>(&backwardOneButton_),
                static_cast<const wi::gui::Widget*>(&forwardOneButton_),
                static_cast<const wi::gui::Widget*>(&frontButton_),
                static_cast<const wi::gui::Widget*>(&nameInput_),
                static_cast<const wi::gui::Widget*>(&textInput_),
                static_cast<const wi::gui::Widget*>(&fillInput_),
                static_cast<const wi::gui::Widget*>(&fillDirectionButton_),
                static_cast<const wi::gui::Widget*>(&positionXInput_),
                static_cast<const wi::gui::Widget*>(&positionYInput_),
                static_cast<const wi::gui::Widget*>(&sizeWidthInput_),
                static_cast<const wi::gui::Widget*>(&sizeHeightInput_),
                static_cast<const wi::gui::Widget*>(&applyButton_),
                static_cast<const wi::gui::Widget*>(&visibleButton_),
                static_cast<const wi::gui::Widget*>(&enabledButton_)})
        {
            if (widget->IsVisible())
            {
                // Owned control renderers draw their backgrounds before setting
                // their text scissor; never inherit the previous field's clip.
                ApplyScissor(canvas, scissorRect, cmd);
                widget->Render(canvas, cmd);
            }
        }
        ApplyScissor(canvas, scissorRect, cmd);
    }

    void RenegadeScreenEditorWorkspace::EnsureSelection()
    {
        if (!session_ || !session_->IsLoaded())
        {selectedWidgetId_.clear();selectedWidgetIds_.clear();return;}
        for(auto i=selectedWidgetIds_.begin();i!=selectedWidgetIds_.end();)
            if(!session_->FindWidget(*i))i=selectedWidgetIds_.erase(i);else ++i;
        if(!session_->FindWidget(selectedWidgetId_))
            selectedWidgetId_=selectedWidgetIds_.empty()
                ? (session_->Document().widgets.empty()?bridge::StableId{}:session_->Document().widgets.front().id)
                : *selectedWidgetIds_.begin();
        if(!selectedWidgetId_.empty()&&!selectedWidgetIds_.count(selectedWidgetId_))
        {selectedWidgetIds_.clear();selectedWidgetIds_.insert(selectedWidgetId_);}
    }

    void RenegadeScreenEditorWorkspace::RefreshInspector()
    {
        EnsureSelection();
        const auto* widget = session_
            ? session_->FindWidget(selectedWidgetId_) : nullptr;
        if (!widget) return;
        nameInput_.SetEnabled(selectedWidgetIds_.size()<=1&&!inspectorSuppressed_);
        nameInput_.SetValue(widget->name);
        textInput_.SetValue(widget->text);
        bridge::ScreenRect resolved = widget->rect;
        std::string geometryError;
        (void)bridge::ResolveScreenWidgetRect(
            session_->Document(), widget->id, resolved, geometryError);
        if(selectedWidgetIds_.size()>1)SelectionBounds(resolved,false);
        const auto number = [](const float value)
        {
            std::ostringstream formatted;
            formatted << std::fixed << std::setprecision(1) << value;
            return formatted.str();
        };
        positionXInput_.SetValue(number(resolved.x));
        positionYInput_.SetValue(number(resolved.y));
        sizeWidthInput_.SetValue(number(resolved.width));
        sizeHeightInput_.SetValue(number(resolved.height));
        {
            std::ostringstream formatted;
            formatted << std::setprecision(6) << widget->fill;
            fillInput_.SetValue(formatted.str());
        }
        const char* directions[] = {"LEFT TO RIGHT", "RIGHT TO LEFT", "BOTTOM TO TOP", "TOP TO BOTTOM"};
        fillDirectionButton_.SetText(std::string("FILL: ") +
            directions[static_cast<int>(widget->fillDirection)]);
        visibleButton_.SetText(widget->visible ? "VISIBLE: YES" : "VISIBLE: NO");
        enabledButton_.SetText(widget->enabled ? "ENABLED: YES" : "ENABLED: NO");
    }

    void RenegadeScreenEditorWorkspace::ApplySelectedWidget()
    {
        if (!session_) return;
        const auto* widget = session_->FindWidget(selectedWidgetId_);
        if (!widget) return;

        bridge::ScreenWidgetAuthoringEdit edit;
        edit.name = Trim(nameInput_.GetValue());
        edit.text = widget->kind == bridge::ScreenWidgetKind::Text || bridge::IsScreenInteractive(widget->kind) ? textInput_.GetValue() : widget->text;
        edit.visible = widget->visible;
        edit.enabled = widget->enabled;
        std::string error;
        if (!bridge::ResolveScreenWidgetRect(session_->Document(), selectedWidgetId_,
                edit.resolvedRect, error))
        {
            SetStatus("EDIT REJECTED // " + error);
            RefreshInspector();
            return;
        }
        const auto number = [](const std::string& input, float& output)
        {
            try
            {
                std::size_t parsed = 0;
                output = std::stof(input, &parsed);
                return parsed == input.size() && std::isfinite(output);
            }
            catch (...) { return false; }
        };
        if (widget->kind == bridge::ScreenWidgetKind::Progress)
        {
            float fill = 0;
            if (!number(fillInput_.GetValue(), fill) || fill < 0 || fill > 1)
            { SetStatus("EDIT REJECTED // FILL MUST BE BETWEEN ZERO AND ONE"); return; }
            edit.fill = fill;
        }
        if (!number(positionXInput_.GetValue(), edit.resolvedRect.x) ||
            !number(positionYInput_.GetValue(), edit.resolvedRect.y) ||
            !number(sizeWidthInput_.GetValue(), edit.resolvedRect.width) ||
            !number(sizeHeightInput_.GetValue(), edit.resolvedRect.height) ||
            edit.resolvedRect.width < 1.0f ||
            edit.resolvedRect.height < 1.0f)
        {
            SetStatus("EDIT REJECTED // X/Y/WIDTH/HEIGHT MUST BE VALID PIXELS");
            return;
        }

        if(selectedWidgetIds_.size()>1)
        {
            bridge::ScreenAuthoringSession::GeometryCapture capture;bridge::ScreenRect bounds;
            if(!session_->CaptureGeometry(SelectedIds(),capture,bounds,error)||
                !session_->TransformGeometry(capture,bounds,edit.resolvedRect,error))
            {SetStatus("BATCH EDIT REJECTED // "+error);RefreshInspector();return;}
            RefreshAfterMutation("SELECTION GEOMETRY UPDATED");return;
        }
        if (!session_->UpdateWidgetManually(selectedWidgetId_, std::move(edit), error))
        {
            SetStatus("EDIT REJECTED // " + error);
            RefreshInspector();
            return;
        }
        RefreshAfterMutation("ELEMENT UPDATED // UNSAVED SCREEN CHANGE");
    }

    bool RenegadeScreenEditorWorkspace::HasPendingSave() const
    {
        return session_ && session_->IsLoaded() &&
            (session_->IsDirty() ||
                (externalFlowDirty_ && externalFlowDirty_()));
    }

    bool RenegadeScreenEditorWorkspace::SavePendingChanges(
        const bool automatic)
    {
        if (!session_ || !session_->IsLoaded()) return false;
        if (!HasPendingSave())
        {
            if (!automatic) SetStatus("ALL SCREEN AND STORYFLOW CHANGES SAVED");
            return true;
        }
        // Atomic Screen/Flow service marks saved history indices only if
        // both files reach disk; Undo/Redo remains available after a Save.
        std::string error;
        const bool saved = coordinatedSave_
            ? coordinatedSave_(*session_, error)
            : session_->Save(error);
        if (!saved)
        {
            SetStatus("AUTOSAVE FAILED // " + error +
                " // CHANGES RETAINED; NAVIGATION BLOCKED");
            wi::backlog::post("Renegade Screen Editor autosave failed: " +
                error, wi::backlog::LogLevel::Error);
            return false;
        }
        SetStatus("AUTOSAVED // SCREEN + STORYFLOW // UNDO AVAILABLE");
        return true;
    }

    void RenegadeScreenEditorWorkspace::SaveScreen()
    {
        (void)SavePendingChanges(false);
    }

    bool RenegadeScreenEditorWorkspace::SavePendingBeforeNavigation()
    {
        if (directManipulationMode_ != DirectManipulationMode::None ||
            !layerDragId_.empty())
        {
            SetStatus("FINISH THE CURRENT GESTURE BEFORE CHANGING SCREENS");
            return false;
        }
        return SavePendingChanges(true);
    }

    void RenegadeScreenEditorWorkspace::TickAutosave(const float dt)
    {
        if (!IsVisible() || !IsEnabled() ||
            !session_ || !session_->IsLoaded()) return;
        const bool inGesture = directManipulationMode_ !=
                DirectManipulationMode::None ||
            !layerDragId_.empty() ||
            panelDrag_ != PanelDrag::None;
        autosave_.Observe(HasPendingSave(), session_->UndoCount(),
            session_->RedoCount(), dt, inGesture);
        if (autosave_.Due())
        {
            autosave_.Attempted(); // Never retry a failed write every frame.
            (void)SavePendingChanges(true);
        }
    }

    void RenegadeScreenEditorWorkspace::HandleUndoRedoShortcuts()
    {
        if(carouselEditorOpen_||liquidEditorOpen_||navigationEditorOpen_||minimapEditorOpen_)return;
        if (!session_ || !session_->IsLoaded() ||
            directManipulationMode_ != DirectManipulationMode::None ||
            panelDrag_ != PanelDrag::None)
            return;
        const bool controlDown =
            wi::input::Down(wi::input::KEYBOARD_BUTTON_LCONTROL) ||
            wi::input::Down(wi::input::KEYBOARD_BUTTON_RCONTROL);
        if (!controlDown) return;
        if (wi::input::Press(static_cast<wi::input::BUTTON>('S')))
        {
            SaveScreen();
            return;
        }
        const bool shiftDown =
            wi::input::Down(wi::input::KEYBOARD_BUTTON_LSHIFT) ||
            wi::input::Down(wi::input::KEYBOARD_BUTTON_RSHIFT);
        if (wi::input::Press(static_cast<wi::input::BUTTON>('Z')))
        {
            if (shiftDown) RedoScreen();
            else UndoScreen();
        }
        else if (wi::input::Press(static_cast<wi::input::BUTTON>('Y')))
            RedoScreen();
    }

    void RenegadeScreenEditorWorkspace::HandleDeleteShortcut()
    {
        if(carouselEditorOpen_||liquidEditorOpen_||navigationEditorOpen_||minimapEditorOpen_)return;
        if (!session_ || !session_->IsLoaded() ||
            session_->FindWidget(selectedWidgetId_) == nullptr ||
            directManipulationMode_ != DirectManipulationMode::None ||
            panelDrag_ != PanelDrag::None ||
            !layerDragId_.empty() ||
            basicActionPicker_.IsOpen() || destinationPicker_.IsOpen())
            return;
        if (wi::input::Press(wi::input::KEYBOARD_BUTTON_DELETE) ||
            wi::input::Press(wi::input::KEYBOARD_BUTTON_BACKSPACE))
            DeleteSelectedWidget();
    }

    void RenegadeScreenEditorWorkspace::HandleLayerOrderShortcuts()
    {
        if(carouselEditorOpen_||liquidEditorOpen_||navigationEditorOpen_||minimapEditorOpen_)return;
#if defined(_WIN32)
        if (!session_ || !session_->IsLoaded() ||
            session_->FindWidget(selectedWidgetId_) == nullptr ||
            panelDrag_ != PanelDrag::None ||
            directManipulationMode_ != DirectManipulationMode::None ||
            !layerDragId_.empty() ||
            basicActionPicker_.IsOpen() ||
            destinationPicker_.IsOpen())
            return;
        const bool ctrl =
            wi::input::Down(wi::input::KEYBOARD_BUTTON_LCONTROL) ||
            wi::input::Down(wi::input::KEYBOARD_BUTTON_RCONTROL);
        const bool alt =
            wi::input::Down(wi::input::KEYBOARD_BUTTON_ALT) ||
            wi::input::Down(wi::input::KEYBOARD_BUTTON_ALTGR);
        if (ctrl || alt) return;
        const bool shift =
            wi::input::Down(wi::input::KEYBOARD_BUTTON_LSHIFT) ||
            wi::input::Down(wi::input::KEYBOARD_BUTTON_RSHIFT);
        // Wicked accepts unlisted Windows VK codes as raw BUTTONs.
        // OEM_4/OEM_6 are the physical [ and ] keys across keyboard layouts.
        constexpr auto Back = static_cast<wi::input::BUTTON>(0xDB);
        constexpr auto Front = static_cast<wi::input::BUTTON>(0xDD);
        if (wi::input::Press(Back))
        {
            if (shift) MoveSelectedWidgetToBack();
            else MoveSelectedWidgetBackward();
        }
        else if (wi::input::Press(Front))
        {
            if (shift) MoveSelectedWidgetToFront();
            else MoveSelectedWidgetForward();
        }
#endif
    }

    void RenegadeScreenEditorWorkspace::UndoScreen()
    {
        if (!session_ || !session_->IsLoaded()) return;
        timeline_.ObserveScreen(session_->UndoCount(), session_->RedoCount(), session_->HistoryMutation());
        const auto kind = timeline_.NextUndo();
        if (kind == ScreenEditorUndoTimeline::Kind::None)
        {
            SetStatus("NOTHING TO UNDO");
            return;
        }
        std::string error;
        if (kind == ScreenEditorUndoTimeline::Kind::FlowRoute)
        {
            if (!routeHistoryAction_ || !routeHistoryAction_(true, error))
            {
                SetStatus("ROUTE UNDO REFUSED // " + error);
                return;
            }
            timeline_.DidUndo(session_->UndoCount(), session_->RedoCount(), session_->HistoryMutation());
            autosave_.Edited();
            SetStatus("STORYFLOW DESTINATION UNDONE // UNSAVED ROUTE CHANGE");
            return;
        }
        if (!session_->Undo(error))
        {
            SetStatus("SCREEN UNDO REFUSED // " + error);
            return;
        }
        timeline_.DidUndo(session_->UndoCount(), session_->RedoCount(), session_->HistoryMutation());
        RefreshAfterMutation("SCREEN UNDO // VALID STATE RESTORED");
    }

    void RenegadeScreenEditorWorkspace::RedoScreen()
    {
        if (!session_ || !session_->IsLoaded()) return;
        timeline_.ObserveScreen(session_->UndoCount(), session_->RedoCount(), session_->HistoryMutation());
        const auto kind = timeline_.NextRedo();
        if (kind == ScreenEditorUndoTimeline::Kind::None)
        {
            SetStatus("NOTHING TO REDO");
            return;
        }
        std::string error;
        if (kind == ScreenEditorUndoTimeline::Kind::FlowRoute)
        {
            if (!routeHistoryAction_ || !routeHistoryAction_(false, error))
            {
                SetStatus("ROUTE REDO REFUSED // " + error);
                return;
            }
            timeline_.DidRedo(session_->UndoCount(), session_->RedoCount(), session_->HistoryMutation());
            autosave_.Edited();
            SetStatus("STORYFLOW DESTINATION REDONE // UNSAVED ROUTE CHANGE");
            return;
        }
        if (!session_->Redo(error))
        {
            SetStatus("SCREEN REDO REFUSED // " + error);
            return;
        }
        timeline_.DidRedo(session_->UndoCount(), session_->RedoCount(), session_->HistoryMutation());
        RefreshAfterMutation("SCREEN REDO // VALID STATE RESTORED");
    }

    void RenegadeScreenEditorWorkspace::ToggleVisible()
    {
        const auto* widget = session_
            ? session_->FindWidget(selectedWidgetId_) : nullptr;
        if (!widget) return;
        bridge::ScreenRect rect;
        std::string error;
        if (!bridge::ResolveScreenWidgetRect(
                session_->Document(), widget->id, rect, error)) return;
        bridge::ScreenWidgetAuthoringEdit edit{
            widget->name, widget->text, rect, !widget->visible, widget->enabled};
        if (!session_->UpdateWidget(widget->id, std::move(edit), error))
        {
            SetStatus("VISIBILITY REJECTED // " + error);
            return;
        }
        RefreshAfterMutation("VISIBILITY UPDATED // UNSAVED SCREEN CHANGE");
    }

    void RenegadeScreenEditorWorkspace::ToggleEnabled()
    {
        const auto* widget = session_
            ? session_->FindWidget(selectedWidgetId_) : nullptr;
        if (!widget || !bridge::IsScreenInteractive(widget->kind)) return;
        bridge::ScreenRect rect;
        std::string error;
        if (!bridge::ResolveScreenWidgetRect(
                session_->Document(), widget->id, rect, error)) return;
        bridge::ScreenWidgetAuthoringEdit edit{
            widget->name, widget->text, rect, widget->visible, !widget->enabled};
        if (!session_->UpdateWidget(widget->id, std::move(edit), error))
        {
            SetStatus("ENABLED STATE REJECTED // " + error);
            return;
        }
        RefreshAfterMutation("ENABLED STATE UPDATED // UNSAVED SCREEN CHANGE");
    }

    void RenegadeScreenEditorWorkspace::ChooseHintList()
    {
        namespace fs=std::filesystem;
        const auto* selected=session_?session_->FindWidget(selectedWidgetId_):nullptr;
        if (!selected) return;
        const auto root=gridProjectRoot_,doc=session_->Document().envelope.documentId,id=selected->id,old=selected->hintListPath;
        const auto valid=[this,root,doc,id] {return session_ && session_->IsLoaded() && gridProjectRoot_==root &&
            session_->Document().envelope.documentId==doc && selectedWidgetId_==id;};
        auto apply=[this,id,valid](const std::string& hint) {
            if (!valid()) return;
            std::string error;
            if (session_->SetHintList(id,hint,error)) RefreshAfterMutation("Hint list updated");
            else SetStatus(error);
        };
        auto write=[this,root,valid,apply](const std::string& path,const std::string& contents) {
            if (!valid()) return;
            bridge::ProjectDocumentWrite file;
            file.destinationPath=(fs::u8path(root)/fs::u8path(path)).generic_u8string();
            file.content.assign(contents.begin(),contents.end());
            bridge::ProjectDocumentTransactionOptions options;
            options.allowedRoot=root;options.journalDirectory=(fs::u8path(root)/"Saved/Transactions").generic_u8string();
            const auto result=bridge::ProjectDocumentTransaction().Execute({std::move(file)},options);
            if (result.success) apply(path); else SetStatus(result.message);
        };
        std::vector<RenegadeScreenChoicePicker::Choice> choices{{"New hint list...","@new"},{"Import text file...","@import"},
            {"Use displayed text","@none"}};
        if (!old.empty()) choices.insert(choices.begin(),{"Edit selected list...","@edit"});
        std::error_code ec;
        auto folder=fs::u8path(root)/"Content/UI/Hints";
        for (fs::directory_iterator it(folder,ec),end;!ec && it!=end;it.increment(ec))
            if (it->is_regular_file(ec) && it->path().extension()==".txt")
                choices.push_back({it->path().stem().u8string(),it->path().lexically_relative(fs::u8path(root)).generic_u8string()});
        destinationPicker_.Open("HINT LIST",std::move(choices),old,
            [this,valid,apply,write,root,old](const std::string& choice) {
                if (!valid()) return;
                if (choice=="@import")
                {
                    wi::helper::FileDialogParams dialog;
                    dialog.type=wi::helper::FileDialogParams::OPEN;dialog.description="Import one hint per line";dialog.extensions={"txt"};
                    wi::helper::FileDialog(dialog,[this,valid,write](const std::string& path){
                        if (path.empty()) return;
                        wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this,valid,write,path](uint64_t){
                            if (!valid()) return;
                            std::ifstream in(fs::u8path(path),std::ios::binary);
                            if (!in) {SetStatus("Could not read hint list.");return;}
                            std::string content{std::istreambuf_iterator<char>(in),{}};
                            write("Content/UI/Hints/"+fs::u8path(path).stem().u8string()+"-"+bridge::GenerateStableId()+".txt",content);
                        });
                    });
                }
                else if (choice=="@edit" || choice=="@new")
                {
                    std::string value,absolute,error;
                    if (choice=="@edit" && bridge::ResolveScreenResourcePath(root,old,absolute,error))
                    {
                        std::ifstream in(fs::u8path(absolute),std::ios::binary);
                        value.assign(std::istreambuf_iterator<char>(in),{});
                    }
                    if (EditHintText(value)) write(choice=="@edit"?old:"Content/UI/Hints/Hints-"+bridge::GenerateStableId()+".txt",value);
                }
                else apply(choice=="@none"?"":choice);
            });
    }

    void RenegadeScreenEditorWorkspace::DropScreenVideo(const std::string& path,const XMFLOAT4& point)
    {
        if (!session_ || !session_->IsLoaded() || !Contains(PreviewViewport(),point) || Contains(MinimapBounds(),point)) return;
        const auto format=bridge::DetectResourceSourceFormat(path);
        if (bridge::ClassifyResourceSourceFormat(format)!=bridge::ResourceClass::Video) return;
        std::string error;
        if (!bridge::ValidateScreenVideoFile(path,error)) {SetStatus(error);return;}
        const auto result=bridge::CreatorVideoWorkflowService().ImportVideo(gridProjectRoot_,session_->ProjectId(),path);
        if (!result.succeeded) {SetStatus(result.error);return;}
        CreateElement(bridge::ScreenWidgetKind::Image,false,&point,true,result.assetId);
    }

    void RenegadeScreenEditorWorkspace::ChooseScreenMedia()
    {
        if (!session_ || !session_->IsLoaded()) return;
        const auto* selected=session_->FindWidget(selectedWidgetId_);
        if (!selected) return;
        const auto widget=selected->id;
        const auto document=session_->Document().envelope.documentId;
        const auto project=session_->ProjectId();
        const auto root=gridProjectRoot_;
        auto current=selected->video.value_or(bridge::ScreenVideoPanel{});
        const auto valid=[this,widget,document,root] {
            return session_ && session_->IsLoaded() && gridProjectRoot_==root &&
                session_->Document().envelope.documentId==document && selectedWidgetId_==widget;
        };
        auto apply=[this,valid,widget](bridge::ScreenVideoPanel panel) {
            if (!valid()) return;
            std::string error;
            if (session_->SetVideoPanel(widget,std::move(panel),error)) RefreshAfterMutation("Video panel updated");
            else SetStatus(error);
        };
        std::vector<RenegadeScreenChoicePicker::Choice> choices;
        if (bridge::IsScreenBackground(session_->Document(),*selected))
        {
            choices.push_back({"Choose image...","@image"});
            if (selected->cameraSource || selected->video) choices.push_back({"Change fallback image...","@fallback"});
        }
        choices.push_back({"Import video...","@import"});
        if (selected->video)
        {
            constexpr const char* transparencyNames[]={"Automatic","Opaque","Remove black","Soft smoke"};
            choices.push_back({std::string("Transparency: ")+transparencyNames[static_cast<unsigned>(current.transparency)],"@transparency"});
            choices.push_back({current.additive ? "Blend: additive" : "Blend: normal","@blend"});
        }
        bridge::AssetRegistry registry;
        std::string error;
        if (bridge::ReadAssetRegistry(root,project,registry,error))
            for (const auto& item:registry.records)
                if (item.dependencyClass==bridge::DependencyClass::Video && item.provider=="lp08.rasset" && item.sourceAvailable)
                    choices.push_back({std::filesystem::u8path(item.projectRelativePath).stem().u8string(),item.assetId});
        const auto details=renderer_ ? renderer_->VideoInfo(widget) : std::string{};
        destinationPicker_.Open(details.empty() ? "SCREEN MEDIA" : details,std::move(choices),current.assetId,
            [this,project,root,valid,apply,current](const std::string& choice) mutable {
                if (!valid()) return;
                if (choice=="@image" || choice=="@fallback")
                {
                    chooseImageSource_=choice=="@image";
                    if (graphicRequested_) graphicRequested_(false);
                    chooseImageSource_=false;
                    return;
                }
                if (choice=="@transparency")
                {
                    destinationPicker_.Open("VIDEO TRANSPARENCY",{{"Automatic (stock artwork)","0"},{"Opaque","1"},
                        {"Remove black background","2"},{"Soft smoke (feather edges)","3"}},
                        std::to_string(static_cast<int>(current.transparency)),
                        [current,apply,valid](const std::string& mode) mutable {
                            if (!valid() || mode.size()!=1 || mode[0]<'0' || mode[0]>'3') return;
                            current.transparency=static_cast<bridge::VideoTransparency>(mode[0]-'0');
                            apply(current);
                        });
                    return;
                }
                if (choice=="@blend") { current.additive=!current.additive; apply(current); return; }
                if (choice!="@import") { current.assetId=choice; apply(current); return; }
                wi::helper::FileDialogParams dialog;
                dialog.type=wi::helper::FileDialogParams::OPEN;
                dialog.description="Import H.264 video";
                for (const auto& extension:wi::resourcemanager::GetSupportedVideoExtensions())
                    dialog.extensions.push_back(extension);
                wi::helper::FileDialog(dialog,[this,project,root,valid,apply,current](const std::string& path) {
                    if (path.empty()) return;
                    wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                        [this,project,root,valid,apply,current,path](std::uint64_t) mutable {
                            if (!valid()) return;
                            std::string error;
                            if (!bridge::ValidateScreenVideoFile(path,error)) {SetStatus(error);return;}
                            const auto result=bridge::CreatorVideoWorkflowService().ImportVideo(root,project,path);
                            if (!result.succeeded) { SetStatus(result.error); return; }
                            auto panel=current;
                            panel.assetId=result.assetId;
                            apply(panel);
                        });
                });
            });
    }

    void RenegadeScreenEditorWorkspace::ChooseBackgroundCamera()
    {
        if(!session_ || !session_->IsLoaded())return;
        const auto widget=selectedWidgetId_;
        const auto doc=session_->Document().envelope.documentId;
        auto apply=[this,widget,doc](std::optional<bridge::ScreenCameraSource> source){
            if(!session_ || !session_->IsLoaded() || selectedWidgetId_!=widget ||
                session_->Document().envelope.documentId!=doc)return;
            std::string error;
            if(session_->SetBackgroundCamera(widget,std::move(source),error))RefreshAfterMutation("Background source updated");
            else SetStatus(error);
        };
        std::vector<RenegadeScreenChoicePicker::Choice> levels{{"Use image background","@image"}};
        std::map<std::string,std::string> hints;
        for(const auto& entry:projectScreens_)
            if(entry.available && !entry.cameraLevelId.empty() &&
                hints.emplace(entry.cameraLevelId,entry.cameraLevelPath).second)
                levels.push_back({"Level: "+entry.name,entry.cameraLevelId});
        destinationPicker_.Open("BACKGROUND SOURCE / LEVEL",std::move(levels),{},
            [this,apply,hints](const std::string& level){
                if(level=="@image"){apply(std::nullopt);return;}
                std::string path,error;
                if(!bridge::ResolveSceneDocumentPath(gridProjectRoot_,session_->ProjectId(),level,hints.at(level),path,error))
                {SetStatus(error);return;}
                const auto prepared=bridge::PrepareWickedSceneOpen(path);
                if(!prepared.IsReady()){SetStatus(prepared.Error());return;}
                const auto& scene=*prepared.ReadOnlyScene();
                std::vector<RenegadeScreenChoicePicker::Choice> choices;
                std::map<std::string,std::string> names;
                for(size_t i=0;i<scene.cameras.GetCount();++i)
                {
                    const auto entity=scene.cameras.GetEntity(i);
                    const auto id=bridge::PersistentEntityId(scene,entity);
                    if(!bridge::IsValidStableId(id))continue;
                    const auto* name=scene.names.GetComponent(entity);
                    names[id]=name && !name->name.empty()?name->name:"Camera";
                    choices.push_back({names[id],id});
                }
                if(choices.empty()){SetStatus("No saved camera. Add a Camera in the Level Editor and save the level.");return;}
                destinationPicker_.Open("CHOOSE LEVEL CAMERA",std::move(choices),{},
                    [apply,level,hint=hints.at(level),names](const std::string& id){
                        apply(bridge::ScreenCameraSource{level,hint,id,names.at(id),false});
                    });
            });
    }

    void RenegadeScreenEditorWorkspace::ChooseBackgroundDestinations()
    {
        if (!session_ || !session_->IsLoaded()) return;
        const auto sourceId=session_->Document().envelope.documentId;
        const auto widgetId=selectedWidgetId_;
        std::map<std::string,std::string> hints;
        std::vector<RenegadeScreenChoicePicker::Choice> choices;
        for(const auto& screen:projectScreens_)
            if(screen.available && screen.documentId!=sourceId && !screen.documentId.empty() &&
                hints.emplace(screen.documentId,screen.pathHint).second)
                choices.push_back({screen.name,screen.documentId});
        destinationPicker_.OpenChecklist("COPY BACKGROUND TO SCREENS",std::move(choices),
            [this,sourceId,widgetId,hints](const auto& checked){
                if(!session_ || !session_->IsLoaded() || session_->Document().envelope.documentId!=sourceId ||
                    selectedWidgetId_!=widgetId || !SavePendingBeforeNavigation())return;
                std::map<std::string,std::string> targets;
                for(const auto& id:checked)targets.emplace(id,hints.at(id));
                std::string error;
                const auto& doc=session_->Document();
                for(const auto& panel:doc.widgets)if(panel.id==widgetId)
                {
                    if(bridge::CopyScreenBackgrounds(gridProjectRoot_,doc,panel,targets,error))
                        SetStatus("Background copied to "+std::to_string(targets.size())+" screen(s)");
                    else SetStatus(error);
                    break;
                }
            });
    }

    void RenegadeScreenEditorWorkspace::CreateElement(
        const bridge::ScreenWidgetKind kind,
        const bool background, const XMFLOAT4* drop, bool video, const bridge::StableId& videoId, bool carousel)
    {
        if (!session_ || !session_->IsLoaded()) return;
        const auto& document = session_->Document();

        bridge::ScreenWidget widget;
        widget.kind = kind;
        widget.visible = true;
        widget.enabled = kind == bridge::ScreenWidgetKind::Button;

        if (background)
        {
            for(const auto& existing:document.widgets)
                if(bridge::IsScreenBackground(document,existing))
                {
                    SelectWidget(existing.id);
                    SetStatus("Background panel selected");
                    return;
                }
            widget.backgroundPanel=true;
            widget.layoutMode=bridge::ScreenLayoutMode::Anchored;
            widget.anchors={0,0,1,1,0,0,0,0};
            widget.name = "Background";
            widget.rect = {0.0f, 0.0f, document.designWidth, document.designHeight};
            widget.enabled = false;
            widget.style = bridge::MakeScreenWidgetStyleTemplate(
                bridge::ScreenWidgetKind::Image, widget.rect.height);
            widget.style.normal.background = {12, 16, 20, 255};
            widget.style.hover = widget.style.normal;
            widget.style.pressed = widget.style.normal;
            widget.style.focused = widget.style.normal;
            widget.style.disabled = widget.style.normal;
        }
        else if (kind == bridge::ScreenWidgetKind::Progress)
        {
            widget.name = "Progress";
            widget.rect = {document.designWidth * 0.25f, document.designHeight * 0.5f,
                document.designWidth * 0.5f, 32.0f};
            widget.enabled = false;
            widget.style = bridge::MakeScreenWidgetStyleTemplate(kind, widget.rect.height);
        }
        else if (kind == bridge::ScreenWidgetKind::Text)
        {
            widget.name = "Text";
            widget.text = "TEXT";
            widget.rect.width = std::min(640.0f, document.designWidth - 40.0f);
            widget.rect.height = std::min(96.0f, document.designHeight - 40.0f);
            widget.rect.x = (document.designWidth - widget.rect.width) * 0.5f;
            widget.rect.y = std::max(20.0f, document.designHeight * 0.2f);
            if (widget.rect.y + widget.rect.height > document.designHeight)
                widget.rect.y = document.designHeight - widget.rect.height;
            widget.enabled = false;
            widget.style = bridge::MakeScreenWidgetStyleTemplate(
                kind, widget.rect.height);
        }
        else if (kind == bridge::ScreenWidgetKind::Button)
        {
            widget.name = "Button";
            widget.text = "BUTTON";
            // The authoring session allocates a new, independent symbolic
            // outcome in the same Undo step as Button creation. Never reuse
            // the first existing button's action or require a Lua script.
            widget.rect.width = std::min(400.0f, document.designWidth - 40.0f);
            widget.rect.height = std::min(76.0f, document.designHeight - 40.0f);
            widget.rect.x = (document.designWidth - widget.rect.width) * 0.5f;
            widget.rect.y = (document.designHeight - widget.rect.height) * 0.5f;
            widget.style = bridge::MakeScreenWidgetStyleTemplate(
                kind, widget.rect.height);
        }
        else
        {
            widget.name = carousel ? "Carousel" : video ? "Video" : "Image";
            if (carousel) {widget.carousel.emplace();widget.imageFit=bridge::ScreenImageFit::Contain;}
            if (video) widget.video.emplace().assetId=videoId;
            widget.rect.width = std::min(480.0f, document.designWidth - 40.0f);
            widget.rect.height = std::min(270.0f, document.designHeight - 40.0f);
            widget.rect.x = (document.designWidth - widget.rect.width) * 0.5f;
            widget.rect.y = (document.designHeight - widget.rect.height) * 0.5f;
            widget.enabled = false;
            widget.style = bridge::MakeScreenWidgetStyleTemplate(
                kind, widget.rect.height);
            widget.style.normal.background = {64, 76, 84, 96};
            widget.style.hover = widget.style.normal;
            widget.style.pressed = widget.style.normal;
            widget.style.focused = widget.style.normal;
            widget.style.disabled = widget.style.normal;
        }

        bridge::StableId created;
        std::string error;
        if (drop)
        {
            const auto view = PreviewViewport();
            widget.rect.x = std::clamp((drop->x-view.x)*document.designWidth/view.width-widget.rect.width/2, 0.0f, document.designWidth-widget.rect.width);
            widget.rect.y = std::clamp((drop->y-view.y)*document.designHeight/view.height-widget.rect.height/2, 0.0f, document.designHeight-widget.rect.height);
            if (gridSnapEnabled_ && !wi::input::Down(wi::input::KEYBOARD_BUTTON_ALT) && !wi::input::Down(wi::input::KEYBOARD_BUTTON_ALTGR))
                widget.rect = SnapScreenGridRect(widget.rect, ScreenGridGesture::Move, static_cast<float>(gridSpacing_), document.designWidth, document.designHeight);
        }
        if(carousel)
        {
            // The linked caption begins eight pixels below the image.
            // Keep the entire newly created pair on the design canvas.
            widget.rect.height=std::min(widget.rect.height,std::max(1.0f,document.designHeight-88.0f));
            widget.rect.y=std::clamp(widget.rect.y,0.0f,std::max(0.0f,document.designHeight-widget.rect.height-88.0f));
        }
        const bool createdOk = kind == bridge::ScreenWidgetKind::Button
            ? session_->CreateButtonWithUniqueAction(
                std::move(widget), created, error)
            : session_->CreateWidget(
                std::move(widget), background, created, error);
        if (!createdOk)
        {
            SetStatus("CREATE REJECTED // " + error);
            return;
        }
        selectedWidgetId_ = created;
        hierarchyScroll_ = 0;
        RefreshAfterMutation(background
            ? "BACKGROUND CREATED // UNSAVED SCREEN CHANGE"
            : "ELEMENT CREATED // UNSAVED SCREEN CHANGE");
    }

    void RenegadeScreenEditorWorkspace::DuplicateSelectedWidget()
    {
        if(!session_||selectedWidgetId_.empty())return;
        bridge::ScreenWidgetClipboard copy;std::string error;
        if(!session_->CopyWidgets(SelectedIds(),copy,error))
        {SetStatus("DUPLICATE REJECTED // "+error);return;}
        std::vector<bridge::StableId> ids;
        if(!session_->PasteWidgets(copy,20,20,ids,error))
        {SetStatus("DUPLICATE REJECTED // "+error);return;}
        selectedWidgetIds_.clear();selectedWidgetIds_.insert(ids.begin(),ids.end());
        selectedWidgetId_=ids.empty()?bridge::StableId{}:ids.front();
        RefreshAfterMutation("SELECTION DUPLICATED // ONE UNDO STEP");
    }

    void RenegadeScreenEditorWorkspace::DeleteSelectedWidget()
    {
        if (!session_ || selectedWidgetId_.empty()) return;
        std::string error;
        if (!session_->DeleteWidgets(SelectedIds(), error))
        {
            SetStatus("DELETE REJECTED // " + error);
            return;
        }
        selectedWidgetId_.clear();
        selectedWidgetIds_.clear();
        hierarchyScroll_ = 0;
        RefreshAfterMutation("ELEMENT DELETED // UNSAVED SCREEN CHANGE");
    }

    void RenegadeScreenEditorWorkspace::MoveSelectedWidgetToBack()
    {
        if (!session_ || selectedWidgetId_.empty()) return;
        std::string error;
        if (!session_->MoveWidgetsLayer(SelectedIds(), -1, true, error))
        {
            SetStatus("LAYER MOVE REJECTED // " + error);
            return;
        }
        RefreshAfterMutation("ELEMENT SENT TO BACK // UNSAVED SCREEN CHANGE");
    }

    void RenegadeScreenEditorWorkspace::MoveSelectedWidgetBackward()
    {
        if (!session_ || selectedWidgetId_.empty()) return;
        std::string error;
        if (!session_->MoveWidgetsLayer(SelectedIds(), -1, false, error))
        {
            SetStatus("LAYER MOVE REJECTED // " + error);
            return;
        }
        RefreshAfterMutation("ELEMENT SENT BACK ONE LAYER // UNSAVED");
    }

    void RenegadeScreenEditorWorkspace::MoveSelectedWidgetForward()
    {
        if (!session_ || selectedWidgetId_.empty()) return;
        std::string error;
        if (!session_->MoveWidgetsLayer(SelectedIds(), 1, false, error))
        {
            SetStatus("LAYER MOVE REJECTED // " + error);
            return;
        }
        RefreshAfterMutation("ELEMENT BROUGHT FORWARD ONE LAYER // UNSAVED");
    }

    void RenegadeScreenEditorWorkspace::MoveSelectedWidgetToFront()
    {
        if (!session_ || selectedWidgetId_.empty()) return;
        std::string error;
        if (!session_->MoveWidgetsLayer(SelectedIds(), 1, true, error))
        {
            SetStatus("LAYER MOVE REJECTED // " + error);
            return;
        }
        RefreshAfterMutation("ELEMENT BROUGHT TO FRONT // UNSAVED SCREEN CHANGE");
    }

    void RenegadeScreenEditorWorkspace::ReturnToStoryFlow()
    {
        if (!SavePendingBeforeNavigation()) return;
        if (returnRequested_) returnRequested_();
    }

    void RenegadeScreenEditorWorkspace::RefreshAfterMutation(
        std::string status)
    {
        EnsureSelection();
        RefreshInspector();
        if (session_ && session_->IsLoaded())
            timeline_.ObserveScreen(session_->UndoCount(), session_->RedoCount(), session_->HistoryMutation());
        if (documentChanged_) documentChanged_();
        SetStatus(std::move(status));
    }

    void RenegadeScreenEditorWorkspace::SetStatus(std::string status)
    {
        status_ = std::move(status);
    }

    void RenegadeScreenEditorWorkspace::SetGridSpacingFromInput()
    {
        const std::string value = Trim(gridSpacingInput_.GetValue());
        char* end = nullptr;
        const long parsed = std::strtol(value.c_str(), &end, 10);
        if (value.empty() || end == value.c_str() || *end != '\0' ||
            parsed < 4 || parsed > 128)
        {
            gridSpacingInput_.SetValue(std::to_string(gridSpacing_));
            SetStatus("GRID SIZE MUST BE A WHOLE NUMBER FROM 4 TO 128");
            return;
        }
        gridSpacing_ = static_cast<int>(parsed);
        if(!SaveProjectGrid())return;
        gridSpacingInput_.SetValue(std::to_string(gridSpacing_));
        NotifyPreferencesChanged();
        SetStatus("GRID SPACING // " + std::to_string(gridSpacing_) +
            " DESIGN PIXELS");
    }

    RenegadeScreenEditorWorkspace::DirectManipulationMode RenegadeScreenEditorWorkspace::HitResizeHandle(const XMFLOAT4& pointer) const noexcept
    {
        if (!renderer_ || selectedWidgetId_.empty()) return DirectManipulationMode::None;
        if(selectedWidgetIds_.size()>1){bridge::ScreenAuthoringSession::GeometryCapture capture;bridge::ScreenRect bounds;std::string error;
            if(!session_->CaptureGeometry(SelectedIds(),capture,bounds,error))return DirectManipulationMode::None;
        }
        bridge::ScreenRect box; if(!SelectionBounds(box,true))return DirectManipulationMode::None; const auto* r=&box;
        const float h=ResizeHandleSize*0.5f; const auto hit=[&](float x,float y){return pointer.x>=x-h&&pointer.x<=x+h&&pointer.y>=y-h&&pointer.y<=y+h;};
        if(hit(r->x,r->y))return DirectManipulationMode::ResizeTopLeft; if(hit(r->x+r->width,r->y))return DirectManipulationMode::ResizeTopRight; if(hit(r->x,r->y+r->height))return DirectManipulationMode::ResizeBottomLeft; if(hit(r->x+r->width,r->y+r->height))return DirectManipulationMode::ResizeBottomRight; return DirectManipulationMode::None;
    }
    void RenegadeScreenEditorWorkspace::BeginDirectManipulation(DirectManipulationMode mode,const XMFLOAT4& pointer)
    {
        if(!session_||!renderer_||selectedWidgetId_.empty()||mode==DirectManipulationMode::None)return; const auto* logical=renderer_->LogicalRect(selectedWidgetId_); bridge::ScreenRect resolved; std::string error;
        if(!logical||!bridge::ResolveScreenWidgetRect(session_->Document(),selectedWidgetId_,resolved,error)||resolved.width<=0||resolved.height<=0||logical->width<=0||logical->height<=0){SetStatus("DIRECT EDIT REJECTED // COULD NOT RESOLVE ELEMENT GEOMETRY");return;}
        if(selectedWidgetIds_.size()>1){
            bridge::ScreenRect box;
            if(!SelectionBounds(box,true)||!session_->CaptureGeometry(SelectedIds(),geometryCapture_,resolved,error))
            {SetStatus("BATCH GEOMETRY REJECTED // "+error);return;}
            static bridge::ScreenRect logicalBox; logicalBox=box;logical=&logicalBox;
        }
        alignmentGuideX_ = false; alignmentGuideY_ = false;
        directManipulationMode_=mode; directManipulationStartPointer_={pointer.x,pointer.y}; directManipulationStartRect_=resolved; directManipulationScaleX_=logical->width/resolved.width; directManipulationScaleY_=logical->height/resolved.height; directManipulationChanged_=false; timeline_.ObserveScreen(session_->UndoCount(),session_->RedoCount(),session_->HistoryMutation());gestureTimeline_=timeline_;session_->BeginCoalescedEdit(); SetStatus(mode==DirectManipulationMode::Move?"DRAGGING ELEMENT // RELEASE TO FINISH":"RESIZING ELEMENT // RELEASE TO FINISH");
    }
    void RenegadeScreenEditorWorkspace::UpdateDirectManipulation(const XMFLOAT4& pointer)
    {
        if(!session_||directManipulationMode_==DirectManipulationMode::None||directManipulationScaleX_<=0||directManipulationScaleY_<=0)return; const auto* w=session_->FindWidget(selectedWidgetId_); if(!w){EndDirectManipulation();return;}
        const float dx=(pointer.x-directManipulationStartPointer_.x)/directManipulationScaleX_,dy=(pointer.y-directManipulationStartPointer_.y)/directManipulationScaleY_; bridge::ScreenRect n=directManipulationStartRect_; const float cw=session_->Document().designWidth,ch=session_->Document().designHeight,m=8.0f;
        // A simple selection click must never snap an untouched element.
        if (!directManipulationChanged_ &&
            std::abs(dx) < 0.5f && std::abs(dy) < 0.5f)
            return;
        switch(directManipulationMode_){case DirectManipulationMode::Move:n.x=std::clamp(directManipulationStartRect_.x+dx,0.0f,std::max(0.0f,cw-directManipulationStartRect_.width));n.y=std::clamp(directManipulationStartRect_.y+dy,0.0f,std::max(0.0f,ch-directManipulationStartRect_.height));break;case DirectManipulationMode::ResizeTopLeft:{float r=directManipulationStartRect_.x+directManipulationStartRect_.width,b=directManipulationStartRect_.y+directManipulationStartRect_.height;n.x=std::clamp(directManipulationStartRect_.x+dx,0.0f,r-m);n.y=std::clamp(directManipulationStartRect_.y+dy,0.0f,b-m);n.width=r-n.x;n.height=b-n.y;break;}case DirectManipulationMode::ResizeTopRight:{float b=directManipulationStartRect_.y+directManipulationStartRect_.height,r=std::clamp(directManipulationStartRect_.x+directManipulationStartRect_.width+dx,directManipulationStartRect_.x+m,cw);n.y=std::clamp(directManipulationStartRect_.y+dy,0.0f,b-m);n.width=r-directManipulationStartRect_.x;n.height=b-n.y;break;}case DirectManipulationMode::ResizeBottomLeft:{float r=directManipulationStartRect_.x+directManipulationStartRect_.width,b=std::clamp(directManipulationStartRect_.y+directManipulationStartRect_.height+dy,directManipulationStartRect_.y+m,ch);n.x=std::clamp(directManipulationStartRect_.x+dx,0.0f,r-m);n.width=r-n.x;n.height=b-directManipulationStartRect_.y;break;}case DirectManipulationMode::ResizeBottomRight:{float r=std::clamp(directManipulationStartRect_.x+directManipulationStartRect_.width+dx,directManipulationStartRect_.x+m,cw),b=std::clamp(directManipulationStartRect_.y+directManipulationStartRect_.height+dy,directManipulationStartRect_.y+m,ch);n.width=r-directManipulationStartRect_.x;n.height=b-directManipulationStartRect_.y;break;}case DirectManipulationMode::None:return;}
        // Work entirely in authored design pixels. Alt temporarily disables
        // snapping without changing the saved geometry or grid setting.
        const bool bypassSnap =
            wi::input::Down(wi::input::KEYBOARD_BUTTON_ALT) ||
            wi::input::Down(wi::input::KEYBOARD_BUTTON_ALTGR);
        alignmentGuideX_ = false;
        alignmentGuideY_ = false;
        if (gridSnapEnabled_ && !bypassSnap)
        {
            ScreenGridGesture gesture = ScreenGridGesture::Move;
            switch (directManipulationMode_)
            {
            case DirectManipulationMode::Move:
                gesture = ScreenGridGesture::Move; break;
            case DirectManipulationMode::ResizeTopLeft:
                gesture = ScreenGridGesture::ResizeTopLeft; break;
            case DirectManipulationMode::ResizeTopRight:
                gesture = ScreenGridGesture::ResizeTopRight; break;
            case DirectManipulationMode::ResizeBottomLeft:
                gesture = ScreenGridGesture::ResizeBottomLeft; break;
            case DirectManipulationMode::ResizeBottomRight:
                gesture = ScreenGridGesture::ResizeBottomRight; break;
            case DirectManipulationMode::None:
                return;
            }

            const bridge::ScreenRect raw = n;
            n = SnapScreenGridRect(
                raw, gesture, static_cast<float>(gridSpacing_), cw, ch);

            // Restrict alignment to visible siblings. Every peer is resolved
            // through ScreenService, so anchors and parent transforms match
            // the Runtime's design-space geometry.
            std::vector<bridge::ScreenRect> peers;
            for (const auto& sibling : session_->Document().widgets)
            {
                if (selectedWidgetIds_.count(sibling.id) ||
                    !sibling.visible || sibling.parentId != w->parentId)
                    continue;
                bridge::ScreenRect resolved;
                std::string resolveError;
                if (bridge::ResolveScreenWidgetRect(
                        session_->Document(), sibling.id, resolved, resolveError))
                    peers.push_back(resolved);
            }
            bridge::ScreenCanvasTransform transform;
            std::string transformError;
            const auto canvas = PreviewViewport();
            if (!peers.empty() && bridge::ResolveScreenCanvasTransform(
                    session_->Document(), canvas.width, canvas.height,
                    transform, transformError))
            {
                // An eight-pixel magnetic range stays visually consistent when
                // the Screen Designer viewport is resized or letterboxed.
                const auto aligned = SnapScreenToSiblings(
                    raw, n, gesture, peers,
                    8.0f / std::max(0.001f, transform.scaleX),
                    8.0f / std::max(0.001f, transform.scaleY),
                    cw, ch);
                n = aligned.rect;
                alignmentGuideX_ = aligned.guideX;
                alignmentGuideY_ = aligned.guideY;
                alignmentGuideCoordinateX_ = aligned.guideCoordinateX;
                alignmentGuideCoordinateY_ = aligned.guideCoordinateY;
            }
        }
        std::string error;
        if(selectedWidgetIds_.size()>1)
        {
            if(!session_->TransformGeometry(geometryCapture_,directManipulationStartRect_,n,error))
            {SetStatus("BATCH GEOMETRY REJECTED // "+error);return;}
        }
        else
        {
            bridge::ScreenWidgetAuthoringEdit e{w->name,w->text,n,w->visible,w->enabled};
            if(!session_->UpdateWidgetManually(selectedWidgetId_,std::move(e),error))
            {SetStatus("DIRECT EDIT REJECTED // "+error);return;}
        }
        directManipulationChanged_=true;
        timeline_.ObserveScreen(session_->UndoCount(), session_->RedoCount(), session_->HistoryMutation());
        RefreshInspector();if(documentChanged_)documentChanged_();
    }
    void RenegadeScreenEditorWorkspace::EndDirectManipulation(){alignmentGuideX_=false;alignmentGuideY_=false;if(session_)session_->EndCoalescedEdit();if(directManipulationMode_!=DirectManipulationMode::None)SetStatus(directManipulationChanged_?"DIRECT GEOMETRY UPDATED // UNSAVED SCREEN CHANGE":"ELEMENT SELECTED // DRAG TO MOVE OR RESIZE");directManipulationMode_=DirectManipulationMode::None;directManipulationChanged_=false;}

    void RenegadeScreenEditorWorkspace::FinishEditingGesture()
    {
        EndDirectManipulation();
        cameraPanning_=false;
        panelDrag_=PanelDrag::None;
        layerDragId_.clear();
        paletteButtonPending_=false;
        wi::gui::TextInputField* fields[]={&toolboxSearchInput_,&gridSpacingInput_,
            &nameInput_,&textInput_,&positionXInput_,&positionYInput_,
            &sizeWidthInput_,&sizeHeightInput_,&fillInput_};
        for(auto* field:fields)field->Deactivate();
    }

    void RenegadeScreenEditorWorkspace::Update(const wi::Canvas& canvas,const float dt)
    {
        if (!IsVisible() || !IsEnabled())
        {if(minimapEditor_&&minimapEditorOpen_)minimapEditor_->Close();if(navigationEditor_&&navigationEditorOpen_)navigationEditor_->Close();if(carouselEditor_&&carouselEditorOpen_)carouselEditor_->Close();if(liquidEditor_&&liquidEditorOpen_)liquidEditor_->Close();return;}
        Widget::Update(canvas,dt);
        if(minimapEditor_&&minimapEditor_->IsVisible()){minimapEditor_->Update(canvas,dt);return;}
        if(navigationEditor_&&navigationEditor_->IsVisible())
        {navigationEditor_->Update(canvas,dt);return;}
        if(liquidEditor_&&liquidEditor_->IsVisible())
        {liquidEditor_->Update(canvas,dt);return;}
        if(carouselEditor_&&carouselEditor_->IsVisible())
        {carouselEditor_->Update(canvas,dt);return;}
        if (overlayInputBlocked_) return;
        // Modal pickers own input before underlying Inspector widgets update.
        // Updating widgets first can replace the picker and callback under its click.
        if (screenSettingsPicker_.IsOpen()) { screenSettingsPicker_.Update(); return; }
        if (basicActionPicker_.IsOpen()) { basicActionPicker_.Update(); return; }
        if (destinationPicker_.IsOpen()) { destinationPicker_.Update(); return; }
        UpdateControls(canvas,dt);
        if(minimapEditor_&&minimapEditor_->IsVisible()){minimapEditor_->Update(canvas,dt);return;}
        if(navigationEditor_&&navigationEditor_->IsVisible())
        {navigationEditor_->Update(canvas,dt);return;}
        if(liquidEditor_&&liquidEditor_->IsVisible())
        {liquidEditor_->Update(canvas,dt);return;}
        if(carouselEditor_&&carouselEditor_->IsVisible())
        {carouselEditor_->Update(canvas,dt);return;}
        toolboxFilter_ = toolboxSearchInput_.GetValue();
        if (screenSettingsPicker_.IsOpen())
        {
            screenSettingsPicker_.Update();
            return;
        }
        if (basicActionPicker_.IsOpen())
        {
            basicActionPicker_.Update();
            return;
        }
        if (destinationPicker_.IsOpen())
        {
            destinationPicker_.Update();
            return;
        }
        const XMFLOAT4 pointer = wi::input::GetPointer();
        const bool holdingLeft = wi::input::Down(wi::input::MOUSE_BUTTON_LEFT);
        if (paletteButtonPending_)
        {
            if (wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE)) paletteButtonPending_ = false;
            else if (!holdingLeft)
            {
                paletteButtonPending_ = false;
                if (Contains(PreviewBounds(), pointer) && Contains(PreviewViewport(), pointer) && !Contains(MinimapBounds(), pointer))
                    CreateElement((paletteVideo_ || paletteCarousel_) ? bridge::ScreenWidgetKind::Image : bridge::ScreenWidgetKind::Button, false, &pointer, paletteVideo_,{},paletteCarousel_);
                else if (std::abs(pointer.x-palettePress_.x)<5 && std::abs(pointer.y-palettePress_.y)<5)
                    CreateElement((paletteVideo_ || paletteCarousel_) ? bridge::ScreenWidgetKind::Image : bridge::ScreenWidgetKind::Button, false, nullptr, paletteVideo_,{},paletteCarousel_);
            }
            return;
        }
        if(!focusCanvas_ && pointer.y>=HeaderHeight+4 && pointer.y<=HeaderHeight+28 && wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)){
            if(pointer.x>=0&&pointer.x<=28){TogglePane(true);return;}
            if(pointer.x>=width_-28){TogglePane(false);return;}
        }

        // An active layer drag keeps pointer ownership even if the cursor
        // crosses into the canvas. Commit once on release, never per frame.
        if (!layerDragId_.empty())
        {
            UpdateLayerDrag(pointer, holdingLeft);
            return;
        }

        if (panelDrag_ != PanelDrag::None)
        {
            if (!holdingLeft)
            {
                panelDrag_ = PanelDrag::None;
                NotifyPreferencesChanged();
            }
            else
            {
                const float previousLeft = panels_.left;
                const float previousRight = panels_.right;
                if (panelDrag_ == PanelDrag::Left)
                    panels_.DragLeft(pointer.x, width_);
                else panels_.DragRight(pointer.x, width_);
                if (std::abs(panels_.left - previousLeft) > 0.01f ||
                    std::abs(panels_.right - previousRight) > 0.01f)
                {
                    LayoutControls();
                    if (panelLayoutChanged_) panelLayoutChanged_();
                }
            }
            return;
        }

        if (!focusCanvas_ &&
            wi::input::Press(wi::input::MOUSE_BUTTON_LEFT) &&
            pointer.y >= HeaderHeight && pointer.y < height_ - FooterHeight)
        {
            const float leftDivider = panels_.left + PanelGap * 0.5f;
            const float rightDivider = width_ - panels_.right - PanelGap * 0.5f;
            if (!leftCollapsed_ && std::abs(pointer.x - leftDivider) <= 6.0f)
            {
                panelDrag_ = PanelDrag::Left;
                return;
            }
            if (!rightCollapsed_ && std::abs(pointer.x - rightDivider) <= 6.0f)
            {
                panelDrag_ = PanelDrag::Right;
                return;
            }
        }

        if (!session_ || !session_->IsLoaded()) return;
        if(directManipulationMode_!=DirectManipulationMode::None){
            if(wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE)){
                session_->CancelCoalescedEdit();
                timeline_=gestureTimeline_;timeline_.SyncMutation(session_->HistoryMutation());
                directManipulationMode_=DirectManipulationMode::None;directManipulationChanged_=false;RefreshInspector();if(documentChanged_)documentChanged_();return;
            }
            if(wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))UpdateDirectManipulation(pointer);else EndDirectManipulation();return;}
        const auto map = MinimapBounds();
        const auto minimapIntent = ResolveOverviewIntent(Contains(map, pointer),
            wi::input::Press(wi::input::MOUSE_BUTTON_LEFT), holdingLeft,
            wi::input::IsDoubleClicked(), minimapSuppressDrag_);
        if (Contains(map, pointer))
        {
            if (minimapIntent == OverviewIntent::Fit)
            {
                cameraPanning_ = false;
                ReturnToStoryFlow();
                return;
            }
            if (minimapIntent == OverviewIntent::Navigate)
            {
                const auto bounds = PreviewBounds();
                camera_.panX = bounds.width * 0.5f -
                    (pointer.x - map.x) / map.width * bounds.width * camera_.zoom;
                camera_.panY = bounds.height * 0.5f -
                    (pointer.y - map.y) / map.height * bounds.height * camera_.zoom;
                if (panelLayoutChanged_) panelLayoutChanged_();
            }
            return;
        }
        if (cameraPanning_) return;
        const auto& widgets = session_->Document().widgets;
        if (!focusCanvas_ && pointer.x < panels_.left &&
            pointer.y >= HeaderHeight &&
            pointer.y < height_ - FooterHeight)
        {
            HandleToolboxPointer(pointer);
            return;
        }
        const auto preview=PreviewBounds();if(!Contains(preview,pointer)||!renderer_||!wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))return;const auto handle=HitResizeHandle(pointer);if(handle!=DirectManipulationMode::None){BeginDirectManipulation(handle,pointer);return;}for(auto i=widgets.rbegin();i!=widgets.rend();++i){
            bool visible=i->visible;auto parent=i->parentId;
            std::size_t remaining=widgets.size();
            while(visible&&!parent.empty()&&remaining--){
                const auto* ancestor=session_->FindWidget(parent);
                visible=ancestor&&ancestor->visible;
                parent=ancestor?ancestor->parentId:bridge::StableId{};
            }
            if(!visible||!parent.empty())continue;
            const auto* b=renderer_->LogicalRect(i->id);if(b&&Contains(*b,pointer)){const bool extend=wi::input::Down(wi::input::KEYBOARD_BUTTON_LCONTROL)||wi::input::Down(wi::input::KEYBOARD_BUTTON_RCONTROL);
            if(extend){SelectWidget(i->id,true);return;}
            if(!selectedWidgetIds_.count(i->id))SelectWidget(i->id);
            else {selectedWidgetId_=i->id;RefreshInspector();}
            BeginDirectManipulation(DirectManipulationMode::Move,pointer);return;}}
    }

    void RenegadeScreenEditorWorkspace::Render(
        const wi::Canvas& canvas,
        const wi::graphics::CommandList cmd) const
    {
        if (!IsVisible()) return;
        ApplyScissor(canvas, scissorRect, cmd);

        const auto preview = PreviewBounds();
        Rect(0.0f, 0.0f, width_, HeaderHeight, Surface, cmd);
        if (!focusCanvas_)
        {
            Rect(0.0f, HeaderHeight, panels_.left, height_ - HeaderHeight,
                Surface, cmd);
            Rect(width_ - panels_.right, HeaderHeight, panels_.right,
                height_ - HeaderHeight, Surface, cmd);
        }
        Rect(0.0f, height_ - FooterHeight, width_, FooterHeight,
            Background, cmd);
        Outline(preview.x - 1.0f, preview.y - 1.0f,
            preview.width + 2.0f, preview.height + 2.0f, Border, 1.0f, cmd);

        // Slim drag handles between the authoring tree, canvas and Inspector.
        // Their hit areas are wider than their drawn lines for easier use.
        const float leftDivider = panels_.left + PanelGap * 0.5f;
        const float rightDivider = width_ - panels_.right - PanelGap * 0.5f;
        const wi::Color dividerColor = panelDrag_ == PanelDrag::None
            ? Border : Cyan;
        if (!focusCanvas_) Rect(leftDivider, HeaderHeight + 12.0f, 2.0f,
            height_ - HeaderHeight - FooterHeight - 24.0f, dividerColor, cmd);
        if (!focusCanvas_) Rect(rightDivider, HeaderHeight + 12.0f, 2.0f,
            height_ - HeaderHeight - FooterHeight - 24.0f, dividerColor, cmd);

        // The narrow header prioritises interactive controls over a redundant
        // title. On a wide desktop the title uses the spare right-side space.
        if (width_ >= 1130.0f)
            Label("SCREEN EDITOR", width_ - 320.0f, 18.0f, 18,
                Text, cmd);
        Label("DESIGN PREVIEW", preview.x, HeaderHeight - 20.0f,
            11, Muted, cmd);
        if (!focusCanvas_)
        {
            Label(leftCollapsed_?"":"UI LIBRARY", 34.0f, HeaderHeight + 7.0f, 10, Muted, cmd);
            Label(rightCollapsed_?"":"INSPECTOR", width_ - panels_.right + 14.0f,
                HeaderHeight + 16.0f, 13, Text, cmd);
        }

        if (session_ && session_->IsLoaded())
        {
            if (!focusCanvas_ && !leftCollapsed_) DrawToolbox(cmd);

            if (!focusCanvas_ && !rightCollapsed_ && !InventoryInspectorActive() && !inspectorSuppressed_) if (const auto* widget = session_->FindWidget(selectedWidgetId_))
            {
                const float inspectorX = width_ - panels_.right + 14.0f;
                if(!InventoryAvailable()) Label(widget->carousel ? "Carousel" : widget->video ? "Video" : bridge::ScreenWidgetKindName(widget->kind), inspectorX,
                    HeaderHeight + 42.0f, 10, Orange, cmd);
                Label("NAME", inspectorX, HeaderHeight + (widget->kind == bridge::ScreenWidgetKind::Image ? 121.0f : 57.0f), 9, Muted, cmd);
                if (widget->kind == bridge::ScreenWidgetKind::Progress || widget->kind == bridge::ScreenWidgetKind::Text || bridge::IsScreenInteractive(widget->kind))
                    Label(widget->kind == bridge::ScreenWidgetKind::Progress ? "FILL VALUE // 0 TO 1" : "DISPLAYED TEXT",
                        inspectorX, HeaderHeight + 117.0f, 9, Muted, cmd);
                Label("POSITION // X / Y (DESIGN PX)", inspectorX,
                    HeaderHeight + 177.0f, 9, Muted, cmd);
                Label("SIZE // WIDTH / HEIGHT (DESIGN PX)", inspectorX,
                    HeaderHeight + 229.0f, 9, Muted, cmd);
                Label(widget->layoutMode == bridge::ScreenLayoutMode::Anchored
                    ? "ANCHORED // ADVANCED LAYOUT"
                    : widget->layoutMode == bridge::ScreenLayoutMode::Manual
                        ? "MANUAL // DRAG OR EDIT NUMERIC VALUES"
                        : "ABSOLUTE // DRAG OR EDIT NUMERIC VALUES",
                    inspectorX, HeaderHeight + 285.0f, 9, Muted, cmd);
                if (widget->kind == bridge::ScreenWidgetKind::Progress)
                    Label("FILL DIRECTION", inspectorX,
                        HeaderHeight + 403.0f, 9, Muted, cmd);
                if (widget->kind == bridge::ScreenWidgetKind::Button)
                {
                    Label("DESTINATION", inspectorX,
                        HeaderHeight + 480.0f, 9, Muted, cmd);
                }
            }
        }

        const auto view = PreviewViewport();
        wi::graphics::Rect canvasClip{
            static_cast<int32_t>(std::ceil(preview.x)),
            static_cast<int32_t>(std::ceil(preview.y)),
            static_cast<int32_t>(std::floor(preview.x + preview.width)),
            static_cast<int32_t>(std::floor(preview.y + preview.height))};
        ApplyScissor(canvas, canvasClip, cmd, false);
        // Editor-only grid aligned with the shared Screen renderer's
        // FIT/FILL/STRETCH transform. Never modify the Screen document or
        // Runtime surfaces just to display alignment guides.
        if (gridVisible_ && session_ && session_->IsLoaded())
        {
            const auto& doc = session_->Document();
            bridge::ScreenCanvasTransform transform;
            std::string error;
            if (bridge::ResolveScreenCanvasTransform(
                    doc, view.width, view.height, transform, error))
            {
                const float originX = view.x + transform.offsetX;
                const float originY = view.y + transform.offsetY;
                const float stepX = static_cast<float>(gridSpacing_) * transform.scaleX;
                const float stepY = static_cast<float>(gridSpacing_) * transform.scaleY;
                // At small preview scales, skip lines too close to distinguish.
                // Snapping always retains the authored grid spacing.
                const int skipX = std::max(1, static_cast<int>(
                    std::ceil(10.0f / std::max(0.01f, stepX))));
                const int skipY = std::max(1, static_cast<int>(
                    std::ceil(10.0f / std::max(0.01f, stepY))));
                const auto drawAxis = [&](bool vertical, float origin, float step,
                                          float dimension, int skip)
                {
                    const float lower = vertical ? preview.x : preview.y;
                    const float upper = lower + (vertical ? preview.width : preview.height);
                    const float scaledMax = origin + dimension * step /
                        static_cast<float>(gridSpacing_);
                    const float visibleLower = std::max(lower, std::min(origin, scaledMax));
                    const float visibleUpper = std::min(upper, std::max(origin, scaledMax));
                    if (visibleUpper < visibleLower || step <= 0.0f) return;
                    const int first = std::max(0, static_cast<int>(
                        std::ceil((visibleLower - origin) / step)));
                    const int last = std::min(static_cast<int>(dimension /
                        static_cast<float>(gridSpacing_)), static_cast<int>(
                        std::floor((visibleUpper - origin) / step)));
                    for (int i = first; i <= last; ++i)
                    {
                        if (i % skip != 0) continue;
                        const float location = origin + static_cast<float>(i) * step;
                        const wi::Color ink = (i % (4 * skip) == 0)
                            ? wi::Color(100, 179, 196, 22)
                            : wi::Color(100, 179, 196, 10);
                        if (vertical) Rect(location, preview.y, 1.0f,
                            preview.height, ink, cmd);
                        else Rect(preview.x, location, preview.width,
                            1.0f, ink, cmd);
                    }
                };
                drawAxis(true, originX, stepX, doc.designWidth, skipX);
                drawAxis(false, originY, stepY, doc.designHeight, skipY);
            }
        }

        // Magnetic sibling alignment guides are presentation-only and never
        // serialized. Draw them over the existing authoring grid during drag.
        if ((alignmentGuideX_ || alignmentGuideY_) &&
            session_ && session_->IsLoaded())
        {
            bridge::ScreenCanvasTransform transform;
            std::string error;
            if (bridge::ResolveScreenCanvasTransform(
                    session_->Document(), view.width, view.height,
                    transform, error))
            {
                const wi::Color guideColor(255, 165, 72, 190);
                if (alignmentGuideX_)
                {
                    const float x = view.x + transform.offsetX +
                        alignmentGuideCoordinateX_ * transform.scaleX;
                    if (x >= preview.x && x <= preview.x + preview.width)
                        Rect(x, preview.y, 1.5f, preview.height,
                            guideColor, cmd);
                }
                if (alignmentGuideY_)
                {
                    const float y = view.y + transform.offsetY +
                        alignmentGuideCoordinateY_ * transform.scaleY;
                    if (y >= preview.y && y <= preview.y + preview.height)
                        Rect(preview.x, y, preview.width, 1.5f,
                            guideColor, cmd);
                }
            }
        }

        if (renderer_) for(const auto& selectedId:selectedWidgetIds_)
        {
            if (const auto* selected = renderer_->LogicalRect(selectedId))
            {
                Outline(selected->x - 2.0f, selected->y - 2.0f,
                    selected->width + 4.0f, selected->height + 4.0f,
                    Cyan, 2.0f, cmd);
      const float half = ResizeHandleSize * 0.5f;
      const float corners[][2] = {{selected->x, selected->y}, {selected->x + selected->width, selected->y}, {selected->x, selected->y + selected->height}, {selected->x + selected->width, selected->y + selected->height}};
      if(selectedWidgetIds_.size()==1) for (const auto& corner : corners) { Rect(corner[0]-half, corner[1]-half, ResizeHandleSize, ResizeHandleSize, Cyan, cmd); Outline(corner[0]-half, corner[1]-half, ResizeHandleSize, ResizeHandleSize, Text, 1.0f, cmd); }
            }
        }

        if(selectedWidgetIds_.size()>1){bridge::ScreenRect box;if(SelectionBounds(box,true)){
            Outline(box.x,box.y,box.width,box.height,Cyan,2,cmd);
            const float corners[][2]={{box.x,box.y},{box.x+box.width,box.y},{box.x,box.y+box.height},{box.x+box.width,box.y+box.height}};
            for(const auto& c:corners)Rect(c[0]-6,c[1]-6,12,12,Cyan,cmd);
        }}
        // Overview uses exactly the inverse camera transform, never saved geometry.
        if (session_ && session_->IsLoaded())
        {
            const auto map = MinimapBounds();
            Rect(map.x, map.y, map.width, map.height, Surface, cmd);
            for (const auto& widget : session_->Document().widgets)
            {
                if (!widget.visible) continue;
                bridge::ScreenRect r;
                std::string error;
                if (!bridge::ResolveScreenWidgetRect(session_->Document(), widget.id, r, error)) continue;
                const auto& doc = session_->Document();
                Rect(map.x + r.x / doc.designWidth * map.width,
                    map.y + r.y / doc.designHeight * map.height,
                    std::max(1.0f, r.width / doc.designWidth * map.width),
                    std::max(1.0f, r.height / doc.designHeight * map.height),
                    widget.id == selectedWidgetId_ ? Cyan : Muted, cmd);
            }
            const float vx = std::clamp(camera_.DesignX(0.0f) / preview.width, 0.0f, 1.0f);
            const float vy = std::clamp(camera_.DesignY(0.0f) / preview.height, 0.0f, 1.0f);
            const float vr = std::clamp(camera_.DesignX(preview.width) / preview.width, 0.0f, 1.0f);
            const float vb = std::clamp(camera_.DesignY(preview.height) / preview.height, 0.0f, 1.0f);
            Outline(map.x + vx * map.width, map.y + vy * map.height,
                (vr - vx) * map.width, (vb - vy) * map.height, Orange, 1.5f, cmd);
            Outline(map.x, map.y, map.width, map.height, Border, 1.0f, cmd);
            if (Contains(map, wi::input::GetPointer()))
            {
                Rect(map.x - 18, map.y - 25, map.width + 18, 23, Surface, cmd);
                Label("Double-click: StoryFlow", map.x - 12, map.y - 20, 10, Text, cmd);
            }
        }
        ApplyScissor(canvas, scissorRect, cmd);
        Label("AUTOSAVE ON", 14.0f, height_ - FooterHeight + 14.0f,
            11, Muted, cmd);
        // Reserve an uncluttered footer lane for the four layer buttons;
        // show status to their right instead of drawing text underneath them.
        Label(Shorten(status_, static_cast<std::size_t>(std::max(
                16.0f, (width_ - (preview.x + 392.0f) - 12.0f) / 6.0f))),
            preview.x + 392.0f, height_ - FooterHeight + 14.0f,
            11, HasPendingSave() ? Orange : Muted, cmd);
        RenderControls(canvas, cmd);
        if (stateColour_.IsVisible() && session_)
        {
            if (const auto* widget = session_->FindWidget(selectedWidgetId_))
            {
                const auto& state = basicState_==0 ? widget->style.normal : basicState_==1 ? widget->style.hover : widget->style.pressed;
                const auto c = state.imageResourcePath.empty() && widget->resourcePath.empty() ? state.background : state.imageTint;
                const auto p = stateColour_.GetPos();
                const float y = p.y+(stateColour_.GetSize().y-14)*0.5f;
                Rect(p.x+6,y,14,14,wi::Color(190,190,190,255),cmd);
                Rect(p.x+6,y,7,14,wi::Color(80,80,80,255),cmd);
                Rect(p.x+6,y,14,14,wi::Color(c.red,c.green,c.blue,c.alpha),cmd);
            }
        }
        if(inventoryButton_.IsVisible()){ApplyScissor(canvas,scissorRect,cmd);inventoryButton_.Render(canvas,cmd);}
        if(InventoryInspectorActive())inventoryInspector_.Render(canvas,cmd);
        ApplyScissor(canvas,scissorRect,cmd);
        // Explicit overlay order inside the same GUI render pass. GUI input
        // priority sorting must never put a full-canvas workspace above its Inspector.
        if (inspectorOverlay_) inspectorOverlay_(canvas, cmd);
        if(!focusCanvas_){
            Rect(0,HeaderHeight+4,28,24,Border,cmd);Label(leftCollapsed_?">":"<",8,HeaderHeight+8,14,Text,cmd);
            Rect(width_-28,HeaderHeight+4,28,24,Border,cmd);Label(rightCollapsed_?"<":">",width_-20,HeaderHeight+8,14,Text,cmd);
        }

        // Toolbar icons need their help after every ordinary control,
        // Inspector and collapse handle, while child pickers remain topmost.
        for (const auto* button : {&returnButton_, &saveButton_, &undoButton_,
                &redoButton_, &focusButton_, &fitCanvasButton_})
        {
            ApplyScissor(canvas, scissorRect, cmd);
            button->RenderTooltip(canvas, cmd);
        }
        ApplyScissor(canvas, scissorRect, cmd);
        screenSettingsPicker_.Render(cmd);
        basicActionPicker_.Render(cmd);
        destinationPicker_.Render(cmd);
        if(carouselEditor_&&carouselEditor_->IsVisible())carouselEditor_->Render(canvas,cmd);
        if(liquidEditor_&&liquidEditor_->IsVisible())liquidEditor_->Render(canvas,cmd);
        if(navigationEditor_&&navigationEditor_->IsVisible())navigationEditor_->Render(canvas,cmd);
        if(minimapEditor_&&minimapEditor_->IsVisible())minimapEditor_->Render(canvas,cmd);
    }
}


namespace
{
    struct ScreenPaletteRow
    {
        const renegade::studio::ScreenToolboxEntry* item = nullptr;
        std::string category;
        const renegade::studio::ScreenEditorLibraryScreen* screen = nullptr;
    };

    std::string LowercasePalette(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        return value;
    }

    std::string PaletteCategoryKey(
        const renegade::studio::ScreenToolboxTab tab,
        const std::string& category)
    {
        return std::to_string(static_cast<int>(tab)) + ":" + category;
    }

    std::vector<ScreenPaletteRow> VisibleScreenPalette(
        const renegade::studio::ScreenToolboxTab tab,
        const std::string& filter,
        const std::vector<renegade::studio::ScreenEditorLibraryScreen>& screens,
        const std::unordered_set<std::string>& collapsed)
    {
        const std::string needle = LowercasePalette(filter);
        std::vector<ScreenPaletteRow> result;
        if (tab == renegade::studio::ScreenToolboxTab::Screens)
        {
            const std::vector<std::string> categories={"MENUS","HUDS","OVERLAYS","MODALS","UNAVAILABLE","PROJECT SCREENS"};
            bool any=false;
            for(const auto& category:categories)
            {
                bool header=false;
                const bool expanded=!needle.empty() || collapsed.count(PaletteCategoryKey(tab,category))==0;
                for(const auto& screen:screens)
                {
                    if(screen.category!=category || (!needle.empty() &&
                        LowercasePalette(screen.name+" "+category).find(needle)==std::string::npos))continue;
                    if(!header){result.push_back({nullptr,category,nullptr});header=true;any=true;}
                    if(expanded)result.push_back({nullptr,{},&screen});
                }
            }
            if(!any && needle.empty())result.push_back({nullptr,"PROJECT SCREENS",nullptr});
        }
        std::string group;
        for (const auto& item : renegade::studio::ScreenToolboxEntries(tab))
        {
            // Never offer a clickable-looking creator tool without a real
            // authoring operation and Runtime support. Full catalogue remains
            // documented separately until individual tools are implemented.
            if (item.action == renegade::studio::ScreenToolboxAction::None)
                continue;
            const std::string label(item.label);
            const std::string category(item.category);
            if (!needle.empty() &&
                LowercasePalette(label).find(needle) == std::string::npos &&
                LowercasePalette(category).find(needle) == std::string::npos)
                continue;
            if (category != group)
            {
                group = category;
                result.push_back({nullptr, group});
            }
            if (!needle.empty() ||
                collapsed.count(PaletteCategoryKey(tab, category)) == 0)
                result.push_back({&item, {}});
        }
        if (result.empty())
            result.push_back({nullptr,
                needle.empty()
                    ? "NO IMPLEMENTED TOOLS IN THIS TAB"
                    : "NO MATCHING IMPLEMENTED TOOLS"});
        return result;
    }
}

namespace renegade::studio
{
    float RenegadeScreenEditorWorkspace::LayersTop() const noexcept
    {
        const float lower = HeaderHeight + 220.0f;
        const float upper = height_ - FooterHeight - 164.0f;
        return std::max(lower, std::min(height_ * 0.62f, upper));
    }

    void RenegadeScreenEditorWorkspace::CreatePalettePreset(const int preset)
    {
        if(!session_||!session_->IsLoaded())return;
        auto widget=bridge::MakeScreenWidgetPreset(static_cast<bridge::ScreenWidgetPreset>(preset),
            session_->Document().designWidth,session_->Document().designHeight);
        const auto name=widget.name;
        bridge::StableId created;std::string error;
        if(!session_->CreateWidget(std::move(widget),false,created,error))
        {SetStatus("PRESET REJECTED // "+error);return;}
        selectedWidgetIds_.clear();selectedWidgetIds_.insert(created);selectedWidgetId_=created;
        RefreshAfterMutation(name+" CREATED // EDIT CONTENT, STYLE AND BINDING IN INSPECTOR");
    }

    void RenegadeScreenEditorWorkspace::HandleToolboxPointer(
        const XMFLOAT4& pointer)
    {
        const float layersTop = LayersTop();
        const bool pressed = wi::input::Press(
            wi::input::MOUSE_BUTTON_LEFT);
        if (pointer.y < layersTop)
        {
            const float tabTop = HeaderHeight + 24.0f;
            const float tabsWidth = (panels_.left - 20.0f) * 0.25f;
            if (pressed && pointer.y >= tabTop &&
                pointer.y < tabTop + 30.0f &&
                pointer.x >= 10.0f && pointer.x < panels_.left - 10.0f)
            {
                const auto index = std::clamp(
                    static_cast<int>((pointer.x - 10.0f) / tabsWidth), 0, 3);
                toolboxTab_ = static_cast<ScreenToolboxTab>(index);
                toolboxScroll_ = 0;
                NotifyPreferencesChanged();
                toolboxSearchInput_.SetValue("");
                toolboxFilter_.clear();
                return;
            }
            const float listTop = HeaderHeight + 120.0f;
            if (pointer.y < listTop) return;
            const auto rows = VisibleScreenPalette(
                toolboxTab_, toolboxFilter_, projectScreens_,
                collapsedCategories_);
            const auto visible = static_cast<std::size_t>(std::max(
                1.0f, std::floor((layersTop - listTop - 8.0f) / 26.0f)));
            const std::size_t maxScroll =
                rows.size() > visible ? rows.size() - visible : 0;
            if (pointer.z < 0.0f)
                toolboxScroll_ = std::min(toolboxScroll_ + 1, maxScroll);
            else if (pointer.z > 0.0f && toolboxScroll_ > 0)
                --toolboxScroll_;
            toolboxScroll_ = std::min(toolboxScroll_, maxScroll);
            if (!pressed) return;
            const auto row = static_cast<std::size_t>(
                (pointer.y - listTop) / 26.0f);
            if (row >= visible || toolboxScroll_ + row >= rows.size()) return;
            const auto& choice = rows[toolboxScroll_ + row];
            if (choice.screen)
            {
                if(!choice.screen->available)
                    SetStatus("SCREEN UNAVAILABLE // "+choice.screen->diagnostic);
                else if (choice.screen->current)
                    SetStatus("CURRENT SCREEN // ALREADY OPEN");
                else if(SavePendingBeforeNavigation())
                {
                    const auto target=*choice.screen;
                    if(!target.nodeId.empty() && openProjectScreen_)openProjectScreen_(target.nodeId);
                    else if(openLibraryDocument_)openLibraryDocument_(target.documentId);
                }
                return;
            }
            if (!choice.item && !choice.screen)
            {
                const std::string key =
                    PaletteCategoryKey(toolboxTab_, choice.category);
                if (collapsedCategories_.count(key))
                    collapsedCategories_.erase(key);
                else collapsedCategories_.insert(key);
                toolboxScroll_ = 0;
                return;
            }
            const auto* item = choice.item;
            if (!item) return;
            switch (item->action)
            {
            case ScreenToolboxAction::SaveUiPrefab: SaveSelectedUiPrefab();break;
            case ScreenToolboxAction::InsertUiPrefab: ChooseUiPrefab();break;
            case ScreenToolboxAction::LocationAnnouncement:
            {
                auto widget=bridge::MakeLocationAnnouncementWidget();bridge::StableId created;std::string error;
                widget.rect.x=session_->Document().designWidth*.125f;widget.rect.y=session_->Document().designHeight*.21f;
                widget.rect.width=session_->Document().designWidth*.75f;
                if(!session_->CreateWidget(std::move(widget),false,created,error)){SetStatus(error);break;}
                SelectWidget(created);RefreshAfterMutation("LOCATION NAME CREATED // EDIT FONT, COLOUR AND POSITION");break;
            }
            case ScreenToolboxAction::WorldNavigation:
                InsertUiPrefab((std::filesystem::u8path(wi::helper::GetExecutablePath()).parent_path()/"Content/UI/Prefabs/WorldNavigation").generic_u8string(),true);break;
            case ScreenToolboxAction::LiquidFill:
                InsertUiPrefab((std::filesystem::u8path(wi::helper::GetExecutablePath()).parent_path()/"Content/UI/Prefabs/LiquidFill").generic_u8string());break;
            case ScreenToolboxAction::Minimap:
            {
                auto widget=bridge::MakeWorldMinimapWidget();bridge::StableId created;std::string error;
                widget.rect.x=std::max(12.f,session_->Document().designWidth-304.f);
                if(!session_->CreateWidget(std::move(widget),false,created,error)){SetStatus(error);break;}
                SelectWidget(created);RefreshAfterMutation("MINIMAP CREATED // STYLE / FONT OPENS MAP SETTINGS");break;
            }
            case ScreenToolboxAction::AmmoCounter:
            {
                std::vector<bridge::StableId> created;std::string error;
                if(!bridge::InsertAmmoCounterPrefab(*session_,session_->Document().designWidth*.6f,
                    session_->Document().designHeight*.7f,created,error)){SetStatus(error);break;}
                if(!created.empty())SelectWidget(created.front());
                RefreshAfterMutation("AMMO COUNTER CREATED // FOLLOWS EQUIPPED WEAPON IN SLOTS 1-9");break;
            }
            case ScreenToolboxAction::AnimatedVial:
                InsertUiPrefab((std::filesystem::u8path(wi::helper::GetExecutablePath()).parent_path()/"Content/UI/Prefabs/AnimatedVial").generic_u8string());break;
            case ScreenToolboxAction::UiBudgets: if(uiBudgetsRequested_)uiBudgetsRequested_();break;
            case ScreenToolboxAction::PublishUiPackage: if(portableLibraryRequested_)portableLibraryRequested_(true);break;
            case ScreenToolboxAction::AdoptUiPackage: if(portableLibraryRequested_)portableLibraryRequested_(false);break;
            case ScreenToolboxAction::StoryFlow: ReturnToStoryFlow(); break;
            case ScreenToolboxAction::GlobalHud: OpenProjectHudPicker(); break;
            case ScreenToolboxAction::ProjectInventory: OpenProjectInventoryPicker(); break;
            case ScreenToolboxAction::GameplayAttributes: if(gameplayAttributesRequested_)gameplayAttributesRequested_();break;
            case ScreenToolboxAction::Progress:
                CreateElement(bridge::ScreenWidgetKind::Progress, false); break;
            case ScreenToolboxAction::Text:
                CreateElement(bridge::ScreenWidgetKind::Text, false); break;
            case ScreenToolboxAction::Carousel:
            case ScreenToolboxAction::Video:
            case ScreenToolboxAction::Button:
                paletteCarousel_=item->action==ScreenToolboxAction::Carousel;
                paletteVideo_=item->action==ScreenToolboxAction::Video;
                paletteButtonPending_ = true;
                palettePress_ = pointer;
                break;
            case ScreenToolboxAction::Image:
                CreateElement(bridge::ScreenWidgetKind::Image, false); break;
            case ScreenToolboxAction::Background:
                CreateElement(bridge::ScreenWidgetKind::Image, true); break;
            case ScreenToolboxAction::Panel: CreatePalettePreset(false); break;
            case ScreenToolboxAction::Heading: CreatePalettePreset(true); break;
            case ScreenToolboxAction::Icon: CreatePalettePreset(2); break;
            case ScreenToolboxAction::Divider: CreatePalettePreset(3); break;
            case ScreenToolboxAction::Spacer: CreatePalettePreset(4); break;
            case ScreenToolboxAction::Counter: CreatePalettePreset(5); break;
            case ScreenToolboxAction::PlayerHealthBar: CreatePalettePreset(6); break;
            case ScreenToolboxAction::PlayerHealthText: CreatePalettePreset(7); break;
            case ScreenToolboxAction::PlayerAmmoText: CreatePalettePreset(8); break;
            case ScreenToolboxAction::Group: CreatePalettePreset(9); break;
            case ScreenToolboxAction::HorizontalStack: CreatePalettePreset(10); break;
            case ScreenToolboxAction::VerticalStack: CreatePalettePreset(11); break;
            case ScreenToolboxAction::LayoutGrid: CreatePalettePreset(12); break;
            case ScreenToolboxAction::Toggle: CreatePalettePreset(13); break;
            case ScreenToolboxAction::Slider: CreatePalettePreset(14); break;
            case ScreenToolboxAction::Selector: CreatePalettePreset(15); break;
            case ScreenToolboxAction::TextInput: CreatePalettePreset(16); break;
            case ScreenToolboxAction::Grid:
                gridVisible_ = !gridVisible_;
            if(!SaveProjectGrid())return;
                NotifyPreferencesChanged();
                gridVisibleButton_.SetText(gridVisible_ ? "GRID ON" : "GRID OFF");
                SetStatus(gridVisible_ ? "GRID VISIBLE" : "GRID HIDDEN");
                break;
            case ScreenToolboxAction::Snap:
                gridSnapEnabled_ = !gridSnapEnabled_;
            if(!SaveProjectGrid())return;
                NotifyPreferencesChanged();
                gridSnapButton_.SetText(gridSnapEnabled_ ? "SNAP ON" : "SNAP OFF");
                SetStatus(gridSnapEnabled_ ? "SNAP ON" : "SNAP OFF");
                break;
            default:
                SetStatus("NOT YET SUPPORTED // " + std::string(item->label));
                break;
            }
            return;
        }

        const auto& widgets = session_->Document().widgets;
        if (pressed && pointer.y >= layersTop + 37.0f &&
            pointer.y < layersTop + 63.0f)
        {
            if (pointer.x < panels_.left * 0.5f)
                DuplicateSelectedWidget();
            else DeleteSelectedWidget();
            return;
        }

        const float listTop = layersTop + 77.0f;
        if (pointer.y < listTop) return;
        const auto visible = static_cast<std::size_t>(std::max(1.0f,
            std::floor((height_ - FooterHeight - listTop - 3.0f) / 26.0f)));
        const std::size_t maxScroll =
            widgets.size() > visible ? widgets.size() - visible : 0;
        if (pointer.z < 0.0f)
            hierarchyScroll_ = std::min(hierarchyScroll_ + 1, maxScroll);
        else if (pointer.z > 0.0f && hierarchyScroll_ > 0)
            --hierarchyScroll_;
        hierarchyScroll_ = std::min(hierarchyScroll_, maxScroll);
        if (!pressed) return;
        const auto row = static_cast<std::size_t>(
            (pointer.y - listTop) / 26.0f);
        if (row < visible && hierarchyScroll_ + row < widgets.size())
        {
            const bridge::StableId id = widgets[hierarchyScroll_ + row].id;
            const bool extend=wi::input::Down(wi::input::KEYBOARD_BUTTON_LCONTROL)||wi::input::Down(wi::input::KEYBOARD_BUTTON_RCONTROL);
            if(extend){SelectWidget(id,true);return;}
            if(!selectedWidgetIds_.count(id))SelectWidget(id);
            else {selectedWidgetId_=id;RefreshInspector();}
            layerDragId_ = id;
            layerDragStart_ = {pointer.x, pointer.y};
            layerDragActive_ = false;
            layerDropTargetId_.clear();
        }
    }

    void RenegadeScreenEditorWorkspace::UpdateLayerDrag(
        const XMFLOAT4& pointer, const bool holdingLeft)
    {
        if (!session_ || !session_->IsLoaded())
        {
            layerDragId_.clear();
            layerDropTargetId_.clear();
            layerDragActive_ = false;
            return;
        }
        if (holdingLeft)
        {
            const float dx = pointer.x - layerDragStart_.x;
            const float dy = pointer.y - layerDragStart_.y;
            if (!layerDragActive_ && dx * dx + dy * dy > 36.0f)
                layerDragActive_ = true;
            layerDropTargetId_.clear();
            if (!layerDragActive_) return;
            const float listTop = LayersTop() + 77.0f;
            if (pointer.x < 0.0f || pointer.x >= panels_.left ||
                pointer.y < listTop ||
                pointer.y >= height_ - FooterHeight) return;
            const auto& widgets = session_->Document().widgets;
            const std::size_t row = static_cast<std::size_t>(
                (pointer.y - listTop) / 26.0f);
            const std::size_t index = hierarchyScroll_ + row;
            if (index >= widgets.size()) return;
            const auto& destination = widgets[index];
            const auto* source = session_->FindWidget(layerDragId_);
            if (!source || selectedWidgetIds_.count(destination.id) ||
                source->parentId != destination.parentId) return;
            layerDropTargetId_ = destination.id;
            layerDropAfter_ = pointer.y - (listTop + row * 26.0f) > 13.0f;
            return;
        }

        const bridge::StableId source = std::move(layerDragId_);
        const bridge::StableId destination = std::move(layerDropTargetId_);
        const bool shouldCommit = layerDragActive_ && !destination.empty();
        const bool after = layerDropAfter_;
        layerDragId_.clear();
        layerDropTargetId_.clear();
        layerDragActive_ = false;
        if (!shouldCommit) return; // Click-selection is not a mutation.
        std::string error;
        if (!session_->MoveWidgetsAdjacentTo(SelectedIds(), destination, after, error))
        {
            SetStatus("LAYER DRAG REJECTED // " + error);
            return;
        }
        RefreshAfterMutation("LAYER REORDERED // ONE UNDOABLE ACTION");
    }

    void RenegadeScreenEditorWorkspace::DrawToolbox(
        const wi::graphics::CommandList cmd) const
    {
        const float left = panels_.left;
        const float layersTop = LayersTop();
        constexpr const char* tabs[4] =
            {"SCREENS", "GAMEPLAY", "ELEMENTS", "GLOBALS"};
        const float tabWidth = (left - 20.0f) * 0.25f;
        for (int i = 0; i < 4; ++i)
        {
            const float x = 10.0f + i * tabWidth;
            const bool active = toolboxTab_ ==
                static_cast<ScreenToolboxTab>(i);
            Rect(x, HeaderHeight + 24.0f, tabWidth - 3.0f, 30.0f,
                active ? Raised : Surface, cmd);
            if (active)
                Rect(x, HeaderHeight + 51.0f, tabWidth - 3.0f,
                    2.0f, Cyan, cmd);
            Label(tabs[i], x + 4.0f, HeaderHeight + 33.0f,
                left < 280.0f ? 8 : 9,
                active ? Text : Muted, cmd);
        }
        Label("SEARCH", 12.0f, HeaderHeight + 63.0f, 9, Muted, cmd);
        Rect(12.0f, HeaderHeight + 76.0f,
            left - 24.0f, 28.0f, Raised, cmd);
        const float listTop = HeaderHeight + 120.0f;
        const auto rows = VisibleScreenPalette(
            toolboxTab_, toolboxFilter_, projectScreens_,
                collapsedCategories_);
        const auto count = static_cast<std::size_t>(std::max(
            1.0f, std::floor((layersTop - listTop - 8.0f) / 26.0f)));
        if (rows.empty())
            Label("NO MATCHES", 17.0f, listTop + 8.0f, 10, Muted, cmd);
        for (std::size_t row = 0; row < count; ++row)
        {
            const std::size_t index = toolboxScroll_ + row;
            if (index >= rows.size()) break;
            const float y = listTop + row * 26.0f;
            if (y + 26.0f > layersTop - 5.0f) break;
            const auto& paletteRow = rows[index];
            if (paletteRow.screen)
            {
                const bool current = paletteRow.screen->current;
                Rect(10.0f, y + 1.0f, left - 20.0f, 24.0f,
                    current ? wi::Color(13, 35, 45, 255) : Raised, cmd);
                Label(Shorten(paletteRow.screen->name,
                        left < 300.0f ? 20 : 28),
                    18.0f, y + 6.0f, 10,
                    current ? Text : Muted, cmd);
                Label(current ? "OPEN" : ">", left - 47.0f,
                    y + 6.0f, 9, current ? Cyan : Text, cmd);
                continue;
            }
            if (!paletteRow.item)
            {
                const bool closed = collapsedCategories_.count(
                    PaletteCategoryKey(toolboxTab_, paletteRow.category)) != 0 &&
                    toolboxFilter_.empty();
                Label(closed ? ">" : "v", 15.0f, y + 6.0f,
                    10, Cyan, cmd);
                Label(Shorten(paletteRow.category, left < 280.0f ? 21 : 30),
                    29.0f, y + 6.0f, 9, Cyan, cmd);
                continue;
            }
            const bool available = paletteRow.item->action !=
                ScreenToolboxAction::None;
            Rect(10.0f, y + 1.0f, left - 20.0f, 24.0f,
                available ? Raised : Surface, cmd);
            Label(Shorten(std::string(paletteRow.item->label),
                    left < 300.0f ? 23 : 30),
                18.0f, y + 6.0f, 10,
                available ? Text : Muted, cmd);
            if (available)
                Label("+", left - 25.0f, y + 5.0f,
                    13, Cyan, cmd);
        }

        Rect(0.0f, layersTop, left, 1.0f, Border, cmd);
        Label("LAYERS", 14.0f, layersTop + 12.0f, 12, Text, cmd);
        Label("CURRENT SCREEN", left - 113.0f,
            layersTop + 14.0f, 9, Muted, cmd);
        const auto& widgets = session_->Document().widgets;
        const bool hasSelection =
            session_->FindWidget(selectedWidgetId_) != nullptr;
        const float half = left * 0.5f - 16.0f;
        Rect(12.0f, layersTop + 37.0f, half,
            26.0f, hasSelection ? Raised : Surface, cmd);
        Rect(left * 0.5f + 4.0f, layersTop + 37.0f, half,
            26.0f, hasSelection ? Raised : Surface, cmd);
        Label("DUPLICATE", 21.0f, layersTop + 45.0f,
            9, hasSelection ? Text : Muted, cmd);
        Label("DELETE", left * 0.5f + 15.0f,
            layersTop + 45.0f, 9, hasSelection ? Text : Muted, cmd);

        const float layerList = layersTop + 77.0f;
        const auto visible = static_cast<std::size_t>(std::max(
            1.0f, std::floor((height_ - FooterHeight - layerList -
                3.0f) / 26.0f)));
        for (std::size_t row = 0; row < visible; ++row)
        {
            const auto index = hierarchyScroll_ + row;
            if (index >= widgets.size()) break;
            const auto& widget = widgets[index];
            const float y = layerList + row * 26.0f;
            if (y + 26.0f >= height_ - FooterHeight) break;
            if (selectedWidgetIds_.count(widget.id))
                Rect(9.0f, y + 1.0f, left - 18.0f,
                    24.0f, Raised, cmd);
            if (layerDragActive_ && widget.id == layerDropTargetId_)
                Rect(12.0f, y + (layerDropAfter_ ? 24.0f : 0.0f),
                    left - 24.0f, 2.0f, Cyan, cmd);
            Label(widget.parentId.empty() ? "o" : "  >",
                13.0f, y + 6.0f, 9, Muted, cmd);
            Label(Shorten(widget.name, left < 280.0f ? 17 : 24),
                38.0f, y + 6.0f, 10,
                selectedWidgetIds_.count(widget.id) ? Text : Muted, cmd);
            Label(widget.carousel ? "Carousel" : widget.video ? "Video" : bridge::ScreenWidgetKindName(widget.kind),
                left - 64.0f, y + 7.0f, 8, Muted, cmd);
        }
    }
}


namespace renegade::studio
{
    void RenegadeScreenEditorWorkspace::OpenBasicActionPicker()
    {
        const auto* selected = session_
            ? session_->FindWidget(selectedWidgetId_) : nullptr;
        if (!selected || selected->kind != bridge::ScreenWidgetKind::Button)
            return;
        std::vector<RenegadeScreenChoicePicker::Choice> choices;
        for (const auto& action : session_->Document().actions)
            choices.push_back({action.id, action.id});
        const bridge::StableId selectedId = selected->id;
        basicActionPicker_.Open(
            "ON CLICK // SELECT ACTION", std::move(choices),
            selected->actionId,
            [this, selectedId](const std::string& value)
            {
                const auto* widget = session_
                    ? session_->FindWidget(selectedId) : nullptr;
                if (!widget || widget->kind !=
                        bridge::ScreenWidgetKind::Button)
                {
                    SetStatus("BUTTON ACTION REJECTED // SELECTION CHANGED");
                    return;
                }
                bridge::ScreenWidgetCreatorEdit edit;
                edit.resourcePath = widget->resourcePath;
                edit.actionId = value;
                edit.parentId = widget->parentId;
                edit.layoutMode = widget->layoutMode;
                edit.anchors = widget->anchors;
                edit.style = widget->style;
                std::string error;
                if (!session_->UpdateWidgetCreatorFields(
                        selectedId, std::move(edit), error))
                {
                    SetStatus("BUTTON ACTION REJECTED // " + error);
                    return;
                }
                RefreshAfterMutation(
                    "BUTTON ACTION UPDATED // DESTINATION OWNED BY STORYFLOW");
            });
    }
}


namespace renegade::studio
{
    void RenegadeScreenEditorWorkspace::UpdateClickSound(const std::string& command,const std::string& clip)
    {
        if(!session_ || gridProjectRoot_.empty()) return;
        const auto* button=session_->FindWidget(selectedWidgetId_);
        if(!button || button->kind!=bridge::ScreenWidgetKind::Button) return;
        const bridge::ScreenClickSounds::Key key={session_->Document().envelope.documentId,button->id};
        auto& current=bridge::ProjectClickSounds(gridProjectRoot_);
        std::string error;
        if(command=="preview")
        {
            if(!bridge::PlayScreenClick(gridProjectRoot_,current.Resolve(key),error)) SetStatus(error);
            return;
        }
        auto next=bridge::ScreenClickSounds::Read(gridProjectRoot_);
        if(command=="promote")
        {
            auto path=next.Resolve(key);
            if(path.empty()) return;
            next.projectDefault=path;
            next.buttons.erase(key);
        }
        else if(command=="default") next.buttons.erase(key);
        else if(command=="silent") next.buttons[key]="";
        else if(command=="local")
        {
            std::string absolute;
            if(!bridge::ResolveScreenResourcePath(gridProjectRoot_,clip,absolute,error) ||
                !bridge::ValidateAudioAssetForWicked(absolute,error)) { SetStatus(error);return; }
            next.buttons[key]=clip;
        }
        else return;
        if(!next.Save(gridProjectRoot_,error)) {SetStatus(error);return;}
        current=std::move(next);
        SetStatus(command=="promote" ? "PROJECT DEFAULT CLICK SOUND SAVED" : "BUTTON CLICK SOUND SAVED");
    }

    void RenegadeScreenEditorWorkspace::ChooseClickSound()
    {
        if(!session_ || gridProjectRoot_.empty()) return;
        const auto root=gridProjectRoot_;
        const auto doc=session_->Document().envelope.documentId;
        const auto id=selectedWidgetId_;
        const auto stillSelected=[this,root,doc,id]{
            return session_ && session_->IsLoaded() && gridProjectRoot_==root &&
                session_->Document().envelope.documentId==doc && selectedWidgetId_==id;
        };
        const auto& sounds=bridge::ProjectClickSounds(root);
        const auto it=sounds.buttons.find({doc,id});
        const auto current=it==sounds.buttons.end() ? "@default" : it->second.empty() ? "@silent" : it->second;
        std::vector<RenegadeScreenChoicePicker::Choice> choices={
            {"Use project default","@default"},{"Silent","@silent"},{"Import sound...","@import"}};
        namespace fs=std::filesystem;
        std::error_code ec;
        const auto base=fs::u8path(root);
        for(fs::recursive_directory_iterator files(base/"Content/Audio",fs::directory_options::skip_permission_denied,ec),end;
            !ec && files!=end;files.increment(ec))
        {
            const auto& file=files->path();
            auto ext=file.extension().u8string();
            std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});
            if((ext==".wav" || ext==".ogg") && files->is_regular_file(ec))
                choices.push_back({file.filename().u8string(),
                    file.lexically_relative(base).generic_u8string()});
        }
        std::sort(choices.begin()+3,choices.end(),[](const auto& a,const auto& b){return a.label<b.label;});
        destinationPicker_.Open("CLICK SOUND",std::move(choices),current,
            [this,root,stillSelected](const std::string& value){
                if(!stillSelected())return;
                if(value=="@default" || value=="@silent"){UpdateClickSound(value.substr(1));return;}
                if(value!="@import"){UpdateClickSound("local",value);return;}
                wi::helper::FileDialogParams dialog;
                dialog.type=wi::helper::FileDialogParams::OPEN;
                dialog.description="Import button click sound";
                dialog.extensions={"wav","ogg"};
                wi::helper::FileDialog(dialog,[this,root,stillSelected](const std::string& path){
                    if(path.empty())return;
                    wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                        [this,root,stillSelected,path](std::uint64_t){
                            if(!stillSelected()) return;
                            std::string clip,error;
                            if(bridge::ImportUiClickSound(root,path,clip,error)) UpdateClickSound("local",clip);
                            else SetStatus(error);
                        });
                });
            });
    }

    void RenegadeScreenEditorWorkspace::OpenDestinationPicker()
    {
        if (!session_ || !session_->IsLoaded() || !openScreenRoute_)
            return;
        const auto* button = session_->FindWidget(selectedWidgetId_);
        if (!button || button->kind != bridge::ScreenWidgetKind::Button)
            return;

        if (routeContext_.empty())
        {
            std::vector<RenegadeScreenChoicePicker::Choice> contexts;
            for (const auto& entry : projectScreens_)
                if (entry.documentId == session_->Document().envelope.documentId && !entry.nodeId.empty() && entry.available)
                    contexts.push_back({entry.name + " // " + entry.nodeId, entry.nodeId});
            if (contexts.empty()) { SetStatus("ROUTES DISABLED // STANDALONE SCREEN HAS NO FLOW CONTEXT"); return; }
            const auto project = session_->ProjectId(), document = session_->Document().envelope.documentId, selectedButton=button->id;
            destinationPicker_.Open("CHOOSE FLOW CONTEXT", std::move(contexts), {},
                [this, project, document, selectedButton](const std::string& id) {
                    if (!session_ || session_->ProjectId()!=project || session_->Document().envelope.documentId!=document ||
                        selectedWidgetId_!=selectedButton || !chooseRouteContext_ || !chooseRouteContext_(id)) return;
                    routeContext_=id; OpenDestinationPicker();
                });
            return;
        }
        std::vector<RenegadeScreenChoicePicker::Choice> choices;
        choices.reserve(projectScreens_.size());
        for (const auto& choice : ScreenDestinationChoices(projectScreens_))
            choices.push_back({choice.first, choice.second});
        if (choices.empty())
        {
            SetStatus("OPEN SCREEN REJECTED // NO STORYFLOW SCREENS");
            return;
        }
        basicActionPicker_.Close();
        const bridge::StableId buttonId = button->id;
        const auto documentId = session_->Document().envelope.documentId;
        const auto projectId = session_->ProjectId();
        const auto contextId = routeContext_;
        destinationPicker_.Open(
            "ON CLICK // OPEN SCREEN", std::move(choices), {},
            [this, buttonId, documentId, projectId, contextId](const std::string& destinationId)
            {
                if (!session_ || !session_->IsLoaded() ||
                    !session_->FindWidget(buttonId) || !openScreenRoute_ ||
                    session_->Document().envelope.documentId != documentId || session_->ProjectId() != projectId ||
                    selectedWidgetId_ != buttonId || routeContext_ != contextId)
                {
                    SetStatus("ROUTE REJECTED // BUTTON IS NO LONGER AVAILABLE");
                    return;
                }
                const auto target=std::find_if(projectScreens_.begin(),projectScreens_.end(),[&](const auto& entry){return entry.nodeId==destinationId && entry.available && entry.enabledInGame && !entry.current;});
                if(target==projectScreens_.end()){SetStatus("ROUTE REJECTED // DESTINATION IS NO LONGER ELIGIBLE");return;}
                // New Buttons own new outcomes. Persist the action first,
                // using the existing atomic Screen/Flow save, so the route
                // coordinator can verify the actual Runtime Screen outcome
                // without making the creator wait for the idle autosave.
                if (!SavePendingChanges(true))
                {
                    SetStatus("ROUTE BLOCKED // SAVE NEW BUTTON ACTION FIRST");
                    return;
                }
                std::string error;
                bool routeChanged = false;
                if (!openScreenRoute_(
                        session_->Document(), buttonId, destinationId,
                        routeChanged, error))
                {
                    SetStatus("ROUTE REJECTED // " + error);
                    return;
                }
                if (routeChanged)
                {
                    timeline_.ObserveScreen(
                        session_->UndoCount(), session_->RedoCount(), session_->HistoryMutation());
                    timeline_.RecordFlowRoute();
                    autosave_.Edited();
                    SetStatus("STORYFLOW ROUTE UPDATED // CTRL+Z UNDOES ROUTE");
                }
                else SetStatus("STORYFLOW DESTINATION ALREADY LINKED");
            });
    }
}

namespace renegade::studio
{
    std::vector<bridge::StableId> RenegadeScreenEditorWorkspace::SelectedIds() const
    {
        std::vector<bridge::StableId> ids;
        if(session_)for(const auto& w:session_->Document().widgets)
            if(selectedWidgetIds_.count(w.id))ids.push_back(w.id);
        return ids;
    }
    void RenegadeScreenEditorWorkspace::PasteClipboard()
    {
        if(!session_)return;
        std::vector<bridge::StableId> ids;std::string error;
        if(!session_->PasteWidgets(clipboard_,20,20,ids,error))
        {SetStatus("PASTE REJECTED // "+error);return;}
        selectedWidgetIds_.clear();selectedWidgetIds_.insert(ids.begin(),ids.end());
        selectedWidgetId_=ids.empty()?bridge::StableId{}:ids.front();
        RefreshAfterMutation("SELECTION PASTED // INDEPENDENT BUTTON ACTIONS");
    }
    void RenegadeScreenEditorWorkspace::HandleSelectionShortcuts()
    {
        if(carouselEditorOpen_||liquidEditorOpen_||navigationEditorOpen_||minimapEditorOpen_)return;
        if(!session_||!session_->IsLoaded()||directManipulationMode_!=DirectManipulationMode::None||
            panelDrag_!=PanelDrag::None||!layerDragId_.empty())return;
        const bool ctrl=wi::input::Down(wi::input::KEYBOARD_BUTTON_LCONTROL)||
            wi::input::Down(wi::input::KEYBOARD_BUTTON_RCONTROL);
        const bool shift=wi::input::Down(wi::input::KEYBOARD_BUTTON_LSHIFT)||
            wi::input::Down(wi::input::KEYBOARD_BUTTON_RSHIFT);
        const auto pressed=[](int key){return wi::input::Press(static_cast<wi::input::BUTTON>(key));};
        std::string error;
        if(ctrl)
        {
            if(pressed('A'))
            {
                selectedWidgetIds_.clear();
                for(const auto& w:session_->Document().widgets)selectedWidgetIds_.insert(w.id);
                if(!session_->Document().widgets.empty())selectedWidgetId_=session_->Document().widgets.front().id;
                RefreshInspector();SetStatus("ALL ELEMENTS SELECTED");return;
            }
            if(pressed('C')||pressed('X'))
            {
                const bool cut=wi::input::Down(static_cast<wi::input::BUTTON>('X'));
                if(!session_->CopyWidgets(SelectedIds(),clipboard_,error))
                {SetStatus("COPY REJECTED // "+error);return;}
                if(cut)DeleteSelectedWidget();else SetStatus("SELECTION COPIED // CTRL+V TO PASTE");
                return;
            }
            if(pressed('V')){PasteClipboard();return;}
            if(pressed('D')){DuplicateSelectedWidget();return;}
            if(pressed('G'))
            {
                std::vector<bridge::StableId> result;bridge::StableId group;
                const bool ok=shift?session_->UngroupWidget(selectedWidgetId_,result,error):
                    session_->GroupWidgets(SelectedIds(),group,error);
                if(!ok){SetStatus("GROUP EDIT REJECTED // "+error);return;}
                if(!shift)result={group};
                selectedWidgetIds_.clear();selectedWidgetIds_.insert(result.begin(),result.end());
                selectedWidgetId_=result.empty()?bridge::StableId{}:result.front();
                RefreshAfterMutation(shift?"CONTAINER UNGROUPED":"SELECTION GROUPED");
                return;
            }
        }
        else if(!selectedWidgetIds_.empty())
        {
            const float step=shift?10.0f:1.0f;
            float dx=0,dy=0;
            if(pressed(0x25))dx=-step;else if(pressed(0x27))dx=step;
            else if(pressed(0x26))dy=-step;else if(pressed(0x28))dy=step;
            if(dx||dy)
            {
                if(!session_->TranslateWidgets(SelectedIds(),dx,dy,error))
                    SetStatus("NUDGE REJECTED // "+error);
                else RefreshAfterMutation("SELECTION MOVED");
            }
        }
    }
}

namespace renegade::studio {
 bool RenegadeScreenEditorWorkspace::SelectionBounds(bridge::ScreenRect& box,bool logical) const {
    bool first=true;
    for(const auto& id:SelectedIds()){
        const auto* widget=session_?session_->FindWidget(id):nullptr;
        auto parent=widget?widget->parentId:bridge::StableId{};bool nested=false;
        std::unordered_set<bridge::StableId> visited;
        while(!parent.empty()&&visited.insert(parent).second){
            if(selectedWidgetIds_.count(parent)){nested=true;break;}
            const auto* p=session_->FindWidget(parent);parent=p?p->parentId:bridge::StableId{};
        }
        if(nested)continue;
        bridge::ScreenRect r;std::string error;
        if(logical){const auto* p=renderer_?renderer_->LogicalRect(id):nullptr;if(!p)return false;r=*p;}
        else if(!session_||!bridge::ResolveScreenWidgetRect(session_->Document(),id,r,error))return false;
        if(first){box=r;first=false;}else{const float right=std::max(box.x+box.width,r.x+r.width),bottom=std::max(box.y+box.height,r.y+r.height);
            box.x=std::min(box.x,r.x);box.y=std::min(box.y,r.y);box.width=right-box.x;box.height=bottom-box.y;}
    }return !first;
 }
}

namespace renegade::studio {
 void RenegadeScreenEditorWorkspace::LoadProjectGrid(){
    gridSettingsValid_=false;gridProjectRoot_.clear();
    if(!session_||!session_->IsLoaded())return;
    auto p=std::filesystem::u8path(session_->FilePath()).parent_path();
    while(!p.empty()&&p.filename()!="Content"&&p!=p.parent_path())p=p.parent_path();
    if(p.filename()!="Content"){SetStatus("GRID SETTINGS REQUIRE PROJECT CONTENT");return;}
    gridProjectRoot_=p.parent_path().generic_u8string();std::string error;
    if(!bridge::ScreenGridPreferences::Load(gridProjectRoot_,session_->ProjectId(),projectGrid_,error)){
        gridSpacing_=16;gridVisible_=gridSnapEnabled_=true;SetStatus("GRID SETTINGS UNAVAILABLE // "+error);return;}
    gridSettingsValid_=true;const auto v=projectGrid_.Resolve(session_->Document().envelope.documentId);
    gridSpacing_=v.spacing;gridVisible_=v.grid;gridSnapEnabled_=v.snap;
    gridSpacingInput_.SetValue(std::to_string(gridSpacing_));gridVisibleButton_.SetText(gridVisible_?"GRID ON":"GRID OFF");gridSnapButton_.SetText(gridSnapEnabled_?"SNAP ON":"SNAP OFF");
 }
 bool RenegadeScreenEditorWorkspace::SaveProjectGrid(){
    if(!session_||!gridSettingsValid_){SetStatus("GRID SETTINGS UNAVAILABLE // CHANGE NOT SAVED");return false;}
    auto candidate=projectGrid_;candidate.screens[session_->Document().envelope.documentId]={gridSpacing_,gridVisible_,gridSnapEnabled_};
    std::string error;if(!candidate.Save(gridProjectRoot_,session_->ProjectId(),error)){
        const auto old=projectGrid_.Resolve(session_->Document().envelope.documentId);gridSpacing_=old.spacing;gridVisible_=old.grid;gridSnapEnabled_=old.snap;
        SetStatus("GRID SAVE FAILED // "+error);return false;}
    projectGrid_=std::move(candidate);return true;
 }
}

namespace renegade::studio {
 void RenegadeScreenEditorWorkspace::TogglePane(bool left){
    if(directManipulationMode_!=DirectManipulationMode::None||panelDrag_!=PanelDrag::None)return;
    if(left){if(!leftCollapsed_)preferredLeft_=panels_.left;leftCollapsed_=!leftCollapsed_;panels_.left=leftCollapsed_?0:preferredLeft_;}
    else{if(!rightCollapsed_)preferredRight_=panels_.right;rightCollapsed_=!rightCollapsed_;panels_.right=rightCollapsed_?0:preferredRight_;}
    LayoutControls();if(panelLayoutChanged_)panelLayoutChanged_();NotifyPreferencesChanged();
 }
}

namespace renegade::studio
{
    const bridge::ScreenWidget* RenegadeScreenEditorWorkspace::FindSelectedLiquid() const
    {
        if(!session_||!session_->IsLoaded())return nullptr;
        const auto* selected=session_->FindWidget(selectedWidgetId_);
        if(selected&&selected->kind==bridge::ScreenWidgetKind::Progress)return selected;
        const bridge::ScreenWidget* found=nullptr;
        for(const auto& w:session_->Document().widgets)if(w.liquid)
        {
            const auto* item=&w;size_t remaining=session_->Document().widgets.size();
            while(item&&remaining--)
            {if(item->id==selectedWidgetId_){if(found)return nullptr;found=&w;break;}item=session_->FindWidget(item->parentId);}
        }
        return found;
    }
    void RenegadeScreenEditorWorkspace::OpenLiquidEditor()
    {
        const auto* selected=FindSelectedLiquid();if(!selected)return;
        const auto id=selected->id,document=session_->Document().envelope.documentId,root=gridProjectRoot_;
        liquidEditor_->Open(*selected,root,session_->ProjectId(),[this,id,document,root](std::optional<bridge::ScreenLiquid> liquid,bridge::ScreenColor colour,
            std::vector<bridge::ScreenBinding> bindings,bridge::ScreenFillDirection direction,std::string& error){
            if(!session_||!session_->IsLoaded()||gridProjectRoot_!=root||session_->Document().envelope.documentId!=document)
            {error="The original Screen is no longer open.";return false;}
            if(!session_->ConfigureProgressFill(id,std::move(liquid),colour,std::move(bindings),direction,error))return false;
            RefreshAfterMutation("Fill layer settings saved");return true;
        },[this]{liquidEditorOpen_=false;if(documentChanged_)documentChanged_();},
            [this,id,document,root](const std::optional<bridge::ScreenLiquid>& liquid,bridge::ScreenFillDirection direction,bridge::ScreenColor colour,float fill,std::string& error){
                if(renderer_&&session_&&session_->IsLoaded()&&root==gridProjectRoot_&&document==session_->Document().envelope.documentId)
                    return renderer_->PreviewLiquidDraft(id,liquid,direction,colour,fill,error);
                error="The original Screen is no longer open.";return false;
            });liquidEditorOpen_=true;
    }
    static std::filesystem::path PersonalUiPrefabs()
    {
        const auto* local=std::getenv("LOCALAPPDATA");
        return local?std::filesystem::u8path(local)/"RenegadeStudio/UILibrary/Prefabs":std::filesystem::path{};
    }
    void RenegadeScreenEditorWorkspace::SaveSelectedUiPrefab()
    {
        if(!session_||!session_->IsLoaded()||selectedWidgetId_.empty())return;
        const auto library=PersonalUiPrefabs();std::error_code ec;
        if(library.empty()){SetStatus("Personal UI prefab location unavailable.");return;}
        std::filesystem::create_directories(library,ec);if(ec){SetStatus(ec.message());return;}
        std::string error;
        if(!bridge::PublishScreenWidgetPrefab(gridProjectRoot_,*session_,selectedWidgetId_,
            (library/std::filesystem::u8path(bridge::GenerateStableId())).generic_u8string(),error))
        {SetStatus(error);return;}
        SetStatus("UI prefab saved with its hierarchy, animation, colours, sounds and binding slots.");
    }
    void RenegadeScreenEditorWorkspace::InsertUiPrefab(const std::string& package,bool top)
    {
        if(!session_||!session_->IsLoaded())return;
        std::vector<bridge::StableId> created;std::string error;
        bridge::ScreenDocument entry;
        if(!bridge::InspectPortableScreenPackage(package,entry,error)){SetStatus(error);return;}
        const auto root=std::find_if(entry.widgets.begin(),entry.widgets.end(),[](const auto& w){return w.parentId.empty();});
        if(root==entry.widgets.end()){SetStatus("This prefab has no root layer.");return;}
        bridge::ScreenRect bounds;
        if(!bridge::ResolveScreenWidgetRect(entry,root->id,bounds,error)){SetStatus(error);return;}
        const auto x=std::max(0.f,(session_->Document().designWidth-bounds.width)*.5f),y=top?24.f:std::max(0.f,(session_->Document().designHeight-bounds.height)*.5f);
        if(!bridge::InsertScreenWidgetPrefab(gridProjectRoot_,*session_,package,x,y,created,error))
        {SetStatus(error);return;}
        if(!created.empty())SelectWidget(created.front());
        RefreshAfterMutation("UI prefab inserted. Expand its layers to edit each part; Fill settings controls a Progress layer.");
    }
    void RenegadeScreenEditorWorkspace::ChooseExternalUiPrefab()
    {
        if(!session_||!session_->IsLoaded())return;
        const auto root=gridProjectRoot_,document=session_->Document().envelope.documentId;
        wi::helper::FileDialogParams dialog;dialog.type=wi::helper::FileDialogParams::OPEN;
        dialog.description="Choose UI prefab renegade-ui-package.json";dialog.extensions={"json"};
        wi::helper::FileDialog(dialog,[this,root,document](const std::string& file){
            if(file.empty())return;
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this,root,document,file](std::uint64_t){
                if(!session_||!session_->IsLoaded()||root!=gridProjectRoot_||document!=session_->Document().envelope.documentId)return;
                const auto path=std::filesystem::u8path(file);
                if(path.filename()!="renegade-ui-package.json"){SetStatus("Choose renegade-ui-package.json.");return;}
                InsertUiPrefab(path.parent_path().generic_u8string());
            });
        });
    }
    void RenegadeScreenEditorWorkspace::ChooseUiPrefab()
    {
        std::vector<RenegadeScreenChoicePicker::Choice> choices;
        const auto library=PersonalUiPrefabs();std::error_code ec;
        if(!library.empty()&&std::filesystem::is_directory(library,ec))
            for(const auto& directory:std::filesystem::directory_iterator(library,ec))
            {
                if(!directory.is_directory())continue;
                bridge::ScreenDocument doc;std::string error;
                if(bridge::InspectPortableScreenPackage(directory.path().generic_u8string(),doc,error)&&!doc.widgets.empty())
                    choices.push_back({doc.widgets.front().name,directory.path().generic_u8string()});
            }
        choices.push_back({"Import UI prefab from file...","@import"});
        destinationPicker_.Open("SAVED UI PREFABS",std::move(choices),"",[this](const std::string& value){
            if(value=="@import")ChooseExternalUiPrefab();else InsertUiPrefab(value);
        });
    }
}

namespace renegade::studio
{
    void RenegadeScreenEditorWorkspace::OpenNavigationEditor()
    {
        const auto* selected=session_?session_->FindWidget(selectedWidgetId_):nullptr;if(!selected||!selected->navigation)return;
        const auto id=selected->id,document=session_->Document().envelope.documentId,root=gridProjectRoot_;
        if(selected->navigation->minimap){
            minimapEditor_->Open(*selected,root,session_->ProjectId(),
                [this,id,document,root](bridge::ScreenWidget draft,std::string& error){
                    if(!session_||!session_->IsLoaded()||gridProjectRoot_!=root||session_->Document().envelope.documentId!=document){error="The original Screen is no longer open.";return false;}
                    draft.id=id;if(!session_->ConfigureNavigationComponent(id,draft,error))return false;
                    RefreshAfterMutation("Minimap settings saved");return true;
                },[this]{minimapEditorOpen_=false;if(documentChanged_)documentChanged_();});
            minimapEditorOpen_=true;return;
        }
        navigationEditor_->Open(*selected,root,session_->ProjectId(),
            [this,id,document,root](bridge::ScreenWidget draft,std::string& error){
                if(!session_||!session_->IsLoaded()||gridProjectRoot_!=root||session_->Document().envelope.documentId!=document)
                {error="The original Screen is no longer open.";return false;}
                draft.id=id;if(!session_->ConfigureNavigationComponent(id,draft,error))return false;
                RefreshAfterMutation("Navigation settings saved");return true;
            },[this]{navigationEditorOpen_=false;if(documentChanged_)documentChanged_();},
            [this,id,document,root](const bridge::ScreenWidget& draft,float heading,std::string& error){
                if(!renderer_||!session_||!session_->IsLoaded()||gridProjectRoot_!=root||session_->Document().envelope.documentId!=document)
                {error="The original Screen is no longer open.";return false;}
                return renderer_->PreviewNavigationDraft(id,*draft.navigation,heading,error,&draft);
            });
        navigationEditorOpen_=true;
    }
}
