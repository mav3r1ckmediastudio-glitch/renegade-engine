# EngineBridge

UI-independent services and adapters around pinned Wicked facilities. Current
integrated baseline: main 618b231, 10 November 2026.

| Responsibility | Existing authorities |
|---|---|
| Project/scene documents | ProjectService, SceneService, SceneDocumentService, StudioSession and document transactions |
| Identity and history | Stable IDs, SelectionService, CommandService and component-specific commands |
| Import/asset lifecycle | Native import candidates, reusable products/projections, catalogue, registry and provenance services |
| Assemblies | SceneAssemblyService, prefab serialization and SceneAssemblyScriptService |
| Script authoring | Typed ScriptDocument services, metadata/properties/references and Library adoption |
| Build closure | Dependency/asset discovery, staging, executable identity, integrity and promotion services |
| World/render/gameplay state | Shared curated service state used by Studio commands and Runtime hydration |

Preserve native component fields outside each curated edit. A normal command must
not erase unrelated settings. General prefab save uses shared native entity remaps
for components/resources and typed companion templates for governed scripts;
placement/history use the existing companion transaction seams.

Do not call renderer-dependent scene factories, mesh creation or Scene::Update
from a headless test. Native archive/import paths can require an initialized device.
Whole-scene discovery belongs at explicit lifecycle/content changes, while point
queries use native lookups or a validated index.

These are internal supported project service boundaries, not a released general
native plugin ABI. See [architecture](../docs/ARCHITECTURE.md),
[current status](../docs/CURRENT_STATUS.md) and
[script Library](../docs/SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE.md).
