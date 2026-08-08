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
