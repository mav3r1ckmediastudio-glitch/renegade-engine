# Renegade Weather V2 — Single-branch delivery policy
**29 October 2026 | DESIGN**

## Owner-directed workflow correction
The weather system must be developed in **one continuing feature branch** until the complete agreed programme is ready for final validation and owner approval. Earlier G1/G2 were developed in separate isolated worktrees, which preserved work but complicated integration. The owner specifically requested the established Renegade workflow: finish all planned gates on the same feature line, with checkpoint commits between gates, then integrate once at the end.

**Active root:** `C:\Users\paulw\source\repos\renegade-weather-v2-all-gates-20261029` on `feature/weather-v2-all-gates`, seeded with exact owner-approved G1+G2 local checkpoint `78339d559687c19914aa0a4e9770cf80ecead3ae`.

**Active Wicked fork:** `WickedEngine/` on its own matching local `feature/weather-v2-all-gates` branch, seeded with `a96314b44d3ddbe364b2ea2f1c9a6e5715a3a1a1`. This includes the accepted river-base `ceb816b844b4ab2fa56a159ac60a4f65c931bdfc` plus G1/G2 weather source. Both branches were confirmed clean immediately after worktree initialization.

**Immutable historical previews:** original G1 tree `renegade-cloud-formations-g1-20261028` and G2 tree `renegade-weather-g2-20261029`; V4 executable in G2 at `BUILD/g2/Studio/Release/RenegadeStudioGate2FinalMoonlightV4Oct29.exe`, SHA256 `34171E94B4656BF5962CE929398B6178FBBBAE96C0EEB975483967B17CAA9581`. Do not delete or overwrite them.

## Gates and acceptance
- **G1** volumetric cloud formations and movement: owner visually approved; isolated 75 FPS baseline protected.
- **G2** moon/sun/atmospheric cloud lighting: final V4 visually approved by owner.
- **G3 next** modular weather presets and transitions: follow `docs/WEATHER_V2_GATE3_MODULAR_TRANSITIONS_PLAN_2026-10-29.md`.
- Remaining gates: the owner explicitly confirmed **FIVE Weather V2 gates**; seven was a typo. Gate 3 covers presets/transitions, Gate 4 environmental weather effects, Gate 5 integration and polish. Keep Gate 4, Gate 5 and Snow V2 out of scope until their turn. No further branch split.

Gate-level visual acceptance is a checkpoint, **not final sign-off or integration of the whole Weather V2 system**. The final release sign-off follows complete agreed gate work and full CI/packaging and owner testing.

## Operational rules
1. Develop G3 and later gates **in this same active root and matching Wicked branches**, not a new worktree per gate. Commit tested milestone changes locally, with clear gate scope and documentation.
2. No push, PR, merge into main, remote fork update, or final system sign-off until owner explicitly authorizes it.
3. Preserve the accepted river work and other owner work. Keep the Gitlink updated when Wicked commits, verify both repositories build from the exact local commits. Never silently use a remote submodule checkout that lacks the local G1/G2 checkpoint.
4. Full Studio user-path proof required for each milestone: clone of the correct project ID, Project Hub, Story Flow > Main Level > Open Editor, Environment settings actually respond, Test Game, FPS check, persistence/Undo/Redo/scripting where in scope.
5. For final integration, the matching Wicked checkpoint must first be published via an approved process before any root commit requiring it can be built in fresh CI. **No such publication has been done.**
