# Renegade Runtime

The standalone Windows game/player executable, separate from Studio editor code.
Current integrated baseline: main 618b231, 10 November 2026.

Runtime consumes governed project/Story Flow or package-relative startup, native
WISCENE and companion documents. It owns gameplay lifecycle, Player/Character,
input, audio, inventory/Screen presentation, equipment/projectiles, weather and
governed creator Lua. Native effect/material resources travel through the same
project/dependency pipeline used by Test Game and Build Game.

General prefab instances retain native components and remapped script references.
Packaged reusable validation preserves authored assembly copies and unpacked
contents rather than replacing edited instances from their original product.
The scripted-brazier package launched with integrity PASS and four script instances
started; this is a scoped proof, not universal gameplay acceptance.

Explicit --project remains the project/Test Game path. A named standalone package
uses its governed project/content/support manifests and executable identity;
staging alone is not the completed package-relative launch step. Runtime code does
not require the editor or an online marketplace to execute project-owned scripts.

See [current status](../docs/CURRENT_STATUS.md), [build guide](../docs/BUILD_WINDOWS.md),
[architecture](../docs/ARCHITECTURE.md) and
[prefab evidence](../docs/evidence/scene-prefab-functional/README.md).
LP01/LP02/LP03 READMEs are historical proof records.
