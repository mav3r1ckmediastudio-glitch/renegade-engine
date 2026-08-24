#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <WickedEngine.h>
#include <imnodes.h>

#include "RenegadeImGuiBridge.h"
#include "renegade/bridge/StoryFlowAuthoringModel.h"
#include "renegade/bridge/StoryFlowAuthoringSession.h"
#include "renegade/bridge/StoryFlowLayoutService.h"

namespace renegade::studio
{
    class RenegadeStoryFlowWorkspace;

    // Graph-only node editor. Journey View never calls or renders this class.
    // ImNodes owns node/link mechanics; StoryFlowAuthoringSession remains the
    // sole semantic/history authority.
    class RenegadeStoryFlowGraphEditor final
    {
    public:
        using ScreenOutcomeQuery = std::function<bool(
            const bridge::StableId&,
            std::vector<std::string>&,
            std::string&)>;

        ~RenegadeStoryFlowGraphEditor();

        [[nodiscard]] bool Initialize(std::string& error);
        void Shutdown() noexcept;

        void Bind(
            bridge::StoryFlowAuthoringSession* session,
            bridge::StoryFlowAuthoringModel* model,
            bridge::StoryFlowLayoutDocument* layout,
            RenegadeStoryFlowWorkspace* workspace);
        void Clear() noexcept;
        void OnScreenOutcomeQuery(ScreenOutcomeQuery callback);

        void SetViewport(const XMFLOAT4& bounds) noexcept;
        [[nodiscard]] const XMFLOAT4& Viewport() const noexcept { return viewport_; }
        [[nodiscard]] bool ContainsPointer(const XMFLOAT4& pointer) const noexcept;

        void Update(const wi::Canvas& canvas, float dt, bool active);
        void Render(wi::graphics::CommandList cmd) const;

        // Gate 9D navigation is Graph-owned. Native Story Flow chrome calls
        // these commands directly instead of synthesizing clicks against the
        // retired pre-ImNodes Graph surface.
        void FitToContent();
        void CenterOnGameStart();
        [[nodiscard]] bool FocusNodeByName(const std::string& query);
        void RenderOverview(wi::graphics::CommandList cmd) const;

        // Host/UI reconciliation that must run after the ImNodes frame. This
        // keeps native Story Flow controls and ImNodes interaction state in
        // lock-step without moving semantic ownership out of the authoring
        // session.
        void ReconcileHostInteractions(bool allowDeleteShortcut);

        // ImNodes click/link state is transient UI state, never Story Flow
        // authority. Rebuild only the editor context when native controls take
        // ownership or a completed gesture remains latched. Node positions,
        // pan, selection authority and all Flow routes survive through the
        // Renegade layout/session model.
        void ResetTransientInteractionState() noexcept;
        void RecoverTransientInteractionIfIdle();

    private:
        struct OutputBinding
        {
            int pinId = 0;
            bridge::StableId sourceNodeId;
            std::string outcome;
            std::string label;
            bool authorable = true;
        };

        struct NodeBinding
        {
            int nodeId = 0;
            int inputPinId = 0;
            int staticAttributeId = 0;
            bridge::StableId stableId;
            std::vector<OutputBinding> outputs;
        };

        struct InputBinding
        {
            bridge::StableId destinationNodeId;
        };

        struct LinkBinding
        {
            int linkId = 0;
            bridge::StableId routeId;
            int outputPinId = 0;
            int inputPinId = 0;
        };

        [[nodiscard]] bool RefreshBindings(std::string& error);
        void ConfigureStyle();
        void DrawGraph();
        void DrawNode(const bridge::StoryFlowNodeView& node, NodeBinding& binding);
        void DrawLinks();
        void ProcessInteractionEvents();
        void SynchronizeSelection();
        void SynchronizeLayout();

        [[nodiscard]] const bridge::FlowNode* FindDocumentNode(
            const bridge::StableId& nodeId) const noexcept;
        [[nodiscard]] const bridge::FlowRoute* FindDocumentRoute(
            const bridge::StableId& routeId) const noexcept;
        [[nodiscard]] const OutputBinding* FindOutput(int pinId) const noexcept;
        [[nodiscard]] const InputBinding* FindInput(int pinId) const noexcept;
        [[nodiscard]] const LinkBinding* FindLink(int linkId) const noexcept;
        [[nodiscard]] const NodeBinding* FindNodeBinding(int nodeId) const noexcept;

        [[nodiscard]] bool DecodeLinkPins(
            int firstPin,
            int secondPin,
            const OutputBinding*& output,
            const InputBinding*& input) const noexcept;
        [[nodiscard]] bool CreateRoute(
            const OutputBinding& output,
            const InputBinding& input);
        [[nodiscard]] bool ReconnectRoute(
            const bridge::StableId& routeId,
            const InputBinding& input);
        void DeleteSelectedLinks();
        void ActivateHoveredNodeOnDoubleClick();
        void SetStatus(std::string message);
        void MarkSemanticRefresh();

        static const char* KindLabel(bridge::FlowNodeKind kind) noexcept;
        static bool IsTerminalKind(bridge::FlowNodeKind kind) noexcept;
        static std::string PortLabel(const std::string& outcome);

        RenegadeImGuiBridge imgui_;
        ImNodesContext* imnodesContext_ = nullptr;
        ImNodesEditorContext* editorContext_ = nullptr;

        bridge::StoryFlowAuthoringSession* session_ = nullptr;
        bridge::StoryFlowAuthoringModel* model_ = nullptr;
        bridge::StoryFlowLayoutDocument* layout_ = nullptr;
        RenegadeStoryFlowWorkspace* workspace_ = nullptr;
        ScreenOutcomeQuery screenOutcomeQuery_;

        std::vector<NodeBinding> nodes_;
        std::vector<LinkBinding> links_;
        std::unordered_map<bridge::StableId, std::size_t> nodeIndexByStableId_;
        std::unordered_map<int, std::size_t> nodeIndexByEditorId_;
        std::unordered_map<int, OutputBinding> outputsByPin_;
        std::unordered_map<int, InputBinding> inputsByPin_;
        std::unordered_map<int, std::size_t> linkIndexByEditorId_;
        std::unordered_map<bridge::StableId, int> linkIdByRouteId_;
        std::unordered_set<int> initializedNodePositions_;

        bridge::StableId pendingReconnectRouteId_;
        int linkStartedAtPin_ = 0;
        bool rejectNewLinkFromInput_ = false;
        bool bindingsDirty_ = true;
        bool frameActive_ = false;
        bool initialized_ = false;

        // ImNodes has no main-canvas zoom. Renegade maintains persisted Story
        // Flow coordinates as logical coordinates and applies a bounded
        // presentation scale to ImNodes geometry. Gate 9D's overview is derived
        // from the same logical coordinates and never becomes semantic state.
        bool zoomGeometryReady_ = false;
        float appliedGraphZoom_ = 1.0f;

        std::size_t observedUndoCount_ = static_cast<std::size_t>(-1);
        std::size_t observedRedoCount_ = static_cast<std::size_t>(-1);
        XMFLOAT4 viewport_ = {};
    };
}
