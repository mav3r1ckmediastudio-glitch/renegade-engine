#pragma once

#include "renegade/bridge/FlowService.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <string>

namespace renegade::bridge
{
    struct StoryFlowJourneyThumbnailImportResult
    {
        bool succeeded = false;
        std::string message;
        std::string relativePath;
        std::string resolvedPath;
    };

    // Gate 9B presentation-resource boundary. A Journey thumbnail is a governed
    // project resource whose slot is derived from the stable Story Flow node ID:
    // Content/StoryFlow/Thumbnails/<node-id>.<supported-image-extension>
    // No creator machine path or second semantic reference is serialized.
    class StoryFlowJourneyThumbnailService final
    {
    public:
        [[nodiscard]] static bool IsSupportedExtension(std::string extension)
        {
            std::transform(
                extension.begin(), extension.end(), extension.begin(),
                [](const unsigned char c)
                {
                    return static_cast<char>(std::tolower(c));
                });
            return std::any_of(
                SupportedExtensions.begin(), SupportedExtensions.end(),
                [&](const char* candidate) { return extension == candidate; });
        }

        [[nodiscard]] static bool ResolveProjectRootFromFlowPath(
            const std::string& flowPath,
            std::string& projectRoot,
            std::string& error)
        {
            namespace fs = std::filesystem;
            projectRoot.clear();
            if (flowPath.empty())
            {
                error = "Story Flow file path is empty.";
                return false;
            }

            try
            {
                std::error_code pathError;
                fs::path current = fs::absolute(fs::u8path(flowPath), pathError)
                    .lexically_normal()
                    .parent_path();
                if (pathError || current.empty())
                {
                    error = "Could not resolve the Story Flow file location: " +
                        pathError.message();
                    return false;
                }

                while (!current.empty())
                {
                    std::error_code iteratorError;
                    bool foundDescriptor = false;
                    for (fs::directory_iterator iterator(current, iteratorError), end;
                        !iteratorError && iterator != end;
                        iterator.increment(iteratorError))
                    {
                        std::error_code typeError;
                        if (!iterator->is_regular_file(typeError) || typeError)
                            continue;
                        std::string extension = iterator->path().extension().generic_u8string();
                        std::transform(
                            extension.begin(), extension.end(), extension.begin(),
                            [](const unsigned char c)
                            {
                                return static_cast<char>(std::tolower(c));
                            });
                        if (extension == ".renegade")
                        {
                            foundDescriptor = true;
                            break;
                        }
                    }
                    if (iteratorError)
                    {
                        error = "Could not inspect a Story Flow project parent: " +
                            iteratorError.message();
                        return false;
                    }
                    if (foundDescriptor)
                    {
                        projectRoot = current.generic_u8string();
                        error.clear();
                        return true;
                    }

                    const fs::path parent = current.parent_path();
                    if (parent.empty() || parent == current)
                        break;
                    current = parent;
                }
            }
            catch (const std::exception& exception)
            {
                error = std::string("Could not locate the Renegade project root: ") +
                    exception.what();
                return false;
            }

            error = "Could not locate a .renegade project descriptor above the Story Flow document.";
            return false;
        }

        [[nodiscard]] StoryFlowJourneyThumbnailImportResult Import(
            const std::string& projectRoot,
            const StableId& nodeId,
            const std::string& sourcePath) const
        {
            namespace fs = std::filesystem;
            StoryFlowJourneyThumbnailImportResult result;
            if (projectRoot.empty())
            {
                result.message = "A project root is required for Journey thumbnails.";
                return result;
            }
            if (!IsValidStableId(nodeId))
            {
                result.message = "A valid stable Story Flow node ID is required.";
                return result;
            }
            if (sourcePath.empty())
            {
                result.message = "A source image is required.";
                return result;
            }

            try
            {
                std::error_code pathError;
                const fs::path root = fs::absolute(fs::u8path(projectRoot), pathError)
                    .lexically_normal();
                if (pathError || root.empty())
                {
                    result.message = "Could not resolve the project root: " +
                        pathError.message();
                    return result;
                }
                const fs::path source = fs::absolute(fs::u8path(sourcePath), pathError)
                    .lexically_normal();
                if (pathError || !fs::is_regular_file(source, pathError) || pathError)
                {
                    result.message = "The selected thumbnail image could not be read.";
                    return result;
                }

                std::string extension = source.extension().generic_u8string();
                std::transform(
                    extension.begin(), extension.end(), extension.begin(),
                    [](const unsigned char c)
                    {
                        return static_cast<char>(std::tolower(c));
                    });
                if (!IsSupportedExtension(extension))
                {
                    result.message =
                        "Journey thumbnails must be JPG, JPEG, PNG, BMP or TGA images.";
                    return result;
                }

                const fs::path relative = ManagedRelativePath(nodeId, extension);
                const fs::path destination = (root / relative).lexically_normal();
                const fs::path folder = destination.parent_path();
                fs::create_directories(folder, pathError);
                if (pathError)
                {
                    result.message = "Could not create the Journey thumbnail folder: " +
                        pathError.message();
                    return result;
                }

                const fs::path temporary = folder /
                    fs::u8path(destination.filename().generic_u8string() +
                        ".tmp-" + GenerateStableId());
                const fs::path backup = folder /
                    fs::u8path(destination.filename().generic_u8string() + ".bak");

                fs::copy_file(
                    source, temporary,
                    fs::copy_options::overwrite_existing,
                    pathError);
                if (pathError)
                {
                    RemoveWithoutThrow(temporary);
                    result.message = "Could not copy the selected Journey thumbnail: " +
                        pathError.message();
                    return result;
                }

                const bool destinationExists = fs::exists(destination, pathError);
                if (pathError)
                {
                    RemoveWithoutThrow(temporary);
                    result.message = "Could not inspect the existing Journey thumbnail: " +
                        pathError.message();
                    return result;
                }
                if (destinationExists)
                {
                    RemoveWithoutThrow(backup);
                    fs::rename(destination, backup, pathError);
                    if (pathError)
                    {
                        RemoveWithoutThrow(temporary);
                        result.message = "Could not preserve the previous Journey thumbnail: " +
                            pathError.message();
                        return result;
                    }
                }

                fs::rename(temporary, destination, pathError);
                if (pathError)
                {
                    RemoveWithoutThrow(temporary);
                    if (destinationExists)
                    {
                        std::error_code rollbackError;
                        fs::rename(backup, destination, rollbackError);
                    }
                    result.message = "Could not promote the new Journey thumbnail: " +
                        pathError.message();
                    return result;
                }
                if (destinationExists)
                    RemoveWithoutThrow(backup);

                // One governed slot per stable node. Remove stale variants only
                // after the new image has been promoted successfully.
                for (const char* candidate : SupportedExtensions)
                {
                    if (extension == candidate)
                        continue;
                    RemoveWithoutThrow(root / ManagedRelativePath(nodeId, candidate));
                }

                result.succeeded = true;
                result.relativePath = relative.generic_u8string();
                result.resolvedPath = destination.generic_u8string();
                result.message = "Journey thumbnail imported into the project.";
                return result;
            }
            catch (const std::exception& exception)
            {
                result.message = std::string("Could not import Journey thumbnail: ") +
                    exception.what();
                return result;
            }
        }

        // Resolve the deterministic thumbnail slot. No thumbnail is a normal
        // success state and returns empty paths. More than one managed variant
        // is treated as ambiguous rather than silently picking one.
        [[nodiscard]] bool ResolveManaged(
            const std::string& projectRoot,
            const StableId& nodeId,
            std::string& relativePath,
            std::string& resolvedPath,
            std::string& error) const
        {
            namespace fs = std::filesystem;
            relativePath.clear();
            resolvedPath.clear();
            if (projectRoot.empty() || !IsValidStableId(nodeId))
            {
                error = "A project root and valid Story Flow node ID are required.";
                return false;
            }

            try
            {
                std::error_code pathError;
                const fs::path root = fs::absolute(fs::u8path(projectRoot), pathError)
                    .lexically_normal();
                if (pathError)
                {
                    error = "Could not resolve the project root: " + pathError.message();
                    return false;
                }

                std::size_t matches = 0;
                fs::path foundRelative;
                fs::path foundAbsolute;
                for (const char* extension : SupportedExtensions)
                {
                    const fs::path relative = ManagedRelativePath(nodeId, extension);
                    const fs::path candidate = (root / relative).lexically_normal();
                    const bool exists = fs::is_regular_file(candidate, pathError);
                    if (pathError)
                    {
                        error = "Could not inspect the Journey thumbnail slot: " +
                            pathError.message();
                        return false;
                    }
                    if (!exists)
                        continue;
                    ++matches;
                    foundRelative = relative;
                    foundAbsolute = candidate;
                }

                if (matches > 1)
                {
                    error = "Journey thumbnail slot is ambiguous; multiple managed image variants exist.";
                    return false;
                }
                if (matches == 1)
                {
                    relativePath = foundRelative.generic_u8string();
                    resolvedPath = foundAbsolute.generic_u8string();
                }
                error.clear();
                return true;
            }
            catch (const std::exception& exception)
            {
                error = std::string("Could not resolve Journey thumbnail: ") +
                    exception.what();
                return false;
            }
        }

    private:
        inline static constexpr std::array<const char*, 5> SupportedExtensions = {
            ".jpg", ".jpeg", ".png", ".bmp", ".tga",
        };

        [[nodiscard]] static std::filesystem::path ManagedRelativePath(
            const StableId& nodeId,
            const std::string& extension)
        {
            return std::filesystem::u8path("Content") /
                "StoryFlow" /
                "Thumbnails" /
                std::filesystem::u8path(nodeId + extension);
        }

        static void RemoveWithoutThrow(const std::filesystem::path& path)
        {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
    };
}
