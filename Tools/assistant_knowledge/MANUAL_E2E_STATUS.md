# Projectile manual — end-to-end native QA receipt

**Test date:** 13 October 2026
**Result:** **PASS** for the beginner's projectile workflow using an **existing configured bow/player fixture**.
**Publication status:** **DRAFT — creator acceptance and optional surface-type exercises still outstanding.**

## Isolation and build

- Authorized DESIGN PC, locally installed `BUILD/renegade/Studio/Release/RenegadeStudio.exe` (last written 9 October 2026, DX12).
- Original source fixture, left untouched: `BUILD/bow-projectile-playground/Bow-Playground.renegade`.
- Full standalone QA copy: `BUILD/manual-projectile-e2e-20261013/Bow-Playground.renegade`, renamed **MANUAL QA COPY - Projectile Playground** within its own descriptor before opening. Its distinct name was verified on the Project Hub, Studio, and the runtime window title.
- The copied project shares the source fixture's original project UUID because it was cloned from an existing asset-rich project. **Do not regard this as a reusable template for shipping**; the QA copy is isolated by project-root path, not UUID.
- Active checkout at testing was `feature/p3-original-impact-vfx` at `60634cec26feb7ffc2d0f941d3381865fa3b41c7` with unrelated uncommitted work. The executable predates some working-tree changes. No new engine build was performed for this receipt.

## Verified native Studio workflow

1. Opened the QA descriptor by name and verified its distinct **MANUAL QA COPY** identity in Project Hub and scene status.
2. Entered existing ArmsPlayground level, selected **Characters → Player Start**, opened **Starting Equipment → WEAPON FIRING**.
3. Existing bow was correctly bound to **Primary hand / Primary fire / use**, `Bow_Nock`, with **After a delay: 0.133333 s**. This project does not use a Release charged action for the shot.
4. Clicked **NEW** and changed **Start with** from Bullet to Arrow. Selected existing imported model `Content/Projectiles/Models/Arrow.rasset`, chose **Stick into the object**, and entered `Manual QA Arrow E2E`.
5. Clicked **SAVE AS NEW**. Studio displayed the newly saved **Manual QA Arrow E2E / 45 m/s** asset under Weapon firing. On disk its file exists **only in the QA clone**:
   `Content/Projectiles/c030b651-00c0-4af7-9827-3a29ed42e46e.rprojectile`. JSON confirms model ID `58cf6cb9-da89-4370-8a04-71d861aef990`, `stick_on_impact: true`, speed 45 m/s, gravity scale 1.
6. Clicked **APPLY TO THIS PLAYER**. The editor confirmed assignment to this player's weapon, and a new equipment definition was written **only inside the QA copy**:
   `Content/Player/Equipment/87a28639-a59f-4bd4-b428-bb75ee82591a.requipment`; that file references the newly authored projectile asset.
7. Pressed **Ctrl+S** in Studio and verified the test scene was written. SHA-256 comparison of `Content/Scenes/ArmsPlayground.wiscene`:
   - Source fixture before/after: `68adb4ecd391e5cf1ec8f667ea9f0c46990ce21082c1494c259056553680c5e0` — **unchanged**.
   - Cloned scene after save: `b542716bf4d0012560c9f2f2b9930872d9e38e0360b9e69b923a27e86088dc0f` — **changed as expected**.
8. Chose **File → Reopen Scene**. In Starting Equipment the selected primary equipment had the newly created `87a28639...` identity, and Weapon firing still showed **Manual QA Arrow E2E**, `Bow_Nock`, **Primary fire / use** and 0.133333 s delay. Real screenshots were captured.
9. Started **Test Level** through the Studio **PLAY** control. Runtime window title explicitly read `Renegade Runtime - MANUAL QA COPY - Projectile Playground [DX12]`, confirming the correct project. The authored bow rendered in first-person. Clicking LMB played its firing animation; a visible impact effect occurred on the center target and a dark retained impact/projectile mark remained after the animation. Moving the player sideways left the mark attached to the target (not the viewport). Captures were taken at ~0.15 s, ~0.5 s and multiple seconds later. The runtime exited normally and Studio reported `TEST LEVEL // COMPLETED`.
10. Returned to **Project Hub** and reopened the **MANUAL QA COPY** project itself. Re-entered Studio, selected Player Start and reopened Starting Equipment. Its primary-hand selection retained the `87a28639...` equipment ID, independently confirming persistence across a complete project close/reopen, in addition to the scene reopen.

## Evidence in the manual

- `docs/manual/PROJECTILES_FIRST_WEAPON.md` — draft 0.3, user-facing tutorial.
- `docs/manual/images/01-player-start.png` through `05-animation-timing.png` — earlier native control/preview inspections.
- `docs/manual/images/06-projectile-saved.png` — freshly saved asset displayed in Weapon firing.
- `docs/manual/images/07-reopened-weapon-firing.png` — reopened scene retained the projectile, spawn point and timing.
- `docs/manual/images/08-reopened-project-equipment.png` — project reopen confirms the new equipment ID.
- `docs/manual/images/09-testlevel-release.png` — first-person firing animation.
- `docs/manual/images/10-testlevel-impact.png` — impact feedback on target.
- `docs/manual/images/11-testlevel-retained.png` — persistent target mark after sideways player movement.

## Evidence limits and outstanding work

- **PASS applies to a new projectile authored from scratch inside a preconfigured bow playground**, not a new weapon setup created from a blank project.
- The live screenshot proves an animation, surface impact feedback and a persistent dark mark. It does **not independently prove the exact 3D arrow embedding orientation or every bone-attachment edge case**; related native tests exist separately.
- Optional target `Surface Type` variations (Glass, Wood, Stone etc.) and specific sound/decal selections were **not exercised in this E2E run**.
- A human creator should follow the published steps without assistance before the manual is marked approved; graphics may benefit from instructional callouts.
- The source-based assistant index does not yet connect to the runtime/model conversation transport. Do not describe this as full in-editor AI integration.
- The screenshots add approximately **703 KB** for six new images, not a new LLM weight. This QA clone itself is ~820 MB **only under BUILD**, not part of Renegade's core shipped package.
- No tracked Studio, EngineBridge or Runtime source files were edited. Nothing committed, pushed or merged. User's original Bow Playground project files were not altered by this test.

**Automated validation reference:** the assistant/manual unittest suite previously passed 16/16 and four focused Release CTest targets passed 4/4; rerun after documentation/fixture modifications before promotion. This receipt is the actual native GUI run, not an assertion about every P3 feature or current `main`.
