#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace renegade::bridge
{
    enum class DependencyClass
    {
        ProjectDocument,
        Scene,
        ImportedContent,
        Texture,
        Audio,
        Video,
        Font,
        Script,
        Data,
        GeneratedData,
        RuntimeSupport,
    };

    enum class DependencyRequirement { Required, Optional, EditorOnly };
    enum class DependencyDiagnosticCode
    {
        Missing,
        OutsideProject,
        Duplicate,
        CaseCollision,
        UndeclaredComputedReference,
    };

    struct DependencyNode
    {
        std::string id;
        std::string projectRelativePath;
        DependencyClass dependencyClass = DependencyClass::Data;
        DependencyRequirement requirement = DependencyRequirement::Required;
        std::string applicability = "windows-x64";
        std::string provider;
        std::uint32_t providerVersion = 1;
        std::string contentHash;
        bool runtimeSupport = false;
    };

    struct DependencyEdge
    {
        std::string sourceId;
        std::string targetId;
        std::string provenance;
    };

    struct DependencyDiagnostic
    {
        DependencyDiagnosticCode code = DependencyDiagnosticCode::Missing;
        std::string sourceId;
        std::string path;
        std::string message;
    };

    struct DependencyGraph
    {
        std::vector<std::string> rootIds;
        std::vector<DependencyNode> nodes;
        std::vector<DependencyEdge> edges;
        std::vector<DependencyDiagnostic> diagnostics;
    };

    struct DependencyPathResult
    {
        bool accepted = false;
        bool exists = false;
        std::string canonicalRelativePath;
        std::string absolutePath;
        std::string error;
    };

    struct DependencyPathRegistration
    {
        bool inserted = false;
        std::vector<DependencyDiagnostic> diagnostics;
    };

    class DependencyPathRegistry
    {
    public:
        [[nodiscard]] DependencyPathRegistration Register(
            const std::string& sourceId,
            const std::string& canonicalRelativePath);

    private:
        std::map<std::string, std::string> pathsByFoldedName_;
    };

    struct DependencyRoot
    {
        std::string declaredPath;
        DependencyClass dependencyClass = DependencyClass::ProjectDocument;
        DependencyRequirement requirement = DependencyRequirement::Required;
        std::string provenance;
    };

    struct DependencyCandidate
    {
        std::string declaredPath;
        DependencyClass dependencyClass = DependencyClass::Data;
        DependencyRequirement requirement = DependencyRequirement::Required;
        std::string provenance;
        bool runtimeSupport = false;
    };

    struct DependencyProviderContext
    {
        std::string projectRoot;
        const DependencyNode* source = nullptr;
    };

    using DependencyCandidateSink =
        std::function<void(const DependencyCandidate&)>;

    class IDependencyProvider
    {
    public:
        virtual ~IDependencyProvider() = default;
        [[nodiscard]] virtual const char* Name() const noexcept = 0;
        [[nodiscard]] virtual std::uint32_t Version() const noexcept = 0;
        [[nodiscard]] virtual bool Supports(
            DependencyClass dependencyClass) const noexcept = 0;
        [[nodiscard]] virtual bool Discover(
            const DependencyProviderContext& context,
            const DependencyCandidateSink& emit,
            std::string& error) const = 0;
    };

    // Gate 2 collector: owns graph-root admission and provider dispatch only.
    // Later LP05 gates add concrete providers and transitive traversal without
    // changing this UI-free contract. Providers are borrowed and must outlive
    // the collector.
    class DependencyCollector
    {
    public:
        explicit DependencyCollector(std::string projectRoot);

        [[nodiscard]] bool RegisterProvider(
            const IDependencyProvider& provider,
            std::string& error);
        [[nodiscard]] bool AddRoot(
            const DependencyRoot& root,
            std::string& error);
        [[nodiscard]] bool DiscoverRootDependencies(std::string& error);
        [[nodiscard]] const DependencyGraph& Graph() const noexcept;

    private:
        void AcceptCandidate(
            const DependencyNode& source,
            const IDependencyProvider& provider,
            const DependencyCandidate& candidate);

        std::string projectRoot_;
        DependencyGraph graph_;
        DependencyPathRegistry pathRegistry_;
        std::map<std::string, const IDependencyProvider*> providers_;
    };

    // Resolves a declared dependency against projectRoot without requiring it
    // to exist. The lexical and weak-canonical checks prevent traversal and
    // symlink escape; existence is reported separately for graph diagnostics.
    [[nodiscard]] DependencyPathResult ResolveDependencyPath(
        const std::string& projectRoot,
        const std::string& declaredPath);
}
