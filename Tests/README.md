# Renegade tests

The suite is a CTest graph of multiple executables, source/resource contracts and
Python/Node checks, not a single RenegadeBridgeTests process. Current integrated
baseline: main 618b231, 10 November 2026. Tests/CMakeLists.txt and configured
ctest -N output are the registration authority.

RenegadeBridgeTests remains a headless test/aggregate build foundation. New proof
targets can be build dependencies without being registered to execute on a hosted
GPU-less runner. Native/GPU proofs explicitly create a graphics device; actual
Studio, save/reopen and standalone visual checks remain separate evidence.

- Headless tests must not call renderer-dependent primitive factories or advance
  rendered scenes. Test pure state/commands or renderer-independent blueprints.
- Native tests verify scene/component/material/resource/history and disk behaviour
  with an initialized device where required.
- Source/resource contracts detect ownership, deployment and schema regressions;
  they do not establish artistic quality.
- Assistant tests enforce source fingerprints, manual/receipt boundaries,
  packaged identity and abstention. Do not renew fingerprints without reviewing
  the affected guide/controls.
- Functional assembly proofs cover geometry and a real scripted brazier, including
  independent copies, reference remapping, Unpack and package loading.

Run local tests using the configured build; see
[BUILD_WINDOWS](../docs/BUILD_WINDOWS.md) and
[TEST_STRATEGY](../docs/TEST_STRATEGY.md).
The documentation reconciliation suppresses hosted CI by owner instruction;
local doc/assistant/source checks are not a new binary build or release gate.
