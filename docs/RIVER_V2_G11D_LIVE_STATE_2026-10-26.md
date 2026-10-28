# G11D live river state and full-size obstacle checks
2026-10-26. HEAD 11bbb90e4214776d5e51c8bc4cba2e7f1ccde296 plus preserved dirty work. No commit/push/merge, Wicked/main/alpha edits or owner scene saves.

Owner observations supersede G11C visual assumptions: detailed normals visible only close; dragging exposes native water and hides foam; delayed contact foam remains at old rock position; enable/disable can hang.

Changes:
- RiverCourseRenderer: enable is a parameter-only change, removed from structural hash; disabled simulation retains its built domain/history. Source-only publications retain both bulk foam textures, appearance history and clock when native bounds/resolution are unchanged. Geometry is not rebuilt on source-only publication.
- Moving sources immediately suppress old rock emission while the immutable CPU update is pending. Existing foam can advect/fade; new source is published only for the latest fingerprint. This does not make the approximately one-second CPU solve instantaneous.
- RiverFlowRenderer and RiverCourseRenderer: suspended preparation/transport retains the valid course render binding. Changed native spline shape still uses its validation guard. No stale binding is allowed across scene replacement.
- Course shading: removed frame-duration-derived texture filtering footprint; restored established native/directed normal detail on level reaches, blending travel-space normals only into the falling sheet. Coherent ladder displacement remains disabled.
- Graded transport: actual-size rock exposed unsafe 25.985m/s projected velocity at a 10cm artificially regularised film on the rock rim. Cells already at the supported 10cm floor and classified as rock contact/submerged are now dry/blocked at this finite resolution. Conservation projection and velocity safety checks remain; no arbitrary increase to the speed limit. This is a coarse-grid wet/dry modelling choice, not a resolved thin-film/free-surface claim.
- Optional RiverCourseRockProof: real RASSET native payload, full-size 2.72m-tall rock, repeated moves and mid-update stale rejection, frozen bulk mass preservation, parameter toggle no-rebuild, suspended shader retention, mature foam capture.

Final evidence:
- build-validated.log: Release Studio/Runtime, proofs and river tests build.
- tests-final.log: 19/19 rebuilt river regressions, 14.15s, PASS.
- final-rock.log: full-size rock repeated edits PASS; one final source publication per edit; ~1.1s completion or ~2s when an intermediate result is discarded. Max frame around 0.13s. Frozen bulk foam sum exactly preserved at 27113.7. Four toggles ~137ms total, no rebuild. Suspended course binding PASS.
- banks-final.log: actual 90 bank checks, concealed cap checks, node/history/native regeneration, amount history, zero speed and native archive PASS.
- reopen-final.log: fresh native process reopen PASS.
- lifecycle-final.log: real Runtime ready/30s/stop/cleanup/post-return edit and Undo PASS.
- full-rock-film.png and final-contact.png: actual full-size emerged rock visibly inspected; foam responds around the rock. Owner aesthetic/real mouse acceptance remains OPEN.
- package-final.log records public Build Game result separately.
- Old G11B and G11C binaries/editors/projects preserved.

Failed/intermediate evidence is retained. Earlier G11C and initial G11D raw-payload tests used the correct 24,648-triangle mesh at centimetre scale, so they did NOT establish visible contact behaviour at owner placement size. full-rock.log and velocity-detail.log then failed with the unsupported thin-film jet; these are not passes. Full-size passing evidence is full-rock-film.log and final-rock.log. Do not repeat the prior claim that initial small-mesh tests proved rock-contact foam.

Limits: source snapshot/publication can still hitch about 0.1s; source updates are delayed roughly 1s and stale work can add another solve; initial/author-geometry rebuilds remain synchronous. Surface/width/bed/reflection/elevated-junction restrictions remain. Thin wet/dry rim is a finite-grid approximation. Existing foam should drift/fade, not disappear instantly. Original owner live unsaved rock edits are untouched. Separate copied review project under RenegadeRiverLiveReview_20261026; new G11D Studio PID29524. Mountain streams paused.

Final handoff: public Build Game PASS. G11D PID29524 project was opened through the real hub file picker, River selected in StoryFlow and Open Editor used. Actual river viewport inspected at75FPS (viewport-check.png); old PID28932/G11C and PID12400/G11B untouched. Await owner actual drag/toggle/normal-appearance feedback.
