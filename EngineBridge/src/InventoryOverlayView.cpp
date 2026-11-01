#include "renegade/bridge/InventoryOverlayView.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace renegade::bridge {
namespace {
std::string BoundedText(std::string value, std::size_t maximum) {
    for (char& ch : value) {
        const unsigned char c=static_cast<unsigned char>(ch);
        if (c<32 || c==127) ch=' ';
    }
    if (value.size()>maximum) value.resize(maximum);
    return value;
}
}
InventoryOverlayView BuildInventoryOverlayView(
    const InventoryService& inventory,
    const std::vector<std::pair<std::string,std::string>>& requested,
    std::size_t maximumRowsPerContainer) {
    InventoryOverlayView view;
    view.revision=inventory.Revision();
    const auto limit=std::min<std::size_t>(maximumRowsPerContainer,24);
    for (const auto& request:requested) {
        if (view.panels.size()==2) break; // 2-column WIP presentation budget.
        const auto* container=inventory.FindContainer(request.first);
        if (!container) continue;
        InventoryContainerView panel;
        panel.containerId=container->id;
        panel.title=BoundedText(request.second.empty()?container->id:request.second,42);
        panel.usedSlots=static_cast<std::uint32_t>(container->stacks.size());
        panel.maxSlots=container->maximumSlots;
        panel.maxWeight=container->maximumWeight;
        panel.truncated=container->stacks.size()>limit;
        panel.rows.reserve(std::min(container->stacks.size(),limit));
        for (const auto& stack:container->stacks) {
            const auto* def=inventory.FindDefinition(stack.definitionId);
            if (!def) continue;
            panel.usedWeight+=static_cast<double>(stack.quantity)*def->weightPerUnit;
            if (panel.rows.size()<limit)
                panel.rows.push_back({stack.instanceId,
                    BoundedText(def->name,36),stack.quantity,def->maximumStack,
                    stack.definitionId});
        }
        // Never render an invalid infinity/NaN from corrupt external data.
        if (!std::isfinite(panel.usedWeight)) panel.usedWeight=0;
        view.panels.push_back(std::move(panel));
    }
    return view;
}
} // namespace renegade::bridge
