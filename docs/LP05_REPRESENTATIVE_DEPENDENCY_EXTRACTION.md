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

