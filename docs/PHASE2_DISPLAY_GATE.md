# Phase 2 Display and Input Gate

## Outcome

This final Phase 2 increment makes the remaining Windows checks explicit and
double-clickable. It does not introduce the production Renegade visual design.

Code commit `1fc5b629a94ad3417fd8ad645ed3bc0cd49ce6b3`:

- parses `dx12` or `vulkan` before Wicked creates the graphics device;
- labels the actual backend in the Studio and Runtime title bars;
- removes the title-bar character-encoding defect;
- displays adapter, physical resolution, logical size, colour space, and FPS;
- applies the Windows-recommended rectangle on per-monitor DPI changes; and
- packages DX12 and Vulkan launchers plus a short visual checklist.

## Package use

The Release Studio package contains:

- `Run-RenegadeStudio-DX12.cmd`
- `Run-RenegadeStudio-Vulkan.cmd`
- `PHASE2-DISPLAY-GATE.txt`

The Release Runtime package contains equivalent DX12 and Vulkan launchers.
The verifier can double-click them; no terminal command is required.

## Acceptance

| Gate | Required evidence |
|---|---|
| DX12 | Studio and Runtime titles show `[DX12]`; fixture renders |
| Vulkan | Studio and Runtime titles show `[Vulkan]`; fixture renders |
| DPI | Displayed DPI/logical size updates and controls remain usable |
| Input | Hierarchy, fields, buttons, gizmo, and file dialog remain isolated |
| SDR | Colour space reports sRGB and UI remains readable |
| HDR | If hardware is available, HDR colour space is reported and UI remains readable |
| Runtime separation | Runtime contains no editor controls |

HDR may be recorded as `NOT AVAILABLE` when the verification machine has no HDR
display. That is a documented hardware limitation, not a fabricated pass.

## Toolkit decision

ADR 0002 accepts `wiGUI`. The decision is based on the pinned source evidence
and the completed Renegade behavioural proof. The display gate validates
platform coverage; it does not reopen the toolkit comparison unless the
accepted path fails.
