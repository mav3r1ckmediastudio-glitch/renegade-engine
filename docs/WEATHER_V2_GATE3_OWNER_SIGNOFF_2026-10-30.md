# Weather V2 — Gate 3 owner acceptance
Date: 30 October 2026
Branch: `feature/weather-v2-all-gates`
Status: **OWNER APPROVED — G3 checkpoint**. Weather V2 system-wide release remains OPEN (G4 and G5).

## Owner decision
The owner explicitly confirmed that **all remaining Gate 3 features have been validated from their side**, following the owner-tested Storm preset at approximately **75 FPS** and a Test Game screenshot showing a running DX12 Runtime at **74 FPS**, 1920 x 1009 on RTX 4070 Ti. This constitutes G3 owner functional and visual acceptance; do not ask for the same tests again without a new regression or new scope.

## Existing independent/automated evidence
- G3 deterministic controller, presets and Studio inspector first checkpoint: `ab32526`.
- Release `RenegadeRuntime.exe` linked from current G3 source before the final Lua test fixture corrections.
- `BUILD/g3-s3-lua-final-build.log`: Release S3 governed Lua tests built successfully.
- `BUILD/g3-s3-lua-final-ctest.log`: **5/5 PASS** (governed Lua Runtime, Weather Lua Runtime, WeatherTransition, Moon, CloudFormation).
- `BUILD/g3-*.png`: previous live Studio inspector, transition, cancel and undo/redo evidence.
- Current Studio Release build requested at `BUILD/g3-studio-current-release-build.log`; check its result separately before committing final milestone.

## Authoring and persistence contract
A Studio preview is not a resumable game-state checkpoint. Saving while blending finishes and commits the intended preset as one undoable authoring action. Reopen loads the committed weather; Undo/navigation cancel an incomplete preview. This is the documented G3 behaviour, not an accidental claim that an editor tween resumes across process restarts. Future saved-game checkpoint support would be separately scoped.

## Boundary
- G1 cloud geometry and G2 V4 moonlight remain approved and protected.
- G4 is environmental weather effects; G5 integration and polish. **Snow V2 remains after completion of Weather V2**, not part of G4.
- Continue on the same root and Wicked `feature/weather-v2-all-gates` branch, with local milestone commits; no push, PR, merge or final weather-release signoff without explicit owner authorization.
