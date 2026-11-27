# World navigation bar

The World navigation element is a generic RPG compass/point-of-interest bar. One component supplies heading, bearing projection, filtering and marker placement. Artwork is independent of that behaviour. The neutral appearance works without supplied images; the supplied fantasy artwork is an example appearance. Other launch UI sets can supply the same appearance slots without adding separate navigation logic.

## Add and customise a bar

Open the Player HUD, choose Elements and search for World navigation. Insertion creates one editable Image layer with a navigation component near the top centre. Use the ordinary Inspector to rename, move or resize it, including placement within authored groups.

Navigation settings provides:

- Follow camera, Follow player or Manual heading; headings are clockwise degrees with +Z north and +X east.
- Visible compass span 30-360 degrees; marker size 4-128 design pixels; spacing 0-256; maximum distance 0 for unlimited.
- Optional target names, planar distances in metres, direction ticks and comma-separated category filters. Blank categories means all.
- Backing, optional full 360-degree direction strip, fixed pointer and an icon for each named category. Custom categories are supported.
- TrueType font import, font size, text colour and backing colour/tint.
- Authored targets with name, category, world X/Y/Z, priority and visibility. Higher priority and then nearer distance decide which overlapping marker remains.
- An optional persistent entity ID for advanced authored following. Missing/deleted/unassigned entities remain hidden; they never silently revert to a stale world coordinate.

The heading slider and embedded preview use explicitly labelled editor sample data. They do not rotate the real camera or change gameplay targets. When authored targets exist, the preview uses those instead of adding demonstration targets. Entity-following needs a running Level to resolve its entity. Camera/player headings are live only during gameplay.

Save applies settings and appearance together as one Undo operation. Cancel restores the authored presentation. Importing art/fonts/appearance packages may leave project-owned imported assets even when settings are cancelled.

## Appearance presets and portable prefabs

Neutral selects the generic appearance. Fantasy example adopts the supplied artwork. Import appearance accepts a portable Renegade UI package manifest containing a navigation component. Raw artwork-pack registries are reference inputs, not that native package format.

Appearance selection changes art/font/style, retaining placement, heading source/span, target identities, filters and bindings. Individual images and category icons can be replaced independently. Clear artwork returns to procedural ticks/markers while retaining font/colour settings. Labels baked into a direction-strip PNG change by replacing that PNG; generated directions and marker labels use the editable font.

Select the layer or its composition root and use Save selected as UI prefab to publish a reusable package. Receiving projects own their art/fonts. Cross-project entity-following references are explicitly unassigned until the receiving project chooses appropriate entities. Preview samples and transient script-created bars/targets are not saved into prefabs.

The four full launch sets are an owner requirement. This checkpoint prepares a neutral package and the available fantasy example; it does not claim all four sets have been supplied, audited or packaged.

## Lua

The governed namespace is renegade.navigation, contract_version 1. An active HUD is needed to display script-created bars. Calls return the created ID/settings/list on success, or nil/false plus an error message on rejection.

    local nav = renegade.navigation
    local bar = assert(nav.create{
        name="Custom compass", x=320, y=24, width=640, height=104,
        source="camera", span=120, marker_gap=80,
        text_colour="ffffffff", backing_colour="121820dd"
    })
    assert(nav.put_target{
        id="quest.river", label="Find the bridge", category="quest",
        position={x=25,y=0,z=70}, priority=100
    })
    -- EntityRef instead of position follows a moving entity safely:
    -- assert(nav.put_target{id="friend",category="allied",entity=self.entity})
    assert(nav.configure(bar,{span=150,categories="quest,allied"}))
    local settings = assert(nav.get(bar))
    settings.marker_size=30
    assert(nav.configure(bar,settings))
    -- nav.views() lists loaded authored and script-created view IDs/names.
    -- nav.remove_target("quest.river")
    -- nav.remove(bar)

create(settings) makes an independent live bar. configure(id, settings) atomically patches a view; get(id) returns its settings; views() lists view IDs/names; remove(id) removes a created view or hides an authored one. Re-enable an authored view with configure(id,{visible=true}). Visibility also respects ordinary live Screen bindings.

put_target(table) registers or replaces a Level-wide target; remove_target(id) unregisters it. Target fields are id, label, category, position={x,y,z}, optional opaque entity=EntityRef, priority and visible. Authored per-view targets use the same fields through configure(...,{targets={...}}). Category filters let independent bars show different subsets.

Settings also include source (camera/player/manual), heading, marker_size, marker_gap, max_distance, show_names, show_distance, show_ticks, categories, icons={category="Content/..."}, strip, pointer, background, font, font_size, text_colour, background_colour, backing_colour, image_tint, border_colour, border_width, opacity, visible, name and x/y/width/height. Colour strings are RRGGBB or RRGGBBAA. Images/fonts use safe existing Content-relative paths; importing external files is an editor authoring operation.

The included Content/UI/Examples/navigation_example.lua demonstrates a world target and a second independent west-facing bar. Scene stop clears views/targets and invalidates created IDs. Entity references are revalidated against the live generation and native transform index.

## C++

EngineBridge WorldNavigationService exposes MakeWorldNavigationWidget, Create, Configure, Remove, View, ObserveDocument, RetainAuthoredViews, PutTarget, RemoveTarget, SetObservers, ResolveEntities, Snapshot and Reset. RuntimeScriptRuntime::Navigation() returns the same service used by the Lua closures.

    auto widget = renegade::bridge::MakeWorldNavigationWidget("Custom compass");
    widget.navigation->fieldOfView=150;
    renegade::bridge::StableId id;
    if (!service.Create(widget,id,error)) { /* report error */ }
    service.PutTarget({"quest.river","Bridge","quest","",{25,0,70},100,true},error);

Authoring uses ScreenAuthoringSession::ConfigureNavigation, ConfigureNavigationComponent and ApplyNavigationAppearance with ordinary validation/history. Runtime mutations are transient. Hosts register theme-resolved navigation prototypes, publish coherent camera/player observers, resolve registered entity IDs through native indexed lookups, and pass Snapshot to the shared ScreenRenderer. Native Runtime already provides this wiring.

Limits: 32 registered views and 512 Level-wide targets; each authored view supports 64 local targets and 64 icon categories. Structural registration and resource loading are separated from live bearing/position updates. No whole-scene per-frame target discovery is used.

## Verification scope

Windows x64 DX12 Release. New CPU/service/Lua checks cover wrap/projection, sources, filtering/range, priority, missing/deleted entity following, history, save/reopen, portable artwork, independent instances, invalid input, scene reset and authored-group preservation. Existing Screen and governed scripting regressions are rerun. Native receipts and exact source are recorded in HANDOFF.md and docs/evidence/world-navigation-2026-11-27. The plain HUD fixture deliberately has no ordinary binding sources; navigation works independently.

Debug, Vulkan, exported-game packaging, small-window/DPI usability, all-four-set content completion and independent release acceptance are not implied.
