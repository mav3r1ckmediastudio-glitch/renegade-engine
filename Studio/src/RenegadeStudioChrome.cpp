#include "RenegadeStudioChrome.h"

#include <algorithm>
#include <array>
#include <utility>

namespace
{
    constexpr float TopBarHeight = 64.0f;
    constexpr float SceneTabsHeight = 34.0f;
    constexpr float BottomTabsHeight = 32.0f;
    constexpr float StatusBarHeight = 28.0f;
    constexpr float PanelHeaderHeight = 43.0f;

    constexpr wi::Color Surface0 = wi::Color(8, 11, 13, 252);
    constexpr wi::Color Surface2 = wi::Color(16, 23, 28, 255);
    constexpr wi::Color Border = wi::Color(38, 52, 61, 255);
    constexpr wi::Color BorderSoft = wi::Color(25, 36, 43, 255);
    constexpr wi::Color Text = wi::Color(210, 200, 188, 255);
    constexpr wi::Color TextStrong = wi::Color(244, 239, 233, 255);
    constexpr wi::Color TextSecondary = wi::Color(170, 179, 184, 255);
    constexpr wi::Color Muted = wi::Color(102, 117, 126, 255);
    constexpr wi::Color Forge = wi::Color(210, 91, 29, 255);
    constexpr wi::Color TechCyan = wi::Color(56, 183, 215, 255);
    constexpr wi::Color Success = wi::Color(76, 195, 138, 255);

    void DrawRect(
        const float x,
        const float y,
        const float width,
        const float height,
        const wi::Color color,
        const wi::graphics::CommandList cmd)
    {
        wi::image::Params params(x, y, width, height, color);
        params.blendFlag = wi::enums::BLENDMODE_ALPHA;
        wi::image::Draw(nullptr, params, cmd);
    }

    void DrawText(
        const std::string& text,
        const float x,
        const float y,
        const int size,
        const wi::Color color,
        const wi::graphics::CommandList cmd,
        const float tracking = 0.0f,
        const float bolden = 0.0f)
    {
        wi::font::Params params(
            x,
            y,
            size,
            wi::font::WIFALIGN_LEFT,
            wi::font::WIFALIGN_TOP,
            color,
            wi::Color::Transparent());
        params.spacingX = tracking;
        params.bolden = bolden;
        wi::font::Draw(text, params, cmd);
    }

    void DrawBorderedRect(
        const float x,
        const float y,
        const float width,
        const float height,
        const wi::Color fill,
        const wi::Color border,
        const wi::graphics::CommandList cmd)
    {
        DrawRect(x, y, width, height, border, cmd);
        DrawRect(x + 1.0f, y + 1.0f, width - 2.0f, height - 2.0f, fill, cmd);
    }
}

namespace renegade::studio
{
    void RenegadeStudioChrome::Create()
    {
        SetName("Renegade-owned Studio chrome");
        SetLayout(width_, height_);
        SetShadowRadius(0.0f);
    }

    void RenegadeStudioChrome::SetLayout(
        const float width,
        const float height)
    {
        width_ = std::max(1.0f, width);
        height_ = std::max(1.0f, height);
        hierarchyWidth_ = width_ < 1350.0f ? 260.0f : 320.0f;
        SetPos(XMFLOAT2(0.0f, 0.0f));
        SetSize(XMFLOAT2(width_, height_));
    }

    void RenegadeStudioChrome::SetHierarchyRows(
        std::vector<HierarchyRow> rows)
    {
        hierarchyRows_ = std::move(rows);
    }

    void RenegadeStudioChrome::SetSceneName(std::string sceneName)
    {
        sceneName_ = std::move(sceneName);
    }

    void RenegadeStudioChrome::SetStatusText(std::string statusText)
    {
        statusText_ = std::move(statusText);
    }

    void RenegadeStudioChrome::SetSelectionName(std::string selectionName)
    {
        selectionName_ = std::move(selectionName);
    }

    void RenegadeStudioChrome::SetActiveTool(const int toolIndex) noexcept
    {
        activeTool_ = toolIndex;
    }

    XMFLOAT4 RenegadeStudioChrome::ViewportBounds() const noexcept
    {
        return XMFLOAT4(
            hierarchyWidth_,
            TopBarHeight + SceneTabsHeight,
            width_,
            height_ - BottomTabsHeight - StatusBarHeight);
    }

    void RenegadeStudioChrome::Update(
        const wi::Canvas& canvas,
        const float dt)
    {
        Widget::Update(canvas, dt);
        // This first slice is deliberately presentation-only. An empty hitbox
        // keeps viewport selection and navigation fully functional while the
        // visual direction is being accepted.
        hitBox = wi::primitive::Hitbox2D(
            XMFLOAT2(-1.0f, -1.0f),
            XMFLOAT2(0.0f, 0.0f));
    }

    void RenegadeStudioChrome::Render(
        const wi::Canvas&,
        const wi::graphics::CommandList cmd) const
    {
        if (!IsVisible())
        {
            return;
        }

        const float viewportTop = TopBarHeight + SceneTabsHeight;
        const float bottomTabsTop =
            height_ - BottomTabsHeight - StatusBarHeight;
        const float statusTop = height_ - StatusBarHeight;

        // App frame and low-noise industrial shell:
        DrawRect(0.0f, 0.0f, width_, TopBarHeight, Surface0, cmd);
        DrawRect(
            0.0f,
            TopBarHeight - 1.0f,
            width_,
            1.0f,
            Border,
            cmd);
        DrawRect(7.0f, 7.0f, width_ - 14.0f, 1.0f, BorderSoft, cmd);
        DrawRect(7.0f, height_ - 8.0f, width_ - 14.0f, 1.0f, BorderSoft, cmd);
        DrawRect(7.0f, 7.0f, 1.0f, height_ - 14.0f, BorderSoft, cmd);
        DrawRect(width_ - 8.0f, 7.0f, 1.0f, height_ - 14.0f, BorderSoft, cmd);

        // Renegade mark and wordmark. This is intentionally geometric instead
        // of a stock button or text pill.
        DrawRect(18.0f, 13.0f, 3.0f, 37.0f, Forge, cmd);
        DrawRect(25.0f, 13.0f, 20.0f, 4.0f, TextStrong, cmd);
        DrawRect(25.0f, 22.0f, 14.0f, 4.0f, TextSecondary, cmd);
        DrawRect(25.0f, 31.0f, 20.0f, 4.0f, TextStrong, cmd);
        DrawRect(39.0f, 35.0f, 6.0f, 15.0f, Forge, cmd);
        DrawText("RENEGADE", 58.0f, 17.0f, 14, TextStrong, cmd, 2.3f, 0.18f);
        DrawText("STUDIO", 58.0f, 38.0f, 9, Muted, cmd, 3.2f);
        DrawRect(204.0f, 0.0f, 1.0f, TopBarHeight, BorderSoft, cmd);

        // Menus are deliberately quiet. Ember is reserved for active state.
        constexpr std::array<const char*, 5> menus = {
            "FILE", "EDIT", "VIEW", "BUILD", "WINDOW"};
        float menuX = 222.0f;
        for (const char* menu : menus)
        {
            DrawText(menu, menuX, 25.0f, 11, TextSecondary, cmd, 0.55f);
            menuX += std::string(menu).size() * 8.0f + 25.0f;
        }
        DrawRect(menuX, 0.0f, 1.0f, TopBarHeight, BorderSoft, cmd);

        // Transform tools use squared controls and a single state line.
        constexpr std::array<const char*, 4> tools = {
            "SELECT", "TRANSLATE", "ROTATE", "SCALE"};
        float toolX = menuX + 13.0f;
        for (int index = 0; index < static_cast<int>(tools.size()); ++index)
        {
            const float controlWidth = index == 1 ? 102.0f : 82.0f;
            const bool active = activeTool_ == index;
            if (active)
            {
                DrawRect(
                    toolX,
                    15.0f,
                    controlWidth,
                    34.0f,
                    wi::Color(16, 23, 28, 220),
                    cmd);
                DrawRect(
                    toolX,
                    47.0f,
                    controlWidth,
                    2.0f,
                    Forge,
                    cmd);
            }
            DrawText(
                tools[index],
                toolX + 11.0f,
                26.0f,
                10,
                active ? TextStrong : TextSecondary,
                cmd,
                0.45f,
                active ? 0.12f : 0.0f);
            toolX += controlWidth + 5.0f;
        }

        const float sceneMetaWidth = 300.0f;
        const float sceneMetaX = width_ - sceneMetaWidth;
        DrawRect(
            sceneMetaX - 116.0f,
            0.0f,
            1.0f,
            TopBarHeight,
            BorderSoft,
            cmd);
        DrawText("▶", sceneMetaX - 93.0f, 20.0f, 17, Success, cmd, 0.0f, 0.1f);
        DrawText("Ⅱ", sceneMetaX - 59.0f, 22.0f, 13, Muted, cmd, 1.0f);
        DrawText("■", sceneMetaX - 27.0f, 22.0f, 12, Muted, cmd);
        DrawRect(
            sceneMetaX,
            0.0f,
            1.0f,
            TopBarHeight,
            BorderSoft,
            cmd);
        DrawText(
            sceneName_,
            sceneMetaX + 20.0f,
            17.0f,
            11,
            Text,
            cmd,
            1.25f,
            0.08f);
        DrawText("SCENE", sceneMetaX + 20.0f, 38.0f, 9, Success, cmd, 1.7f);

        // Left hierarchy panel:
        DrawRect(
            0.0f,
            TopBarHeight,
            hierarchyWidth_,
            height_ - TopBarHeight - StatusBarHeight,
            Surface0,
            cmd);
        DrawRect(
            hierarchyWidth_ - 1.0f,
            TopBarHeight,
            1.0f,
            height_ - TopBarHeight - StatusBarHeight,
            Border,
            cmd);
        DrawRect(
            0.0f,
            TopBarHeight,
            hierarchyWidth_,
            PanelHeaderHeight,
            Surface2,
            cmd);
        DrawRect(
            0.0f,
            TopBarHeight + PanelHeaderHeight - 1.0f,
            hierarchyWidth_,
            1.0f,
            BorderSoft,
            cmd);
        DrawText(
            "SCENE HIERARCHY",
            14.0f,
            TopBarHeight + 15.0f,
            11,
            TextSecondary,
            cmd,
            1.45f,
            0.1f);
        DrawText(
            "+   ...",
            hierarchyWidth_ - 62.0f,
            TopBarHeight + 15.0f,
            11,
            Muted,
            cmd,
            1.0f);

        const float searchY = TopBarHeight + PanelHeaderHeight + 10.0f;
        DrawBorderedRect(
            12.0f,
            searchY,
            hierarchyWidth_ - 24.0f,
            31.0f,
            wi::Color(8, 12, 15, 255),
            BorderSoft,
            cmd);
        DrawText("SEARCH SCENE...", 40.0f, searchY + 9.0f, 10, Muted, cmd, 0.7f);
        DrawText("⌕", 22.0f, searchY + 7.0f, 13, Muted, cmd);

        const float rowsTop = searchY + 42.0f;
        const float rowsBottom = statusTop - 8.0f;
        constexpr float rowHeight = 28.0f;
        for (std::size_t index = 0; index < hierarchyRows_.size(); ++index)
        {
            const float rowY = rowsTop + index * rowHeight;
            if (rowY + rowHeight > rowsBottom)
            {
                break;
            }
            const auto& row = hierarchyRows_[index];
            if (row.selected)
            {
                DrawRect(
                    8.0f,
                    rowY,
                    hierarchyWidth_ - 16.0f,
                    rowHeight,
                    wi::Color(67, 28, 13, 150),
                    cmd);
                DrawRect(8.0f, rowY, 2.0f, rowHeight, Forge, cmd);
            }
            const float indent = 14.0f + std::max(0, row.depth) * 16.0f;
            DrawText(
                row.depth == 0 ? "▼" : "",
                indent,
                rowY + 8.0f,
                9,
                Muted,
                cmd);
            DrawText(
                "◇",
                indent + 15.0f,
                rowY + 7.0f,
                9,
                row.selected ? Forge : wi::Color(198, 118, 41, 255),
                cmd);
            DrawText(
                row.name,
                indent + 36.0f,
                rowY + 7.0f,
                11,
                row.selected ? TextStrong : TextSecondary,
                cmd);
            DrawText(
                "◉",
                hierarchyWidth_ - 25.0f,
                rowY + 7.0f,
                10,
                Muted,
                cmd);
        }

        // Scene tab strip and viewport framing:
        DrawRect(
            hierarchyWidth_,
            TopBarHeight,
            width_ - hierarchyWidth_,
            SceneTabsHeight,
            wi::Color(8, 12, 15, 255),
            cmd);
        DrawRect(
            hierarchyWidth_,
            viewportTop - 1.0f,
            width_ - hierarchyWidth_,
            1.0f,
            Border,
            cmd);
        DrawRect(
            hierarchyWidth_,
            TopBarHeight,
            230.0f,
            SceneTabsHeight,
            Surface2,
            cmd);
        DrawRect(
            hierarchyWidth_,
            TopBarHeight,
            230.0f,
            2.0f,
            Forge,
            cmd);
        DrawText(
            sceneName_ + ".WISCENE",
            hierarchyWidth_ + 14.0f,
            TopBarHeight + 12.0f,
            10,
            Text,
            cmd,
            0.55f);
        DrawText(
            "×",
            hierarchyWidth_ + 211.0f,
            TopBarHeight + 10.0f,
            11,
            Muted,
            cmd);
        DrawRect(
            hierarchyWidth_,
            viewportTop,
            1.0f,
            bottomTabsTop - viewportTop,
            Border,
            cmd);
        DrawRect(
            hierarchyWidth_,
            viewportTop,
            width_ - hierarchyWidth_,
            1.0f,
            BorderSoft,
            cmd);

        // Viewport overlays are chrome, not scene content.
        float chipX = hierarchyWidth_ + 12.0f;
        for (const char* chip : {"PERSPECTIVE", "LIT", "SHOW"})
        {
            const float chipWidth = std::string(chip).size() * 7.5f + 24.0f;
            DrawBorderedRect(
                chipX,
                viewportTop + 10.0f,
                chipWidth,
                28.0f,
                wi::Color(9, 14, 17, 220),
                Border,
                cmd);
            DrawText(
                chip,
                chipX + 10.0f,
                viewportTop + 19.0f,
                9,
                TextSecondary,
                cmd,
                0.65f);
            chipX += chipWidth + 6.0f;
        }
        if (!selectionName_.empty())
        {
            const float tagY = bottomTabsTop - 39.0f;
            DrawRect(
                hierarchyWidth_ + 14.0f,
                tagY,
                2.0f,
                25.0f,
                Forge,
                cmd);
            DrawRect(
                hierarchyWidth_ + 16.0f,
                tagY,
                230.0f,
                25.0f,
                wi::Color(4, 7, 9, 180),
                cmd);
            DrawText(
                "SELECTED: " + selectionName_,
                hierarchyWidth_ + 25.0f,
                tagY + 8.0f,
                9,
                TextSecondary,
                cmd,
                1.0f);
        }

        // Hidden-until-needed bottom drawer: only its tabs are present.
        DrawRect(
            hierarchyWidth_,
            bottomTabsTop,
            width_ - hierarchyWidth_,
            BottomTabsHeight,
            wi::Color(8, 16, 21, 250),
            cmd);
        DrawRect(
            hierarchyWidth_,
            bottomTabsTop,
            width_ - hierarchyWidth_,
            1.0f,
            Border,
            cmd);
        float bottomX = hierarchyWidth_ + 15.0f;
        for (const char* tab : {
                 "ASSET BROWSER", "CONSOLE", "OUTPUT", "DIAGNOSTICS"})
        {
            DrawText(
                tab,
                bottomX,
                bottomTabsTop + 11.0f,
                9,
                TextSecondary,
                cmd,
                0.9f);
            bottomX += std::string(tab).size() * 7.2f + 31.0f;
            DrawRect(
                bottomX - 16.0f,
                bottomTabsTop,
                1.0f,
                BottomTabsHeight,
                BorderSoft,
                cmd);
        }

        // Status is secondary information, never a second toolbar.
        DrawRect(0.0f, statusTop, width_, StatusBarHeight, Surface0, cmd);
        DrawRect(0.0f, statusTop, width_, 1.0f, Border, cmd);
        DrawRect(16.0f, statusTop + 10.0f, 7.0f, 7.0f, Success, cmd);
        DrawText("READY", 31.0f, statusTop + 9.0f, 9, Muted, cmd, 1.15f);
        DrawText(
            statusText_,
            105.0f,
            statusTop + 9.0f,
            9,
            Muted,
            cmd,
            0.75f);
        DrawText(
            "RENEGADE STUDIO VISUAL PROOF",
            width_ - 270.0f,
            statusTop + 9.0f,
            9,
            TechCyan,
            cmd,
            0.9f);
    }
}
