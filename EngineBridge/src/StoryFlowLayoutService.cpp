#include "renegade/bridge/StoryFlowLayoutService.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <unordered_map>
#include <utility>

#include "json.hpp"

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr float DefaultColumnSpacing = 360.0f;
        constexpr float DefaultRowSpacing = 220.0f;
        constexpr float MinimumZoom = 0.05f;
        constexpr float MaximumZoom = 8.0f;
        constexpr float MaximumCoordinateMagnitude = 10000000.0f;

        bool IsFiniteCoordinate(const float value) noexcept
        {
            return std::isfinite(value) &&
                std::abs(value) <= MaximumCoordinateMagnitude;
        }

        bool HasOnlyKeys(
            const nlohmann::json& object,
            const std::set<std::string>& allowed)
        {
            if (!object.is_object())
                return false;
            for (auto iterator = object.begin(); iterator != object.end(); ++iterator)
            {
                if (allowed.find(iterator.key()) == allowed.end())
                    return false;
            }
            return true;
        }

        bool ReadFiniteFloat(
            const nlohmann::json& object,
            const char* key,
            float& value,
            std::string& error)
        {
            if (!object.contains(key) || !object.at(key).is_number())
            {
                error = std::string("Story Flow layout '") + key +
                    "' must be numeric.";
                return false;
            }
            value = object.at(key).get<float>();
            if (!std::isfinite(value))
            {
                error = std::string("Story Flow layout '") + key +
                    "' must be finite.";
                return false;
            }
            return true;
        }

        void RemoveWithoutThrow(const fs::path& path)
        {
            std::error_code ignored;
            fs::remove(path, ignored);
        }
    }

    std::string ResolveStoryFlowLayoutPath(
        const std::string& projectRoot,
        const StableId& flowDocumentId)
    {
        if (projectRoot.empty() || !IsValidStableId(flowDocumentId))
            return {};
        return (fs::u8path(projectRoot) /
            "Saved" /
            "EditorState" /
            "StoryFlow" /
            fs::u8path(flowDocumentId + StoryFlowLayoutExtension))
            .lexically_normal()
            .generic_u8string();
    }

    StoryFlowLayoutDocument BuildDeterministicStoryFlowLayout(
        const StoryFlowAuthoringModel& model,
        const StableId& projectId,
        const StableId& flowDocumentId)
    {
        StoryFlowLayoutDocument layout;
        layout.projectId = projectId;
        layout.flowDocumentId = flowDocumentId;
        layout.nodes.reserve(model.Nodes().size());
        for (const auto& node : model.Nodes())
        {
            layout.nodes.push_back({
                node.id,
                static_cast<float>(node.presentationColumn) * DefaultColumnSpacing,
                static_cast<float>(node.presentationRow) * DefaultRowSpacing,
            });
        }
        return layout;
    }

    bool ReconcileStoryFlowLayout(
        const StoryFlowAuthoringModel& model,
        const StableId& projectId,
        const StableId& flowDocumentId,
        StoryFlowLayoutDocument& layout,
        std::string& error)
    {
        if (!model.IsLoaded())
        {
            error = "Story Flow layout cannot be reconciled without a loaded Flow.";
            return false;
        }
        if (layout.projectId.empty() && layout.flowDocumentId.empty())
        {
            layout = BuildDeterministicStoryFlowLayout(
                model, projectId, flowDocumentId);
            error.clear();
            return true;
        }
        if (!ValidateStoryFlowLayout(
                layout, projectId, flowDocumentId, error))
        {
            return false;
        }

        std::unordered_map<StableId, StoryFlowNodeLayout> saved;
        saved.reserve(layout.nodes.size());
        for (const auto& item : layout.nodes)
            saved.emplace(item.nodeId, item);

        StoryFlowLayoutDocument reconciled =
            BuildDeterministicStoryFlowLayout(model, projectId, flowDocumentId);
        reconciled.canvas = layout.canvas;
        for (auto& item : reconciled.nodes)
        {
            const auto found = saved.find(item.nodeId);
            if (found != saved.end())
            {
                item.x = found->second.x;
                item.y = found->second.y;
            }
        }

        layout = std::move(reconciled);
        error.clear();
        return true;
    }

    bool ValidateStoryFlowLayout(
        const StoryFlowLayoutDocument& layout,
        const StableId& expectedProjectId,
        const StableId& expectedFlowDocumentId,
        std::string& error)
    {
        if (layout.formatIdentifier != StoryFlowLayoutFormat)
        {
            error = "The Story Flow layout has an unsupported format identifier.";
            return false;
        }
        if (layout.schemaVersion != StoryFlowLayoutDocument::CurrentSchemaVersion)
        {
            error = "Unsupported Story Flow layout schema version: " +
                std::to_string(layout.schemaVersion);
            return false;
        }
        if (!IsValidStableId(layout.projectId) ||
            (!expectedProjectId.empty() && layout.projectId != expectedProjectId))
        {
            error = "The Story Flow layout belongs to a different or invalid project.";
            return false;
        }
        if (!IsValidStableId(layout.flowDocumentId) ||
            (!expectedFlowDocumentId.empty() &&
             layout.flowDocumentId != expectedFlowDocumentId))
        {
            error = "The Story Flow layout belongs to a different or invalid Flow document.";
            return false;
        }
        if (!IsFiniteCoordinate(layout.canvas.panX) ||
            !IsFiniteCoordinate(layout.canvas.panY) ||
            !std::isfinite(layout.canvas.zoom) ||
            layout.canvas.zoom < MinimumZoom ||
            layout.canvas.zoom > MaximumZoom)
        {
            error = "The Story Flow layout contains invalid canvas state.";
            return false;
        }

        std::set<StableId> nodeIds;
        for (const auto& node : layout.nodes)
        {
            if (!IsValidStableId(node.nodeId) ||
                !nodeIds.insert(node.nodeId).second)
            {
                error = "The Story Flow layout contains a malformed or duplicate node ID.";
                return false;
            }
            if (!IsFiniteCoordinate(node.x) || !IsFiniteCoordinate(node.y))
            {
                error = "The Story Flow layout contains an invalid node position.";
                return false;
            }
        }

        error.clear();
        return true;
    }

    bool WriteStoryFlowLayout(
        const std::string& filePath,
        const StoryFlowLayoutDocument& layout,
        std::string& error)
    {
        if (filePath.empty())
        {
            error = "A Story Flow layout path is required.";
            return false;
        }
        if (!ValidateStoryFlowLayout(
                layout, layout.projectId, layout.flowDocumentId, error))
        {
            return false;
        }

        nlohmann::json root;
        root["format"] = layout.formatIdentifier;
        root["schema_version"] = layout.schemaVersion;
        root["project_id"] = layout.projectId;
        root["flow_document_id"] = layout.flowDocumentId;
        root["canvas"] = {
            {"pan_x", layout.canvas.panX},
            {"pan_y", layout.canvas.panY},
            {"zoom", layout.canvas.zoom},
        };
        root["nodes"] = nlohmann::json::array();
        for (const auto& node : layout.nodes)
        {
            root["nodes"].push_back({
                {"node_id", node.nodeId},
                {"x", node.x},
                {"y", node.y},
            });
        }
        const std::string content = root.dump();

        try
        {
            std::error_code pathError;
            const fs::path destination =
                fs::absolute(fs::u8path(filePath), pathError).lexically_normal();
            if (pathError || destination.filename().empty())
            {
                error = "Could not resolve the Story Flow layout path: " +
                    pathError.message();
                return false;
            }
            fs::create_directories(destination.parent_path(), pathError);
            if (pathError)
            {
                error = "Could not create the Story Flow layout folder: " +
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
                    error = "Could not create the Story Flow layout render file.";
                    return false;
                }
                stream.write(content.data(), static_cast<std::streamsize>(content.size()));
                stream.flush();
                if (!stream)
                {
                    RemoveWithoutThrow(temporary);
                    error = "Could not write complete Story Flow layout content.";
                    return false;
                }
            }

            const bool destinationExists = fs::exists(destination, pathError);
            if (pathError)
            {
                RemoveWithoutThrow(temporary);
                error = "Could not inspect the existing Story Flow layout: " +
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
                    error = "Could not preserve the previous Story Flow layout: " +
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
                error = "Could not promote the Story Flow layout: " +
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
            error = std::string("Could not write Story Flow layout: ") +
                exception.what();
            return false;
        }
    }

    bool ReadStoryFlowLayout(
        const std::string& filePath,
        const StableId& expectedProjectId,
        const StableId& expectedFlowDocumentId,
        StoryFlowLayoutDocument& layout,
        std::string& error)
    {
        layout = {};
        try
        {
            std::ifstream stream(fs::u8path(filePath), std::ios::binary);
            if (!stream)
            {
                error = "Could not read Story Flow layout: " + filePath;
                return false;
            }
            const std::string content(
                (std::istreambuf_iterator<char>(stream)),
                std::istreambuf_iterator<char>());
            const nlohmann::json root = nlohmann::json::parse(content);
            if (!HasOnlyKeys(root, {
                    "format", "schema_version", "project_id",
                    "flow_document_id", "canvas", "nodes"}) ||
                !root.contains("format") || !root.at("format").is_string() ||
                !root.contains("schema_version") ||
                !root.at("schema_version").is_number_unsigned() ||
                !root.contains("project_id") ||
                !root.at("project_id").is_string() ||
                !root.contains("flow_document_id") ||
                !root.at("flow_document_id").is_string() ||
                !root.contains("canvas") || !root.at("canvas").is_object() ||
                !root.contains("nodes") || !root.at("nodes").is_array())
            {
                error = "The Story Flow layout has an invalid root structure.";
                return false;
            }

            StoryFlowLayoutDocument parsed;
            parsed.formatIdentifier = root.at("format").get<std::string>();
            parsed.schemaVersion = root.at("schema_version").get<std::uint32_t>();
            parsed.projectId = root.at("project_id").get<std::string>();
            parsed.flowDocumentId = root.at("flow_document_id").get<std::string>();

            const auto& canvas = root.at("canvas");
            if (!HasOnlyKeys(canvas, {"pan_x", "pan_y", "zoom"}) ||
                !ReadFiniteFloat(canvas, "pan_x", parsed.canvas.panX, error) ||
                !ReadFiniteFloat(canvas, "pan_y", parsed.canvas.panY, error) ||
                !ReadFiniteFloat(canvas, "zoom", parsed.canvas.zoom, error))
            {
                if (error.empty())
                    error = "The Story Flow layout has invalid canvas state.";
                return false;
            }

            parsed.nodes.reserve(root.at("nodes").size());
            for (const auto& item : root.at("nodes"))
            {
                if (!HasOnlyKeys(item, {"node_id", "x", "y"}) ||
                    !item.contains("node_id") || !item.at("node_id").is_string())
                {
                    error = "The Story Flow layout contains an invalid node record.";
                    return false;
                }
                StoryFlowNodeLayout node;
                node.nodeId = item.at("node_id").get<std::string>();
                if (!ReadFiniteFloat(item, "x", node.x, error) ||
                    !ReadFiniteFloat(item, "y", node.y, error))
                {
                    return false;
                }
                parsed.nodes.push_back(std::move(node));
            }

            if (!ValidateStoryFlowLayout(
                    parsed,
                    expectedProjectId,
                    expectedFlowDocumentId,
                    error))
            {
                return false;
            }

            layout = std::move(parsed);
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error = std::string("Could not parse Story Flow layout: ") +
                exception.what();
            return false;
        }
    }
}
