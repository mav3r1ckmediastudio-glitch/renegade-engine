# Story Flow Gate 7 — Project-home lifecycle

Status: implementation contract

Gate 7 completes the project-home transition begun by Gate 5. A project created by
Renegade Studio is Story Flow-native from its first committed descriptor. It does
not need a placeholder Scene merely to satisfy project construction.

## Acceptance contract

A newly created project must have all of the following before Studio adopts it:

- a valid stable project ID;
- a canonical `Content/StoryFlow/Main.renegade-flow` document owned by that
  project;
- exactly one permanent Game Start node in that Flow;
- a descriptor whose paired `startup_flow_id` and `startup_flow` reference that
  document;
- no `startup_scene` value and no generated `Content/Scenes/Main.wiscene`;
- an empty governed asset registry and the standard project folder structure.

After adoption, the Project Hub closes and Story Flow becomes the project home.
The neutral in-memory Scene document is cleared so content from the previously
active project cannot remain visible or become associated with the new project.

## Descriptor invariant

Project format version 1 remains backward compatible. A valid descriptor must
declare at least one launch root:

1. a safe project-relative `startup_scene`; or
2. a complete, valid stable `startup_flow_id` plus safe project-relative
   `startup_flow` pair.

An empty startup Scene is therefore valid only for a Story Flow-native project.
An incomplete Flow pair, a malformed stable ID, an unsafe hint, or a descriptor
with neither launch root is rejected.

## Adoption safety

Opening and creating projects remain staged Studio operations. For a Story
Flow-native candidate, Studio must resolve and read the configured startup Flow
before changing active project identity or Recent Projects. If validation fails,
the pending candidate is discarded and the previous project and Scene remain
authoritative.

Scene-first candidates keep the existing prepare-Scene-then-commit boundary.
Story Flow-native candidates use a validate-Flow-then-commit boundary and clear
the old Scene only after project adoption succeeds.

## Compatibility and migration

- Existing scene-first projects continue to open unchanged.
- `StoryFlowProjectHomeService::Ensure` remains the governed legacy migration
  seam. It creates the canonical Flow, adopts the existing startup Scene as the
  first Level, and routes Game Start to that Level.
- Existing projects that already declare a valid startup Flow remain idempotent;
  their Flow is not recreated or rewritten.
- Runtime accepts a Story Flow-native descriptor without a startup Scene. It
  begins at Game Start and loads a Scene only if Flow traversal reaches a Level.
- Dependency extraction roots the startup Flow and does not invent a Scene
  dependency when `startup_scene` is empty.

## Exclusions

Gate 7 does not implement the Screen Editor (Gate 8), Journey View interaction
and feedback polish (Gate 9), or standalone programme closeout (Gate 10).

## Required evidence

- focused service tests for native creation and descriptor invariants;
- staged Studio lifecycle tests proving failed candidates preserve authority;
- Runtime bootstrap/Flow tests for a Flow-only project;
- dependency closure evidence showing no placeholder Scene root;
- legacy migration regression coverage;
- Windows Release owner validation from the packaged CI artifact.
