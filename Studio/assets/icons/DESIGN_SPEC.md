# Renegade Studio — Icon Library v1 (design handoff)

**Scope:** main editor, shared chrome, StoryFlow Journey/Graph and dialogue/flow authoring. All designs are original, hand-authored SVG paths, on one 24×24 grid. This package changes **no engine code or project content**. It does not replace the existing Renegade logo.

## Layout

- Icon-only toolbar controls: 32×32 **logical** px square, 5–6 px corner radius; 28×28 where space genuinely requires it.
- 18–20 logical px nominal icon rendering within a 32 px button; artwork is a 24×24 vector master. Use pixel-aligned DPI-aware rasterisation.
- 4 px gap, optional 1 px group separators. No permanent label next to icons in compact toolbars. File / Edit / Add remain textual menus.
- Tooltips on **every** icon: title (as defined in `manifest.json`) and the **actual bound** shortcut if one exists. Never invent shortcut assignments. Hover 400–500 ms. Opaque dark tooltip, smart edge placement, accessible through focus where appropriate, hide on click, mouse leave, transient menu and modal.

## Exact palette from the supplied UI cohesion audit (design proposal, not engine certification)

| Role | Hex |
|---|---|
| Canvas | `#111315` |
| Panel | `#232629` |
| Inset | `#191C1F` |
| Control face | `#303437` |
| Hover face | `#3A4248` |
| Action accent / active boundary | `#DE5B1D` |
| Pressed face | `#853410` |
| Text / neutral icon | `#F4F4F4` |
| Muted label | `#B2B2B0` |
| Border | `#4A5156` |

SVG strokes are white by default, so the same silhouette can be tinted at render time by the icon renderer. Avoid tinting all icons orange. Status colours, graph connectors, and creator-authored game UI retain their distinct semantic meanings. The logo should be the **real existing asset** in Studio, not an image-generated approximation.

## Interactions

- Idle: charcoal face, neutral icon, subtle border.
- Hover: lighter charcoal, orange boundary; tooltip after delay.
- Press: `#853410`, approximately 2 logical px depression; dispatch once on release **inside**; release outside cancels.
- Active/toggled: persistent orange indicator/border, visually distinct from a held press. For StoryFlow use same state policy instead of hard-coded blue selection chrome.
- Keyboard focus: visible distinct outline, not the same as hover.
- Disabled: muted, no click dispatch, explain why via tooltip if useful.
- Tooltip must not cover/steal clicks from menus or graph and must obey popup/modal/load ownership.

## Integration notes

- Do not implement via arbitrary Unicode or icon font glyphs. Use SVG rasterisation, or import these into a compiled texture atlas for the Wicked UI renderer. PNG sizes `16/20/24/32/48` provided as a fallback.
- Reuse IDs in `manifest.json` for accessibility names and localization keys. A label is deliberately omitted on compact buttons; its tooltip/accessibility label is **not** optional.
- On StoryFlow keep Journey structure and existing graph canvas semantics. Update chrome and tool icons only; link and node semantic colours need not be forced orange.
- The screenshot shows an actual branded logo in the application. No logo asset is bundled here because the real repository logo was not supplied as an independent source asset.
- Current v1 has illustrative vector silhouettes. Review actual at 16/20 px, especially character, physics, hierarchy and graph; revise before final production signoff.
- QA at 100%, 125%, 150% Windows scaling, across both StoryFlow and main Studio; verify hover, press/cancel, selection, keyboard focus, selected states, tooltips, icon clipping and render ordering.
