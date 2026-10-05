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
The current arms and weapon dropdowns enumerate unrelated imported models across
the whole project; dedicated picker filtering is not implemented by PR #178.

- Default Arms selection to an arms collection/folder and Weapon selection to a
  weapons collection/folder, rather than every imported project asset.
- Proposed defaults: `Content/Player/Arms`, `Content/Player/Weapons` and
  `Content/Player/Assemblies`. These are follow-up conventions, not enforced
  directories in the merged implementation.
- Support packs that keep both parts together. Explicit part roles should govern
  picker eligibility independently of physical folder layout; retain an explicit
  browse/import route for other creator layouts. Role metadata is future work.
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
