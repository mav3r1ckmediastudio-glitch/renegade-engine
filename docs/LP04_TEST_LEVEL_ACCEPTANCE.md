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
- STOP terminates the running Runtime and returns Studio to PLAY;
- Studio's own scene, selection and dirty state are unaffected by a Test
  Level session, both while it runs and after STOP.

The transport control reuses the chrome's existing top-bar glyph area
(previously drawn but unwired) rather than adding a separate button
elsewhere in the UI, matching where a creator would already expect
play/stop controls to live.

### Fixes found and applied during Gate 3B3 verification

Three real defects were found only once the project owner exercised the
actual click-through in a running Studio build - none were caught by
compilation, the 22/22 automated suite, or a startup-only smoke test, since
nothing in the existing automated coverage launches Studio and clicks its
UI. Each was diagnosed against the real, running application before being
fixed and re-verified.

**STOP was unreachable.** `StudioRenderPath::Update()`'s branch for an
active Test Level session returned unconditionally every frame before
reaching the code that dispatches queued editor actions further down.
Clicking STOP correctly queued the action, but nothing ever processed it
while Runtime was active, so the click had no visible effect. Fixed by
letting a queued `StopTestLevel` action through to dispatch before that
branch's early return; any other action requested during an active session
is discarded rather than left to fire unexpectedly once Runtime exits.

**A stale Runtime binary produced a false "stuck on STARTING" symptom.**
Early verification repeatedly showed Runtime launching and rendering
correctly while Studio never left the STARTING state. The Runtime
executable being launched had, at that point, last been built roughly
20 hours before the READY-event feature was committed, so it had no
readiness-signalling code at all - it ran normally and simply never
signalled anything. Not a logic bug; resolved by rebuilding Runtime.

**`ResolveTestLevelRuntimePath()` depended on `fs::current_path()`.**
After the above was resolved, Studio still failed with "RenegadeRuntime.exe
was not found beside this Studio build" against a Runtime binary that
genuinely existed. Windows' common file-open dialogs (Open Project, Open
Scene) are documented to change a process's working directory as a side
effect of browsing to a file; once that happens, every relative candidate
this lookup built silently resolved against the wrong root. Fixed by
anchoring to Studio's own executable path via `GetModuleFileNameW`
instead, which cannot drift regardless of what dialogs have been shown
during the session.

Verified after each fix: `RenegadeStudio.exe` builds clean in both Debug
and Release, the full `ctest -C Debug` suite remains 22/22, and the actual
PLAY -> STARTING -> RUNNING -> STOP interaction was re-confirmed working
in a real, running Studio build by the project owner directly.

### Known, deferred, non-blocking

- The transport control's visual state (STARTING -> RUNNING, and the
  STOP -> PLAY reversion after termination) has been observed to lag
  behind the real underlying state while Runtime's window holds input
  focus, catching up once focus returns to Studio. Most likely explained
  by Studio's own render/update loop being throttled while its window is
  unfocused, which is normal Windows/driver behaviour for background
  real-time applications - not yet confirmed, and not blocking, since the
  underlying state and every functional transition are correct throughout.
- The real Runtime READY handshake was observed to succeed on a second
  attempt after an earlier attempt in the same session appeared to hang
  on STARTING, with no code change between attempts. Not reproduced since,
  and not root-caused. Worth revisiting if it recurs.

