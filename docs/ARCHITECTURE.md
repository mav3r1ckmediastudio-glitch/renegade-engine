# Architecture

## Dependency direction

```text
Wicked Engine
      |
      v
EngineBridge services
   |            |
   v            v
Studio       Runtime
   \            /
    v          v
 Projects, assets, WISCENE
```

Dependencies flow downward. `WickedEngine` must not depend on Renegade layers.
Studio panels must depend on bridge interfaces rather than reaching arbitrarily
into Wicked internals.

## Layers

### WickedEngine

Pinned upstream dependency containing rendering, ECS/scene, physics, animation,
resource management, audio, input, jobs, importers, serialization, shaders, and
the original editor used as a parity oracle.

### EngineBridge

UI-independent adapters and services that translate product workflows into
Wicked operations. Initial service boundaries:

- `ProjectService`
- `ScreenService`
- `SceneService`
- `SceneDocumentService`
- `SelectionService`
- `CommandService`
- `AssetService`
- `ImportService`
- `BuildService`
- `PlaySessionService`
- `SettingsService`
- `DiagnosticsService`

### Studio

Renegade's editor shell, panels, layout, project hub, content browser, hierarchy,
inspectors, viewport tooling, preferences, and visual language.

The Phase 3 shell uses the accepted native `wiGUI` integration as a canvas,
update scheduler, and low-level input host. It does not use Wicked's stock
widget rendering as Renegade's visual language. `RenegadeStudioChrome`
overrides `Widget::Render` and draws the Studio shell from Renegade-owned
primitives and design tokens. Interactive components will migrate onto that
visual foundation one accepted vertical slice at a time.

The first functional slice uses `RenegadeTextInputField`, `RenegadeButton`,
`RenegadeCheckBox`, and `RenegadeComboBox`. They inherit wiGUI's proven focus,
keyboard and pointer mechanics but override rendering completely. This is the
standard migration seam: reuse low-level interaction machinery when it is
useful, while Renegade owns all visible pixels and all workflow composition.

`ProjectService` owns `.renegade` descriptor validation and recent-project
state. Studio calls that service rather than parsing project files in widgets.

`SceneDocumentService` is the UI-free scene-document operation boundary.
WISCENE preparation uses Wicked's archive and scene serialization on Wicked's
job system without mutating the active document. Studio commits the prepared
scene, document path, selection reset and command-history reset together at a
Wicked thread-safe point. `SceneService::LoadScene` is reserved for Runtime
startup and shares the same archive preparation operation; Studio must not use
it as a second editor lifecycle.

Save and Save As cross the same boundary. A save serializes the active Wicked
scene into a same-directory temporary WISCENE, deserializes it as validation,
protects the existing destination, replaces the destination atomically and
validates the final file before marking the command history saved. The previous
valid version remains as an openable `*.bak.wiscene`, while successful saves
also maintain the newest ten WISCENE files under
`Saved/Backups/Scenes/<scene-name>`. A backup warning never disguises a
successful primary save, and a primary failure never changes the document path
or clears dirty state.

`CommandService` owns persistent Studio scene mutations. Full local transforms
use a toolkit-independent `TransformState`; duplicate and delete commands use
recursive in-memory Wicked entity archives so Undo/Redo can restore the same
entity IDs. Studio may preview a live gizmo transform, but it restores the
before-state before committing the completed command.

The generated Proving Ground is described by a renderer-independent blueprint
and instantiated as uniquely named, creator-facing scene entities. The reserved
`__renegade_internal_` prefix remains available for legacy/internal scene
helpers and `SceneService` excludes such entities from creator-facing lists.
The editor grid is not a scene helper: Studio draws it in an explicit
renderer-owned colour/depth pass, so it is never selectable or serialized.

Component authoring follows one shape, established by `SetTransformCommand` and
extended by `SetWeatherCommand`. A toolkit-independent state struct captures the
curated subset of a Wicked component that Renegade exposes; the command stores
a before and an after state; applying a state writes only the fields the struct
covers and leaves every other value on the component untouched. `WeatherState`
therefore authors sky mode, exposure, ambient, fog, and the primary cloud layer
while the second cloud layer, wind, rain, ocean, weather maps, and advanced
scattering parameters survive an edit unmodified. Light and material authoring
must follow the same pattern.

Creator-facing entity creation follows the same command boundary. The first
implementation, `CreateLightCommand`, calls Wicked's native
`Scene::Entity_CreateLight` and snapshots the complete resulting entity through
Wicked's `Entity_Serialize`. Undo removes it and Redo restores the same entity
identity without reconstructing a parallel Renegade light. Studio owns the Add
menu, surface-raycast placement, selection, and Inspector transition; the
bridge owns native creation and history. Point, Spot, and Rectangle use a
modal click-to-place tool; Directional creates camera-relative because its
position is only an editor anchor. Light discoverability uses distinct
screen-space Studio markers rendered and hit-tested from every native light
transform. They are not scene entities, components, meshes, serialized light
visualizers, or Runtime features. The custom hierarchy groups visible entities
by their native component role, keeps those groups collapsible, reveals a new
selection automatically, and retains wheel scrolling for large groups.

Dedicated state services extend that curated model without widening
`WeatherState`: `PrecipitationState`, `SunState` and `OceanState` own their
respective native fields and commands. `OceanState` is intentionally complete
for the pinned `OceanParameters`, including FFT resolution and spectral wind.
`ApplyOcean` invalidates Wicked's lazy FFT runtime only when an authored change
requires recreation, while always synchronizing the serialized primary Weather
component into `Scene::weather`.

### Model import boundary

**Rebuild state (1 October 2026):** The legacy guided creator importer and model import transaction were removed on the no-import baseline. The paragraphs below record historical V1/v3 design, not a current Studio capability. The new `ModelImportCandidateService` begins with isolated GLB conversion and truthful scene evidence. An experimental native ADD > IMPORT STATIC GLB entry now converts and commits static embedded GLBs; its preview extension renders a private scene clone with an independently framed camera and neutral lighting. Native rotation changes only that camera; Studio captures a 512 by 320 PNG and supplies it to the commit service, which validates it and includes it plus its managed projection path in the same journaled transaction. Headless callers may omit a thumbnail. The image bypasses GUI theme tint and retains its ready render until rotation. Release graphics proofs pass nonblank pixels, changed rotated pixels, candidate isolation and persisted thumbnail decode; the corrected native Bow preview was visually inspected. Owner preview/rotation acceptance and independent fd8b247 native cancel, thumbnail commit/card, placement Undo/Redo/save and fresh-process project reopen checks passed on 1 October; evidence and bounded limitations are in HANDOFF.md. The static GLB transaction and placement/Undo/Redo/save/reopen graphics proof pass in Windows VS18 Release on disposable projects. Studio's asset drag preview update runs after GUI callbacks and before chrome pointer-consumption returns, so a queued release is processed in the same frame. The owner confirmed native Bow 05 drag/drop in the repaired executable on 1 October 2026. It targets the retained `.rasset` reader, catalogue, stable registry and placement loader; see `docs/MODEL_IMPORTER_REBUILD.md` for its acceptance contract.

`ImportService` is the UI-independent boundary for Model Import V1. Renegade
compiles Wicked's standalone `ModelImporter_GLTF.cpp` conversion unit into
EngineBridge without enabling or linking the stock Wicked Editor application.
The service uses a two-stage operation. `PrepareGltfAsset` imports GLB/GLTF into
an isolated heap-backed `Scene` and summarizes its native components on
Wicked's job system. `CompleteGltfAsset` receives that move-only prepared scene
at `EVENT_THREAD_SAFE_POINT`, writes the reusable WISCENE, reloads it, and
rejects a round trip whose component structure changed. This follows the
upstream Editor boundary: model conversion is asynchronous, while WISCENE save
runs at the engine-safe point. Neither stage merges into the active Studio
scene.

The pinned Wicked converter is editor-independent but not renderer-independent:
it calls native mesh/material render-data creation and `Scene::Update()` during
conversion. `ImportService` therefore requires an initialized Wicked graphics
device and returns a clear error without one. A genuinely GPU-free importer
would require an approved Wicked core patch or a maintained Renegade fork of
the conversion code; Model Import V1 takes neither path.

The creator importer is a dedicated Studio mode rather than ordinary editor
chrome plus a popup. Temporary preview geometry remains isolated at a remote
stage and is removed back to a captured command-history baseline. Preview-only
lighting, ambient state and the 1.82 m male scale reference never serialize.
Model bounds are measured from Wicked's updated world-space object AABBs, with
raw mesh bounds retained as a headless fallback; real-dimension scale presets
therefore target measured height rather than arbitrary multipliers.
The accepted creator position, rotation and scale are serialized in the model
recipe and applied as a marked root inside the governed WISCENE payload. New
`.rasset` placement preserves that authored root at unit wrapper scale; legacy
products without the marker retain automatic placement normalization.

Importer v3's local Model completion gate is stricter than transaction success:
`ReusableAssetService` reopens the committed `.rasset` and checks its identity
and payload hash; `CreatorAssetWorkflowService` checks the current catalogue
entry and reopens the embedded scene through the normal stable-ID placement
loader. Studio separately requires Asset Browser reveal before showing READY.
A failure after commit leaves the retained source in place and reports that a
product was committed so the creator does not blindly retry the same name.
The v3 preview defaults to literal source scale; conversion is an explicit
creator choice. These changes are local WIP until build and owner acceptance.

The native v3 inspector now uses six Renegade-owned heading buttons as its
stage selector. Only the active stage's controls are visible; Model skips Rig
and Animations, while Character exposes current source rig evidence without
claiming verified mapping or external retarget playback. The stage layout
remains a Studio presentation concern over the existing bridge commit path.

`CreatorModelMaterialPreparationService` is the authoritative material seam for
both temporary preview and governed commit. It resolves the same declared,
suffix-detected and creator-overridden sources, normalizes supplied/generated
Surface data to Wicked's R=AO/G=roughness/B=metalness/A=reflectance layout, and
persists material scalar values in the reusable-model recipe. No-Surface
materials default to a neutral dielectric (roughness 0.75, metalness 0.0,
reflectance 0.04). This prevents preview and final placement from drifting.

Gate 5 package promotion remains an atomic same-volume directory transaction.
On Windows, only access-denied, sharing-violation and lock-violation rename
results receive a bounded retry window (50 attempts at 100 ms) to allow Runtime
descendants or scanners to release package handles. Permanent locks still fail
closed with candidate evidence retained and any previous valid build restored;
all non-transient rename errors fail immediately.

Terrain sculpting treats Wicked's streamed chunks as views into one canonical
integer height grid. Neighbouring 67x67 chunk meshes deliberately duplicate
their shared edge and corner vertices; a sculpt edit is therefore calculated
once per global grid coordinate, copied to every duplicate, and followed by a
cross-chunk normal/tangent rebuild. Smooth samples the same global grid. The
native per-chunk 16-bit height data remains the serialized authority and a
whole multi-chunk stroke remains one `SculptTerrainCommand`.

Until material importing exists, new terrain uses a bundled grass PBR default.
The accepted source maps remain 4096-pixel RLE TGAs. The
`RenegadeTerrainSurfacePacker` tiles base colour and normal data and packs
Wicked's surface layout (R=AO, G=roughness, B=metalness, A=reflectance) into
three complete RGBA TGAs beside both executables. The rejected Gate 5 DDS
replacement was truncated and was shared by new-Terrain creation and
old-project rebinding. Scene load rebinds only filenames identifying the
bundled TGA default, so later creator-assigned terrain materials are never
overwritten.

Renegade finite terrain uses a fixed authored centre and retains native chunk
height/blend data as the WISCENE authority. The standard extent is radius 9:
19x19 chunks at 66 metres per chunk and one-metre vertex spacing, producing a
1.254 km square. Expansion increments the radius without calling Wicked's
`Generation_Restart()`; the generator fills only missing outer coordinates.
Undo cancels active generation and removes only chunks beyond the former
radius. Wicked's stock camera-following removal is deliberately disabled for
authored terrain because it erases the authoritative chunk entry, not merely
its render/physics residency. A future residency layer must preserve authored
height/blend data separately before enabling that removal path.

### Renegade-owned shaders

Renegade may own shaders without forking Wicked. `Studio/shaders` holds
standalone HLSL with an inline `[RootSignature]` and its own constant buffer,
deliberately free of `globals.hlsli` and `ShaderInterop` so that Wicked's shader
source tree does not have to ship beside the executable. The build copies them
to `Content/shaders/`, which the Windows packaging script already collects, and
Studio compiles them at load by pointing `wi::renderer::SetShaderSourcePath` at
that directory just long enough to resolve them. Wicked's own `Example_ImGui`
sample uses the same approach. A shader that fails to load must degrade to a
missing feature and a logged warning, never a failed startup.

### Editor preferences

`ProjectService` owns editor preferences, stored in an `editor` section of the
same `wi::config` state file that holds the recent-project registry. Preferences
describe how the creator likes Studio to behave rather than what a project
contains, so they must never reach a WISCENE or a `.renegade` descriptor. A
read-only settings location degrades to a session-only preference rather than
an error. Grid visibility is the first; camera speed and editor layout are still
session-only and should adopt this route rather than inventing a second one.

### Story Flow project-home lifecycle

Project format v1 supports two backward-compatible launch-root shapes. A
scene-first project declares a safe project-relative `startup_scene`. A Story
Flow-native project leaves `startup_scene` empty and declares a complete stable
`startup_flow_id` plus safe project-relative `startup_flow` pair. A descriptor
with neither root, or with an incomplete Flow pair, fails closed.

`ProjectService::CreateStoryFlowProject` creates the canonical Flow containing
exactly one permanent Game Start before committing the descriptor. Studio stages
that candidate through `StudioProjectService`, resolves and reads its Flow, and
only then adopts project identity. The Flow-only adoption boundary clears the
neutral in-memory Scene after commit so content from the previous project cannot
leak across the switch. Scene-first projects retain the existing prepared-Scene
adoption boundary and `StoryFlowProjectHomeService` remains their transactional
migration seam.

Runtime prefers the configured Flow and does not synthesize a startup Scene.
It can enter Game Start with no loaded Scene and resolves/loads WISCENE content
only when traversal reaches a Level. Dependency extraction likewise omits an
empty Scene root and follows the startup Flow.

### Story Flow authoring projections

`StoryFlowAuthoringSession` and `StoryFlowAuthoringModel` remain the one
authoritative semantic editor boundary. Graph View and Journey View consume that
same model, selection and history. Neither view owns a second node/route format.

`StoryFlowJourneyModel` is a deterministic read-only projection. It follows the
first ordered exit from Game Start for the main reel, emits additional ordered
exits as subordinate tracks, represents semantic nodes exactly once across
merges/loops and keeps unreachable content visible on detached tracks. The
separate `StoryFlowLayoutDocument` stores Graph positions plus Journey pan/zoom,
active view and card offsets. Schema-v1 Graph layouts migrate to schema v2 with
Journey as the default; editor layout data never changes Runtime traversal.

Level/Screen activation reuses their governed lifecycle boundaries. The
workspace recognizes a bounded second click on the same card/node and the
Studio integration dispatches it to the existing Level Editor or Screen Editor
handoff. Game Start and terminal destinations are not editor-activatable.

### Runtime screen and action boundary

`ScreenService` owns the serialized Renegade `runtime-screen` document above
Wicked. Screen schema v2 carries a stable document envelope, explicit design
size and fit/fill/stretch canvas policy, bounded widget records, parent/anchor
layout, stable action IDs and deterministic focus order. It also serializes the
complete editable appearance contract for the current Image/Text/Button slice:
normal, hover, pressed, focused and disabled colours/state images, opacity,
border/corner data, typography, wrapping and alignment. A font is never selected
implicitly: every text-bearing widget identifies either the explicit
`builtin:liberation-sans` resource or a project-contained `Content/.../*.ttf`.
Schema-v1 runtime-proof Screens migrate in memory to schema v2 with their
accepted appearance made explicit; subsequent writes are deterministic v2.

Image, state-image and project-font resources must be safe paths inside the
owning project's `Content` tree and are projected into dependency extraction.
Malformed documents, duplicate IDs, cyclic/missing parents, unsafe paths,
invalid anchors or appearance, missing required controls, unknown actions and
ambiguous stable-ID resolution fail closed.

Runtime presents the document on its existing `RenderPath3D`/`RenderPath2D`
seam; it does not create a second application window or embed the stock Wicked
Editor. Renegade owns focus, navigation, hidden/disabled skipping, pointer focus,
confirm/debounce and the `RuntimeActionRequest` evidence shape. Mouse, keyboard
and gamepad-labelled requests enter the same `RuntimeActionDispatcher`.
The stable `play` action enters the existing LP02 project flow loader, while
`quit` requests normal application-owned Win32 shutdown.

Gate 8B places the sole Wicked-backed implementation in
`Renegade::ScreenRenderer`. It emits Renegade-owned pixels while retaining
Wicked's proven GUI scheduling and button input mechanics. Runtime delegates all
Screen presentation to this renderer; Gate 8C preview must instantiate the same
class rather than reproduce its drawing rules. The renderer consumes authored
state colours/images, governed font identity and metrics, shadow, alignment,
wrapping, opacity, corner radius and exact inset border geometry. Canvas,
border, corner, font and shadow metrics use the same resolved transform, so the
default 1280x720 design maps exactly to 1920x1080 at 1.5x.

Visual-state precedence is deterministic: disabled, pointer pressed, pointer
hover, keyboard/gamepad focus, then normal. Non-interactive Image/Text widgets
remain in their authored normal state instead of inheriting Wicked's disabled
fade. The custom draw path does not call the stock Button/Label/Image renderer,
so Wicked cannot introduce a second colour, font, corner or opacity policy.
`BuildScreenRenderItems` is the renderer-independent frame evidence seam used
to test exact ordering, state choice and scaled metrics without a GPU.

Gate 8C adds `ScreenAuthoringSession` as the sole mutable Screen Editor document
boundary. It keeps complete validated Screen snapshots in bounded history,
commits through `WriteScreenDocument`, and converts edits to anchored widgets'
resolved rectangles back into parent-relative offsets without replacing their
anchor contract. The native `RenegadeScreenEditorRenderPath` consumes the
accepted stable Story Flow handoff and owns the active Studio frame while a
Screen is open. Its hierarchy, selection and Inspector are editor presentation;
the central canvas is a viewport-confined instance of the exact shared
`Renegade::ScreenRenderer` used by Runtime. The renderer-independent
`BuildScreenRenderItemsInViewport` seam proves that confinement is only a
translation of the same scaled frame contract.

Wicked `wiGUI` stores widgets in insertion order but renders that storage in
reverse. The shared renderer therefore inserts the authored back-to-front widget
document in reverse so the full-screen background renders first and
title/buttons remain visible above it. This is an integration detail; authored
document order remains back-to-front and independent of Wicked's storage order.

A project descriptor may pair `startup_screen_id` with `startup_screen`.
The stable ID is reference authority; the project-relative path is a mutable
diagnostic and discovery hint. Projects without this optional pair retain the
legacy LP01/LP02 immediate startup route.

### Player foundation

Phase 6 uses one authored Player Start and one Runtime-only player. The Player
Start is an ordinary WISCENE Transform/Name/Metadata entity identified by a
Renegade-owned metadata key in addition to Wicked's broader `Player` preset.
`CreatePlayerStartCommand` owns creation and Undo/Redo; Studio exposes it through
the Renegade Add menu and existing transform workflow. The same native Metadata
stores the curated `PlayerControllerSettings`; `SetPlayerControllerSettingsCommand`
is the sole persistent editing path and Runtime consumes those authored values.

The Player Start's flat forward arrow and selected capsule are Studio-only
visualizers projected from the authored transform and settings. They are not
mesh/object entities, do not serialize, and cannot enter Runtime or packages.
The arrow deliberately uses only yaw so it remains on the ground while matching
the exact first-person spawn heading.

`PlayerService` resolves the marker deterministically and creates a native
Wicked `RigidBodyPhysicsComponent` in character mode only inside Runtime. The
live capsule is never serialized and no second Jolt world, transform authority
or player registry exists. A `PlayerInputFrame` carries named movement, look,
jump and sprint values into the service, so gameplay code does not poll raw
keyboard/mouse state and Gate 2 can add persistent bindings without replacing
player movement. Runtime owns possession and the first-person camera. Studio
continues to launch the separate Runtime process for Test Level/Preview rather
than hosting gameplay simulation in the editor render path.

### Runtime

Standalone player without editor code. It loads project settings, startup scene,
assets, input configuration, and scripts, then runs the packaged game.

### Native audio authoring and playback

Phase 6 Gate 3 keeps Wicked's `SoundComponent`, `Sound`, `SoundInstance`, four
serialized submix types and global reverb as the sole audio backend. Renegade's
`AudioService` adds only stable authoring semantics, metadata for Play On Start
and Scene Mix, command-backed mutations and Runtime activation/pause boundaries.

Studio exposes global 2D sources and movable positional 3D sources as two
explicit command-backed creation paths. A 3D source is created in front of the
editor camera, selected immediately and then uses the ordinary transform gizmo;
global sources deliberately have no TransformComponent.

The Audio authoring surface is an independent top-level Studio widget rendered
above the existing Inspector. It never hides or re-shows the Inspector
`Window`: pinned Wicked propagates `Window::SetVisible(true)` to every child,
which destroys the Inspector's established per-section visibility state.

Studio Preview owns a separate transient 2D `SoundInstance` backed by the
selected source resource. It never changes the authored source's spatial,
looping or Play On Start state and is stopped on selection, Scene, project or
workspace lifecycle changes. Authored 3D playback remains a Runtime concern.

The pinned Wicked WAV loader assumes a bounded standard `WAVEFORMATEX` chunk
and copies the declared `fmt` size directly. `ValidateAudioAssetForWicked`
therefore rejects malformed, extended-header and over-limit WAV/OGG inputs
before they reach that loader. `ApplySoundSource` creates a replacement native
resource/instance once and commits it only after validation succeeds; it does
not build a parallel audio object or mixer.

Audio-specific trigger zones are not part of Gate 3. Reusable spatial volumes
must be introduced through one shared ZoneService with common viewport
placement, transform/shape authoring and Runtime entry/exit state so audio,
weather, objectives and later systems do not grow incompatible zone models.

### Dependency extraction boundary

`DependencyCollector` owns UI-free graph admission and provider dispatch.
Providers emit both typed dependency candidates and structured non-fatal
diagnostics into temporary per-provider buffers; neither output is committed to
the graph when that provider fails. This keeps negative evidence transactional
alongside ordinary edges.

Lua dependency discovery is policy-driven rather than source-text inference.
`LuaDependencyPolicyProvider` consumes explicit typed nested-script
declarations and explicit records of computed references that cannot be
resolved statically. Declared targets become Script edges; undeclared computed
targets become `UndeclaredComputedReference` diagnostics. Extraction never
executes Lua and never treats arbitrary string literals as paths.

Gate 7 completes the graph-production boundary. The collector traverses every
newly admitted existing node once per supporting provider and terminates across
cycles without discarding the typed cycle edge. Missing nodes remain graph
evidence but are not opened or dispatched. Existing files carry an explicitly
named `fnv1a64` content hash; missing nodes use the stable `missing` marker.
The serializer validates root and edge references, then emits versioned UTF-8
JSON with independently sorted roots, nodes, edges and diagnostics. This is a
read-only interchange format for LC01; it performs no copying or cooking.

Gate 8 proves that boundary from the assembled Studio package rather than only
from the build tree. A console-only proof executable and fixed representative
mini-project ship under `Tools` and `Proof/LP05` in the evidence package. The
Windows build invokes the packaged executable twice as separate processes,
compares the graph files byte-for-byte and verifies SHA-256 records for every
fixture input before and after. Outputs are written outside the project fixture.
This is packaging of the proof harness, not asset cooking or game packaging.

### Asset identity and source tracking

`AssetRegistryService` is LC01's UI-independent consumer of the accepted LP05
graph. A canonical project path locates the current source; a UUID-v4
`assetId` is the durable identity authority. Refresh retains an existing ID
for the same canonical path, updates source hashes and provider metadata,
projects dependency edges onto asset IDs, and reports added, changed and
removed records. Missing graph nodes remain explicit unavailable records.
Runtime-support nodes are not admitted into the project asset registry.

The versioned `renegade-asset-registry` JSON format is deterministic and
validates project ownership, unique IDs and paths, complete source metadata
and referential integrity.

LC01 Gate 2 fixes the authoritative document at project-root
`AssetRegistry.renegade-assets`. `WriteAssetRegistry` serializes canonical
bytes and commits them through `ProjectDocumentTransaction` with the project
root as its containment boundary and `Intermediate/Transactions` as its
journal directory. The staged validator reparses the file, confirms project
ownership and requires both canonical and exact-requested bytes. Unchanged
writes use the transaction's validated no-op path.

`ReadAssetRegistry` rejects missing, cross-project and non-canonical documents.
An interrupted registry write leaves the normal project transaction journal;
`ProjectService::OpenProject` already recovers all such journals before it
reads or activates the project, so registry persistence does not introduce a
second recovery system.

Transaction containment compares filesystem-resolved canonical identities,
not only lexical path strings. This is required on Windows because one real
directory can appear through both an 8.3 short name and its long name; recovery
must accept those aliases without accepting a genuine escape or reparse-point
redirect outside the project root.

LC01 Gate 3 adds versioned imported-product provenance keyed entirely by
durable asset IDs. It records the importer and settings schema versions,
canonical settings JSON, and source/product hashes from the last successful
import without invoking an importer during registry refresh.

LC01 Gate 4 retains a compact tombstone when a previously available asset
disappears. Recovery compares content hash, dependency class, requirement,
applicability, provider and provider version. An ID is reclaimed only when the
candidate-to-tombstone and tombstone-to-candidate relationship is unique in
both directions; ambiguous candidates receive new IDs and leave all possible
tombstones intact. Provenance remains keyed to the retained ID, so a unique
move changes location without retargeting the import recipe. Refresh remains
metadata-only and never moves, edits or imports creator files.

LC01 Gate 5 packages a console proof beside a fixed mini-project. The Windows
build copies that project into artifact-only working state and launches the
proof as four independent processes around a controlled content update and
move. Each invocation enters Project Open and uses the production dependency,
registry and transaction services. Canonical registry evidence and immutable
fixture hashes make the process boundary and non-mutation claim auditable; this
is lifecycle proof packaging, not asset cooking or standalone-game packaging.

### Tools

Import, shader, packaging, migration, feature-inventory, and validation tools.
Native Windows build commands are logged without placing the process inside a
PowerShell output pipeline. The exit status is captured before output replay,
and CI executes a deliberate non-zero child-process probe so test failures
cannot be represented as successful build evidence.

### Frame-loop lifecycle boundary

Studio and Runtime frame updates may perform constant-time revision/state checks,
but project adoption, Scene replacement, governed-resource rehydration, LUT path
validation, whole-terrain vegetation initialization and persistent diagnostics are
not frame work.

`SceneService::Revision()` is the shared complete-Scene replacement signal.
Studio and Runtime apply authored render settings when that revision changes;
creator commands continue to apply their own committed state directly, including
Undo/Redo callbacks. A failed LUT resolution is therefore attempted once for the
current Scene revision rather than once per video frame.

Governed material texture restoration remains an explicit project/Scene adoption
operation. PR58 lifecycle timing may post one operation result to the in-memory
Wicked backlog, but normal product code does not open or append a timing file.
Gate 9 Diagnostics is the sole owner of native Wicked environment-probe debug
rendering.

WD01 vegetation initialization follows the native terrain chunk lifecycle. The
frame loop compares terrain identity and chunk count, and calls the full native
synchronization pass only when that lifecycle changes. Grass painting retains
the existing Wicked `HairParticleSystem` instances and completed-stroke rebuild
contract.

## Core rules

1. Prefer adapters over Wicked source changes.
2. Isolate unavoidable Wicked changes in a dedicated fork commit and core-patch
   ledger before changing the submodule pointer.
3. Do not change WISCENE semantics without an ADR and migration tests.
4. Commands own editor mutations that require undo/redo.
5. Creator-facing state must survive save, close, reopen, and standalone play.
6. Editor services must remain independent of the chosen UI toolkit.
7. Headless bridge commands mutate scene data but do not advance the rendered
   scene. The render-capable application loop owns `Scene::Update()`.
8. Studio-owned renderer extensions must begin and end their own valid render
   passes unless they are invoked from inside a documented active Wicked pass.
   Do not assume a Wicked virtual render hook returns with attachments bound.

## Standalone build authoring and bundled-resource boundary

Build Windows Game consumes saved project documents. Studio owns one save-first
handoff for every entry point: dirty Scene state is committed first, dirty Story
Flow state second, and only then may the synchronous build controller inspect
the project. A save cancellation or error is a build failure, never permission
to package stale disk state.

The live Story Flow `Runtime Ready` indicator and standalone preparation share
one deterministic route authority. The editor validates the in-memory document;
the builder validates the saved document. Both require a bounded route from
Game Start to Complete Game under the default Runtime state before Screen
outcome parity and packaging can succeed.

Creator project dependencies remain project-relative and fail closed when they
escape the root. Renegade-owned Runtime resources that are generated or copied
beside Studio are a separate exact-file boundary: a Scene outside-project
diagnostic may be admitted only when its canonical filesystem identity matches
a controller declaration that is also hashed and staged as Runtime support at
the declared package-root `Content/...` destination. The declaration is not a
directory allowlist. Unrelated external files remain fatal, and existing build
plan, manifest, integrity and safe-promotion services govern the staged bytes.

## UI foundation

ADR 0002 accepts Wicked's native `wiGUI` as Renegade's production integration
and rendering foundation after the Phase 2 HDR-source, DPI, input, persistence,
DX12, and Vulkan gate.

This does not select Wicked's stock Editor, widget presentation, or styling.
The accepted boundary allows Renegade widgets to inherit `wi::gui::Widget` for
canvas scheduling and input while completely overriding their rendering.
Renegade owns every visible shell primitive, information architecture, visual
language, component, docking/layout layer, project hub, and workflow.
EngineBridge remains UI-toolkit independent.

The accepted Studio visual proof is the workspace UX authority. The supplied
Renegade brand guideline is narrower: it governs only the official mark,
wordmark lockup and logo type treatment. Its palette, layouts and general
typography do not override the accepted editor direction. The official lockup
is packaged as a Studio-owned UI asset rather than reconstructed with Wicked
widgets.

Owned continuous controls use a preview/commit transaction: capture component
state at drag start, apply temporary direct preview while moving, restore the
captured state on release, then execute one before/after command. This keeps
the viewport responsive without turning every rendered frame into an Undo
entry. Workspace splitters are shell state, persisted through ProjectService
editor preferences rather than scene serialization.

The Environment workspace resolves `SceneService::WeatherEntity()` directly.
Every governed new Level WISCENE is seeded with one dedicated serialized
`Environment` entity and named directional `Sun` before its archive is written.
A legacy blank Level that has no Weather component captures the complete live
resolved atmosphere and creates the Environment/Sun pair through
`CreateEnvironmentCommand` when Environment is first opened. Existing authored
directional lighting is never replaced. Terrain creation establishes the same
command-backed precondition before it calls Wicked terrain generation. The
bridge-level `CreateTerrain` boundary rejects a blank scene so Wicked cannot
silently attach its fallback Weather component to the Terrain entity and
collapse the two workspace owners.

The dedicated Weather carrier is deliberately omitted from hierarchy rows.
For compatibility with a scene saved by the rejected Gate 5 candidate, an
entity carrying both Weather and Terrain remains visible, and Inspector
resolution filters Weather out of Terrain mode and Terrain out of Environment
mode. This is recovery behavior only; newly created Levels keep the two
components on separate entities.
Native precipitation is isolated behind `PrecipitationService`: rain maps to
Wicked's GPU rain emitter and snow is a distinct Renegade-authored visual
profile over that emitter. This preserves an upgrade path to a snow-specific
renderer without coupling Studio widgets to Wicked's raw rain fields.

Sun authoring is isolated behind `SunService`. One command updates the
serialized Weather sun direction and the primary directional-light transform
together, keeping atmosphere, illumination, shadows and Undo/Redo in lockstep.
Its deterministic editor clock is also the contract for the future Lua runtime
day/night system.

## Project metadata

ADR 0003 defines the v1 `.renegade` descriptor. It is a versioned,
project-relative identity above WISCENE. Project metadata does not replace or
alter Wicked scene serialization.

LF01 adds a Renegade-owned stable-identity layer without changing Wicked's ECS
or WISCENE format. Projects and Renegade documents use canonical UUID-v4 IDs.
Authored Wicked entities carry a persistent UUID-v4 in serialized
`MetadataComponent` data; runtime code resolves that stable ID to the current
process-local Wicked entity after load. File paths remain mutable diagnostic
hints rather than reference authority. Normal duplication regenerates IDs for
all newly created entities, while Undo/Redo restores the assigned identities.
The LF01 document-envelope writer is deliberately non-transactional; backup,
rollback, migration chains and project-wide commit remain LF02.


## Local live diagnostics (candidate)

DiagnosticService is the shared read-only evidence store for Studio and Runtime.
Dedicated StudioLiveDiagnostics/RuntimeLiveDiagnostics units copy bounded state
on the application thread; DiagnosticEndpoint only reads those copies. Local
HTTP binds exclusively to loopback, supports no commands, and never reads ECS
from a network thread. Runtime evidence is polled/cached off the render thread;
the previous shared temporary diagnostic file is no longer an authority.
The existing Diagnostics tab projects bounded summaries from these same stores.
No upstream changes, project serialization, API key or cloud service is involved.
A local coding agent must run on the machine hosting the executable: this does
not connect cloud sessions to an owner's localhost. See LIVE_DIAGNOSTIC_ACCESS.md
for schema, limits, reader and outstanding acceptance proof.


## Static model importer rebuild: FBX sources

ModelImportCandidateService owns isolated GLB/FBX conversion and FBX dependency
snapshots. Studio uses PrepareStaticModel and CommitStaticModel through the
bridge; strict GLB entry points remain for compatibility. FBX textures receive
unique preview resource keys so a previous filename cache cannot conceal changed
bytes. External texture references are limited to the source folder tree;
embedded textures require usable filenames.

ModelImportCommitService clones the candidate before relocating material paths.
FBX source bundles retain original filenames and source-relative texture layout
under SourceAssets/Models/<asset name>. Texture bytes and EditorOnly source
records join the existing governed product/projection/thumbnail/registry/metadata
transaction without extending the canonical creator recipe schema. Payload
serialization embeds resource bytes with a scoped resource-mode restoration and
uses the retained-source directory as its path base, matching placement staging.
Empty-directory cleanup never removes files or nonempty recovery directories.
This is a static Windows x64/DX12 candidate; Character/animation remains separate.


## Rigged importer preview and paused placement

The rebuild now classifies native skinned-mesh/armature candidates through the
canonical Character recipe. ModelAnimationPreviewService is a transient bridge
controller over a cloned native scene; Studio owns its native widgets and camera.
It restores source transforms for Reference Pose, evaluates only the selected
AnimationComponent and forces paused scrub evaluation through last_update_time.
ComputeVisibleModelBounds uses native CPU SkinVertex after Scene::Update to fit
deformed geometry without relying on joint helper bounds. No second animation
runtime or authored action inference is introduced.

CommitModel serializes source pose with native clips paused and a scoped
renegade.model_import.starts_paused metadata marker. Reusable placement and
Studio drag preview respect that marker, preserving legacy auto-idle behavior
for earlier assets. Rig/animation evidence populates existing managed metadata;
canonical options.asset_kind=character drives the existing Character promotion.
Preview time, speed and selection are deliberately transient. External retarget
and semantic action assignment remain a later bounded slice.


## Runtime Character collision startup

RuntimeCharacterCollision.h prepares the native collision scene once per Level
synchronization before activating authored Character controllers. Rigid-body
render geometry is admitted to native Character surface queries even when no
Navigation Grid exists. A zero-time Scene update creates the query bounds and
object matrices; previous object matrices are then seeded from current matrices.
This prevents initial world translation being interpreted as moving-platform
inertia. Normal per-frame advancement remains owned by the existing RenderPath.
The preparation does not advance gameplay time or alter the pinned Wicked source.

## External Character clips and action labels

ModelImportCandidateService appends animation FBXs through the existing native
Humanoid retarget command on a private candidate clone. Existing valid destination
mappings are reused; named armatures are auto-mapped only when required. Multiple
valid destination rigs and incomplete source conversion fail before replacing the
candidate. The source animation count guards durable numeric take indices against
partial retarget results. Preview remains a separate clone.

External source bytes are snapshotted as non-texture dependencies and retained
beneath the existing SourceAssets/Animations/Snapshots tree. The existing governed transaction registers
those files as editor-only source assets alongside the model and textures.
No external source mesh or materials are merged into the Character payload.

The canonical external_animations recipe accepts optional auto_map_source, action
and speed fields. Omitted fields retain legacy defaults and serialization shape.
New Character commits assign every native clip an explicit action, defaulting to
Unassigned. Embedded and external recipe entries reproduce those labels during
retained-source reimport. Native animation-entity metadata remains the runtime
contract; existing action variants and missing-slot semantics remain unchanged.
Studio owns one native per-clip action selector and an external FBX picker.
Preview playback speed and scrub position remain transient.

## Runtime Character crossfades

RuntimeCharacterAnimation owns transient logical weights and transition time
alongside the existing semantic action record. AI decisions and explicit native
clip metadata remain the action authority. Wicked's native AnimationComponent
evaluates translation, rotation and scale; no second skeleton evaluator or
movement controller is introduced.

The pinned Scene applies animations sequentially in component order. Runtime
sorts only its contributing clips by native component index and converts their
logical weights into cumulative amounts. The first contribution establishes a
full base pose; later contributions overlay their share of cumulative weight.
Rapid interruptions capture existing weights and preserve the phase of an
already contributing loop. Completed outgoing one-shots contribute their final
pose until their weight reaches zero, then stop.

Crossfades require matching target/path channel coverage and no event channels.
Transform coverage is completed below; events retain guarded fallback. Layered and additive
animation require a separate design. Locomotion fades take 0.20 seconds; other
actions take 0.08 seconds, Attack 0.18 seconds and Death 0.05 seconds. Runtime simulation time drives
the fade, and paused gameplay does not advance it. Native root motion is disabled
for AI-owned clips because the existing Character controller owns movement.
Hit retains priority until its final 0.08 seconds; Death is terminal. Unassigned
Idle or Death stops owned playback without substituting a bind pose.

### Runtime Character crossfades with differing transform coverage

Assigned Character clips use the union of their target/path transform tracks.
At Runtime animation initialization, missing translation, rotation or scale
tracks receive a constant native AnimationDataComponent using that target's
initial authored local transform. These additions occur only in the transient
Runtime scene; importer products and saved editor Levels are unchanged.
Coverage completion is idempotent and scoped to the Character's assigned clips.
Event or non-transform tracks retain the guarded immediate-switch fallback.
Native cumulative animation amounts still own pose blending; the native
Character controller owns movement. Diagnostics expose crossfade and fallback
counts so owner gameplay reports can distinguish a short fade from no fade.

### Continuous action tails and quiet idle

Melee attacks wait for the current attack to reach its final 0.18 seconds before
issuing the next attack. The outgoing clip continues through the transition.
Repeating a single attack preserves its outgoing timer in a transient native
animation component while the incoming instance restarts. Damage still occurs
at the existing combat event; animation-authored contact markers are deferred.
Hit and action-to-locomotion transitions also begin before the one-shot stops.

Among explicitly assigned Idle clips, Runtime selects the lowest rotational
travel per second as the base loop. Other assigned Idle clips play once after
12 seconds of quiet time, then crossfade back to the base. This is a heuristic,
not an authored base-idle setting. With only a stretch assigned, that stretch
remains the base. Motionless or unusual retargeted clips may need authoring UI
in a later change. Existing importer assignments and saved Levels are unchanged.

Dead subjects cannot be actionable memories or retain stale search continuation.
Dead players emit no new visual or footstep observations. Self-generated attack sounds cannot create search memories. Normal role decisions
resume: Patrol requires an assigned route; Guard returns to its initial post;
a character with no patrol route uses its configured fallback role.

## Player Arms & Combat architecture — Alpha Playability authority

The next programme extends the accepted Player rather than replacing it.
World-space Player movement, collision, camera ownership and lifecycle remain
authoritative. A first-person View Rig is presentation-only and may not become a
second Player controller or physics body.

After Scene::Update, Runtime samples the Player's rendered scene transform for
both camera position and the attached View Rig. Wicked may interpolate that
transform between fixed physics steps. Raw Jolt position remains the simulation
sample and is only a camera fallback when the scene transform is unavailable;
mixing raw physics camera position with an interpolated rig causes movement jitter.

The Player combat boundary is divided into four responsibilities:

- world Player/controller: movement, position, health/damage receiver and interaction;
- first-person View Rig: primary/off-hand/two-hand presentation and procedural layers;
- combat/action controller: aim solution, hand reservation and action state;
- item/ability definitions: rules, projectiles, animation, audio and VFX bindings.

Both hands are first-class. Items declare hand usage instead of assuming that the
right hand owns gameplay. Two-handed/support actions reserve or assist the other
hand through the same action model.
Combat families share one semantic action framework. Firearms, melee, bows,
crossbows and spells may use Prepare/Wind-up/Hold/Charge/Active/Release/Recovery
phases without embedding family-specific state in the Player controller.

Projectiles and damage remain world simulation. First-person recoil, muzzle
flash, hand motion, particles, camera impulse and sound are presentation/effect
layers driven from deterministic gameplay trigger points.

Directional melee is explicitly not a centre-camera raycast. During active
strike windows, relevant weapon sockets/regions are swept between frames through
the accepted Wicked physics/query authority. Attack direction, guard direction,
weapon region, reach, relative motion, surface and defensive state can therefore
participate in contact resolution.

Directional guards, shields, parries, feints, counters/chambers and stamina are
extensions of that same action/contact model. Sword + shield is the required
reference proof for independent hand ownership.

Bows use a real draw value and the shared projectile framework. Crossbows use
generic staged actions for cock/load/fire. Magic uses the same hand/action/effect
boundary without being represented as a fake weapon.
Animation remains semantic and native-Wicked-backed. The Arms Rig requests
actions such as Equip, Aim, Fire, Reload, Charge, Release, Block, Parry and Cast;
it does not request asset-specific filenames from gameplay code. Authored clips
may be layered with procedural sway, breathing, aim offsets, recoil and impact
deflection without introducing a second skeleton evaluator.

The detailed product contract, reference combat families and acceptance gates are
defined in PLAYER_ARMS_COMBAT_FRAMEWORK.md. That document is the canonical
authority for the Alpha Playability combat programme.

Combat acceptance includes direct owner play. Automated tests can prove action
state, collision, damage and persistence, but cannot prove recoil, melee timing,
audio/particle response or overall combat feel. A dedicated owner feel session
is therefore an explicit programme gate.


### P1 native semantic hand anchors

Runtime Player View Rig sockets can attach to explicitly tagged native bones or
child grip transforms inside the governed view-model hierarchy. Optional boolean
roles are documented in P1_HAND_SOCKET_BINDINGS.md; they survive WISCENE entity-ID
remapping. Duplicate/non-skeletal/cyclic anchors fail validation before mutation;
missing roles use the original independent fixed offsets. Socket bindings are
transient native Component_Attach relationships, evaluated by Wicked after native
animation. This preserves one Player controller and the accepted interpolated
camera/rig contract. The Studio Hand Grips panel uses a private EngineBridge
asset working copy and command history. Save journals the native product, managed
projection and registry together, retaining hierarchy-path bindings in the import
recipe. See P1_HAND_GRIP_EDITOR.md. There is no name-inferred hand mapping or
separate skeleton evaluator.

### Matching-rig animation and explicit FBX relinks

MatchingRigAnimationService validates one named skeleton, parent topology and
inverse binds before copying native channels/keyframe data without retargeting.
ModelImportCandidateService tries this route before the existing humanoid route.
Explicit material-slot relinks snapshot selected texture bytes and retain their
project-relative paths in the import recipe and journaled model transaction.
These are shared bridge boundaries; Studio does not implement another converter.
See P1_ASSEMBLY_IMPORT_FOUNDATION.md for real-pack evidence and creator UI limits.

### First-person assembly authoring

FirstPersonAssemblyService composes private retained native arms and weapon scenes.
An explicit hierarchy path and local transform attach the weapon to the authored
arms bone; paired native action tracks preserve both skeletons. Studio controls
call the bridge service and evaluate the preview on one clock. The assembled
WISCENE embeds resources; its versioned recipe, product, projection, registry and
catalogue cross the existing project document transaction. Player Start assignment
uses the existing settings command. Runtime paired-track action evaluation remains
outstanding. See P1_ASSEMBLY_AUTHORING.md for the contract and limitations.
