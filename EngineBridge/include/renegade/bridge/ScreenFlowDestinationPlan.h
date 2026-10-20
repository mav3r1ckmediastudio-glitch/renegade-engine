#pragma once

#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/ScreenService.h"

#include <algorithm>
#include <string>
#include <utility>

namespace renegade::bridge
{
    // UI-024: pure, pre-commit plan. StoryFlow remains the only route authority.
    // No disk writes and no mutation to the caller's documents occur here.
    struct ScreenFlowDestinationPlan
    {
        bool succeeded = false;
        bool noChanges = false;
        bool updatingExisting = false;
        std::string message;
        StableId routeId;
        std::string actionId;
        FlowDocument candidateFlow;
    };

    [[nodiscard]] inline ScreenFlowDestinationPlan PlanButtonOpenScreen(
        const ScreenDocument& screen,
        const FlowDocument& flow,
        const StableId& sourceScreenNodeId,
        const StableId& buttonId,
        const StableId& destinationScreenNodeId)
    {
        ScreenFlowDestinationPlan plan;
        const StableId& projectId = screen.envelope.projectId;
        std::string error;
        if (!ValidateScreenDocument(screen, projectId, error) ||
            !ValidateFlowDocument(flow, projectId, error) ||
            flow.envelope.projectId != projectId)
        {
            plan.message = "A valid same-project Screen and Flow are required: " + error;
            return plan;
        }

        const auto source = std::find_if(flow.nodes.begin(), flow.nodes.end(),
            [&](const FlowNode& node)
            {
                return node.id == sourceScreenNodeId &&
                    node.kind == FlowNodeKind::Screen &&
                    node.screenDocumentId == screen.envelope.documentId;
            });
        const auto target = std::find_if(flow.nodes.begin(), flow.nodes.end(),
            [&](const FlowNode& node)
            {
                return node.id == destinationScreenNodeId &&
                    node.kind != FlowNodeKind::GameStart;
            });
        if (source == flow.nodes.end() || target == flow.nodes.end())
        {
            plan.message = "Open Screen requires an existing source Screen and target Screen in this StoryFlow.";
            return plan;
        }

        const auto button = std::find_if(screen.widgets.begin(), screen.widgets.end(),
            [&](const ScreenWidget& widget)
            {
                return widget.id == buttonId && widget.kind == ScreenWidgetKind::Button;
            });
        if (button == screen.widgets.end())
        {
            plan.message = "The selected element is not a Button in the source Screen.";
            return plan;
        }
        if (button->localOnly)
        {
            plan.message = "This Button is Local Only. Enable Local + Flow in its Interactions Inspector before assigning a StoryFlow destination.";
            return plan;
        }
        if(IsRuntimeUiAccessibilityAction(button->actionId))
        {
            plan.message = "This Button changes player UI accessibility settings and does not create a StoryFlow route.";
            return plan;
        }
        plan.actionId = button->actionId;
        const bool declared = std::any_of(screen.actions.begin(), screen.actions.end(),
            [&](const ScreenAction& action) { return action.id == plan.actionId; });
        if (!declared || plan.actionId.empty())
        {
            plan.message = "Button action has no declared Screen outcome.";
            return plan;
        }

        // Shared action identity routes ALL such Buttons. Never silently repoint
        // more than the selected Button through this simple destination picker.
        const auto shared = std::count_if(screen.widgets.begin(), screen.widgets.end(),
            [&](const ScreenWidget& widget)
            {
                return widget.kind == ScreenWidgetKind::Button &&
                    widget.actionId == plan.actionId;
            });
        if (shared != 1)
        {
            plan.message = "This Screen action is shared by multiple Buttons; edit the shared routing explicitly in StoryFlow.";
            return plan;
        }

        plan.candidateFlow = flow;
        FlowRoute* existingRoute = nullptr;
        for (auto& route : plan.candidateFlow.routes)
        {
            if (route.sourceNodeId != sourceScreenNodeId ||
                route.outcome != plan.actionId) continue;
            if (existingRoute != nullptr)
            {
                plan.message = "Multiple StoryFlow routes use this action; edit conditional routing in the Graph.";
                return plan;
            }
            existingRoute = &route;
        }

        const std::string targetEntry=target->kind==FlowNodeKind::Level
            ? (existingRoute && existingRoute->destinationNodeId==destinationScreenNodeId && !existingRoute->destinationEntry.empty()
                ? existingRoute->destinationEntry : "player_entry") : "";
        if (existingRoute != nullptr)
        {
            if (!existingRoute->conditions.empty())
            {
                plan.message = "This action uses conditional routing; edit its destination in the StoryFlow Graph.";
                return plan;
            }
            plan.routeId = existingRoute->id;
            plan.updatingExisting = true;
            plan.noChanges =
                existingRoute->destinationNodeId == destinationScreenNodeId &&
                existingRoute->destinationEntry == targetEntry;
            if (!plan.noChanges)
            {
                existingRoute->destinationNodeId = destinationScreenNodeId;
                existingRoute->destinationEntry = targetEntry;
            }
        }
        else
        {
            FlowRoute created;
            created.id = GenerateStableId();
            created.sourceNodeId = sourceScreenNodeId;
            created.outcome = plan.actionId;
            created.destinationNodeId = destinationScreenNodeId;
            if(target->kind==FlowNodeKind::Level)created.destinationEntry="player_entry";
            plan.routeId = created.id;
            plan.candidateFlow.routes.push_back(std::move(created));
        }

        if (!ValidateFlowDocument(plan.candidateFlow, projectId, error))
        {
            plan.message = "The requested StoryFlow route would be invalid: " + error;
            return plan;
        }
        plan.succeeded = true;
        plan.message = plan.noChanges
            ? "The selected Button already opens this Screen."
            : "Validated StoryFlow route plan ready for one Flow-authoring transaction.";
        return plan;
    }
}
