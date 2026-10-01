# Model importer rebuild — first vertical slice

This branch starts from the verified no-creator-model-import baseline. The old
guided import UI and its transaction are not the implementation template. The
existing `.rasset` reader, identity registry, Project Assets catalogue and
placement path remain the destination contract for a newly committed model.

## First complete result

A creator can select a self-contained GLB or static FBX, inspect an isolated converted
model, choose a name and destination, commit it, find its `.rasset` card in
Project Assets, place it in a scene, save, close and reopen. A failed operation
must leave no registered product or misleading READY state. A success message
must identify the actual card and stable asset ID. The first static slice is retained below as historical evidence. The current
Character slice adds embedded native clip preview and paused reusable placement.
External clip retargeting and action assignment remain separate work.

## Boundaries

1. Convert into a separate scene. Never merge into the authored scene during
   selection or preview. Report real mesh/object evidence and converter errors.
2. Before commit, verify the selected source and every declared dependency is
   still available and unchanged. A `.glb` extension alone does not prove that
   referenced images are embedded; reject an external URI until the retention
   path supports it.
3. Commit the retained source, `.rasset` payload, managed projection and stable
   registry identity as one governed project transaction. Reopen the exact
   serialized product and verify it through the existing placement loader.
4. Studio owns native input controls and presentation; bridge services own
   conversion, validation and persistence. Every visible button and field must
   be backed by a real input/action and tested by clicking the built executable.
5. No animation transport, Character classification or rig claim is displayed
   until those operations have their own native behaviour and persistence proof.

## Acceptance sequence

- Local Windows Release build and focused conversion/transaction tests.
- Real GLB selection, failure reporting and a populated preview in the built
  Studio, with no authored-scene mutation before commit.
- Committed asset card, placement Undo/Redo, scene Save/Reopen and project
  reopen using the same stable asset ID.
- Failed/missing/external-dependency source leaves no partial project asset.
- Owner inspects the exact build before any release/merge claim.

The branch implements static self-contained GLB and FBX import to Content/Models.
ADD > IMPORT MODEL opens the native picker and a 512 by 320 rendered
preview, editable name, Rotate Left/Right, Import Asset and Cancel controls.
The preview owns a cloned scene, auto-framed camera and neutral lighting;
rotation does not alter the candidate or authored scene. The image remains
untinted by the global GUI theme and freezes once ready until rotation.
Import stays disabled while the preview warms up.

Studio captures the selected view as a PNG. The bridge validates its format,
size and decode, and writes the thumbnail with retained source, .rasset,
managed projection, registry and metadata in the governed transaction.
The existing Asset Browser consumes the sibling .thumbnail.png. Headless
callers may omit a thumbnail; old assets are not automatically regenerated.

Windows VS18 Release Studio and graphics proof builds pass. CTest's focused
RenegadeModelImporterRebuildGraphicsProof passes (1/1). Direct triangle and
Bow 05 runs pass nonblank rendered pixels, changed pixels after rotation,
candidate isolation, malformed thumbnail refusal, persisted PNG decode,
external URI refusal, duplicate refusal, stable-ID placement, Undo/Redo and
WISCENE save/reopen. Generated triangle and Bow PNGs were visually inspected.
The corrected native Bow preview was visually inspected in the running Studio.

The owner confirmed the repaired native card drag/drop, corrected preview and
both rotation buttons. A different recovery conversation independently verified
native cancel, rotation, naming, thumbnail commit/card, drag placement, Undo/Redo,
scene save and a fresh-process project reopen on 1 October. Exact implementation
fd8b247 and executable hash plus evidence are recorded in HANDOFF.md.
No release gate is marked complete. Scope remains Windows x64/DX12 static models;
sidecars for glTF, rigs, animation and destination selection remain later work.

## Static FBX texture retention - 1 October 2026

PrepareStaticModel supports FBX through the pinned Wicked converter. Dependency
inspection snapshots embedded images and external images within the FBX source
folder tree before conversion. Every material texture must match an inspected
dependency and decode from its exact snapshot under a unique preview cache key.
Missing, changed, unreadable, unnamed or ambiguous dependencies fail explicitly;
external textures outside the source folder tree require a relative-path export.

CommitStaticModel retains the FBX under SourceAssets/Models/<asset name> with
its original filename and external texture directory layout. Embedded images are
also retained as named files. Texture source records, source FBX, product,
projection, optional thumbnail, registry and metadata use one governed transaction.
The canonical creator recipe schema stays unchanged. A cloned scene relocates
texture identities without consuming the retryable preview candidate.

The serialized payload embeds resource bytes and uses paths relative to the
retained source bundle, matching the existing placement loader. Serialization
temporarily enables embedding and restores the previous resource mode. GLB
compatibility entry points remain available and the existing GLB proof passes.

All five Release CTest cases pass: GLB; external-texture FBX; embedded-texture
FBX; and a fresh-process reopen for each FBX after deleting the disposable
original sources. Coverage includes texture byte retention, missing/changed
texture refusal, rotation, thumbnail capture, placement, Undo/Redo, save/reopen
and reconversion from retained sources. Fixtures are generated with Blender
5.1.1; provenance and reproduction are in Tests/fixtures/Importer/FBX_FIXTURES.md.

Native external-FBX preview, rotation, naming, thumbnail card, drag placement
and Undo/Redo were visually checked in the dedicated Studio build. Saved-scene
and project reopening evidence is recorded in HANDOFF.md. These are local
candidate checks; owner acceptance of FBX and independent exact-commit review
remain outstanding. No Character, animation or standalone gameplay claim.


## Rigged Character and embedded clip preview - 1 October 2026

PrepareModel retains isolated pinned-native GLB/FBX conversion. A candidate with
skinned meshes and armature bones is committed as a Character using the existing
canonical options.asset_kind recipe. Bones, inverse bind matrices, weights,
texture bytes and native embedded animation/data components stay in the WISCENE
payload. Static compatibility entry points continue to refuse Character requests.

ModelAnimationPreviewService owns transient playback on a private scene clone.
Reference Pose restores saved transforms; selecting a clip evaluates only that
clip. Native Play/Pause, Restart, time in seconds and speed 0.1-4 operate on the
real AnimationComponent. Paused scrubbing explicitly invalidates native
last_update_time so the new pose evaluates. Root motion is disabled in preview.
Preview controls and camera changes do not become creator recipe or AI actions.
The camera fits deformed vertex positions rather than armature joint helper AABBs.

CommitModel resets the cloned payload to paused source pose and marks new
Character imports with renegade.model_import.starts_paused. The existing
placement and drag-preview paths respect that marker, including single-clip
characters. Existing assets without it retain their previous behavior. The marker
is native serialized metadata, not a second runtime animation system.

The generated animated_character.fbx fixture has two bones, two one-second
clips and an embedded checker texture. It is generated with Blender 5.1.1 using
FBX_SCALE_ALL (FBX Units Scale). Default per-node unit scaling exposed a pinned
native conversion mismatch: rendered skin was 100 times the static mesh extent.
That exporter combination is not claimed supported; no skin weights, inverse
bind matrices or Wicked sources were patched to conceal it. Inspect source scale
and poses in preview before importing unfamiliar FBXs. This fixture and the
owner Mutant asset are the bounded compatibility evidence, not universal FBX parity.

Seven focused graphics cases cover static GLB, external/embedded FBX plus their
cold reopen, and animated Character plus cold reopen. Character checks include
visible changed scrub pose, time advance/freeze, speed, clip switch, candidate
isolation, strict static refusal, real Character placement, skin/bind fingerprints,
Undo/Redo and paused save/reopen after deleting original sources. Thumbnail
coverage rejects an image filled by oversized geometry. The proof also writes a
separate disposable Runtime Level with a Player Start and floor.

This slice does not add external animation files, retarget mapping, Idle/Walk/Run/
Attack action slots, custom actions, destination selection or reimport editing.
Previewing an embedded reference-pose take does not prove locomotion or AI.
Native Studio and standalone-player acceptance evidence belongs in HANDOFF.md;
independent exact-commit verification and owner acceptance remain required.


### Grounding recovery validation - 1 October 2026

The generated rigged checker cube and retained owner Mutant both remain grounded
at (4,5,6) for 300 native updates after reopening. The proof invokes the same
RuntimeCharacterCollision startup helper as the standalone player and requires
native ground contact plus all three position coordinates within 0.01 metres.
The unseeded startup reproduced a launch to (1202,1365.47,1803), despite a valid
floor ray hit. Runtime now prepares rigid-body query geometry and initializes
previous object matrices before activating Character controllers.

Release Runtime/proof builds and all seven importer CTest cases passed. Native
standalone inspection showed the checker cube on the floor and Mutant visible
in reference pose after approximately 20 seconds. No gameplay animation action
assignment or locomotion is claimed. Exact commands and evidence are in HANDOFF.

## External-animation continuation slice

The native Character importer now offers ADD ANIMATION FBX and a Gameplay action
selector for the selected clip. Choose Unassigned, Idle, Walk, Run, Attack, Reload,
Hit or Death. Several clips may share an action; missing actions remain valid.
Reference pose is a preview mode and cannot acquire an action. Imported source
takes default to Unassigned until the creator assigns one.

External conversion reuses the pinned native humanoid retargeter on an isolated
clone. It requires one usable destination humanoid and a complete source result.
Failure preserves the prior candidate. Successful addition refreshes the preview
and clip list; Import remains disabled until the new preview is ready.

Commit retains exact external FBX bytes in SourceAssets/Animations/Snapshots, registers them
as editor-only source dependencies, and persists explicit native action metadata
plus durable source/take recipe entries in the existing transaction.
Reimport uses the retained files with optional source auto-mapping.
Preview time and speed remain presentation controls.

This is bounded new-import authoring. Custom slots, frame-range authoring,
editing an existing Character, arbitrary multi-rig retargeting and universal
FBX compatibility remain outside this slice. Exact validation is recorded in
HANDOFF.md; independent verification and owner acceptance remain pending.
