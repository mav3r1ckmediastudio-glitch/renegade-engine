# Renegade Weather V2 — scripting and lightning impacts owner requirement
Date: 2026-10-30 | Status: DEFERRED FUTURE-DEVELOPMENT BACKLOG; NOT A CURRENT WEATHER V2 GATE REQUIREMENT
Feature branch: feature/weather-v2-all-gates

## Owner requirement
Every weather feature (all presets, cloud quality/formation/appearance, environment, moon/sky, storms, lightning, rain, wind, future snow and water interactions) must ultimately have **complete supported Lua and C++ scripting access**. Developers must be able to read current settings and state, modify them during gameplay, trigger events, and subscribe to weather and impact events. Do not treat a Weather Inspector-only slider as sufficient implementation.

Owner example: lightning hits a tree and an attached game script causes that tree to catch fire. Fire gameplay/system itself can be future scope, but the weather strike event and target identity must support this application.

## Verified existing seams (2026-10-30)
- `EngineBridge/include/renegade/bridge/ImpactSurface.h` enumerates Default, Metal, Wood, Concrete, Stone, Dirt, Glass, Water, Character. `MaterialImpactSurfaceService.h` and `ObjectImpactSurfaceService.h` provide authored material/instance classifications.
- `Runtime/src/RuntimeProjectileWorld.h` already uses native scene collision queries and resolves contact identity/material. Explicit limitation: mesh/terrain/native CPU colliders are included; physics-only Jolt bodies are **not automatically** covered by this scene-query seam.
- `Runtime/src/RuntimeImpactMarks.h` and impact audio/material systems exist, but bullet holes/damage are **not** visually appropriate generic lightning effects.
- `EngineBridge/include/renegade/bridge/GameplayEventService.h` offers a bounded event queue. `RuntimeScriptRuntime.cpp` binds `renegade.events.emit/send`, `on_event`, and a limited `renegade.weather` Lua namespace: `set_preset(name, seconds)` (clear/scattered/overcast/storm), `get_state()` (coverage, cloud speed, fog, exposure, transition state) and `cancel()`, plus `on_weather_transition_complete`.
- There is **no confirmed complete Lua and C++ parity** for all weather controls and no implemented weather lightning-impact callbacks. The existing limited G3 Lua preset binding is not equivalent to full weather scripting.

## Design: one strike event, optional collision
- Distinguish in-cloud sheet flashes, visible cloud-cloud lightning, and cloud-to-ground strikes. Only the latter needs an impact query. Purely visual lightning remains inexpensive.
- For cloud-ground strikes, resolve a valid target/ground endpoint **before rendering the bolt**. Use bounded world/terrain/object/material query methods; investigate Jolt-only collider coverage before declaring all objects hittable. Record a stable entity reference, surface type, position and normal. No gameplay mutation by default.
- The impact event is the contract. Render optional shock sparks, debris, brief scorch mark, steam at water, tree bark fragments or transient light as an impact-specific VFX, not generic bullet holes, gun smoke or automatic bullet damage.
- Resolve impact effects by authored material/object category and optional impact-profile overrides. Let developers choose no physical impact VFX for distant/background weather.
- A selected struck tree can fire `OnLightningImpact` with its EntityRef/material; an attached developer script chooses `ignite` (when a fire system exists), play audio, damage, quests or no gameplay effect. **Do not auto-ignite every wooden object**.

## Unified versioned scripting contract (proposal)
- One tested, supported Weather API backed by the same engine services for C++ and Lua, not direct unsandboxed renderer globals.
- Read / write and transition between all presets and full weather properties; typed access to clouds, sun/moon/sky, fog, rain, wind, precipitation, cloud shadow, lighting and all present/future lightning parameters. Keep editor authoring values distinct from temporary runtime overrides and safe setters.
- Emit typed weather events: preset-transition started/completed/cancelled, storm intensity change, sheet flash, fork strike generated, cloud-ground impact, and rain/wind threshold changes. Prefer event subscriptions and change notifications to flooding one event per simulation frame or cloud pixel.
- Lightning event payload: deterministic strikeId, event type, origin, resolved endpoint, impact position/normal, surface type, stable target EntityRef (nullable), flash intensity, seed/time and visual-vs-physical classification. Consistent ordering; one impact event per actual strike.
- Scriptable override/policy: enable/disable impact detection, choose target classes, control spawn strength/frequency/branch density/preset/visual effects, optionally request a strike at a validated location, and subscribe to hit tree/ground/water. Bounded ranges, thread-safe ownership, deterministic seeds and sensible performance limits.
- Lua must be able to subscribe and respond; C++ must expose equivalent typed methods and callbacks. Add examples, API docs, generated reference, tests for each property/event, Studio-vs-TestGame parity and persistence where appropriate.

## Roadmap / non-goals
- Current Weather V2: preserve signed-off lightning visuals and finish planned basic Lightning On/Off, Type, Frequency and Advanced Branch Density authoring controls as appropriate. Do NOT add impact collision, scripted strike notifications, gameplay fire, custom bolt artwork or comprehensive API parity as requirements of this iteration. Thunder remains independently controlled with distance delay.
- Future dedicated weather scripting/API programme (timing subject to owner decision): audit every Weather Inspector control and all preset/scene properties against Lua and C++ access, then implement and test full read/write/event parity. This is NOT a current G4 or five-gate Weather V2 signoff blocker.
- Actual propagation of fire, tree burning/char/deformation, widespread lightning damage, terrain craters and long-lived wetness/vegetation response may be separate future systems; weather scripts and event payloads must be designed to support them.
- Nothing in this contract authorizes modifications to signed-off G1–G3 weather or sheet-lightning visuals, nor a push/PR/merge.

## 2026-10-30 — Owner question: imported lightning artwork (FUTURE DEVELOPMENT, design only)

Owner wants artists to be able to replace the stock fork-lightning artwork with their own assets; stock procedural visuals must NOT be a forced aesthetic.

**Simple artist UI under Weather > Lightning > Appearance:**
- Visual Source: **Renegade Procedural** (default) / **Custom Texture** / **Custom Lightning Effect** (advanced).
- Custom Texture: "Choose Graphic / Replace Graphic" uses the existing creator asset/texture-import workflow and supports a transparent bolt texture or compatible animated/flipbook material. Continue to use Renegade's native 3D strike path, branch density, per-branch fade, frequency, cloud illumination and optional impacts; map the imported artwork along actual segment length with UVs and preserve its alpha/emission. Current fork mesh is generated with vertex positions/normals/vertex colours, **no generated UV coordinates yet**: textured bolt art is a NEW implementation task, not already working.
- Custom Lightning Effect (advanced): permit a project-owned, packaged visual effect/prefab/mesh or animated sprite sequence to replace the whole visible bolt. A single flat full-bolt image must be labelled **camera-facing 2D** and cannot pretend to have real 3D branching or occlusion. This override can choose an orientation strategy; source strike origin/endpoint/timing still come from the weather engine. The built-in procedural branching slider may be irrelevant/disabled with a wholly custom full-strike asset.
- Keep **Cloud Illumination** separately toggleable; default to the approved sheet-lightning cloud-scattering effect triggered at the visible strike origin, unless creators explicitly override it. Preserve independent thunder, impact queries, strike frequency and weather-script event delivery even for custom bolt art.
- Art creators should be able to tune visible colour, emission, transparency/halo, thickness/scale and animation speed without writing shaders. Advanced controls can target trunk/secondary/tertiary appearances individually; do not require separately setting every branch.
- Reuse existing `CreatorTextureWorkflowService`, `MaterialTextureAssetService`, `AssetBrowserService` and particle texture workflows as applicable. Save references via the project asset registry, copy/package dependencies correctly, and provide a harmless missing-asset fallback. Never use raw absolute local paths that break packaged games.
- Allow the same replacement and control through typed C++ and governed Lua APIs as part of the full weather scriptability contract. New visuals should fire the same world-space strike/impact events as stock visuals.
- Acceptance: import a creator-owned transparent texture and an animated strike effect, preview correct 3D/2D behaviour, save/reopen, Undo/Redo, restart Runtime, package/test standalone, vary branch density and frequency where relevant, confirm event parity and 75FPS budget. Baseline procedural fork and approved sheet lightning unchanged.
- **Status:** DEFERRED, NOT IMPLEMENTED. Preserve as a named future creator-facing feature; NOT a blocker for the current G4 or five-gate weather signoff. No merge or push authorized.
