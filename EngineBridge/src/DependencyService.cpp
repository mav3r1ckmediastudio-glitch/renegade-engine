#include "renegade/bridge/DependencyService.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace renegade::bridge
{
    namespace
    {
        bool IsWithin(const std::filesystem::path& root,
                      const std::filesystem::path& candidate)
        {
            auto rootPart = root.begin();
            auto candidatePart = candidate.begin();
            for (; rootPart != root.end(); ++rootPart, ++candidatePart)
            {
                if (candidatePart == candidate.end() || *rootPart != *candidatePart)
                    return false;
            }
            return true;
        }

        std::string FoldPathCase(std::string path)
        {
            std::transform(path.begin(), path.end(), path.begin(),
                [](unsigned char character) {
                    return static_cast<char>(std::tolower(character));
                });
            return path;
        }

        std::string StablePathId(const std::string& path)
        {
            std::uint64_t hash = 1469598103934665603ull;
            for (const unsigned char value : path)
            {
                hash ^= value;
                hash *= 1099511628211ull;
            }
            std::ostringstream stream;
            stream << "asset:" << std::hex << std::setfill('0')
                   << std::setw(16) << hash;
            return stream.str();
        }

        DependencyDiagnostic PathDiagnostic(
            const DependencyDiagnosticCode code,
            const std::string& sourceId,
            const std::string& path,
            const std::string& message)
        {
            return {code, sourceId, path, message};
        }
    }

    DependencyPathResult ResolveDependencyPath(
        const std::string& projectRoot,
        const std::string& declaredPath)
    {
        DependencyPathResult result;
        if (projectRoot.empty() || declaredPath.empty())
        {
            result.error = "Project root and dependency path are required.";
            return result;
        }

        std::error_code error;
        const auto root = std::filesystem::weakly_canonical(projectRoot, error);
        if (error || !std::filesystem::is_directory(root, error))
        {
            result.error = "Project root does not resolve to a directory.";
            return result;
        }

        const std::filesystem::path declared(declaredPath);
        if (declared.is_absolute())
        {
            result.error = "Absolute dependency paths are outside the project.";
            return result;
        }

        const auto lexical = (root / declared).lexically_normal();
        if (!IsWithin(root, lexical))
        {
            result.error = "Dependency path escapes the project root.";
            return result;
        }

        const auto resolved = std::filesystem::weakly_canonical(lexical, error);
        if (error || !IsWithin(root, resolved))
        {
            result.error = "Dependency resolves outside the project root.";
            return result;
        }

        result.accepted = true;
        result.exists = std::filesystem::is_regular_file(resolved, error) && !error;
        result.absolutePath = resolved.generic_string();
        result.canonicalRelativePath =
            std::filesystem::relative(resolved, root, error).generic_string();
        if (error)
        {
            result = {};
            result.error = "Dependency path could not be made project-relative.";
        }
        return result;
    }

    DependencyPathRegistration DependencyPathRegistry::Register(
        const std::string& sourceId,
        const std::string& canonicalRelativePath)
    {
        DependencyPathRegistration result;
        const auto folded = FoldPathCase(canonicalRelativePath);
        const auto [entry, inserted] =
            pathsByFoldedName_.emplace(folded, canonicalRelativePath);
        result.inserted = inserted;
        if (inserted)
            return result;

        const bool exactDuplicate = entry->second == canonicalRelativePath;
        result.diagnostics.push_back({
            exactDuplicate ? DependencyDiagnosticCode::Duplicate
                           : DependencyDiagnosticCode::CaseCollision,
            sourceId,
            canonicalRelativePath,
            exactDuplicate
                ? "Dependency path was already registered."
                : "Dependency path differs from an existing path only by case: " +
                    entry->second,
        });
        return result;
    }

    DependencyCollector::DependencyCollector(std::string projectRoot)
        : projectRoot_(std::move(projectRoot))
    {
    }

    bool DependencyCollector::RegisterProvider(
        const IDependencyProvider& provider,
        std::string& error)
    {
        const std::string name = provider.Name() == nullptr
            ? std::string{} : std::string(provider.Name());
        if (name.empty() || provider.Version() == 0)
        {
            error = "Dependency providers require a name and non-zero version.";
            return false;
        }
        if (!providers_.emplace(name, &provider).second)
        {
            error = "A dependency provider named '" + name + "' is already registered.";
            return false;
        }
        error.clear();
        return true;
    }

    bool DependencyCollector::AddRoot(
        const DependencyRoot& root,
        std::string& error)
    {
        const auto resolved = ResolveDependencyPath(projectRoot_, root.declaredPath);
        if (!resolved.accepted)
        {
            graph_.diagnostics.push_back(PathDiagnostic(
                DependencyDiagnosticCode::OutsideProject,
                {}, root.declaredPath, resolved.error));
            error = resolved.error;
            return false;
        }

        const auto registration = pathRegistry_.Register(
            {}, resolved.canonicalRelativePath);
        graph_.diagnostics.insert(
            graph_.diagnostics.end(),
            registration.diagnostics.begin(), registration.diagnostics.end());
        if (!registration.inserted)
        {
            error = "Dependency root path was already registered.";
            return false;
        }

        DependencyNode node;
        node.id = StablePathId(resolved.canonicalRelativePath);
        node.projectRelativePath = resolved.canonicalRelativePath;
        node.dependencyClass = root.dependencyClass;
        node.requirement = root.requirement;
        node.provider = root.provenance.empty() ? "root" : root.provenance;
        node.providerVersion = 1;
        graph_.rootIds.push_back(node.id);
        graph_.nodes.push_back(std::move(node));
        if (!resolved.exists)
        {
            graph_.diagnostics.push_back(PathDiagnostic(
                DependencyDiagnosticCode::Missing,
                graph_.rootIds.back(), resolved.canonicalRelativePath,
                root.requirement == DependencyRequirement::Required
                    ? "Required graph root does not exist."
                    : "Declared graph root does not exist."));
        }
        error.clear();
        return true;
    }

    bool DependencyCollector::DiscoverRootDependencies(std::string& error)
    {
        const std::size_t rootCount = graph_.rootIds.size();
        for (std::size_t index = 0; index < rootCount; ++index)
        {
            const DependencyNode source = graph_.nodes[index];
            for (const auto& [name, provider] : providers_)
            {
                (void)name;
                if (!provider->Supports(source.dependencyClass))
                    continue;

                DependencyProviderContext context{projectRoot_, &source};
                std::vector<DependencyCandidate> candidates;
                std::string providerError;
                const bool succeeded = provider->Discover(
                    context,
                    [&candidates](const DependencyCandidate& candidate)
                    {
                        candidates.push_back(candidate);
                    },
                    providerError);
                if (!succeeded)
                {
                    error = "Dependency provider '" + std::string(provider->Name()) +
                        "' failed for " + source.projectRelativePath + ": " +
                        providerError;
                    return false;
                }
                for (const auto& candidate : candidates)
                    AcceptCandidate(source, *provider, candidate);
            }
        }
        error.clear();
        return true;
    }

    const DependencyGraph& DependencyCollector::Graph() const noexcept
    {
        return graph_;
    }

    void DependencyCollector::AcceptCandidate(
        const DependencyNode& source,
        const IDependencyProvider& provider,
        const DependencyCandidate& candidate)
    {
        const auto resolved = ResolveDependencyPath(projectRoot_, candidate.declaredPath);
        if (!resolved.accepted)
        {
            graph_.diagnostics.push_back(PathDiagnostic(
                DependencyDiagnosticCode::OutsideProject,
                source.id, candidate.declaredPath, resolved.error));
            return;
        }

        const auto registration = pathRegistry_.Register(
            source.id, resolved.canonicalRelativePath);
        graph_.diagnostics.insert(
            graph_.diagnostics.end(),
            registration.diagnostics.begin(), registration.diagnostics.end());

        std::string targetId = StablePathId(resolved.canonicalRelativePath);
        if (!registration.inserted)
        {
            const auto existing = std::find_if(
                graph_.nodes.begin(), graph_.nodes.end(),
                [&resolved](const DependencyNode& node)
                {
                    return FoldPathCase(node.projectRelativePath) ==
                        FoldPathCase(resolved.canonicalRelativePath);
                });
            if (existing != graph_.nodes.end())
                targetId = existing->id;
        }
        if (registration.inserted)
        {
            DependencyNode node;
            node.id = targetId;
            node.projectRelativePath = resolved.canonicalRelativePath;
            node.dependencyClass = candidate.dependencyClass;
            node.requirement = candidate.requirement;
            node.provider = provider.Name();
            node.providerVersion = provider.Version();
            node.runtimeSupport = candidate.runtimeSupport;
            graph_.nodes.push_back(std::move(node));
            if (!resolved.exists)
            {
                graph_.diagnostics.push_back(PathDiagnostic(
                    DependencyDiagnosticCode::Missing,
                    source.id, resolved.canonicalRelativePath,
                    "Declared dependency does not exist."));
            }
        }
        graph_.edges.push_back({source.id, targetId, candidate.provenance});
    }
}
