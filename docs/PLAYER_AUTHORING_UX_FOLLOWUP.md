# Player authoring UI/UX follow-up

PR #178 merged into main on 2026-10-05 at `7105a95ddcc103d6a024a17fddefd62f705a86b3`.
All four pre-merge Windows checks passed: Studio Debug/Release and baseline
Debug/Release. The owner confirmed the expected player/shotgun behaviour in the
existing v2 game project and explicitly accepted the PR's functionality.
This records functional acceptance of the merged scope, not completion of every
P1 requirement or of the full Alpha Playability combat programme.

## Required UI/UX follow-up (owner request, 2026-10-05)

The owner accepts the functionality but reports poor setup UX. Revisit the player
Inspector, prefab placement and Assembly workflow in a separate bounded slice.
PR #178 enumerated unrelated imported models across the whole project. The first
P2 implementation slice now filters each picker by dedicated folder or saved
assembly part role. This addresses the immediate selection problem; the broader
workflow revision remains open.

- Default Arms selection to an arms collection/folder and Weapon selection to a
  weapons collection/folder, rather than every imported project asset.
- Proposed defaults: `Content/Player/Arms`, `Content/Player/Weapons` and
  `Content/Player/Assemblies`. The first P2 slice recognizes Arms/Weapons folders without moving legacy parts.
  Assemblies remains a proposed organization convention.
- Support packs that keep both parts together. Explicit part roles should govern
  picker eligibility independently of physical folder layout; retain an explicit
  browse/import route for other creator layouts. Saved assembly provenance supplies legacy/shared-pack roles in the first P2 slice;
  explicit new-pack role authoring and browse/import remain future work.
- Show readable names and clearly distinguish raw parts, completed assemblies and
  player prefabs. Applying a finished prefab should not require rebuilding it.
- Provide complete cross-project import/transfer of the prefab, assembly, authoring
  parts, textures, animation sources, registration and project identity. Copying
  only `.rplayerprefab` files is insufficient. The v2 transfer was manually
  prepared and verified; it is not an existing one-click migration feature.
- Preserve the last valid preview and automatic refresh. Debounced refresh is
  already implemented; verify its loading/error feedback during the UX revision.

Acceptance: in a project containing many unrelated models, pick the intended
arms/weapon without trawling the whole project; also load a combined-folder pack,
place/apply a finished player, save/reopen, and verify Test Level and packaged
Runtime. The UX follow-up does not block the accepted PR #178 functionality.

See [P2 implementation](P2_EQUIPMENT_ACTION_IMPLEMENTATION.md) for current scope
and remaining work. Native filtered selections and restored preview were checked
in an isolated copy of the accepted shotgun project.
