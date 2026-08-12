#pragma once

#include "renegade/bridge/DependencyService.h"
#include "renegade/bridge/IdentityService.h"

namespace renegade::bridge
{
    // Gate 6 projects persistent reusable-instance stable IDs into the same
    // LP05 graph used by LC01 and LP06. It supports Scene so a saved WISCENE
    // can declare its governed .rasset product, and ImportedContent so the
    // current .rasset payload can expose its ordinary WISCENE resources.
    class ReusableAssetDependencyProvider final : public IDependencyProvider
    {
    public:
        explicit ReusableAssetDependencyProvider(StableId projectId);

        [[nodiscard]] const char* Name() const noexcept override;
        [[nodiscard]] std::uint32_t Version() const noexcept override;
        [[nodiscard]] bool Supports(
            DependencyClass dependencyClass) const noexcept override;
        [[nodiscard]] bool Discover(
            const DependencyProviderContext& context,
            const DependencyCandidateSink& emit,
            const DependencyDiagnosticSink& diagnose,
            std::string& error) const override;

    private:
        StableId projectId_;
    };
}
