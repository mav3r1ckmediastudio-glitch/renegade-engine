#pragma once

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScreenBinding.h"
#include "renegade/bridge/ScreenAnimation.h"
#include "renegade/bridge/ScreenTransition.h"
#include "renegade/bridge/ScreenControl.h"
#include "renegade/bridge/ScreenInteraction.h"
#include "renegade/bridge/ScreenComposition.h"
#include <optional>
#include <algorithm>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace renegade::bridge
{
    inline constexpr const char* RuntimeScreenDocumentType = "runtime-screen";
    inline constexpr const char* RuntimeScreenPlayAction = "play";
    inline constexpr const char* RuntimeScreenQuitAction = "quit";
    inline constexpr const char* RuntimeUiReducedMotionToggleAction = "ui.reduced_motion.toggle";
    inline constexpr const char* RuntimeUiReducedMotionOnAction = "ui.reduced_motion.on";
    inline constexpr const char* RuntimeUiReducedMotionOffAction = "ui.reduced_motion.off";
    inline constexpr const char* RuntimeUiTextScaleIncreaseAction = "ui.text_scale.increase";
    inline constexpr const char* RuntimeUiTextScaleDecreaseAction = "ui.text_scale.decrease";
    inline constexpr const char* RuntimeUiTextScaleResetAction = "ui.text_scale.reset";
    inline constexpr const char* RuntimeUiContrastCycleAction = "ui.contrast.cycle";
    inline constexpr const char* RuntimeUiFocusColorCycleAction = "ui.focus_color.cycle";
    inline constexpr const char* RuntimeUiFocusWidthCycleAction = "ui.focus_width.cycle";
    inline constexpr const char* RuntimeUiScaleIncreaseAction = "ui.scale.increase";
    inline constexpr const char* RuntimeUiScaleDecreaseAction = "ui.scale.decrease";
    inline constexpr const char* RuntimeUiScaleResetAction = "ui.scale.reset";
    inline constexpr const char* RuntimeUiTargetSizeCycleAction = "ui.target_size.cycle";
    inline constexpr bool IsRuntimeUiAccessibilityAction(std::string_view action) noexcept
    {
        return action==RuntimeUiReducedMotionToggleAction||
            action==RuntimeUiReducedMotionOnAction||
            action==RuntimeUiReducedMotionOffAction||
            action==RuntimeUiTextScaleIncreaseAction||
            action==RuntimeUiTextScaleDecreaseAction||
            action==RuntimeUiTextScaleResetAction||
            action==RuntimeUiContrastCycleAction||
            action==RuntimeUiFocusColorCycleAction||
            action==RuntimeUiFocusWidthCycleAction||action==RuntimeUiScaleIncreaseAction||
            action==RuntimeUiScaleDecreaseAction||action==RuntimeUiScaleResetAction||
            action==RuntimeUiTargetSizeCycleAction;
    }
    inline constexpr const char* BuiltinScreenFont =
        "builtin:liberation-sans";

    enum class ScreenWidgetKind
    {
        Image,
        Text,
        Button,
        Progress,
        Toggle, Slider, Selector, TextInput,
    };

    inline bool IsScreenValueControl(ScreenWidgetKind kind) noexcept
    {return kind==ScreenWidgetKind::Toggle||kind==ScreenWidgetKind::Slider||
        kind==ScreenWidgetKind::Selector||kind==ScreenWidgetKind::TextInput;}
    inline bool IsScreenInteractive(ScreenWidgetKind kind) noexcept
    {return kind==ScreenWidgetKind::Button||IsScreenValueControl(kind);}
    inline ScreenControlKind ScreenControlKindFor(ScreenWidgetKind kind) noexcept
    {
        switch(kind){case ScreenWidgetKind::Toggle:return ScreenControlKind::Toggle;
        case ScreenWidgetKind::Slider:return ScreenControlKind::Slider;
        case ScreenWidgetKind::Selector:return ScreenControlKind::Selector;
        default:return ScreenControlKind::TextInput;}
    }

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

    // Optional v3 design-pixel inset for root-anchored, responsive elements.
    // Absolute artboards and old Screen documents retain their prior geometry.
    struct ScreenSafeArea
    {
        float left = 0.0f, top = 0.0f, right = 0.0f, bottom = 0.0f;
    };
    [[nodiscard]] bool ValidateScreenSafeArea(const ScreenSafeArea& area,
        float designWidth, float designHeight, std::string& error);

    // Zero maxima mean unbounded. Constraints apply to the final layout
    // bounds of this widget (including container-assigned cells); authored
    // geometry is never overwritten when the viewport or parent changes.
    struct ScreenSizeConstraints
    {
        float minimumWidth=0.0f,minimumHeight=0.0f;
        float maximumWidth=0.0f,maximumHeight=0.0f;
    };
    [[nodiscard]] bool ValidateScreenSizeConstraints(
        const ScreenSizeConstraints& constraints,std::string& error);

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

    enum class ScreenFillDirection { LeftToRight, RightToLeft, BottomToTop, TopToBottom };

    // Pure geometry used by both GPU-independent frame evidence and drawing.
    [[nodiscard]] ScreenRect ResolveScreenFillRect(const ScreenRect& bounds,
        float fraction, ScreenFillDirection direction, float borderWidth) noexcept;

    enum class ScreenChildLayout { Free, Horizontal, Vertical, Grid };
    struct ScreenContainerLayout
    {
        ScreenChildLayout mode = ScreenChildLayout::Free;
        float padding = 8.0f;
        float gap = 8.0f;
        int columns = 2;
        // Optional narrow-viewport child arrangement; zero disables it.
        // Baseline/manual widget geometry is retained for wider viewports.
        float compactBelowWidth = 0;
        ScreenChildLayout compactMode = ScreenChildLayout::Vertical;
        int compactColumns = 1;
    };
    [[nodiscard]] inline ScreenChildLayout EffectiveScreenChildLayout(
        const ScreenContainerLayout& layout,float logicalWidth) noexcept
    {return layout.compactBelowWidth>0&&logicalWidth<layout.compactBelowWidth?
        layout.compactMode:layout.mode;}

    [[nodiscard]] bool ValidateScreenContainerLayout(
        const ScreenContainerLayout& layout, std::string& error);
    [[nodiscard]] const char* ScreenChildLayoutName(ScreenChildLayout mode) noexcept;

    // Linked instances retain their own geometry, identity, focus and Flow action.
    // Each reference follows one exact, same-kind widget in a project Screen.
    struct ScreenComponentReference
    {
        StableId documentId;
        std::string pathHint;
        StableId widgetId;
        // Style groups 1..32; Text=64, Control=128, Bindings=256,
        // Progress=512. Set bits are instance overrides; container layout stays local.
        std::uint32_t overrides=0;
        // Optional linked hierarchy membership. Every member stores the
        // instance root identity; empty preserves legacy single-widget links.
        StableId treeRootId;
    };

    enum class ScreenImageFit { Stretch, Contain, Cover };

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
        ScreenSizeConstraints sizeConstraints;
        ScreenWidgetStyle style;
        // Authored references only; no captured live gameplay state.
        std::vector<ScreenBinding> bindings;
        float fill = 0.5f;
        ScreenFillDirection fillDirection = ScreenFillDirection::LeftToRight;
        // Optional project-contained artwork; empty keeps the legacy solid-colour progress bar.
        std::string progressFrameResourcePath;
        std::string progressFillResourcePath;
        // Additive v3 contract. Free preserves existing absolute/anchor semantics.
        // Flow containers allocate equal cells to immediate children in paint order.
        ScreenContainerLayout container;
        std::vector<ScreenAnimationTrack> animations;
        // Colors=1 Border=2 Typography=4 Images=8 Opacity=16 Animations=32.
        // Old widgets remain wholly local until the creator chooses inheritance.
        std::uint32_t themeOverrides=63;
        ScreenControlData control;
        std::vector<ScreenLocalAction> interactions;
        std::optional<ScreenComponentReference> component;
        // A button may execute local UI interactions without dispatching its
        // declared Flow action. Defaults to legacy Flow-after-local behavior.
        bool localOnly = false;
        // Navigation policy stays local even on linked component instances.
        std::optional<ScreenTransitionPair> transitionOverride;
        // Optional v3 native image presentation; absent legacy field stretches artwork.
        ScreenImageFit imageFit=ScreenImageFit::Stretch;

    };

    inline ScreenRect FitScreenImage(ScreenRect bounds,float width,float height,ScreenImageFit fit) {
        if(width<=0||height<=0||fit==ScreenImageFit::Stretch)return bounds;
        const float sx=bounds.width/width,sy=bounds.height/height;
        const float factor=fit==ScreenImageFit::Contain?std::min(sx,sy):std::max(sx,sy);
        const float w=width*factor,h=height*factor;
        return {bounds.x+(bounds.width-w)*0.5f,bounds.y+(bounds.height-h)*0.5f,w,h};
    }

    enum class ScreenUsage { Menu, Hud, Overlay, Modal };
    [[nodiscard]] inline const char* ScreenUsageName(ScreenUsage usage) noexcept
    {
        switch (usage) {
        case ScreenUsage::Menu: return "menu";
        case ScreenUsage::Hud: return "hud";
        case ScreenUsage::Overlay: return "overlay";
        case ScreenUsage::Modal: return "modal";
        }
        return "unknown";
    }

    struct ScreenThemeReference
    {
        StableId documentId;
        std::string pathHint;
    };

    struct ScreenDocument
    {
        static constexpr std::uint32_t CurrentSchemaVersion = 3;
        static constexpr std::uint32_t MaximumActions = 128;

        DocumentEnvelope envelope;
        std::uint32_t schemaVersion = CurrentSchemaVersion;
        std::uint32_t migratedFromVersion = 0;
        float designWidth = 1280.0f;
        float designHeight = 720.0f;
        ScreenCanvasScaleMode scaleMode = ScreenCanvasScaleMode::Fit;
        ScreenSafeArea safeArea;
        std::vector<ScreenAction> actions;
        std::vector<ScreenWidget> widgets;
        std::vector<StableId> focusOrder;
        // v3 adds explicit passive usages. v1/v2 remain menu documents.
        ScreenUsage usage = ScreenUsage::Menu;
        bool enabledInGame = true;
        std::optional<ScreenCompositionPolicy> composition;
        // A project-owned Screen supplies one editable style exemplar per widget kind.
        std::optional<ScreenThemeReference> theme;
        ScreenTransitionPair transitions;
    };

    inline ScreenCompositionLayer ScreenLayerForUsage(ScreenUsage usage) noexcept
    {
        switch(usage)
        {
        case ScreenUsage::Hud:return ScreenCompositionLayer::Hud;
        case ScreenUsage::Overlay:return ScreenCompositionLayer::Overlay;
        case ScreenUsage::Modal:return ScreenCompositionLayer::Modal;
        default:return ScreenCompositionLayer::Screen;
        }
    }
    inline ScreenCompositionPolicy EffectiveScreenCompositionPolicy(const ScreenDocument& document)
    {
        return document.composition.value_or(DefaultScreenCompositionPolicy(ScreenLayerForUsage(document.usage)));
    }

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

    // Resolve every widget once in document paint order, sharing parent/cache
    // and sibling indexes. Failure leaves the caller's previous frame intact.
    [[nodiscard]] bool ResolveScreenWidgetRects(const ScreenDocument& document,
        std::vector<ScreenRect>& rects,std::string& error,float layoutWidth=0);

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
    // Preserve original bytes before an explicit schema upgrade. Existing
    // originals are immutable; conflicting content fails instead of overwriting.
    [[nodiscard]] bool PreserveScreenUpgradeOriginal(
        const std::string& filePath, const ScreenDocument& candidate, std::string& error);
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
