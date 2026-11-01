#pragma once
#include "RenegadeScreenLiteralTextInput.h"
#include "RenegadeScreenInventoryInspector.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/screen/ScreenRenderer.h"
#include "RenegadeStudioChrome.h"
#include "ScreenEditorPanelLayout.h"
#include "ScreenEditorLibraryPalette.h"
#include "RenegadeScreenChoicePicker.h"
#include "ScreenEditorUndoTimeline.h"
#include "ScreenEditorAutosavePolicy.h"
#include "ScreenEditorWorkspacePreferences.h"
#include "ScreenEditorCanvasCamera.h"
#include "renegade/bridge/ScreenGridPreferences.h"

namespace renegade::studio
{
    // Native Screen authoring workspace. Gate 8C established selection,
    // Inspector editing and shared Runtime preview; Gate 8D adds creator-level
    // element transactions while keeping ScreenAuthoringSession authoritative.
    class RenegadeScreenEditorWorkspace final : public wi::gui::Widget
    {
    public:
        void Create();
        void SetLayout(float width, float height);
        void Bind(
            bridge::ScreenAuthoringSession* session,
            screen::ScreenRenderer* renderer);
        void Clear() noexcept;
        void SelectWidget(const bridge::StableId& widgetId, bool additive = false);
        void SetInspectorSuppressed(const bool suppressed) noexcept
        {
            if (inspectorSuppressed_ == suppressed) return;
            inspectorSuppressed_ = suppressed;
            if (suppressed)
            {
                basicActionPicker_.Close();
                destinationPicker_.Close();
            }

            for (wi::gui::Widget* widget : {
                    static_cast<wi::gui::Widget*>(&nameInput_),
                    static_cast<wi::gui::Widget*>(&textInput_),
                    static_cast<wi::gui::Widget*>(&positionXInput_),
                    static_cast<wi::gui::Widget*>(&positionYInput_),
                    static_cast<wi::gui::Widget*>(&sizeWidthInput_),
                    static_cast<wi::gui::Widget*>(&sizeHeightInput_),
                    static_cast<wi::gui::Widget*>(&applyButton_),
                    static_cast<wi::gui::Widget*>(&visibleButton_),
                    static_cast<wi::gui::Widget*>(&enabledButton_)})
            {
                widget->SetEnabled(!suppressed);
                if (suppressed) widget->SetVisible(false);
            }

            // The accepted 8C workspace still updates its controls each frame.
            // Guard every basic-Inspector mutation callback as well as disabling
            // the widgets so an advanced-panel click can never create a second
            // mutation through a covered 8C control.
            applyButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                if (!inspectorSuppressed_) ApplySelectedWidget();
            });
            visibleButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                if (!inspectorSuppressed_) ToggleVisible();
            });
            enabledButton_.OnClick([this](const wi::gui::EventArgs&)
            {
                if (!inspectorSuppressed_) ToggleEnabled();
            });
            textInput_.OnInputAccepted([this](const wi::gui::EventArgs&)
            {
                if (!inspectorSuppressed_) ApplySelectedWidget();
            });
        }

        void OnUiBudgetsRequested(std::function<void()> callback){uiBudgetsRequested_=std::move(callback);}
        void OnPortableLibraryRequested(std::function<void(bool)> callback){portableLibraryRequested_=std::move(callback);}
        void ReportLibraryStatus(std::string message){SetStatus(std::move(message));}
        void OnDocumentChanged(std::function<void()> callback);
        using InspectorOverlay = std::function<void(
            const wi::Canvas&, wi::graphics::CommandList)>;
        void OnInspectorOverlay(InspectorOverlay overlay)
        {
            inspectorOverlay_ = std::move(overlay);
        }
        void OnPanelLayoutChanged(std::function<void()> callback);
        void OnOpenProjectScreen(std::function<void(const bridge::StableId&)> callback);
        void OnOpenLibraryDocument(std::function<void(const bridge::StableId&)> callback);
        using SetProjectHudCallback=std::function<bool(const bridge::StableId&,const std::string&,std::string&)>;
        void OnSetProjectHud(SetProjectHudCallback callback);
        using SetProjectInventoryCallback=std::function<bool(const bridge::StableId&,const std::string&,std::string&)>;
        void OnSetProjectInventory(SetProjectInventoryCallback callback);
        void OnReadProjectInventory(std::function<bridge::StableId()> callback);
        void OpenProjectInventoryPicker();
        void OnGameplayAttributesRequested(std::function<void()> callback){gameplayAttributesRequested_=std::move(callback);}
        void SetProjectDefaultHud(bridge::StableId id) { projectDefaultHud_=std::move(id); }
        void SetProjectScreenLibrary(std::vector<ScreenEditorLibraryScreen> screens);
        void OpenProjectHudPicker();
        void SetRouteContext(bridge::StableId context, std::function<bool(const bridge::StableId&)> choose)
        { routeContext_ = std::move(context); chooseRouteContext_ = std::move(choose); }

        using OpenScreenRouteCallback = std::function<bool(
            const bridge::ScreenDocument&, const bridge::StableId&,
            const bridge::StableId&, bool&, std::string&)>;
        void OnOpenScreenRoute(OpenScreenRouteCallback callback);
        using RouteHistoryCallback = std::function<bool(bool undo, std::string&)>;
        void OnRouteHistoryAction(RouteHistoryCallback callback);
        using CoordinatedSaveCallback = std::function<bool(
            bridge::ScreenAuthoringSession&, std::string&)>;
        void OnCoordinatedSave(CoordinatedSaveCallback callback);
        void OnImageAssetRequested(std::function<void()> callback);
        void OnGraphicRequested(std::function<void(bool)> callback){graphicRequested_=std::move(callback);}
        void ReportGraphicStatus(std::string message,bool error) {
            SetStatus(message);inventoryInspector_.ReportGraphicStatus(std::move(message),error);
        }
        void OnProgressArtworkRequested(std::function<void(bool)> callback)
        { progressArtworkRequested_ = std::move(callback); }
        void OnAppearanceRequested(std::function<void()> callback);
        void HandleUndoRedoShortcuts();
        void HandleLayerOrderShortcuts();
        void HandleDeleteShortcut();
        void HandleSelectionShortcuts();
        // Called after normal and Advanced Inspector widget updates.
        void TickAutosave(float dt);
        [[nodiscard]] bool SavePendingBeforeNavigation();
        void OnPreferencesChanged(std::function<void()> callback);
        void OnExternalFlowDirty(std::function<bool()> callback);
        [[nodiscard]] ScreenEditorWorkspacePreferences ExportPreferences() const;
        void ImportPreferences(const ScreenEditorWorkspacePreferences& prefs);
        void SetOverlayInputBlocked(bool blocked) noexcept { overlayInputBlocked_ = blocked; }
        [[nodiscard]] bool HasOpenPopup() const noexcept
        {
            return screenSettingsPicker_.IsOpen() || basicActionPicker_.IsOpen() ||
                destinationPicker_.IsOpen();
        }
        [[nodiscard]] bool IsFocusCanvas() const noexcept
        {
            return focusCanvas_ || rightCollapsed_;
        }
        [[nodiscard]] float InspectorPanelWidth() const noexcept
        {
            return panels_.right;
        }
        void OnReturnRequested(std::function<void()> callback);

        [[nodiscard]] bridge::ScreenRect PreviewBounds() const noexcept;
        [[nodiscard]] bridge::ScreenRect PreviewViewport() const noexcept;
        void HandleCanvasNavigation();
        void FitCanvas();
        [[nodiscard]] bridge::ScreenRect MinimapBounds() const noexcept;
        [[nodiscard]] const bridge::StableId& SelectedWidgetId() const noexcept
        {
            return selectedWidgetId_;
        }
        [[nodiscard]] bool IsInspectorSuppressed() const noexcept
        {
            return inspectorSuppressed_;
        }

        void Update(const wi::Canvas& canvas, float dt) override;
        void Render(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override
        {
            return "RenegadeScreenEditorWorkspace";
        }

        // A missing project-owned linked source must not lock the creator out
        // of the authored Screen. Surface the failure in the normal status bar
        // while allowing Inspector repair and one-step fallback recovery.
        void ReportPreviewConflict(const std::string& error)
        {SetStatus("PREVIEW SOURCE CONFLICT // "+error+" // REPAIR LINK IN INSPECTOR");}
        void ReportPreviewRecovered()
        {SetStatus("SOURCE PREVIEW RESTORED // VERIFY LINKED OR LOCAL FALLBACK APPEARANCE");}

    private:
        std::function<void()> uiBudgetsRequested_;
        std::function<void(bool)> portableLibraryRequested_;
        bool InventoryAvailable() const;
        bool InventoryInspectorActive() const;
        bool inventoryElementMode_=false;
        RenegadeButton inventoryButton_;
        RenegadeScreenInventoryInspector inventoryInspector_;
        std::function<void(bool)> graphicRequested_;
        void CreateControls();
        void LayoutControls();
        void UpdateControls(const wi::Canvas& canvas, float dt);
        void RenderControls(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const;
        void RefreshInspector();
        void EnsureSelection();
        void ApplySelectedWidget();
        void SaveScreen();
        [[nodiscard]] bool SavePendingChanges(bool automatic);
        [[nodiscard]] bool HasPendingSave() const;
        void UndoScreen();
        void RedoScreen();
        void ToggleVisible();
        void ToggleEnabled();
        void CreateElement(bridge::ScreenWidgetKind kind, bool background);
        void DuplicateSelectedWidget();
        void DeleteSelectedWidget();
        void MoveSelectedWidgetToBack();
        void MoveSelectedWidgetBackward();
        void MoveSelectedWidgetForward();
        void MoveSelectedWidgetToFront();
        void ReturnToStoryFlow();
        void RefreshAfterMutation(std::string status);
        void SetStatus(std::string status);
        void SetGridSpacingFromInput();
        void ToggleFocusCanvas();
        void NotifyPreferencesChanged();
        void CreatePalettePreset(int preset);
        void OpenBasicActionPicker();
        void OpenDestinationPicker();
        void DrawToolbox(wi::graphics::CommandList cmd) const;
        void HandleToolboxPointer(const XMFLOAT4& pointer);
        void UpdateLayerDrag(const XMFLOAT4& pointer, bool holdingLeft);
        [[nodiscard]] float LayersTop() const noexcept;

        enum class DirectManipulationMode : std::uint8_t
        {
            None, Move, ResizeTopLeft, ResizeTopRight, ResizeBottomLeft, ResizeBottomRight,
        };
        [[nodiscard]] DirectManipulationMode HitResizeHandle(const XMFLOAT4& pointer) const noexcept;
        void BeginDirectManipulation(DirectManipulationMode mode, const XMFLOAT4& pointer);
        void UpdateDirectManipulation(const XMFLOAT4& pointer);
        void EndDirectManipulation();

        bridge::ScreenAuthoringSession* session_ = nullptr;
        screen::ScreenRenderer* renderer_ = nullptr;
        bridge::StableId selectedWidgetId_;
        std::unordered_set<bridge::StableId> selectedWidgetIds_;
        bridge::ScreenWidgetClipboard clipboard_;
        [[nodiscard]] std::vector<bridge::StableId> SelectedIds() const;
        void PasteClipboard();

        std::function<void()> documentChanged_;
        InspectorOverlay inspectorOverlay_;
        std::function<void()> returnRequested_;
        std::string status_ = "NO SCREEN OPEN";
        float width_ = 1.0f;
        float height_ = 1.0f;
        ScreenEditorPanelLayout panels_;
        ScreenEditorCanvasCamera camera_;
        bool minimapSuppressDrag_ = false;
        bool cameraPanning_ = false;
        XMFLOAT2 cameraLastPointer_ = {};
        RenegadeButton fitCanvasButton_;
        RenegadeButton screenSettingsButton_;
        RenegadeScreenChoicePicker screenSettingsPicker_;
        enum class PanelDrag { None, Left, Right };
        PanelDrag panelDrag_ = PanelDrag::None;
        std::function<void()> panelLayoutChanged_;
        std::function<void()> preferencesChanged_;
        std::function<bool()> externalFlowDirty_;
        ScreenEditorPanelLayout savedPanels_;
        bool focusCanvas_ = false;
        std::size_t hierarchyScroll_ = 0;
        bridge::StableId layerDragId_;
        bridge::StableId layerDropTargetId_;
        XMFLOAT2 layerDragStart_ = {};
        bool layerDragActive_ = false;
        bool layerDropAfter_ = false;
        std::size_t toolboxScroll_ = 0;
        std::vector<ScreenEditorLibraryScreen> projectScreens_;
        std::function<void(const bridge::StableId&)> openProjectScreen_;
        std::function<void(const bridge::StableId&)> openLibraryDocument_;
        SetProjectHudCallback setProjectHud_;
        SetProjectInventoryCallback setProjectInventory_;
        std::function<bridge::StableId()> readProjectInventory_;
        std::function<void()> gameplayAttributesRequested_;
        bridge::StableId projectDefaultHud_;
        OpenScreenRouteCallback openScreenRoute_;
        bridge::StableId routeContext_;
        std::function<bool(const bridge::StableId&)> chooseRouteContext_;
        RouteHistoryCallback routeHistoryAction_;
        CoordinatedSaveCallback coordinatedSave_;
        std::function<void()> imageAssetRequested_;
        std::function<void(bool)> progressArtworkRequested_;
        std::function<void()> appearanceRequested_;
        ScreenEditorUndoTimeline timeline_;
        ScreenEditorUndoTimeline gestureTimeline_;
        ScreenEditorAutosavePolicy autosave_;
        std::string toolboxFilter_;
        // Compact by default; searching temporarily reveals matching rows.
        std::unordered_set<std::string> collapsedCategories_ = {
            "0:SETTINGS", "0:HUDS", "0:GAMEPLAY", "0:OUTCOMES",
            "1:PLAYER", "1:CHARACTERS", "1:COMBAT", "1:EQUIPMENT",
            "1:INVENTORY", "1:PROGRESSION", "1:OBJECTIVES", "1:WORLD",
            "1:CUSTOM", "2:LAYOUT", "2:MEDIA", "2:DATA", "2:INPUT",
            "2:NAVIGATION", "2:GAMEPLAY", "2:FEEDBACK", "2:REUSABLE",
            "3:PROJECT", "3:THEME", "3:ASSETS", "3:SHARED",
            "3:ACCESSIBILITY", "3:LIBRARY"
        };
        ScreenToolboxTab toolboxTab_ = ScreenToolboxTab::Elements;
        bool overlayInputBlocked_ = false;
        bool inspectorSuppressed_ = false;
        DirectManipulationMode directManipulationMode_ = DirectManipulationMode::None;
        XMFLOAT2 directManipulationStartPointer_ = {};
        bridge::ScreenRect directManipulationStartRect_;
        bridge::ScreenAuthoringSession::GeometryCapture geometryCapture_;
        bool SelectionBounds(bridge::ScreenRect& rect, bool logical) const;
        float directManipulationScaleX_ = 1.0f;
        float directManipulationScaleY_ = 1.0f;
        bool directManipulationChanged_ = false;
        bool alignmentGuideX_ = false;
        bool alignmentGuideY_ = false;
        float alignmentGuideCoordinateX_ = 0.0f;
        float alignmentGuideCoordinateY_ = 0.0f;
        bool gridVisible_ = true;
        bridge::ScreenGridPreferences projectGrid_;
        std::string gridProjectRoot_;
        bool gridSettingsValid_=false;
        void LoadProjectGrid();
        bool SaveProjectGrid();
        void TogglePane(bool left);
        bool leftCollapsed_=false,rightCollapsed_=false;
        float preferredLeft_=260,preferredRight_=360;
        bool gridSnapEnabled_ = true;
        int gridSpacing_ = 16;

        RenegadeButton returnButton_;
        RenegadeButton saveButton_;
        RenegadeButton undoButton_;
        RenegadeButton redoButton_;
        RenegadeTextInputField toolboxSearchInput_;
        RenegadeButton imageAssetButton_;
        RenegadeButton progressFrameButton_;
        RenegadeButton progressFillButton_;
        RenegadeButton appearanceButton_;
        RenegadeButton basicActionButton_;
        RenegadeScreenChoicePicker basicActionPicker_;
        RenegadeButton destinationButton_;
        RenegadeScreenChoicePicker destinationPicker_;
        RenegadeButton addTextButton_;
        RenegadeButton addButtonButton_;
        RenegadeButton addImageButton_;
        RenegadeButton addBackgroundButton_;
        RenegadeButton duplicateButton_;
        RenegadeButton deleteButton_;
        RenegadeButton backButton_;
        RenegadeButton backwardOneButton_;
        RenegadeButton forwardOneButton_;
        RenegadeButton frontButton_;
        RenegadeButton focusButton_;
        RenegadeButton gridVisibleButton_;
        RenegadeButton gridSnapButton_;
        RenegadeTextInputField gridSpacingInput_;
        RenegadeButton applyButton_;
        RenegadeButton visibleButton_;
        RenegadeButton enabledButton_;
        RenegadeScreenLiteralTextInput nameInput_;
        RenegadeScreenLiteralTextInput textInput_;
        RenegadeTextInputField fillInput_;
        RenegadeButton fillDirectionButton_;
        RenegadeTextInputField positionXInput_;
        RenegadeTextInputField positionYInput_;
        RenegadeTextInputField sizeWidthInput_;
        RenegadeTextInputField sizeHeightInput_;
    };
}
