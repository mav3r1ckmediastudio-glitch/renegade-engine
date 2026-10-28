#pragma once

#include "renegade/bridge/DependencyService.h"

namespace renegade::bridge {

// Optional Scene companion discovery for Build Game. Other projects remain
// unchanged; never treat authored inventory state as a mutable game-save slot.
class InventoryDependencyProvider final : public IDependencyProvider {
public:
    [[nodiscard]] const char* Name() const noexcept override {
        return "inventory-scene-companions";
    }
    [[nodiscard]] std::uint32_t Version() const noexcept override { return 1; }
    [[nodiscard]] bool Supports(DependencyClass dependencyClass) const noexcept override {
        return dependencyClass == DependencyClass::Scene;
    }
    [[nodiscard]] bool Discover(const DependencyProviderContext& context,
        const DependencyCandidateSink& emit,
        const DependencyDiagnosticSink& diagnose,
        std::string& error) const override;
};

} // namespace renegade::bridge
