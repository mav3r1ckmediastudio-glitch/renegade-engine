# Story Flow Gate 4A — Governed Level Creation Transaction

**Status:** implementation candidate; stacked on the integrated Gate 3 exact head. Full Gate 4 CI is deferred until the 4A/4B/4C stack is consolidated.

## Purpose

Gate 4A establishes the persistence-safe backend for `Add New Level`. A Level is not just a Graph card: it is a governed Wicked WISCENE, a Renegade Scene identity sidecar and a stable Story Flow reference that must appear together or not at all.

This slice deliberately contains no new creator-facing button yet. Gate 4B binds this backend to Story Flow `Add New Level` / `Add Existing Level`; Gate 4C owns Story Flow -> Level Editor -> Story Flow workspace transitions.

## Atomic creation contract

`StoryFlowLevelLifecycleService::CreateNewLevel()` receives the active project, the authoritative current Flow snapshot, the Flow document path, the creator-visible Level name and a project-relative `Content/Scenes/*.wiscene` destination.

It prepares four transaction payloads:

1. a blank valid Wicked WISCENE;
2. `<scene>.wiscene.rmeta` containing a `renegade-document` envelope of type `scene` with a new stable Scene document ID;
3. the updated Story Flow containing a new `Level` node whose `sceneAssetId` is that stable Scene ID and whose path remains only the project-relative hint;
4. the previous Story Flow bytes at the existing permanent `.bak` location.

All final writes are committed through the existing LF02 `ProjectDocumentTransaction` with the project root as its containment boundary and `Intermediate/Transactions` as the durable journal directory.

The WISCENE, sidecar and Flow staged payloads are semantically validated before any live destination is replaced. A later replacement failure rolls earlier replacements back. Interruptions retain the normal LF02 recovery journal rather than inventing a second recovery mechanism.

## Important authoring boundary

`Add New Level` is intentionally a Story Flow persistence boundary. The Flow snapshot supplied by the caller is the complete authoritative editor candidate and is committed in the same transaction as the new Scene. This prevents a persistent Scene/identity pair from being created while its Flow relationship exists only in volatile memory.

Gate 4B must make this behaviour clear when it binds the operation to the native Story Flow UI and must resynchronize the authoring session from the committed Flow after success.

## Stable identity

The new Level receives two independent stable IDs:

- the Story Flow node ID;
- the Scene document ID stored in `.wiscene.rmeta` and referenced by the Flow node.

The Scene path is never identity. `ResolveSceneDocumentPath()` remains the authoritative resolution seam and can later repair a stale path hint after a move; moved/missing diagnostic UX is Gate 4B.

## Safety rules

- the project ID must be valid;
- the Flow snapshot must validate against the active project;
- the on-disk Flow must be the same stable Flow document as the supplied snapshot;
- new Scene destinations must remain under `Content/Scenes/`;
- project escape paths are rejected;
- `.wiscene` is required;
- an existing Scene or sidecar is never overwritten by `Add New Level`;
- staged WISCENE bytes are parsed without relying on the temporary stage-file extension;
- staged Flow and sidecar bytes must both round-trip with the expected stable identities before commit.

## Automated proof

`RenegadeStoryFlowGate4LevelLifecycleTests` proves:

- successful atomic creation of WISCENE + `.rmeta` + Flow reference;
- valid new stable Story Flow node and Scene document identities;
- the previous Flow is preserved at `.bak`;
- the created WISCENE can be opened through Wicked;
- the Scene sidecar carries the expected project/type/path authority fields;
- the committed Flow retains the exact new Level-to-Scene relationship;
- stable Scene resolution succeeds after creation;
- a forced later LF02 replacement failure rolls the Flow back and leaves no half-created Scene/sidecar;
- project-escaping Scene paths are rejected;
- occupied Scene destinations are rejected.

## Explicit exclusions

This slice does not implement:

- Story Flow `Add New Level` UI;
- Add Existing Level/adoption;
- moved/missing Level diagnostics presentation;
- double-click/open Level;
- Return to Story Flow;
- Level Editor lifecycle state restoration;
- Journey View cards.

Those remain Gate 4B, Gate 4C and Gate 6 respectively.
