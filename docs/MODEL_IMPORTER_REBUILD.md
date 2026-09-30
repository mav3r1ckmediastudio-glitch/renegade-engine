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

The branch contains isolated GLB conversion and a governed static-model
commit/reopen service. The Windows VS18 Release bridge and proof executable
build. The focused graphics proof passed (exit 0) on a disposable project:
static GLB conversion, external URI rejection, governed commit, asset reopen,
current catalogue and stable-ID placement preparation, plus duplicate refusal.
No Studio button is exposed. Native UI, scene Save/Reopen, project reopen and
owner acceptance remain to be verified.
