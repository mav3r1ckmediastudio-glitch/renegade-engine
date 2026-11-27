# UI Designer guidance before release, 27 November 2026

Status: release requirements recorded; wizard and comprehensive tooltip coverage are not implemented or verified by this checkpoint. Some existing Studio icons already have title tooltips. This requirement adds an audit and useful coverage across the actual UI Designer workflow.

## Owner requirements and context

Before Renegade releases, the UI Designer needs a wizard because the system is large. Controls should have hover tooltips, with the most-used controls as the minimum priority. The system must remain flexible enough for creators to construct their own interfaces, including liquid vials, battery/shield gauges and armour indicators that acquire damage overlays or change appearance as condition falls. Supplied prefabs should demonstrate the same editable building blocks users can author themselves.

The workflow below is a proposed implementation brief. The owner has requested a pre-release wizard and tooltips; individual wizard pages and their exact layout are not yet approved designs.

## Proposed guided creation workflow

1. Choose what to create: a menu, HUD, reusable indicator or blank composition. Offer supplied examples with an explanation of their parts.
2. Add the needed components: images, text, groups, fills, animated media and supported controls. Start with a small useful selection and let users add or remove components.
3. Choose artwork and masks, arrange layers and adjust their appearance. Replacing a bottle with a battery or another shape should use the same component controls.
4. Choose values and behaviour: the data source, current/maximum values, optional state thresholds, animation and sound. Explain choices in everyday terms; a custom attribute is a first-class choice. Do not silently assign an unrelated gameplay source.
5. Preview changes using sample values and relevant states. A damaged-armour example should demonstrate scratch/crack overlays and damaged/broken images at chosen condition thresholds. The wizard only offers behaviour actually supported by the ordinary editor.
6. Finish into a normal editable Screen or group. Optionally save the group as a reusable prefab. Make its Layers immediately available, with clear names and a short description of each part.

Provide Back, Cancel and a way to skip guidance for experienced users. Reopening the guidance should explain the selected composition or offer another example without overwriting existing work. A worked example should make it clear how to reproduce a composition starting from a blank group. The wizard generates the same documents, bindings and layers as normal authoring through existing bridge services and history; it introduces no exclusive Runtime object or private authoring format.

Expose generic layer, mask/fill, appearance/state, animation, binding and prefab controls before making a wizard that depends on them. Generic Liquid Fill creation and Progress effect/mask editing are now exposed; see SCREEN_LIQUID_FILL_AUTHORING.md. Broader appearance thresholds and animation/state authoring remain prerequisites for the corresponding wizard pages. The supplied glass/cork/metal PNG is one flattened artwork layer; the guidance must accurately describe combined artwork instead of presenting imaginary editable sublayers.

## Tooltip coverage

Release minimum: cover the common authoring controls, all icon-only actions in that workflow, and fields whose meaning or effect is unclear. Audit the remaining enabled Designer controls and extend coverage as practical; existing short icon titles are not evidence of complete explanatory coverage.

| Priority | Controls | Help to provide |
|---|---|---|
| P0 | Back, Save, Undo/Redo, Preview/Close, fit, focus/panels, grid and snap | What happens; relevant existing shortcut; whether only the editor view changes |
| P0 | Layer selection/expansion, grouping, duplication, visibility, ordering, movement and resizing | How to select/edit a part or a whole composition; relevant drag and multi-select gestures |
| P0 | Position/size, anchors/layout, fill, artwork/masks, colours and appearance states | What the value changes; meaningful unit/range; how automatic and manual layout behave |
| P0 | Binding source/current/maximum, missing sources, sample values and Preview controls | Which value drives the element; ratio versus direct value; temporary Preview versus saved authoring |
| P0 | Common animation/effect settings and sound assignment/preview | Visible/audible effect; loop/timing meaning; what triggers a response |
| P0 | Palette entries, import/change artwork, prefab save/insert | What is created or replaced and which selected layer/hierarchy is affected |
| P1 | Remaining enabled specialist/advanced controls | Equivalent concise explanations, prioritising specialist terminology and side effects |

Use a consistent brief format: purpose first, then one useful consequence, example or shortcut when needed. Example: 'Maximum value: used with Current to calculate how full this indicator is. Leave blank when Current is already between 0 and 1.' Tooltips must be readable over the shared opaque dark Studio UI, avoid covering their target, and preserve clicking and dragging. Offer equivalent help on keyboard focus or through an accessible help control where hovering is unavailable. Use existing Renegade tooltip styling and render ordering; do not introduce differently styled OS tooltip windows.

## Release acceptance

- A new user can create a working menu and a bound HUD/indicator through the wizard, preview them and finish without reading implementation code.
- A second visually different composition can be built with the same generic components. Include a condition-driven armour-display example once those generic state/overlay tools are available.
- Every generated layer can be selected, replaced, moved/resized, reordered or removed through the normal editor. The wizard can be skipped; independent authoring remains fully usable.
- Back/Cancel preserve the existing Screen. Finish uses governed asset/document services and coherent history. Verify Undo/Redo, Save/reopen, project ownership and standalone Runtime parity.
- Common controls meet the P0 tooltip audit. Test hover/help visibility, panel edges, modal/popup ordering, keyboard access and useful wording at supported window/DPI configurations.
- Repeated Preview open/close retains editor view and authored content. Validate common resolutions and small windows as part of native review, rather than treating source checks as visual proof.
- Record exact source/build/test/native receipts and obtain owner/independent acceptance before marking the release requirement complete.

No implementation, application rebuild, UI takeover, GitHub push, merge or release is part of this documentation checkpoint. Generic authoring gaps should be addressed before wizard implementation; the remaining controls can receive tooltip coverage as their interfaces settle.
