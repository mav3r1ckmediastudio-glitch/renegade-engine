# LP07 Gate 1 — Common Model Import and FBX Proof

Status: **implementation candidate; final exact-head CI/audit pending**.

Base programme:
`docs/LP07_REUSABLE_PROJECT_ASSET_WORKFLOW.md`

Asset-model authority:
`docs/adr/0004-renegade-asset-model-and-managed-metadata.md`

Wicked pin:
`3a800b7134aafe58461093c8abb2e274d4e64033`

## Purpose

Gate 1 proves that Renegade can own a format-neutral model-import boundary while
reusing the exact pinned Wicked converters and preserving the data required for
FBX-first creator workflows.

This gate deliberately does **not** define the permanent public imported-asset
container. WISCENE is used here as an internal Wicked serialization round-trip
proof. ADR 0004 assigns the later permanent reusable product to Renegade's
versioned `.rasset` abstraction in LP07 Gate 3.

## Production boundary

`ImportService` now exposes a format-neutral contract:

- `ModelSourceFormat` classifies FBX, GLTF, GLB and assessed future formats;
- `ModelImportRequest` carries source path, proof/output path and an optional
  explicit expected format;
- `PrepareModelAsset()` dispatches through a Renegade-owned boundary into the
  accepted Wicked converter;
- `SavePreparedModelAsset()` serializes and reopens the native Wicked scene and
  rejects structural or rig/animation evidence drift; and
- the existing GLB/GLTF entry points remain compatibility wrappers over the
  neutral implementation.

EngineBridge compiles only Wicked's standalone converter translation units used
by Renegade. Gate 1 adds `Editor/ModelImporter_FBX.cpp` beside the already-used
`Editor/ModelImporter_GLTF.cpp`; it does not enable or link the stock Wicked
Editor application or UI.

Accepted current backends:

- FBX -> `wicked.ufbx`;
- GLTF/GLB -> `wicked.gltf`.

The exact pinned FBX converter sets `ufbx_load_opts::target_unit_meters = 1.0f`,
so declared FBX units are normalized into metres by ufbx before Wicked creates
the native scene. This avoids carrying a hidden FBX-centimetres assumption into
the common Renegade model boundary.

The source file is fingerprinted before conversion and the real graphics proof
compares source bytes before and after import. The converter never treats the
external source as a mutable project product.

## Rig and animation preservation evidence

A simple component count is insufficient for an animated/skinned FBX. Gate 1
therefore records deterministic imported/reloaded evidence for:

- skinned mesh count;
- vertices carrying primary and secondary bone influences;
- armature bone count;
- animation component/channel/sampler/data counts;
- keyframe time/value counts; and
- a structural fingerprint built from stable component indices, bone weights,
  inverse bind matrices, animation channels/samplers and keyframe payloads.

The WISCENE round trip is rejected if the imported and reopened evidence differ.

## Immutable real FBX fixtures

The normal Studio CI does not commit or redistribute third-party DCC fixtures in
Renegade. Before CMake configure, it downloads two ASCII FBX files from exact
ufbx commit:

`fcc5d6ba444cfd3eb80677dba5e37e493941abe5`

and verifies their exact Git blob identities with native `git.exe hash-object`:

1. `maya_cube_6100_ascii.fbx`
   - Git blob: `b0aa64ea2361060b4f9bc4cd4e7bd0f730b6f7d8`
   - proof role: real static mesh/material FBX;
2. `maya_transformed_skin_7700_ascii.fbx`
   - Git blob: `683b74cf082e5adab645082c73dd221e0ff7f281`
   - proof role: real Skin/Cluster deformation plus animation stack/curve data.

A hash mismatch fails before configure, so a changed upstream file cannot
silently replace the accepted proof input.

## Real graphics-backed proof

`RenegadeModelImportGraphicsProof` is a dedicated executable linked to the
production EngineBridge. It initializes Wicked's actual Windows DX12 graphics
device and runs the production `ImportService` against both immutable FBXs.

For the static fixture it requires at minimum a mesh, object and material.

For the skinned/animated fixture it additionally requires:

- an armature;
- at least one skinned mesh;
- weighted vertices;
- armature bones;
- animation components;
- animation channels and samplers;
- animation data;
- keyframe times and values.

Both imports are then serialized to WISCENE and reopened. Structural summary and
rig/animation evidence must match exactly, the evidence fingerprint must remain
non-zero, and the original FBX bytes must remain byte-identical.

### Authoritative Release evidence checkpoint

On exact branch head
`10b4f2a6b230fcb814c92f6a2e897c137fc8b870`:

- Renegade Studio run **#253**, workflow run `31493165223`;
- Release job `93784303109`: **SUCCESS**;
- both fixture Git blob checks passed;
- `RenegadeModelImportGraphicsProof`: **Passed, 0.22 s**;
- Release CTest: **43/43 passed, 100%**;
- `RenegadeStandalonePackageTests`: passed;
- LP05 canonical graph remained
  `23b67f63099293d79a239997730b287f157fb38e5421aecb5505e0ca42c84384`,
  4681 bytes; and
- LC01 canonical registry remained
  `547a26c09e6a74394cc9bc67885070272928f5af14d736e8e086d022f0aeea0e`,
  2180 bytes.

This is real conversion/serialization evidence, not merely compilation of the
FBX converter.

## Hosted Debug DX12 capability boundary

The same proof executable is compiled and linked in Debug. On GitHub's hosted
Debug virtual graphics environment, real FBX conversion reaches Wicked's native
GPU resource creation and then asserts in the pinned DX12 backend at
`wiGraphicsDevice_DX12.cpp` while `ID3D12Device::CreatePlacedResource()` is
allocating GPU-backed mesh buffers.

The earlier full-application harness also encountered the already-known hosted
XAudio2 absence. Gate 1 removed that unrelated dependency by initializing only
the job system and real Wicked graphics device. The remaining failure is
therefore the exact native graphics allocation path required by the importer.

The pinned Wicked DX12 adapter-selection code explicitly skips adapters carrying
`DXGI_ADAPTER_FLAG_SOFTWARE`, including the Basic Render Driver. There is no
supported WARP switch available to this proof without changing Wicked.

Gate 1 therefore uses this evidence policy:

- the real proof executable must compile/link in Debug and Release;
- format-neutral/headless `ImportService` contract tests remain mandatory in
  Debug and Release;
- the real GPU-backed FBX behavioural CTest is mandatory in Release;
- Debug hosted CI does not execute that one GPU-backed CTest; and
- no Wicked patch, WARP hack or false-green importer skip is introduced.

The configuration restriction is local to `RenegadeModelImportGraphicsProof`;
it does not broaden the existing hosted skip policy for unrelated tests.

## Upstream converter error-surface limitation

The pinned Wicked FBX and GLTF converter functions are legacy `void` APIs. On a
low-level parse/read failure they call `wi::helper::messageBox()` internally
before returning. Wicked exposes no message-box redirection/suppression callback
at the pinned helper API.

This does **not** mean Renegade links or exposes the stock Wicked Editor UI, and
successful imports remain driven entirely through the Renegade service boundary.
It does mean that the upstream converter seam is not yet suitable for a fully
non-modal background import transaction on malformed input.

Gate 1 records this explicitly rather than hiding it. Before LP07 Gate 3 makes
model import a permanent creator-facing `.rasset` transaction, expected importer
failures must be mediated into Renegade-owned structured/non-modal errors. If
that cannot be achieved safely through an adapter around the pinned converters,
a narrowly justified Wicked/core change must be proposed and reviewed before
moving the submodule pin. No such Wicked change is included in Gate 1.

## Secondary-format seam assessment

Gate 1 distinguishes an upstream converter seam from Renegade product support.

### OBJ

Pinned Wicked declares and implements a dedicated `ImportModel_OBJ()` converter
using its existing OBJ import stack. The seam is suitable for the common
Renegade contract, but Gate 1 does not compile/test/accept OBJ as a Renegade
supported format yet.

### PLY

Pinned Wicked declares and implements a dedicated `ImportModel_PLY()` converter,
including ordinary PLY scene conversion and upstream Gaussian-splat handling.
The seam is suitable for later integration, but Gate 1 does not compile/test/
accept PLY as a Renegade supported format yet.

### VRM / VRMA

At the exact Wicked pin, Editor routing sends `.vrm` and `.vrma` through the
same `ImportModel_GLTF()` converter used for `.gltf`/`.glb`; there is no separate
VRM converter function in `ModelImporter.h`.

That is a real upstream route, but it is **not** sufficient to claim Renegade
VRM/VRMA support. Renegade must explicitly enable the extensions through its
neutral contract and provide representative behavioural evidence in a later
bounded gate.

### Assimp

No Gate 1 capability gap has justified Assimp. The accepted Wicked/ufbx FBX path
has now completed a real skinned/animated Release round trip. Assimp therefore
remains an evidence-driven fallback under the LP07 policy rather than a parallel
import stack.

## Gate 1 exclusions retained

This gate does not implement:

- the `.rasset` persistent container/transaction;
- registry-backed creator catalogue;
- reimport;
- Asset Browser UI changes;
- `.rentity` authoring;
- animation timeline/state-machine/retargeting UI;
- scene-format migration;
- cooked-package encryption; or
- additional third-party importer libraries.

## Final close-out still required

Before Gate 1 is accepted, the final exact PR head must still prove:

- Debug Studio CI green under the documented configuration policy;
- Release Studio CI green with the real FBX proof mandatory;
- Debug/Release pinned-Wicked baseline green;
- no Wicked source/submodule change;
- no unrelated build-wrapper change; and
- independent exact-head review/audit.
