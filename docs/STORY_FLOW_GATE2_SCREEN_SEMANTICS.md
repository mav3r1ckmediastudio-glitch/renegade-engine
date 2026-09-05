# Story Flow Gate 2 — First-Class Screen Semantics

**Status:** implementation candidate; exact-head CI and owner/runtime acceptance pending.

**Base:** `8806f6f6c7f4742c91dcffab952e5c74c198956a`

## Purpose

Gate 2 extends the existing LP02 Story Flow contract rather than creating a second editor-only graph format. A Runtime Screen is now a first-class Flow destination with stable document identity and a project-relative path hint.

The bounded target proof is:

`Game Start -> Title Screen -> Level One -> Victory Screen -> Complete Game`

The Title Screen emits the authored outcome `new_game`; the Victory Screen emits the authored outcome `continue`. Both outcomes are consumed by the same deterministic Flow route-selection machinery already used by Level outcomes.

## Contract changes

- `FlowNodeKind::Screen` is serialized as `screen`.
- Screen nodes carry `screen_document_id` plus `screen_path_hint`.
- stable Screen document ID is authoritative; path remains a repairable hint.
- Screen nodes may emit named outcomes and have ordinary conditional/priority routes.
- only Level destinations may carry `destination_entry`; Screen destinations do not overload Player Entry semantics.
- Screen and Level references are mutually exclusive on a node.
- terminal-node and Game Start invariants are unchanged.

## Runtime integration

When Story Flow enters a Screen node, Runtime resolves the governed `runtime-screen` document by stable ID, validates it, and exposes it as the active Screen destination. Runtime Screen button/action IDs feed directly back into `FlowInterpreter::EmitOutcome`.

The existing LP03 project-level startup Screen remains supported as a compatibility path. Gate 2 does not remove or redefine that lifecycle; it adds Screen destinations inside Story Flow.

Runtime Screen validation is generalized from the old LP03 proof-specific `play`/`quit` shape to a bounded authored action set. Existing LP03 documents remain valid.

## Dependency/build integration

Story Flow dependency discovery now emits:

- Level path hints as `DependencyClass::Scene`;
- Screen path hints as `DependencyClass::RuntimeScreenDocument`.

This allows the existing Runtime Screen dependency provider to discover each Screen's declared resources transitively.

## Shared Studio model

The presentation-independent Story Flow authoring model carries Screen identity/path fields, and the temporary Gate 1 proof workspace labels Screen nodes as `SCREEN`. This is semantic support only; it is not Journey View polish.

The dedicated Story Flow render path remains a Gate 3 deliverable. Gate 2 does not make the temporary overlay architecture permanent.

## Automated proof

`RenegadeStoryFlowScreenSemanticsTests` proves:

- arbitrary valid Screen action IDs are accepted;
- Screen references round-trip through Flow serialization;
- Story Flow dependency reading exposes Level and Screen documents separately;
- dependency provider types Screen documents as `RuntimeScreenDocument`;
- `Game Start -> Title Screen -> Level One -> Victory Screen -> Complete Game` traverses the shared interpreter;
- Runtime Screen controller emits `new_game` and `continue` from real governed Screen documents;
- Runtime resolves a moved Title Screen from a stale path hint by stable document ID;
- malformed/missing Screen identity and mixed Level/Screen references fail closed.

## Explicit exclusions

No Screen creation lifecycle, templates, Screen Editor, Journey View polish, Graph editing, New Project lifecycle change, semantic Flow mutation UI, or dedicated Story Flow render path is introduced by this gate.
