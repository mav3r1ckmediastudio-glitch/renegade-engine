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
must identify the actual card and stable asset ID. glTF sidecars, Character
authoring and animation controls follow only after the static result works locally.

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
ADD > IMPORT STATIC MODEL opens the native picker and a 512 by 320 rendered
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
