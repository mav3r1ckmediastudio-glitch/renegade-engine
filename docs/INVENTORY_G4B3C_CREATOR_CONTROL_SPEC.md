# Inventory G4-B3C — Creator-controlled Inventory Grid (owner-approved contract)

**Date:** 27 October 2026
**Branch/worktree:** `feature/inventory-core-g1-20261026` /
`renegade-inventory-core-20261026`.
**Decision:** Owner approves a functional Inventory Grid **only with full creative control over appearance and ability to edit/replace default logic**. They do not want a locked, opinionated RPG inventory.
**Implementation status:** G4-B3C0 *pure viewport/layout model* implemented and standalone-tested. **No creator Inspector, runtime scrollbar, generic grid renderer, user-defined item-icon picker or end-to-end editor integration exists yet.** No visual acceptance of this phase.

## 1. Product contract

A new creator can place one optional Inventory Grid onto the existing Renegade Screen, bind it to a chosen gameplay container, and immediately obtain functional item slots, icons, quantities, tooltip, scrolling, keyboard/controller access and default item actions. **No Lua or C++ authoring should be necessary to get a working inventory.** It must also be possible to create the same UI wholly from custom Lua/C++, or use the gameplay service without any inventory UI.

The inventory is **a gameplay service, not a fixed-looking RPG template**. One shared underlying `InventoryService` owns item definitions, unique instances, stack quantities, container constraints and transactions. Multiple visual views (traditional 5×5, survival backpack, 1×8 hotbar, radial picker, storage chest) may bind to the same state. Never impose a second shadow inventory, second equipment authority or automatic StoryFlow navigation.

## 2. Creator-authorable appearance: required, not aspirational

The existing Screen Designer should expose:
- **Shape and geometry:** arbitrary screen layout and anchors; square or non-square slots; columns/visible rows; auto-fill or explicit pixel sizing; slot gaps, border width/radius, padding, animation and opacity; either vertical/horizontal scrollbar or none; optional grid background artwork.
- **Every visual state:** normal/hover/focus/pressed/disabled backgrounds, borders and textures, scrollbar track/thumb/controls, focus outline, rarity/border colours, selection/drag overlays, empty-slot art, stack-count placement, text, font, size and alignment, hover card/tooltip contents and styling, icon fit (contain/cover/stretch), independent icon padding and tint.
- **Per-instance override:** a shared preset/style asset for sensible defaults, but every inventory screen and individual slot may override style/property/resource in the normal UI Designer. Linked reusable components and themes are optional; theme editing must not force immutable appearance.
- **Creator art:** any supported imported image can be assigned as a governed asset reference to a particular item definition or slot/skin. Do not try to infer semantic inventory regions from a single background PNG. Import textures through project Asset Browser; validate references; let renderer scale them to slot bounds with configurable fit and padding. A creator can draw their own inventory background, place a Grid over it, or write custom widgets entirely.
- **Responsive behaviour:** layout, clipping and hit boxes must match across resolutions/DPI; keyboard/controller focus and hover areas follow the resolved positions. Scale PNG on GPU without rewriting the original. High-res icons may use mipmaps and sensible caching. User-defined fonts/assets must remain governable and packageable.

Creator should never need to edit C++ merely to change colours, spacing or artwork.

## 3. Default logic available, but replaceable

The initial plug-and-play actions use the **already owner-accepted G4-B3A2 contracts**: click moves one item, normal drag transfers one, Shift+drag transfers the whole source stack, E invokes the optional scripted transfer, drop outside cancels. But these are **sample choices**, not unchangeable global engine rules.

Expose configurable action mappings in Inspector (Click / Double-click / Drag / Right-click / Use / Equip / Drop / Split / Sort / Context menu) and typed gameplay events. The creator may replace/adapt behaviours with their own standard Lua scripts or C++ providers, including a complete UI authored without the designer. Lua need not match GameGuru MAX's API. Scripts may query authorised item/container snapshots and submit commands. The host validates item identity, revisions, scoped access permissions, capacity/weight, stacking, trade/equip compatibility and atomically applies transfers. UI callbacks don't write item quantities directly or grant their own permissions. Custom gameplay policy providers can implement game-specific rule checks before a command is committed, with bounded, documented hooks.

Expose examples that work out of box, and readable/browsable documentation and entry points for creators wishing to edit the behaviour. When one default behaviour is disabled, other gameplay systems remain unaffected. Gameplay transaction/service compatibility must be preserved for packaged Runtime.

## 4. Pagination vs owned capacity

**Do not confuse container capacity with visible slots.** If a backpack owns 100 slots and a Screen displays 5 columns x 5 rows, only 25 slots are shown at a time. Wheel/scrollbar changes the index window into the same 100 authoritative slots. Standard sliders are not automatically inventory scrollbars; the viewport model must explicitly bind a scroll position and clamp it. The inventory service must never truncate item ownership to the current viewport.

Selection, active drag and keyboard focus need stable stack-instance identity and container ID, not an assumed visible row number. Scrolling or Lua inventory revisions during a drag must not move a different item; cancel/refresh/reconcile as appropriate. Empty visible slots may be valid drop targets but must not allow invalid transfers. Sorting/filtering display policy also cannot silently reorder the authoritative gameplay container without an explicit authorised command.

The existing `InventoryOverlayView` is a **two-panel proof with a 24-row cap** and must not be reused as an unbounded scrolling data source. Introduce a governed, paginated read-only projection in later gates that can fetch items beyond row 24 without copying an entire 1000-item container into the GUI frame.

## 5. Implementation sequence and actual status

**G4-B3C0 — implemented in this commit:**
- `InventoryGridLayout` is a UI-independent configuration for columns, visible rows, slot aspect/size, gaps, padding, icon padding, icon scaling, scrollbar visibility.
- `ResolveInventoryGridViewport` derives row-count, start index, maximum row offset, visible capacity, scroll fraction/thumb fraction from container capacity (never from the count of populated stacks); clamps oversized requested offsets and supports empty containers.
- Pure wheel and scrollbar-offset mapping helpers, with invalid author settings rejected.
- `Tests/InventoryGridViewportTests.cpp` covers 100-slot 5×5, 27-slot partial final row, smaller and empty containers, max offset, disabled scrollbar, non-square cells, invalid size/padding, extremes, wheel and slider fractions.
- Verified Windows x64 standalone inventory CTest **13/13 PASS**; normal production EngineBridge Windows x64 Release link and focused linked grid viewport CTest **PASS**. No new DX12 Runtime/editor UI was linked, launched or visually accepted in G4-B3C0.

**G4-B3C1 — NOT built:** replace fixed 8-slot `InventoryScreenAdapter` presentation with a governed paginated inventory view, generated Grid widgets and stable mapping from absolute slot IDs to focus/click/drop. Add edit/save/reopen and render parity tests.

**G4-B3C2 — NOT built:** Inspector layout/skin editor, scroll wheel, genuine scrollbar drag and clipping; theme and per-slot overrides; original icon assignment to user-defined item IDs through asset picker; tooltip and controller input, Test Game visual approval.

**G4-B3C3 — NOT built:** documented Lua/C++ extension providers, reorder/sort/split/menu action configuration and examples; preserve the underlying validated/authorised transaction service.

**G4-B3C4 — NOT built:** packaged standalone asset dependencies, save/restore, multiple inventory viewers and original B3B2 regression parity across desktop resolutions, independent verification and owner acceptance.

## 6. Acceptance contract

A designer with no scripting knowledge can set up (for example) 5×5 square slots, scrollbar and their own PNG artwork, build a working interactive 100-slot backpack, customise every aspect and still use the standard gameplay. The same 100-slot service can simultaneously power a differently styled chest or hotbar. Another creator can replace the UI and author different item action rules in Lua/C++ without editing Renegade core code. A changed slot's visible icon, quantity and focus correspond to the actual unique item after scrolling, filtering and gameplay revisions. DX12 Studio/Test Game/packaged standalone render and behave consistently. Failing appearance/pointer tests override green automated tests.

Nothing in this document claims those future end-to-end results have been delivered. The isolated implementation remains unmerged/unpushed until owner reviews later gates.
