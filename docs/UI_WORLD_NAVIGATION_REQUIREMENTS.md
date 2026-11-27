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
