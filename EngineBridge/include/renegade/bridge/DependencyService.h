#pragma once

#include <cstdint>
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

    // Resolves a declared dependency against projectRoot without requiring it
    // to exist. The lexical and weak-canonical checks prevent traversal and
    // symlink escape; existence is reported separately for graph diagnostics.
    [[nodiscard]] DependencyPathResult ResolveDependencyPath(
        const std::string& projectRoot,
        const std::string& declaredPath);
}

