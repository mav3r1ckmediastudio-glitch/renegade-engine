#pragma once

#include "renegade/bridge/IdentityService.h"

#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    inline constexpr const char* RuntimeScreenDocumentType = "runtime-screen";
    inline constexpr const char* RuntimeScreenPlayAction = "play";
    inline constexpr const char* RuntimeScreenQuitAction = "quit";
    inline constexpr const char* BuiltinScreenFont =
        "builtin:liberation-sans";

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

    struct ScreenColor
    {
        std::uint8_t red = 255;
        std::uint8_t green = 255;
        std::uint8_t blue = 255;
        std::uint8_t alpha = 255;
    };

    enum class ScreenHorizontalAlignment
    {
        Left,
        Center,
        Right,
    };

    enum class ScreenVerticalAlignment
    {
        Top,
        Center,
        Bottom,
    };

    enum class ScreenCanvasScaleMode
    {
        Fit,
        Fill,
        Stretch,
    };

    enum class ScreenLayoutMode
    {
        Absolute,
        Anchored,
    };

    struct ScreenTextStyle
    {
        // Fonts are governed resources. Built-in identities are explicit and
        // replaceable; project fonts use a safe Content-relative .ttf path.
        std::string fontResource = BuiltinScreenFont;
        float fontSize = 24.0f;
        float characterSpacing = 0.0f;
        float lineSpacing = 0.0f;
        float softness = 0.0f;
        float bolden = 0.0f;
        ScreenColor shadowColor{0, 0, 0, 0};
        float shadowOffsetX = 0.0f;
        float shadowOffsetY = 0.0f;
        float shadowSoftness = 0.5f;
        float shadowBolden = 0.1f;
        ScreenHorizontalAlignment horizontalAlignment =
            ScreenHorizontalAlignment::Left;
        ScreenVerticalAlignment verticalAlignment =
            ScreenVerticalAlignment::Top;
        bool wrap = false;
    };

    struct ScreenVisualState
    {
        ScreenColor background{0, 0, 0, 0};
        ScreenColor foreground{255, 255, 255, 255};
        ScreenColor imageTint{255, 255, 255, 255};
        // Empty inherits the widget's primary resource. A non-empty value is
        // a project-contained state image selected by the shared renderer.
        std::string imageResourcePath;
    };

    struct ScreenWidgetStyle
    {
        ScreenVisualState normal;
        ScreenVisualState hover;
        ScreenVisualState pressed;
        ScreenVisualState focused;
        ScreenVisualState disabled;
        ScreenColor borderColor{0, 0, 0, 0};
        float borderWidth = 0.0f;
        float cornerRadius = 0.0f;
        float opacity = 1.0f;
        ScreenTextStyle text;
    };

    struct ScreenAnchors
    {
        float minimumX = 0.0f;
        float minimumY = 0.0f;
        float maximumX = 0.0f;
        float maximumY = 0.0f;
        float offsetMinimumX = 0.0f;
        float offsetMinimumY = 0.0f;
        float offsetMaximumX = 0.0f;
        float offsetMaximumY = 0.0f;
    };

    struct ScreenCanvasTransform
    {
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
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
        // Empty means the design canvas. A parent establishes a proper Screen
        // scene graph without exposing Wicked widget ownership to documents.
        StableId parentId;
        ScreenLayoutMode layoutMode = ScreenLayoutMode::Absolute;
        ScreenAnchors anchors;
        ScreenWidgetStyle style;
    };

    struct ScreenDocument
    {
        static constexpr std::uint32_t CurrentSchemaVersion = 2;

        DocumentEnvelope envelope;
        std::uint32_t schemaVersion = CurrentSchemaVersion;
        std::uint32_t migratedFromVersion = 0;
        float designWidth = 1280.0f;
        float designHeight = 720.0f;
        ScreenCanvasScaleMode scaleMode = ScreenCanvasScaleMode::Fit;
        std::vector<ScreenAction> actions;
        std::vector<ScreenWidget> widgets;
        std::vector<StableId> focusOrder;
    };

    [[nodiscard]] const char* ScreenWidgetKindName(
        ScreenWidgetKind kind) noexcept;

    // Explicit editable template values used by new documents and the
    // lossless schema-v1 reader. Runtime presentation never supplies style.
    [[nodiscard]] ScreenWidgetStyle MakeScreenWidgetStyleTemplate(
        ScreenWidgetKind kind,
        float widgetHeight);

    // Resolves absolute or anchored widget geometry through the authored
    // parent graph into design-canvas coordinates.
    [[nodiscard]] bool ResolveScreenWidgetRect(
        const ScreenDocument& document,
        const StableId& widgetId,
        ScreenRect& rect,
        std::string& error);

    [[nodiscard]] bool ResolveScreenCanvasTransform(
        const ScreenDocument& document,
        float outputWidth,
        float outputHeight,
        ScreenCanvasTransform& transform,
        std::string& error);

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
