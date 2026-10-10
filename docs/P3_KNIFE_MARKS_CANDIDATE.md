## Glass crack contrast and puncture - 2026-10-09

Owner reports pale spider silhouette and missing dark centre, not a see-through
hole requirement. Native full material vs colour-only comparison (diagnostic
RENEGADE_GLASS_COLOR_ONLY) was nearly identical. Source GlassBase pixel inspection:
RGB188-226; opaque average222; all four tile centres alpha0. Thus source has no
painted dark puncture and lighting dims grey crack colour; normal/surface maps
are not principal cause. Runtime glass crack tint now3x white, normal strength.25.
Adds separate small native projected dark puncture with grey irregular rim, using
bounded64x64 procedural mask; same contact triangle anchor and120s expiry.
Both layers share existing64 decal-entity cap (glass uses two slots per hit).
No original asset altered; four source atlas shapes retained; no true hole cut.
Other surfaces and shard effects unchanged. Native final glass frame inspected:
bright crack detail and distinct dark centre visible; owner quality review pending.

Base cf04bd38434dbc3e84c90ce22ee628e3c3953a80; feature/p3-projectile-impact.
Changed Runtime/src/RuntimeImpactMarks.h, RuntimeProjectileVisuals.h,
Tests/ImpactAudioTests.cpp, docs. No upstream edit/commit/push/gate closure.
Comparison helper build PASS BUILD/p3-glass-compare-build.log; --marks-scene full
PASS p3-glass-compare-full.log and process-local colour-only PASS
p3-glass-compare-colour.log. Final Runtime/session/helper Release build PASS
BUILD/p3-glass-contrast-build.log, same three targets --parallel4.
Native --marks-scene Bow descriptor PASS BUILD/p3-glass-contrast-native.log;
verifies glass has two layers and complete expiry. Focused ctest initial run
BUILD/p3-glass-contrast-ctest.log had unchanged ProjectileWorldTests20s timeout;
retry exact four-test Release regex --timeout20 --output-on-failure PASS4/4
BUILD/p3-glass-contrast-ctest-retry.log. Stage PASS
BUILD/p3-glass-contrast-stage.log. Studio embedded Runtime refreshed; standalone
PID68748 process-local directional blood opt-in. Diagnostics
BUILD/p3-glass-contrast-runtime.json. Next: owner judge cracks/puncture around
embedded arrow in actual target lighting. Contrast factor remains artistic,
not physical calibration or final glass-quality acceptance.

## Surface mark scale/visibility correction - 2026-10-09

Owner screenshot image(20261009-153454).png shows elongated KNIFE marks and reports
marks disappear with distance after retained arrow expiry. Governed world marks
were still Component_Attach children of nonuniform receiver transforms, allowing
rotated footprints to distort. They now use the same native triangle contact
anchor path as skin; width/height remain world-space independent and receiver
motion follows surface barycentrics without hierarchical scale. Projector half
 depth increased .008 to .035 for surface-volume tolerance. Distance symptoms
have not been reproduced in a complete live arrow-expiry sequence; do not claim
proven cause. Native no-arrow proof now shows marks at1.2/2.5/4 metres. Four-metre
first proof was occluded by unrelated fixture object; read-only diagnostic now
hides unrelated renderables and repeated proof inspected successfully.

Base cf04bd38434dbc3e84c90ce22ee628e3c3953a80; branch feature/p3-projectile-impact.
Changes Runtime/src/RuntimeImpactMarks.h, RuntimeProjectileVisuals.h,
Tests/ImpactAudioTests.cpp, docs. Build Runtime/session/helper PASS
BUILD/p3-marks-scale-build.log. Helper --marks-scene disposable Bow descriptor
PASS BUILD/p3-marks-scale-native.log, verifies seven marks retain equal world
axis lengths under nonuniform receiver(.4,.8,.2) and have no hierarchy parent.
Focused ctest4/4 PASS BUILD/p3-marks-scale-ctest.log, same prior regex/Release
configuration. Helper-only rebuild PASS BUILD/p3-marks-distance-build.log;
repeat --marks-scene PASS BUILD/p3-marks-distance-native.log. Distance images
BUILD/p3-mark-distance-{12,25,40}.png inspected. --stage Bow descriptor PASS
BUILD/p3-marks-scale-stage.log. Studio embedded Runtime refreshed; staged
standalone PID63392 process-local directional blood opt-in; diagnostics
BUILD/p3-marks-scale-runtime.json. No commit/push/upstream change/gate closure.
Next owner check: several angled hits, wait arrow expiry then retreat/approach;
report whether pop-in persists. New launch starts with clean transient marks.

## KNIFE surface mark candidate - 2026-10-09

Owner requested supplied bullet holes including skin. Seven governed 2x2 atlas
sets now cover Metal Wood Concrete Stone Glass Dirt and Character. Water leaves
no bullet hole; Default retains procedural fallback. Each set imports colour-alpha,
normal and repacked surface PNG ResourceAssets. Source Unity gloss alpha inverted
to roughness G; grayscale specular approximates F0 in A; AO R=1 and metalness B=0.
No pack artwork embedded in source or committed. Optional material metadata keys
renegade.impact.mark.{metal,wood,concrete,stone,glass,dirt,skin} govern assets and
normal package discovery. No authoring picker/UI exposure added.

Runtime selects one static atlas quarter and varies roll/size per hit. Embedded
projectile marks remain .65 scale. Governed marks persist120s with64 oldest-eviction
cap; reset/expiry retained. Missing donors retain previous art and18s lifetime.
Skin only spawns if a donor and near-contact mesh triangle can be resolved; no
floating mark fallback. Native GetPositionOnSurface tracks barycentric centre and
triangle basis across rigid movement, CPU skinning and mesh deformation. Invalid
or deleted receiver/topology retires mark. This is a shallow projected decal, not
UV-painted skin; it does not wrap perfectly across joints and full live character
animation acceptance remains open. Current Character classification does not
separate exposed skin from clothing. Latest native CPU pose can introduce pose
latency; do not claim exact skin attachment parity. Blood effects unchanged.

Base cf04bd38434dbc3e84c90ce22ee628e3c3953a80, feature/p3-projectile-impact.
Changed Runtime/src/RuntimeImpactMarks.h, RuntimeProjectileVisuals.h,
Tests/ImpactAudioTests.cpp and documentation; disposable Bow scene/products.
Broad dirty tree predates task; no commits/push/gate closure.

Verification exact commands/results:
cmake --build BUILD/renegade --config Release --target RenegadeRuntime
RenegadeRuntimeProjectileSessionTests RenegadeImpactAudioTests --parallel 4 PASS
BUILD/p3-knife-marks-build.log. Earlier compile failures corrected token spacing
and explicit XMFLOAT3 helper overloads; final build passed.
Helper --install-knife-marks BUILD/bow-projectile-playground/Bow-Playground.renegade
BUILD/knife-marks-candidate PASS BUILD/p3-knife-marks-native.log: all21 governed
slots retain identities on Save/Reload; seven native marks render and expire;
skin anchor follows receiver translation and CPU triangle deformation.
Seven BUILD/p3-knife-mark-{Metal,Wood,Concrete,Stone,Glass,Dirt,Character}.png
images inspected; marks visible but somewhat soft/dark in the proof lighting.
ctest --test-dir BUILD/renegade -C Release -R
'(Renegade(ImpactAudio|ProjectileWorld|RuntimeProjectileSession)Tests|^LaunchSocket$)'
--timeout20 --output-on-failure PASS4/4 BUILD/p3-knife-marks-ctest.log.
Helper --stage same descriptor PASS BUILD/p3-knife-marks-stage.log.
Helper --marks-scene packaged GameData/Bow-Playground.renegade PASS
BUILD/p3-knife-marks-packaged-native.log; packaged21 map restoration and native
mark proof no Downloads/source-folder dependency. Studio embedded Runtime
refreshed; standalone PID68548 with process-local directional blood opt-in.
Diagnostics BUILD/p3-knife-marks-runtime.json. Next: owner visual judgement of
marks on actual targets/dummy, then animated Character wound verification.
