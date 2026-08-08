# LP04 — Unsaved Test Level Snapshot Acceptance

## Result

**Status: PASS**

**Date:** 2026-08-08

**Merged pull request:** PR #21 — `Add LP04 unsaved Test Level snapshot gate`

**Merged implementation head:** `f35ffef588f01a928ba833aea7262d70a67ee1b8`

**Main merge commit:** `fbf572e01bff04106b081c72e2ea14ec5fc22bb3`

LP04 proves that Renegade Studio can launch the real standalone Runtime from
the editor's current live, unsaved scene state without first overwriting the
authoritative scene document.

## Accepted end-to-end behaviour

The project owner manually verified the completed Studio workflow on a real
Windows GPU:

- Studio remained open with the authoring scene visible.
- A new GLTF/GLB model (crate) was imported and placed in the live scene.
- The imported crate remained an unsaved editor change.
- PLAY created the disposable Test Level snapshot and launched the real
  `RenegadeRuntime.exe`.
- Runtime reached the explicit READY handshake and Studio entered the RUNNING
  state.
- Runtime displayed the current unsaved scene content, including the newly
  imported crate.
- The crate was initially mistaken for missing because a bright glowing sphere
  visually obscured it from the Runtime camera angle; subsequent inspection
  confirmed the crate was present in the Runtime scene.
- No ordinary Studio scene save was required to make the crate appear in
  Runtime.
- Clicking STOP terminated the Runtime window/process correctly.
- Studio remained open and usable after STOP.

This is the required LP04 acceptance: the Test Level launch consumes a
disposable snapshot of the current live editor scene rather than requiring the
authoritative WISCENE to be saved first.

## Gate 3A — process lifecycle

The process-lifecycle gate established:

- real Win32 process launch and observation;
- explicit READY-event ownership;
- startup timeout handling;
- Runtime bootstrap failure observation;
- STOP/termination handling;
- snapshot cleanup on normal and failure paths;
- synthetic fixture coverage for deterministic automated lifecycle testing.

## Gate 3B1 — Runtime readiness

Runtime accepts the Renegade-owned `--renegade-ready-event=<name>` argument and
signals it only after `application.StartupFinished()` reports successful
startup.

Runtime startup failures in Test Level mode are surfaced through the existing
20–29 bootstrap exit-code range rather than blocking Studio behind an
additional Renegade startup dialog.

## Gate 3B2 — real Runtime handshake

The real Runtime handshake was executed twice on the project owner's Windows
machine using a real GPU and real DX12 device.

Both runs:

- reached `Running`;
- stopped successfully;
- removed the Test Level snapshot session directory;
- left no lingering Runtime process;
- left no stray Runtime window.

The full Debug CTest suite remained 22/22.

The dedicated GitHub-hosted probe does not constitute a Renegade failure. The
pinned Wicked DX12 adapter-selection path deliberately skips adapters carrying
`DXGI_ADAPTER_FLAG_SOFTWARE` ("Don't select the Basic Render Driver adapter").
On a hosted Windows runner with no accepted hardware adapter, Wicked reaches
its no-capable-adapter failure path and calls `wilog_messagebox`, whose Windows
implementation invokes a synchronous Win32 `MessageBox`. With nobody present
to dismiss that dialog, Runtime never reaches `StartupFinished()`, READY is
never signalled, and the Test Level watcher correctly reaches its startup
timeout and terminates the process.

The probe is therefore retained as a manual `workflow_dispatch` diagnostic,
not a required CI check. Wicked source is intentionally unchanged.

See `docs/LP04_GATE3B2_RUNTIME_HANDSHAKE_EVIDENCE.md` for the detailed Gate 3B2
record.

## Gate 3B3 — Studio PLAY/STOP wiring

Studio now connects the proven snapshot and process primitives to the Renegade
chrome:

- PLAY snapshots the current live scene;
- the real Runtime launches using the same graphics backend as Studio;
- STARTING remains distinct from RUNNING;
