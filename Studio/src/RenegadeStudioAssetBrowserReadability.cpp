#include "RenegadeStudioChrome.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float BottomTabsHeight = 32.0f;
    constexpr float StatusBarHeight = 28.0f;
    constexpr float AssetFolderRowHeight = 24.0f;
    constexpr float AssetCardWidth = 148.0f;
    constexpr float AssetCardHeight = 112.0f;
    constexpr float AssetCardGap = 10.0f;

    constexpr int AssetFolderTextSize = 11;
    constexpr int AssetCardNameTextSize = 11;
    constexpr int AssetCardMetadataTextSize = 10;
    constexpr int AssetCardFallbackTextSize = 10;

    constexpr wi::Color FolderSurface = wi::Color(6, 10, 12, 255);
    constexpr wi::Color FolderSelectedSurface = wi::Color(38, 22, 16, 255);
    constexpr wi::Color CardSurface = wi::Color(16, 23, 28, 255);
    constexpr wi::Color CardSelectedSurface = wi::Color(38, 22, 16, 255);
    constexpr wi::Color ThumbnailSurface = wi::Color(8, 16, 21, 255);
    constexpr wi::Color TextStrong = wi::Color(244, 244, 244, 255);
    constexpr wi::Color TextSecondary = wi::Color(226, 226, 226, 255);
    constexpr wi::Color Muted = wi::Color(142, 151, 156, 255);

    void DrawRect(
        const float x,
        const float y,
        const float width,
        const float height,
        const wi::Color color,
        const wi::graphics::CommandList cmd)
    {
        if (width <= 0.0f || height <= 0.0f)
            return;
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
        const float bolden = 0.16f)
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

    std::string EllipsizeForWidth(
        std::string value,
        const float width,
        const int size)
    {
        const float approximateGlyphWidth =
            std::max(5.0f, static_cast<float>(size) * 0.64f);
        const std::size_t maximum = static_cast<std::size_t>(std::max(
            3.0f,
            std::floor(std::max(0.0f, width) / approximateGlyphWidth)));
        if (value.size() <= maximum)
            return value;
        if (maximum <= 3)
            return value.substr(0, maximum);
        value.resize(maximum - 3);
        value += "...";
        return value;
    }
}

namespace renegade::studio
{
    void RenderCreatorAssetBrowserReadabilityOverlay(
        const wi::graphics::CommandList cmd)
    {
        auto* chrome = CreatorAssetStudioChrome::Current();
        if (chrome == nullptr || chrome->ActiveBottomTab() != 0)
            return;

        const float hierarchyX = chrome->HierarchyWidth();
        const float inspectorX =
            chrome->LayoutWidth() - chrome->InspectorWidth();
        const float bottomTabsTop =
            chrome->LayoutHeight() - BottomTabsHeight - StatusBarHeight;
        const float drawerTop = bottomTabsTop - chrome->DrawerHeight();
        const float bodyTop = drawerTop + 79.0f;
        const float bodyBottom = bottomTabsTop - 6.0f;
        const float browserWidth = inspectorX - hierarchyX;
        const float folderPaneWidth = chrome->AssetBrowserFoldersVisible()
            ? std::clamp(browserWidth * 0.28f, 190.0f, 270.0f)
            : 28.0f;

        // The base browser owns all hit testing and scroll state. Gate 4 only
        // repaints the text-bearing area of each visible folder row so the old
        // 9 px label cannot ghost behind the new 11 px presentation.
        if (chrome->AssetBrowserFoldersVisible())
        {
            const auto& folders = chrome->AssetBrowserFolders();
            const auto& visibleFolders = chrome->VisibleAssetFolderRows();
            const std::size_t capacity = static_cast<std::size_t>(std::max(
                1.0f,
                std::floor((bodyBottom - bodyTop) / AssetFolderRowHeight)));
            const std::size_t first = chrome->AssetBrowserFolderScrollRow();
            const std::size_t end = std::min(
                visibleFolders.size(),
                first + capacity);

            for (std::size_t visible = first; visible < end; ++visible)
            {
                const std::size_t folderIndex = visibleFolders[visible];
                if (folderIndex >= folders.size())
                    continue;
                const auto& folder = folders[folderIndex];
                const float y = bodyTop +
                    static_cast<float>(visible - first) * AssetFolderRowHeight;
                const float indent = hierarchyX + 14.0f +
                    std::max(0, folder.depth) * 14.0f;
                const float textX = indent + 17.0f;
                const float textRight = hierarchyX + folderPaneWidth - 8.0f;
                const wi::Color fill = folder.selected
                    ? FolderSelectedSurface
                    : FolderSurface;

                DrawRect(
                    textX - 2.0f,
                    y + 1.0f,
                    std::max(0.0f, textRight - textX + 2.0f),
                    AssetFolderRowHeight - 2.0f,
                    fill,
                    cmd);
                DrawText(
                    EllipsizeForWidth(
                        folder.name,
                        std::max(0.0f, textRight - textX),
                        AssetFolderTextSize),
                    textX,
                    y + 5.0f,
                    AssetFolderTextSize,
                    folder.selected ? TextStrong : TextSecondary,
                    cmd,
                    0.1f);
            }
        }

        // Cards keep the accepted 148x112 footprint so the Gate 2 minimum
        // drawer still exposes a complete first row. Only the text band and
        // missing-thumbnail fallback are repainted at a readable floor.
        const float gridX = hierarchyX + folderPaneWidth + 12.0f;
        const float gridWidth = inspectorX - gridX - 12.0f;
        const int columns = std::max(
            1,
            static_cast<int>((gridWidth + AssetCardGap) /
                (AssetCardWidth + AssetCardGap)));
        const auto& assets = chrome->AssetBrowserAssets();
        const std::size_t firstAsset = chrome->AssetBrowserAssetScrollRow() *
            static_cast<std::size_t>(columns);

        for (std::size_t index = firstAsset; index < assets.size(); ++index)
        {
            const std::size_t local = index - firstAsset;
            const int row = static_cast<int>(
                local / static_cast<std::size_t>(columns));
            const int column = static_cast<int>(
                local % static_cast<std::size_t>(columns));
            const float x = gridX +
                column * (AssetCardWidth + AssetCardGap);
            const float y = bodyTop +
                row * (AssetCardHeight + AssetCardGap);
            if (y + AssetCardHeight > bodyBottom)
                break;

            const auto& asset = assets[index];
            const bool selected =
                asset.relativePath == chrome->AssetBrowserSelectedPath();
            const wi::Color cardFill = selected
                ? CardSelectedSurface
                : CardSurface;

            DrawRect(
                x + 2.0f,
                y + 77.0f,
                AssetCardWidth - 4.0f,
                AssetCardHeight - 79.0f,
                cardFill,
                cmd);

            if (!asset.thumbnail.IsValid())
            {
                DrawRect(
                    x + 9.0f,
                    y + 11.0f,
                    AssetCardWidth - 18.0f,
                    20.0f,
                    ThumbnailSurface,
                    cmd);
                DrawText(
                    asset.directory ? "FOLDER" : "NO PREVIEW",
                    x + 12.0f,
                    y + 14.0f,
                    AssetCardFallbackTextSize,
                    asset.directory ? TextStrong : Muted,
                    cmd,
                    0.35f);
            }

            constexpr float textXOffset = 8.0f;
            constexpr float textRightInset = 8.0f;
            const float textWidth =
                AssetCardWidth - textXOffset - textRightInset;
            DrawText(
                EllipsizeForWidth(
                    asset.name,
                    textWidth,
                    AssetCardNameTextSize),
                x + textXOffset,
                y + 80.0f,
                AssetCardNameTextSize,
                selected ? TextStrong : TextSecondary,
                cmd,
                0.1f);
            DrawText(
                EllipsizeForWidth(
                    asset.typeLabel,
                    textWidth,
                    AssetCardMetadataTextSize),
                x + textXOffset,
                y + 97.0f,
                AssetCardMetadataTextSize,
                Muted,
                cmd,
                0.15f,
                0.14f);
        }
    }
}
