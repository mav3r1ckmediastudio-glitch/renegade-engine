# Terrain initialization and Player Start preview recovery

Validation updated 2026-11-03 on DESIGN.
Branch: recovery/20261101-repair-implementation.
Base: 6c59d2a089d3fa57f92f03534354927e9ff8676e.
This is a review candidate. Main, the existing alpha and release gates remain unchanged.

## Result

Fresh standard terrain now starts at world Y=0, with an identity root and the
original -20..120 m encoding range. Ordinary shallow river drawing and Link
Riverbed work without Allow Deeper Beds. The Player Start preview no longer
rebinds the editor terrain's generated texture resources.

## Implementation

- TerrainService creates a native serialized constant HeightmapModifier.
  It supplies the same zero reference to initial chunks, expansion and regeneration.
  Existing WISCENE landscapes receive no modifier, offset or sample migration.
- Sculpting clamps actual mesh heights before R16 encoding; the live mesh and
  saved state cannot diverge beyond the encoding floor or ceiling.
- New terrain starts bare. Manual vegetation synchronization waits for generated
  chunks to merge before caching completion; the idle identity/count gate remains.
- PlayerCameraPreviewService gives generated terrain chunk materials private
  resource wrappers. Ordinary authored textures retain the prior sharing path.
  Native preview atlas updates can no longer mutate editor bindings.
- Existing edited/resident chunk separation, payload/work/layer limits, range and
  transform guards, explicit depth-extension operation and linked expansion
  restrictions are unchanged. Wicked's source and gitlink are unchanged.
- The new native regression uses production creation and never extends the floor
  to prepare fresh-terrain Link. Its original-terrain baseline fails.

## Actual Studio verification

The ordinary RenegadeStudio.exe image was used, with its existing PageHeap setting.
SHA256: A443C2C54D269C6EBBDEDCD9076ECCF9DC8B4071D61049F6236CB05E7382D1B1.

| Check | Result |
| --- | --- |
| New governed starter level, default 361 chunks | PASS: root 0, surface within 0.003 m of 0, range -20..120 |
| Draw shallow river, Link Riverbed, no depth extension | PASS: visible water/channel, explicit linked state, 1.35 m bed setting |
| Width and node-width history | PASS: base width 8/12 m Undo/Redo; node width 12/18 m; linked bed follows |
| Lower sculpt | PASS: saved minimum about -2.988 m below initial zero |
| Sustained Lower at strength 10 | PASS: actual saved mesh stops at -20 m |
| Sculpt Undo/Redo | PASS: exact pre-stroke and restored sculpt height hashes |
| Unlinked expansion and history | PASS: 361 -> 441 -> 361 -> 441; all 361 inner encoded maps unchanged; 80 new chunks within 0.003 m of zero |
| Player Start selection and preview | PASS: main viewport stays textured before/after Test Game and after cold process reopening |
| Actual Test Game | PASS: player walks from starting bank through the channel to the opposite bank; clean Studio return |
| Close and cold process reopening | PASS: expanded sculpt and linked river/node width persist; saved height hashes unchanged |
| Standalone Runtime | PASS: actual project, Title -> Main Menu -> New Game -> saved river; channel and bank walking |
| Old linked river/grass copy | PASS: visible grass/rain and linked state; UI resave retains original root, heights, masks and layer hashes |
| Original owner files/backups | PASS: 145/145 unchanged |
| Inherited uncommitted work | PASS: original imnodes patch unchanged; unrelated files retained |

The expansion UI comparison used flat inner terrain. The separate native proof
also expands sculpted and painted terrain, verifying inner sculpt and grass
preservation, history and cold reopening.

Runtime SHA256:
F648F308BE9747464CB68817E713675D5153DDDAFE16FDE3F149C13E0306EB14.
Standalone launched the actual saved project, not a Test Level snapshot.

## Numerical and native evidence

- Cold Studio river height hash: 14348197206417472456, unchanged after resaving.
  Sampled visual bed Y=-1.5; Jolt Y=-1.497055; error 0.002945 m.
- Cold expanded sculpt height hash: 14223170957828022511, unchanged after resaving.
  Sampled visual floor Y=-20; Jolt Y=-19.995850; error 0.004150 m.
- Sculpt Undo restores flat expanded height hash 3453240936459153443;
  Redo restores 14223170957828022511 exactly.
- The native Player preview regression runs three rebuilds after sculpt, painting
  and expansion: all 1,764 editor bindings and clone R16 heights remain unchanged.
- The original native preview reproduction changed 1,444/1,444 source bindings.
  The repaired identical probe changes zero; negative and fixed captures remain.
- Old-level native baseline/current comparisons preserve authored data. The old
  linked scene retains root Y=20, range -30..120, no new modifier, height hash
  6293622727350231422, grass/paint hash 9387035919787997421 and river-layer hash
  5296365137439531552, including after actual Studio copy resaving.

An extra sub-cell ray near the steep edge of the -20 m stress pit found a
0.097260 m visual/collider difference. The original bridge translation unit
produces exactly the same result with the pinned native physics. This strict
0.02 m probe failure is retained, not relabelled a pass. Grid sample contacts,
ordinary channel walking and cold sculpt collision checks pass. The precise
native sub-cell discrepancy is a separate existing limitation; no core physics
change or universal collision-accuracy claim is made.

## Responsiveness and validation

On the same ordinary image, 1584x861 client and Player preview open, recorded
before/after Test Game screenshots show 60/61 FPS (roughly 16.7/16.4 ms periods).
These are visible status readings, not frame-time percentiles. Sculpt receipts
report 13 ms for one edited tile and 15 ms for the four-tile limit stroke.
The renamed earlier executable is not a comparable performance baseline.
No registry change or performance optimization is claimed.

Release RenegadeStudioCI aggregate: PASS.
Final unfiltered Release CTest: 307/307 PASS, no skips, 207.59 s.
The strengthened preview-after-sculpt regression is included in that final run.
The original 305/307 stale-contract run is retained as history and superseded.

Commands:

```text
cmake --build <build-dir> --config Release --target RenegadeStudioCI --parallel 2
ctest --test-dir <build-dir> -C Release --output-on-failure
```

Screenshots, saved scene copies, raw logs, baseline/fixed probe receipts and
per-chunk comparisons are preserved outside source control in
TERRAIN_INIT_PRESERVE_20261102_215658 under the recovery audit. The reviewed
evidence pack contains original PNGs and the numerical receipts.

## Remaining gates

One fresh four-check CI run must validate the final proposed recovery revision.
The older green checks and existing alpha on 6c59d2a do not validate this repair.
Record CI results outside source control to avoid result-only rebuild loops.

Independent exact-revision verification using VERIFICATION_CHECKLIST remains open.
No main merge, release-gate closure, alpha replacement or distribution is authorized.
The owner's earlier running session is preserved and still uses the interim
executable; it does not acquire this repair while running.


## 2026-11-03: Hosted terrain regression timeout repair

The first four-check run on ae045c35cbed13ec3e511eea52547441e468be15
passed Windows baseline Debug/Release but failed both Studio configurations.
Both applications compiled. The sole failing CTest in each was the new
RenegadeTerrainInitializationRecovery at its existing 600-second timeout on
Microsoft Basic Render Driver. RenegadeRiverDefaultTerrainRecovery passed in
both configurations (Release 412.26 s; Debug 483.98 s).

The new initialization helper rasterized the full scene during every settling
tick. Native data, uploads, generation and physics now settle through native
updates on hosted graphics. Full-resolution source captures and each preview
render have separate bounded cases; every case still performs all three preview
rebuilds, twelve updates per rebuild, all 1,764 binding checks and every authored
sculpt/R16, grass, expansion, history and cold-collision assertion. Desktop mode
retains the full presentation path. Terrain extent, renderer quality, 960x640
configuration and the 600-second per-test deadline are unchanged.

The original initialization case captures the source before/after the complete
preview sequence. RenegadeTerrainPreviewRender0Recovery through Render2Recovery
render the corresponding preview; they also retain all three native rebuilds.
The Release suite now contains 310 tests. This is test scheduling only: no
application behavior, Wicked source/pin, OS graphics setting or safety limit
changes. Studio and Runtime remain byte-identical to the earlier UI-tested
binaries. The two-thread exploratory candidate and four-thread combined-render
candidate are retained externally; the latter still timed out during its third
preview draw and is not a passing receipt.

Local WARP validation uses an audit-only driver translation unit outside the
checkout: real EnumWarpAdapter selection and software-adapter acceptance, with
all remaining pinned driver code unchanged. Process affinity is scoped to each
proof process. The negative executable substitutes only the preserved old
PlayerCameraPreviewService header and must fail the terrain-atlas assertion.
Receipts are under CI_TERRAIN_INITIALIZATION_TIMEOUT_20261103 in the recovery
audit. Final local results and the next exact-head four-check run are recorded
there; neither historical local success nor these fixture changes close the
independent release gate. Main merge and another alpha distribution still
require owner approval.
