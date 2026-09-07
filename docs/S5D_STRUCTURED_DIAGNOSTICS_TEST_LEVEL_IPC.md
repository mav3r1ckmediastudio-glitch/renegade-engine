# S5D — structured diagnostics and Studio/Test Level IPC closure

## Status

S5D is the closure gate for S5. Its scope is deliberately narrow: accept the
structured diagnostic evidence path and the supervised Studio -> Test Level
Runtime handshake that S5A-S5C now depend on for owner-visible proof.

S5D does **not** add another Lua state, gameplay API family, event bus, network
transport or remote-control surface. S5A remains the entity/transform API, S5B
the governed gameplay lifecycle/input projection, and S5C the bounded
cross-script event path.

The functional substrate already exists on `main` after the live diagnostics
integration. S5D hardens that substrate as an explicit gate and adds a source
contract so later work cannot silently weaken the handshake or structured
evidence while still compiling green.

## Structured diagnostics contract

Renegade keeps one read-only local diagnostic endpoint per process:

- Studio: `127.0.0.1:38741`
- Runtime: `127.0.0.1:38742`

`DiagnosticService` is the copied-state owner. Endpoint threads never read ECS
or widgets and expose no project mutation or command surface. The snapshot
schema remains `renegade.diagnostics.v2`.

Studio must publish a `test_level` group containing at least:

- `active`
- `ready`
- `state_code`
- `message`
- `warning`
- `child_pid`

Runtime must publish bounded live state containing at least project/scene,
startup outcome, player presence, governed-script health and authored-audio
synchronization. Studio's diagnostic drawer may cache the Runtime peer summary,
but peer unavailability is never interpreted as successful Runtime shutdown.

The child PID is the machine-readable association seam: a local reader can
compare Studio `test_level.child_pid` with Runtime `process.pid` and reject a
standalone or stale Runtime as evidence for the active Test Level.

## Studio -> Test Level handshake contract

`TestLevelRuntimeProcess` remains the single Studio owner of the child process.
Every launch creates a unique local Windows ready-event name tied to the Studio
process identity and a monotonically increasing launch sequence. The child is
created suspended, optionally attached to a kill-on-close Job Object, resumed,
and then polled without blocking the editor.

Studio promotes the child from `Starting` to `Running` only when that exact
ready event is signalled. Runtime receives the event name through
`--renegade-ready-event=<name>` and signals it only after
`RuntimeApplication::StartupFinished()` reports successful startup. Runtime
bootstrap failures therefore cannot masquerade as readiness.

The ready event is a readiness barrier, not a general command channel. S5D does
not add bidirectional editor control, arbitrary localhost writes or an embedded
AI/API dependency.

## Failure and stale-session contract

The supervision path must stay fail-closed:

- missing or malformed Runtime executable -> `LaunchFailed`;
- Runtime bootstrap exit in the governed failure range ->
  `RuntimeReportedFailure`;
- child exit before readiness -> `AbnormalExit`;
- no readiness before the configured deadline -> `StartupTimedOut` and child
  termination;
- watcher failure -> `WatchFailed` and child termination;
- explicit owner stop/destruction -> child termination and snapshot cleanup.

Temporary Test Level snapshots carry an ownership marker containing both the
Studio PID and its process creation timestamp. Abandoned-session recovery must
compare both values, so Windows PID reuse cannot cause an old snapshot to be
accepted as live. Unverifiable ownership remains conservative and is not
silently deleted.

## Automated acceptance

S5D deliberately reuses the production-path tests rather than adding duplicate
fixtures that could go green independently of the real code:

- `RenegadeDiagnosticServiceTests` — structured store, loopback endpoint,
  read-only routing, changing state, restart/bind failure and controlled
  diagnostic fault evidence.
- `RenegadeTestLevelRuntimeProcessTests` — real Windows child supervision,
  readiness, Runtime-reported failure, abnormal pre/post-ready exit, timeout,
  watcher failure, manual/destructor stop, cleanup, PID-reuse stale-session
  rejection and live-owner retention.
- `RenegadeS5DClosureSourceContract` — pins the cross-file integration seams:
  Studio/Runtime endpoints and state, ready-event ordering, failure/timeout
  paths, ownership timestamp defence, and registration of the two production
  tests above.

The two production tests remain in their existing `Tests/CMakeLists.txt` CTest
scope and therefore run once in every normal Studio validation. S5D does not
register aliases that would execute those Windows fixtures a second time. The
cheap `RenegadeS5DClosureSourceContract` carries the `S5D` label.

## Gate acceptance

S5D may be closed only when the exact PR head has all four normal Renegade
Windows checks green (baseline Debug/Release and Studio Debug/Release), the
normal production diagnostics/Test Level tests plus the S5D source contract
pass, and the owner performs one Test Level smoke from the CI artifact
confirming:

1. Studio starts Test Level and reports `active=true`, `ready=true`, and a
   non-zero child PID in Diagnostics.
2. Runtime starts the intended Scene and its diagnostic `process.pid` matches
   Studio's child PID.
3. Runtime script/audio/player state is visible while the child is running.
4. Stopping Test Level removes the child and Studio no longer presents cached
   Runtime evidence as a live child.

Once those conditions pass, S5 is closed. Later scripting work must extend the
existing governed runtime and diagnostic seams rather than reopening S5 with a
parallel transport or identity model.
