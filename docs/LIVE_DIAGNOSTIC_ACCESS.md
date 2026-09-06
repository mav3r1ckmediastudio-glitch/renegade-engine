# Live diagnostic access — implementation candidate

Base: `5ae9cb579613d58b52a4db9fc89e13e0c0cd884d`, PR #140,
`scripting/s5c-cross-script-events`. This task is not accepted or owner-proven.
Main does not yet contain the existing DiagnosticService; preserve the PR #140
implementation when integrating this branch.

## Connection boundary

This is a local, read-only bridge. A coding agent with command execution on the
Windows machine running Renegade can invoke the reader below. A cloud chat does
**not** gain access to that machine's localhost by installing this code. There
is no tunnel, cloud relay, OpenAI API, API key, embedded model or paid service.
No available tool in the implementation session could reach the owner's PC.
Therefore the brief's end-to-end workflow is still unproven from that session.

With Studio running, a local coding agent runs:

```powershell
py -3 Tools/Read-RenegadeDiagnostics.py
py -3 Tools/Read-RenegadeDiagnostics.py --process studio
py -3 Tools/Read-RenegadeDiagnostics.py --process runtime --events errors
py -3 Tools/Read-RenegadeDiagnostics.py --expected-commit FULL_40_CHARACTER_COMMIT
```

The reader ships in the Studio ZIP's `Tools` directory. It requires Python 3.9+
and uses only its standard library. No setup inside a project is required.
Exit 0 means an endpoint was read with no identity/liveness finding, not that
all engine features passed; event errors remain evidence for the agent to inspect.
Exit 1 identifies build/heartbeat/child-association findings. Exit 2 means no
requested process could be read. An unavailable endpoint is not proof that the
application is stopped: bind conflict, startup failure or wrong build are possible.

## Retained implementation and repairs

The existing DiagnosticService, severity/source/code/message events, Studio
Diagnostics tab, Runtime script/audio events and Wicked diagnostics remain.
The old endpoint spun on nonblocking accept, did not check HTTP routes, reported
success before binding and did not guarantee full sends. Runtime file sharing
could become stale and performed file I/O on Record; Studio read and serialized
that evidence during every PreRender.

DiagnosticEndpoint now binds synchronously to 127.0.0.1 only; it reports bind
failures, uses exclusive address binding, select waits and bounded request/send
deadlines, handles partial sends, joins on shutdown and supports restart.
Only `GET /snapshot`, `GET /` and `GET /summary` are served. Browser Origin and
non-loopback Host headers are rejected. No writes or commands are exposed.

Ports are 38741 (Studio) and 38742 (Runtime). Multiple same-type instances are
not supported in this first version: the second records a bind failure rather
than silently pretending to be attached. The local reader checks process type,
commit, heartbeat age and Test Level child PID. A standalone Runtime is not
automatically claimed as Studio's child. Studio caches live Runtime HTTP evidence
on the endpoint thread approximately once per second; unavailable data clears
the cache. There are no diagnostic filesystem writes.

The human tab receives bounded recent event summaries from the same stores,
including Runtime summaries. Existing Wicked diagnostic controls are untouched.
Full machine evidence is available from the local reader. GPU-dependent layout
and preservation of Wicked diagnostics still require owner verification.

## Snapshot contract

`renegade.diagnostics.v2` contains `state`, bounded `events`, `elapsed_ms`,
`heartbeat_age_ms` and `endpoint_port`. Events include sequence, elapsed time
and consecutive-repeat counts. Clear removes events, not live state or sequence.
Missing/unavailable values are distinct from success. Event buffers cap at 512;
groups at 32; fields at 64 per group; text fields and event messages are bounded.

| Group | Evidence |
|---|---|
| process | Type, PID, start timestamp, source commit, configuration |
| editor | Project/scene, workspace, selected process-local entity/name, tool, placement, drag, import and scene-loading state |
| last_action | Named request, sequence, requested/handler-entered/handler-returned stage and source location |
| inspector_refresh | Last entry to Inspector refresh and timestamp |
| input | Sampled viewport/focus/camera state and applicability |
| last_pointer | Latest held mouse-button frame, routing stop, whether camera handler was reached/active, timestamp and source |
| vegetation | Brush enabled, paint/erase and active stroke |
| test_level | Child PID, active/ready, lifecycle enum code, message and warning |
| runtime | Startup result, scene, player spawn, script status/counts, authored audio sync/source count, pause/screen/action state |

`handler_returned` does not mean success. An asynchronous import/save can still
be in progress. Pair it with current state, errors and the relevant source.
`has_selection` means the selection service holds an ID, not a full-scene
existence proof. The name is read by direct component lookup. Scene-specific
selection/input are marked inapplicable while Story Flow/Screen Editor owns the
application; this slice does not expose those editors' internal selections.
`audio_scene_synced` reports completion of existing authored activation, not a
hardware audibility test. `last_pointer` is retained evidence and its timestamp
must be checked; it is not a claim that a button is still held.

Studio and Runtime copy cheap state approximately four times per second. The
application heartbeat remains separate, so a responsive transport cannot hide a
stalled app thread. No full-scene scans, raw per-frame event logging or ECS reads
from network threads occur. Action callbacks use bounded hooks; diagnostic
implementation is in dedicated files rather than StudioApplication.cpp.

Future gates add small `SetState` groups and stable Record events at the actual
failure boundary. They should not scan every entity or claim that successful
compilation proves behavior. Missing fields must remain explicitly unknown.

## Acceptance and evidence

Local production-store tests and reader tests are documented in HANDOFF.md.
The Windows branch of DiagnosticServiceTests exercises real loopback requests,
port conflict, changing state, read-only routing, restart and a test-only blocked
action. It clears that fault before exiting and never alters a project.
This transport fixture is **not** acceptance test D in a running Studio build.

Still required on the exact Windows artifact:

1. **A:** Launch Studio and query it locally. Verify commit/configuration, project,
   scene/workspace, selected entity and recent events against the running UI.
2. **B:** Switch a tool/select an entity; observe request/dispatch/state evidence.
   Hold RMB while painting and inspect `last_pointer.camera_handler_reached`,
   `routing_stop`, vegetation state and timestamps. Verify navigation is unchanged.
3. **C:** Start Test Level and verify its Runtime PID matches the child PID, both
   build commits match, and scene/player/script/audio evidence changes live.
   Stop Runtime and verify the reader reports unavailability and Studio's cached
   Runtime text clears. Launch a second Studio to exercise the bind-conflict case.
4. **D:** In a disposable test branch/project, temporarily block one harmless
   diagnostic test consumer at its real handler and record its source/reason.
   Have the local agent identify that boundary from live queries. Remove the
   fault, rebuild and repeat the query. Do not accept the fixture as a substitute.
5. Verify Diagnostics tab/Wicked controls and editor behavior, including Test
   Level and a packaged Runtime, then have another AI or human verify the exact
   commit. Measure idle endpoint CPU and populated-scene frame time against base.

Do not mark the task complete until A–D and the local-agent connection are proven.
