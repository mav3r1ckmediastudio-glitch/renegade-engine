# Model importer rebuild — first vertical slice

This branch starts from the verified no-creator-model-import baseline. The old
guided import UI and its transaction are not the implementation template. The
existing `.rasset` reader, identity registry, Project Assets catalogue and
placement path remain the destination contract for a newly committed model.

## First complete result

A creator can select one self-contained GLB, inspect an isolated converted
model, choose a name and destination, commit it, find its `.rasset` card in
Project Assets, place it in a scene, save, close and reopen. A failed operation
must leave no registered product or misleading READY state. A success message
must identify the actual card and stable asset ID. FBX, glTF sidecars, Character
authoring and animation controls follow only after this result works locally.

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

The branch implements static self-contained GLB import to Content/Models.
ADD > IMPORT STATIC GLB opens the native picker and a 512 by 320 rendered
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
No release gate is marked complete. Scope remains Windows x64/DX12 static GLB;
FBX, sidecars, rigs, animation and destination selection remain later work.