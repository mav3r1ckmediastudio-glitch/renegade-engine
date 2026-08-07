# LP04 Gate 3B2 — Real Runtime Handshake Evidence

## Claim

**Gate 3B2 is PROVEN**, on real-hardware local evidence, following the same
evidence-tier precedent LP03 already established for graphics behavior that
hosted CI cannot meaningfully validate.

## What Gate 3B2 required

The full, previously-unproven join: a real `TestLevelSnapshotService`
snapshot, launched through the real `TestLevelRuntimeProcess` launcher,
against the real windowed `RenegadeRuntime.exe` (not the synthetic Gate 3A
fixture), reaching `Running` via the real ready-event signal (`310b8e1`),
then `Stop()` terminating it and cleaning up the snapshot.

## Accepted evidence — local, real hardware

`Tests/TestLevelRuntimeHandshakeTests.cpp`, run directly against the built
`RenegadeRuntime.exe` on the project owner's Windows machine (real GPU, real
DX12 device):

- Run 1: reached `Running`, `Stop()` succeeded, snapshot cleanup succeeded,
  session directory confirmed removed.
- Run 2 (repeat, to rule out a fluke): identical clean result.
- Process/window check after each run: no lingering `RenegadeRuntime.exe`
  process, no stray window.
- `EXCLUDE_FROM_ALL` verified to actually hold: deleted the built exe, ran a
  full default `cmake --build` with no explicit target (matching how CI
  builds), and the exe did not reappear.
- Full `ctest -C Debug`: 22/22, unaffected.

This is real, repeated, independently-verified proof that the handshake
works correctly end to end on real graphics hardware.

## Rejected evidence — hosted GitHub Actions runner

A dedicated, isolated probe workflow
(`.github/workflows/lp04-runtime-handshake.yml`) was built specifically to
find out whether the same handshake also works on GitHub's hosted Windows
runner, without risking that question blocking any required check.

**Run:** [`31225218342`](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/31225218342),
`windows-2025`, triggered manually via `workflow_dispatch`.

| Stage | Result |
|---|---|
| Checkout, MSBuild setup, CMake configure | Passed |
| Full build (WickedEngine through the handshake test) | Passed |
| Real Runtime process launch | Passed — the process starts |
| Real Runtime reaching `Running` | **Failed** — `StartupTimedOut`, exit code `60930` (`0xEE02`, Studio's own forced-termination code, not a Runtime-reported failure) |
| Studio's timeout/forced-termination/cleanup safety net | Passed — even in this failure, the process was killed cleanly and the snapshot was fully cleaned up; no zombie process, no corrupted state |

No `RuntimeBootstrapLog` artifact was produced (`if-no-files-found: ignore`
triggered), meaning Runtime never reached the point in its own bootstrap
sequence where that log is written. Combined with the exit code being the
*timeout* code rather than a `RuntimeBootstrapCode` failure (which would
have been a clean exit in the 20-29 range), this points to Runtime hanging
during early startup — most plausibly graphics device/window creation —
rather than failing on anything project- or scene-related.

## Root cause classification

**A hosted CI graphics-environment limitation, not a Renegade defect.**

This is consistent with, not a new finding beyond, what R06 already
recorded about this CI environment: the observed baseline build host used a
"Microsoft Hyper-V Video adapter" with no detected Vulkan runtime, and CI
has never previously launched or rendered with the packaged Runtime — only
compiled and unit-tested it. This probe is the first attempt to actually
launch the real windowed Runtime under CI, and it demonstrates directly
what R06 could previously only infer: the hosted runner's graphics adapter
cannot support real Runtime startup.

## Evidence-tier precedent

This is the same distinction LP03 already made explicit: CI proves
compile/test correctness; a separate real-machine pass is required for
DX12/packaged behavior CI cannot validate. LP03's accepted evidence
combined a GitHub Actions run with the project owner's own packaged Release
test on real hardware. Gate 3B2 uses the identical structure: CI proves the
code builds; the project owner's real-hardware run proves the handshake
itself.

## Decision

- Gate 3B2 is accepted as proven on the local real-hardware evidence above.
- The probe workflow is kept, but changed to `workflow_dispatch`-only,
  removing its previous `pull_request` trigger. It was never a required
  check and never blocked a merge, but leaving it on automatic path-filtered
  triggers would mean it re-runs and fails on every relevant future change,
  for a reason nobody can fix from the Renegade side. It remains available
  to re-run by hand if a future GitHub runner generation changes this.
- No GPU-capable hosted runner tier will be pursued for this. The cost and
  complexity are not justified by what would be gained: local real-hardware
  verification is already the legitimate, established evidence tier for
  exactly this class of behavior.
- Gate 3B3 (Studio PLAY/STOP wiring) proceeds on this basis.
