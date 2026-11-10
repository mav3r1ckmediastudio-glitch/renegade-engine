# Current creator workflow guide

**Baseline:** integrated main 618b231, 10 November 2026. This is a practical entry
guide, not a claim that all tutorials or every release gate are owner-approved.
See [current status](CURRENT_STATUS.md) for exact merges, tests and limits.

## Open a project and save your work

Use Studio's Project Hub to create/open a project, then its normal Level/Scene
workflow. Keep source assets inside the governed project workflow; a loose file
copied beside an executable is not a registered project dependency. Save the scene
with Ctrl+S. Use normal Save As/Reopen when checking persisted settings.
Keep backups and test unfamiliar changes in a separate project.

New levels do not automatically place a Player Start. Place one from
Content/Player when gameplay requires it; the project supports one governed start
per level. Change the existing start rather than placing a second.
[Player authoring](PLAYER_AUTHORING_CONTINUATION.md).

## Import models and Characters

Use Import Model, choose the supported file, review preview/material maps and
destination, and commit the asset. Place the resulting Asset Browser card.
Static OBJ/GLB/FBX and Character workflows share governed identity/provenance;
a file extension alone does not prove every feature of that format is supported.
Character import has its own animation/action assignment path.
[Importer instructions](MODEL_IMPORTER_REBUILD.md);
[Character workflow](CHARACTER_WORKFLOW_PROGRESS.md).

## Select, transform and make assemblies

1. Click an object; Shift-click toggles objects in viewport or hierarchy.
2. Drag empty viewport space for box selection. Left-to-right encloses;
   right-to-left intersects. Shift adds. Active authoring tools keep their gestures.
3. Move/rotate/scale uses a shared selection pivot. Duplicate/Delete acts on
   selected roots as one history action.
4. Choose Group. It becomes one selectable unit. Edit Pieces enters it;
   Finish Editing or Escape returns to the group.
5. Save Prefab asks for a unique name and stores it in Content/Prefabs.
   Drag its Asset Browser card into the scene to create another independent copy.
6. Unpack exposes the real objects. A controller/root carrying its own behaviour
   remains as a named individual object; empty structural wrappers disappear.
7. Undo/Redo restores the corresponding operation; Save/Reopen checks persistence.

Prefabs can include lights, complete particle effects, governed scripts and
material/shader/resource state. Script references must point inside the selected
assembly, or be cleared before saving. The modal explains an external-target
error. A light/effect-only prefab is valid. There is no explicit entity-count cap,
but very large assemblies are not yet stress-tested.
[Complete workflow and evidence](SCENE_ASSEMBLY_WORKFLOW.md).

## Terrain, rivers and waterfalls

Create finite terrain in the Terrain workspace. Fresh recovered terrain starts
at world Y=0 with carving depth below it. Expansion/sculpt/paint use the existing
commands. Recovery covers through 33×33 chunks, with the existing safety budgets.

Use the Terrain Rivers controls to author a spline, then Link / Update Bed to
apply the corresponding terrain layer. Keep ordinary shallow operations within
their bounds; Allow Deeper Beds is an explicit policy choice, not the standard
repair for new terrain. Later point/width/history edits must preserve the linked
terrain transaction. Waterfalls have their own placeable controls in the river
authoring area.
[Maximum terrain recovery](MAXIMUM_TERRAIN_RIVER_RECOVERY_2026-11-03.md);
[riverbed repair](RIVERBED_PERFORMANCE_2026-11-05.md);
[waterfall/persistence record](RIVER_G13E11_PERSISTENCE_RUNTIME_PACKAGING_2026-10-27.md).

## Environment and weather

The Environment workspace owns time/sun, sky/night controls and weather.
Use its weather presets and immediate/timed transition controls. Clouds,
lightning/thunder, rain, snow and grass-wind behaviour are integrated. Night
gradient, aurora and Milky Way have separate authored controls. Save/Reopen and
Test Game should use the same level settings. Different scene/content/settings
can produce different performance; a reported FPS figure is not a guarantee.
[Weather integration](CURRENT_STATUS.md);
[night-sky scope](NIGHT_SKY_OWNER_ACCEPTANCE.md).

## Lights

Choose an Add-menu light entry to open the Light Picker, or open it from an
existing light. Select type/colour/intensity/range/shadows and optional behaviour.
Flame, Electrical Fault, Constant, Rotating, Pulsing and Strobe use governed
behaviour attachments. Save project colours/presets explicitly. Updating a preset
does not silently update placed instances.

Keep placing defaults on; RMB finishes placement and consumes that gesture before
fly-camera navigation resumes. Use typed values when a slider is inconvenient.
[Full light instructions](LIGHT_PICKER_AND_BEHAVIOURS.md).

## Layered particle effects

Add > Particle opens the picker. Add/select layers and choose textures/sheets
through Included/Project browsing. Edits remain in the private draft until Apply
or Place. Stationary/looping sprites stay in place; continuous emitters spawn
particles. Use integer atlas rows/columns/frames/FPS matching the actual sheet.

Fields and per-layer influence control motion response. Lifetime edits colour,
size and opacity curves. Shapes changes emission geometry; Timing controls
delay/duration/loop/bursts. Save As New creates a separate project preset;
Update Preset is explicit and does not mutate placed copies.
RMB finishes repeated placement. Select an effect and use Edit Effect to edit it.

[Picker](PARTICLE_PICKER_AND_LAYERED_EFFECTS.md);
[textures](PARTICLE_TEXTURE_LIBRARY.md);
[lifetime](PARTICLE_LIFETIME_CONTROLS.md);
[shapes/timing](PARTICLE_SHAPES_TIMING.md);
[Lua](PARTICLE_LUA_API.md).
Local QA starter catalogs are not a promise that every project installs the
same preset collection.

## Scripts and reuse

Use Inspector ACTION/SCRIPT/GLOBAL SCRIPT authoring and typed properties.
Library adoption copies governed script/module closure into the project;
the project copy becomes Runtime authority. Supported light/particle APIs use
the same governed script runtime. Check each API's documented scope; complete
Lua access to every engine subsystem and a general native plugin SDK are not
established.
[Library workflow](SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE.md);
[particle Lua](PARTICLE_LUA_API.md).

## Test and build a game

Use Studio's normal Test Game/PLAY workflow. The activity indicator is indeterminate;
initial native capture can still pause briefly before background preparation.
It is not a percentage or a promise of instant launch.

Build Game uses governed dependency extraction, staging, executable identity and
package integrity. Fix missing dependencies through their existing authoring
workflow; do not bypass validation or copy unrelated source assets into output.
Test the actual named executable after packaging and relocation. Building or
merging does not authorize distributing another alpha or third-party content.
[Standalone contract](LP06_NAMED_STANDALONE_SAFE_REBUILD.md);
[developer build guide](BUILD_WINDOWS.md).

## Where to find help

[Documentation index](README.md) separates live guidance, specifications,
historical checkpoints and scoped evidence. The integrated assistant's verified
answers are deliberately narrower than arbitrary documentation search. A retrieved
plan is not proof that a feature exists. The older
[projectile tutorial](manual/PROJECTILES_FIRST_WEAPON.md) remains a scoped draft
with its original screenshots and publication limits.
