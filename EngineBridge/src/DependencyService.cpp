#include "renegade/bridge/DependencyService.h"

#include <filesystem>

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
}
