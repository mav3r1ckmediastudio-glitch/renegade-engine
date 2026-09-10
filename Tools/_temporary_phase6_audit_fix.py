from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, found {count}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


# CreateTerrain binds/materializes bundled terrain resources and is intentionally
# not a headless fixture. Keep the executable Terrain test GPU-free and prove
# the world-zero creation code through the already-registered source contract.
replace_once(
    "Tests/TerrainTests.cpp",
    """    // Fresh authored terrain keeps its internal -20..+120 sculpt envelope but
    // maps bottomLevel to world Y=0 through the terrain root transform.
    wi::scene::Scene authoredScene;
    const auto environment = wi::ecs::CreateEntity();
    authoredScene.weathers.Create(environment);
    const auto authoredTerrainEntity = renegade::bridge::CreateTerrain(
        authoredScene, renegade::bridge::TerrainState{}, "World Zero Terrain");
    const auto* authoredTerrain =
        authoredScene.terrains.GetComponent(authoredTerrainEntity);
    const auto* authoredTransform =
        authoredScene.transforms.GetComponent(authoredTerrainEntity);
    if (authoredTerrain == nullptr || authoredTransform == nullptr ||
        !NearlyEqual(authoredTerrain->bottomLevel, -20.0f) ||
        !NearlyEqual(authoredTransform->translation_local.y, 20.0f) ||
        authoredTerrain->physics_generation != 10)
    {
        return Fail("fresh terrain did not map its -20 m local baseline to world Y=0 with full physics coverage");
    }

""",
    "",
)

replace_once(
    "Tests/Phase6NativeNavigationSourceContract.cmake",
    """file(READ
    "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeLiveDiagnostics.cpp"
    runtime_source)

""",
    """file(READ
    "${RENEGADE_SOURCE_DIR}/Runtime/src/RuntimeLiveDiagnostics.cpp"
    runtime_source)
file(READ
    "${RENEGADE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/TerrainService.h"
    terrain_header)
file(READ
    "${RENEGADE_SOURCE_DIR}/EngineBridge/src/TerrainService.cpp"
    terrain_source)
file(READ
    "${RENEGADE_SOURCE_DIR}/Studio/src/StudioApplication.cpp"
    studio_application)

""",
)

replace_once(
    "Tests/Phase6NativeNavigationSourceContract.cmake",
    'message(STATUS "Phase 6 native navigation source contract passed")',
    """require_text("${terrain_header}"
    "physicsChunkRadius = DefaultTerrainChunkRadius + 1"
    "full fixed-terrain physics default")
require_text("${terrain_source}"
    "std::max(requestedPhysicsRadius, terrain.generation + 1)"
    "full fixed-terrain Jolt heightfield coverage")
require_text("${terrain_source}"
    "rootTransform->translation_local.y = -terrain.bottomLevel"
    "fresh terrain world-zero baseline mapping")
require_text("${studio_application}"
    "constexpr float gridPlaneHeight = 0.02f;"
    "shader grid restored to world-zero reference")

message(STATUS "Phase 6 native navigation + terrain foundation source contract passed")""",
)

for temporary in (
    ".github/workflows/_temporary-phase6-repair.yml",
    ".github/workflows/_temporary-phase6-repair-v2.yml",
    "Tools/_temporary_phase6_repair.py",
):
    if Path(temporary).exists():
        raise RuntimeError(f"temporary repair artifact unexpectedly remains: {temporary}")

print("Headless/source-contract audit correction passed")
