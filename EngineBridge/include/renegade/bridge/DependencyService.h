#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace wi::scene
{
    struct Scene;
}

namespace renegade::bridge
{
    enum class DependencyClass
    {
        ProjectDocument,
        StoryFlowDocument,
        RuntimeScreenDocument,
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
        std::string existingCanonicalRelativePath;
        std::vector<DependencyDiagnostic> diagnostics;
    };

    class DependencyPathRegistry
    {
    public:
        [[nodiscard]] DependencyPathRegistration Register(
            const std::string& sourceId,
            const std::string& canonicalRelativePath);

    private:
        // O(n) intentionally: do not replace Windows Unicode ordinal comparison with byte-folded keys.
        std::vector<std::string> registeredPaths_;
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

    struct ProjectDependencyDocument
    {
        std::string projectId;
        std::string startupScene;
        std::string startupFlow;
        std::string startupScreen;
        // Gate 5: paths the project descriptor declares must remain in the
        // dependency closure regardless of whether any scene, flow or screen
        // document currently references them (e.g. a fallback texture or a
        // required runtime font). Each entry carries its own declared class,
        // matching every other typed provider in this file -- "always
        // include" is a declared project-level fact about a specific kind of
        // asset, never inferred from a path's extension. Each entry becomes
        // its own graph root candidate with DependencyRequirement::Required,
        // the same way startupScene/startupFlow/startupScreen already do;
        // there is no separate provider type for this.
        std::vector<DependencyCandidate> alwaysInclude;
    };

    struct StoryFlowDependencyDocument
    {
        std::string projectId;
        std::vector<std::string> scenePathHints;
    };

    struct RuntimeScreenDependencyDocument
    {
        std::string projectId;
        std::vector<std::string> imagePaths;
        std::vector<std::string> fontPaths;
    };

    struct WisceneDependencyDocument
    {
        std::vector<DependencyCandidate> references;
        // Generated terrain payloads such as sculpted height samples and
        // blend maps are serialized inside the owning WISCENE. They are
        // therefore evidence carried by the scene root, not separate path
        // dependencies. Recording them explicitly prevents the collector
        // from either overlooking generated data or inventing fake files.
        struct EmbeddedGeneratedData
        {
            std::string provenance;
            std::uint64_t byteCount = 0;
        };
        std::vector<EmbeddedGeneratedData> embeddedGeneratedData;
    };

    using ProjectDependencyReader = std::function<bool(
        const std::string&, ProjectDependencyDocument&, std::string&)>;
    using StoryFlowDependencyReader = std::function<bool(
        const std::string&, StoryFlowDependencyDocument&, std::string&)>;
    using RuntimeScreenDependencyReader = std::function<bool(
        const std::string&, RuntimeScreenDependencyDocument&, std::string&)>;
    using WisceneDependencyReader = std::function<bool(
        const std::string&, WisceneDependencyDocument&, std::string&)>;

    // Public typed-walker seam. It is const and performs no loads, writes or
    // renderer updates; the provider contract and output remain Wicked-free.
    void InspectWisceneDependencies(
        const wi::scene::Scene& scene,
        WisceneDependencyDocument& document);

    // Production adapters use the existing validated document services. They
    // are factories rather than hard dependencies in the providers so the
    // extraction policy remains testable without a renderer or UI.
    [[nodiscard]] ProjectDependencyReader MakeProjectDependencyReader();
    [[nodiscard]] StoryFlowDependencyReader MakeStoryFlowDependencyReader(
        std::string expectedProjectId);
    [[nodiscard]] RuntimeScreenDependencyReader MakeRuntimeScreenDependencyReader(
        std::string expectedProjectId);
    [[nodiscard]] WisceneDependencyReader MakeWisceneDependencyReader();

    // Gate 3 document providers consume typed, validated document views. The
    // reader seam lets production bind ProjectService/FlowService/ScreenService
    // while tests remain independent of Wicked and wi::config.
    class ProjectDependencyProvider final : public IDependencyProvider
    {
    public:
        explicit ProjectDependencyProvider(ProjectDependencyReader reader);
        [[nodiscard]] const char* Name() const noexcept override;
        [[nodiscard]] std::uint32_t Version() const noexcept override;
        [[nodiscard]] bool Supports(DependencyClass dependencyClass) const noexcept override;
        [[nodiscard]] bool Discover(const DependencyProviderContext& context,
            const DependencyCandidateSink& emit, std::string& error) const override;
    private:
        ProjectDependencyReader reader_;
    };

    class StoryFlowDependencyProvider final : public IDependencyProvider
    {
    public:
        explicit StoryFlowDependencyProvider(StoryFlowDependencyReader reader);
        [[nodiscard]] const char* Name() const noexcept override;
        [[nodiscard]] std::uint32_t Version() const noexcept override;
        [[nodiscard]] bool Supports(DependencyClass dependencyClass) const noexcept override;
        [[nodiscard]] bool Discover(const DependencyProviderContext& context,
            const DependencyCandidateSink& emit, std::string& error) const override;
    private:
        StoryFlowDependencyReader reader_;
    };

    class RuntimeScreenDependencyProvider final : public IDependencyProvider
    {
    public:
        explicit RuntimeScreenDependencyProvider(RuntimeScreenDependencyReader reader);
        [[nodiscard]] const char* Name() const noexcept override;
        [[nodiscard]] std::uint32_t Version() const noexcept override;
        [[nodiscard]] bool Supports(DependencyClass dependencyClass) const noexcept override;
        [[nodiscard]] bool Discover(const DependencyProviderContext& context,
            const DependencyCandidateSink& emit, std::string& error) const override;
    private:
        RuntimeScreenDependencyReader reader_;
    };

    // Gate 4 loads a WISCENE through Renegade's validated scene-document seam
    // and walks only authoritative resource fields on public Wicked component
    // structures. The reader keeps Wicked types out of this provider contract.
    class WisceneDependencyProvider final : public IDependencyProvider
    {
    public:
        explicit WisceneDependencyProvider(WisceneDependencyReader reader);
        [[nodiscard]] const char* Name() const noexcept override;
        [[nodiscard]] std::uint32_t Version() const noexcept override;
        [[nodiscard]] bool Supports(DependencyClass dependencyClass) const noexcept override;
        [[nodiscard]] bool Discover(const DependencyProviderContext& context,
            const DependencyCandidateSink& emit, std::string& error) const override;
    private:
        WisceneDependencyReader reader_;
    };

    struct DeclaredDependencyReference
    {
        std::string sourcePath;
        DependencyCandidate candidate;
    };

    // Gate 5: a raw, un-imported .gltf/.glb source file sitting in the
    // project as its own dependency-bearing document. This is distinct from
    // ImportService, which bakes GLTF content into an embedded Wicked scene
    // at import time and never persists the source's own external resource
    // paths -- once a WISCENE exists, Gate 4's typed walker already covers
    // its baked-in resources. This provider covers the other case: the
    // source file itself, before or independent of any import, declaring
    // its own external buffer (.bin) and image dependencies per the glTF
    // spec. It reads the glTF/GLB JSON structure directly (nlohmann::json,
    // already vendored via WickedEngine/Editor/tiny_gltf.h and linked into
    // this library through ModelImporter_GLTF.cpp) rather than using
    // tinygltf's own Model-loading API, because that API treats a missing
    // external .bin as a hard parse failure -- the opposite of this
    // project's "missing declared dependency is a diagnostic, never a load
    // failure" contract that every other provider follows via
    // ResolveDependencyPath's own independent existence check.
    struct GltfDependencyDocument
    {
        std::vector<DependencyCandidate> references;
        // Structural evidence for the representative Gate 5 fixture. The
        // external dependency closure still comes only from buffer/image
        // URIs, while these counts prove that those resources are consumed
        // by material texture slots and animation content.
        std::uint32_t materialTextureSlotCount = 0;
        std::uint32_t animationCount = 0;
    };

    using GltfDependencyReader = std::function<bool(
        const std::string&, GltfDependencyDocument&, std::string&)>;

    [[nodiscard]] GltfDependencyReader MakeGltfDependencyReader();

    class GltfDependencyProvider final : public IDependencyProvider
    {
    public:
        explicit GltfDependencyProvider(GltfDependencyReader reader);
        [[nodiscard]] const char* Name() const noexcept override;
        [[nodiscard]] std::uint32_t Version() const noexcept override;
        [[nodiscard]] bool Supports(DependencyClass dependencyClass) const noexcept override;
        [[nodiscard]] bool Discover(const DependencyProviderContext& context,
            const DependencyCandidateSink& emit, std::string& error) const override;
    private:
        GltfDependencyReader reader_;
    };

    class DeclaredReferenceDependencyProvider final : public IDependencyProvider
    {
    public:
        explicit DeclaredReferenceDependencyProvider(
            std::vector<DeclaredDependencyReference> references);
        [[nodiscard]] const char* Name() const noexcept override;
        [[nodiscard]] std::uint32_t Version() const noexcept override;
        [[nodiscard]] bool Supports(DependencyClass dependencyClass) const noexcept override;
        [[nodiscard]] bool Discover(const DependencyProviderContext& context,
            const DependencyCandidateSink& emit, std::string& error) const override;
    private:
        std::vector<DeclaredDependencyReference> references_;
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

    // Gate 5: a stable, lowercase snake_case textual name for each
    // DependencyClass, used only where a class must be declared in a
    // human-authored text format (the project descriptor's "always_include"
    // entries). This is not a general string<->class inference mechanism;
    // callers still declare a type, they simply declare it by name in a text
    // file instead of by constructing an enum value directly in code.
    [[nodiscard]] const char* DependencyClassName(
        DependencyClass dependencyClass) noexcept;
    [[nodiscard]] bool TryParseDependencyClassName(
        const std::string& name, DependencyClass& dependencyClass) noexcept;
}
