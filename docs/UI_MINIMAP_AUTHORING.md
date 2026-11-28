# Minimap and illustrated world map

UI Designer > Elements > BASIC > Minimap adds an ordinary, resizable Image navigation component. Gameplay > Navigation offers the same component. Add it to the project's or Level's gameplay HUD. Select it and open Navigation settings (or STYLE / FONT). Live overhead view is the default; the editor shows a labelled sample rather than running a second gameplay scene.

The shared Renegade modal edits live/artwork mode, round/rectangle, fixed north/player rotation, range, camera height, icon sizes, nearby categories, names, distances, image bounds, fonts and colours. Save commits one Screen history operation. Cancel discards the settings draft; imported resources remain reusable project assets. Screen autosave waits 1.5 seconds idle; Save Now writes immediately. Normal geometry controls move and resize the component. Additional ordinary Image layers can supply decorative frames.

## Marker setup

| Marker | Source | Icon replacement |
| --- | --- | --- |
| Player | Possessed Player Start position and heading | Player icon |
| Enemy | Existing live hostile Character faction and native position | Enemy icon |
| Quest giver | Location Zone type `questgiver`, or a registered target following an entity | Quest-giver icon |
| Vendor | Location Zone type `vendor`, or a registered target following an entity | Vendor icon |
| Location / objective / custom type | Existing Location Zone or registered navigation target | Individual Zone/target icon, category icon, then Default location icon |

There is one shared navigation target owner. Locations do not need setting up separately for the compass and minimap. Show on navigation and Only after discovery retain the existing Location Zone policy. Quest giver and vendor tags identify map roles; they do not create quest or shop gameplay. For moving NPCs, register an entity-following target through the governed navigation API. Nearby enemy markers disappear when the native character dies or is removed.

All icons are replaceable with imported artwork, including alpha PNGs. Individual Zone/target artwork wins over a category override. Use generic icons clears this minimap's category/player overrides and retains individual Zone assignments. Category names are exact strings; the built-in nearby filters use `enemy`, `questgiver` and `vendor`. Arbitrary types can be assigned category artwork in C++/Lua. The stock fallbacks are a player chevron, enemy diamond, quest-giver exclamation mark, vendor dollar symbol and location cross.

Range is the distance in metres from the centre to the top edge. Nearby NPC categories outside range are hidden. Distant locations/objectives may pin to the edge when Edge markers is enabled. Circular clipping reserves room for icon sizes; rectangular surfaces preserve world scale on both axes.

## Artwork and full map

Choose Minimap artwork to use an image instead of the live camera. Full map artwork is a separate optional image. Set West/min X, East/max X, South/min Z and North/max Z to the world coordinates represented by the image edges. +Z is north. Both images currently share these bounds, so a detailed full map should cover the same world area as the minimap artwork. The map does not infer building positions from painted pixels.

M opens the illustrated full map when enabled (the governed Map action can be rebound in gameplay input). Drag to pan, wheel to zoom, and use Close, M or Esc to return. Opening pauses gameplay, hides the ordinary HUD and gives the map pointer/keyboard ownership. Closing restores the prior manual pause request and normal HUD visibility. The initial artwork view fits the configured world bounds; the player marker remains in its actual world position while panning. A missing full-map image falls back to minimap artwork in artwork mode, or a neutral grid in live mode.

The full map uses a separate 2048-pixel presentation texture; the normal minimap defaults to a 256-pixel live camera at 20 updates/second. Live quality choices are 128/256/512 and refresh choices 10/20/30/60. Native render-to-texture cameras share Wicked's existing scene/render pass: there is no cloned scene or second simulation. Hidden/retired maps release their transient cameras. At most four live minimap cameras run simultaneously; each view draws at most 128 markers. Automatic enemies use the nearest 128 known hostile Character records and indexed component lookups. This is an alpha performance bound, not a large-world benchmark.

## C++ and creator Lua

C++ uses `MakeWorldMinimapWidget`, `ScreenMinimap`, `BuildMinimapFrame`, `WorldNavigationService` and normal `ScreenAuthoringSession::ConfigureNavigationComponent` history. Image/player/category/full-map resources are persisted and included in portable Screen dependency remapping.

Creator scripts use the same service and validation. Paths must identify existing resources inside project Content. These Runtime changes are session state; persistent authoring uses the Screen document.

```lua
local map = renegade.minimap
local id = assert(map.create {
    x = 956, y = 24, width = 280, height = 280,
    live = true, range = 80, rotate = false, round = true,
    map_key = true,
    show_enemies = true, show_quest_givers = true, show_vendors = true,
    player_icon = "Content/UI/Icons/Player.png",
    icons = {
        enemy = "Content/UI/Icons/Enemy.png",
        questgiver = "Content/UI/Icons/Quest.png",
        vendor = "Content/UI/Icons/Shop.png",
        default = "Content/UI/Icons/Location.png"
    },
    world_map_image = "Content/UI/Maps/World.png",
    west = -500, east = 500, south = -500, north = 500
})
assert(map.configure(id, {range = 120}))
local settings = assert(map.get(id))

-- myVendor is an existing governed entity reference:
assert(map.put_target {
    id = "vendor:smith", label = "Blacksmith", category = "vendor",
    entity = myVendor, icon = "Content/UI/Icons/Anvil.png",
    visible = true, minimap_only = true
})
assert(renegade.map.open())
assert(renegade.map.is_open())
assert(renegade.map.close())
```

Additional fields include `camera_height`, `refresh_hz`, `resolution`, `player_size`, `marker_size`, `clamp_edges`, `show_player`, `show_names`, `show_distance`, `map_image`, `font`, `font_size`, `text_colour`, `background_colour`, `border_colour`, `border_width`, `opacity`, and the existing navigation category filter and targets. `views`, `remove`, `put_target` and `remove_target` share the existing navigation owner. `renegade.map` exposes `open`, `close` and `is_open`. A disabled Map setting also rejects scripted open.

## Verification and limits

Local Windows x64 Release builds and focused tests cover compass compatibility, projection, nearby filters, moving entity resolution, invalid inputs, Screen history/persistence, custom resource portability, native camera lifecycle, input migration and governed Lua controls. Native DX12 proof covers live overhead alignment, M artwork, pan/zoom, blocked movement while open, Close/M/Esc return and resumed movement, custom artwork/icons and editor palette/history. See docs/evidence/minimap-2026-11-28.

No fog of war, roof cutaway, automatic building/quest/vendor inference, persisted exploration, terrain map baking or cartographic artwork generation is included. The generated coordinate artwork in review screenshots is a diagnostic fixture, not launch artwork. Debug, exported-player packaging, large scenes, Vulkan and independent release acceptance remain separate checks.
