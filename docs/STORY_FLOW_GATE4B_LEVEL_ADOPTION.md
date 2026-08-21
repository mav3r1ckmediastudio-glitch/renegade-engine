# Story Flow Gate 4B — Existing Level Adoption and Stable Identity

**Status:** stacked implementation for Gate 4 review.

Gate 4B extends the Gate 4A atomic New Level boundary with governed adoption and stable-ID resolution for existing WISCENE content.

## Delivered

- `StoryFlowLevelReferenceService::AddExistingLevel()` accepts only WISCENE files contained by the active project.
- The WISCENE must deserialize successfully before it can enter Story Flow.
- Existing `.wiscene.rmeta` identity is preserved when valid.
- An ungoverned project-local WISCENE receives a new Scene document envelope and stable Scene ID.
- A sidecar owned by another project or carrying another document type is rejected fail-closed.
- The Flow Level node stores the Scene document ID as authority and the project-relative path as a hint.
- Duplicate Story Flow references to the same governed Scene identity are rejected.
- Flow update + Scene metadata creation/repair use `ProjectDocumentTransaction`; a failed commit cannot leave a newly governed sidecar without its Flow reference.
- `ResolveLevel()` uses `ResolveSceneDocumentPath()` so a moved Scene can still resolve by stable identity and reports that the stored hint is stale.
- Missing/mismatched references surface structured diagnostics instead of falling back to an arbitrary WISCENE.

## Proof

`RenegadeStoryFlowGate4BLevelReferenceTests` covers:

- adoption of an ungoverned existing WISCENE;
- stable Scene identity creation;
- persisted Level reference;
- stable-ID resolution;
- move/rename recovery after the Scene and sidecar move together;
- duplicate stable identity rejection;
- missing Scene fail-closed behaviour.

## Gate boundary

Gate 4B is backend/content-lifecycle work. Gate 4C binds Add New / Add Existing / Open Level to the native Story Flow workspace and implements the explicit Story Flow -> Level Editor -> Story Flow transition while preserving the shared authoring session.
