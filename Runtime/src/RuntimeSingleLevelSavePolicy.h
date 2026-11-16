#pragma once
#include "renegade/bridge/FlowService.h"
namespace renegade::runtime
{
// Reset can restore this exact authored level without inventing a Flow cursor.
// Menus, branches and multiple levels still require a full resume journal.
inline bool SupportsSingleLevelQuickSave(const bridge::FlowDocument &document, const bridge::FlowNode *current)
{
    using namespace bridge;
    if (!current || current->kind != FlowNodeKind::Level || document.nodes.size() != 3 || document.routes.size() != 2)
        return false;
    const FlowNode *start = nullptr, *level = nullptr, *complete = nullptr;
    for (const auto &node : document.nodes)
    {
        switch (node.kind)
        {
        case FlowNodeKind::GameStart:
            if (start)
                return false;
            start = &node;
            break;
        case FlowNodeKind::Level:
            if (level)
                return false;
            level = &node;
            break;
        case FlowNodeKind::CompleteGame:
            if (complete)
                return false;
            complete = &node;
            break;
        default:
            return false;
        }
    }
    if (!start || !level || !complete || document.startNodeId != start->id || current->id != level->id)
        return false;
    bool enters = false, finishes = false;
    for (const auto &route : document.routes)
    {
        if (!route.conditions.empty())
            return false;
        if (route.sourceNodeId == start->id && route.destinationNodeId == level->id &&
            route.outcome == GameStartOutcome && !enters)
            enters = true;
        else if (route.sourceNodeId == level->id && route.destinationNodeId == complete->id && !route.outcome.empty() &&
                 !finishes)
            finishes = true;
        else
            return false;
    }
    return enters && finishes;
}
} // namespace renegade::runtime
