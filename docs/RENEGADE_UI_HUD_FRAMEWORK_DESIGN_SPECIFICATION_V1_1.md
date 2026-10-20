# Renegade UI/HUD Framework Design Specification v1.1

**Status:** owner-review draft; no implementation authority.
**Prepared:** 16 October 2026.
**Repository baseline:** `feature/p3-original-impact-vfx`, HEAD `60634cec26feb7ffc2d0f941d3381865fa3b41c7`, with pre-existing local modifications.
**Scope:** Windows x64 / DX12; Vulkan remains the existing Windows cross-check.
**Decision source:** owner-supplied UI-001–UI-019 register from *Projectile Work*, conversation `6ad13218-50d0-83eb-b32f-893c74dd7845`, plus supplied detailed UI-015–UI-019 text.

## 1. Authority, evidence and review boundary

The 25 owner-approved design decisions are product requirements (UI-001–UI-019 as originally recorded, plus the UI-020–UI-025 continuation below). They do not establish that any feature is implemented. This document proposes the architecture and acceptance contract needed to deliver them. Owner approval of this specification must precede implementation, migrations or replacement of existing paths.

Labels used throughout:

- **Existing:** observed in the inspected source or recorded canonical documentation. Static inspection is not a fresh execution or acceptance result.
- **Required:** follows from the supplied owner-approved decision register.
- **Proposed:** a concrete elaboration for owner review; not claimed as wording approved in an earlier session.
- **Unverified:** requires execution, fuller source tracing or owner confirmation before implementation can depend on it.

The conversation reader returned only sessions 16–19 and opaque assistant references. The owner subsequently supplied the complete decision register and detailed sessions 15–19. Earlier decisions are therefore traced to their supplied titles; detailed mechanics below are proposals, not reconstructed quotations.

This review read `AGENTS.md`, `README.md`, the charter, architecture, roadmap, handoff and relevant feature-matrix entries, then followed Screen, renderer, Runtime and authoring sources. Canonical files contain historical checkpoints and local candidates; no single historical status is treated as proof of current release acceptance. Existing local changes include Studio startup/chrome/Test Level work and additions to architecture, roadmap, feature matrix and handoff. They remain outside this task.

No playground, project, model, executable or service is changed or launched by this design task. No rewrite, implementation, commit, merge or release-gate closure is authorized. The document's handoff section records this task without editing the already-modified shared handoff.

## 2. Decision-to-requirement register

| Decision | Owner-approved requirement | Specification sections | Acceptance gates |
|---|---|---|---|
| UI-001 | Hybrid templates and blank canvas | 6, 9 | G2, G7 |
| UI-002 | Combined canvas and Inspector editing | 6 | G2 |
| UI-003 | Professional presets and advanced animation | 8 | G4, G7 |
| UI-004 | Explicit manual gameplay bindings | 5, 9 | G1, G5 |
| UI-005 | Inspector attributes and reusable profiles | 5 | G1 |
| UI-006 | Simple attributes with expandable gameplay rules | 5 | G1, G5 |
| UI-007 | Contextual visibility and animated transitions | 7, 8 | G3, G4 |
| UI-008 | Smart responsive layouts with full manual control | 6, 7 | G2, G6 |
| UI-009 | Linked and independent reusable components | 9 | G5 |
| UI-010 | Reusable interactions and optional gameplay modules | 5, 6 | G1, G5 |
| UI-011 | Inspector action lists and optional conditions | 5, 7 | G1, G3 |
| UI-012 | StoryFlow-managed screens with configurable composition | 4, 7 | G3 |
| UI-013 | Per-screen gameplay, input and pause behaviour | 7 | G3, G7 |
| UI-014 | Professional screen transition presets and custom animation | 8 | G4 |
| UI-015 | Global themes with individual overrides | 9 | G5, G6 |
| UI-016 | Design preview, simulation and live gameplay testing | 6, 10 | G2, G7 |
| UI-017 | Accessibility foundations with creator control | 10 | G6, G7 |
| UI-018 | Intelligent optimisation and configurable performance budgets | 11 | G6, G7 |
| UI-019 | Project assets and optional personal creator library | 9 | G5, G7 |

## 3. Current-state assessment and evidence

References are repository-relative so the specification remains portable under `docs/`. Symbol names identify the inspected seam where line numbers may move during ongoing work.

| Evidence | Existing implementation or documented constraint | Implication and limit |
|---|---|---|
| E1: `AGENTS.md`; `docs/PROJECT_CHARTER.md`; `docs/ARCHITECTURE.md`, Layers / UI foundation | Bridge services separate creator UI from Wicked; native wiGUI integration is accepted; Renegade owns visual presentation; Wicked is pinned | Extend accepted integration. A backend replacement requires a separate decision and measured justification. |
| E2: `EngineBridge/include/renegade/bridge/ScreenService.h`, `ScreenDocument`, `ScreenWidget` | Schema v2; Image/Text/Button primitives; parent IDs; Absolute/Anchored layout; Fit/Fill/Stretch canvas; five visual states; text metrics; symbolic actions and focus order | A useful retained document foundation, not yet a complete gameplay HUD framework. |
| E3: `EngineBridge/src/ScreenService.cpp`, `ValidateScreenDocument`, `ReadScreenDocument`, `WriteScreenDocument` | Project identity and structural validation; v1 migration; transactional write path; currently 1–64 actions, 1–256 widgets, at least one button and one visible enabled control | Passive HUDs cannot simply use the current menu validator. Introduce explicit usage semantics in a compatible versioned extension. Existing limits are safety constraints, not future universal creative budgets. |
| E4: `EngineBridge/include/renegade/bridge/ScreenAuthoringSession.h`; `EngineBridge/src/ScreenAuthoringSession.cpp` | Candidate-document validation, bounded history, coalesced manipulation, Save/Reload, subtree duplication, action edits, reparenting | Preserve one mutation authority. Existing subtree cloning is not evidence of linked component assets. |
| E5: `ScreenRenderer/include/renegade/screen/ScreenRenderer.h`; `ScreenRenderer/src/ScreenRenderer.cpp` | Shared presenter; GPU-independent `BuildScreenRenderItems` and viewport projection; authored state/layout; input suppression for editor preview; custom `ScreenSurface` over wiGUI | Preserve exact preview/Runtime presentation seam. No evidence here of a general binding evaluator, linked theme resolver, timeline or multi-screen compositor. |
| E6: `Runtime/src/RuntimeScreen.h`; `Runtime/src/RuntimeScreen.cpp` | Controller owns focus and action requests; presenter delegates drawing to shared renderer | Extend orchestration without putting gameplay state or Flow destinations in renderer widgets. |
| E7: `docs/STORY_FLOW_GATE8E_OUTCOME_PARITY.md`; `EngineBridge/src/StoryFlowScreenReferenceService.cpp` | Screen actions are symbolic; Flow owns destinations; parity audit diagnoses stale/unrouted button outcomes | Preserve routing ownership and explicit repair. Future local HUD commands require distinct semantics, not fake Flow routes. |
| E8: `docs/STORY_FLOW_GATE8C_SCREEN_EDITOR_SHELL.md`; `docs/STORY_FLOW_GATE8D_CREATOR_CONTROLS.md`; `Studio/src/RenegadeScreenEditorWorkspace.*` | Dedicated Screen workspace, shared preview, Basic/Advanced controls, direct manipulation and governed pickers | Evolve discoverability and workflow; do not assume that the documented Gate 8D aspiration for typed binding already supplies generic gameplay data binding. |
| E9: Gate 8D governed-resource section; `EngineBridge/include/renegade/bridge/ScreenCreatorResourceService.h` | Picker carries governed product identity, while v2 Screen fields still persist project-contained source-path hints | Stable resource persistence is a migration requirement. A safe path alone does not establish asset identity. |
| E10: `Runtime/src/RuntimeApplication.cpp`, `RuntimeRenderPath::Compose` / `SetInteractionPrompt` | Interaction prompt and paused presentation are directly drawn in Runtime | Migrate incrementally after parity proof; preserve gameplay producers and lifecycle. These are existing presentation paths, not a unified authored HUD. |
| E11: `EngineBridge/include/renegade/bridge/GameplayEventService.h`; architecture dependency-extraction and frame-loop sections | Bounded gameplay event queue; governed dependency collection; revision-gated frame work | Reuse services where appropriate. Events with string payloads are not a typed attribute store. No repeated file scans or resource rehydration in UI frame updates. |
| E12: `docs/ROADMAP.md`, Gameplay HUD / UI authoring; `docs/PLAYER_ARMS_COMBAT_FRAMEWORK.md`; README combat limitations | HUD must join Screen/Journey and real combat loop; several health/resource/combat capabilities remain programme work | Health, ammo and crossbow adapters must be audited against actual current gameplay. A design example does not establish available data. |
| E13: `Tests/ScreenTests.cpp`, `ScreenAuthoringSessionTests.cpp`, `ScreenRendererTests.cpp`, `RuntimeScreenTests.cpp`, `StoryFlowScreenSemanticsTests.cpp`; Gate 8E and Gate 10 docs | Existing validation, history, projection, focus, route and build regression foundations | Retain these regressions. They were inspected, not rerun for this documentation task. |
| E14: `docs/FEATURE_MATRIX.csv`, `REN-FLW-001`, `REN-UI-001`, `REN-PLY-001`, `REN-EQP-001`, `REN-PRJTL-001`; `HANDOFF.md` | Capability claims are bounded, dated and sometimes pending exact-head/owner verification | Do not label the new framework complete or upgrade current acceptance from this review. |

## 4. Preserve, refactor, extend and replace boundaries

| Treatment | Boundary | Required proof before change |
|---|---|---|
| Preserve | Project/document stable IDs, Flow interpreter/routes, Journey/Graph semantic identity, governed Level/Screen lifecycle | Existing journeys reopen and execute unchanged. Layout never changes route semantics. |
| Preserve | Screen mutation/history/save authority; schema-v1/v2 reading; authored styling and ordering | Golden old documents retain appearance/actions; no automatic save migration. |
| Preserve | Wicked renderer, ECS, physics, native scene ownership; player/controller/equipment authority; governed input and script lifecycle | No second world, controller, damage store or raw input stack. No Wicked pointer/source change. |
| Extend | Screen document contract and validation | Versioned optional bindings, usage, theme/component references and layout/animation data; explicit migration report and rollback. |
| Refactor incrementally | Shared frame projection/presenter for resolved multi-screen state | Same old-document output, hit regions and navigation; measured reason for any internal split. |
| Extend | Story Flow authoring and Runtime orchestration | Screen composition is explicit while destinations remain Flow-owned. |
| Replace only after parity gate | Direct Runtime prompt/pause drawing and any identified ad hoc HUD presentation | Equivalent data, timing, appearance, pause/reset, input and package behaviour; owner visual approval. Keep old path until replacement passes. |
| New | Typed attributes/adapters, binding diagnostics, linked assets/themes, simulation, composition and animation contracts | Approved schemas, ownership and tests before integration. |

Whole-StoryFlow, whole-Studio and whole-renderer rewrites are outside this specification. New services below are proposed responsibilities, not claims that named APIs or classes exist.

Proposed data path:

```text
Gameplay authorities -> typed attribute adapters -> versioned UI snapshot
Project Screen + components + themes + bindings -> resolved UI instance
Flow context + active instances -> composition/input/pause coordinator
Resolved instance + snapshot -> layout/state/animation -> shared Screen renderer
UI action -> governed command/event or symbolic Flow outcome -> owning service
```

EngineBridge owns UI-independent documents, validation, binding descriptions and reusable evaluation. Runtime owns live instances, gameplay adapters, action dispatch and lifecycle. Studio owns authoring tools and simulation controls. `ScreenRenderer` owns Wicked-backed presentation, not gameplay or routing policy. Build tools consume explicit dependency edges for all new references.

## 5. Gameplay attributes, bindings, rules and actions

### 5.1 Attributes and profiles

**Required UI-005/006:** expose attributes through Inspectors and reusable profiles, beginning simply and allowing richer gameplay rules. **Proposed:** each definition contains stable ID, readable name, type, default, optional numeric bounds/unit, ownership scope and supported operations. Initial useful types are numeric, boolean, text and enumerated state; entity/asset references require existing governed identity handling.

Profiles supply authored defaults and optional rule configuration; each Player/Character instance owns its live values. A profile edit must show affected instances and local overrides. Built-in values such as weapon loaded count must be adapters over authoritative equipment state, never an independently maintained UI counter. Generic creator attributes are gameplay-owned data and must remain useful without a HUD.

Simple numeric configuration supports an initial value and optional minimum/maximum. Regeneration, costs, damage/death consequences, cooldowns and dependencies are optional gameplay rules, not compulsory health-bar settings. Rules execute once in the gameplay domain with deterministic ordering, bounded work and cycle diagnostics. The HUD cannot introduce its own damage, ammo or death authority. Save-game persistence of live values needs a separate explicit gameplay contract; Screen persistence stores definitions and references, not captured live health.

### 5.2 Explicit binding contract

**Required UI-004:** every binding is chosen explicitly. Proposed binding fields are: target element/property; source scope and stable target; attribute ID/type; approved formatter or transform; missing-source policy; optional sample value; update semantics. A readable binding might say `HealthBar.fill <- explicitly selected Player / health.current / health.maximum`.

The picker shows only compatible sources and explains unavailable ones. Selecting “current possessed Player” is an explicit role choice, not permission to guess the nearest entity. Character/world targets use generation-safe identity and must handle despawn, replacement and scene reset. Binding expressions are bounded typed operations; arbitrary source-code evaluation is not implicit.

Bindings are one-way by default. A slider changing gameplay sends a validated command to its owning service with capability checks; assigning a visual value never writes gameplay. Text formatting, ratios, thresholds and visibility predicates are presentation operations. Zero maximum, NaN, missing source, type change and stale target produce defined fallback plus structured diagnostics, never fabricated healthy/ammo values. Required invalid bindings block readiness/package validation; optional decorative bindings may fall back with an explicit warning. Authoring drafts may remain incomplete.

Snapshot once per gameplay revision/frame boundary so related HUD values are coherent. Subscribe or dirty-check rather than enumerate all entities each frame. Reset/scene switch clears subscriptions, transient animation and stale target caches. Preserve pause semantics and reject late callbacks from previous sessions.

### 5.3 Interactions, modules and Inspector action lists

**Required UI-010/011:** reusable interactions, optional gameplay modules, Inspector action lists and optional conditions. Proposed interaction assets contain ordered typed actions, optional conditions and explicit parameters. Conditions have no side effects. Actions declare their execution domain: local UI state, governed gameplay command/event, or symbolic Flow outcome.

Opening an inventory, switching an equipment slot and changing a Flow destination are distinct operations. Flow remains the destination authority. Optional inventory/resource/quest modules expose capabilities and do not become dependencies of every UI document. Reusable interactions must work without forcing those modules to exist. Use governed Lua/events where appropriate; do not add unrestricted Lua execution or arbitrary filesystem/network actions through UI bindings.

Validate the whole action list before dispatch; report failed action and condition. Define cancellation and partial-failure behaviour for non-atomic gameplay operations. Destructive or irreversible operations cannot be made transactional merely by grouping them in an Inspector list. Bound recursion/re-entrant action chains and sequence requests to avoid double activation.

## 6. Creator UX and iteration

**Required UI-001/002:** create from a professional editable template or blank canvas, with direct canvas editing and Inspector precision. Proposed dedicated UI Designer evolves the existing Screen workspace: left asset/element hierarchy, central shared-renderer canvas, right contextual Inspector, optional bottom animation/diagnostics area, and visible Design / Simulate / Live Test modes. Story Flow remains the entry and return context; direct asset opening resolves the same project-owned document.

The initial workflow is: create Screen/HUD -> choose blank/template -> place elements -> arrange/anchor -> choose explicit gameplay sources -> preview/simulate -> save -> configure composition in Story Flow -> real gameplay test -> package. Template elements use exactly the same editing path as blank-canvas elements. A blank draft must not require a dummy action button; readiness validation depends on its declared usage.

Canvas operations include select, drag/resize, multi-select, alignment, grouping, hierarchy reorder, snapping and responsive preview. Numeric Inspector edits remain available. Each drag is one undo step; cancel restores the captured state. Linked-source, local-override and runtime-only values must be visibly distinct. Hidden/locked elements remain discoverable through hierarchy without affecting game input.

Bindings show source, current/sample value, type and error beside the chosen property. Contextual panels disclose advanced rules and animation without making simple health text require an animation graph. Save/Undo/Redo, dirty prompts and return-to-Flow preserve current guarantees. Imported templates first show unbound gameplay references for explicit remapping.

**UI-016:** Design gives immediate visual updates without gameplay commands. Simulate uses isolated labelled sample state, a controllable clock and reproducible scenarios. Live Test connects to the existing real Runtime process and shows connection/session identity and freshness. A disconnected view is explicitly stale; it never quietly becomes simulation. Live edits, if offered later, are a deliberate transient application step and cannot silently write the project or live gameplay values.

## 7. Screen management, composition and responsive layout

### 7.1 Composition and lifecycle

**Required UI-012/013:** Story Flow manages screens with configurable composition and per-screen gameplay/input/pause behaviour. **Proposed:** distinguish full-screen destinations from HUD, nonmodal overlay and modal instances. A Flow context declares which UI assets are active alongside its Level or Screen, their layer/order and lifecycle. HUD attachment does not require leaving the active Level or adding invented navigation edges.

Each active instance has unique identity, source document, context, visibility condition, composition order, input policy and pause request. Lifecycle is `prepare -> enter -> active -> exit -> disposed`; cancellation/failed prepare must leave the previous valid composition intact. Assets may be preloaded, but hidden/inactive instances cannot retain unintended input or gameplay work. Repeated opening is either explicitly single-instance or explicitly creates a new instance.

Input policy is passive, shared nonmodal or exclusive modal. Topmost eligible modal receives navigation and pointer capture; lower layers cannot activate from the same gesture. Opening/closing restores valid previous focus, repairs deleted/hidden targets and consumes the opening/closing input to prevent click-through. The governed gameplay input service remains authoritative. Per-screen cursor and gameplay-input behaviour is explicit.

Pause is a coordinator-owned request held for the instance lifetime. Multiple requesting screens cannot resume gameplay when only one closes. Unpause releases that instance's request; it does not override another owner. Gameplay timers follow the existing pause authority; menus may animate on a separate UI clock. Reset/death/Flow transitions tear down invalid instances and pause requests deterministically. Detailed transition ordering is proposed for review, not an existing guarantee.

### 7.2 Layout

**UI-008:** smart layouts must retain manual control. Preserve existing absolute/anchor/canvas behaviour for old documents. Propose parent-local containers, constraints, padding, minimum/maximum sizes and optional breakpoint overrides as additive authoring features. Resolve composition viewport and safe area, then responsive layout, accessibility scale, animated presentation and final clipping/hit geometry through one shared evaluator.

Creators can choose fixed layout where appropriate; warnings reveal clipping/overlap without silently rearranging authored elements. Text scaling must reflow rather than merely stretch glyphs. Shared resolved geometry drives rendering, pointer tests and navigation. Test 16:9, 16:10, ultrawide and small windows at agreed DPI/text scales. These test shapes do not expand the supported operating-system platform scope.

## 8. Rendering, animation and visual quality

**Required UI-003/007/014:** professional editable presets, advanced animation, contextual visibility and screen transitions. Proposed timeline tracks cover opacity, transform, colour, fill and governed effect parameters, with easing, delay, duration, loops and event/condition triggers. Presets instantiate editable data; custom timelines use the same evaluator. Animation priority and interruption must be deterministic: exit/lifecycle, interaction feedback, attribute response and ambient effects have explicit ownership rather than several writers fighting over a property.

Contextual hiding distinguishes immediate input removal from optional visual exit. Exiting elements cannot activate commands, yet may finish a bounded animation. Re-entry/interruption resumes or restarts according to authored policy without jumps or duplicate completion events. Transition cancellation cannot emit duplicate Flow outcomes. Reduced-motion alternatives preserve state/meaning and avoid delaying essential actions.

Extend the existing shared render-item/presenter architecture to accept resolved instance properties, clipping, theme output and animation values. Keep evaluation testable without a GPU. Renegade continues to own visible pixels; wiGUI mechanics may remain behind the presenter. New controls such as bars, sliders, toggles, lists and masks require explicit semantics and hit/accessibility support; do not pretend the three existing primitives already implement them.

Effects, sprites/animated imagery and supported video overlays should fit governed resources and measured budgets. Codec support, media packaging, HDR/blend policy, masking and effect implementation need technical spikes before availability is promised. No external rendering library or new backend is selected by this document. Prototype visual feasibility inside disposable fixtures after implementation approval; preserve the existing shared renderer until a parity replacement is approved.

## 9. Reuse, themes and project ownership

**UI-009:** independent copies receive fresh internal IDs and no continuing source dependency. Linked component instances retain a stable source reference and explicit per-property overrides. Source edits flow only into non-overridden properties. Show override provenance and provide revert, inspect-source and detach operations. Deleting or changing source structure requires a conflict report; never silently redirect bindings or erase overridden content. Prevent reference cycles and bound nesting.

**UI-015:** global themes cover typography, colours, borders, backgrounds, effects and animation defaults. Proposed precedence is semantic defaults -> project theme -> component defaults/variant -> screen override -> instance/element override -> transient interaction/animation. Player accessibility preferences then select semantic alternatives or adjust layout/interaction constraints. Theme changes must not override explicit local values; clearing an override restores inheritance. Resolve precedence per property and show its origin. Accessibility cannot be implemented as an arbitrary final colour wash that makes authored content unreadable.

**UI-019:** screens, components, interaction/animation presets and themes belong to the project. Optional personal-library publication creates a versioned package; adopting it creates an authoritative project copy. Runtime and Build Game never search a user's personal library. Shareable themes use the same package path; a marketplace/cloud service is not introduced.

Proposed import transaction: inspect manifest/version -> resolve transitive typed closure -> report conflicts and gameplay slots -> stage copies and identity remapping -> validate -> commit -> reveal adopted assets. Include fonts, textures, animations, component/theme dependencies and supported media, with required attribution/licence metadata retained. Do not overwrite same-named assets; reuse only with verified identity/content compatibility and explicit policy. Failure rolls back new registry/files together. Treat a dependency cycle as a diagnosed invalid package unless its type explicitly supports cycles.

Gameplay bindings remain explicit. Imported target IDs are not permission to bind to a same-named Player/NPC. Remap project-owned internal identities safely and leave external gameplay slots unresolved for creator choice. Test adoption with the library disconnected and the original project unavailable. Existing scripting-library closure/transaction patterns are precedent, not evidence that UI library adoption already exists.

Proposed new schema concepts are Screen usage, typed binding records, composition configuration, linked component instances/overrides, theme tokens and animation references. Exact filenames, extension names and schema numbers require a schema review; do not reserve them by implication here. Existing v1/v2 reads stay supported, conversion is explicit and writes use validated transactions. Preserve a reopenable original before upgrade; unknown future versions fail clearly rather than being downgraded or partially overwritten.

## 10. Accessibility and testing modes

**UI-017:** keyboard, mouse and controller navigation, visible/customisable focus, configurable order, UI/text scaling, high-contrast and colour-accessibility themes, reduced motion and configurable interaction targets are framework capabilities. Player preferences apply across Flow screens, reusable templates and newly opened modal layers. Creator freedom cannot silently remove the chosen accessibility capability.

Proposed controls carry semantic role, label, value, enabled state and focus/navigation metadata. Ensure information is not communicated by colour alone. Test focus after dynamic visibility, pointer/controller switching, modal escape/back, text expansion, contrast-theme substitution and reduced-motion timelines. Platform assistive-technology integration and localisation scope require explicit technical assessment; neither is claimed implemented or fully specified by the decision titles.

Design, simulation and Live Test share resolution, layout, theme and animation code. Simulation uses mock adapters, never the gameplay world. Live Test acceptance uses actual Player/NPC/equipment state and the same Runtime behaviour later exercised in the standalone. Screenshot similarity alone cannot establish data or input parity.

## 11. Performance, diagnostics and budgets

**UI-018:** automatically optimise safe work, identify expensive elements and give creators adjustable budgets. Use dirty/revision propagation; reuse layout/render state; bound events; cache static surfaces when invalidation is known; suspend hidden animation/input; release unnecessary inactive-screen resources. Optimisation must preserve action timing, visible output and accessibility. A cache needs defined invalidation for data, theme, text scale, viewport, resources and animation.

Diagnostics identify document/instance/element, binding source, last revision, validity, layout/animation time, draw/batch counts, CPU/GPU cost, memory and input-to-visible latency. Distinguish measured GPU timing from estimates and report unavailable counters honestly. No per-frame catalogue scans, file writes or resource decoding. Detailed profiling must be bounded and opt-in where expensive.

No invented universal FPS target. Before G6 closure, establish a versioned budget sheet on agreed target hardware and representative gameplay scenes: UI CPU p50/p95/p99 in ms, incremental UI GPU ms, peak/resident UI memory, loading/transition spikes, active-instance/animation counts and input-to-visible latency. Record UI-disabled baseline and UI-enabled deltas, warm/cold conditions, resolution, build configuration and capture duration. Owner approves numeric thresholds before performance can pass. Structural safety limits remain necessary; creative expense warnings are distinct from malformed-data protection.

## 12. Phased implementation proposal and acceptance gates

All phases are future work and depend on owner approval of this specification. Each should be a bounded independently reviewable change with its own exact revision and rollback route. Do not use the phase table as permission to begin implementation.

| Phase / dependency | Deliverable | Acceptance gate |
|---|---|---|
| P0: specification review | Ratified requirements, ownership map, old-project fixtures and scope decisions | **G0:** owner approves specification and migration boundary; fuller UI-001–014 wording may refine proposals without rewriting approved titles. |
| P1 after G0 | Attribute/profile/adapters, explicit typed bindings, governed actions and optional rule seams | **G1:** real-source audit; no duplicate ammo/health authority; compatible picker; missing/type/stale diagnostics; deterministic reset/pause; profile overrides and action/condition failures tested. |
| P2 after G0, binding UX after G1 | Additive Screen schema, passive HUD validation, blank/templates, canvas/Inspector improvements and simulation | **G2:** old Screens read unchanged; migration is explicit/reversible; one gesture/Undo; transactional Save/close/reopen; exact preview projection; sample state visibly isolated; owner accepts authoring workflow. |
| P3 after G1/G2 | Flow composition, UI instances, input/cursor/pause coordinator | **G3:** Level plus HUD plus modal works; no click-through; nested pause ownership; restored focus; scene/reset cleanup; existing symbolic Flow parity and packaged journey regressions pass. |
| P4 after G2/G3 | Animation/presets/transitions; measured renderer extensions | **G4:** deterministic interruption/cancellation; visual/input geometry agreement; no duplicate outcomes; shared preview/Runtime output; owner approves visual quality on real hardware. |
| P5 after G1/G2, transition assets after G4 | Linked/independent reuse, themes/overrides, dependency-aware personal-library adoption | **G5:** source updates preserve overrides; detach works; conflicts/rollback proven; no unintended gameplay binding; disconnected library/origin package launches correctly. |
| P6 integrated with P2–P5 | Accessibility and performance hardening | **G6:** cross-screen preferences and navigation matrix pass; owner agrees measured numeric budgets; representative gameplay meets them with diagnostics and hidden-screen lifecycle proof. |
| P7 after G1–G6 | Integrated crossbow gameplay/UI reference, migration closeout and packaged candidate | **G7:** exact-head Debug/Release checks, independent verifier, one consolidated owner creator/gameplay/visual acceptance, isolated standalone parity and evidence ledger complete. |

Allow early accessibility/performance work throughout; P6 is a closure gate, not permission to defer their architecture. Personal-library publication can ship after core HUD composition if owner approves staging, but the framework cannot claim all 25 decisions complete before its G5 proof.

## 13. Verification and final acceptance contract

Retain existing Screen, authoring, renderer, Runtime, Flow outcome and Windows-build regressions. Add tests for new failure boundaries rather than duplicating old coverage. Required categories:

1. **Documents/migration:** frozen old-project fixtures; deterministic read/write; duplicate/cyclic/missing references; unknown versions; aborted upgrade; transaction failure and rollback; no silent conversion on open.
2. **Gameplay:** typed adapters vs authoritative values; incompatible/missing bindings; entity replacement; coherent updates; optional module absent; reset/death/pause; duplicate input requests; bounded actions/rules.
3. **Composition/input:** HUD with zero buttons; modal stack; visibility during exit; focus repair; nested pause; Flow transition failure; cursor and gameplay capture; no stale subscriptions after disposal.
4. **Visual/animation:** shared frame evaluation; manual/responsive geometry; clipping and hit tests; all interaction states; transitions under interruption; text scaling/reflow; theme/override propagation; image/font/resource failures.
5. **Assets/packages:** transitive closure, conflicts, missing resources, stale path hints, internal-ID remap and deliberate external-binding repair; library offline; package without source-tree access.
6. **Hardware/user:** keyboard/mouse/controller, accessibility options, DPI/aspect ratios, owner-reviewed screenshots/video and performance captures on agreed DX12 target hardware. Vulkan follows the repository cross-check policy.

The proposed real-game reference is the owner's working crossbow, not a recreated playground. Audit its current firing/reload/equipment/ammunition data first. On disposable copies or an owner-approved acceptance fixture, author a HUD with loaded/reserve values only where genuine gameplay authority exists, contextual aim/prompt state, a resource/health display backed by actual supported attributes, a pause modal and a transition screen. Missing reserve/health support becomes an explicit prerequisite, not a fake live value. Do not edit the existing crossbow playground in this task.

Final scenario: open -> edit -> undo/redo -> save -> close/reopen -> simulate depletion/transition -> connect real Test Game -> fire/reload/change equipment -> pause/resume -> reset/death where available -> traverse Flow -> Build Windows Game -> launch isolated standalone. Confirm the same explicit bindings, screen composition, navigation, themes, accessibility and gameplay outcomes. Compare performance with agreed baselines. Owner rejection of visual quality or behaviour overrides automated passes.

Each gate evidence record must contain exact commit and dirty-state disclosure, changed files, commands and results, skipped checks, fixture identity, screenshots/captures where relevant, package identity, limitations and verifier. An implementer cannot independently close their own release gate. Update canonical handoff/architecture/feature exposure only in the later approved implementation task, coordinated with ongoing work.

## 14. Owner review points and design risks

The following are proposed mechanics requiring ratification with the specification: role-based explicit source selection; command domains separate from Flow outcomes; stack-based modal/pause coordination; inheritance precedence; usage-specific Screen validation; and transactional linked-asset migration. They elaborate approved requirements without claiming earlier approval.

Open implementation questions are exact gameplay attribute APIs and existing health/reserve sources; schema/version layout; supported rich controls/media/effects; accessibility platform integration; Live Test transport/reconnect semantics; and numerical hardware budgets. Resolve each before its dependent gate. None justifies replacing accepted Story Flow or building a second gameplay world.

Principal risks are breaking older menus by relaxing validation globally; duplicated gameplay values; theme/animation writers overriding accessibility or explicit local settings; modal input leaking into gameplay; pause requests surviving disposal; broken imported links; and expensive per-frame work. The ownership boundaries and negative tests above are acceptance requirements addressing those risks.

## 15. Documentation-only handoff and review evidence

- Baseline commit was obtained with `git rev-parse HEAD`; `git status --short -uno` reported 14 already-modified tracked files. This review did not stage, reset, stash, build, restart or alter them.
- Inspection used bounded file reads and searches of canonical docs, Screen/renderer/Runtime/Studio sources and named tests. Relevant existing feature rows were read as an evidence ledger, not promoted to accepted status.
- Original v1.0 source: `docs/RENEGADE_UI_HUD_FRAMEWORK_DESIGN_SPECIFICATION_V1_0.md`. The present v1.1 file is a separate design-review revision; no engine implementation is authorised.
- Documentation checks: verify all 25 IDs are present, local evidence links resolve relative to the intended `docs/` placement, no machine-specific path is embedded, and only the new document is added by this task.
- Build, CTest, save/reopen, native visual inspection, gameplay execution and packaged Runtime tests were **not run**: this change is a specification only. No fresh functional or release acceptance is claimed.
- Shared `HANDOFF.md`, `docs/ARCHITECTURE.md`, `docs/ROADMAP.md` and `docs/FEATURE_MATRIX.csv` are intentionally not edited during concurrent work. This self-contained handoff and proposed architecture become canonical only after owner review and a coordinated follow-up.
- Next action: owner review of v1.0 and its proposed mechanics; resolve review comments, then explicitly authorize a bounded implementation phase. Implementation has not begun.

## 16. Repository reading index

These links target the canonical copy in `docs/`; the exported review copy retains the same portable references.

- Governance: [repository instructions](../AGENTS.md), [README](../README.md), [charter](PROJECT_CHARTER.md), [architecture](ARCHITECTURE.md), [roadmap](ROADMAP.md), [handoff](../HANDOFF.md), [feature matrix](FEATURE_MATRIX.csv), [verification checklist](VERIFICATION_CHECKLIST.md).
- Existing contracts: [Screen schema](../EngineBridge/include/renegade/bridge/ScreenService.h), [Screen validation/persistence](../EngineBridge/src/ScreenService.cpp), [authoring authority](../EngineBridge/include/renegade/bridge/ScreenAuthoringSession.h), [shared renderer](../ScreenRenderer/include/renegade/screen/ScreenRenderer.h), [Runtime controller/presenter](../Runtime/src/RuntimeScreen.h), [direct Runtime overlays](../Runtime/src/RuntimeApplication.cpp).
- Focused programme evidence: [shared rendering](STORY_FLOW_GATE8B_SHARED_SCREEN_RENDERER.md), [Screen workspace](STORY_FLOW_GATE8C_SCREEN_EDITOR_SHELL.md), [creator controls and limits](STORY_FLOW_GATE8D_CREATOR_CONTROLS.md), [symbolic outcome parity](STORY_FLOW_GATE8E_OUTCOME_PARITY.md), [Flow/build closeout contract](STORY_FLOW_GATE10_RUNTIME_BUILD_CLOSEOUT.md), [combat framework](PLAYER_ARMS_COMBAT_FRAMEWORK.md).

---

## 17. v1.1 owner decision continuation — UI-020 through UI-025

This section is part of v1.1 and overrides earlier *proposals* where they differ. Earlier repository evidence and gates remain in force unless explicitly amended. The v1.0 document remains untouched for review/history. Product decisions below are approved; detailed API, serialization, cross-document transaction and visual layouts remain proposals until the owner approves them.

| ID | Owner-approved requirement | Acceptance implications |
|---|---|---|
| UI-020 | Reusable typed gameplay attribute profiles with per-entity property overrides; non-overridden values inherit profile edits; live instance state remains independent | Real Player/NPC health and weapons keep their gameplay authority; inherited/overridden field provenance and Undo/Redo are visible in Inspector. |
| UI-021 | Global project HUD default, overridable on each StoryFlow Level | HUD attaches without changing the loaded gameplay scene; new Levels inherit the project default. |
| UI-022 | Event-based HUD switching plus optional creator-authored contextual rules | Enter Vehicle changes to Vehicle HUD; Exit Vehicle restores the previous eligible HUD or Level/project fallback, without Level reload. |
| UI-023 | Clean high-level Journey/StoryFlow; categorised Screen Library; existing Graph Editor used for wiring | No second node editor and no forced cluttering of Journey. Graph can show project Screen nodes with presentation-only grouping. |
| UI-024 | Inspector-first button actions: select button, choose On Click destination, Graph synchronises; destination transition default with optional per-button override | Ordinary buttons require no Lua/C++ or Graph visit. One explicit validated user action updates Screen action identity and Flow route transactionally. |
| UI-025 | Every new project begins with a complete, preconfigured and prewired collection of standard game Screens, both core and optional; all can be edited, disabled, deleted, replaced and restored | Core startup navigation is functional in Runtime/standalone; optional screens remain editable and disconnected from active Runtime rendering when disabled. No fake functionality claims. |

### 17.1 Attribute profiles, HUD selection and dynamic lifetime

- Profile definition and instance-specific override masks are owned by gameplay, not the UI system. Each entity's live attribute values remain independent. Editing the shared profile updates non-overridden authored properties without blindly resetting live health or duplication of Character/native combat state.
- A project's default HUD is a stable document reference; an individual Level may override it. HUD attachment and dynamic switching are presentation/composition operations, not Level-to-Level Flow transitions.
- HUD switches may be caused by governed gameplay events, explicit Inspector actions and creator-configured conditions. Respect screen transition, cursor/focus, pause, input priority and the earlier approved death/victory HUD-underlay rules.

### 17.2 Existing Journey and Graph ownership, plus Inspector-first buttons

The repository's **existing** Graph View is a separate ImNodes-backed wired editor (see [Gate 9C](STORY_FLOW_GATE9C_VISUAL_ROUTING.md)). Journey is deliberately the wire-free progression overview. Preserve that split. The project Screen Library owns cataloguing, labels, categories and Enabled in Game state; categorisation does not create a second route authority.

An Options button workflow must be:

1. Select the button in the UI Designer.
2. In its right-hand Inspector choose `On Click: Open Screen` and `Destination: Graphics Options`.
3. The governed authoring transaction updates the Screen's symbolic action mapping and the authoritative StoryFlow route; the Graph View immediately reflects the exact new link.
4. The destination Screen's configured transition is used by default; the button can optionally specify an Advanced transition override.

Step 3 is engine-managed, not an extra creator action. The display labels and dropdown affordances remain an owner visual review item.

**Compatibility requirement:** [Gate 8E](STORY_FLOW_GATE8E_OUTCOME_PARITY.md) intentionally diagnoses stale outcome references instead of auto-rewriting them on arbitrary Screen edits. v1.1 adds a *deliberate new coordinated command* only for an explicit Inspector action/destination choice. Flow continues to own destination, priority and conditions; Screen widgets keep symbolic action IDs. The command must validate both documents, preserve stable IDs, offer coherent Undo/Redo, and guarantee rollback/recovery under failed partial writes or concurrent external changes. Existing manual action rename remains diagnostic rather than silently rerouting. When multiple conditional routes share an outcome, the Inspector must prompt for a choice, not guess or delete routes.

### 17.3 Prepopulated new-project screen scaffold

Every new project starts with these creator-editable Screens and their appropriate, real links:

- **Core:** Splash/Title, Main Menu, New Game -> Level, Load/Save, Options (Graphics/Audio/Controls), Pause, Credits, Exit/Quit, Death, Victory and Return to Menu.
- **Optional extended:** Inventory, Trading, Crafting, Character, Skills, Journal/Quest Log, Map, Dialogue and specialist HUD templates. All remain project assets and may be extended with blank/custom Screens.

The list describes **required future scaffolding**. Current source only verifies a smaller Screen template catalogue (Custom, Title, Loading, Options, Pause, Failure/Victory, Save/Load, Credits) in `Studio/src/RenegadeStoryFlowDestinationComposer.h`; it does not prove all the above are currently instantiated in a fresh project or that backend Save/Load, Trading or Crafting services are complete.

A prewired interface must not fake backend success. Every built-in option must either perform real governed gameplay/file/settings actions or show an explicit missing-capability diagnostic until implemented. Core functionality marked ready must work in an isolated packaged game; optional subsystems require explicit dependency gates.

**Enabled / Disabled / Deleted are different:**

- **Enabled:** Screen is eligible for game display subject to its local visibility/conditions; render, input and binding subscriptions activate only when its instance is displayed.
- **Disabled:** Screen remains saved, editable and wired in the authoring Graph, but Runtime will not instantiate it or process its rendering, animations, UI input or bindings. Routes targeting disabled Screens are diagnosed; default button behavior must not activate them. Owner review is required for the final hidden-versus-disabled navigation affordance.
- **Deleted:** Creator intentionally removes the Screen and its owned graph links through a validated transaction with Undo and recovery. Incoming references require a clear warning/repair state; no silent redirection. Shared resource and gameplay data remain untouched. A standard Screen can be recreated from the built-in template catalogue.

At build time, screen-specific assets reachable **only** from disabled/deleted Screens can be pruned *after* governed dependency closure verifies they are unused elsewhere. Project storage and editor catalogue metadata may still cost space. We do not promise literally zero total overhead: the runtime guarantee is negligible work/GPU cost for disabled and dormant Screen instances, measured against a baseline.

### 17.4 Static implementation audit on 16 October 2026

This v1.1 review rechecked the live local source on DESIGN; branch `feature/p3-original-impact-vfx` at `60634ce`, with unrelated dirty Studio, architecture, roadmap, feature-matrix and assistant files. No implementation files were changed and no tests/builds were run.

| Repository reference | What source establishes | What must still be built or tested |
|---|---|---|
| `EngineBridge/include/renegade/bridge/ScreenService.h` | Schema v2 ScreenDocument; Image/Text/Button, states, hierarchy, styles, focus order | Versioned HUD usage, extra widgets, bindings, enabled policy; legacy migration |
| `EngineBridge/include/renegade/bridge/FlowService.h` | First-class Screen/Level nodes, FlowRoute outcome/destination/conditions | Atomic Inspector route assignment, defaults/HUD policies, prepopulated screen graph |
| `docs/STORY_FLOW_GATE9C_VISUAL_ROUTING.md` | Authoritative ImNodes Graph over shared Flow, distinct Journey | Presentation-only grouping/categories and Inspector sync |
| `docs/STORY_FLOW_GATE8E_OUTCOME_PARITY.md` | Screen symbolic IDs, Flow destination authority, diagnostic repair | New opt-in multi-document command, failure/Undo/Redo tests |
| `Studio/src/RenegadeStoryFlowDestinationComposer.h` | A real existing basic Screen template picker | Full standard/extended default project scaffold, backend readiness matrix |
| `Runtime/src/RuntimeCombatService.h` | Player/Character health, ammo fields and damage handling | Attribute profile facade, authoritative bindings and gameplay events |
| `Runtime/src/RuntimeScreen.h` | Runtime screen focus, action request, shared renderer | Multi-screen HUD composition, policy, dormant lifecycle and contextual switching |

### 17.5 Additional implementation gate requirements

| Existing gate | v1.1 addition |
|---|---|
| G0 Baseline | Audit actual new-project screen seeds and existing backend capabilities for Save/Load, Options persistence and optional modules. Freeze legacy Flow/Screen test fixtures. Owner approval of visual direction before UI Designer code. |
| G1 Gameplay | Prove shared profile inheritance, instance overrides, live-state independence and no competing health/ammo source. |
| G2 Editor | Owner completes button -> Inspector On Click -> destination and optional transition override with no Graph visit; direct canvas and Inspector stay synchronised. |
| G3 Routing/composition | Fresh project scaffold traverses valid routes; default, per-Level and contextual HUDs work; disable/delete/restore; atomic cross-document undo and failure rollback. |
| G5 Reuse | Restore/import template with safe dependencies; no automatic gameplay binding activation or destruction of creator overrides. |
| G6 Performance | Compare disabled optional screens to baseline; measure CPU/GPU/memory/package effects and prove no dormant rendering/input/binding subscriptions. |
| G7 Packaged | New project Splash -> Main Menu -> Level -> Pause/Resume -> Options/Return/Quit, stable save/reopen, and only capability-ready Save/Load/optional screens certified as working. |

### 17.6 Visual UI Designer review: design before code

The owner first reviews screenshot-scale concepts at 1920×1080 and 1280×720. **Candidate, not approved layout:**

- Left: project Screen Library categories/search and widget layers/elements.
- Centre: precise shared-renderer canvas, handles, guides and responsive aspect preview.
- Right: contextual Inspector with easy Content / On Click / explicit Data / Appearance, Advanced tucked away.
- Bottom (collapsible): animation timeline, preview/simulation/live-test indicator, binding errors and performance diagnostics.

Owner checks immediate discoverability, readable Inspector labels, button destination picker, Enabled/Delete/Restore controls, missing-data diagnostics, snapping/resize and keyboard/controller affordances. An image is a design concept until an actual build is interactively verified.

**Owner decision status:** UI-001–UI-025 product requirements approved. **Architecture and implementation remain subject to owner review**; do not begin engine changes or modify the accepted crossbow playground on the strength of this document.

---

## 18. UI Designer visual design freeze — UI-026 to UI-029 (16 October 2026)

**Owner decision:** Freeze additional UI Designer feature discovery after UI-029; focus on visual clarity, current functionality, real gameplay and hands-on validation before implementing additional ideas. This supersedes any implied invitation to continue expanding the element catalogue as part of this phase. The owner approved the overall Concept 02 direction, asked for compact layer controls, and accepted the recommendation to stop feature expansion.

| ID | Approved design rule | Minimal default surface |
|---|---|---|
| UI-026 | Editable canvas grid with configurable spacing, visibility and snapping; snap-to-element alignment; snapping for moves and resizing; precise override and aspect previews | Central canvas grid/snap/spacing controls; per-screen override of project grid defaults without shifting authored element positions |
| UI-027 | Adaptive Studio UI Designer layout; independently resizable/collapsible left Library and right Inspector, collapsible Layers, saved workspace sizes, canvas fit/zoom/pan | Screens / Gameplay / Elements / Globals tabbed toolbox, central canvas, right contextual Inspector |
| UI-028 | Structured authoring toolbox: Screens, Gameplay, UI Elements and Globals, searchable expandable categories and a separate Layers hierarchy | Only one primary toolbox tab visible at once; selected element remains accessible in Layers |
| UI-029 | One-step forward/backward and absolute front/back drawing-order controls; drag ordering; scope confined to the selected element's parent container; selected group order preserved | Four clearly labelled controls in the bottom canvas toolbar and documented keyboard shortcuts; no redundant controls in the Layers panel or Inspector |

### UI Designer simplicity contract

- The canvas is visually dominant. Side panels do not enlarge automatically on high-resolution displays. They can be resized or collapsed independently; preserve settings between Studio sessions.
- Keep the basic Inspector contextual and small: e.g., a Button exposes displayed text, On Click and destination; a Health Bar exposes explicit gameplay source; advanced animations and formatting appear only when relevant.
- There is a single obvious method for ordinary actions: choose element, then configure the property or destination. For the common button path, StoryFlow links are updated transactionally behind the Inspector, not authored manually in a second graph.
- Do not duplicate the same controls in the toolbar, Layer panel and Inspector. UI-029 layer-order buttons live in the bottom canvas toolbar with optional keyboard shortcuts; element addition uses the Elements toolbox or a single Add Element shortcut.
- Reuse the existing Journey/Graph/screen rendering and authoring services as identified in v1.1. Do not construct parallel routing, health, input, save/load or rendering authorities.
- Preserve all 29 approved product decisions as requirements; **freeze scope, not future bug fixes, accessibility corrections or evidence-driven UX refinements**.
- Mark unsupported media/gameplay elements clearly until functional Runtime, persistence, package and owner testing are established. A prototype listing is not a supported-feature claim.

### Owner visual and functional acceptance gate

Before implementing the new Designer UI, owner-review Concept 03 against: (1) uncluttered 1920×1080 and 1366×768 layouts; (2) a button connected from Inspector without opening Graph; (3) grid/snap/editing/Undo and resizing; (4) clearly visible incremental/absolute layer ordering directly below the canvas; (5) editable but disabled prebuilt screens; (6) easy discoverability of current Screen elements and real gameplay binding sources.

Concept 03 is a local standalone HTML interaction demonstrator, not an implemented Renegade Studio surface. Its layering, canvas editing and category switching are visual/workflow evidence only; StoryFlow synchronisation and Runtime are *not* connected in the prototype. Implementation remains unauthorised pending owner acceptance of the visual direction and G0 baseline.

**Owner visual-direction approval (16 October 2026):** The owner confirmed that Concept 03 is the preferred UI Designer layout ("yep, thats the one"). The visual direction and four-section toolbox, central snapping canvas, contextual Inspector and compact Layers ordering are **approved for engineering planning**. This is *not* acceptance of the prototype as functional Renegade code and does *not* approve engine implementation or close Runtime/packaging/UX verification gates.

**Next action:** prepare a bounded G0 architecture and first working-slice plan against the current Screen/Flow services, including source ownership, legacy parity fixtures, cross-document Inspector action transaction and exact owner-test scenario. No more feature-design sessions by default.
