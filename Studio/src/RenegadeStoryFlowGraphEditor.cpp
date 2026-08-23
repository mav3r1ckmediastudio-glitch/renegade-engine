#include "RenegadeStoryFlowGraphEditor.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

#include <imgui.h>

#include "RenegadeStoryFlowWorkspace.h"

namespace renegade::studio
{
    namespace
    {
        constexpr ImU32 Surface0 = IM_COL32(8, 12, 16, 255);
        constexpr ImU32 Surface2 = IM_COL32(16, 23, 28, 255);
        constexpr ImU32 SurfaceHover = IM_COL32(22, 31, 37, 255);
        constexpr ImU32 SurfaceSelected = IM_COL32(38, 27, 21, 255);
        constexpr ImU32 Border = IM_COL32(38, 52, 61, 255);
        constexpr ImU32 BorderSoft = IM_COL32(25, 36, 43, 255);
        constexpr ImU32 Text = IM_COL32(214, 214, 214, 255);
        constexpr ImU32 TextStrong = IM_COL32(244, 244, 244, 255);
        constexpr ImU32 Muted = IM_COL32(142, 151, 156, 255);
        constexpr ImU32 Forge = IM_COL32(210, 91, 29, 255);
        constexpr ImU32 Link = IM_COL32(107, 126, 136, 255);
        constexpr ImU32 LinkHover = IM_COL32(211, 218, 221, 255);
        constexpr ImU32 Warning = IM_COL32(224, 165, 82, 255);

        bool AlmostEqual(const float a, const float b) noexcept
        {
            return std::abs(a - b) < 0.05f;
        }

        ImVec4 ToVec4(const ImU32 color)
        {
            const ImVec4 unpacked = ImGui::ColorConvertU32ToFloat4(color);
            return unpacked;
        }
    }

    RenegadeStoryFlowGraphEditor::~RenegadeStoryFlowGraphEditor()
    {
        Shutdown();
    }

    bool RenegadeStoryFlowGraphEditor::Initialize(std::string& error)
    {
        if (initialized_)
        {
            error.clear();
            return true;
        }
        if (!imgui_.Initialize(error))
            return false;

        imnodesContext_ = ImNodes::CreateContext();
        if (!imnodesContext_)
        {
            error = "ImNodes context creation failed.";
            imgui_.Shutdown();
            return false;
        }
        ImNodes::SetCurrentContext(imnodesContext_);
        editorContext_ = ImNodes::EditorContextCreate();
        if (!editorContext_)
        {
            error = "ImNodes editor context creation failed.";
            Shutdown();
            return false;
        }
        ImNodes::EditorContextSet(editorContext_);
        ConfigureStyle();
        initialized_ = true;
        error.clear();
        return true;
    }

    void RenegadeStoryFlowGraphEditor::Shutdown() noexcept
    {
        frameActive_ = false;
        initialized_ = false;
        if (imnodesContext_)
        {
            ImNodes::SetCurrentContext(imnodesContext_);
            if (editorContext_)
                ImNodes::EditorContextFree(editorContext_);
            editorContext_ = nullptr;
            ImNodes::DestroyContext(imnodesContext_);
            imnodesContext_ = nullptr;
        }
        imgui_.Shutdown();
    }

    void RenegadeStoryFlowGraphEditor::Bind(
        bridge::StoryFlowAuthoringSession* session,
        bridge::StoryFlowAuthoringModel* model,
        bridge::StoryFlowLayoutDocument* layout,
        RenegadeStoryFlowWorkspace* workspace)
    {
        session_ = session;
        model_ = model;
        layout_ = layout;
        workspace_ = workspace;
        pendingReconnectRouteId_.clear();
        linkStartedAtPin_ = 0;
        rejectNewLinkFromInput_ = false;
        observedUndoCount_ = static_cast<std::size_t>(-1);
        observedRedoCount_ = static_cast<std::size_t>(-1);
        bindingsDirty_ = true;
        initializedNodePositions_.clear();
    }

    void RenegadeStoryFlowGraphEditor::Clear() noexcept
    {
        session_ = nullptr;
        model_ = nullptr;
        layout_ = nullptr;
        workspace_ = nullptr;
        nodes_.clear();
        links_.clear();
        nodeIndexByStableId_.clear();
        nodeIndexByEditorId_.clear();
        outputsByPin_.clear();
        inputsByPin_.clear();
        linkIndexByEditorId_.clear();
        linkIdByRouteId_.clear();
        initializedNodePositions_.clear();
        pendingReconnectRouteId_.clear();
        linkStartedAtPin_ = 0;
        rejectNewLinkFromInput_ = false;
        bindingsDirty_ = true;
        frameActive_ = false;
        observedUndoCount_ = static_cast<std::size_t>(-1);
        observedRedoCount_ = static_cast<std::size_t>(-1);
    }

    void RenegadeStoryFlowGraphEditor::OnScreenOutcomeQuery(
        ScreenOutcomeQuery callback)
    {
        screenOutcomeQuery_ = std::move(callback);
        bindingsDirty_ = true;
    }

    void RenegadeStoryFlowGraphEditor::SetViewport(
        const XMFLOAT4& bounds) noexcept
    {
        viewport_.x = bounds.x;
        viewport_.y = bounds.y;
        viewport_.z = std::max(1.0f, bounds.z);
        viewport_.w = std::max(1.0f, bounds.w);
    }

    bool RenegadeStoryFlowGraphEditor::ContainsPointer(
        const XMFLOAT4& pointer) const noexcept
    {
        return pointer.x >= viewport_.x &&
            pointer.y >= viewport_.y &&
            pointer.x < viewport_.x + viewport_.z &&
            pointer.y < viewport_.y + viewport_.w;
    }

    void RenegadeStoryFlowGraphEditor::ConfigureStyle()
    {
        ImNodes::SetCurrentContext(imnodesContext_);
        ImNodes::EditorContextSet(editorContext_);
        ImNodes::StyleColorsDark();
        auto& style = ImNodes::GetStyle();
        style.GridSpacing = 28.0f;
        style.NodeCornerRounding = 4.0f;
        style.NodePadding = ImVec2(12.0f, 9.0f);
        style.NodeBorderThickness = 1.0f;
        style.LinkThickness = 2.5f;
        style.LinkHoverDistance = 10.0f;
        style.PinCircleRadius = 4.5f;
        style.PinHoverRadius = 10.0f;
        style.PinOffset = 1.0f;
        style.Colors[ImNodesCol_NodeBackground] = Surface2;
        style.Colors[ImNodesCol_NodeBackgroundHovered] = SurfaceHover;
        style.Colors[ImNodesCol_NodeBackgroundSelected] = SurfaceSelected;
        style.Colors[ImNodesCol_NodeOutline] = Border;
        style.Colors[ImNodesCol_TitleBar] = Surface0;
        style.Colors[ImNodesCol_TitleBarHovered] = SurfaceHover;
        style.Colors[ImNodesCol_TitleBarSelected] = SurfaceSelected;
        style.Colors[ImNodesCol_Link] = Link;
        style.Colors[ImNodesCol_LinkHovered] = LinkHover;
        style.Colors[ImNodesCol_LinkSelected] = Forge;
        style.Colors[ImNodesCol_Pin] = Forge;
        style.Colors[ImNodesCol_PinHovered] = LinkHover;
        style.Colors[ImNodesCol_BoxSelector] = IM_COL32(210, 91, 29, 38);
        style.Colors[ImNodesCol_BoxSelectorOutline] = Forge;
        style.Colors[ImNodesCol_GridBackground] = Surface0;
        style.Colors[ImNodesCol_GridLine] = BorderSoft;
    }

    const char* RenegadeStoryFlowGraphEditor::KindLabel(
        const bridge::FlowNodeKind kind) noexcept
    {
        switch (kind)
        {
        case bridge::FlowNodeKind::GameStart: return "GAME START";
        case bridge::FlowNodeKind::Level: return "LEVEL";
        case bridge::FlowNodeKind::Screen: return "SCREEN";
        case bridge::FlowNodeKind::CompleteGame: return "COMPLETE GAME";
        case bridge::FlowNodeKind::ReturnToMainMenu: return "RETURN TO MENU";
        case bridge::FlowNodeKind::Quit: return "QUIT";
        default: return "UNKNOWN";
        }
    }

    bool RenegadeStoryFlowGraphEditor::IsTerminalKind(
        const bridge::FlowNodeKind kind) noexcept
    {
        return kind == bridge::FlowNodeKind::CompleteGame ||
            kind == bridge::FlowNodeKind::ReturnToMainMenu ||
            kind == bridge::FlowNodeKind::Quit;
    }

    std::string RenegadeStoryFlowGraphEditor::PortLabel(
        const std::string& outcome)
    {
        if (outcome == bridge::GameStartOutcome)
            return "START";
        if (outcome == "next")
            return "NEXT";
        std::string result = outcome;
        std::transform(
            result.begin(), result.end(), result.begin(),
            [](const unsigned char c)
            {
                return static_cast<char>(std::toupper(c));
            });
        if (result.size() > 24)
        {
            result.resize(21);
            result += "...";
        }
        return result;
    }

    bool RenegadeStoryFlowGraphEditor::RefreshBindings(std::string& error)
    {
        nodes_.clear();
        links_.clear();
        nodeIndexByStableId_.clear();
        nodeIndexByEditorId_.clear();
        outputsByPin_.clear();
        inputsByPin_.clear();
        linkIndexByEditorId_.clear();
        linkIdByRouteId_.clear();
        initializedNodePositions_.clear();

        if (!session_ || !model_ || !layout_ ||
            !session_->IsLoaded() || !model_->IsLoaded())
        {
            bindingsDirty_ = false;
            error.clear();
            return true;
        }

        int nextNodeId = 1;
        int nextAttributeId = 100001;
        int nextLinkId = 500001;

        for (const auto& node : model_->Nodes())
        {
            NodeBinding binding;
            binding.nodeId = nextNodeId++;
            binding.staticAttributeId = nextAttributeId++;
            binding.stableId = node.id;
            if (node.kind != bridge::FlowNodeKind::GameStart)
            {
                binding.inputPinId = nextAttributeId++;
                inputsByPin_.emplace(
                    binding.inputPinId,
                    InputBinding{node.id});
            }

            std::vector<std::pair<std::string, bool>> outcomes;
            if (!IsTerminalKind(node.kind))
            {
                if (node.kind == bridge::FlowNodeKind::GameStart)
                {
                    outcomes.emplace_back(bridge::GameStartOutcome, true);
                }
                else if (node.kind == bridge::FlowNodeKind::Screen)
                {
                    std::vector<std::string> authored;
                    std::string queryError;
                    if (screenOutcomeQuery_ &&
                        screenOutcomeQuery_(node.id, authored, queryError))
                    {
                        for (const auto& action : authored)
                        {
                            if (std::none_of(
                                    outcomes.begin(), outcomes.end(),
                                    [&](const auto& item)
                                    {
                                        return item.first == action;
                                    }))
                            {
                                outcomes.emplace_back(action, true);
                            }
                        }
                    }

                    for (const auto& routeId : node.outgoingRouteIds)
                    {
                        const auto* route = model_->FindRoute(routeId);
                        if (!route) continue;
                        const auto found = std::find_if(
                            outcomes.begin(), outcomes.end(),
                            [&](const auto& item)
                            {
                                return item.first == route->outcome;
                            });
                        if (found == outcomes.end())
                            outcomes.emplace_back(route->outcome, false);
                    }
                }
                else
                {
                    outcomes.emplace_back("next", true);
                    for (const auto& routeId : node.outgoingRouteIds)
                    {
                        const auto* route = model_->FindRoute(routeId);
                        if (!route) continue;
                        if (std::none_of(
                                outcomes.begin(), outcomes.end(),
                                [&](const auto& item)
                                {
                                    return item.first == route->outcome;
                                }))
                        {
                            outcomes.emplace_back(route->outcome, true);
                        }
                    }
                }
            }

            for (const auto& [outcome, authorable] : outcomes)
            {
                OutputBinding output;
                output.pinId = nextAttributeId++;
                output.sourceNodeId = node.id;
                output.outcome = outcome;
                output.label = PortLabel(outcome);
                output.authorable = authorable;
                outputsByPin_.emplace(output.pinId, output);
                binding.outputs.push_back(std::move(output));
            }

            nodeIndexByStableId_.emplace(node.id, nodes_.size());
            nodeIndexByEditorId_.emplace(binding.nodeId, nodes_.size());
            nodes_.push_back(std::move(binding));
        }

        for (const auto& route : model_->Routes())
        {
            const auto sourceNode = nodeIndexByStableId_.find(route.sourceNodeId);
            const auto destinationNode = nodeIndexByStableId_.find(route.destinationNodeId);
            if (sourceNode == nodeIndexByStableId_.end() ||
                destinationNode == nodeIndexByStableId_.end())
            {
                error = "Graph binding omitted a valid route endpoint.";
                return false;
            }

            const auto& sourceBinding = nodes_[sourceNode->second];
            const auto& destinationBinding = nodes_[destinationNode->second];
            const auto output = std::find_if(
                sourceBinding.outputs.begin(), sourceBinding.outputs.end(),
                [&](const OutputBinding& item)
                {
                    return item.outcome == route.outcome;
                });
            if (output == sourceBinding.outputs.end() ||
                destinationBinding.inputPinId == 0)
            {
                error = "Graph binding could not map route pins for " + route.id + '.';
                return false;
            }

            LinkBinding link;
            link.linkId = nextLinkId++;
            link.routeId = route.id;
            link.outputPinId = output->pinId;
            link.inputPinId = destinationBinding.inputPinId;
            linkIndexByEditorId_.emplace(link.linkId, links_.size());
            linkIdByRouteId_.emplace(link.routeId, link.linkId);
            links_.push_back(std::move(link));
        }

        if (editorContext_)
        {
            ImNodes::SetCurrentContext(imnodesContext_);
            ImNodes::EditorContextSet(editorContext_);
            ImNodes::EditorContextResetPanning(
                ImVec2(layout_->canvas.panX, layout_->canvas.panY));
        }

        bindingsDirty_ = false;
        observedUndoCount_ = session_->UndoCount();
        observedRedoCount_ = session_->RedoCount();
        error.clear();
        return true;
    }

    void RenegadeStoryFlowGraphEditor::DrawNode(
        const bridge::StoryFlowNodeView& node,
        NodeBinding& binding)
    {
        if (initializedNodePositions_.insert(binding.nodeId).second)
        {
            if (const auto* position = workspace_->FindLayout(node.id))
            {
                ImNodes::SetNodeGridSpacePos(
                    binding.nodeId,
                    ImVec2(position->x, position->y));
            }
        }

        const bool start = node.kind == bridge::FlowNodeKind::GameStart;
        if (start)
        {
            ImNodes::PushColorStyle(ImNodesCol_NodeOutline, Forge);
            ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(37, 24, 18, 255));
        }

        ImNodes::BeginNode(binding.nodeId);
        ImNodes::BeginNodeTitleBar();
        ImGui::TextColored(
            start ? ToVec4(Forge) : ToVec4(Muted),
            "%s",
            KindLabel(node.kind));
        ImNodes::EndNodeTitleBar();

        if (binding.inputPinId != 0)
        {
            ImNodes::PushAttributeFlag(
                ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
            ImNodes::BeginInputAttribute(
                binding.inputPinId,
                ImNodesPinShape_CircleFilled);
            ImGui::TextColored(ToVec4(Muted), "IN");
            ImNodes::EndInputAttribute();
            ImNodes::PopAttributeFlag();
        }

        ImNodes::BeginStaticAttribute(binding.staticAttributeId);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 188.0f);
        ImGui::TextColored(ToVec4(TextStrong), "%s", node.name.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(188.0f, 1.0f));
        ImNodes::EndStaticAttribute();

        for (const auto& output : binding.outputs)
        {
            ImNodes::BeginOutputAttribute(
                output.pinId,
                ImNodesPinShape_CircleFilled);
            const ImVec4 color = output.authorable
                ? ToVec4(Text)
                : ToVec4(Warning);
            const std::string label = output.authorable
                ? output.label
                : "STALE // " + output.label;
            const float textWidth = ImGui::CalcTextSize(label.c_str()).x;
            ImGui::Indent(std::max(0.0f, 188.0f - textWidth));
            ImGui::TextColored(color, "%s", label.c_str());
            ImNodes::EndOutputAttribute();
        }

        ImNodes::EndNode();
        if (start)
        {
            ImNodes::PopColorStyle();
            ImNodes::PopColorStyle();
        }
    }

    void RenegadeStoryFlowGraphEditor::DrawLinks()
    {
        for (const auto& link : links_)
        {
            if (!pendingReconnectRouteId_.empty() &&
                link.routeId == pendingReconnectRouteId_)
            {
                continue;
            }
            ImNodes::Link(link.linkId, link.outputPinId, link.inputPinId);
        }
    }

    void RenegadeStoryFlowGraphEditor::DrawGraph()
    {
        ImGui::SetNextWindowPos(ImVec2(viewport_.x, viewport_.y));
        ImGui::SetNextWindowSize(ImVec2(viewport_.z, viewport_.w));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ToVec4(Surface0));

        constexpr ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus;
        ImGui::Begin("##RenegadeStoryFlowGraphCanvas", nullptr, flags);

        ImNodes::SetCurrentContext(imnodesContext_);
        ImNodes::EditorContextSet(editorContext_);
        ImNodes::BeginNodeEditor();

        for (auto& binding : nodes_)
        {
            const auto* node = model_->FindNode(binding.stableId);
            if (node)
                DrawNode(*node, binding);
        }
        DrawLinks();

        ImNodes::EndNodeEditor();
        ProcessInteractionEvents();
        SynchronizeSelection();
        ActivateHoveredNodeOnDoubleClick();
        SynchronizeLayout();

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    const bridge::FlowNode* RenegadeStoryFlowGraphEditor::FindDocumentNode(
        const bridge::StableId& nodeId) const noexcept
    {
        if (!session_ || !session_->IsLoaded()) return nullptr;
        const auto& nodes = session_->Document().nodes;
        const auto found = std::find_if(
            nodes.begin(), nodes.end(),
            [&](const bridge::FlowNode& node)
            {
                return node.id == nodeId;
            });
        return found == nodes.end() ? nullptr : &*found;
    }

    const bridge::FlowRoute* RenegadeStoryFlowGraphEditor::FindDocumentRoute(
        const bridge::StableId& routeId) const noexcept
    {
        if (!session_ || !session_->IsLoaded()) return nullptr;
        const auto& routes = session_->Document().routes;
        const auto found = std::find_if(
            routes.begin(), routes.end(),
            [&](const bridge::FlowRoute& route)
            {
                return route.id == routeId;
            });
        return found == routes.end() ? nullptr : &*found;
    }

    const RenegadeStoryFlowGraphEditor::OutputBinding*
    RenegadeStoryFlowGraphEditor::FindOutput(const int pinId) const noexcept
    {
        const auto found = outputsByPin_.find(pinId);
        return found == outputsByPin_.end() ? nullptr : &found->second;
    }

    const RenegadeStoryFlowGraphEditor::InputBinding*
    RenegadeStoryFlowGraphEditor::FindInput(const int pinId) const noexcept
    {
        const auto found = inputsByPin_.find(pinId);
        return found == inputsByPin_.end() ? nullptr : &found->second;
    }

    const RenegadeStoryFlowGraphEditor::LinkBinding*
    RenegadeStoryFlowGraphEditor::FindLink(const int linkId) const noexcept
    {
        const auto found = linkIndexByEditorId_.find(linkId);
        return found == linkIndexByEditorId_.end()
            ? nullptr
            : &links_[found->second];
    }

    const RenegadeStoryFlowGraphEditor::NodeBinding*
    RenegadeStoryFlowGraphEditor::FindNodeBinding(const int nodeId) const noexcept
    {
        const auto found = nodeIndexByEditorId_.find(nodeId);
        return found == nodeIndexByEditorId_.end()
            ? nullptr
            : &nodes_[found->second];
    }

    bool RenegadeStoryFlowGraphEditor::DecodeLinkPins(
        const int firstPin,
        const int secondPin,
        const OutputBinding*& output,
        const InputBinding*& input) const noexcept
    {
        output = FindOutput(firstPin);
        input = FindInput(secondPin);
        if (output && input)
            return true;
        output = FindOutput(secondPin);
        input = FindInput(firstPin);
        return output && input;
    }

    void RenegadeStoryFlowGraphEditor::SetStatus(std::string message)
    {
        if (workspace_)
            workspace_->SetStatus(std::move(message));
    }

    void RenegadeStoryFlowGraphEditor::MarkSemanticRefresh()
    {
        bindingsDirty_ = true;
        initializedNodePositions_.clear();
        if (workspace_ && !workspace_->RefreshPresentationAfterSemanticChange())
            SetStatus("GRAPH REFRESH FAILED // AUTHORITATIVE FLOW REMAINS IN SESSION");
    }

    bool RenegadeStoryFlowGraphEditor::CreateRoute(
        const OutputBinding& output,
        const InputBinding& input)
    {
        if (!session_ || !output.authorable)
        {
            SetStatus("LINK REJECTED // THIS OUTPUT IS A STALE SCREEN ACTION");
            return false;
        }
        const auto* source = FindDocumentNode(output.sourceNodeId);
        const auto* destination = FindDocumentNode(input.destinationNodeId);
        if (!source || !destination || IsTerminalKind(source->kind) ||
            destination->kind == bridge::FlowNodeKind::GameStart)
        {
            SetStatus("LINK REJECTED // INVALID STORY FLOW ENDPOINT");
            return false;
        }

        bridge::FlowRoute route;
        route.sourceNodeId = source->id;
        route.outcome = output.outcome;
        route.destinationNodeId = destination->id;
        if (destination->kind == bridge::FlowNodeKind::Level)
            route.destinationEntry = "player_entry";

        bridge::StableId createdRouteId;
        std::string error;
        if (!session_->AddRoute(std::move(route), createdRouteId, error))
        {
            SetStatus("LINK REJECTED // " + error);
            return false;
        }

        MarkSemanticRefresh();
        if (workspace_)
            workspace_->SelectRoute(createdRouteId);
        SetStatus("ROUTE CREATED // UNSAVED FLOW CHANGE");
        return true;
    }

    bool RenegadeStoryFlowGraphEditor::ReconnectRoute(
        const bridge::StableId& routeId,
        const InputBinding& input)
    {
        if (!session_)
            return false;
        const auto* current = FindDocumentRoute(routeId);
        const auto* destination = FindDocumentNode(input.destinationNodeId);
        if (!current || !destination ||
            destination->kind == bridge::FlowNodeKind::GameStart)
        {
            SetStatus("REWIRE CANCELLED // INVALID DESTINATION");
            return false;
        }

        bridge::FlowRoute replacement = *current;
        replacement.destinationNodeId = destination->id;
        if (destination->kind == bridge::FlowNodeKind::Level)
        {
            if (replacement.destinationEntry.empty())
                replacement.destinationEntry = "player_entry";
        }
        else
        {
            replacement.destinationEntry.clear();
        }

        std::string error;
        if (!session_->UpdateRoute(routeId, std::move(replacement), error))
        {
            SetStatus("REWIRE REJECTED // " + error);
            return false;
        }

        MarkSemanticRefresh();
        if (workspace_)
            workspace_->SelectRoute(routeId);
        SetStatus("ROUTE REWIRED // SAME STABLE ROUTE ID // UNSAVED");
        return true;
    }

    void RenegadeStoryFlowGraphEditor::ProcessInteractionEvents()
    {
        int startedPin = 0;
        if (ImNodes::IsLinkStarted(&startedPin))
        {
            linkStartedAtPin_ = startedPin;
            rejectNewLinkFromInput_ =
                FindInput(startedPin) != nullptr && pendingReconnectRouteId_.empty();
        }

        int destroyedLinkId = 0;
        if (ImNodes::IsLinkDestroyed(&destroyedLinkId))
        {
            if (const auto* link = FindLink(destroyedLinkId))
            {
                pendingReconnectRouteId_ = link->routeId;
                rejectNewLinkFromInput_ = false;
                SetStatus("REWIRE // DRAG THIS ROUTE END TO A NEW INPUT");
            }
        }

        int firstPin = 0;
        int secondPin = 0;
        bool createdFromSnap = false;
        const bool created = ImNodes::IsLinkCreated(
            &firstPin,
            &secondPin,
            &createdFromSnap);
        if (created)
        {
            const OutputBinding* output = nullptr;
            const InputBinding* input = nullptr;
            if (!DecodeLinkPins(firstPin, secondPin, output, input))
            {
                SetStatus("LINK REJECTED // ROUTES REQUIRE OUTPUT -> INPUT");
            }
            else if (!pendingReconnectRouteId_.empty())
            {
                const bridge::StableId reconnecting = pendingReconnectRouteId_;
                pendingReconnectRouteId_.clear();
                (void)ReconnectRoute(reconnecting, *input);
            }
            else if (rejectNewLinkFromInput_ || FindInput(linkStartedAtPin_) != nullptr)
            {
                SetStatus("LINK REJECTED // START NEW ROUTES FROM AN OUTPUT SOCKET");
            }
            else
            {
                (void)CreateRoute(*output, *input);
            }
            linkStartedAtPin_ = 0;
            rejectNewLinkFromInput_ = false;
        }

        int droppedFromPin = 0;
        if (ImNodes::IsLinkDropped(&droppedFromPin, true))
        {
            if (!pendingReconnectRouteId_.empty())
            {
                pendingReconnectRouteId_.clear();
                SetStatus("REWIRE CANCELLED // ORIGINAL ROUTE RESTORED");
            }
            linkStartedAtPin_ = 0;
            rejectNewLinkFromInput_ = false;
        }
    }

    void RenegadeStoryFlowGraphEditor::SynchronizeSelection()
    {
        if (!workspace_)
            return;

        const auto selectedRoute = linkIdByRouteId_.find(workspace_->SelectedRouteId());
        if (!workspace_->SelectedRouteId().empty() &&
            selectedRoute != linkIdByRouteId_.end() &&
            !ImNodes::IsLinkSelected(selectedRoute->second))
        {
            ImNodes::ClearLinkSelection();
            ImNodes::ClearNodeSelection();
            ImNodes::SelectLink(selectedRoute->second);
        }

        const int selectedLinks = ImNodes::NumSelectedLinks();
        if (selectedLinks > 0)
        {
            std::vector<int> ids(static_cast<std::size_t>(selectedLinks));
            ImNodes::GetSelectedLinks(ids.data());
            if (const auto* link = FindLink(ids.back()))
            {
                if (workspace_->SelectedRouteId() != link->routeId)
                    workspace_->SelectRoute(link->routeId);
            }
            return;
        }

        const int selectedNodes = ImNodes::NumSelectedNodes();
        if (selectedNodes > 0)
        {
            std::vector<int> ids(static_cast<std::size_t>(selectedNodes));
            ImNodes::GetSelectedNodes(ids.data());
            if (const auto* node = FindNodeBinding(ids.back()))
            {
                if (workspace_->SelectedNodeId() != node->stableId)
                    workspace_->SelectNode(node->stableId);
            }
        }
    }

    void RenegadeStoryFlowGraphEditor::DeleteSelectedLinks()
    {
        if (!session_ || !workspace_ ||
            !wi::input::Press(wi::input::KEYBOARD_BUTTON_DELETE))
        {
            return;
        }

        const int count = ImNodes::NumSelectedLinks();
        if (count <= 0)
            return;
        std::vector<int> selected(static_cast<std::size_t>(count));
        ImNodes::GetSelectedLinks(selected.data());

        bool deleted = false;
        std::string error;
        for (const int editorLinkId : selected)
        {
            const auto* link = FindLink(editorLinkId);
            if (!link)
                continue;
            if (!session_->DeleteRoute(link->routeId, error))
            {
                SetStatus("ROUTE DELETE REJECTED // " + error);
                return;
            }
            deleted = true;
        }
        if (!deleted)
            return;

        pendingReconnectRouteId_.clear();
        workspace_->selectedRouteId_.clear();
        MarkSemanticRefresh();
        SetStatus("ROUTE DELETED // UNDO RESTORES IT");
    }

    void RenegadeStoryFlowGraphEditor::ActivateHoveredNodeOnDoubleClick()
    {
        if (!workspace_ || !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            return;
        int hoveredNodeId = 0;
        if (!ImNodes::IsNodeHovered(&hoveredNodeId))
            return;
        const auto* binding = FindNodeBinding(hoveredNodeId);
        const auto* node = binding ? model_->FindNode(binding->stableId) : nullptr;
        if (!node)
            return;
        if ((node->kind == bridge::FlowNodeKind::Level ||
             node->kind == bridge::FlowNodeKind::Screen) &&
            workspace_->nodeActivated_)
        {
            workspace_->nodeActivated_(node->id);
        }
    }

    void RenegadeStoryFlowGraphEditor::SynchronizeLayout()
    {
        if (!layout_ || !workspace_)
            return;
        bool changed = false;
        for (const auto& binding : nodes_)
        {
            auto* layout = workspace_->FindLayout(binding.stableId);
            if (!layout)
                continue;
            const ImVec2 position = ImNodes::GetNodeGridSpacePos(binding.nodeId);
            if (!AlmostEqual(layout->x, position.x) ||
                !AlmostEqual(layout->y, position.y))
            {
                layout->x = position.x;
                layout->y = position.y;
                changed = true;
            }
        }

        const ImVec2 panning = ImNodes::EditorContextGetPanning();
        if (!AlmostEqual(layout_->canvas.panX, panning.x) ||
            !AlmostEqual(layout_->canvas.panY, panning.y))
        {
            layout_->canvas.panX = panning.x;
            layout_->canvas.panY = panning.y;
            changed = true;
        }
        if (changed)
            workspace_->NotifyLayoutChanged();
    }

    void RenegadeStoryFlowGraphEditor::Update(
        const wi::Canvas& canvas,
        const float dt,
        const bool active)
    {
        frameActive_ = false;
        if (!active || !session_ || !model_ || !layout_ || !workspace_ ||
            !session_->IsLoaded() || !model_->IsLoaded())
        {
            if (workspace_)
                workspace_->connectButton_.SetVisible(false);
            return;
        }

        std::string error;
        if (!Initialize(error))
        {
            SetStatus("GRAPH EDITOR FAILED // " + error);
            return;
        }
        ImNodes::SetCurrentContext(imnodesContext_);
        ImNodes::EditorContextSet(editorContext_);

        if (session_->UndoCount() != observedUndoCount_ ||
            session_->RedoCount() != observedRedoCount_)
        {
            bindingsDirty_ = true;
        }
        if (bindingsDirty_ && !RefreshBindings(error))
        {
            SetStatus("GRAPH BINDING FAILED // " + error);
            return;
        }

        // Direct socket linking supersedes the legacy CONNECT button and closes
        // the Gate 9B layout collision instead of moving the old state machine.
        workspace_->connectButton_.SetVisible(false);

        const bool acceptMouse = ContainsPointer(wi::input::GetPointer());
        if (!imgui_.BeginFrame(canvas, dt, acceptMouse, error))
        {
            SetStatus("GRAPH FRAME FAILED // " + error);
            return;
        }

        DrawGraph();
        DeleteSelectedLinks();
        imgui_.EndFrame();
        frameActive_ = true;
    }

    void RenegadeStoryFlowGraphEditor::Render(
        const wi::graphics::CommandList cmd) const
    {
        if (frameActive_)
            imgui_.Render(cmd);
    }
}
