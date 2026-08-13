#pragma once

#include "renegade/bridge/DependencyService.h"
#include "renegade/bridge/IdentityService.h"

namespace renegade::bridge
{
    // Gate 5 projects durable material texture stable IDs into the existing
    // LP05/LC01 dependency graph. The governed resource product is the Runtime
    // dependency; its retained SourceAssets input remains editor-only freshness
    // authority and is added later by the normal owner-build freshness seam.
    class ResourceAssetDependencyProvider final : public IDependencyProvider
    {
    public:
        explicit ResourceAssetDependencyProvider(StableId projectId);

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
