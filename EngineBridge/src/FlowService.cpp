#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/DependencyService.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <wiConfig.h>

namespace
{
    namespace fs = std::filesystem;

    bool IsValidSymbol(const std::string& value)
    {
        if (value.empty() || value.front() < 'a' || value.front() > 'z')
        {
            return false;
        }

        return std::all_of(
            value.begin(),
            value.end(),
            [](const unsigned char character)
            {
                return (character >= 'a' && character <= 'z') ||
                    (character >= '0' && character <= '9') ||
                    character == '.' || character == '_' || character == '-';
            });
    }

    bool IsSafeRelativePath(const std::string& value)
    {
        if (value.empty())
        {
            return false;
        }

        const fs::path path = fs::u8path(value);
        if (path.is_absolute() || path.has_root_name() ||
            path.has_root_directory())
        {
            return false;
        }

        return std::none_of(
            path.begin(),
            path.end(),
            [](const fs::path& part)
            {
                return part == "..";
            });
    }

    bool TryParseNodeKind(
        const std::string& value,
        renegade::bridge::FlowNodeKind& kind)
    {
        using renegade::bridge::FlowNodeKind;
        if (value == "game_start")
        {
            kind = FlowNodeKind::GameStart;
            return true;
        }
        if (value == "level")
        {
            kind = FlowNodeKind::Level;
            return true;
        }
        if (value == "screen")
        {
            kind = FlowNodeKind::Screen;
            return true;
        }
        if (value == "complete_game")
        {
            kind = FlowNodeKind::CompleteGame;
            return true;
        }
        if (value == "return_to_main_menu")
        {
            kind = FlowNodeKind::ReturnToMainMenu;
            return true;
        }
        if (value == "quit")
        {
            kind = FlowNodeKind::Quit;
            return true;
        }
        return false;
    }

    bool TryParseConditionOperator(
        const std::string& value,
        renegade::bridge::FlowConditionOperator& operation)
    {
        using renegade::bridge::FlowConditionOperator;
        if (value == "equals")
        {
            operation = FlowConditionOperator::Equals;
            return true;
        }
        if (value == "not_equals")
        {
            operation = FlowConditionOperator::NotEquals;
            return true;
        }
        if (value == "exists")
        {
            operation = FlowConditionOperator::Exists;
            return true;
        }
        if (value == "missing")
        {
            operation = FlowConditionOperator::Missing;
            return true;
        }
        return false;
    }

    std::string IndexedSection(const char* prefix, const std::size_t index)
    {
        return std::string(prefix) + std::to_string(index);
    }

    std::string IndexedKey(
        const std::size_t conditionIndex,
        const char* suffix)
    {
        return "condition_" + std::to_string(conditionIndex) + '_' + suffix;
    }

    const renegade::bridge::FlowNode* FindNode(
        const renegade::bridge::FlowDocument& document,
        const renegade::bridge::StableId& id)
    {
        const auto found = std::find_if(
            document.nodes.begin(),
            document.nodes.end(),
            [&id](const renegade::bridge::FlowNode& node)
            {
                return node.id == id;
            });
        return found == document.nodes.end() ? nullptr : &*found;
    }

    bool IsWithinRoot(
        const fs::path& root,
        const fs::path& candidate,
        fs::path& canonicalCandidate,
        std::string& error)
    {
        std::error_code pathError;
        const fs::path canonicalRoot = fs::weakly_canonical(root, pathError);
        if (pathError)
        {
            error = "Could not canonicalize the project root: " +
                pathError.message();
            return false;
        }

        canonicalCandidate = fs::weakly_canonical(candidate, pathError);
        if (pathError)
        {
            error = "Could not canonicalize a project document path: " +
                pathError.message();
            return false;
        }

        const fs::path relative = fs::relative(
            canonicalCandidate,
            canonicalRoot,
            pathError);
        if (pathError || relative.empty() || relative.is_absolute() ||
            std::any_of(
                relative.begin(),
                relative.end(),
                [](const fs::path& part)
                {
                    return part == "..";
                }))
        {
            error = "A document reference resolved outside the project root.";
            return false;
        }

        error.clear();
        return true;
    }

    std::vector<fs::path> EnumerateCandidateDocuments(
        const fs::path& contentRoot,
        const std::string& extension)
    {
        std::vector<fs::path> candidates;
        std::error_code scanError;
        if (!fs::is_directory(contentRoot, scanError) || scanError)
        {
            return candidates;
        }

        fs::recursive_directory_iterator iterator(
            contentRoot,
            fs::directory_options::skip_permission_denied,
            scanError);
        const fs::recursive_directory_iterator end;
        while (!scanError && iterator != end)
        {
            if (iterator->is_regular_file(scanError) && !scanError &&
                iterator->path().extension() == extension)
            {
                candidates.push_back(iterator->path());
            }
            iterator.increment(scanError);
        }
        std::sort(candidates.begin(), candidates.end());
        return candidates;
    }

    bool FindDocumentEnvelope(
        const fs::path& projectRoot,
        const renegade::bridge::StableId& expectedProjectId,
        const renegade::bridge::StableId& documentId,
        const std::string& documentType,
        const fs::path& directEnvelopePath,
        const std::string& scanExtension,
        fs::path& envelopePath,
        renegade::bridge::DocumentEnvelope& envelope,
        std::string& error)
    {
        std::vector<fs::path> matches;
        std::size_t sameIdCount = 0;
        bool foundWrongOwner = false;
        bool foundWrongType = false;

        const auto consider = [&](const fs::path& candidate)
        {
            renegade::bridge::DocumentEnvelope parsed;
            std::string readError;
            if (!renegade::bridge::ReadDocumentEnvelope(
                    candidate.generic_u8string(),
                    parsed,
                    readError) ||
                parsed.documentId != documentId)
            {
                return;
            }
            ++sameIdCount;
            if (parsed.projectId != expectedProjectId)
            {
                foundWrongOwner = true;
                return;
            }
            if (parsed.documentType != documentType)
            {
                foundWrongType = true;
                return;
            }

            matches.push_back(candidate);
            envelope = std::move(parsed);
        };

        if (!directEnvelopePath.empty() &&
            fs::is_regular_file(directEnvelopePath))
        {
            consider(directEnvelopePath);
        }

        const auto candidates = EnumerateCandidateDocuments(
            projectRoot / "Content",
            scanExtension);
        for (const auto& candidate : candidates)
        {
            if (!matches.empty() && candidate == matches.front())
            {
                continue;
            }
            if (!directEnvelopePath.empty() && candidate == directEnvelopePath)
            {
                continue;
            }
            consider(candidate);
        }

        if (sameIdCount > 1)
        {
            error = "Document ID " + documentId + " is ambiguous: " +
                std::to_string(sameIdCount) +
                " project documents claim the same stable ID.";
            return false;
        }
        if (matches.empty())
        {
            if (foundWrongOwner)
            {
                error = "Document ID " + documentId +
                    " belongs to a different project.";
            }
            else if (foundWrongType)
            {
                error = "Document ID " + documentId +
                    " has the wrong asset type; expected " + documentType + '.';
            }
            else
            {
                error = "Could not resolve " + documentType +
                    " document ID " + documentId + " inside the project.";
            }
            return false;
        }
        if (matches.size() > 1)
        {
            error = "Document ID " + documentId + " is ambiguous: " +
                std::to_string(matches.size()) +
                " matching project documents were found.";
            return false;
        }

        fs::path canonicalEnvelope;
        if (!IsWithinRoot(
                projectRoot,
                matches.front(),
                canonicalEnvelope,
                error))
        {
            return false;
        }
        envelopePath = std::move(canonicalEnvelope);
        error.clear();
        return true;
    }
}

namespace renegade::bridge
{
    StoryFlowDependencyReader MakeStoryFlowDependencyReader(
        std::string expectedProjectId)
    {
        return [projectId = std::move(expectedProjectId)](
            const std::string& path,
            StoryFlowDependencyDocument& document,
            std::string& error)
        {
            FlowDocument flow;
            if (!ReadFlowDocument(path, projectId, flow, error))
            {
                return false;
            }

            document.projectId = flow.envelope.projectId;
            document.scenePathHints.clear();
            document.screenPathHints.clear();
            for (const auto& node : flow.nodes)
            {
                if (node.kind == FlowNodeKind::Level)
                {
                    document.scenePathHints.push_back(node.scenePathHint);
                }
                else if (node.kind == FlowNodeKind::Screen)
                {
                    document.screenPathHints.push_back(node.screenPathHint);
                }
            }
            error.clear();
            return true;
        };
    }

    const char* FlowNodeKindName(const FlowNodeKind kind) noexcept
    {
        switch (kind)
        {
        case FlowNodeKind::GameStart:
            return "game_start";
        case FlowNodeKind::Level:
            return "level";
        case FlowNodeKind::Screen:
            return "screen";
        case FlowNodeKind::CompleteGame:
            return "complete_game";
        case FlowNodeKind::ReturnToMainMenu:
            return "return_to_main_menu";
        case FlowNodeKind::Quit:
            return "quit";
        default:
            return "unknown";
        }
    }

    const char* FlowConditionOperatorName(
        const FlowConditionOperator operation) noexcept
    {
        switch (operation)
        {
        case FlowConditionOperator::Equals:
            return "equals";
        case FlowConditionOperator::NotEquals:
            return "not_equals";
        case FlowConditionOperator::Exists:
            return "exists";
        case FlowConditionOperator::Missing:
            return "missing";
        default:
            return "unknown";
        }
    }

    bool ValidateFlowDocument(
        const FlowDocument& document,
        const StableId& expectedProjectId,
        std::string& error)
    {
        if (!ValidateDocumentEnvelope(document.envelope, error))
        {
            return false;
        }
        if (document.envelope.documentType != StoryFlowDocumentType)
        {
            error = "The document envelope is not a Story Flow document.";
            return false;
        }
        if (!expectedProjectId.empty() &&
            document.envelope.projectId != expectedProjectId)
        {
            error = "The Story Flow document belongs to a different project.";
            return false;
        }
        if (!IsValidStableId(document.startNodeId))
        {
            error = "Story Flow is missing a valid Game Start node ID.";
            return false;
        }
        if (document.nodes.empty())
        {
            error = "Story Flow contains no nodes.";
            return false;
        }

        std::unordered_map<StableId, const FlowNode*> nodes;
        std::size_t gameStartCount = 0;
        for (const auto& node : document.nodes)
        {
            if (!IsValidStableId(node.id))
            {
                error = "Story Flow contains a node with a malformed stable ID.";
                return false;
            }
            if (!nodes.emplace(node.id, &node).second)
            {
                error = "Story Flow contains duplicate node ID: " + node.id;
                return false;
            }
            if (node.name.empty())
            {
                error = "Story Flow contains an unnamed node: " + node.id;
                return false;
            }

            if (node.kind == FlowNodeKind::GameStart)
            {
                ++gameStartCount;
            }

            switch (node.kind)
            {
            case FlowNodeKind::Level:
                if (!IsValidStableId(node.sceneAssetId))
                {
                    error = "Level node '" + node.name +
                        "' is missing a valid scene asset ID.";
                    return false;
                }
                if (!IsSafeRelativePath(node.scenePathHint))
                {
                    error = "Level node '" + node.name +
                        "' has an unsafe scene path hint.";
                    return false;
                }
                if (!node.screenDocumentId.empty() ||
                    !node.screenPathHint.empty())
                {
                    error = "Level node '" + node.name +
                        "' cannot reference a Runtime screen document.";
                    return false;
                }
                break;

            case FlowNodeKind::Screen:
                if (!IsValidStableId(node.screenDocumentId))
                {
                    error = "Screen node '" + node.name +
                        "' is missing a valid Runtime screen document ID.";
                    return false;
                }
                if (!IsSafeRelativePath(node.screenPathHint))
                {
                    error = "Screen node '" + node.name +
                        "' has an unsafe Runtime screen path hint.";
                    return false;
                }
                if (!node.sceneAssetId.empty() || !node.scenePathHint.empty())
                {
                    error = "Screen node '" + node.name +
                        "' cannot reference a Scene document.";
                    return false;
                }
                break;

            default:
                if (!node.sceneAssetId.empty() || !node.scenePathHint.empty() ||
                    !node.screenDocumentId.empty() ||
                    !node.screenPathHint.empty())
                {
                    error = "Only Level and Screen nodes may reference project content documents.";
                    return false;
                }
                break;
            }
        }

        if (gameStartCount != 1)
        {
            error = "Story Flow must contain exactly one Game Start node.";
            return false;
        }

        const auto start = nodes.find(document.startNodeId);
        if (start == nodes.end() ||
            start->second->kind != FlowNodeKind::GameStart)
        {
            error = "Story Flow start_node does not identify its Game Start node.";
            return false;
        }

        std::unordered_set<StableId> routeIds;
        std::unordered_set<std::string> unconditionalRouteSlots;
        for (const auto& route : document.routes)
        {
            if (!IsValidStableId(route.id) ||
                !routeIds.insert(route.id).second)
            {
                error = "Story Flow contains a malformed or duplicate route ID.";
                return false;
            }
            if (!IsValidStableId(route.sourceNodeId) ||
                !IsValidStableId(route.destinationNodeId))
            {
                error = "Story Flow routes must use stable node IDs, not paths or canvas state.";
                return false;
            }

            const auto source = nodes.find(route.sourceNodeId);
            const auto destination = nodes.find(route.destinationNodeId);
            if (source == nodes.end() || destination == nodes.end())
            {
                error = "Story Flow route references a node that does not exist.";
                return false;
            }
            if (source->second->kind == FlowNodeKind::CompleteGame ||
                source->second->kind == FlowNodeKind::ReturnToMainMenu ||
                source->second->kind == FlowNodeKind::Quit)
            {
                error = "Terminal Story Flow nodes cannot have outgoing routes.";
                return false;
            }
            if (destination->second->kind == FlowNodeKind::GameStart)
            {
                error = "Story Flow routes cannot target the permanent Game Start node.";
                return false;
            }
            if (!IsValidSymbol(route.outcome))
            {
                error = "Story Flow route contains an invalid named outcome.";
                return false;
            }

            if (destination->second->kind == FlowNodeKind::Level)
            {
                if (!IsValidSymbol(route.destinationEntry))
                {
                    error = "A route entering Level node '" +
                        destination->second->name +
                        "' is missing a valid destination entry.";
                    return false;
                }
            }
            else if (!route.destinationEntry.empty())
            {
                error = "Only routes entering a Level node may name a destination entry.";
                return false;
            }

            if (route.conditions.empty())
            {
                const std::string slot = route.sourceNodeId + '|' +
                    route.outcome + '|' + std::to_string(route.priority);
                if (!unconditionalRouteSlots.insert(slot).second)
                {
                    error = "Story Flow contains ambiguous equal-priority "
                        "unconditional routes for outcome '" + route.outcome + "'.";
                    return false;
                }
            }

            for (const auto& condition : route.conditions)
            {
                if (!IsValidSymbol(condition.key))
                {
                    error = "Story Flow route contains an invalid condition key.";
                    return false;
                }
                if ((condition.operation == FlowConditionOperator::Equals ||
                     condition.operation == FlowConditionOperator::NotEquals) &&
                    condition.value.empty())
                {
                    error = "Equals and not_equals conditions require a value.";
                    return false;
                }
            }
        }

        error.clear();
        return true;
    }

    bool WriteFlowDocument(
        const std::string& filePath,
        const FlowDocument& document,
        std::string& error)
    {
        if (!ValidateFlowDocument(
                document,
                document.envelope.projectId,
                error))
        {
            return false;
        }

        const StableId expectedDocumentId = document.envelope.documentId;
        const StableId expectedProjectId = document.envelope.projectId;
        const StableId expectedStartNode = document.startNodeId;
        const std::size_t expectedNodeCount = document.nodes.size();
        const std::size_t expectedRouteCount = document.routes.size();

        return WriteTransactionalDocument(
            filePath,
            document.envelope,
            false,
            [&document](wi::config::File& file)
            {
                auto& flow = file.GetSection("flow");
                flow.Set("start_node", document.startNodeId);
                flow.Set(
                    "node_count",
                    static_cast<int>(document.nodes.size()));
                flow.Set(
                    "route_count",
                    static_cast<int>(document.routes.size()));

                for (std::size_t index = 0;
                    index < document.nodes.size(); ++index)
                {
                    const auto& node = document.nodes[index];
                    auto& section = file.GetSection(
                        IndexedSection("node_", index).c_str());
                    section.Set("id", node.id);
                    section.Set("kind", FlowNodeKindName(node.kind));
                    section.Set("name", node.name);
                    section.Set("scene_asset_id", node.sceneAssetId);
                    section.Set("scene_path_hint", node.scenePathHint);
                    section.Set("screen_document_id", node.screenDocumentId);
                    section.Set("screen_path_hint", node.screenPathHint);
                }

                for (std::size_t index = 0;
                    index < document.routes.size(); ++index)
                {
                    const auto& route = document.routes[index];
                    auto& section = file.GetSection(
                        IndexedSection("route_", index).c_str());
                    section.Set("id", route.id);
                    section.Set("source", route.sourceNodeId);
                    section.Set("outcome", route.outcome);
                    section.Set("destination", route.destinationNodeId);
                    section.Set("destination_entry", route.destinationEntry);
                    section.Set("priority", route.priority);
                    section.Set(
                        "condition_count",
                        static_cast<int>(route.conditions.size()));

                    for (std::size_t conditionIndex = 0;
                        conditionIndex < route.conditions.size();
                        ++conditionIndex)
                    {
                        const auto& condition = route.conditions[conditionIndex];
                        section.Set(
                            IndexedKey(conditionIndex, "key").c_str(),
                            condition.key);
                        section.Set(
                            IndexedKey(conditionIndex, "operator").c_str(),
                            FlowConditionOperatorName(condition.operation));
                        section.Set(
                            IndexedKey(conditionIndex, "value").c_str(),
                            condition.value);
                    }
                }
            },
            [
                expectedDocumentId,
                expectedProjectId,
                expectedStartNode,
                expectedNodeCount,
                expectedRouteCount](
                    const std::string& path,
                    std::string& validationError)
            {
                FlowDocument roundTrip;
                if (!ReadFlowDocument(
                        path,
                        expectedProjectId,
                        roundTrip,
                        validationError))
                {
                    return false;
                }
                if (roundTrip.envelope.documentId != expectedDocumentId ||
                    roundTrip.startNodeId != expectedStartNode ||
                    roundTrip.nodes.size() != expectedNodeCount ||
                    roundTrip.routes.size() != expectedRouteCount)
                {
                    validationError =
                        "The Story Flow document did not round-trip exactly.";
                    return false;
                }
                validationError.clear();
                return true;
            },
            error);
    }

    bool ReadFlowDocument(
        const std::string& filePath,
        const StableId& expectedProjectId,
        FlowDocument& document,
        std::string& error)
    {
        DocumentEnvelope envelope;
        if (!ReadDocumentEnvelope(filePath, envelope, error))
        {
            return false;
        }

        try
        {
            wi::config::File file;
            if (!file.Open(filePath))
            {
                error = "Could not read Story Flow document: " + filePath;
                return false;
            }
            if (!file.HasSection("flow"))
            {
                error = "The Story Flow document is missing its flow section.";
                return false;
            }

            FlowDocument parsed;
            parsed.envelope = std::move(envelope);
            const auto& flow = file.GetSection("flow");
            parsed.startNodeId = flow.GetText("start_node");
            const int nodeCount = flow.GetInt("node_count");
            const int routeCount = flow.GetInt("route_count");
            if (nodeCount <= 0 || routeCount < 0 ||
                nodeCount > 100000 || routeCount > 1000000)
            {
                error = "The Story Flow document contains invalid node or route counts.";
                return false;
            }

            parsed.nodes.reserve(static_cast<std::size_t>(nodeCount));
            for (int index = 0; index < nodeCount; ++index)
            {
                const std::string sectionName = IndexedSection(
                    "node_",
                    static_cast<std::size_t>(index));
                if (!file.HasSection(sectionName.c_str()))
                {
                    error = "The Story Flow document is missing section " +
                        sectionName + '.';
                    return false;
                }

                const auto& section = file.GetSection(sectionName.c_str());
                FlowNode node;
                node.id = section.GetText("id");
                if (!TryParseNodeKind(section.GetText("kind"), node.kind))
                {
                    error = "Story Flow contains an unsupported node kind.";
                    return false;
                }
                node.name = section.GetText("name");
                node.sceneAssetId = section.GetText("scene_asset_id");
                node.scenePathHint = section.GetText("scene_path_hint");
                node.screenDocumentId = section.GetText("screen_document_id");
                node.screenPathHint = section.GetText("screen_path_hint");
                parsed.nodes.push_back(std::move(node));
            }

            parsed.routes.reserve(static_cast<std::size_t>(routeCount));
            for (int index = 0; index < routeCount; ++index)
            {
                const std::string sectionName = IndexedSection(
                    "route_",
                    static_cast<std::size_t>(index));
                if (!file.HasSection(sectionName.c_str()))
                {
                    error = "The Story Flow document is missing section " +
                        sectionName + '.';
                    return false;
                }

                const auto& section = file.GetSection(sectionName.c_str());
                FlowRoute route;
                route.id = section.GetText("id");
                route.sourceNodeId = section.GetText("source");
                route.outcome = section.GetText("outcome");
                route.destinationNodeId = section.GetText("destination");
                route.destinationEntry = section.GetText("destination_entry");
                route.priority = section.GetInt("priority");

                const int conditionCount = section.GetInt("condition_count");
                if (conditionCount < 0 || conditionCount > 10000)
                {
                    error = "Story Flow route contains an invalid condition count.";
                    return false;
                }
                route.conditions.reserve(
                    static_cast<std::size_t>(conditionCount));
                for (int conditionIndex = 0;
                    conditionIndex < conditionCount;
                    ++conditionIndex)
                {
                    FlowCondition condition;
                    condition.key = section.GetText(
                        IndexedKey(
                            static_cast<std::size_t>(conditionIndex),
                            "key").c_str());
                    if (!TryParseConditionOperator(
                            section.GetText(
                                IndexedKey(
                                    static_cast<std::size_t>(conditionIndex),
                                    "operator").c_str()),
                            condition.operation))
                    {
                        error = "Story Flow contains an unsupported condition operator.";
                        return false;
                    }
                    condition.value = section.GetText(
                        IndexedKey(
                            static_cast<std::size_t>(conditionIndex),
                            "value").c_str());
                    route.conditions.push_back(std::move(condition));
                }
                parsed.routes.push_back(std::move(route));
            }

            if (!ValidateFlowDocument(parsed, expectedProjectId, error))
            {
                return false;
            }

            document = std::move(parsed);
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error = std::string("Could not read Story Flow document: ") +
                exception.what();
            return false;
        }
    }

    bool ResolveStoryFlowDocumentPath(
        const std::string& projectRoot,
        const StableId& expectedProjectId,
        const StableId& documentId,
        const std::string& pathHint,
        std::string& resolvedPath,
        std::string& error)
    {
        if (!IsValidStableId(expectedProjectId) ||
            !IsValidStableId(documentId))
        {
            error = "Story Flow reference is missing a valid project or document ID.";
            return false;
        }
        if (!IsSafeRelativePath(pathHint))
        {
            error = "Story Flow path hint must be project-relative.";
            return false;
        }

        const fs::path root = fs::u8path(projectRoot);
        fs::path envelopePath;
        DocumentEnvelope envelope;
        if (!FindDocumentEnvelope(
                root,
                expectedProjectId,
                documentId,
                StoryFlowDocumentType,
                root / fs::u8path(pathHint),
                ".renegade-flow",
                envelopePath,
                envelope,
                error))
        {
            return false;
        }

        resolvedPath = envelopePath.generic_u8string();
        error.clear();
        return true;
    }

    bool ResolveSceneDocumentPath(
        const std::string& projectRoot,
        const StableId& expectedProjectId,
        const StableId& documentId,
        const std::string& pathHint,
        std::string& resolvedPath,
        std::string& error)
    {
        if (!IsValidStableId(expectedProjectId) ||
            !IsValidStableId(documentId))
        {
            error = "Scene reference is missing a valid project or document ID.";
            return false;
        }
        if (!IsSafeRelativePath(pathHint))
        {
            error = "Scene path hint must be project-relative.";
            return false;
        }

        const fs::path root = fs::u8path(projectRoot);
        fs::path envelopePath;
        DocumentEnvelope envelope;
        fs::path directSidecar = root / fs::u8path(pathHint);
        directSidecar += ".rmeta";
        if (!FindDocumentEnvelope(
                root,
                expectedProjectId,
                documentId,
                SceneDocumentType,
                directSidecar,
                ".rmeta",
                envelopePath,
                envelope,
                error))
        {
            return false;
        }

        fs::path scenePath;
        if (!IsWithinRoot(
                root,
                root / fs::u8path(envelope.pathHint),
                scenePath,
                error))
        {
            return false;
        }
        if (!fs::is_regular_file(scenePath))
        {
            error = "Resolved Scene document is missing: " +
                scenePath.generic_u8string();
            return false;
        }

        resolvedPath = scenePath.generic_u8string();
        error.clear();
        return true;
    }

    const char* FlowStepCodeName(const FlowStepCode code) noexcept
    {
        switch (code)
        {
        case FlowStepCode::Success:
            return "SUCCESS";
        case FlowStepCode::NotStarted:
            return "NOT_STARTED";
        case FlowStepCode::InvalidOutcome:
            return "INVALID_OUTCOME";
        case FlowStepCode::MissingRoute:
            return "MISSING_ROUTE";
        case FlowStepCode::AmbiguousRoute:
            return "AMBIGUOUS_ROUTE";
        default:
            return "UNKNOWN";
        }
    }

    const char* FlowTerminalActionName(
        const FlowTerminalAction action) noexcept
    {
        switch (action)
        {
        case FlowTerminalAction::None:
            return "none";
        case FlowTerminalAction::CompleteGame:
            return "complete_game";
        case FlowTerminalAction::ReturnToMainMenu:
            return "return_to_main_menu";
        case FlowTerminalAction::Quit:
            return "quit";
        default:
            return "unknown";
        }
    }

    bool FlowInterpreter::Initialize(
        FlowDocument document,
        std::string& error)
    {
        if (!ValidateFlowDocument(
                document,
                document.envelope.projectId,
                error))
        {
            initialized_ = false;
            started_ = false;
            currentNodeId_.clear();
            return false;
        }

        document_ = std::move(document);
        state_.clear();
        currentNodeId_.clear();
        initialized_ = true;
        started_ = false;
        error.clear();
        return true;
    }

    bool FlowInterpreter::SetState(
        std::string key,
        std::string value,
        std::string& error)
    {
        if (!IsValidSymbol(key))
        {
            error = "Flow state keys must use the same stable symbol syntax as outcomes.";
            return false;
        }
        state_[std::move(key)] = std::move(value);
        error.clear();
        return true;
    }

    void FlowInterpreter::RemoveState(const std::string& key)
    {
        state_.erase(key);
    }

    void FlowInterpreter::ClearState() noexcept
    {
        state_.clear();
    }

    FlowStepResult FlowInterpreter::Start()
    {
        if (!initialized_)
        {
            FlowStepResult result;
            result.message = "Story Flow interpreter has not been initialized.";
            return result;
        }

        const FlowNode* start = FindNode(document_.startNodeId);
        if (start == nullptr)
        {
            FlowStepResult result;
            result.code = FlowStepCode::NotStarted;
            result.message = "Story Flow Game Start node could not be resolved.";
            return result;
        }

        started_ = true;
        return EnterNode(*start, nullptr, {}, {});
    }

    FlowStepResult FlowInterpreter::EmitOutcome(const std::string& outcome)
    {
        if (!initialized_ || !started_)
        {
            FlowStepResult result;
            result.message = "Story Flow must be started before an outcome is emitted.";
            return result;
        }
        if (!IsValidSymbol(outcome))
        {
            FlowStepResult result;
            result.code = FlowStepCode::InvalidOutcome;
            result.message = "Invalid Story Flow outcome: " + outcome;
            result.currentNodeId = currentNodeId_;
            return result;
        }

        const FlowNode* current = FindNode(currentNodeId_);
        if (current == nullptr)
        {
            FlowStepResult result;
            result.code = FlowStepCode::NotStarted;
            result.message = "Current Story Flow node could not be resolved.";
            return result;
        }
        if (current->kind == FlowNodeKind::CompleteGame ||
            current->kind == FlowNodeKind::ReturnToMainMenu ||
            current->kind == FlowNodeKind::Quit)
        {
            FlowStepResult result;
            result.code = FlowStepCode::MissingRoute;
            result.message = "Terminal Story Flow node '" + current->name +
                "' cannot emit outcome '" + outcome + "'.";
            result.currentNodeId = current->id;
            result.currentNodeName = current->name;
            result.currentNodeKind = current->kind;
            result.outcome = outcome;
            return result;
        }

        int selectedPriority = std::numeric_limits<int>::max();
        std::vector<const FlowRoute*> candidates;
        for (const auto& route : document_.routes)
        {
            if (route.sourceNodeId != currentNodeId_ ||
                route.outcome != outcome ||
                !ConditionsMatch(route))
            {
                continue;
            }

            if (route.priority < selectedPriority)
            {
                selectedPriority = route.priority;
                candidates.clear();
                candidates.push_back(&route);
            }
            else if (route.priority == selectedPriority)
            {
                candidates.push_back(&route);
            }
        }

        if (candidates.empty())
        {
            FlowStepResult result;
            result.code = FlowStepCode::MissingRoute;
            result.message = "No Story Flow route from '" + current->name +
                "' accepts outcome '" + outcome + "'.";
            result.currentNodeId = current->id;
            result.currentNodeName = current->name;
            result.currentNodeKind = current->kind;
            result.outcome = outcome;
            return result;
        }
        if (candidates.size() > 1)
        {
            FlowStepResult result;
            result.code = FlowStepCode::AmbiguousRoute;
            result.message = "Story Flow outcome '" + outcome + "' from '" +
                current->name + "' matched " +
                std::to_string(candidates.size()) +
                " routes at priority " + std::to_string(selectedPriority) + '.';
            result.currentNodeId = current->id;
            result.currentNodeName = current->name;
            result.currentNodeKind = current->kind;
            result.outcome = outcome;
            return result;
        }

        const FlowRoute& route = *candidates.front();
        const FlowNode* destination = FindNode(route.destinationNodeId);
        if (destination == nullptr)
        {
            FlowStepResult result;
            result.code = FlowStepCode::MissingRoute;
            result.message = "Selected Story Flow route destination could not be resolved.";
            result.currentNodeId = current->id;
            return result;
        }

        return EnterNode(
            *destination,
            &route,
            outcome,
            current->id);
    }

    const FlowDocument& FlowInterpreter::Document() const noexcept
    {
        return document_;
    }

    const FlowNode* FlowInterpreter::CurrentNode() const noexcept
    {
        return FindNode(currentNodeId_);
    }

    bool FlowInterpreter::HasStarted() const noexcept
    {
        return started_;
    }

    const FlowNode* FlowInterpreter::FindNode(const StableId& id) const noexcept
    {
        return ::FindNode(document_, id);
    }

    bool FlowInterpreter::ConditionsMatch(const FlowRoute& route) const
    {
        return std::all_of(
            route.conditions.begin(),
            route.conditions.end(),
            [this](const FlowCondition& condition)
            {
                const auto found = state_.find(condition.key);
                switch (condition.operation)
                {
                case FlowConditionOperator::Equals:
                    return found != state_.end() &&
                        found->second == condition.value;
                case FlowConditionOperator::NotEquals:
                    return found == state_.end() ||
                        found->second != condition.value;
                case FlowConditionOperator::Exists:
                    return found != state_.end();
                case FlowConditionOperator::Missing:
                    return found == state_.end();
                default:
                    return false;
                }
            });
    }

    FlowStepResult FlowInterpreter::EnterNode(
        const FlowNode& node,
        const FlowRoute* route,
        std::string outcome,
        StableId previousNodeId)
    {
        currentNodeId_ = node.id;

        FlowStepResult result;
        result.succeeded = true;
        result.code = FlowStepCode::Success;
        result.previousNodeId = std::move(previousNodeId);
        result.currentNodeId = node.id;
        result.currentNodeName = node.name;
        result.currentNodeKind = node.kind;
        result.outcome = std::move(outcome);
        result.sceneAssetId = node.sceneAssetId;
        result.scenePathHint = node.scenePathHint;
        result.screenDocumentId = node.screenDocumentId;
        result.screenPathHint = node.screenPathHint;
        if (route != nullptr)
        {
            result.routeId = route->id;
            result.destinationEntry = route->destinationEntry;
        }

        switch (node.kind)
        {
        case FlowNodeKind::CompleteGame:
            result.terminalAction = FlowTerminalAction::CompleteGame;
            break;
        case FlowNodeKind::ReturnToMainMenu:
            result.terminalAction = FlowTerminalAction::ReturnToMainMenu;
            break;
        case FlowNodeKind::Quit:
            result.terminalAction = FlowTerminalAction::Quit;
            break;
        default:
            result.terminalAction = FlowTerminalAction::None;
            break;
        }

        result.message = "Entered Story Flow node: " + node.name;
        return result;
    }
}
