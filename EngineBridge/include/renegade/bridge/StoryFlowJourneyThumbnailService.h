#pragma once

#include "renegade/bridge/FlowService.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace renegade::bridge
{
    using StoryFlowJourneyThumbnailMap =
        std::unordered_map<StableId, std::string>;

    struct StoryFlowJourneyThumbnailImportResult
    {
        bool succeeded = false;
        std::string message;
        std::string relativePath;
        std::string resolvedPath;
    };

    // Gate 9B presentation-resource boundary. Journey thumbnails are project
    // resources owned by the stable Story Flow node ID; the creator's original
    // machine path is never serialized. Runtime Story Flow semantics do not
    // depend on this service, its state file, or these images.
    class StoryFlowJourneyThumbnailService final
    {
    public:
        inline static constexpr const char* StateFormat =
            "renegade-story-flow-journey-thumbnails";
        inline static constexpr unsigned StateVersion = 1;
        inline static constexpr const char* StateExtension =
            ".journey-thumbnails";

        [[nodiscard]] static bool IsSupportedExtension(std::string extension)
        {
            std::transform(
                extension.begin(), extension.end(), extension.begin(),
                [](const unsigned char c)
                {
                    return static_cast<char>(std::tolower(c));
                });
            static constexpr std::array<const char*, 5> Supported = {
                ".jpg", ".jpeg", ".png", ".bmp", ".tga",
            };
            return std::any_of(
                Supported.begin(), Supported.end(),
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

        [[nodiscard]] static std::string ResolveStatePath(
            const std::string& projectRoot,
            const StableId& flowDocumentId)
        {
            namespace fs = std::filesystem;
            if (projectRoot.empty() || !IsValidStableId(flowDocumentId))
                return {};
            return (fs::u8path(projectRoot) /
                "Saved" /
                "EditorState" /
                "StoryFlow" /
                fs::u8path(flowDocumentId + StateExtension))
                .lexically_normal()
                .generic_u8string();
        }

        [[nodiscard]] bool ReadState(
            const std::string& projectRoot,
            const StableId& expectedProjectId,
            const StableId& expectedFlowDocumentId,
            StoryFlowJourneyThumbnailMap& thumbnails,
            std::string& error) const
        {
            namespace fs = std::filesystem;
            thumbnails.clear();
            if (projectRoot.empty() ||
                !IsValidStableId(expectedProjectId) ||
                !IsValidStableId(expectedFlowDocumentId))
            {
                error = "Valid project and Flow identities are required for Journey thumbnail state.";
                return false;
            }

            const std::string filePath =
                ResolveStatePath(projectRoot, expectedFlowDocumentId);
            if (filePath.empty())
            {
                error = "Could not resolve the Journey thumbnail state path.";
                return false;
            }

            try
            {
                std::error_code existsError;
                if (!fs::exists(fs::u8path(filePath), existsError))
                {
                    if (existsError)
                    {
                        error = "Could not inspect Journey thumbnail state: " +
                            existsError.message();
                        return false;
                    }
                    error.clear();
                    return true;
                }

                std::ifstream stream(fs::u8path(filePath), std::ios::binary);
                if (!stream)
                {
                    error = "Could not open Journey thumbnail state.";
                    return false;
                }

                std::string format;
                std::string version;
                std::string projectId;
                std::string flowId;
                if (!ReadKeyValue(stream, "format", format) ||
                    !ReadKeyValue(stream, "version", version) ||
                    !ReadKeyValue(stream, "project_id", projectId) ||
                    !ReadKeyValue(stream, "flow_document_id", flowId))
                {
                    error = "Journey thumbnail state has an invalid header.";
                    return false;
                }
                if (format != StateFormat ||
                    version != std::to_string(StateVersion) ||
                    projectId != expectedProjectId ||
                    flowId != expectedFlowDocumentId)
                {
                    error = "Journey thumbnail state belongs to a different or unsupported Flow.";
                    return false;
                }

                std::string line;
                while (std::getline(stream, line))
                {
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();
                    if (line.empty())
                        continue;
                    const std::size_t separator = line.find('=');
                    if (separator == std::string::npos)
                    {
                        error = "Journey thumbnail state contains a malformed card record.";
                        return false;
                    }
                    const StableId nodeId = line.substr(0, separator);
                    const std::string relativePath = line.substr(separator + 1);
                    if (!IsValidStableId(nodeId) ||
                        relativePath.empty() ||
                        !IsSafeProjectRelativePath(std::filesystem::u8path(relativePath)) ||
                        !IsSupportedExtension(
                            std::filesystem::u8path(relativePath).extension().generic_u8string()) ||
                        !thumbnails.emplace(nodeId, relativePath).second)
                    {
                        error = "Journey thumbnail state contains an invalid or duplicate card record.";
                        return false;
                    }
                }
                if (!stream.eof())
                {
                    error = "Journey thumbnail state could not be read completely.";
                    return false;
                }

                error.clear();
                return true;
            }
            catch (const std::exception& exception)
            {
                error = std::string("Could not read Journey thumbnail state: ") +
                    exception.what();
                return false;
            }
        }

        [[nodiscard]] bool WriteState(
            const std::string& projectRoot,
            const StableId& projectId,
            const StableId& flowDocumentId,
            const StoryFlowJourneyThumbnailMap& thumbnails,
            std::string& error) const
        {
            namespace fs = std::filesystem;
            if (projectRoot.empty() ||
                !IsValidStableId(projectId) ||
                !IsValidStableId(flowDocumentId))
            {
                error = "Valid project and Flow identities are required for Journey thumbnail state.";
                return false;
            }

            std::vector<std::pair<StableId, std::string>> ordered;
            ordered.reserve(thumbnails.size());
            for (const auto& item : thumbnails)
            {
                const fs::path hint = fs::u8path(item.second);
                if (!IsValidStableId(item.first) || item.second.empty() ||
                    !IsSafeProjectRelativePath(hint) ||
                    !IsSupportedExtension(hint.extension().generic_u8string()))
                {
                    error = "Journey thumbnail state contains an invalid card resource.";
                    return false;
                }
                ordered.push_back(item);
            }
            std::sort(
                ordered.begin(), ordered.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            const fs::path destination =
                fs::u8path(ResolveStatePath(projectRoot, flowDocumentId));
            if (destination.empty())
            {
                error = "Could not resolve the Journey thumbnail state path.";
                return false;
            }

            try
            {
                std::error_code pathError;
                fs::create_directories(destination.parent_path(), pathError);
                if (pathError)
                {
                    error = "Could not create the Journey thumbnail state folder: " +
                        pathError.message();
                    return false;
                }

                const fs::path temporary = destination.parent_path() /
                    fs::u8path(destination.filename().generic_u8string() +
                        ".tmp-" + GenerateStableId());
                const fs::path backup = destination.parent_path() /
                    fs::u8path(destination.filename().generic_u8string() + ".bak");

                {
                    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
                    if (!stream)
                    {
                        error = "Could not create Journey thumbnail state.";
                        return false;
                    }
                    stream << "format=" << StateFormat << '\n';
                    stream << "version=" << StateVersion << '\n';
                    stream << "project_id=" << projectId << '\n';
                    stream << "flow_document_id=" << flowDocumentId << '\n';
                    for (const auto& item : ordered)
                        stream << item.first << '=' << item.second << '\n';
                    stream.flush();
                    if (!stream)
                    {
                        RemoveWithoutThrow(temporary);
                        error = "Could not write complete Journey thumbnail state.";
                        return false;
                    }
                }

                const bool destinationExists = fs::exists(destination, pathError);
                if (pathError)
                {
                    RemoveWithoutThrow(temporary);
                    error = "Could not inspect existing Journey thumbnail state: " +
                        pathError.message();
                    return false;
                }
                if (destinationExists)
                {
                    RemoveWithoutThrow(backup);
                    fs::rename(destination, backup, pathError);
                    if (pathError)
                    {
                        RemoveWithoutThrow(temporary);
                        error = "Could not preserve previous Journey thumbnail state: " +
                            pathError.message();
                        return false;
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
                    error = "Could not promote Journey thumbnail state: " +
                        pathError.message();
                    return false;
                }
                if (destinationExists)
                    RemoveWithoutThrow(backup);
                error.clear();
                return true;
            }
            catch (const std::exception& exception)
            {
                error = std::string("Could not write Journey thumbnail state: ") +
                    exception.what();
                return false;
            }
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

                const fs::path relative =
                    fs::u8path("Content") /
                    "StoryFlow" /
                    "Thumbnails" /
                    fs::u8path(nodeId + extension);
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

                static constexpr std::array<const char*, 5> Supported = {
                    ".jpg", ".jpeg", ".png", ".bmp", ".tga",
                };
                for (const char* candidate : Supported)
                {
                    if (extension == candidate)
                        continue;
                    RemoveWithoutThrow(folder / fs::u8path(nodeId + candidate));
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

        [[nodiscard]] bool Resolve(
            const std::string& projectRoot,
            const std::string& relativePath,
            std::string& resolvedPath,
            std::string& error) const
        {
            namespace fs = std::filesystem;
            resolvedPath.clear();
            if (projectRoot.empty() || relativePath.empty())
            {
                error = "Project root and project-relative thumbnail path are required.";
                return false;
            }

            try
            {
                const fs::path hint = fs::u8path(relativePath);
                if (!IsSafeProjectRelativePath(hint))
                {
                    error = "Journey thumbnail path must remain inside the project.";
                    return false;
                }
                if (!IsSupportedExtension(hint.extension().generic_u8string()))
                {
                    error = "Journey thumbnail path has an unsupported image extension.";
                    return false;
                }

                std::error_code pathError;
                const fs::path root = fs::absolute(fs::u8path(projectRoot), pathError)
                    .lexically_normal();
                if (pathError)
                {
                    error = "Could not resolve the project root: " + pathError.message();
                    return false;
                }
                const fs::path candidate = (root / hint).lexically_normal();
                const fs::path relative = candidate.lexically_relative(root);
                if (!IsSafeProjectRelativePath(relative) ||
                    !fs::is_regular_file(candidate, pathError) || pathError)
                {
                    error = "Journey thumbnail is missing or outside the project.";
                    return false;
                }

                resolvedPath = candidate.generic_u8string();
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
        static void RemoveWithoutThrow(const std::filesystem::path& path)
        {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }

        [[nodiscard]] static bool ReadKeyValue(
            std::istream& stream,
            const char* expectedKey,
            std::string& value)
        {
            std::string line;
            if (!std::getline(stream, line))
                return false;
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            const std::string prefix = std::string(expectedKey) + '=';
            if (line.rfind(prefix, 0) != 0)
                return false;
            value = line.substr(prefix.size());
            return true;
        }

        [[nodiscard]] static bool IsSafeProjectRelativePath(
            const std::filesystem::path& path)
        {
            if (path.empty() || path.is_absolute() ||
                path.has_root_directory() || path.has_root_name())
            {
                return false;
            }
            for (const auto& component : path)
            {
                if (component == "..")
                    return false;
            }
            return true;
        }
    };
}
