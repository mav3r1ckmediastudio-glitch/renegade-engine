# VSync control

Owner requested an immediate on/off control for visible uncapped frame rates.
This branch starts at accepted main e413a50d4fd2c9f7ba70e75ef76aaf7e68233b1c and retains Wicked 82557d439a41d104f559b0d3134d160b7bf6154e. Meshlet experiments are excluded.

Render workspace exposes VSync at its top. The optional schema-v3 metadata field renegade.render.vsync.enabled defaults on for existing levels. Normal render commands provide dirty state, Undo/Redo and scene persistence; Test Game/Runtime read the same level state.

VSync controls application presentation through the existing EVENT_SET_VSYNC bridge API. It is deliberately separate from RenderPath state comparison. Each application requests presentation on first application and actual setting changes, avoiding a swap-chain rebuild for every render refresh or unrelated image-quality edit. No Wicked source, shader or timestep change is required.

Validation pending: build Studio/Runtime; render-state persistence and command regressions; native swap-chain state and timing on/off; actual UI toggle, Undo/Redo, save/reopen and Runtime carry-through. No merge or alpha release authorised.
