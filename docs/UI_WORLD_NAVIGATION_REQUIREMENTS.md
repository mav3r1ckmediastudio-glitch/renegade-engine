# Generic RPG world navigation bar, 27 November 2026

Status: generic navigation is implemented and locally verified on Windows x64 DX12 Release with neutral and supplied fantasy example appearances. Four full launch-set completion and the remaining acceptance work below are pending. See UI_WORLD_NAVIGATION_AUTHORING.md and evidence/world-navigation-2026-11-27.

The owner means the horizontal in-game compass/point-of-interest strip at the top of an RPG world view. The owner will provide four full UI asset sets for launch. Build one generic navigation capability with a neutral starter appearance, with each set supplying an interchangeable visual preset. All may be customised or replaced by creators.

## Shared behaviour and editable appearance

- Separate heading/bearing/target behaviour from artwork. The four appearances use identical marker identities, target bindings and navigation controls.
- Support fixed backing/centre pointer, moving clipped directions/ticks and POI markers, plus optional target name/distance.
- Expose replaceable backing, rail, pointer, direction labels/strip, marker icons, fonts and colours using named editable parts and semantic appearance slots.
- Use semantic marker categories such as quest, waypoint, town, cave, merchant and custom categories. Artwork selection must not determine gameplay logic.
- Changing visual preset retains live targets, marker IDs, bindings and behaviour settings. Preserve creator placement/size unless explicitly applying a preset layout.
- Expose size, position, visible bearing span, marker sizes, label/distance visibility and category filters through friendly settings; advanced controls remain available.
- Creators can use their own images/fonts and save the edited composition as a reusable prefab. Do not require one specific supplied atlas, path or genre.
- The heading source may use the gameplay camera/player or an explicit scripted heading. Target data comes from registered world targets; missing/deleted targets and level changes must be handled safely.
- Provide C++ and Lua creation/configuration/control, including marker add/update/remove, target assignment, appearance selection and visibility. Editor controls are wrappers over shared supported capabilities.

## Existing asset evidence

Inspected the supplied fantasy set's navigation_registry.json and gui/1x/world_compass/compass_config.json plus reference compass_math.js. It supplies 1x/2x backing/rail, bearing strip, fixed needle, direction images and fifteen POI icon categories. The registry describes fixed housing/needle and moving clipped ticks/markers, real caller-provided bearings/distances, +Z north and +X east. Reference code covers wrapped bearing deltas, projection and priority-based decluttering. Native implementation evidence is now recorded separately in evidence/world-navigation-2026-11-27. This asset inspection is not an audit of all four launch sets.

## Acceptance before completion

1. One generic bar works with a neutral appearance and each of the four supplied appearance presets; inventory/validate the other launch sets before claiming parity.
2. Replace individual artwork/fonts/icons and save/reopen without losing behaviour or target identity. Switching appearance preserves bindings and script handles.
3. Native gameplay proves continuous heading wrap at north, correct relative target placement, clipping/visibility, target movement/removal and scene transition cleanup.
4. C++ and Lua examples drive the same component and cover custom target categories, independent instances and invalid/missing targets.
5. Verify coherent authoring Undo/Redo, resource ownership and portable prefab adoption; preview sample data is clearly identified separately from live gameplay.
6. Validate supported resolutions/DPI, asset density selection and populated-scene performance. Do not introduce full-scene per-frame target discovery.
7. Record builds/tests and native evidence before marking implementation complete.

Minimap rendering is a separate capability that may share target registration/categories and appearance conventions. The existing editor overview is not evidence of a gameplay minimap.


## World-target authoring gap raised by owner, 27 November 2026

The current compass presents registered fixed positions or entity references. It does not infer towns, castles, caves or bosses from geometry, names or artwork. The Level editor does not yet expose a complete world-target placement/tagging workflow, and gameplay minimap rendering is not implemented.

The proposed next authoring workflow is a reusable World target on a selected entity or a standalone invisible point marker, with name/category/icon and explicit visibility/discovery rules. Targets should enter the same shared registry for compass, future minimap and C++/Lua. A moving entity target follows its transform; deletion/scene transitions retire it. Proximity, reveal-on-entry and quest/script-driven disclosure must be explicit rules rather than assumed recognition. Exact discovery defaults and UI remain design work; this section does not claim an approved or implemented discovery system.

## Owner-selected authoring direction: Location Zone

The owner explicitly prefers extending the existing gameplay zone system instead of maintaining a manual navigation-target list. Place a Location Zone in the centre of a town or at a cave entrance; placement selects it and opens its Inspector. The creator sets Name, Icon and Type (examples: Location, Quest, Dungeon, POI, Event, plus custom types). Reuse existing zone geometry, transforms, resize handles, command/history, persistence and script/event infrastructure.

Basic location setup must not require typed world coordinates, entity IDs, manual target registration or keeping a separate category-to-icon mapping in the compass editor. The zone provides its world position automatically and registers/updates/retires the corresponding target through the shared navigation registry. A location-specific icon is independent of its semantic type: assigning a unique cave icon must not require a fake category or silently restyle every other cave. Preset/theme type icons may be defaults, with a project-owned per-location override.

The navigation bar's Basic settings should focus on appearance, placement/size, visible range and optional labels/distances. Manual targets, scripted headings, internal IDs and specialist filtering belong under Advanced. The same zone targets should serve the compass and a future minimap; this does not claim minimap rendering exists.

Discovery/visibility settings, entry/exit events and scripted overrides belong under Advanced so Name/Type/Icon remain the ordinary flow. Exact default visibility, undiscovered presentation, discovery trigger size, remembered discovery state and save-game persistence still require a concrete design/implementation; do not assume these decisions from the zone placement proposal. Zone bounds define an entry/discovery area, while its placement supplies a marker anchor. A large town and a small cave entrance can use different zone extents through the existing controls.

C++ and Lua must expose the same creation/configuration, icon/type assignment, enable/reveal/hide, query/events and lifecycle operations. Imported or prefabbed zones need destination-owned icon resources and fresh stable identities. Acceptance must cover automatic Inspector opening, creation/move/resize/delete, Undo/Redo, save/reload, prefab portability, scene changes, custom type/icon independence, scripting parity and actual standalone compass display/discovery. Index zone membership on lifecycle/content changes; use direct native lookups for live transforms.

Status: owner direction recorded, not implemented in this checkpoint. Inspection confirms the existing gameplay zone branch has thirteen presets, native bounds editing and entry/exit sampling. The UI working branch predates that zone integration; integrate against the current zone implementation before extending it rather than creating a parallel zone system. Studio remains closed at the owner's request.


## Location implementation checkpoint, 27 November 2026

The preceding planned-direction paragraphs are superseded by the current local implementation: existing gameplay zones are integrated, Location is appended as preset 13, its Inspector owns Name/Type/Icon and proximity announcement settings, and the compass Basic panel consumes live locations. None/Fade/Glitch/Wobble effects, repeat-on-return, explicit zone-entry discovery and matching governed Lua controls are implemented. Discovery defaults to visible-before-entry and is remembered only for this Level visit. Native locations exist before script on_start callbacks. See UI_LOCATION_ZONE_AUTHORING.md for defaults, scripting and limits. Focused contracts pass; native authoring/standalone visual acceptance remains pending.
