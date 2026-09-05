#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/StoryFlowAuthoringModel.h"
#include "renegade/bridge/StoryFlowAuthoringSession.h"
#include "renegade/bridge/StoryFlowJourneyModel.h"
#include "renegade/bridge/StoryFlowJourneyThumbnailService.h"
#include "renegade/bridge/StoryFlowLayoutService.h"
#include "RenegadeStoryFlowJourneyCard.h"
#include "RenegadeStoryFlowJourneyLane.h"
#include "RenegadeStudioChrome.h"

namespace renegade::studio
{
    class RenegadeStoryFlowGraphEditor;

    // Native synchronized Journey/Graph authoring surface. Semantic edits are
    // delegated to StoryFlowAuthoringSession; this widget owns presentation
    // state, shared selection, activation and native editor controls only.
    class RenegadeStoryFlowWorkspace final : public wi::gui::Widget
    {
    public:
        using ScreenOutcomeQuery = std::function<bool(
            const bridge::StableId&,
            std::vector<std::string>&,
            std::string&)>;

        void Create();
        void SetLayout(float width, float height);
        void Bind(
            bridge::StoryFlowAuthoringSession* session,
            bridge::StoryFlowAuthoringModel* model,
            bridge::StoryFlowLayoutDocument* layout);
        void Clear() noexcept;

        void FitToContent();
        void CenterOnGameStart();
        void SelectAndFocusNode(const bridge::StableId& nodeId);
        [[nodiscard]] bool FindAndFocusJourneyNode(const std::string& query);
        void AdjustJourneyZoom(float factor);
        void SetJourneyZoom(float zoom);
        void ToggleJourneyFilter();
        void SaveJourney();
        void UndoJourney();
        void RedoJourney();
        void ValidateRuntimeReadiness();
        void AddJourneyTerminal(bridge::FlowNodeKind kind);

        [[nodiscard]] bool IsDirty() const noexcept
        {
            return session_ && session_->IsDirty();
        }

        [[nodiscard]] bool CanUndo() const noexcept
        {
            return session_ && session_->CanUndo();
        }

        [[nodiscard]] bool CanRedo() const noexcept
        {
            return session_ && session_->CanRedo();
        }

        [[nodiscard]] float JourneyZoom() const noexcept
        {
            return layout_ ? layout_->journeyCanvas.zoom : 1.0f;
        }

        [[nodiscard]] bool JourneyFilterActive() const noexcept
        {
            return hideDetached_;
        }

        // Gate 9D promotes Journey/Graph switching to real native chrome. The
        // workspace remains the presentation-state authority, but callers no
        // longer have to synthesize clicks against its legacy painted tabs.
        void ActivateView(const bridge::StoryFlowViewMode view)
        {
            SetActiveView(view);
        }

        void OnSelectionChanged(
            std::function<void(const bridge::StableId&)> callback);
        void OnLayoutChanged(std::function<void()> callback);
        void OnNodeActivated(
            std::function<void(const bridge::StableId&)> callback);
        void OnScreenOutcomeQuery(ScreenOutcomeQuery callback);
        void OnSemanticChanged(std::function<void()> callback);
        void SetExternalStatus(std::string message);

        // Shared semantic deletion entry point. Gate 9D exposes this command
        // only while Graph View is active; Journey remains a navigation and
        // presentation surface rather than a second topology editor.
        [[nodiscard]] bool CanDeleteSelection() const noexcept;
        void DeleteSelection();

        [[nodiscard]] const bridge::StableId& SelectedNodeId() const noexcept
        {
            return selectedNodeId_;
        }

        [[nodiscard]] const bridge::StableId& SelectedRouteId() const noexcept
        {
            return selectedRouteId_;
        }

        [[nodiscard]] bool ConsumedPointerThisFrame() const noexcept
        {
            return pointerConsumed_;
        }

        [[nodiscard]] bridge::StoryFlowViewMode ActiveView() const noexcept
        {
            return layout_ ? layout_->activeView
                : bridge::StoryFlowViewMode::Journey;
        }

        void Update(const wi::Canvas& canvas, float dt) override;
        void Render(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const override;
        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStoryFlowWorkspace";
        }

    private:
        friend class RenegadeStoryFlowGraphEditor;

        static constexpr float InspectorWidth = 320.0f;

        [[nodiscard]] const bridge::StoryFlowNodeLayout* FindLayout(
            const bridge::StableId& nodeId) const noexcept;
        [[nodiscard]] bridge::StoryFlowNodeLayout* FindLayout(
            const bridge::StableId& nodeId) noexcept;
        [[nodiscard]] const bridge::StoryFlowJourneyCardLayout* FindJourneyLayout(
            const bridge::StableId& nodeId) const noexcept;
        [[nodiscard]] bridge::StoryFlowJourneyCardLayout* FindJourneyLayout(
            const bridge::StableId& nodeId) noexcept;
        [[nodiscard]] const bridge::FlowNode* FindDocumentNode(
            const bridge::StableId& nodeId) const noexcept;
        [[nodiscard]] const bridge::FlowRoute* FindDocumentRoute(
            const bridge::StableId& routeId) const noexcept;
        [[nodiscard]] bool QueryScreenOutcomes(
            const bridge::StableId& sourceNodeId,
            std::vector<std::string>& outcomes,
            std::string& error) const;
        [[nodiscard]] bool ValidateRouteOutcomeForSource(
            const bridge::StableId& sourceNodeId,
            const std::string& outcome,
            std::string& error) const;
        [[nodiscard]] XMFLOAT2 CanvasToScreen(float x, float y) const noexcept;
        [[nodiscard]] XMFLOAT2 ScreenToCanvas(float x, float y) const noexcept;
        [[nodiscard]] XMFLOAT4 NodeScreenBounds(
            const bridge::StoryFlowNodeLayout& node) const noexcept;
        [[nodiscard]] XMFLOAT4 JourneyCardScreenBounds(
            const bridge::StoryFlowJourneyCard& card) const noexcept;
        [[nodiscard]] wi::graphics::Rect JourneyCanvasScissorRect() const noexcept;
        [[nodiscard]] XMFLOAT4 NodeBounds(
            const bridge::StableId& nodeId) const noexcept;
        [[nodiscard]] bridge::StoryFlowCanvasLayout& ActiveCanvas() noexcept;
        [[nodiscard]] const bridge::StoryFlowCanvasLayout& ActiveCanvas() const noexcept;
        [[nodiscard]] float GraphWidth() const noexcept;
        [[nodiscard]] bool PointerInsideGraph(const XMFLOAT4& pointer) const noexcept;
        [[nodiscard]] bridge::StableId HitTestRoute(
            const XMFLOAT4& pointer) const;
        [[nodiscard]] static bool IsTerminalKind(
            bridge::FlowNodeKind kind) noexcept;

        void SelectNode(const bridge::StableId& nodeId);
        void SelectRoute(const bridge::StableId& routeId);
        void NotifyLayoutChanged();
        void SetStatus(std::string message);
        void EnsureSelectionValid();
        void SetActiveView(bridge::StoryFlowViewMode view);
        [[nodiscard]] bool RebuildJourneyProjection();
        void RebuildJourneyObjects();
        void UpdateJourneyObjects(const wi::Canvas& canvas, float dt);
        void RefreshJourneyThumbnailResources();
        void ChooseLevelThumbnail(const bridge::StableId& nodeId);
        [[nodiscard]] std::string JourneyCardSubtitle(
            const bridge::StoryFlowNodeView& node) const;
        void RememberOrActivateNodeClick(
            const bridge::StoryFlowNodeView& node,
            const XMFLOAT4& pointer);

        void CreateAuthoringControls();
        void LayoutAuthoringControls();
        void UpdateAuthoringControls(const wi::Canvas& canvas, float dt);
        void RenderAuthoringControls(
            const wi::Canvas& canvas,
            wi::graphics::CommandList cmd) const;
        void RefreshInspectorControls();
        void RefreshJourneyExitControls();
        void RefreshRuntimeValidation();
        void RewireJourneyExit(
            std::size_t exitIndex,
            std::size_t destinationIndex);
        void AddJourneyAction();

        [[nodiscard]] bool RefreshPresentationAfterSemanticChange();
        void SaveFlow();
        void UndoFlow();
        void RedoFlow();
        void ApplySelectedNode();
        void DeleteSelectedNode();
        void ApplySelectedRoute();
        void DeleteSelectedRoute();
        void AddTerminalNode(bridge::FlowNodeKind kind, const char* defaultName);
        void BeginConnect();
        void BeginReconnect();
        void CommitConnectionTo(const bridge::StableId& destinationNodeId);

        bridge::StoryFlowAuthoringSession* session_ = nullptr;
        bridge::StoryFlowAuthoringModel* model_ = nullptr;
        bridge::StoryFlowJourneyModel journeyModel_;
        bridge::StoryFlowLayoutDocument* layout_ = nullptr;
        bridge::StoryFlowJourneyThumbnailService journeyThumbnailService_;
        std::unordered_map<bridge::StableId, wi::Resource>
            journeyThumbnailResources_;
        std::unordered_map<
            bridge::StableId,
            std::unique_ptr<RenegadeStoryFlowJourneyCard>> journeyCardObjects_;
        std::unordered_map<
            std::size_t,
            std::unique_ptr<RenegadeStoryFlowJourneyLane>> journeyLaneObjects_;
        std::string projectRoot_;
        bridge::StableId selectedNodeId_;
        bridge::StableId selectedRouteId_;
        bridge::StableId connectionSourceNodeId_;
        bridge::StableId reconnectRouteId_;
        std::function<void(const bridge::StableId&)> selectionChanged_;
        std::function<void(const bridge::StableId&)> nodeActivated_;
        std::function<void()> layoutChanged_;
        ScreenOutcomeQuery screenOutcomeQuery_;
        std::function<void()> semanticChanged_;
        std::string statusMessage_ = "READY";
        bool runtimeValidationReady_ = true;
        std::string runtimeValidationMessage_ =
            "Runtime readiness checks passed.";
        float width_ = 1.0f;
        float height_ = 1.0f;
        bool panning_ = false;
        bool nodeDragging_ = false;
        XMFLOAT2 panPointerAnchor_ = {};
        XMFLOAT2 panValueAnchor_ = {};
        XMFLOAT2 nodeDragPointerAnchor_ = {};
        XMFLOAT2 nodeDragValueAnchor_ = {};
        bridge::StableId draggedNodeId_;
        bridge::StableId previousClickedNodeId_;
        XMFLOAT2 previousClickPointer_ = {};
        float secondsSincePreviousNodeClick_ = 1000.0f;
        bool pointerConsumed_ = false;
        bool hideDetached_ = false;
        std::unordered_set<std::size_t> collapsedJourneyTracks_;

        RenegadeButton saveButton_;
        RenegadeButton undoButton_;
        RenegadeButton redoButton_;
        RenegadeButton connectButton_;

        RenegadeTextInputField nodeNameInput_;
        RenegadeButton applyNodeButton_;
        RenegadeButton deleteNodeButton_;
        RenegadeButton openDestinationButton_;
        RenegadeButton addJourneyActionButton_;

        static constexpr std::size_t MaxJourneyInspectorExits = 5;
        std::array<RenegadeComboBox, MaxJourneyInspectorExits>
            journeyExitDestinationCombos_;
        std::array<bridge::StableId, MaxJourneyInspectorExits>
            journeyExitRouteIds_;
        std::vector<bridge::StableId> journeyExitDestinationIds_;
        bool journeyAddActionAvailable_ = false;
        std::size_t pendingJourneyExitIndex_ = MaxJourneyInspectorExits;
        std::size_t pendingJourneyDestinationIndex_ = 0;
        bool pendingAddJourneyAction_ = false;

        RenegadeTextInputField routeOutcomeInput_;
        RenegadeTextInputField routeEntryInput_;
        RenegadeTextInputField routePriorityInput_;
        RenegadeButton applyRouteButton_;
        RenegadeButton reconnectRouteButton_;
        RenegadeButton deleteRouteButton_;

        RenegadeButton addCompleteButton_;
        RenegadeButton addReturnButton_;
        RenegadeButton addQuitButton_;
    };
}
