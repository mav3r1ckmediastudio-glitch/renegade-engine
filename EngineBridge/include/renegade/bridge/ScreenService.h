#pragma once

#include "renegade/bridge/IdentityService.h"

#include <string>
#include <vector>

namespace renegade::bridge
{
    inline constexpr const char* RuntimeScreenDocumentType = "runtime-screen";
    inline constexpr const char* RuntimeScreenPlayAction = "play";
    inline constexpr const char* RuntimeScreenQuitAction = "quit";

    enum class ScreenWidgetKind
    {
        Image,
        Text,
        Button,
    };

    struct ScreenRect
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    struct ScreenAction
    {
        // Stable symbolic identity. Actions deliberately do not contain scene
        // paths, flow destinations or editor-canvas semantics.
        std::string id;
    };

    struct ScreenWidget
    {
        StableId id;
        ScreenWidgetKind kind = ScreenWidgetKind::Text;
        std::string name;
        ScreenRect rect;
        bool visible = true;
        bool enabled = true;
        std::string text;
        std::string resourcePath;
        std::string actionId;
    };

    struct ScreenDocument
    {
        DocumentEnvelope envelope;
        float designWidth = 1280.0f;
        float designHeight = 720.0f;
        std::vector<ScreenAction> actions;
        std::vector<ScreenWidget> widgets;
        std::vector<StableId> focusOrder;
    };

    [[nodiscard]] const char* ScreenWidgetKindName(
        ScreenWidgetKind kind) noexcept;

    [[nodiscard]] bool ValidateScreenDocument(
        const ScreenDocument& document,
        const StableId& expectedProjectId,
        std::string& error);
    [[nodiscard]] bool WriteScreenDocument(
        const std::string& filePath,
        const ScreenDocument& document,
        std::string& error);
    [[nodiscard]] bool ReadScreenDocument(
        const std::string& filePath,
        const StableId& expectedProjectId,
        ScreenDocument& document,
        std::string& error);

    // Stable document ID is authoritative. pathHint is only the current
    // project-relative location and a deterministic Content scan can repair it
    // after rename or move.
    [[nodiscard]] bool ResolveRuntimeScreenDocumentPath(
        const std::string& projectRoot,
        const StableId& expectedProjectId,
        const StableId& documentId,
        const std::string& pathHint,
        std::string& resolvedPath,
        std::string& error);

    // Resolves one explicit project-relative visual dependency. LP03 does not
    // infer dependencies by scanning strings and does not permit escape from
    // the project Content tree.
    [[nodiscard]] bool ResolveScreenResourcePath(
        const std::string& projectRoot,
        const std::string& resourcePath,
        std::string& resolvedPath,
        std::string& error);
}
