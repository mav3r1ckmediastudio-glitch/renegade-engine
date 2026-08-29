from pathlib import Path

root = Path(__file__).resolve().parents[1]
cpp_path = root / "Studio/src/StudioApplication.cpp"
contract_path = root / "Tests/Phase5Gate3SourceContract.cmake"

cpp = cpp_path.read_text(encoding="utf-8")
anchor = "        QueueCreatorImportScaleRuler();\n\n        if (testLevelRuntime_.IsActive())"
replacement = (
    "        QueueCreatorImportScaleRuler();\n\n"
    "        // Gate 3 uses Wicked's native environment-probe debug renderer so Studio\n"
    "        // shows the actual captured cubemap as a reflective sphere together with\n"
    "        // the probe's parallax-correct oriented influence box. Keep this editor-only:\n"
    "        // Test Level runs in a separate Runtime process and the Studio overlay is\n"
    "        // explicitly disabled while that process owns preview execution.\n"
    "        wi::renderer::SetToDrawDebugEnvProbes(\n"
    "            !projectHubVisible_ && !testLevelRuntime_.IsActive());\n\n"
    "        if (testLevelRuntime_.IsActive())"
)
if cpp.count(anchor) != 1:
    raise SystemExit(f"Studio anchor count was {cpp.count(anchor)}, expected 1")
if "SetToDrawDebugEnvProbes(" in cpp:
    raise SystemExit("Studio already contains SetToDrawDebugEnvProbes; refusing duplicate patch")
cpp_path.write_text(cpp.replace(anchor, replacement, 1), encoding="utf-8")

contract = contract_path.read_text(encoding="utf-8")
anchor2 = '    "HandleDecalProbeSceneIcons"\n    "RefreshEnvironmentProbe"'
replacement2 = (
    '    "HandleDecalProbeSceneIcons"\n'
    '    "SetToDrawDebugEnvProbes"\n'
    '    "RefreshEnvironmentProbe"'
)
if contract.count(anchor2) != 1:
    raise SystemExit(f"Contract anchor count was {contract.count(anchor2)}, expected 1")
contract_path.write_text(contract.replace(anchor2, replacement2, 1), encoding="utf-8")
