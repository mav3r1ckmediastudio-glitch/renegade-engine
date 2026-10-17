# Owner acceptance — Environment night sky

On 2026-10-16 the owner explicitly confirmed that the night sky, aurora and
Milky Way are all owner accepted. This confirms visual/artistic acceptance of:
- the subtle night atmosphere gradient;
- soft green/pink aurora curtains with adjustable surrounding/upper-sky coverage;
- the independent blue-violet, rose and cream Milky Way with irregular dust lanes.

Accepted implementation code checkpoint: f5ef376.
The acceptance-documentation commit adds no functional code changes.
Accepted Studio binary SHA256:
9E6458B73CE5C7E70ECDD888F5AD5FCFBB3B42846658FBE3BF0773158AEA7D8C.
Build: BUILD/night-sky-check/Studio/Release/RenegadeStudioMilkyWayColourPreview.exe.

All authoring belongs in the existing collapsible Environment panel. No separate
Night Sky tab. Native daytime scattering/stars remain the base renderer.
Milky Way and aurora are independently optional and default off for old scenes.

## Verification already performed

Release Studio and Runtime built; full embedded HLSL6/SPIR-V shaderdump rebuilt.
Actual Studio rendering/controls and actual DX12/Vulkan GPU frames inspected.
Day-native and day-extensions frames are pixel-identical.
58/58 focused source/native tests pass, including metadata, legacy optional-key
defaults, disk reopening and command Undo/Redo. GPU weather exposure stays at
byte 136; the pinned WickedEngine pointer is unchanged.

Commands, screenshots, hashes and limits:
- docs/NIGHT_SKY_ENVIRONMENT_RECOVERY.md
- docs/NIGHT_SKY_CURTAIN_VISUAL_CHECK.md
- docs/MILKY_WAY_VISUAL_CHECK.md
- Tools/night_sky/PreviewChecklist.md

## Remaining merge/release checks

- PR CI against the combined main/feature result and independent exact-head review.
- An uninterrupted final manual editor Save/Reopen and Undo/Redo pass for the
  new Milky Way control. Native command/disk tests pass; this manual pass remains
  unverified. Inspector/focus guards blocked the latest attempted interaction.
- Complex cloud/fog/reflection scene checks and representative 1080p/1440p GPU
  performance. A capped empty preview is not a complex-scene benchmark.
- Packaged standalone Runtime visual acceptance.

Owner visual acceptance does not close these checks or a full release gate.
No changes were made to the user's main checkout/UI Designer work. Publishing
this feature branch and opening a draft PR are authorized; no merge is performed.
