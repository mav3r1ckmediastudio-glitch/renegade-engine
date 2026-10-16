# Milky Way local visual candidate — 2026-10-16

The owner accepted the soft green/pink aurora baseline, then requested a separate
Milky Way and supplied a photographic colour/structure reference. This candidate
adds a broader, domain-warped galactic band: blue-violet outer clouds, cream/rose
core regions, dense faint points, brighter clusters and irregular branched dust.
It is procedural GPU shading, not an image replacement or a new skybox.

## Authoring and compatibility

Environment > Stars > MILKY WAY INTENSITY. Zero disables it; aurora and native
Stars remain independent. Legacy scenes default to zero, including schema-v1
night metadata without the new optional renegade.night_sky.milky_way key.
The same SetNightSkyCommand owns previews and commits, Undo/Redo and scene data.
No separate workspace/tab. The approved aurora shader was not edited:
SHA256 830CF031E47303A578E684F93BC25D1B584BCCA0C99163988DC669E3236773C6.

Milky Way intensity uses the previously reserved night_zenith.w scalar.
All ShaderWeather offsets/size remain unchanged, including sky_exposure at 136.
Night gradient reads RGB only. The modular galactic contribution is added in the
existing realistic sky before fog/cloud composition, also used by reflections.
It returns before noise/cluster work when off, in daylight or below the horizon.
The complete embedded HLSL6/SPIR-V shader package was rebuilt consistently.

## Build and evidence

BUILD/night-sky-check/Studio/Release/RenegadeStudioMilkyWayColourPreview.exe
SHA256 9E6458B73CE5C7E70ECDD888F5AD5FCFBB3B42846658FBE3BF0773158AEA7D8C.
The existing preview launcher and safety guard select this executable.
The isolated Runtime Release target also rebuilt successfully with these shaders.
No changes to the main repository or UI Designer work; no merge or push.

Actual Studio:
- qa-milky-start.png shows the Stars section and live Milky Way slider at 1,
  with aurora off (first muted-palette candidate).
- qa-milky-colour-current.png shows the final richer palette and dust structure
  in the actual Studio viewport at its 75 FPS cap.
- Automated clicks were stopped when focus/inspector guards detected concurrent
  user changes. The helper now compares the observed inspector before clicking.
  Final manual Milky Way slider Undo/Redo and Save/Reopen were not completed
  independently in Studio; native command and disk tests below pass.

Actual GPU captures: BUILD/milky-way-final-dx12 and milky-way-final-vulkan.
Milky Way alone (native stars off), with native stars, and combined with aurora
were rendered and inspected. Day/dusk/dawn and disabled baselines were rendered.
Final DX12 day-native and day-extensions frames are pixel-identical (maximum
channel difference 0), with Milky Way and aurora enabled in the latter. This proves native rendering, not just
successful shader compilation. It is a designed style, not an astronomical atlas.

Validation:
- Final targeted source/native CTest sweep: 58/58 PASS.
- Native tests cover independent Milky Way edits preserving aurora, Undo/Redo,
  missing optional-key compatibility, nonfinite values, clearing, in-memory
  WISCENE and actual disk reopening/rehydration of the scalar.
- Preview safety guard passes against the saved existing scene, whose Milky Way
  remains 0 until the current unsaved editor changes are explicitly saved.
- Full shaderdump rebuild passed, both backends. CPU ABI assertions still pass.
- Empty-scene timing CSVs are diagnostic only; complex-scene GPU performance,
  clouds/fog/reflection scene acceptance and packaged Runtime visual acceptance
  remain open. No full release gate or independent exact-head review claimed.
- Owner artistic acceptance of this Milky Way revision remains open.
