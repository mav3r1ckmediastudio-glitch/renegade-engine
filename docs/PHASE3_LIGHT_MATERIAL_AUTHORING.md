# Phase 3 — Light and Material Authoring

## Outcome

Expose the pinned Wicked Engine light and core PBR material controls through
Renegade's existing hierarchy and Inspector without embedding the stock Wicked
Editor windows or creating a second scene/component model.

This milestone is complete only when a creator can select supported scene
entities, edit their native Wicked components through Renegade-owned controls,
Undo/Redo the edits, save and reopen them, and see the same result in packaged
DX12, Runtime, and Vulkan builds.

## Baseline

- Branch base: `c5a2fb8` (`Add native terrain authoring foundation (#11)`).
- Wicked pin: `3a800b7134aafe58461093c8abb2e274d4e64033`.
- Wicked remains an unmodified submodule and parity oracle.
- The existing Renegade Project Hub, workspace, selection, hierarchy,
  Inspector, command history, WISCENE document workflow, and visual language
  remain authoritative.

## Current gate status

- Slice 0 is complete.
- The Light and Material bridge proofs are implemented together in Gate 1.
- All four PR #12 checks passed at `38c9f24` on 2026-08-02.
- Slice 2 Light Inspector CI passed at `32351d9` and its selection-driven UI
  was visually confirmed in packaged DX12.
- Slice 2A is active: native Add Light, surface placement, Directional scene
  icon, and complete packaged lifecycle acceptance.
- No placement, icon, persistence, or Runtime result is accepted from
  compilation alone.

## Verified Wicked capability map

### Lights

`wi::scene::LightComponent` already owns and serializes the required values:

- `type`: Directional, Point, Spot, or Rectangle;
- `color` and `intensity`;
- `range`;
- `outerConeAngle` and `innerConeAngle`;
- `radius`, `length`, and `height` for native source shapes;
- `CAST_SHADOW` and `VOLUMETRICS` flags; and
- `volumetric_boost`.

The stock `Editor/LightWindow.cpp` changes those same component values
directly. Its window is not reusable as a Renegade workflow, but the underlying
component operations are UI-independent and need no renderer-dependent
`Scene::Update()` call from EngineBridge.

### Materials

`wi::scene::MaterialComponent` already owns and serializes the required core
PBR values:

- `baseColor`;
- `metalness`;
- `roughness`;
- `reflectance`; and
- `emissiveColor`, whose alpha is Wicked's emissive strength.

The stock `Editor/MaterialWindow.cpp` uses `SetBaseColor()`,
`SetMetalness()`, `SetRoughness()`, `SetReflectance()`,
`SetEmissiveColor()`, and `SetEmissiveStrength()`. These operations are also
independent of Wicked's stock window.

Every appearance setter marks the material dirty. That is correct for ordinary
mesh materials, because Wicked must refresh their renderer state.

## Terrain safety boundary

Wicked streamed terrain has different invalidation semantics from ordinary
mesh materials. `wi::terrain::Terrain::Generation_Update()` checks every
entity in `materialEntities`; one dirty source material triggers
`Generation_Restart()`, which clears and regenerates terrain chunks.

The generic Material Inspector must therefore reject all terrain-owned
materials, including source-region materials, grass, and generated chunk
materials. Terrain materials remain exclusively behind `TerrainService` until
a separate sculpt-preserving material-authoring slice has a regression test
proving that all loaded chunk height data survives the first update.

This is a service-level rule, not only a hidden/disabled widget, so a future UI
cannot accidentally bypass it.

## Curated state contracts

### `LightState`

Capture and apply only:

- type;
- RGB colour;
- intensity;
- range;
- inner and outer cone angles;
- source radius;
- point-light capsule length / rectangle-light width;
- rectangle-light height;
- cast-shadow;
- volumetrics enabled; and
- volumetric boost.

Applying a `LightState` must preserve cascade distances, forced shadow
resolution, light masks, camera source, lens flares,
visualizer/static/cloud flags, and every future field not listed above.

### `MaterialState`

Capture and apply only:

- base colour RGBA;
- metalness;
- roughness;
- reflectance;
- emissive colour RGB; and
- emissive strength.

The V1 UI edits base-colour RGB while preserving opacity. Applying a
`MaterialState` must preserve textures and UV sets, shader type, blend mode,
normal/displacement/transmission settings, alpha cut-off, shadow flags,
stencil state, animation, custom shader data, and every future field not listed
above.

All numeric input is clamped before it reaches Wicked. Inner spot angle is
never allowed to exceed outer spot angle.

## Material target resolution

Selection resolves an editable material in this order:

1. a non-terrain `MaterialComponent` on the selected entity;
2. a selected object's referenced mesh when every visible subset resolves to
   the same non-terrain material entity; otherwise
3. no material target.

Multi-material subset selection needs a deliberate material-slot picker and is
deferred to the asset pipeline. Renegade must not silently choose the first
subset or overwrite multiple materials together.

## Delivery slices and stop/go gates

### Slice 0 — Audit and contract

- Record the native Wicked fields, setters, serialization, editor reference
  behaviour, Renegade selection path, and terrain invalidation risk.
- Add this bounded plan before production implementation.

Gate: no UI code begins until every exposed field has an identified native
authority, preservation rule, and automated assertion.

### Slice 1 — Light bridge proof

- Add UI-independent `LightState`, capture/apply helpers, and
  `SetLightCommand`.
- Reject missing or incompatible entities.
- Filter no-op edits.
- Add headless tests for apply, hidden-field preservation, Undo, Redo, no-op,
  and removed-entity failure.

Gate: the complete light bridge test must pass before Light Inspector widgets
are wired.

### Slice 2 — Light Inspector

- Extend the existing Renegade Inspector for selected light entities.
- Use Renegade-owned type, colour, intensity, range, cone, shadow, volumetric,
  boost and native source-shape controls.
- Show spot cone controls only for Spot lights; expose radius for Directional,
  Point and Spot; expose capsule length for Point; expose width and height for
  Rectangle; disable range for Directional lights.
- Continuous sliders preview directly, restore the before-state on release,
  and commit one command. Discrete edits execute one command.
- Keep transform controls available for the same light entity; the component
  section supplements rather than replaces Transform authoring.

Gate: packaged DX12 must show a visibly changed light, one Undo entry per drag,
correct Undo/Redo, and no change after Save/Open.

### Slice 2A — Add Light workflow

- Add a permanent Renegade-owned `ADD` menu to the top bar; do not introduce a
  stock Wicked Editor window or a permanent Lighting workspace.
- Offer Point, Spot, Directional, and Rectangle Light entries.
- Execute a UI-independent `CreateLightCommand` that calls Wicked's native
  `Scene::Entity_CreateLight`, gives the entity a unique creator-facing name,
  and stores a native entity snapshot for Undo/Redo.
- Point, Spot, and Rectangle enter a modal placement tool. Raycast the next
  viewport click against terrain or scene geometry, show a live hit/normal
  preview, and consume that click so it cannot also select or sculpt.
- Align new Spot and Rectangle emission to the clicked surface normal. `Esc`
  or right-click cancels without creating an entity or history entry.
- Create Directional immediately five metres in front of the editor camera;
  its position anchors a Renegade-owned sun-and-direction icon even though
  only its rotation affects illumination.
- Draw and hit-test Directional icons entirely in Studio. They must remain a
  constant screen size, select the native entity, and never serialize or
  appear in Runtime as a mesh, helper entity, or Wicked visualizer flag.
- Select every successfully created light automatically so the existing Light
  Inspector opens immediately.
- Use Wicked Editor's native starting values and rectangle source shape rather
  than inventing parallel light data.
- Reuse the existing `DeleteEntityCommand` for Delete and restoration.
- Prove native Name, Layer, Transform, and Light components; all four types;
  unique names; rectangle shape; Undo/Redo; Delete/Undo; and WISCENE round trip.

Gate: packaged DX12 must create, select, inspect, edit, Undo/Redo, Delete/Undo,
save, close and reopen every light type. Repeat the visual workflow in Vulkan
before beginning the Material Inspector.

### Slice 3 — Material bridge proof

- Add UI-independent `MaterialState`, target resolution, terrain ownership
  detection, capture/apply helpers, and `SetMaterialCommand`.
- Add headless tests for direct and single-subset target resolution,
  multi-material ambiguity, terrain rejection, apply, hidden-field
  preservation, dirty-state behaviour, Undo, Redo, no-op, and removed-entity
  failure.

Gate: terrain rejection and ordinary-material preservation tests must pass
before Material Inspector widgets are wired.

### Slice 4 — Material Inspector

- Extend the selected-entity Inspector with Renegade-owned base RGB,
  metalness, roughness, reflectance, emissive RGB, and emissive-strength
  controls.
- Explain unsupported multi-material and terrain targets instead of showing
  controls that do nothing.
- Use the same preview/restore/single-command transaction as lights.
- Never expose or mutate texture filenames in this milestone.

Gate: packaged DX12 must visibly darken an ordinary mesh to smoked near-black,
reduce the hologram core from clipped white to readable cyan, preserve the
terrain sculpt, and produce one Undo entry per drag.

### Slice 5 — Persistence and cross-renderer acceptance

- Save, close, and reopen the edited WISCENE through Renegade's document
  workflow.
- Load the same scene in Runtime.
- Repeat the editor visual check using Vulkan on Windows.
- Confirm terrain remains at 169 sculpted chunks before and after the first
  terrain update.
- Update project truth only with results actually observed.

Gate: Debug and Release CI, all headless tests, packaged DX12, Save/Open,
Runtime, and Vulkan must pass before merge.

## Automated acceptance matrix

| Risk | Required proof |
|---|---|
| A control only changes a copied value | Assert the native component changed |
| Hidden Wicked values are overwritten | Seed sentinel values and assert they survive Execute/Undo/Redo |
| Slider creates hundreds of Undo steps | Assert one completed drag records one command |
| No-op dirties the document | Assert unchanged state returns false and history remains empty |
| Entity disappears during edit | Command fails without history or unrelated mutation |
| Add Light creates UI-only state | Assert native Name, Layer, Transform and Light components exist |
| Add/Undo/Redo changes identity | Serialize the entity snapshot and restore the same entity and unique name |
| Added light is not discoverable | Use the permanent ADD menu and auto-select the created hierarchy entity |
| Directional light has no visible source | Draw a selectable editor-only sun/direction icon at its transform without serialized helper state |
| Placement click performs two tools | Placement consumes viewport input before navigation, sculpting, gizmos, and ordinary selection |
| Editor helper leaks into the game | Keep scene icons in Studio and assert new native lights do not enable the serialized visualizer flag |
| Shared or multi-material mesh is ambiguous | Resolve only one unique material; otherwise reject |
| Generic material edit regenerates terrain | Reject every terrain-owned material in EngineBridge |
| Save/Open changes authored values | Serialize, reopen, recapture, and compare curated states |
| CI passes while visuals are wrong | Require packaged screenshots and owner-observed DX12/Vulkan checks |

## Explicit exclusions

- No stock Wicked Editor windows or stock editor layout.
- No Wicked source or submodule-pointer change.
- No material texture import, reassignment, reimport, or asset database.
- No multi-material slot picker.
- No terrain-region appearance editing through the generic Material Inspector.
- No light cookies, mask assignment, or batch-placement workflow.
- No advanced light cascades, masks, lens flares, cookies, static baking, or
  forced shadow-resolution UI.
- No advanced PBR shader, transparency, clearcoat, sheen, transmission,
  subsurface, displacement, animation, or custom-shader UI.
- No scene tabs, Identity Handshake, renderer rewrite, or new platform target.

## Human packaged checklist

1. Open a freshly generated project in packaged DX12 Studio.
2. Select the Sun, a point light, and a spot light; verify type-specific
   controls and visible live preview.
3. Drag intensity and colour controls; verify exactly one Undo step per drag.
4. Undo and Redo each light change.
5. Select ordinary Proving Ground meshes and edit the core PBR values.
6. Confirm a terrain-owned material cannot be edited in the generic Material
   Inspector and the sculpted terrain never regenerates.
7. Save, close, reopen, and compare the light/material appearance and values.
8. Launch Runtime with the saved scene and compare the appearance.
9. Repeat the editor checks with the `vulkan` argument.
10. Capture screenshots and record the exact tested commit in `HANDOFF.md`.
