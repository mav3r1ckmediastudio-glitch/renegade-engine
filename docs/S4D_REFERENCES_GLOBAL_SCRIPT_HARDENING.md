# S4D — References, GLOBAL SCRIPT & S4 Hardening

## Purpose

S4D closes the S4 creator-authoring programme on top of the accepted S2 document model, S3 governed Runtime, S4A metadata contract, S4B attachment Inspector and S4C typed properties.

It adds the four reference-backed property pickers and Level-wide `GLOBAL SCRIPT` authoring without creating a second scripting state, execution path or identity system.

## Reference authority

Reference-backed metadata remains stored in the existing S2 `ScriptPropertyValue` fields (`referenceId` plus optional `pathHint`). Studio never exposes raw Wicked ECS IDs.

- **EntityReference** — persistent Renegade Scene entity ID. The picker shows creator-visible named Scene entities.
- **Animation** — persistent Renegade ID of the named Wicked animation Scene entity. This preserves clip-level identity without inventing a parallel animation-ID namespace.
- **AssetReference** — stable LC01 asset-registry ID with project-relative path hint.
- **Audio** — stable LC01 asset-registry ID filtered to audio-classified project assets, with project-relative path hint.

The visible picker label is human-readable. Stable IDs remain document/runtime data and are not shown as creator-facing labels.

## Unresolved references

A valid persisted reference is not deleted merely because its target is temporarily missing. Studio presents it as an unresolved value so the creator can repair it or explicitly choose `(None)`.

An unresolved option cannot be assigned as a new value. New assignments must resolve to a current Scene entity/animation or current asset-registry record.

All assignments and clears use the existing S2 `MakeSetScriptPropertyCommand` through the shared Studio `CommandService`, preserving normal Undo/Redo and dirty-state semantics.

## GLOBAL SCRIPT

`GLOBAL SCRIPT` uses the already-reserved S2 **Level** scope. It has no entity owner and therefore does not depend on the current entity selection.

Studio exposes a collapsed-by-default `GLOBAL SCRIPT` Inspector section for a saved active Level. It provides the same governed creator operations as entity attachments:

- discover S4A-valid `GLOBAL SCRIPT` Lua sources from `Content/Scripts`;
- attach multiple instances;
- enable/disable;
- deterministic Level execution ordering;
- remove;
- typed properties through the S4C property editor;
- reference pickers through S4D;
- shared Undo/Redo and dirty state;
- `.wiscene.rscripts` save/reopen persistence.

The S3 governed Runtime already understands Level-scope attachments, so S4D does not create a second Runtime execution mechanism.

## Boundaries retained

S4D does **not** introduce:

- Wicked `ScriptComponent` as creator scripting authority;
- Wicked's global Lua VM as the governed Runtime;
- raw native/ECS entity IDs;
- Game-scope script authoring;
- gameplay APIs/events/diagnostics (S5);
- installed-library adoption/package-closure work (S6).

## Acceptance

Before merge, S4D requires:

1. headless reference-authoring tests;
2. headless GLOBAL SCRIPT attach/order/Undo/Redo/save/reopen tests;
3. S4D source-contract and diff-hygiene preflight;
4. both Renegade Studio Windows x64 jobs green;
5. both Windows baseline jobs green;
6. creator owner test covering all four reference picker classes and GLOBAL SCRIPT authoring/persistence.
