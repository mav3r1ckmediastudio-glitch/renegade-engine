# LP03 — Minimal Runtime Screen and Actions

LP03 is a bounded Runtime proof, not the full Renegade UI Designer.

The project fixture contains one serialized Renegade-owned Runtime screen with:

- one project-relative background image;
- one text element;
- one Play button using stable action `play`;
- one Quit button using stable action `quit`;
- deterministic Play -> Quit focus order.

All mouse, keyboard and gamepad activation routes produce the same
`RuntimeActionRequest` and enter the Runtime-owned dispatcher. `play` calls the
existing LP02 `LoadRuntimeProjectFlow()` path. `quit` requests ordinary window
destruction and exits through the existing Win32 shutdown path.

## Positive Runtime proof

From a built Runtime directory:

```bat
Run-LP03-Screen-Proof.cmd
```

Required manual checks:

1. Background, title, Play and Quit render.
2. Play has visible initial focus.
3. Arrow keys / Tab move focus and Enter / Space activate.
4. Gamepad D-pad / left stick moves focus and A/Cross activates.
5. Mouse hover changes focus and click activates.
6. Play removes the menu only after LP02 enters Level One.
7. Quit closes with exit code 0.
8. `Logs/RuntimeBootstrap.log` records screen ID, focused widget, action,
   input source, action result and LP02 flow trace.

## Automated proof

```powershell
.\Tools\Build-Studio-Windows.ps1 -Configuration Debug
ctest --test-dir BUILD\renegade -C Debug --output-on-failure
```

The dedicated tests are:

- `RenegadeScreenTests`
- `RenegadeRuntimeScreenTests`
- existing `RenegadeRuntimeFlowTests`

Negative coverage includes malformed documents, duplicate widget/action IDs,
missing Play, unknown action, invalid focus target, unsafe paths, duplicate
screen identity and missing background resources.

## Accepted evidence

Accepted on 2026-08-06 for commit `288dc91bba8e0e184dd8ac4fbc3e2ba224ad3f53`:

- GitHub Actions run #92: Debug PASS and Release PASS.
- Release artifact: `renegade-studio-windows-x64-Release-288dc91bba8e0e184dd8ac4fbc3e2ba224ad3f53`.
- Artifact digest: `sha256:23ea9b8657fb93ef01f2a79031190792d55b6f6530c8d206f1f329de918b9c2b`.
- Packaged DX12 visual startup: PASS.
- Mouse Play: PASS; entered LP02 Level One.
- Mouse Quit: PASS; normal shutdown and exit code 0.
- Keyboard Play: PASS; entered LP02 Level One.
- Keyboard Quit: PASS; normal shutdown and exit code 0.
- Gamepad-labelled controller/dispatcher path: automated PASS.
- Physical gamepad hardware path: DEFERRED because no controller was available.

The deferred physical-controller check is not represented as a pass. The same
stable action dispatcher is covered automatically, while real Wicked hardware
mapping remains a later physical verification item.

## Scope limits

LP03 does not add Studio authoring, anchors, responsive layout, HUDs, custom
fonts, scripting, video, cooking, standalone-game packaging or LF02
transactional project saves.
