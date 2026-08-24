#pragma once

#include <WickedEngine.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace renegade::studio
{
    // Gate 9E keeps Story Flow's presentation in one editable seam. Runtime/
    // Story Flow semantics never depend on this data. The packaged defaults are
    // loaded from Content/ui/story-flow-theme.cfg and may be edited without
    // recompiling Studio; invalid/missing values fail soft to these defaults.
    struct StoryFlowVisualTheme
    {
        float navigationRailWidth = 86.0f;
        float headerHeight = 78.0f;
        float inspectorWidth = 344.0f;
        float shellPadding = 14.0f;
        float headerBrandWidth = 188.0f;
        float headerBrandHeight = 54.0f;
        float inspectorPreviewHeight = 150.0f;
        float minimapWidth = 246.0f;
        float minimapHeight = 132.0f;
        float canvasControlHeight = 36.0f;

        float journeyCardWidth = 154.0f;
        float journeyCardHeight = 172.0f;
        float journeyColumnSpacing = 208.0f;
        float journeyTrackSpacing = 210.0f;
        float journeyTrackTop = 72.0f;
        float journeyCompactHeight = 58.0f;
        float cardRadius = 5.0f;
        float routeThickness = 3.0f;
        float routeGlowThickness = 7.0f;
        float routeLabelOffset = 10.0f;

        int fontStyle = 0;
        int fontHeaderTitle = 15;
        int fontHeaderMeta = 8;
        int fontCardTitle = 10;
        int fontCardSubtitle = 7;
        int fontCardMeta = 7;
        int fontInspectorTitle = 12;
        int fontInspectorBody = 8;
        int fontRouteLabel = 8;
        float fontTracking = 0.22f;
        float fontBolden = 0.10f;

        std::string logoPath = "Content/ui/renegade-story-flow-logo.png";
        std::string backgroundPath;
        std::string fontPath;

        wi::Color canvas = wi::Color(5, 10, 14, 255);
        wi::Color canvasOverlay = wi::Color(4, 9, 13, 116);
        wi::Color rail = wi::Color(6, 10, 14, 252);
        wi::Color header = wi::Color(7, 12, 16, 250);
        wi::Color inspector = wi::Color(8, 13, 17, 252);
        wi::Color panel = wi::Color(12, 19, 24, 246);
        wi::Color panelRaised = wi::Color(16, 24, 30, 250);
        wi::Color panelHover = wi::Color(23, 34, 41, 252);
        wi::Color border = wi::Color(47, 64, 74, 230);
        wi::Color borderSoft = wi::Color(27, 40, 48, 220);

        wi::Color text = wi::Color(220, 226, 229, 255);
        wi::Color textStrong = wi::Color(247, 248, 248, 255);
        wi::Color muted = wi::Color(137, 151, 159, 255);
        wi::Color accent = wi::Color(233, 103, 29, 255);
        wi::Color selection = wi::Color(87, 169, 231, 255);
        wi::Color selectionSurface = wi::Color(18, 45, 62, 184);
        wi::Color success = wi::Color(115, 206, 94, 255);
        wi::Color warning = wi::Color(229, 173, 75, 255);
        wi::Color error = wi::Color(224, 79, 65, 255);

        wi::Color routeMain = wi::Color(126, 210, 76, 255);
        wi::Color routeFailure = wi::Color(231, 78, 55, 255);
        wi::Color routeLoad = wi::Color(73, 155, 235, 255);
        wi::Color routeSystem = wi::Color(157, 90, 211, 255);
        wi::Color routeEnding = wi::Color(226, 174, 65, 255);
        wi::Color routeOther = wi::Color(187, 193, 196, 255);

        wi::Color gameStart = wi::Color(77, 163, 217, 255);
        wi::Color level = wi::Color(124, 192, 83, 255);
        wi::Color screen = wi::Color(91, 151, 188, 255);
        wi::Color terminal = wi::Color(224, 79, 65, 255);

        [[nodiscard]] static StoryFlowVisualTheme& Get()
        {
            static StoryFlowVisualTheme theme;
            static bool initialized = false;
            if (!initialized)
            {
                initialized = true;
                std::string ignored;
                theme.LoadFromFile("Content/ui/story-flow-theme.cfg", ignored);
            }
            return theme;
        }

        bool LoadFromFile(const std::string& path, std::string& warning)
        {
            std::ifstream input(path);
            if (!input)
            {
                warning = "Story Flow theme file not found; built-in defaults are active.";
                return false;
            }

            std::unordered_map<std::string, std::string> values;
            std::string line;
            while (std::getline(input, line))
            {
                const auto hash = line.find('#');
                if (hash != std::string::npos)
                    line.resize(hash);
                const auto equals = line.find('=');
                if (equals == std::string::npos)
                    continue;
                std::string key = Trim(line.substr(0, equals));
                std::string value = Trim(line.substr(equals + 1));
                if (!key.empty())
                    values[std::move(key)] = std::move(value);
            }

            const auto number = [&values](const char* key, float& target)
            {
                const auto it = values.find(key);
                if (it == values.end()) return;
                try
                {
                    std::size_t used = 0;
                    const float parsed = std::stof(it->second, &used);
                    if (used == it->second.size() && std::isfinite(parsed))
                        target = parsed;
                }
                catch (...) {}
            };
            const auto integer = [&values](const char* key, int& target)
            {
                const auto it = values.find(key);
                if (it == values.end()) return;
                try
                {
                    std::size_t used = 0;
                    const int parsed = std::stoi(it->second, &used);
                    if (used == it->second.size())
                        target = parsed;
                }
                catch (...) {}
            };
            const auto textValue = [&values](const char* key, std::string& target)
            {
                const auto it = values.find(key);
                if (it != values.end()) target = it->second;
            };
            const auto color = [&values](const char* key, wi::Color& target)
            {
                const auto it = values.find(key);
                if (it == values.end()) return;
                std::stringstream stream(it->second);
                std::string token;
                std::vector<int> channels;
                while (std::getline(stream, token, ','))
                {
                    try
                    {
                        channels.push_back(std::clamp(std::stoi(Trim(token)), 0, 255));
                    }
                    catch (...) { return; }
                }
                if (channels.size() == 3 || channels.size() == 4)
                {
                    target = wi::Color(
                        static_cast<std::uint8_t>(channels[0]),
                        static_cast<std::uint8_t>(channels[1]),
                        static_cast<std::uint8_t>(channels[2]),
                        static_cast<std::uint8_t>(channels.size() == 4 ? channels[3] : 255));
                }
            };

            number("shell.navigation_rail_width", navigationRailWidth);
            number("shell.header_height", headerHeight);
            number("shell.inspector_width", inspectorWidth);
            number("shell.padding", shellPadding);
            number("shell.header_brand_width", headerBrandWidth);
            number("shell.header_brand_height", headerBrandHeight);
            number("shell.inspector_preview_height", inspectorPreviewHeight);
            number("shell.minimap_width", minimapWidth);
            number("shell.minimap_height", minimapHeight);
            number("shell.canvas_control_height", canvasControlHeight);

            number("journey.card_width", journeyCardWidth);
            number("journey.card_height", journeyCardHeight);
            number("journey.column_spacing", journeyColumnSpacing);
            number("journey.track_spacing", journeyTrackSpacing);
            number("journey.track_top", journeyTrackTop);
            number("journey.compact_height", journeyCompactHeight);
            number("journey.card_radius", cardRadius);
            number("journey.route_thickness", routeThickness);
            number("journey.route_glow_thickness", routeGlowThickness);
            number("journey.route_label_offset", routeLabelOffset);

            integer("font.header_title", fontHeaderTitle);
            integer("font.header_meta", fontHeaderMeta);
            integer("font.card_title", fontCardTitle);
            integer("font.card_subtitle", fontCardSubtitle);
            integer("font.card_meta", fontCardMeta);
            integer("font.inspector_title", fontInspectorTitle);
            integer("font.inspector_body", fontInspectorBody);
            integer("font.route_label", fontRouteLabel);
            number("font.tracking", fontTracking);
            number("font.bolden", fontBolden);
            textValue("font.path", fontPath);

            textValue("asset.logo", logoPath);
            textValue("asset.background", backgroundPath);

            color("color.canvas", canvas);
            color("color.canvas_overlay", canvasOverlay);
            color("color.rail", rail);
            color("color.header", header);
            color("color.inspector", inspector);
            color("color.panel", panel);
            color("color.panel_raised", panelRaised);
            color("color.panel_hover", panelHover);
            color("color.border", border);
            color("color.border_soft", borderSoft);
            color("color.text", text);
            color("color.text_strong", textStrong);
            color("color.muted", muted);
            color("color.accent", accent);
            color("color.selection", selection);
            color("color.selection_surface", selectionSurface);
            color("color.success", success);
            color("color.warning", this->warning);
            color("color.error", error);
            color("route.main", routeMain);
            color("route.failure", routeFailure);
            color("route.load", routeLoad);
            color("route.system", routeSystem);
            color("route.ending", routeEnding);
            color("route.other", routeOther);
            color("destination.game_start", gameStart);
            color("destination.level", level);
            color("destination.screen", screen);
            color("destination.terminal", terminal);

            navigationRailWidth = std::clamp(navigationRailWidth, 54.0f, 180.0f);
            headerHeight = std::clamp(headerHeight, 54.0f, 150.0f);
            inspectorWidth = std::clamp(inspectorWidth, 240.0f, 560.0f);
            journeyCardWidth = std::clamp(journeyCardWidth, 96.0f, 360.0f);
            journeyCardHeight = std::clamp(journeyCardHeight, 96.0f, 360.0f);
            journeyColumnSpacing = std::max(journeyCardWidth + 20.0f, journeyColumnSpacing);
            journeyTrackSpacing = std::max(journeyCardHeight + 18.0f, journeyTrackSpacing);
            routeThickness = std::clamp(routeThickness, 1.0f, 12.0f);
            routeGlowThickness = std::max(routeThickness, routeGlowThickness);

            fontStyle = 0;
            if (!fontPath.empty())
            {
                const int loadedStyle = wi::font::AddFontStyle(fontPath);
                if (loadedStyle >= 0)
                    fontStyle = loadedStyle;
                else
                    warning = "Story Flow custom font could not be loaded; default Wicked font is active.";
            }
            if (warning.empty())
                warning = "Story Flow visual theme loaded.";
            return true;
        }

    private:
        [[nodiscard]] static std::string Trim(std::string value)
        {
            const auto whitespace = [](const unsigned char c)
            {
                return std::isspace(c) != 0;
            };
            while (!value.empty() && whitespace(value.front()))
                value.erase(value.begin());
            while (!value.empty() && whitespace(value.back()))
                value.pop_back();
            return value;
        }
    };
}
