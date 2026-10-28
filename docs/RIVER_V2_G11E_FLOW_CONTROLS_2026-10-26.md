# G11E flow-control blocking, normal advection and upper-reach contact
2026-10-26. HEAD11bbb90e4214776d5e51c8bc4cba2e7f1ccde296 plus preserved dirty work. No commits/push/merge, owner scene saves, main/alpha or Wicked edits.

Owner accepts G11D normal detail and rock-placement responsiveness, but reports transverse normal motion, missing contact foam with upper-reach rocks, and freezes when changing flow settings. These reports override nominal G11D completion claims.

Changes:
- Course author/geometry hash no longer includes inlet speed/contact width. Those enter the immutable worker snapshot fingerprint with source geometry. Rapid edits discard stale results. Speed changes publish updated travel UV/path data and refresh the derived binding after the CPU domain is ready, retaining foam. Zero-speed reset remains a bounded synchronous empty-domain operation. Appearance parameters/toggles remain buffer edits.
- Course normal advection maps world current through the actual pixel world-to-texture Jacobian, instead of assuming native tangent conventions on the derived mesh. Existing normal texture/strength, falling-sheet sampling and disabled ladder displacement retained. Shader stages/compiles; owner animated direction review remains required.
- Actual full-size upper-reach rock reproduced a velocity-envelope failure even after G11D thin-film drying. Enlarging the dry threshold did NOT fully solve it; that attempted approach was reverted. The current projection limits admitted discharge uniformly ONLY when projected speed would exceed the existing20m/s supported envelope, targeting19m/s. Both boundary flux and pressure correction, plus incident source velocity, scale together; no per-cell velocity clipping and no raised safety threshold. This preserves local/global conservation but may reduce the achieved inlet speed in sharp-obstacle cases. It is bounded prescribed-surface kinematics, not solved backwater/free-surface hydraulics.
- Proof now actually assigns the camera after RenderPath init, tests upstream rock placement and default foam appearance, repeated moves/stale rejection/frozen foam, and ten rapid combined commits of speed/contact width/intensity/lifetime/turbulence/threshold/rapid amount/on-off. Default profile is restored before mature foam capture.

Evidence:
build-validated.log Release Studio/Runtime/proofs/tests PASS.
tests.log 19/19 river regressions PASS13.53s.
upper-default.log: real24,648-triangle rock, ~2.72m high, upper reach; repeated move max render frame~0.123s, async completion~1.08s or~2.01s for stale replacement. Frozen bulk mass25182.2 preserved exactly. Ten rapid settings commits max frame12.5633ms. Four toggles132.802ms total with no rebuild. Suspended course shader retention PASS.
upper-default.png visually inspected: default-profile contact response visible around upper rock, foam carried downstream. Owner style/actual interaction acceptance still OPEN.
banks.log real bank/cap/node/history/native-save proof PASS. Initial reopen.log process exited -1073741819 (access violation); isolated retry reopen-retry.log / reopen-retry.exit passed with mass10814.6, rebuilds1, exit0. The first failure is retained and is not pass evidence.
lifecycle.log real Runtime30s/return/edit/Undo/cleanup PASS.
package.log public Build Game and package startup smoke PASS.

Failures retained: upper-rock.log21.876m/s at0.103m depth; upper-final.log adaptive dry threshold still failed20.95m/s at0.203m depth; neither is pass evidence. upper-validated.log had a619ms frame while build/initial rendering were concurrent; no cause is claimed established. Final upper-default.log uses120 warmup frames and no concurrent build. It verifies warm editing, not zero cold-start hitch.

Limits: initial loads and actual width/node/linked-terrain geometry edits still synchronous. Source/settings worker solve~1s, stale replacement~2s; GPU publication can still hitch~0.1s. Public UI native slider drags are NOT independently certified by rapid backend command tests. Achieved discharge may be reduced in above-envelope obstacle cases. Existing river/course bounds, reflection and connection restrictions remain. No universal physics/performance/release claim. Owner's G11D PID29524 and unsaved live scene preserved. New G11E PID15060, copied project RenegadeRiverControlsReview_20261026.

Owner follow-up: normal detail/direction and editing responsiveness accepted. Contact foam/wakes initially absent, then appeared around all rocks together. Update latency/recovery remains unresolved; no exact root cause yet established. Owner rejects the visibly different end strip and requests continuous original surface extended into terrain. G11F is authorised to address both; main integration awaits the owner's final river feature.
