#pragma once
// UI-028 catalogue metadata only. Unimplemented entries are disabled;
// data binding and Runtime support must be proven before enabling them.
#include <cstdint>
#include <string_view>
#include <string>
#include <vector>

namespace renegade::studio
{
    enum class ScreenToolboxTab : std::uint8_t
    {
        Screens, Gameplay, Elements, Globals
    };

    enum class ScreenToolboxAction : std::uint8_t
    {
        None, StoryFlow, Text, Button, Image, Background,
        Panel, Heading, Duplicate, Grid, Snap, Progress, GlobalHud, ProjectInventory,
        Icon, Divider, Spacer, Counter, PlayerHealthBar, PlayerHealthText, PlayerAmmoText, Group, HorizontalStack, VerticalStack, LayoutGrid, Toggle, Slider, Selector, TextInput, GameplayAttributes, PublishUiPackage, AdoptUiPackage, UiBudgets
    };

    struct ScreenEditorLibraryScreen
    {
        std::string nodeId;
        std::string name;
        bool current = false;
        std::string documentId;
        std::string pathHint;
        std::string category = "PROJECT SCREENS";
        bool available = true;
        bool enabledInGame = true;
        std::string diagnostic;
    };

    inline std::vector<std::pair<std::string,std::string>> ScreenDestinationChoices(const std::vector<ScreenEditorLibraryScreen>& entries)
    {
        std::vector<std::pair<std::string,std::string>> result;
        for(const auto& entry:entries)if(!entry.nodeId.empty()&&entry.available&&entry.enabledInGame&&!entry.current)
            result.emplace_back(entry.name,entry.nodeId);
        return result;
    }
    struct ScreenToolboxEntry
    {
        std::string_view category;
        std::string_view label;
        ScreenToolboxAction action = ScreenToolboxAction::None;
    };

    inline const std::vector<ScreenToolboxEntry>& ScreenToolboxEntries(
        const ScreenToolboxTab tab)
    {
        static const std::vector<ScreenToolboxEntry> screens = {
            {"PROJECT", "Open Screen Library in StoryFlow", ScreenToolboxAction::StoryFlow},
            {"CORE SCREENS", "Splash / Title"}, {"CORE SCREENS", "Main Menu"},
            {"CORE SCREENS", "New Game"}, {"CORE SCREENS", "Load / Save"},
            {"CORE SCREENS", "Pause"}, {"CORE SCREENS", "Credits"},
            {"SETTINGS", "Graphics"}, {"SETTINGS", "Audio"},
            {"SETTINGS", "Controls"}, {"SETTINGS", "Gameplay"},
            {"HUDS", "Player HUD"}, {"HUDS", "Vehicle HUD"},
            {"GAMEPLAY", "Inventory"}, {"GAMEPLAY", "Trading"},
            {"GAMEPLAY", "Crafting"}, {"GAMEPLAY", "Skills"},
            {"GAMEPLAY", "Character"}, {"GAMEPLAY", "Map"},
            {"GAMEPLAY", "Journal"}, {"GAMEPLAY", "Dialogue"},
            {"OUTCOMES", "Death"}, {"OUTCOMES", "Victory"},
            {"OUTCOMES", "Game Over"}
        };
        static const std::vector<ScreenToolboxEntry> gameplay = {
            {"BINDING SOURCE", "Choose an element, then Bind in Inspector"},
            {"PLAYER", "Attribute Profiles", ScreenToolboxAction::GameplayAttributes}, {"PLAYER", "Movement"},
            {"CHARACTERS", "Character stats"}, {"CHARACTERS", "NPC state"},
            {"COMBAT", "Health bar - current Player", ScreenToolboxAction::PlayerHealthBar},
            {"COMBAT", "Health text - current Player", ScreenToolboxAction::PlayerHealthText},
            {"COMBAT", "Loaded ammo - current Player", ScreenToolboxAction::PlayerAmmoText},
            {"EQUIPMENT", "Equipped item"}, {"EQUIPMENT", "Ability slots"},
            {"INVENTORY", "Inventory slots"}, {"PROGRESSION", "Experience"},
            {"OBJECTIVES", "Quest state"}, {"WORLD", "World state"},
            {"CUSTOM", "Typed script value"}
        };
        static const std::vector<ScreenToolboxEntry> elements = {
            {"BASIC", "Panel", ScreenToolboxAction::Panel},
            {"BASIC", "Button", ScreenToolboxAction::Button},
            {"BASIC", "Text", ScreenToolboxAction::Text},
            {"BASIC", "Heading", ScreenToolboxAction::Heading},
            {"BASIC", "Image", ScreenToolboxAction::Image},
            {"BASIC", "Progress Bar", ScreenToolboxAction::Progress},
            {"BASIC", "Background", ScreenToolboxAction::Background},
            {"BASIC", "Icon", ScreenToolboxAction::Icon}, {"BASIC", "Divider", ScreenToolboxAction::Divider}, {"BASIC", "Spacer", ScreenToolboxAction::Spacer},
            {"LAYOUT", "Group", ScreenToolboxAction::Group}, {"LAYOUT", "Horizontal Stack", ScreenToolboxAction::HorizontalStack},
            {"LAYOUT", "Vertical Stack", ScreenToolboxAction::VerticalStack},
            {"LAYOUT", "Grid", ScreenToolboxAction::LayoutGrid}, {"LAYOUT", "Tabs"},
            {"LAYOUT", "Scroll Area"}, {"LAYOUT", "Window"},
            {"MEDIA", "Video"}, {"MEDIA", "Animated Image"},
            {"MEDIA", "Animation Player"}, {"MEDIA", "Sequence"},
            {"DATA", "Radial"},
            {"DATA", "Segmented"}, {"DATA", "Counter text - bind in Inspector", ScreenToolboxAction::Counter},
            {"DATA", "Gauge"}, {"DATA", "Graph"},
            {"INPUT", "Checkbox"}, {"INPUT", "Toggle", ScreenToolboxAction::Toggle},
            {"INPUT", "Slider", ScreenToolboxAction::Slider}, {"INPUT", "Scrollbar"},
            {"INPUT", "Selector", ScreenToolboxAction::Selector}, {"INPUT", "Dropdown"}, {"INPUT", "Text Input", ScreenToolboxAction::TextInput},
            {"INPUT", "Radio"},
            {"NAVIGATION", "Menu List"}, {"NAVIGATION", "Tab Bar"},
            {"NAVIGATION", "Breadcrumbs"}, {"NAVIGATION", "Pagination"},
            {"GAMEPLAY", "Health bar - current Player", ScreenToolboxAction::PlayerHealthBar},
            {"GAMEPLAY", "Health text - current Player", ScreenToolboxAction::PlayerHealthText},
            {"GAMEPLAY", "Loaded ammo - current Player", ScreenToolboxAction::PlayerAmmoText},
            {"GAMEPLAY", "Inventory"}, {"GAMEPLAY", "Ability"},
            {"GAMEPLAY", "Quest"}, {"GAMEPLAY", "Minimap"},
            {"FEEDBACK", "Tooltip"}, {"FEEDBACK", "Notification"},
            {"FEEDBACK", "Popup"}, {"FEEDBACK", "Modal"},
            {"FEEDBACK", "Loading"},
            {"REUSABLE", "Linked Component"}, {"REUSABLE", "Saved Component"}
        };
        static const std::vector<ScreenToolboxEntry> globals = {
            {"CANVAS", "Toggle Grid", ScreenToolboxAction::Grid},
            {"CANVAS", "Toggle Snapping", ScreenToolboxAction::Snap},
            {"CANVAS", "Spacing in top toolbar"},
            {"PROJECT", "Global HUD defaults", ScreenToolboxAction::GlobalHud},
            {"PROJECT", "Inventory Screen", ScreenToolboxAction::ProjectInventory},
            {"PROJECT", "Gameplay attribute profiles", ScreenToolboxAction::GameplayAttributes}, {"PROJECT", "Input mapping"},
            {"THEME", "Colors"}, {"THEME", "Typography"},
            {"THEME", "Appearance states"},
            {"ASSETS", "Image and font resources"},
            {"SHARED", "Components"}, {"SHARED", "Animations"},
            {"ACCESSIBILITY", "Focus order"}, {"ACCESSIBILITY", "Text scale"},
            {"PERFORMANCE", "UI warning budgets", ScreenToolboxAction::UiBudgets},
            {"LIBRARY", "Publish UI package", ScreenToolboxAction::PublishUiPackage},
            {"LIBRARY", "Adopt UI package", ScreenToolboxAction::AdoptUiPackage}
        };
        switch (tab)
        {
        case ScreenToolboxTab::Screens: return screens;
        case ScreenToolboxTab::Gameplay: return gameplay;
        case ScreenToolboxTab::Globals: return globals;
        case ScreenToolboxTab::Elements:
        default: return elements;
        }
    }
}
