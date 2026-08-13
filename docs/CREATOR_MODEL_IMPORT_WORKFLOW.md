# Creator Model Import Workflow

## Scope
PR #57 restores Renegade Studio's guided creator model-import workflow. Importing a model first opens an isolated preview workspace. The preview is temporary; authoritative project assets are created only after the creator accepts the import.

## Workflow contract
`ADD -> IMPORT MODEL...` accepts FBX, GLB, and GLTF. The Model Import Workspace owns the screen while active: normal scene search/Inspector controls are hidden, and the right-side task workspace is divided into Asset, Transform & Scale, Material, Lighting & Scale Reference, and Animation sections. Cancel restores the previous editor state. `IMPORT & PLACE` commits the governed reusable model and places that governed asset in the level.

The source filename seeds an editable asset name. The creator can also choose a canonical project destination below `Content`; the workflow rejects traversal, destinations outside Content, and an explicit name that is already occupied.

Position, rotation and scale use slider-plus-number controls. Continuous sliders
share a recessed neutral rail, orange filled range and high-contrast diamond
handle; the numeric field remains available for exact entry. Animation range
boundaries remain numeric fields rather than sliders. XYZ scale is linked by
default and can be unlocked. Real-dimension presets derive their factor from
measured model height rather than fixed multipliers. The preset list includes
0.50 m small prop, the canonical 1.82 m human height, 2.04 m doorway and 3.00 m
large prop, while source-size and unit conversion options remain available.

## Material resolution
Each imported material is handled independently. Texture sources resolve in this order: an existing imported material binding, a recognised filename suffix beside the model, then an explicit creator replacement or removal.

Recognised suffixes are `_color`, `_normal`, `_surface`, `_roughness`, `_metalness`, `_ao`, and `_emissive`. A supplied `_surface` map is preferred. If no Surface map is supplied, roughness, metalness and AO component sources can be packed during commit.

The Wicked Surface layout used by Renegade is R=AO, G=roughness, B=metalness, A=reflectance. Preview and commit use the same bridge-level material resolution contract, including generated Surface packing. A supplied Surface map is normalized through that packer so its channels and creator scalar choices cannot diverge between preview and the committed product. Missing Surface data uses a neutral non-metal fallback: roughness 0.75, metalness 0.0 and reflectance 0.04. The generated reflectance channel is therefore 10/255 by default, not full-white 255. Component images with different dimensions or corrupt image data are rejected rather than silently altered.

Generated preview Surface filenames include the resolved source identity, file
state and PBR scalar inputs. Wicked caches GPU resources by filename, so this
cache identity is required for a newly browsed Surface map to replace the old
preview immediately. Texture preview loading uses Wicked's native per-slot
flags, including normal-map handling, and a decode/load failure is shown in the
Material section rather than leaving the picker path changed with no visible
effect.

Each material exposes roughness, metalness, reflectance, normal strength, AO strength and emissive strength with slider-plus-number controls. These values are stored in the creator recipe and replayed during reimport.

The selected material displays a MAX-style vertical map list for Base Color,
Normal, packed Surface, Roughness, Metalness, AO and Emissive. Every map owns a
labeled thumbnail, filename-tail path and browse row; hover and the editable
path field expose the complete source path. The material selector identifies
the mesh/subset using the material and includes its Base Color filename when
available, rather than exposing only anonymous `Material.00x` labels. PBR
scalar controls follow the map rows in the same scrollable material workspace.
A missing map is shown explicitly rather than borrowing another slot's image.

Preview lighting is editor-only and never enters the model product or level. The creator can adjust intensity, horizontal direction, elevation and ambient brightness, use Neutral/Outdoor/Dark presets, or reset to neutral. A toggleable neutral male reference is drawn at exactly 1.82 m beside the model. It is never parented to the import, never inherits its transform, and its side offset follows the current world-space preview bounds plus fixed clearance.

The initial preview camera is also derived from those measured, scaled bounds. It uses a neutral 32-degree perspective lens and a bounding-sphere fit instead of a fixed close camera position, preventing character heads, hands or boots from acquiring exaggerated near-camera proportions. The creator's previous editor lens and transform are restored on commit or cancel.

The temporary stage is positioned beyond the normal editor camera's far plane, but remains close enough to the world origin to preserve sub-millimetre 32-bit transform precision. This avoids preview-only vertex/normal quantization; it does not modify or repair source mesh data.

Studio uses one neutral visual system across the importer and ordinary editor:
opaque dark blue/black panels, consistent white text, restrained grey focus
treatment and no cyan or translucent glass chrome. Headings, readouts and
controls share the same surface treatment rather than using pale label slabs.

The importer task workspace is docked eight pixels from the application right edge and uses the available application height rather than being positioned inside the ordinary editor viewport. Long sections scroll while Import & Place and Cancel remain docked. This returns previously unused space to the model preview and makes the panel read as owned workspace chrome instead of a floating popup.

The 1.82 m reference is derived directly from the measured source bounds and current root transform rather than waiting for live render AABBs. It is placed on the unobstructed left side of the preview, clear of the task panel, and uses a neutral dark silhouette plus a capped world-space 0.00 m-to-1.82 m ruler matching the original creator reference intent. The panel reports the imported model's independently measured height, so a short Automatic result cannot be mistaken for an oversized ruler. Its draw transform explicitly includes the active preview camera view-projection matrix, as required by Wicked's dummy visualizer.

The taller orange-range slider renderer is importer-only. Existing Studio
Inspector, Environment and Terrain controls retain Wicked's compact native
slider renderer because those workspaces use denser absolute row layouts.

## Persistence
Creator material choices are persisted using governed LP08 texture stable IDs inside the LP07 reusable-model creator recipe. Base Color, Normal, Surface, Emissive and AO bindings survive reimport through that recipe. Invalid stable IDs, duplicate material indices, invalid animation ranges and unknown recipe fields are rejected.

The packaged Runtime continues to consume governed products rather than editor source files. The existing Base Color compatibility mirror remains supported for older data.

## Animations
The importer exposes the animation actions present in the imported Wicked scene. Creator clip records contain source animation index, name, start, end and include/exclude state. Clips can be renamed, ranged, added, deleted and excluded, and the stored recipe is replayed on reimport. Stored ranges must remain within the source action.

The current start/end values are Wicked timeline values. PR #57 does not claim they are source-file frame numbers where reliable source frame-rate metadata is unavailable.

## Fail-closed guarantees
PR #57 preserves the previously established model/resource lifecycle guarantees: invalid recipes do not become authoritative; rejected reimport does not mutate existing product or registry bytes; prepared models still require real mesh/object evidence; governed texture cache identity remains payload-SHA-256 based; and governed required textures remain package dependencies.

## Deliberate exclusions
This PR does not yet include automatic PBR derivation from a single colour image, final collision-authoring UX, a separate import-to-assets-only action, or source-frame-number conversion.

## Automated acceptance
`RenegadeReusableAssetReimportRecipeTests` now covers supported recipe and PBR scalar round-trip, invalid recipe rejection, multi-material texture discovery and precedence, neutral no-Surface fallback, exact Surface channel packing (including neutral reflectance) and defaults, dimension mismatch and corrupt-input rejection, distinct preview cache identity for changed Surface sources/PBR values, plus authoritative-byte preservation after rejected reimport. `RenegadeImportTests` covers bounds measurement and real-height preset scaling.

Existing LP07/LP08 tests continue to cover reusable-model conversion, placement, resource lifecycle, package closure, cache identity and Runtime behaviour.

## Owner acceptance
CI is necessary but not sufficient. Before accepting PR #57, test the Release artifact with the previously problematic crocodile FBX, a textured GLB/GLTF, a multi-material model where available, and an animated model where available. Confirm preview visibility, material selection and texture detection, animation editing, Import & Place, save/reopen/reimport, and at least one packaged textured model.
