# LP05 — Representative Dependency Extraction

## Proof boundary

LP05 proves that Renegade can discover a deterministic, machine-readable
transitive dependency closure for representative project content. It produces
graph data and diagnostics only. It does not copy, cook, repair or package
assets; LC01 consumes the accepted graph and LP06 owns standalone building.

## Architectural constraint

WISCENE dependencies are extracted by a Renegade-owned, read-only typed walker
using Wicked's public scene/ECS component query APIs. The walker reads the
authoritative resource fields on native components. LP05 must not modify Wicked
source, expose or intercept `EntitySerializer::resource_registration`, parse
WISCENE binary data independently, or treat arbitrary path-looking strings as
dependencies. If a required dependency is not observable through the public
component surface, that is an evidenced blocker requiring a separate decision.

## Required representative closure

The fixture grows with each provider and ultimately contains:

- the startup WISCENE and a second Story Flow-reachable WISCENE;
- GLTF content with an external BIN, multiple material texture slots and
  animation content;
- sky, colour-grading LUT, cloud, lens-flare and environment-probe textures;
- audio and video scene components;
- Runtime Screen image and font references;
- a root Lua script and one explicitly declared nested script dependency;
- one computed, undeclared `dofile` target, which must diagnose predictably;
- one picker-backed gameplay/data reference and one Always Include asset;
- one path-looking metadata string, which must not become an edge;
- terrain default materials and representative generated data;
- deliberately missing, outside-project, duplicate and case-collision cases.

## Graph contract

Every node has stable identity, canonical project-relative path, dependency
class, requirement, platform/backend applicability, owning provider/version and
content hash. Every edge records source, target and typed provenance. Runtime
support files are distinguishable from project content. Diagnostics identify
missing files, outside-project paths, collisions and statically undiscoverable
computed references. Ordering and logical output are deterministic.

## Pass conditions

1. Every expected runtime dependency is found without manual guessing.
2. The computed undeclared Lua dependency fails predictably.
3. Unrelated assets and path-looking metadata are excluded.
4. Repeated extraction yields identical logical output.
5. LC01 can consume the graph without LP05 copying or cooking anything.

## Incremental gates

1. Contract, fixture manifest, graph schema and canonical path/security layer.
2. Roots and provider interface.
3. Project, Story Flow, Runtime Screen and declared-reference providers.
4. Read-only WISCENE typed walker, extended one component class at a time.
5. Imported-content, terrain/generated-data and Always Include providers.
6. Lua policy boundary and complete negative-case diagnostics.
7. Deterministic transitive closure and machine-readable serialization.
8. Repeatability, Debug/Release CI and packaged read-only proof.

## Gate 2 implementation boundary

Gate 2 admits canonical project-relative roots and defines the UI-free
`IDependencyProvider` contract. Providers declare a stable name and version,
the dependency classes they support, and typed candidates with provenance.
`DependencyCollector` dispatches providers in stable name order and applies a
provider's candidates only after that provider succeeds, preventing a failed
provider from leaving a partial graph mutation. Concrete project, Story Flow,
Runtime Screen and declared-reference providers remain Gate 3 work.

Path resolution deliberately keeps two forms. A filesystem-resolved absolute
path is authoritative for containment, symlink escape prevention, existence
and reads. A lexically normalized UTF-8 project-relative declaration retains
the provider's spelling and is authoritative for graph identity and collision
diagnostics. On the Windows x64 target, case equivalence is non-linguistic
Unicode ordinal comparison through `CompareStringOrdinal(..., TRUE)`; it is
not byte-wise `tolower`, locale folding or Unicode normalization. The registry
returns the first registered spelling, so every duplicate/collision edge reuses
the same stable graph node without a second collector-side comparison.

## Gate 3 implementation boundary

Gate 3 adds concrete providers for the validated project descriptor, Story
Flow, Runtime Screen and explicit declarations. Production reader adapters use
`ProjectService`, `ReadFlowDocument` and `ReadScreenDocument`; providers consume
only typed document views and never scan arbitrary strings. Project extraction
retains a declared missing startup-scene edge for diagnostics instead of making
document inspection fail. Story Flow emits only Level-node scene path hints.
Runtime Screen emits only authored Image resources; the provider schema also
reserves typed font resources for the later custom-font document extension.
Declared references are exact, typed source-to-target records supplied by the
owning authoring or scripting policy, never inferred from Lua source text.

This gate tests each provider at an explicit source root. Recursive traversal
and complete transitive closure remain Gate 7, so Gate 3 does not silently
introduce an incomplete traversal algorithm.

## Gate 4 implementation boundary

Gate 4 adds the `WisceneDependencyProvider` and a production reader that loads
the source scene through Renegade's existing validated
`PrepareWickedSceneOpen` seam. `PreparedSceneOpen` exposes a const-only scene
view for inspection; the active Studio scene is never involved and the source
archive is never written. The dependency contract remains free of Wicked
types.

The first complete native walker reads only these authoritative public fields:

- every `MaterialComponent::textures[*].name` slot;
- `LightComponent::lensFlareNames`;
- `EnvironmentProbeComponent::textureName`;
- the sky, colour-grading and two volumetric-cloud map names on
  `WeatherComponent`;
- `SoundComponent::filename`;
- `VideoComponent::filename`;
- `ScriptComponent::filename`.

These become typed Texture, Audio, Video and Script candidates. Provenance is
deterministic and identifies the component ordinal plus exact field or texture
slot. Empty fields are ignored. Arbitrary metadata strings are never scanned.
Wicked expands serialized resource names against the WISCENE directory while
loading; the provider converts those public absolute field values back to
project-relative declarations lexically before the existing path-security
layer performs authoritative containment, existence and collision checks.

`RenegadeDependencyTests` exercises all seven component classes through the
direct const-scene walker, including Environment Probe. Its real serialized
headless WISCENE covers Material, Light, Weather, Sound, Video and Script;
Environment Probe is omitted from that archive fixture because Wicked creates
its render cubemap during deserialization and therefore requires an initialized
graphics device. The fixture includes repeated texture references, missing
native resources and a path-looking metadata value. It proves typed field
coverage and provenance, stable repeated read order, duplicate node reuse,
missing diagnostics, metadata exclusion and byte-for-byte source-file identity
after extraction. Gate 4 does not add recursive traversal; the scene is still
exercised as an explicit root.

Imported-source/BIN discovery, terrain/generated data and Always Include remain
Gate 5. Lua source policy and computed-target diagnostics remain Gate 6.
Transitive traversal and graph serialization remain Gate 7.

Gate 4's remote implementation commit
`3d888de02c97a83ada27ba4f3d17f8a75053fd53` passed the authoritative
`Renegade Studio` GitHub Actions workflow run 126 in both Debug and Release.
Each configuration passed the complete 23-test CTest suite, including the
extended `RenegadeDependencyTests`. The separate pinned-Wicked baseline run 127
also passed in Debug and Release. This is the accepted build/test proof because
the project owner's local CPU is confirmed unstable under compilation load.

## Gate 2 correction — Windows Unicode path identity

The original Gate 2 resolver used the filesystem-resolved spelling as graph
identity. On case-insensitive Windows filesystems that erased a provider's
declared casing before duplicate/collision registration. The replacement keeps
the resolved absolute path for containment, symlink protection, existence and
reads, while the lexically normalized UTF-8 declaration supplies graph
identity and diagnostics.

The registry owns the single equivalence policy. On Windows it strictly
decodes UTF-8 to UTF-16 and calls `CompareStringOrdinal(..., TRUE)`, the
non-linguistic Windows comparison intended for NTFS filenames. It returns the
first registered spelling to the collector, which hashes that exact spelling
to reuse the existing node. The collector no longer performs a second fold.
Exact UTF-8 equality remains `Duplicate`; ordinal case equivalence with a
different spelling is `CaseCollision`. Invalid UTF-8 fails closed.

Validated on real Windows Debug hardware and via a Linux host syntax check of
the Win32 code path. The host run proves ASCII exact duplicates, existing and
missing ASCII case collisions, node reuse, path security and all Gate 3
assertions; only the owner-side Windows run executes the Unicode fixture
(`Épée.glb` / `épée.glb`) and invalid-UTF-8 rejection.

## Gate 2 correction — declared-path canonicalization leak

The Unicode/case-identity fix above was correct in intent but had one
remaining bug: it computed the declared-path identity with
`std::filesystem::relative(lexical, root, error)`. That two-argument free
function is defined by the standard as
`weakly_canonical(p).lexically_relative(weakly_canonical(base))` — it
silently canonicalizes both arguments against the filesystem, even though
`lexical` was deliberately built to avoid that. On a case-insensitive Windows
volume this let canonicalization quietly rewrite a declared spelling like
`Content/Textures/stone.png` back to the on-disk `Stone.png`, reintroducing
the exact bug the fix exists to prevent for any path that already exists on
disk. It went undetected in Linux host validation because Linux filesystems
are case-sensitive, so `stone.png` never case-insensitively resolves to
`Stone.png` there. It reproduced as a live test failure —
`RenegadeDependencyTests: existing dependency declaration casing was not
preserved` — on the real Windows Debug build.

Fix: swap the canonicalizing free function for the purely-lexical member
function, which never touches the filesystem:

```cpp
const auto declaredRelative =
    lexical.lexically_relative(root).lexically_normal();
```

Validated on Windows: full clean Debug build, `RenegadeDependencyTests` and
all `RenegadeBridgeTests`/suite tests passing, repeated twice from a fresh
configure with no regressions.

## Release build — CL.exe access-violation, resolved as local hardware fault

A separate, pre-existing issue blocked Release builds during this gate's
development: Release `/O2` builds intermittently crashed with `CL.exe exited
with code -1073741819` (STATUS_ACCESS_VIOLATION). Debug was unaffected
throughout.

Two rounds of toolchain investigation — upgrading the MSVC toolset, and
applying `CMAKE_POLICY_DEFAULT_CMP0141 NEW` plus
`CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded` for Release (`/Z7`, on the
theory the crash was PDB type-server interaction) — did not resolve it; the
crash kept recurring in a different translation unit each time. A further
session eliminated every remaining software variable on the local machine
(OneDrive sync stopped, zero orphaned build processes, `--parallel 1`) and
the crash still reproduced. Windows Event Viewer showed why: the `CL.exe`
access violations landed in the same second as
`Microsoft-Windows-WHEA-Logger` corrected machine-check errors (internal
parity errors) on the CPU. A 14-day WHEA history showed 6 such corrected
errors, 4 of them clustered in a single 25-minute window across three
different CPU cores, matching every local Release attempt made that night.
BIOS and CPU microcode were both confirmed current. This is local hardware
instability, not a compiler, CMake or code defect, and is being pursued
separately with the hardware vendor.

Release was verified sound by running the `Renegade Studio` GitHub Actions
workflow (`studio.yml`, not the WickedEngine-only `windows-baseline.yml`,
which does not build or test Renegade's own code) against this branch. Both
Debug and Release jobs passed 23/23 tests, `RenegadeDependencyTests`
included, no crash. This satisfies Release proof for LP05 Gates 1-3 on CI
hardware. The `/Z7` CMake change is retained as harmless and directionally
reasonable but was never the actual fix; local Release builds on the
affected machine remain unreliable until the hardware issue is resolved.
