# Hosted default-terrain river proof — 2026-11-02

## Observed failure

Recovery parent `9591ba2a511cce6ce940db3d8376f72decc772d5` passed the
complete local hardware Release suite (305/305). Its new GPU upload-buffer
regression also passed both hosted CI configurations, confirming the prior
invalid UPLOAD/UAV allocation was repaired.

The default-361 river proof then timed out at its unchanged 600-second limit.
Hosted Release generated all 361 chunks at 583 seconds and reached the default
terrain data checks at 587 seconds. It had spent almost the whole budget drawing
the fixed 160+20-frame warm-up on Microsoft Basic Render Driver. Hosted Debug
also timed out during that warm-up. These were failed checks, never waived.

## Independent diagnostic

A separate unshipped DX12 object explicitly selected local WARP and limited the
diagnostic process to four logical CPUs. Production/upstream source, adapter
selection and settings were not changed. The original fixed-frame path reached
only frame 8 /120 chunks at 46.4 seconds; that local diagnostic was deliberately
stopped at 90 seconds and is not a pass. The first standalone setup attempt
lacked its native terrain Content; that fixture deployment was corrected before
this comparison.

The candidate generated the full native terrain through normal scene updates
in about 1.4 seconds. Three real GPU warm-up frames then took about 63 seconds.
All linking/locality/history/mask/external-edit refusal checks passed, followed
by eight real post-edit draws, PNG GPU readback, save and cold scene hydration /
recomposition. The complete candidate exited0 in 217.96 seconds with 361 chunks,
three edited chunks and a 219,183-byte river-layer document.

## Bounded test change

Only the hosted `default-recovery` path changes:

- Run normal PreUpdate/Update, submit command lists and advance deferred GPU
  allocation releases while native terrain generation progresses.
- Require all 361 chunks before drawing. Preserve the existing bounded 1800-update
  generation loop and wait for native generation/GPU ownership before edits.
- Execute three real GPU warm-up frames, normal updates after river creation,
  and eight real GPU frames before the existing PNG readback.
- Keep every original data/safety/history/persistence assertion. Emit update and
  GPU-frame counters for diagnosis.

The owner desktop retains its original Application::Run presentation path and
frame counts. Terrain extent, renderer quality, application code, adapter
capabilities, assertions, test registration and the 600-second timeout are
unchanged. This is a functional graphics/data proof, not a software-renderer
performance benchmark or a replacement for actual Studio visual acceptance.

## Successor verification

The local diagnostic used the current production Release libraries with the
candidate test driver; it is a preflight. Rebuild and run the full successor
suite, all four exact-head CI jobs, actual packaged Studio/assistant/exported
game checks and the freshly extracted ZIP launcher before handoff. PR comments
and VALIDATION identify the final accepted source and immutable receipts.
