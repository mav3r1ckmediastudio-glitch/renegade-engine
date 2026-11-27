# Location Zones and animated arrival names, 27 November 2026

Implementation checkpoint on feature/ui-loading-media. Integrated the existing feature/gameplay-zones foundation locally (ba4bdaf) rather than duplicating the zone framework; no hosted merge or push. Existing zone IDs 0-12 stay unchanged, Location is appended as 13. New Add > Location Zone selects its Inspector, with name/type/per-location icon, proximity name range and total duration, None/Fade/Glitch/Wobble, advanced fades/strength/speed/repeat/compass/discovery and editable project Lua. Native positions feed the existing navigation registry automatically; Basic compass settings hide manual targets under Advanced. Optional Location name Text layer remains editable in the UI Designer; the shared renderer supplies a default if absent, including direct levels without a HUD.

WorldLocationService owns bounded per-Level locations and serial messages. Native points exist before governed Lua on_start callbacks. Transform sync preserves Lua profile overrides until authored content changes. Lua renegade.locations exposes register/configure/get/state/discover/announce/hide_announcement/remove. Scene dependency walks include per-location icon resources. Discovery uses zone entry and lasts only this Level visit. A Lua register creates a logical point, not a native physical zone box; removing a native point is temporary because its zone owns its lifecycle.

LOCAL VERIFICATION: Release EngineBridge, ScreenRenderer, RuntimeBootstrap, Studio (review and normal installed binary), Runtime and seven focused tests build/pass: WorldLocationTests, GameplayZoneTests (14 presets), WorldNavigationTests, ScreenTests, ScreenHealthPrefabTests, RuntimeScreenTests, ScreenRendererTests. Also rebuilt and passed S3GovernedLuaRuntime, S5CGameplayEventRuntime and S6ScriptLibrary tests. MarkerIcons, Phase5Gate3, Phase6Gate3, EditorPointLookup and StudioUiPolicy contracts pass. The complete offline AssistantWorkflowSuite passes after refreshing canonical UTF8/LF source hashes. Initial Add-menu contract counts and raw/normalised evidence hash mismatch were corrected and rerun; there is no outstanding automated failure among these checks. Commands/logs: navigation_build.py helper builds each listed vcxproj with MSBuild Release/x64 /m:1 /p:CL_MPCount=3 /p:BuildProjectReferences=false, BUILD/navigation-*.log. CTest uses BUILD/creator -C Release --output-on-failure with the named suites. Public ScreenWidget consumers were explicitly recompiled through the include graph.

NOT COMPLETE: native Inspector placement/layout, standalone proximity playback and multi-resolution visual acceptance are pending. Owner previously asked to leave Studio closed; an explicit request to reopen briefly for final checks is pending. Neither Studio nor Runtime was launched for this checkpoint. Do not mark owner acceptance or release complete. No hosted CI was run. Existing custom compass appearance controls also retain their previous native-check pending status.

Remaining scope limits: no gameplay minimap, persistent save-game discovery, or cross-project scene-prefab icon remapping claim. Queue has eight waiting entries and up to 512 registered points. Glitch means flicker/displacement; reduced motion keeps fades without glitch/wobble movement. Follow docs/UI_LOCATION_ZONE_AUTHORING.md for controls, scripting and defaults.

WickedEngine's pre-existing dirt and root log.txt are preserved/excluded. No remote push/PR/CI/release. Next: finish native checks only once allowed, record observed screenshots/results, then update acceptance status.

---

# Location Zone authoring direction recorded, 27 November 2026

Requirement checkpoint a0d46dd639e847040fa6b42d50f16cd42a8294fd on feature/ui-loading-media. Owner proposes a new Location Zone in the existing gameplay zone palette: place it in a town or cave entrance, automatically open its Inspector, set Name/Icon/Type (Location, Quest, Dungeon, POI, Event or custom). Basic navigation consumes these locations automatically and focuses on presentation; manual targets/coordinates/IDs move to Advanced. Per-location custom icons must be independent of semantic type and remain available through C++/Lua. Recorded requirements and acceptance in UI_WORLD_NAVIGATION_REQUIREMENTS.md, UI_DESIGNER_GUIDANCE_RELEASE_REQUIREMENTS.md and planned REN-UI-LOCATION-ZONE-001.

Inspected existing ZoneService, ZoneStudio controls and RuntimeZoneState on feature/gameplay-zones: bounds/resize/history, thirteen presets and entry/exit sampling already exist. That branch is an ancestor of feature/weapon-editor-zones-blood, which contains the later combined integration. Current UI branch predates those zones; next implementation must reuse/integrate the current zone source rather than add a duplicate. No local merge, application/scene mutation, Location Zone implementation, automatic discovery or minimap completion is claimed. Exact discovery defaults and persistent discovery state remain design work.

Documentation-only checkpoint; CSV16-column and staged whitespace validation passed. No new rebuild is needed. Studio remains intentionally closed at the owner's request. Next: integrate the existing zone foundation, add Location Zone metadata/Inspector/runtime registration and C++/Lua parity, simplify Basic compass authoring, then native/standalone acceptance when the owner is ready. Prior icon-preservation source 2c5545d and handoff503df2e retain their visual-check pending status. No push, CI, merge or release; Wicked dirt and root log.txt excluded.

# Custom navigation icons corrected; native check pending, 27 November 2026

Source 2c5545ddda4e439b0f923e0188db456bacd85886 on feature/ui-loading-media. Applying an appearance previously discarded custom category icon assignments. Shared ApplyNavigationAppearanceDraft now retains existing assignments by default, including custom artwork for a standard category; new preset categories are added. Explicit keepAssignedIcons=false replaces the set. ScreenAuthoringSession, Studio and WorldNavigationService use the same merge; governed Lua apply_appearance provides equivalent policy and validation. No serialized/model-layout change. New Studio controls default Keep assigned icons on, select the target's category, offer direct selected-target icon import and label missing assignments as a fallback dot. See git show --stat and docs/evidence/navigation-custom-icons-2026-11-27.

Built Windows x64 DX12 Release Bridge, RuntimeBootstrap, WorldNavigationTests, Studio, Runtime, ScreenAuthoringSessionTests and ScreenLibraryTests. Include-only sources were explicitly touched. Four scoped CTest suites (WorldNavigation, authoring, library, Studio UI policy) passed in 6.31 seconds; source/evidence receipt records exact command/flags. Regressions cover custom and same-category icons, merge/replacement, invalid caller preservation, Undo/save/reopen, portable ownership/source removal and governed Lua controls. Staged whitespace passed.

The owner intentionally closed Studio during preparation for native visual checks and explicitly asked to leave it closed. Studio remains closed. New controls and standalone playback of this follow-up are NOT natively verified; prior base-navigation evidence does not close this acceptance. Next: native direct icon import, switch appearances with keep on/off, Save/Cancel/Undo/reopen, standalone display. Preserve the owner's current HUD; a pre-follow-up Screen snapshot exists locally under BUILD.

The owner also identified the incomplete world-target workflow: current targets are registered positions/entity references, not automatic recognition of a town/castle/cave/boss. Recorded the proposed entity/marker authoring and explicit visibility/discovery requirement in UI_WORLD_NAVIGATION_REQUIREMENTS.md; discovery state/persistence, Level UI and gameplay minimap remain unimplemented. Exact discovery defaults remain design work.

Local only: no push, hosted CI, merge or release. Pre-existing Wicked dirt and root log.txt excluded. No new runtime/native parity or independent release acceptance claim.

# Generic world navigation implemented and locally verified, 27 November 2026

Source dae7d3cfbcdebec4d40696f3f6e78d614f62be7e on feature/ui-loading-media. Owner requested a generic Skyrim-style world compass/POI bar with four interchangeable launch UI-set appearances and C++/Lua access. Implemented one shared navigation component on ordinary schema3 Image layers, a neutral no-artwork prefab and the supplied fantasy example. Only that available supplied set is packaged; four-set completeness remains pending. See docs/UI_WORLD_NAVIGATION_AUTHORING.md, UI_WORLD_NAVIGATION_REQUIREMENTS.md and docs/evidence/world-navigation-2026-11-27; git show --stat identifies all 67 source/content/evidence files.

EngineBridge owns ScreenNavigation validation/serialization, wrapped bearing projection/filter/range/declutter, atomic authoring/history, portable resources and WorldNavigationService. RuntimeScriptRuntime exposes Navigation() and governed renegade.navigation creation/configuration/get/list/remove and target registration/removal. Safe EntityRefs resolve through generation-aware native transform indexes; deleted/missing references hide markers. Entity reference IDs are cached; no whole-scene per-frame target discovery. Authored grouping, targets/handles/geometry and visibility bindings are preserved. RuntimeApplication::UpdateWorldNavigation runs independently of binding-source availability; native testing caught and corrected the initial binding-only wiring. Shared ScreenRenderer handles authored and independent scripted bars, immutable live snapshots and unchanged-resource reuse.

Native shared Studio settings expose camera/player/manual heading, span, marker size/spacing, range/categories, names/distances/ticks, authored targets/priority/visibility/entity ID, backing/strip/pointer/category icons, font/size/colours and appearance packages. Embedded renderer preview uses labelled sample state. Ordinary Inspector handles position/size. Save is one history mutation; Cancel restores presentation. Native image/font/appearance imports may retain project-owned assets after Cancel.

Built Windows x64 DX12 Release EngineBridge, ScreenRenderer/DXC, RuntimeBootstrap, Studio, Runtime and scoped tests. Explicitly recompiled model-layout consumers (18 Bridge,10 Runtime,2 Renderer,45 Studio,48 test translation units). Include-only modifications touched owning C++ sources. Final ten scoped CTest tests and Studio UI policy passed in 11.42 seconds. Governed Lua regression additionally proves joint image/backing-colour updates independent of table iteration order. Final Studio link succeeded, canonical executable matches the review executable, installed guide matches source. Package trailing-space normalization refreshed byte/hash manifests; all source/installed hashes passed, followed by actual native adoption of both normalized appearances. Staged diff whitespace passed. Build log inventory and commands are recorded in the evidence receipt and BUILD/navigation-*.

Native checks used a separate disposable project C:/RenegadeNavigationReview1127/HUD Components Review. Palette insertion, neutral/fantasy switching, preview heading, target editing/custom-category image import, Save, exact-byte Cancel, one-step exact-byte Undo/Redo, restart/save/reopen and ordinary Preview/Escape return passed. Final saved HUD SHA256 is 3f526eed84684636538566c3bc09f1b15ce3f2b269d8d28cef902b5409f58c44. Target is Watchtower at (15,0,30), custom tower icon, priority10. Original owner project retained.

Standalone normal Runtime entered Title/Main Menu/New Game/Start without proof flags. Shipped governed Level example created an independent west-facing neutral bar filtered to Town alongside the camera-following fantasy bar. Native W movement changed Quest8 m to5 m and Watchtower/Merchant34 m to31 m; native mouse turning moved directions/targets across north while the second bar stayed west-facing. Pause/Main Menu/New Game reset to one fresh bar and initial distances without duplicates. 75 FPS observed in this small fixture; no populated-scene claim. No navigation or Lua errors observed. Service tests cover actual native-transform entity movement/deletion; the native gameplay fixture uses fixed targets. Runtime was closed normally.

Latest Studio is left open on the disposable Player HUD, World navigation selected, settings open with saved Watchtower and working fantasy sample preview. Input returned. Source checkpoint and this handoff are local only: no push, hosted CI, merge or release. Pre-existing Wicked dirt and root log.txt remain excluded. Other launch appearances, wizard/global tooltip audit, minimap, Debug/Vulkan/exported games, small-window/DPI, automatic 2x density selection, populated-scene profiling, Runtime resource hot-reload and independent release acceptance remain pending.

# Generic RPG navigation and four launch UI appearances required, 27 November 2026

Requirement-only follow-up to source315edf48901bf4b58fadb0697370b2db25259f98 and scripting requirement checkpoint d17b43c on feature/ui-loading-media. Owner clarified navbar means the top-of-world RPG compass/POI strip, and requires a generic version because four full UI sets will ship at launch. Inspected the supplied fantasy world_compass artwork, registry/configuration and reference bearing/projection/declutter code; no native navigation implementation or four-set completeness is claimed. Recorded one shared behaviour with neutral/default and interchangeable editable appearances, persistent marker/binding/script identities, C++/Lua access and acceptance in docs/UI_WORLD_NAVIGATION_REQUIREMENTS.md; updated guidance, architecture and planned REN-UI-NAV-001. Documentation-only: no application/project changes, rebuild or UI takeover. CSV16-column and staged whitespace validation required. No push/CI/merge/release; pre-existing Wicked dirt and root log.txt excluded. Next: audit existing heading/marker APIs and all four asset sets before implementing generic native navigation with scripting parity.

# C++ and Lua access required for UI additions, 27 November 2026

Requirement-only follow-up to implementation source315edf48901bf4b58fadb0697370b2db25259f98 on feature/ui-loading-media; prior handoff7d08435. Owner explicitly requires scripting access for whatever UI capabilities are added. Recorded supported C++/Lua creation/configuration/control, runtime events and lifecycle, common wizard/editor/script data and services, paired examples and parity acceptance in docs/UI_DESIGNER_GUIDANCE_RELEASE_REQUIREMENTS.md and docs/ARCHITECTURE.md; updated the planned guidance feature row. Existing coverage must be audited, not presumed complete. No application/API implementation or runtime/project state changed. Validate CSV16 columns and staged diff whitespace. No rebuild needed for requirement-only documentation. No push/CI/merge/release; pre-existing Wicked dirt and root log.txt excluded. Next UI feature must include scripting coverage and evidence alongside its editor controls.

# Generic fill authoring and live Player HUD verified, 27 November 2026

Source 315edf48901bf4b58fadb0697370b2db25259f98 on feature/ui-loading-media. The owner clarified that vials are gameplay health indicators and that creators must be able to edit their parts and construct other indicators. Elements > Basic > Liquid Fill now inserts an unbound ordinary Progress component; any Progress exposes Fill settings. The shared Studio modal supports effect enable/remove, project-owned mask import/change, visible-mask automatic bounds plus editable percentages, all four fill directions, independent tint, motion, sound, existing attribute binding and transient full-draft audition. Selecting a common ancestor is a shortcut only for one liquid descendant. Artwork stays on ordinary independent Image layers; supplied glass/cork/metal remains one combined PNG.

EngineBridge owns WIC inspection/import and atomic ConfigureProgressFill history; existing schema3/liquid metadata and hashed portable Screen packages are reused. Renderer supports all four masked directions, including reduced/no motion, and private full-draft mask/direction preview. Mask coverage is red times alpha; white transparent pixels cannot expand fitted bounds. New imports retain project-owned source/mirror resources. A basic unbound package, technical BatteryMask and reproducible guide are installed with Studio. See docs/SCREEN_LIQUID_FILL_AUTHORING.md and docs/UI_ANIMATED_HEALTH_PREFAB_2026-11-27.md; git show --stat identifies exact files.

Configured/built Windows x64 DX12 Release EngineBridge, ScreenRenderer/DXC, five affected suites, Runtime and Studio. Final five scoped CTest suites passed in6.35 seconds; Studio UI policy passed. After trimming only the new package's redundant final blank line and updating its byte/hash manifest, the health/prefab suite passed again and the installed package was updated. Final CMake configure/Studio build and installed example mask/guide check passed. git diff --cached --check passed. Tests cover alpha/empty/unsafe masks, caller preservation on failure, generic manual configuration, direction/effect history, save/reload and standalone unbound package plus existing portability regressions.

Native Studio used a separate disposable ProjectService-created HUD Components Review project at a short local path, with a real native Starter Level, Player Start, floor and marker. Indicators were authored through the actual palette, Inspector, mask picker and shared modal. Vial resized/placed at108x216 near the lower left; fill and artwork selected independently. Green custom-mask manual65 gauge, native Player Health text and health.current/health.maximum vial binding were saved/reopened. Custom mask auto-fit, all four directions with animation disabled, full100 audition, Cancel exact Screen-byte preservation, settings Undo/Redo, and effect-off Save followed by one-step Undo passed. Screenshots/cropped direction comparison and combat receipt are under docs/evidence/generic-liquid-hud-2026-11-27.

Standalone Runtime entered real gameplay through Title/Main Menu/New Game. Existing opt-in --renegade-inventory-health-proof applies25 actual combat damage on H: health100 became75,50,25, with matching liquid levels, unchanged frame artwork, independent manual65 gauge and75 FPS. Backlog records those real combat values. No new damage shortcut or mana system was added. Healing and audible cues were not checked in this follow-up.

Studio is left open on the separate Player HUD, Health liquid selected, Fill settings accessible, Liquid Fill visible in the filtered Elements palette. Runtime was closed and input returned. Original owner project retained. Source checkpoint and this handoff are local only: no push, hosted CI, merge or release. Pre-existing Wicked dirt and root log.txt remain excluded. Wizard/full tooltip audit, threshold-driven armour overlays, Debug/Vulkan/exported-game playback, small-window/DPI usability and independent acceptance remain pending. New controls include explanatory tooltips; this does not complete the global P0 audit.

# UI Designer wizard and tooltip release requirements, 27 November 2026

Documentation checkpoint e2b74c156b1d86fb4334150c9e1360d4738107b7 on feature/ui-loading-media. The owner requested a wizard before Renegade releases and tooltips throughout the Designer, with most-used controls the minimum priority. Recorded the requirement and proposed editable-layer creation flow in docs/UI_DESIGNER_GUIDANCE_RELEASE_REQUIREMENTS.md, added a dated roadmap item, and added REN-UI-GUIDANCE-001 explicitly as planned/not implemented. The proposed wizard generates ordinary Screen/group/binding data and teaches the same generic construction workflow needed for bottle, battery/shield and condition-driven armour-display examples. Exact wizard pages are proposed, not approved implementation designs.

P0 tooltip coverage is a release minimum for common toolbar actions, Layers, geometry/layout, masks/fills, appearance, bindings, Preview, effects/sounds and prefab/import actions. Remaining enabled specialist controls follow an audit. Existing icon titles are partial coverage, not completion. Generic authoring gaps take precedence before a wizard that relies on those controls. No application code, UI state or project files changed; no rebuild/test run needed for this documentation-only checkpoint. CSV structure and git diff --cached --check passed. Implementation, native usability, owner and independent acceptance remain pending. No push, hosted CI, merge or release. Pre-existing Wicked dirt and root log.txt remain excluded. Next: expose the generic construction tools, then implement the guided workflow and tooltip audit before release.

# Continue resizing and Preview return corrected, 27 November 2026

Source 9d318982e85b1e999d1df29cd098564ac855c9c4 on feature/ui-loading-media. The owner reported Continue could not resize, then reported an offset/clipped editor canvas after closing Preview; restarting restored the view. This source adds explicit per-element manual geometry under automatic containers, enables single-element numeric/handle editing, preserves parent/action/sibling slots, and makes new Title templates use a free360x64 Continue. Interactive Preview return now invalidates the editor layout cache, restores viewport/clip, finishes input gestures and typing ownership, then waits for closing input release. See docs/SCREEN_MANUAL_GEOMETRY.md for exact changed files, commands, results and limitations; git show --stat identifies the full source/evidence list.

Built Windows x64 DX12 Release: EngineBridge, ScreenRenderer, RuntimeBootstrap, Studio, Runtime and seven Screen suites. All seven passed, including explicit automatic-to-manual resize, sibling isolation, content-only preservation, coalesced mode/geometry Undo, save/reload and normal-size Title regressions. The initial full build/tests took538.70 seconds. After Preview changes, explicitly compiled39 transitive Studio consumers, then built TargetName=RenegadeStudioPreviewReview (14.71 seconds including final workspace cleanup). This allowed compilation/linking while the owner retained the previous application. After explicit owner authorization to restart and finish checks, copied the built executable to the normal Studio location and launched it. Studio UI policy and git diff --check passed.

Clean native verification after handover: corner resize431.2x96.8 to368x80, one-step exact-byte Undo, Redo/Undo, numeric320x64 Save changing only width/height, two-step Undo to exact owner bytes, and Story Flow return/reopen retaining the owner's geometry. Five Preview return checks passed using Close and Escape, Continue-to-MainMenu routing, and a focused/zoomed/panned editor. Before/after static cork pixels matched in both layouts. The full saved Title Screen remained identical to the owner baseline after all checks. Standalone Runtime showed the compact saved Continue at75 FPS; clicking it opened Main Menu. Screenshots are under docs/evidence/button-resize-2026-11-27. Preliminary overlapping-input attempts are excluded from final evidence.

Studio is left open on the original review Title with Continue selected, both panels visible and fit view restored. The owner's saved431.2x96.8 geometry at(448,512) and all other Screen content are retained; numeric test sizes were undone. Control was returned to the owner before documentation/commits. Debug, export, small-window layouts and independent exact-source acceptance remain unverified. No push, hosted CI, merge or release. Pre-existing Wicked dirt and root log.txt remain excluded. Next: owner retest of resizing and Preview return.

# Animated liquid vial prefabs implemented locally, 27 November 2026

Source 9dceb0ceaed34ea18b6907210ef22f1eb7913ef9 on feature/ui-loading-media. This supersedes the waiting-for-artwork prerequisite below. Supplied Heathen Fantasy glass/mask are now packaged as a layered Animated vial in Elements > Gameplay. Edit vial is a shared native Studio modal for colour, Health/Mana/Strength or custom attribute binding, animated wave/bubbles, optional value-change sounds and transient fill audition. Saved UI prefabs publishes/reinserts a selected hierarchy through the existing hashed portable Screen package, with fresh identities, destination-owned resources and explicit gameplay slots. Compact resource names fix the repeated-import Windows path failure found during native review. See docs/UI_ANIMATED_HEALTH_PREFAB_2026-11-27.md and its evidence folder for ownership, commands, tests and limitations.

Release x64 DX12 builds passed for EngineBridge, ScreenRenderer (including DXC liquid shader), RuntimeBootstrap, affected tests, Studio and Runtime. Transitive consumers were explicitly rebuilt after the Screen layout change because this reused build has incomplete header tracking. Seven scoped CTest suites passed; amended ScreenHealthPrefabTests passed again after adding repeated-save/reinsert regression; Studio UI policy and git diff --check passed. Actual supplied WAV import recontainers extensible PCM losslessly without modifying Wicked or source files.

Native Studio: two palette-inserted vials saved/reopened; health/red100 and mana/blue35 audition remain independent with untinted glass/metal. Cancel preserved prior saved bytes. Actual native sound import, prefab Save/picker insertion, retained motion/colour/sound and one-step insertion Undo passed. Removed the extra review copy and generic test cue; review has 11 widgets and a silent reusable mana prefab. Studio is left open on BUILD/health-prefab-review-1127/UI Starter, Title > Mana vial > Edit vial, Preview35. Standalone Runtime showed the same layered animation at75 FPS using a disposable authored mana35 fallback; no new in-level mana system or dynamic gameplay-drain demonstration is claimed. Tests cover real ratio-query independence and source/package offline adoption.

The supplied precomposited frames animate their own fill level, so the native effect recreates wave/bubble motion from the supplied layout at24 FPS/four seconds while binding the true liquid fraction. The glass PNG remains original. Bottom-up liquid is the supported visual direction. Imported standalone Screen assets remain after hierarchy Undo. Liquid cues are portable; generic button sound sidecars and video/camera/hint prefabs are outside this pass. Debug, Vulkan, exported-game playback, small-window layout, audible mixing and independent acceptance remain unverified. No push, hosted CI, merge or release. Pre-existing Wicked dirt and root log.txt remain excluded.

# Animated health prefab resumed: artwork portability prerequisite, 27 November 2026

Source 80449c9775a6842663f700425d1c1fb13b539866 on feature/ui-loading-media. Added progress frame/fill typed dependencies and destination remapping in portable UI adoption. Built EngineBridge and ScreenLibraryTests Release x64; the full ScreenLibraryTests executable passed including cross-project adoption with source artwork/package offline, exact frame/fill bytes, fresh widget identity and retained animation. git diff --check passed. See docs/UI_ANIMATED_HEALTH_PREFAB_2026-11-27.md for files, commands, recovered requirements and limits.

The owner is locating the intended animated vial asset. No vial assembly, new artwork, animation-format choice or SFX implementation is claimed. Next: use the intended artwork, retain separate liquid and glass/frame styling, finish the reusable UI prefab workflow and test health/red plus mana/blue instances independently in another project Preview. Studio/Runtime are not yet relinked for this bridge-only prerequisite. No push/CI/merge/release; accepted carousel Studio remains open.

# Carousel editor uses shared Studio UI, 27 November 2026

Source checkpoint: c6afbd307e85218598376fc1088c4a2bc0c8dda4 on feature/ui-loading-media. Use git show --stat for the exact implementation file list. This supersedes the separate Windows carousel dialog and the intermediate recolour. The original carousel data/playback implementation remains 1821e38918bb4369593a7cd55944091d559de63b.

Replaced the dialog with a shared StudioModalWindow, shared accent-outline Renegade buttons, native list/preview, numeric duration, shuffle and a native multiline caption widget. The modal blocks underlying Screen operations and preserves native text-input ownership. File picker callbacks return through the GUI safe point with weak ownership and generation checks. Removed only the obsolete carousel dialog resource; hint editing is unchanged. The body scrolls and footer actions remain pinned.

Changed Studio/src/ScreenCarouselEditor.h, RenegadeScreenEditorWorkspace.h/.cpp, main_Windows.cpp, ScreenHintEditor.rc and added StudioMultilineTextInput.h and StudioMultilineInputRoute.h. Updated docs/SCREEN_CAROUSEL.md, docs/FEATURE_MATRIX.csv and docs/evidence/carousel-2026-11-27/README.md plus editor.jpg. No Wicked files or gitlink changes were included.

Built Studio Release x64 with the configured VS18 MSBuild: /t:Build /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /p:CL_MPCount=3 /m:1 /v:minimal /nologo and CL=/MP3. Explicitly rebuilt the 40 transitive Studio sources after the workspace-header layout change; freshly linked Studio. Final incremental compile/link after numeric draft handling passed. Ran BUILD/creator/Studio/Release/RenegadeStudioUiPolicyTests.exe: passed. git diff --check passed. The already recorded carousel bridge/renderer/runtime tests are unaffected by this Studio-only correction.

Actual Studio checks passed: row changes and paired reordering, Unicode multiline entry, clipboard, local text Undo/Redo, duration/shuffle, Save/reopen, external image import/removal, Escape/Cancel preserving saved bytes, and a single Screen Undo restoring the whole deck edit. Saving after Undo returned the disposable review Screen byte for byte to its original baseline. The accepted outline-button window is left open for owner review. See the final native-modal section of the evidence receipt for details.

No GitHub push, CI, merge or release; exported-package playback, Debug and independent release verification remain pending. Next: owner feedback and remaining loading-media package checks. Pre-existing Wicked dirt and root log.txt remain excluded.

# Basic UI carousel and paired captions, 27 November 2026

Implementation commit: 1821e38918bb4369593a7cd55944091d559de63b on feature/ui-loading-media, based on b300333. Use git show --stat 1821e38918bb4369593a7cd55944091d559de63b for the exact changed-file list. The Wicked gitlink and pre-existing generated upstream changes were not included.

Elements > Basic > Carousel now creates an image carousel and linked Carousel caption Text child in one history operation. Edit carousel opens the native image/optional-caption editor with picture preview, import/replace/remove, reorder, Previous/Next, duration and shuffle. Blank or whitespace-only captions draw nothing. Normal Text controls style and position the linked caption. Captions and images advance together in Studio Preview and Runtime. See docs/SCREEN_CAROUSEL.md.

The implementation retains schema 3 with optional carousel data and caption role. Stable slide identities own each image/caption pair. Captions are stored as hexadecimal UTF-8 to preserve config punctuation, whitespace and line breaks. Typed Screen dependencies include all slide images, and portable UI adoption remaps slide resources and identities. ScreenRenderer retains image handles and uses separate transient caption content so ordinary controller Text refresh does not erase the active caption.

## Commands and results

Windows x64 DX12 Release on DESIGN. Built these existing MSBuild projects in dependency order: EngineBridge/RenegadeEngineBridge, ScreenRenderer/RenegadeScreenRenderer, Runtime/RenegadeRuntimeBootstrap, Tests/RenegadeScreenAuthoringSessionTests, Tests/RenegadeScreenRendererTests, Tests/RenegadeRuntimeScreenTests, Tests/RenegadeProjectUiScaffoldTests, Runtime/RenegadeRuntime and Studio/RenegadeStudio. Used Configuration=Release, Platform=x64, BuildProjectReferences=false, CL_MPCount=3, /m:1 and /v:minimal. All passed. Rebuilt the renderer and freshly linked Runtime/renderer tests after the transient-caption correction. Rebuilt Studio again after the bounded canvas-placement and reorder-button corrections.

Ran:

    ctest --test-dir BUILD/creator -C Release -R "^(RenegadeScreenAuthoringSessionTests|RenegadeScreenRendererTests|RenegadeRuntimeScreenTests|RenegadeProjectUiScaffoldTests)$" --output-on-failure
    BUILD/creator/Tests/Release/RenegadeScreenAuthoringCompletionTests.exe
    BUILD/creator/Studio/Release/RenegadeStudioUiPolicyTests.exe

The four CTest suites passed after correction, authoring completion reported zero failures and Studio UI policy passed. The extended authoring test verifies creation/schema upgrade/history, deck edits, UTF-8 multiline and punctuation round-trip, empty captions, typed image dependencies, rejected invalid IDs/paths/durations/oversized captions, copying and deletion/Undo.

## Native review and fixes

A fresh project was created using ProjectUiScaffoldTests --fixture. Standalone Runtime showed A with caption A, B with caption B and C with no caption. Screenshots are under docs/evidence/carousel-2026-11-27. The review Continue button was moved below the captions. Studio Preview displayed the correct paired caption.

Opened Title via Story Flow in Studio, exercised row preview, Previous/Next, reordering, external PNG import, removal and Save. Reopening retained every image/caption pair. Cancel preserved the Screen bytes exactly. Basic palette creation produced both widgets; Undo removed both, Redo restored both and final Undo restored the original review document. Dropping a new carousel at the bottom-right succeeded with image (800,362,480,270) and its caption ending at the 720-pixel canvas edge. Move up is disabled for the first entry and Move down for the last; navigation still cycles. Studio is left with the entry editor open for owner review.

The first visual pass exposed ordinary Text refresh overwriting the caption; this is fixed through a separate transient presentation value. A separate review-fixture mistake inherited a grid container; the corrected review uses the same free child layout and bottom caption anchors as new carousels.

## Build caveat and remaining scope

This reused incremental build had missing project-header dependency records and skipped some executable relinks. Earlier mixed-layout binaries crashed or retained old validation. Explicitly recompiled every source transitively including the changed Screen headers and freshly linked the affected tests/applications before final results. Do not reuse stale objects as exact-source evidence. The configured Hub revision label can still predate the current source; this commit and compilation/link receipts identify the reviewed implementation.

Exported-package playback, Debug, hosted CI and independent release verification were not performed. No GitHub push, merge or release was made. Next task is owner review of the carousel controls, then any requested alpha-audit continuation and the retained loading-media package checks.

# Loading video transparency, 27 November 2026

Source `79e052f9958452dc53fea93a88ee988aa721b458`. The original compass and smoke assets now get alpha at playback by content identity. Other video layers can select Automatic, Opaque, Remove black or Soft smoke through Choose video ? Transparency. Lua exposes the same selection through `renegade.ui.screen_media` with `transparency` values `auto`, `opaque`, `remove_black` and `soft_smoke`. Removing black also affects dark detail in custom footage. Full-screen video is opaque by default for other clips.

Built Windows x64 DX12 Release using the MSBuild targets documented below. Ran the two CTest suites, including the Lua transparency change and reset. Viewed the older project in Studio Preview and the standalone Runtime loading transition using a contrasting checkerboard in a separate project copy. Returned the review Studio to the original black-background project and verified Preview again. Screenshots are in `docs/evidence/loading-media-2026-11-27/transparent-*.jpg`. The initial diagnostic build failed on shader creation due to a missing embedded root signature. Confirmed the corrected shader bytes in the linked Studio executable and exercised it on screen. No exported-package test in this pass.

# UI Designer loading screens and video, 27 November 2026

Source commit `585e326e5562b19809357517da96f4c882d92de1` on `feature/ui-loading-media`. Run `git show --stat 585e326` for the complete file list. EngineBridge owns screen authoring, hints, video assets and scene preparation. Studio owns the Basic palette, native hint list editor and two stock MP4s. ScreenRenderer owns visual playback and optional Windows Media Foundation audio. Runtime owns Story Flow transitions and Lua access.

Open **Story Flow ? Loading**. Select **Turning compass** or **Rising smoke** in Layers, then **Choose video?**. Drag **Elements ? Basic ? Video** onto the canvas to add a layer. **Loop**, **Mute** and **Blend** are per layer. For a full background choose **Background ? Add file? ? Import video?**. Return to the plain background using the source picker and **Use image background**. Select **Hint text ? Hint list? ? Edit selected list?**, enter one hint per line, and open **Preview**. Hints shuffle and cycle. The media importer accepts AVC MP4 and single-slice Annex B .h264. Raw playback uses the pinned engine's 60 fps convention. The existing creator import ceiling is 4 GiB per file.

Lua exposes `renegade.ui.screen_media(screenPath, widgetId)` for inspection. Pass a third table with `asset` (governed asset ID), `loop`, `mute` or `additive` for a Video element, or `hints` (project .txt path) for Text. Pass `"authored"` to restore the saved settings.

Windows x64 DX12 Release on DESIGN. For the final incremental verification, used MSBuild on EngineBridge, ScreenRenderer, RuntimeBootstrap, Studio, Runtime, RuntimeScreenTests and ProjectUiScaffoldTests in dependency order with `/p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /p:CL_MPCount=3 /m:1`. Both stock clips were staged in Studio. Ran:

```
ctest --test-dir BUILD/creator -C Release -R "^(RenegadeProjectUiScaffoldTests|RenegadeRuntimeScreenTests)$" --output-on-failure
BUILD/creator/Studio/Release/RenegadeStudioUiPolicyTests.exe
```

Both CTest suites and the Studio UI policy executable passed. The scaffold suite includes screen round-trip, stock media and public Lua overrides. Independently exercised the controls in the application. Dragged a Video layer, imported raw H.264 and MP4, used video as the background, changed the hint list through the native editor, restarted Studio, confirmed the saved hints and a clean Story Flow validation, and launched the forest review scene through Test Game ? Continue ? New Game ? Start. The compass, smoke and authored hint appeared during loading, then the forest and gameplay HUD appeared. MP4 mute was measured using the Studio process audio-session meter (0.130 unmuted, zero muted). The final raw stream displayed its advancing timecode and 224 ? 160 / 60 fps information. Captures are in `docs/evidence/loading-media-2026-11-27/`.

The async path covers Story Flow screen-to-level transitions. Final scene adoption is on the main thread. Direct boot and save-game restore are separate paths. Exported-package playback, Explorer drag-and-drop and near-limit media still need independent testing. Local feature review only.

# UI Designer live level-camera backgrounds, 27 November 2026

Source commit `0aceb4d238c7a086ece51b3aa97b903c670a52f1` on `feature/ui-live-camera-background`. See `git show --stat 0aceb4d` for the file list. The change covers EngineBridge screen data and authoring, the Studio inspector and Story Flow level picker, the shared screen renderer, Runtime presentation and the governed Lua API.

In Basic, select Background under Layers. Choose **Source: image / choose camera...**, a Story Flow level and its named camera. Toggle **Slow pan** and use **Preview**. **Choose fallback image...** sets the image drawn when the camera cannot be loaded. **Copy to other screens...** writes independent copies including the camera and pan setting.

The feed loads a private copy of the saved level, keeps native animation and disables gameplay scripts, sound, video and physics components. It renders at 640 x 360 with a 30 Hz cap. Runtime Lua uses `renegade.ui.background_camera(screenPath [, mode [, levelPath, cameraId]])`. Query returns mode, level path and persistent camera ID. Runtime overrides support image, fixed, pan and authored (restore).

Built on DESIGN with `cmake --build BUILD/feed --config Release --target RenegadeStudio RenegadeRuntime -- /m:1 /p:CL_MPCount=3` after a higher-parallelism build hit MSVC C1001. Both Release executables ran. `ctest --test-dir BUILD/feed -C Release -R "RenegadeProjectUiScaffoldTests|RenegadeRuntimeScreenTests" --output-on-failure` passed both tests. The scaffold suite also passed after adding Lua query and override coverage.

On screen, used the Basic inspector to assign Gallery overlook from Starter Level, observed the centre pier changing height with pan off, enabled Slow pan, copied to Title and Options and navigated Main Menu to Options in Preview. Closed Studio, reopened the review project through the Hub and ran Test Game. Title to Main Menu to Options retained the moving background. Reopened the Main Menu inspector and confirmed the named camera and pan setting. Screenshots are in `docs/evidence/live-camera-2026-11-27/`. Geometry, looping animation and named camera were supplied in a prepared review fixture. Camera authoring, complex populated levels, packaged distribution and independent verification remain open.

# UI Designer backgrounds and interactive Preview, 27 November 2026

Source commit `e32a955f214716e59e5df758769944f21ebbf441` on `feature/ui-background-preview`, based on the reviewed button-audio branch at `22ef694`. The combined importer and weapons PR #205 is merged. This review used a separate checkout and a new project.

In Basic, open **Elements**, choose **Add background panel**, then **Choose background image...** in the Inspector. Existing full-screen template backgrounds are selected in place. **Copy to other screens...** opens an unchecked list of distinct screen assets. Check the destinations and click **Copy to selected**. The image comes from the project. Each screen owns its own appearance. The batch writes through the project transaction service. Cross-screen copying has no cross-screen Undo command.

**Preview** opens the current in-memory screen and runs the Runtime screen presenter inside Studio. A button uses its authored local actions, click sound and StoryFlow screen destination. Screen and button transition tracks use the Runtime transition controller. Gameplay actions still require Test Game. **Close preview** or Escape returns to the same authoring session. The source changes are in ScreenService, ScreenBackgroundPanel, the Studio Screen Editor workspace, palette, choice picker and render path, StoryFlowStudioIntegration, and ProjectUiScaffoldTests.

Release build on DESIGN used `cmake -S . -B BUILD/creator -G "Visual Studio 18 2026" -A x64` and `cmake --build BUILD/creator --config Release --target RenegadeStudio RenegadeRuntime RenegadeProjectUiScaffoldTests -- /m:2 /p:CL_MPCount=8`. The final Studio overlay fix was rebuilt from its consuming translation units using the generated Release Studio project. Regression coverage includes two destination files, retained panel identity, independent replacement, unchanged HUD and batch failure.

In a new Hub project, Main Menu's selected background was imported in Basic and copied to Title and Options. Test Game displayed the artwork from Title through Main Menu to Options. After Studio restart the project reopened from the Hub with its artwork. Preview followed the Options button and its imported click sound registered a Windows process audio peak of 0.330708 over 300 samples. Evidence is in `docs/evidence/background-preview-2026-11-27`. A pulse preset applied to the Options button through Advanced Animation ran in the interactive preview. The recorded button region varied in mean luminance from 16.51 to 23.78 and back during a stationary capture. Both Close preview and Escape restored the canvas grid and the original selection after navigating to Options. The Release CTest run passed RenegadeRuntimeScreenTests and RenegadeProjectUiScaffoldTests (2/2). Hosted CI and export packaging remain outstanding. Project creation initially failed under the nested build-review directory and succeeded from the Hub using a shorter project root. The cause of that separate project-creation failure is unconfirmed. The next review is an independent creator walkthrough.

# Basic button click sounds on DESIGN, 26 November 2026

Source c765749903fd0e92ad57e0ca3ec833c4936d6a38 on feature/ui-designer-basic-pause. Select a button in Basic, open Click sound, choose an existing clip or Import sound..., then Preview. Use as default for all buttons promotes this clip and sets that button to inherit. Choose Silent for an individual button or Use project default to restore inheritance. Imported WAV and OGG files go into Content/Audio/UI. Project and per-button settings are stored in Content/UI/ButtonSounds.rui-audio with screen and widget IDs.

Lua exposes renegade.ui.button_click_sound(screenId, widgetId). It returns the effective project-relative clip and the mode (default, local or silent). A third argument sets the mode for this run, with the project-relative clip as the fourth argument for local. The native governed Lua regression checks all modes and confirms script changes leave the authored configuration intact.

Local incremental Release Studio and Runtime linked on DESIGN. A new project was authored through Basic with Resume following the project default, Options using Soft tick.wav, and Main Menu silent. Changing the project default retained both overrides and a new button displayed the current default. After restarting Studio and opening the project from the Hub, Test Game played the default via keyboard on Title and via mouse on Resume, the soft click on Options, and effectively zero signal on Main Menu with successful navigation. Studio Preview and Test Game audio were measured using a Windows audio-session peak meter restricted to their process IDs. Receipts are in docs/evidence/basic-pause-2026-11-26. Hosted CI, export packaging and physical controller listening were not part of the review.

# UI Designer colour wheel and palette, 26 November 2026

Source 48faa9fd996fac6f1599270f7ca2354708d8d226 on feature/ui-designer-basic-pause. Basic button state Colour opens the same wheel as the Light Picker. Opacity and Save to palette sit below the wheel. A solid button edits its background and an image button edits its tint. The Basic canvas previews the colour before Apply. Project colours include opacity and live in Content/UI/Palette.rcolours. Saving a swatch is independent of applying an element edit.

DESIGN native incremental Release linked with cached dependencies. Using a fresh project through Studio, the reviewer previewed orange at 75% opacity on Pause Options hover, saved the swatch, and cancelled. Options returned to cyan. Applying the saved swatch to Main Menu hover produced orange in Test Game using the existing runtime. After restarting Studio, selecting the swatch on the Save / Load screen gave its Back button the same live preview. The screenshot directory docs/evidence/basic-pause-2026-11-26 includes the saved swatch, the runtime hover and the reopened picker. Hosted checks, an image-tint visual check and a Light Picker UI regression pass remain open.

# Basic UI Designer review on DESIGN, 26 November 2026

Source commit f789813f4d1075ddf24bfa0650bc16193e2ebda2 on feature/ui-designer-basic-pause, based on main a9d10b5. This local review used an incremental native Release Studio with cached dependencies and the existing Test Game runtime.

In a new project created through Project Hub, the reviewer opened Pause from Story Flow, dragged and resized the stock Options button, changed its hover colour and dragged a new Button from Elements onto the grid. The reviewer edited its display text, assigned SaveLoad using Destination and imported a striped PNG for Pressed. In Test Game, Escape opened the edited Pause screen. Hover showed the chosen colour, holding the added button showed the imported image, and releasing opened Save / Load. The blank project's save module is unconfigured. After closing and restarting Studio, the recent project reopened with the edited position, button state artwork and route intact. Screenshots are in docs/evidence/basic-pause-2026-11-26.

Free placement in starter content applies to newly generated Pause screens. Legacy managed containers, the other screen templates, preset authoring and scripting exposure still need separate review. The Basic inspector still represents the image by a Change image button and colour by hex text. Hosted checks and a clean packaged build remain outstanding.

# Foliage PR 204 resident-walk CI correction, 24 November 2026

Correction commit cbdbf59ec9d3e871d5696d313fd6da838d3b9320 addresses the second hosted failure on head fe1d8c688ac6750b6bfbbf462149b64194a0554d (merge 1623d8b222b6461f818bb2242477515496f10c2d). Studio run 45470313728 Release passes the full suite/startup; Debug fails only RenegadeVegetation_grass-palette. Baseline and the previous assistant correction pass. Local native Debug/WARP cold-load repetition reproduces the exact walking failure at run 4: prediction grows required collision cells from four to six while two new records are still unpublished. The runtime correctly holds unknown support; the test had not established residency over its entire measured route.

Tests/GrassPaletteProof.h now pins and explicitly verifies all nine native route cells before walking, with bounded normal native publication and scoped interest cleanup. Every movement/arm/final-distance assertion remains. Stale-collision refusal is strengthened to prove a live old body and exactly one blocked cell (9 required, 8 ready). Failure diagnostics record requested cells/body state safely. No production rendering, collision policy, assets, importer, upstream or workflow changes.

Verification: 20 consecutive native Debug/WARP cold-load walking and stale-cell checks pass. Complete registered grass proof via RunRiverDefaultRecovery.cmake passes Debug WARP (67.365 s) and Release WARP (57.774 s), including GPU response, eight stock choices, Custom mixing, Undo/Redo and reopening. WD01VegetationSourceContract and all 161 assistant evidence records pass; git diff --check passes. These use matching cached dependencies, not full clean local Studio builds. Exact commands, source hash, hosted job IDs, limits and four portable logs are in docs/FOLIAGE_PR204_WALK_CI_CORRECTION_2026-11-24.md and docs/evidence/foliage-ci-walk-2026-11-24. This documentation checkpoint records that exact correction commit.

Next: push source and evidence together; inspect fresh full hosted checks against the latest head. Keep PR 204 unmerged pending green CI and explicit owner merge approval. Importer work remains separate; no owner application was closed/restarted.

# Foliage PR 204 CI correction, 24 November 2026

Correction commit ce3f6ee6f31786c8d302ab503c8534f158c10757 fixes the failed assistant evidence fingerprint on PR 204. Failed head b118cdcafa8e6f8828ba10466072ea94b7865884 compiled successfully in both Studio configurations; only RenegadeAssistantWorkflowSuite failed. Baseline Debug/Release passed. The changed foliage architecture document invalidated the whole-file digest for the unchanged Surface Type instructions.

Changed Tools/assistant_knowledge/workflows.json: renewed only the reviewed architecture digest and recorded the review. No validation/test weakening, engine, renderer, importer or upstream change. This checkpoint adds this handoff and docs/FOLIAGE_PR204_CI_CORRECTION_2026-11-24.md with exact run/job identifiers, commands and reproduction evidence. Full workflow-evidence audit found no other stale digests.

Fresh local AssistantWorkflowSuite reproduces the identical four subtest failures before the correction and passes all 84 tests after it (exit 0). AssistantPackagedKnowledgeTests passes Gitless routing/identity and changed-source/stale-index rejection (exit 0). git diff --check passes. Full commands and limits are in the correction report. Existing foliage previews remain untouched; no second renderer was launched.

Next: push the correction and this checkpoint, evaluate fresh hosted checks against the latest PR head, fix any further failures, and await explicit merge approval. Importer work stays separate. Hosted CI is not yet claimed green.

# Foliage PR 204 opened, 24 November 2026

The owner explicitly authorised opening the foliage PR separately from importer work. PR https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/pull/204 targets main from feature/foliage-512. Initial published head is 688d1b1fb6ea78b8478f103ef6c9fdffd57f9f3c; production integration source is e963d2ee39d73cd2400ddafbcc80358ad1153d48, with the later lighting/cloud preview commits retained. This is a documentation-only publication checkpoint; code/assets are unchanged from the initial PR head.

Fetched main is 7ad72ea7842f5d731fd8019867d7d6f50a2d2916 and is already an ancestor (0 commits behind, 27 foliage commits ahead before this checkpoint). The 135-file PR diff contains no importer paths or upstream submodule pointer changes. Full-range git diff --check passes. Both studio.yml and windows-baseline.yml trigger on pull_request targeting main and include Debug/Release matrices; the separate known-environment handshake probe is manual-only. Local bounded validation and its cached-build limits are recorded below and in the linked foliage reports. Hosted checks must be evaluated against the latest pushed head, not the earlier cached application identity.

User authorisation is to open the PR for CI, not merge it. No merge or auto-merge is requested. Importer work remains on its own branch/PR. Next: inspect the latest-head hosted checks, fix foliage failures within scope, and retain exact-head evidence. Full CI and independent/owner final painter acceptance remain before merge/release. The cloudy preview and desktop review launchers are preserved; this publication work does not start additional local renderers.

# Cloud-cover and shadow preview, 24 November 2026

Preview source/evidence commit 816b0f1b6af5e5b728cbba4796abb3670f533b02 on feature/foliage-512. Before proceeding, the owner asked to see the approved scene with cloud cover and shadows, then responded positively to the cloudy-night view. Keep that as appearance feedback only. The original clear-night archive remains unchanged.

Tools/FoliageCloudReview.cpp creates four saved/reopened native variants of the unchanged six-plot reference: night and 14:00 daylight, each with the cloud-shadow flag on/off. Cumulus coverage 0.52, base 1200 m, thickness 3500 m, animation 0.65. Night keeps approved lighting/exposure 3; daylight exposure 1. Only capture-time cloud motion is frozen, with 360 warmup frames. Windows x64 Release cached native build and cold captures pass; cloudy-night/daylight normal Runtime player captures also pass. Commands: Integration/build-cloud-review.cmd, FoliageCloudReview.exe <approved-scene> <fresh-output>, CloudReviewV1/verify-weather-player.py. The initial discarded-nodiscard warning is corrected with an explicit void cast; final compile is clean.

Actual daylight cloud-shadow toggle changes the selected ground/grass region by mean absolute RGB (85.19,80.07,56.98)/255. The night pair changes only (0.108,0.082,0.057)/255. Native renderer/shader inspection confirms a single Sun-directed cloud shadow map; the separate world moonlight has no physically aligned lunar cloud projection. The owner was told this limitation. Night clouds render and hide the sky visually, but do not claim moonlight occlusion works. Its proper projection is separate renderer work, with no upstream/shader patch made here. The fixed broad map also limits fine spatial shadow patterns across this small field.

Desktop Foliage Weather Review folder provides 01 Cloudy Night, 02 Day - Cloud Shadows On and 03 Day - Cloud Shadows Off. Launchers close only known foliage player previews before opening another; editors and unrelated Test Games remain untouched. The clear-night Runtime PID 53320 was closed gracefully; current live cloudy-night Runtime is PID 45572. Three private projects have fresh project and scene-envelope IDs. Full scripts/projects/captures remain under CloudReviewV1 in the owner QA workspace; evidence and details are in docs/FOLIAGE_CLOUD_PREVIEW_2026-11-24.md and docs/evidence/foliage-clouds-2026-11-24.

No production exposure change, importer/zone change, CI launch, push, merge or release sign-off. Next: follow owner feedback on the preview, address lunar cloud shadows if requested, otherwise return to the foliage painter/CI gates below.

# Independent stock plants integrated locally, 24 November 2026

Implementation/evidence commit e963d2ee39d73cd2400ddafbcc80358ad1153d48 on feature/foliage-512, based on 41eec45. The owner confirmed the approved reference plays well at 75 FPS, then authorised continuing the five accepted additions into the painter. All eight stock choices now appear alongside Custom. Each has independent saved geometry/density/dynamics/detail settings; selection only changes the brush, stock painting adds to existing vegetation, erasing affects all under the brush, and Custom retains replacement painting and shared settings for Custom/legacy layers. New stock identities isolate Full from Very Full. Existing layers are not migrated.

Changed: VegetationService.h/.cpp (stock definitions, source digests, scoped APIs/commands and metadata), GroundPaint.cpp (per-layer density), WD01Vegetation.cpp (eight choices and scoped sliders, nine visible rows), PackageStockFoliage.cmake, five exact approved Content PNGs, GrassPaletteProof.h plus StockPlantProfilesProof.h, source contract, reference verifier, actual-painter review author, architecture/feature/preset/integration docs and evidence. GrassAtlas and VegetationStrokeState layouts are unchanged; the settings command carries its optional atlas. No upstream/shader/global lighting/weather defaults, importer or zone edits.

Windows x64 Release compile/link used the matching existing dependency/object cache. Commands: build-integrated-stock.cmd; link-integrated-stock.cmd; Integration/build-integrated-ui.cmd; Integration/verify-established-player.py; Integration/build-painter-review.cmd; Integration/build-owner-startup.cmd; Integration/verify-painter-launcher.py; Integration/final-picker-build.cmd; WD01VegetationSourceContract.cmake. Registered grass-palette now additionally reports STOCK_EIGHT_PLANTS_OVERLAP_SETTINGS_GROUND_HISTORY_REOPEN_PASS, alongside Custom/GPU variety/interaction/walk/history/reopen passes. Ground-paint reports both live-frame writeback and full import/coverage/erase/history/reopen passes. Actual preset and slider callbacks with settings Undo/Redo pass for all eight choices and preserve shared values; Custom remains available. Packaging verifies seven exact approved sheets, removes the thirty obsolete names and preserves owner install/Custom/Mixes sentinels.

The expanded FoliageNightReferenceVerify compares the accepted broad meadow and five additions against the preserved snapshot: height/width, dynamics, topology/seed, RGB/material and six crops all pass. It caught and corrected inherited stock drag/gravity (now the approved 0.12/0.02). The original reference integrity and GI-off negative control also pass. Its archive SHA remains 798a180b7c74c897fb4bccd743dd123ccf92b92e80d419d78c31e7123db120e9.

StockPlantPainterReview paints eight actual native terrain plots, saves/reopens, verifies position spans move under native breeze and captures textured terrain/plants at night, noon and near/25m/60m. Completed lifecycle fixture is Integration/PainterReviewLifecycleV6. Native captures were inspected; receipt reports ALL_EIGHT_STOCK_NATIVE_POSITION_SPANS_MOVED_PASS and STOCK_PAINTER_NATIVE_DAY_NIGHT_NEAR_DISTANCE_CAPTURE_PASS. Static views do not establish universal temporal shimmer/stutter or whole-world performance. Early raw-archive helpers lacked native terrain/resource lifecycle and are not accepted evidence; SceneService plus initial native frames corrects that harness. Other retained harness failures include a ground coverage test using an unregistered material, an alternate cached Runtime relink without a capture result, and a plain cached Studio startup route that did not open/report the requested project. The established normal Runtime successfully captured both saved painter data and the approved reference at frame 60; controllers stopped only their own isolated processes. No clean-exit or converged-GI claim from those player captures.

Desktop Foliage Painter Studio.cmd opens a separate editable eight-plot project in Vegetation. The local review executable uses a launch-only QA startup controller; it opens that private project, selects the workspace and frames the view, with no widget/painting automation during owner use. Its exact launcher/project readiness passed on an isolated desktop. A verified Runtime and Content are copied alongside for local Test Game support; actual Test Game UI launch was not separately exercised. The helper closes only the known foliage preview to prevent duplicate foliage renderers and preserves importer sessions. The final picker-only change (nine visible rows) compiled/relinked; prior callback tests cover the same input logic.

The original approved night preview was restored as PID 53320. Its later sample was paused/unfocused, so do not treat submitted frames as an interactive FPS benchmark. The owner-confirmed 75 FPS applies to the previously tested reference. Full local scripts, response files, failed fixtures and private projects remain with the owner QA workspace; portable receipts/images are in docs/evidence/stock-plants-2026-11-24 and the detailed report is docs/FOLIAGE_STOCK_INTEGRATION_2026-11-24.md. git diff --cached --check, the feature-row column check and new-text path audit pass.

Next: owner tries the real painter review, including walking/motion, distance and representative performance. Prepare the bounded full Debug/Release CI candidate against the current integration base; hosted/clean whole-configuration and independent exact-commit gates remain. No push, merge or release sign-off occurred. Automatic world moonlight phase/position tracking remains separate follow-up.

# Approved foliage night reference and GI study, 24 November 2026

Source/evidence commit: 984867f0adcefe239acdaff5d11441c07ac8a7af on feature/foliage-512. The owner approved the live night moonlight + lantern + Surfel GI appearance and asked that it be preserved. Examples/FoliageNightReference contains the byte-identical approved WISCENE (SHA-256 798a180b7c74c897fb4bccd743dd123ccf92b92e80d419d78c31e7123db120e9), project identities, artwork, settings/file manifest and reference images. Desktop Approved Night Reference.cmd opens the saved example; close the current review before launching another. Live review PID 2800 and unrelated importer Test Game PID 43104 were preserved.

Tools/FoliageLightingReview.cpp and docs/FOLIAGE_TERRAIN_NIGHT_GI_2026-11-24.md record 21 saved/cold-reopened native cases and 51 captures. Grass and terrain respond to direct moonlight/local light/shadow and Surfel GI; no-light controls are essentially black. Original artwork fixture ambient 0.75 is unsuitable as night lighting. Sky MoonService does not create world moonlight: this reference explicitly authors the existing Cold Moonlight directional preset. Exposure is 3 for the approved night view, GI boost 1, Surfel on, SSGI off, ambient zero; wind is frozen for matched light comparison. Automatic moon phase/position world-light tracking is not implemented. SSGI resources were verified but its visual effect in this fixture was tiny. No DDGI/VXGI or continuous day/night-cycle acceptance is claimed.

Windows x64 Release native author/verifier compilation and linking passed using existing matching dependency/object caches. QA commands: build-lighting-review.cmd; build-lighting-followup.cmd; summarize-lighting.py; verify-lighting-runtime.py; verify-approved-night.cmd; verify-approved-reference-runtime.py. The matrix receipts end FOLIAGE_LIGHTING_SAVE_REOPEN_NATIVE_CAPTURE_PASS. FoliageNightReferenceVerify.exe <approved-Main.wiscene> reports APPROVED_FOLIAGE_NIGHT_REFERENCE_PASS embedded_foliage_atlases=6 and rejects the GI-off negative control with exit 1. Four normal Runtime cold loads/captures passed (day, moon, lantern, relocated approved example); controllers terminated only their own isolated test processes, so no clean-exit claim. All images were inspected. Original/relocated player screenshots have different view directions, so their RGB difference is not a parity metric; unchanged scene bytes and native settings/resource checks establish saved-state preservation. git diff --cached --check passed.

Initial diagnostic failures are retained locally: resource-member capitalization, unsupported exposure-4 expectation (service correctly clamps to 3), and incorrectly requiring retained CPU texture bytes for lazily streamed embedded archive images. Corrected guards inspect the native resource table/GPU textures. A path-rewrite experiment was abandoned without modifying the approved scene; generated terrain virtual textures are not ordinary source image files. Native scene/archive and all production code remain unchanged. Generated descriptor/input whitespace was trimmed without changing values.

The owner then reported a large FPS drop. The second Runtime used for relocated verification had completed and exited; possible transient GPU contention is unproven. Afterward only the live foliage review and unrelated importer Runtime remained. A five-second diagnostic sample gave about 75.8 submitted frames/s while paused and unfocused, which does not prove interactive presentation FPS recovery. An asynchronous question asks whether FPS recovered and what FPS the owner sees while moving. Do not open additional GPU tests while the owner views the scene. Performance acceptance remains open.

Next: resolve the owner FPS observation first, using matched focused interactive conditions and actual frame/GPU timing if the drop persists. Preserve this approved appearance for future comparisons. Five accepted additional plants still require production palette integration with proper per-species sizes/densities (existing settings are terrain-wide), distance/mip/motion checks, then bounded CI preparation. No upstream/submodule, production shader/default lighting/palette, importer or zone changes; no push, merge, clean whole-build/hosted CI or release closure.

# Owner approval of five temperate additions, 24 November 2026

At add0cb2d04e97091e66d8fb6045e2b4c81140ba6 the owner reviewed the live Temperate Foliage Review and said "i love it all!". Record visual acceptance of the five additional plants alongside the previously approved meadow: short grass, dry grass, seedhead grass, low clover and small wildflowers. This is appearance approval, not a GI, distance/mip, benchmark or release sign-off. The live review was opened as PID 30308 and its advancing frames were verified after focus/resize. Production palette integration and CI preparation remain the next implementation work.

The owner asks whether grass reacts to global illumination. Inspection of the existing local hairparticlePS.hlsl, shadingHF.hlsli and lightingHF.hlsli confirms sky/ambient lighting, direct lights/shadows, an SSGI input and the shared indirect-diffuse paths for ray-traced diffuse, Surfel GI, voxel GI and DDGI when their resources/settings are active. This is shader-path evidence; a GI on/off visual test of the accepted plants has not yet been run. No lighting defaults or shaders were changed to answer this question.

# Approved native-breeze appearance and five temperate candidates, 24 November 2026

The owner reviewed 01 Native Breeze in the live normal Runtime and said "grass looks great". Keep that meadow/native-breeze appearance as the accepted baseline. Do not expand this wording into an explicit stutter, distance/mip, whole-level performance or TAA acceptance. The owner Runtime PID 42040 remains open and responsive; it was not closed or replaced while preparing the additions.

Candidate source/art commit a80d9f9052fc1f58b23baf309eaba2923c90ab4b on feature/foliage-512, based on 331f58a. Tools/TemperateFoliageReview.cpp authors six labelled plots: the unchanged approved meadow material and five existing v3 atlases for short grass, dry grass, seedhead grass, low clover and small wildflowers. assets/foliage/review/temperate-v3 contains the unmodified PNGs, native crop sidecars, SHA-256/coverage manifest and full review notes. Each PNG is 1024 by 1536 with six 512-square cells. Native crops come from alpha >= 128 boxes with one pixel of in-cell padding; PNG bytes were not changed.

Each plot is 72 square metres, with two segments/two cards, seed 17 and the approved native breeze. Front left/centre/right are approved meadow (3600 tufts, 0.50m), short grass (1800, 0.25m) and dry grass (1800, 0.45m). Rear left/centre/right are seedhead grass (1080, 0.65m), low clover (1440, 0.12m) and small wildflowers (288, 0.30m). These are trial sizes/densities. New short/seedhead greens are brighter than the accepted meadow and require colour review. The clover needs inspection from standing height as well as the low close-up camera.

Windows x64 Release compiled the native author. The first build-v3-review.cmd harness link pointed two cached bridge objects at the wrong directory; that failed log is retained. The corrected resume-v3-review.cmd linked the already compiled object against the original matching dependencies and ended TEMPERATE_FOLIAGE_NATIVE_REVIEW_PASS. Its receipt also records TEMPERATE_SIX_PLOTS_COLD_REOPEN_PASS and TEMPERATE_SIX_NATIVE_CAPTURES_PASS. Save/cold reopen checks plot counts, dimensions/counts/atlas variants, emitter subsets and texture resources, and native breeze. Six native close-up renders were inspected and all emitters retained their populated distributions. Run TemperateFoliageReview.exe <native-breeze-source.wiscene> <prepared-project-directory>. Reproduction scripts, response files and complete logs are under the existing TemperateFoliageQA owner workspace.

verify-v3-runtime.py launched the unmodified normal Runtime on an isolated Windows desktop, captured the actual player camera at frame 60, and verified the process stayed alive afterward. The capture was visually inspected for the six-plot layout and labels. The controller then terminated its own test; clean exit was not verified. verify-temperate-launcher.py exercised the exact owner helper on an isolated desktop and printed TEMPERATE_OWNER_LAUNCHER_READY_PASS, then terminated only its own review. The new owner's Desktop launcher is Temperate Foliage Review.cmd. Its private project is TemperateReviewV3/TemperateReview.renegade in the QA workspace with fresh project and scene IDs. Close the existing grass review before opening it.

Only the review tool, candidate source artwork/metadata and handoff changed in this pass. Stock brush/presets, renderer/shaders, upstream pins, importer, zones and the existing approved review remain unchanged. No push, merge, clean build/CI or release gate closure occurred. Next: owner judges all five candidates against the accepted meadow, especially brightness, clover from above and sparse flower coverage; check distance/mips and motion, then integrate accepted choices into the production palette and prepare the bounded foliage CI candidate.

# Wind isolation and playable comparisons, 24 November 2026

Diagnostic source and evidence commit: e89a808b47c3cc1abe9830989d43a180a7e765b7 on feature/foliage-512, based on f2d82b114962d8fbcff232b123666c6c5145f9f6. This records investigation, not a motion/shimmer fix or acceptance. The three approved stock meadow choices remain; the five temperate additions still await native visual comparison and integration.

Tools/WindIsolationReview.cpp is a standalone native diagnostic. It cold reopens four matched 204800-strand scenes and compares atlas, geometry, dynamics and force/animation counts. The four modes are travelling gusts, no wind, one constant force and a real native breeze. Windows x64 Release compilation/linking used the existing QA compiler/link response files and matching dependency cache. The command was build-wind-isolation-v2.cmd; its log ends WIND_ISOLATION_FOUR_MATCHED_MODES_PASS. Run the tool as WindIsolationReview.exe <prairie-source.wiscene> <fresh-output-directory>. docs/FOLIAGE_WIND_ISOLATION_2026-11-24.md and docs/evidence/foliage-wind-isolation-2026-11-24.csv retain parameters and results. This is not a clean configuration or hosted CI result.

The earlier ordinary-wind comparison was invalid as a moving-wind baseline: its weather direction was zero. The corrected native breeze uses direction (0.08,0,0), speed 1, randomness 5. In the four-mode isolated run each median was about 13.334 ms. Travelling gusts were not consistently worse; all modes had outliers. No screenshot, readback or file write occurred during each 900-frame timing window after 180 warmup frames. Do not infer present-time smoothness or whole-level GPU cost from this capped diagnostic.

Private QA prepare-wind-comparison.cmd made five independently identified projects, including native breeze with TAA, and cold reopened their render settings. Source AA was Off; the extra TAA trial persists mode 2. Depth of field and motion blur are disabled consistently in the five playable comparisons. verify-wind-comparison-runtime.py obtained actual unmodified Runtime player-camera captures at frame 60 for gusts, no wind, native breeze and native breeze with TAA on an isolated Windows desktop. All four remained alive after capture. The controller terminated its own processes with exit 1; clean shutdown and continuous motion quality are not claimed. Static screenshots were inspected: TAA smooths grass edges but noticeably softens detail.

The owner's Desktop has a Grass Wind Comparison folder with five numbered launchers and a short Read Me. Start 01 Native Breeze, compare 04 Original Travelling Gusts, then compare 02 Native Breeze - TAA for shimmer and softness. Open one at a time. verify-wind-launcher.py exercised the exact Native Breeze launch helper on an isolated desktop and printed NATIVE_BREEZE_OWNER_LAUNCHER_READY_PASS. It verified the ready DX12 window, title and maximize/focus path, then terminated only its own process. QA scripts, captures, logs, projects and per-frame CSVs remain under the existing StockGrassQA owner workspace. No review was left running on the user's active desktop. The unrelated importer Studio was preserved.

The only repository changes are the diagnostic tool, evidence report/CSV and this handoff. No production renderer, shader, palette, upstream pointer, importer or zone files changed. No push, merge or release gate closure occurred. Next: owner judges live native breeze versus gusts and the optional TAA softness, stationary and walking. If motion still steps, collect actual Runtime present intervals and tip trajectories before changing dynamics. Validate distance/mips and the five additional temperate candidates before preparing the combined foliage CI candidate.

# Stock foliage scope and failed motion acceptance, 23 November 2026

Owner direction: keep the three meadow grass choices for the next PR; remove the other stock additions, especially aquatic and alien plants. Appearance is approved apart from texture flicker/shimmer and stuttering movement in Desktop 2026-11-23 16-40-30.mp4. Those defects remain unresolved. This entry supersedes positive wind interpretations below: the recording does not demonstrate acceptable rolling prairie wind, regardless of force animation, GPU readback or FPS.

The stock cleanup removes thirty bundled cutout PNGs, removes their group selector, limits stock choices to Meadow Open/Full/Very Full and retains Custom PNG/sheet imports. Retired PlantGroup enum values remain source-compatible and query empty even with stale installed files. PackageStockFoliage.cmake cleans only the exact retired bundled filenames in the install foliage directory before copying the two approved sheets. Project-owned Custom and Mixes resources and existing WISCENE layers remain untouched. Choose a meadow preset explicitly; an empty Custom selection cannot silently paint a different fallback.

Implementation: b45076bdd2dcbccb9605e17263da7c6195d5b492 on feature/foliage-512. Changed GrassPalette.h, VegetationService.cpp, WD01Vegetation.cpp, Studio/Runtime/Tests CMake staging, GrassPaletteProof.h, GroundPaintProof.h, WD01VegetationSourceContract.cmake, StockGrassPresetProof.cpp, new Tools/PackageStockFoliage.cmake, architecture/feature/preset documentation and the thirty retired PNGs under Content/terrain/foliage. No upstream, importer, zone or original worktree edits were made.

Focused Windows x64 Release checks passed using matching existing dependency/object caches: VegetationService, WD01Vegetation, StockGrassPresetProof and RiverTerrainFollowProof compiled; the isolated normal Studio and private controller linked. Local QA commands were StockGrassQA/build-retired-stock.cmd, resume-retired-regressions.cmd, build-retired-ui.cmd and python StockGrassQA/retire-verify.py. Response files and complete logs remain beside those scripts. The initial regression compile needed the existing upstream header include root; the resumed run completed. The initial private UI link retained the original controller due to case-sensitive object replacement; the corrected case-insensitive replacement and fresh fixture passed. These harness failures are preserved separately rather than counted as passing product checks.

Results: ALL_STOCK_MEADOW_PRESETS_PASS covers actual painting, shared-setting history, paint Undo/Redo and cold reopen for all three presets, while the install still contained retired stock files. GPU counts remained 674-675 over 4049 strands for Open/Very Full and 337-338 over 2024 for Full. The registered grass-palette regression passed custom sheet import, loose/legacy project PNG discovery, two independent mixed painted layers, Undo/Redo, erase, six-variety GPU distribution, local cursor/player response, cold reopen and player walking. Ground-paint passed grass coverage, writeback, import, soft edges, erasing, history and reopen. WD01 source contract and git diff --check passed. The package fixture removed all thirty stale bundled filenames, copied only the two approved sheets and retained unrelated install PNGs and project Custom/Mixes sentinels unchanged.

The fresh private UI fixture stock-ui-run-11260 passed STOCK_GRASS_UI_PRESETS_CUSTOM_RETIREMENT_PASS: four selector entries (Custom plus the three meadow presets), each real preset callback with Undo/Redo, no retired group combo and one retained clickable Custom thumbnail. Its stock-grass-menu.png was visually inspected for the menu only; the private desktop capture has a black viewport and is not rendering acceptance. The QA editor closed after the check. No new standalone wind acceptance is claimed; the owner's video remains the controlling failure. No push, merge or release gate approval is implied. Full clean build/CI and independent exact-version verification remain pending. Next motion task: reproduce the owner's visible flicker/stutter in the normal Runtime and establish its cause before further wind claims.

# Meadow stock integration and prairie motion correction, 23 November 2026

Stock implementation 9455e47 adds three Terrain > Vegetation presets via VegetationService and WD01Vegetation, two owned sheets in Content/terrain/foliage/Presets, documentation, feature matrix entry REN-VEG-002 and native proof tools. Meadow Open uses the narrow atlas at nominal 50 tufts/m2; Full and Very Full use the broad atlas at 25 and 50. Common geometry is length 0.5, width 0.9, two segments/two cards. The terrain-wide density multiplier now supports 20. Preset selection is a command-backed shared-terrain settings edit, with an explicit tooltip; existing textures remain assigned. Selecting an individual plant restores the existing mixed-palette workflow. Native WISCENE stores crop rectangles and material calibration. Versioned project texture filenames let Undo/Redo recreate the same layout without changing GrassAtlas ABI.

Windows x64 Release compiled VegetationService, GroundPaint, WD01Vegetation and the proof tools using existing matching dependency/object caches. The isolated StockGrassStudio install and its private UI controller linked. StockGrassPresetProof passed all three actual paint/settings-history/paint-history/cold-reopen paths, with six-variation GPU counts 674-675 over 4049 strands for Open/Very Full and 337-338 over 2024 for Full. The UI controller invoked the real selector callbacks, undid/redid each settings change, selected an existing thumbnail and verified the existing five-entry marine filter. Logs report ALL_STOCK_MEADOW_PRESETS_PASS and STOCK_GRASS_UI_CALLBACKS_HISTORY_FILTER_PASS. The existing WD01VegetationSourceContract CMake script exited zero. Production Runtime/Studio source outside the isolated worktree and upstream pins were not changed.

IMPORTANT CORRECTION: the first review/field emitters omitted mesh subsets. Native CreateFromMesh consequently found no usable index distribution and reconstructed/reset hair buffers every frame. Tiny changes in a whole-GPU-buffer hash were insufficient motion evidence. The owner twice correctly reported static grass. Earlier motion claims and field timing runs are invalidated, including the first CSV in 9455e47. Corrective implementation 094af28 adds the six-index emitter subset in FoliageReferenceReview, HairGrassAtlasReview and GrassFieldReview. PrairieWindReview repairs and validates a loaded emitter, and compares its simulation span rather than unrelated GPU counters. Existing terrain painting uses native terrain subsets and did not share this fixture defect. The earlier artwork approval remains an artwork judgement, not a motion/performance approval.

The valid-emitter uncapped 4096 m2 benchmark used 1920x1009, matching settings and 90 GPU timestamp samples after 75 warmup frames per view, with the current Runtime grass shaders. Bare near-view total Render median/P95 was 1.845/2.151 ms; Wicked at 25 tufts/m2 3.298/3.645; Open at 50 4.754/5.291; Full at 25 3.710/4.099; Very Full at 50 5.535/6.205. The corrected CSV replaces docs/evidence/meadow-field-2026-11-23.csv. Near/25m/60m captures and receipts passed. These are complete native Render intervals in an isolated field, not grass-only GPU cost, populated-world gameplay FPS or a release gate. Local reproduction: GrassFieldQA/correct-field-benchmark.cmd, correct-field-benchmark.log and CorrectedNative. Prior invalid runs remain preserved separately.

PrairieWindReview authors a gentle directional ambient force (strength 0.08, radius 500) with slow direction variation and four broad directional gusts (strength 0.4, radius 20). Native translation animations move the gusts across the 64m field at 3 m/s, with staggered phases and a 40-second loop. It uses no new upstream shader or Runtime feature. The saved/reopened scene contains five forces and five native animations with 204800 strands. Native author captures visibly show the passing bend and recovery. The standalone Runtime diagnostic verified advancing gust positions, positive simulation time and actual simulated tip angles: sampled means around 11-20 degrees and peaks around 35-45, versus only a few degrees with the invalid resetting emitter. The private RuntimeApplication diagnostic build and readback logs are QA-only files, not committed production changes.

The normal staged Runtime then ran the corrected scene at a sampled 75 FPS. Captures from a fixed view had mean RGB difference about 12.05 over the grass versus 0.92 over ground and zero in sky, consistent with visible grass motion; subsequent frames show the owner pausing and closing the game. Do not reopen a deliberately closed review repeatedly or mistake pause/quit for a crash. The final player is currently closed. The owner has not yet approved the prairie motion quality.

The ready review is StockGrassQA/Playable/PrairieWindReview/PrairieWindReview.renegade. A fresh project identity was created. Its missing scene envelope was then generated and read back through IdentityService, with type scene, the new project ID and a fresh document ID; create-prairie-project.log reports PRAIRIE_SCENE_IDENTITY_PASS. This repairs the specific missing-Main.wiscene.rmeta quick-load error seen in a capture; a complete save-game acceptance pass is not claimed. Desktop shortcut Prairie Wind Review launches the normal player using open-prairie-stable.py. Open Prairie Wind.cmd is kept beside that script. The StockGrassStudio executable is the separate review editor install. All output/log names in this entry are under the local owner QA workspace; no assets were published.

No push or merge occurred. Full clean configuration/hosted CI and independent exact-version verification remain before release. Next: owner judges the corrected prairie movement and stock choices; refine gust strength/timing if requested, then prepare the bounded integration candidate.
# Owner approval of three grass presentations, 23 November 2026

The owner reviewed the live Grass Coverage Review and said all three Renegade grasses look good. This approves the visual direction for previous narrow-clump grass at 3600 tufts and revised broad-clump grass at 1800 and 3600 tufts. Keep both atlas styles and the three review presentations available. These are two textures at three style/density combinations, all using native hair rendering. This does not establish a whole-level performance gate, distance/mip acceptance, production palette integration, merge permission or distribution approval. The implementation and native evidence are recorded below. The active playable review and unrelated importer Studio were left untouched.
# Grass clump coverage correction, 23 November 2026

Implementation 7b977746f7bc59bd851b7779c8a9d7605bb12349 on feature/foliage-512 follows the owner's observation that even the previous very dense grass had larger gaps than Wicked. This is a separate visual trial; production painting defaults remain unchanged.

Inspection at source alpha >= 128 gave mean aspect-weighted visible sprite area about 0.0875 for the previous set and 0.1344 for the normalized Wicked set. This is a source-mask proxy, not a measured screen-coverage percentage. Previous grass also had tall narrow bases, while Wicked includes wider low patches. Raising tuft count alone did not correct the silhouette distribution.

Added assets/foliage/review/renegade_coverage_grass_atlas_v2.png, its visible-root crop sidecar and exact artwork prompt. Six 512-square cells form a 1024x1536 atlas. Several small crowns distribute narrow blades across each broad base. SHA-256 d863e86a0a4cf17fdc714982f7778fd29b3efd9bebe53aaea6af56534a28c562. The new source-mask proxy is about 0.2825. The PNG is unmodified; native alpha cutoff 0.5 remains necessary to reject its low-alpha halo. No claim is made for alpha blending or distant mip quality.

The generated sheet was lighter than v1. Coverage mode applies only to the v2 material a linear RGB base multiplier of (0.670896, 0.660796, 0.778305), derived from the average linear colour ratio of alpha-qualified source pixels. Other plots retain white multipliers. This calibrates the new artwork towards the earlier accepted colour, rather than silently changing its PNG.

Tools/HairGrassAtlasReview.cpp now accepts --coverage previous-atlas.png after its four normal arguments. Four 72-square-metre patches run left to right: Wicked 1800 at X-12, previous v1 3600 at X-4, v2 1800 at X4 and v2 3600 at X12. Common height/width, two segments, two cards, randomness and seed remain unchanged. Counts are asserted after cold reopen; six new rectangles are checked. The default and --dense modes remain available.

Windows x64 Release compile, link, save/cold reopen and four native captures passed. The author printed REBUILT_HAIR_ATLAS_REVIEW_PASS and the harness printed HAIR_ATLAS_BUILD_AND_NATIVE_REVIEW_PASS. Native close-ups show much fuller coverage at 1800 and 3600 than the previous narrow-clump trial. The owner must still judge appearance and coverage.

Reproduction is under E:\RenegadeAssets\HairGrassRebuild: prepare-coverage-review.py, build-coverage.py/.cmd, build-coverage.log, response files and open-coverage-review.py. The playable project is CoverageGrassReview/CoverageGrassReview.renegade. Captures hair-plot-0 through hair-plot-3 and the receipt are in Content/Scenes. Old projects and PNGs remain available.

The fresh standalone DX12 Runtime PID 8036 opened Grass Coverage Review at 1920x1009. Focused player capture CoverageGrassReview-Focused.jpg shows the player inside the revised 1800 patch and a 75 FPS overlay. Live diagnostics had 1430 submitted frames, unpaused, X3.39 Z3.90 after starting at X4 Z-3, demonstrating advancing frames and movement. This is a spot observation, not an isolated GPU cost or whole-level benchmark. The preceding review Runtime was closed; importer Studio was preserved. No merge, push or production rollout occurred.

# Open grass atlas and density review, 23 November 2026

Implementation `d3dcadd` on `feature/foliage-512` adds `Tools/HairGrassAtlasReview.cpp` and the original trial PNG, crop metadata and README in `assets/foliage/review`. The owner said the revised artwork looks better than Wicked's original and requested very dense coverage. Treat this as artwork direction approval; density judgement and production integration are still pending.

The final atlas is 1024 by 1536 RGBA, with six 512-square cells. SHA-256 `c083bbb89d84d7df9e7ac353dcbd4b6ed131f81dc9b51afc51d0d50321b77113`. The sidecar uses visible alpha-threshold crop rectangles to remove root padding without resampling or changing the PNG. The first bright, uniform trial was rejected internally; the final native capture has subdued green/brown variation and small seed heads. The source retains low-alpha fringes, so the native 0.5 cutout threshold is required in this trial. Alpha-blended use and distant mip quality are not certified.

The default author creates two matched 300-strand plots, comparing Wicked artwork with the six new sprites. The optional `--dense` mode creates three 72-square-metre patches: Wicked 1800 strands at X-8, new 1800 at X0 and new 3600 at X8; all span Z2-14. The new patches are six and twelve times the first trial's density. Shared settings are length 0.5, width 0.9, two segments, two cards, randomness 0.25, seed 17. Stock sprite size ratios are normalized to their mean. Both materials have white base colour, roughness 0.9, reflectance 0.02, alpha cutoff 0.5. The renderer remains native hair; no per-plant mesh instancing or renderer modification is involved.

Built and linked the standalone author in Windows x64 Release using the existing native dependency libraries. Both matched and dense author runs exited zero with `REBUILT_HAIR_ATLAS_COLD_REOPEN_PASS`, `REBUILT_HAIR_NATIVE_CAPTURES_PASS` and `REBUILT_HAIR_ATLAS_REVIEW_PASS`. Cold reopen asserts plot count, density, geometry controls and six new atlas rectangles. Native close-ups were visually checked for both new densities. Texture embedding is enabled before scene loads and save. This work did not change production palette defaults or upstream pins.

Reproduction, response files and logs are under `E:\RenegadeAssets\HairGrassRebuild`: `build-review.py/.cmd`, `build-dense.py/.cmd`, `build.log`, `build-dense.log`, `open-review.py` and `open-dense-review.py`. The source scene comes from the original 512px foliage QA. The playable projects are `HairGrassReview/HairGrassReview.renegade` and `DenseGrassReview/DenseGrassReview.renegade`. Their project-local reference textures and sign PNGs are required. Native captures and receipts are beside each Main.wiscene.

The final dense standalone DX12 Runtime is PID 39232, 1920x1009 client size, with the importer Studio untouched. Early maximization left a static first frame; sending focus and current client size again restored advancing frames and mouse pitch. The corrected focused capture shows all three dense plots and 75 FPS. Four live snapshots advanced from 3319 to 3775 submitted frames across 6.07 seconds, giving 75.22, 75.07 and 75.06 frames/second between samples. Evidence: `DenseGrassReview-Focused.jpg`, `dense-live-throughput.json`, `dense-owner-receipt.json`, `dense-owner-pid.txt`. This is capped scene throughput from the starting view on the RTX 4070 Ti, not isolated GPU grass cost, a whole-level benchmark or an overdraw/streaming release gate.

Next: owner walks the centre and right patches to judge close-range dense appearance. Then integrate the accepted art and per-species controls into painting, and test distance/mips and representative large-area coverage. No merge or publication was performed.

# Uploaded mesh foliage visual review, 23 November 2026

Implementation `7621d2800391d9b8f1ffa5a8b80594186f6ee775` on `feature/foliage-512` extends `Tools/FoliageReferenceReview.cpp`. The owner reviewed the playable scene and said all variants are significantly better. Record this as visual direction approval, not production integration, distribution rights, merge permission or a performance release gate.

Four uploaded archives were verified byte-for-byte against the attachments. First grass pack SHA-256 `0bf1410176e2287bd4c2e56c52a1eadbb3b8fe56638f1e240a248f109afdcb42`; daisies `8816cf3d7b8514e5e01a6a2f8602831f6366432737bacaaefc2b25e777e49af5`; undergrowth `bf6e9c392508e3fd2441457e495b26da6d2492dd8a9bb86d99d1dd280db27fbc`; nine-variant grass `f6753cb944a581d0a3f0aeb3df07608739a11dee2dd5e38716d10386c7005aaa`.

The first pack has 15 grass meshes at 20-74 triangles each. The second grass pack has nine at 6-18 triangles. Undergrowth has 15 plantain, dandelion and wild lettuce models at 28-82 triangles. Daisies have nine LOD0 models at 283-2402 triangles, with separate LOD1/LOD2 source variants preserved. The preview uses LOD0 only; automatic distance switching is not wired. Asset-zoo ground and human reference meshes are excluded. Blender conversion reconnects source colour, alpha, normal and roughness maps, converts the nine-variant pack's gloss to roughness, recentres roots and preserves metre scale. Original archives are unchanged. Normal maps and colour maps were verified on native imported materials. AO and subsurface maps are not separately connected in this trial.

The review adds 240 placements sharing 15 first-pack grass meshes, 36 placements sharing nine daisy meshes, 90 placements sharing 15 undergrowth meshes, and 180 placements sharing nine second-pack grass meshes. All 48 prototypes and 546 placements survived cold native reopen. Instances share native mesh and material resources. Native wind uses root-to-tip vertex weights. There are no plant colliders or automatic mesh-plant interaction in this trial.

The local comparison harness preserves exact compile/link flags and reproduction in `all-build.cmd`, `all-build-generated.py`, `author-compile.rsp`, `author-link.rsp` and `all-build.log`. The source supports the original four-argument invocation, an additional grass GLB, or three more GLBs in daisies/undergrowth/nine-variant-grass order. Source and target scenes, native stock-terrain folder and GLBs are positional arguments. Four native close-up captures are written beside the saved scene. Blender conversion sources are kept in the local QA directory as `prepare-uploaded-grass.py` and `prepare-additional-packs.py`, with export logs and reconnected Blend/GLB files in the owner asset workspace.

An initial native save reopened with white materials because imported embedded GLB images had virtual names and embedding was disabled. The author now enables native `EMBED_FILE_DATA` before loading/importing. Final native captures visibly show correct textured alpha cutouts and leaf geometry in all four groups. Final author exited zero with `ALL_FOUR_UPLOADED_PACKS_COLD_REOPEN_PASS variants=48 placements=546` and `ALL_FOUR_UPLOADED_NATIVE_CAPTURES_PASS`. The saved scene is approximately 18 MB. A fresh standalone DX12 Runtime restored the textures without missing `gltfimport_*.png` warnings. An unused legacy stock-grass DDS warning remains from the seed scene; the new mesh textures are present.

The playable project is `GrassMeshReview/FoliageComparison/FoliageComparison.renegade` under the owner asset workspace. First uploaded grass is front right at X16, Z2-14. Daisies, undergrowth and nine-variant grass are behind at X-8/0/8, Z20-32. The old three reference plots remain. The actual player review ran at a 1920x1009 client size; live diagnostics showed 4,373 submitted frames and player movement to X8.26, Z19.85 near the nine-variant patch. The owner then confirmed the variants are significantly better. No current importer/editor process was closed.

Next: design the mesh-plant palette integration using the accepted visual baseline, per-species size and density, shared meshes/materials, native wind and authored distance LODs. This trial does not implement production painting, collision/interaction, streaming budgets or a populated woodland benchmark. Stock defaults and upstream source are unchanged; no merge or publication was performed.

# Stock versus custom foliage review, 23 November 2026

Review author implementation `67da73f5e39fd1a82f78c85cfafc306bc58a25c1` on `feature/foliage-512`, following `ae0ad34`. The source is `Tools/FoliageReferenceReview.cpp`. This is a local visual comparison, not a change to production palette defaults, a renderer replacement, or a release gate.

The tool loads the existing GrassReview scene and creates three labelled plots on the same terrain: the pinned Wicked stock preset and artwork, a 512px short-grass/daisy mix at the existing settings, and that exact mix with tuned native properties. Each emitter covers 72 square metres and has 900 strands and seed 17. Native stock and current palette both use one segment and one card. Stock length is 2.5 with five independently sized and cropped sprite rectangles. Current custom rectangles are square and all size 1. The tuned review uses length 0.875, width 0.6, three segments, two crossed cards, randomness 0.35, grass size 0.8 and daisy size 0.45. The flower multiplier is 15.75 percent of the previous overall height. The common lighting and ground are unchanged. The remaining colour and silhouette differences belong to the artwork and require owner review.

Built the author with the existing Windows x64 Release native compiler/link flags and dependency libraries. Local commands are preserved by the comparison QA harness in `build.cmd`, `author-compile.rsp`, `author-link.rsp` and `build.log`. Invocation from a staged Runtime directory: `FoliageReferenceReview <source.wiscene> <destination.wiscene> <stock-terrain-directory>`. Runtime Content/terrain/foliage must contain the 512px stock cutouts. Exact source/destination parameters are retained in the local harness. The output writes `FoliageComparison-receipt.txt` with native settings and assertions.

The final author exited zero and printed `THREE_PLOT_SAVE_COLD_REOPEN_PASS` and `COMPARISON_AUTHOR_PASS`. It verifies the three serialized plots and tuned sizes/card/segment counts after a cold native reopen. An initial author shutdown fault was corrected by clearing native scenes and waiting for pipeline creation before teardown. Standalone DX12 Runtime build `257a5c51564466f80e7ae1db589f67873bd29798` opened the final project. Visually inspected the labelled plots. Live diagnostics later recorded over 7,000 submitted frames and player movement from Z -12 to -8.63. The owner review window remains available; no current importer/editor process was closed. This is not an independent Wicked Editor parity run or a dense woodland performance benchmark.

The local review project is `FoliageComparison/FoliageComparison.renegade` in the comparison QA output. Preserve its Content directory and reference textures. Next: owner judges the three plots at close range; then decide whether to change stock artwork and expose persistent per-species presets in the existing painting workflow. No merge or stock-default deployment was performed.

# Stock foliage resolution

Local branch feature/foliage-512, implementation c37b5667fd59adfc4a8881b4f7011a7d854cbd4b. Regenerated the 30 stock cutouts in Content/terrain/foliage at 512 by 512 with straight RGBA, retaining filenames, palette groups and plant anchoring. Updated the dimension check in Tests/WD01VegetationSourceContract.cmake.

Verified with cmake -DRENEGADE_SOURCE_DIR=<checkout> -P Tests/WD01VegetationSourceContract.cmake and PaintHardware.exe <fresh-output> grass-palette using the staged Release assets. Native RTX 4070 Ti readback showed 243 strands distributed 8 or 9 per species for the 30-frame mix. Verified 3072-square full-mix atlas and 1024-square two-species atlases. Cursor and player response, walking and arm pose, import, history and cold reopen passed. Compared meadow-riverbank renders from the two asset versions. Existing project-local mixed atlases persist as authored. This is a local asset change awaiting integration.
# Zone resize and gallery placement

Implementation commit 44bf593d30bb921d690a3a030b6373947a997358 on feature/gameplay-zones. StudioApplication routes single-zone Scale to its six face handles. ZoneStudio enlarges handles in Scale and adds dimension tooltips. Updated architecture and assistant source evidence. The local ZoneAnchorReview project uses one station mesh per zone, in zone-local coordinates. The original gallery projects were retained.

Local x64 Release build compiled StudioApplication, MarkerIconOverlay and ZoneStudio and linked Studio. The gallery fixture proof exercised all 13 presets at 12 m wide, 2 m tall and 0.4 m deep. Checked local pivot and world translation, unit scale, square pad, board aspect and UVs, extended gameplay containment, and undo/redo. Ran native zone tests, marker source contract, 84 assistant workflows and packaged knowledge tests. The standalone DX12 gallery loaded all 13 zones and the boards were readable from the spawn. Desktop shortcuts target the corrected review copy. Hosted CI and owner acceptance are pending.

# Gameplay zone visual identification

Implementation commit: 07ed43606c04314b737adeb7d91788c0db181f99 on feature/gameplay-zones. ZoneStudio, MarkerIconOverlay and StudioApplication draw preset icons, names and matching bounds in Scene. The stock atlas is staged by the Windows Studio packaging check and covered by the marker asset contract. Updated architecture, feature matrix and assistant evidence.

Local Release Studio compiled both changed translation units and linked against the previous configured Release objects. Native gameplay zone cases, the marker icon CMake contract, 84 assistant workflow tests and the packaged knowledge test passed. The desktop review gallery includes 13 matching illustrated stations. Inspected labels and zone dimensions in the DX12 editor and verified signs and 13 zones in the standalone DX12 runtime. Gallery art is review-project scenery. Debug and hosted CI are pending.

# Complete gameplay zone palette

Implementation commit `8982534b3b3731812e633baa38d027a34cef4a55` on local branch `feature/gameplay-zones`.

- The 11 additions complete 13 stock zone packages. Studio owns the dropdown and preset-specific controls. Source changes cover the zone bridge and script authoring service, ZoneStudio and its source editor, RuntimeApplication, RuntimeZoneState, RuntimeLiveDiagnostics, gameplay-zone tests, the bundled Studio package check, architecture, matrix and assistant evidence. The project Lua copy stays editable and a zone in a new unsaved level retains its attachment through the first Save As.
- On Windows x64 DX12 the local Release review builds reused the configured CMake dependency cache, compiled changed sources and linked Studio and Runtime. Run `cmake --build <build> --config Release --target RenegadeStudio RenegadeRuntime RenegadeGameplayZoneTests` on a configured checkout and `ctest --test-dir <build> -C Release -R RenegadeGameplayZoneTests --output-on-failure`. Local review commands included `palette-parallel.cmd Studio`, `palette-build.cmd Runtime` and `palette-finish.cmd`. The native test passed for all 13 packages in two projects, disabled scripts, scene save/reopen, first-save bindings, and activation policies.
- The nine real standalone runs passed using `python palette_walk.py Mix Effects Interact Movement Vitals Teleport Dialogue Transition Win`. Verified message and objective changes, target activation dispatch, actual player health, death and checkpoint returns, one-way teleport arrival, dialogue activation, original ambient cue volume restoration, local weather restoration, and separate Win/Transition Story Flow routes including a named arrival marker. The isolated gallery has 13 placed zones. Normal Studio Test Game launched a snapshot with 13 active scripts. The Scene dropdown was inspected, a preset changed and the edit undone. Review builds and machine-generated evidence are in the local ZonePaletteReview directory.
- All 61 CMake source contracts passed. The two sky contracts used the configured shader overlay proof as in the earlier zone verification. Ran `python Tests/AssistantWorkflowSuite.py --repo . --output <review>/AssistantChecks` (84 passed) and `python Tests/AssistantPackagedKnowledgeTests.py --repo . --output <review>/AssistantPackage` (passed).
- Objectives and checkpoints last for the current level visit. Win and named transitions can be exercised with the full Story Flow launcher. The editor script preview dispatches the saved script's edge in an isolated runtime. Owner acceptance, Debug and hosted CI remain pending.

# Gameplay zone picking and editable Lua

Implementation commit `e4e86eb313c22d941ee8f9572b41fd97ade49bd7` on the local `feature/gameplay-zones` branch.

- Click a visible face in the Scene viewport to reselect a zone. Picking uses the ray intersection with the transformed box and respects closer scene geometry. Normal gizmo and resize-handle handling stays available.
- Studio includes the Trigger and Win packages under Content/ScriptLibrary/RenegadeZones. Adopting a preset uses its project-owned source in Content/Scripts/Library. The zone inspector opens a Lua source window with Save, Preview Enter and Preview Exit. A save or clean external edit reruns the isolated preview. A launched game reads the project source when its scene starts.
- Changes cover ZoneService, Studio selection and zone authoring, ZoneScriptEditor, RuntimeApplication, RuntimeScriptRuntime, stock packages, build and test definitions, gameplay zone tests, assistant evidence, architecture and feature matrix.
- Local Windows x64 Release builds of Studio and Runtime passed using the existing dependency cache with build writes on C. The native zone test covered rotated faces, inside rays, stable history and reopening, independent project copies, editable behavior and disabled attachments. Reproduce with RenegadeGameplayZoneTests. The two affected Phase 5/6 source contracts, packaged assistant knowledge check and the offline 84-case assistant suite also passed.
- In the private Studio review, a click within an unselected Win Zone face opened its Gameplay Zone inspector. Checked Preview Exit, modified and saved the real Lua source, rejected an invalid draft without replacing the saved script, and verified external changes refreshed the preview. Restored the original source. In the standalone player a single attached Lua Win Zone was active in Level One. Using normal W movement through the doorway produced a successful gameplay level.complete and entered Level Two. Proof is in the private ZoneLuaReview/Proof directory on DESIGN. No Debug build or hosted CI run in this pass.

# Scene gameplay zones

Implementation commit `929158a933b9ff9d1a6b2d83d69a306bc9f5c480` on the local `feature/gameplay-zones` branch.

- Studio exposes Zones in the Scene hierarchy and Add menu. Place a box on a surface, switch its preset, focus it with F and resize with any of its six handles or the width, height and depth controls. Min/max bounds, target and enter/exit event names survive preset changes. Only players activate this first version.
- Changed EngineBridge/include/renegade/bridge/ZoneService.h, Studio/src/ZoneStudio.cpp, StudioApplication and RenegadeStudioChrome headers and implementations, RuntimeApplication header and implementation, root/Studio/Tests CMakeLists, Tests/GameplayZoneTests.cpp, both affected Phase 5/6 Gate 3 source contracts, the assistant workflow evidence, docs/ARCHITECTURE.md and docs/FEATURE_MATRIX.csv.
- Local Windows x64 Release verification used isolated dependency-cache builds on healthy storage. Ran `zone-build.cmd Studio`, `zone-build.cmd Runtime` and `zone-test.cmd`. For a configured checkout build `cmake --build <build> --config Release --target RenegadeStudio RenegadeRuntime RenegadeGameplayZoneTests` and run `ctest --test-dir <build> -C Release -R RenegadeGameplayZoneTests --output-on-failure`. Native tests covered creation, history, rotated bounds, another storey, adjacent room, absence of collision/render geometry, preset identity and WISCENE reopening.
- The native Studio review placed a box through Scene > Zones > +, changed the preset to Win Zone, dragged the right face, typed width 3 and saved the disposable terrain scene. The Release standalone player loaded a test scene with the player already inside the Win Zone and advanced Level One to Level Two via the gameplay level.complete action. In a direct launch the LEVEL COMPLETE overlay appeared and Enter closed the player.
- A second standalone project sent review.enter to a single Lua listener on a Win Zone placed 100 metres from the player. The listener checked the target identity and moved the triggering box away. Only upon review.exit after review.enter could the listener move the Win Zone onto the player. The log recorded one active script in Level One, one successful level.complete and the transition to Level Two. Proof images and both runtime logs are in the separate ZoneReview/Proof directory on the review machine.
- Source checks used `cmake -DRENEGADE_SOURCE_DIR=<source> -P Tests/<contract>.cmake`. All 61 source contracts passed, including the two night-sky contracts against copies of the cached configured shaders. Ran `python Tests/AssistantWorkflowSuite.py --repo . --output <checks>` (84 passed) and `python Tests/AssistantPackagedKnowledgeTests.py --repo . --output <package-checks>` (passed). Hosted CI, Debug and independent owner acceptance remain pending. A separate standalone doorway fixture started outside the zone at ground level. Level One remained active at rest. Holding the normal W movement key crossed the doorway 3 m ahead and advanced the flow to Level Two. The review binaries reused the existing dependency cache and its diagnostic build stamp. Use the branch SHA for source identity. Owner review in Studio is next.

# PR 203 Studio verification repair

Implementation commit `16d39a42f65bf853f14ba11bfd6d86fb52be94bf`. The first hosted Studio run failed the same five checks in Debug and Release. Both Windows baseline jobs passed.

- Updated `Tests/SceneUiGate2SourceContract.cmake` to check each workspace action and the Vegetation mode flag. Reverified the evidence for changed Studio and architecture files in `Tools/assistant_knowledge/workflows.json`. The package test and the 84-case offline assistant suite pass.
- Updated `Tests/RiverTerrainFollowProof.cpp`, `Tests/GrassPaletteProof.h` and `Tests/GroundPaintProof.h`. On hosted graphics, normal scene updates publish the nine meshes before offscreen draws exercise the brush. The desktop path retains its full-size captures. The ten-minute limit, live paint/undo cycles, saved masks, GPU grass readbacks and movement assertions remain in force.
- On WARP, a texture-size query in terrain tile baking caused device removal before the next material could load. The overlay in `cmake/GroundPaintOverlay.cmake` supplies each channel's actual source width and height with its material index. The shader uses those dimensions in the existing mip and world-scale calculations. See `docs/ARCHITECTURE.md`.
- Local verification used a separate checkout and existing CMake-built dependency libraries. The changed native sources were compiled for Debug and Release. The repository offline compiler produced the embedded DXIL and SPIR-V shaders. The WARP binaries used the repository's test-only adapter selection. Ran both native cases through `Tests/RunRiverDefaultRecovery.cmake` with CTest on WARP and the RTX 4070 Ti. The pairs passed in 68.68 and 24.50 seconds respectively in Debug, and 58.10 and 21.53 seconds in Release. Both captures showed a continuous path with mixed red and white plants. The grass proof counted 121/122 strands for two varieties and 8/9 per variety for thirty.
- For reproduction in a complete checkout, build `RenegadeStudioCI` in both configurations. Run `ctest --test-dir BUILD/renegade -C <config> --output-on-failure`. For a dedicated WARP test build configure with `-DRENEGADE_TEST_FORCE_WARP=ON`, set `RENEGADE_HOSTED_HEADLESS_GRAPHICS=1` and run `ctest --test-dir <build> -C <config> -R RenegadeVegetation_ -V`. Sixty source contracts also passed locally. The full local aggregate run was interrupted by a filesystem health warning. Use the replacement hosted matrix for full-suite acceptance. The PR stays draft pending owner review.

# Owner-review fixes for mixed foliage and live terrain painting

Implementation `f2a22afc6a2600ad85b0141ce108594bdee464cd`. CTest registration `e6cb76ef6827152637a686ce25fe69beb4c3e0b6`. Use `BUILD/recovered-r1-ground/TerrainPaintReview/TerrainPaintReview.renegade` for the saved mixed poppy/daisy path on DESIGN.

- The Studio crash was traced to DX12 terrain tile writeback after a brush invalidated the pending resolution. The overlay in cmake/GroundPaintOverlay.cmake now uses the live residency resolution for GPU copies and handles a previous-frame blendmap after Undo. Tests/GroundPaintProof.h paints and undoes new layers between scene update and rendering.
- EngineBridge/src/VegetationService.cpp selects deterministic per-strand palette sampling for newly painted and reopened mixed grass. Each complete group of strand IDs uses every selected frame once. Tests/GrassPaletteProof.h reads actual GPU UVs, checks spatial mixing and compares assignments after reopen.
- Rebuild the Studio, Runtime and RenegadeRiverTerrainFollowProof targets for Windows x64 Debug and Release. Run `ctest --test-dir BUILD/renegade -C <config> -R "RenegadeVegetation_|MarkerIconsSourceContract|WD01VegetationSourceContract|S1AInspectorSectionSourceContract|S1BInspectorMigrationSourceContract|RockyRiverbedDeployment" --output-on-failure`. The seven focused checks passed in both configurations. The two native vegetation cases now run in ordinary Studio CI.
- On the RTX 4070 Ti the two-species patch returned 121/122 strands, with identical frame choices after reopen. The 30-species brush returned 8 or 9 per variety across 243 strands. Visual inspection of ground-paint.png showed red poppies and white daisies interspersed along both edges of the path. Coverage is by strand count; sprite size and alpha affect apparent area.
- The private Studio input check dragged the path on a disposable copy, undid and redid the stroke and launched Test Game. Release also passed the flat-rocky river regression and a standalone Runtime walk of the recovered scene. Hosted results and final owner visual acceptance are separate.

# Terrain material painting in Vegetation

Implementation commit `3789ce2afe9d748f5ff0bfedefab932c256768bc` on `feature/vegetation-grass`. Review locally with the saved `BUILD/ground-review-release/TerrainPaintReview/TerrainPaintReview.renegade` project. The review level includes a path painted in two passes, a sculpted bank and particle grass. Final ground artwork and independent owner sign-off are pending.

- Studio routes Vegetation → Terrain Painting to GroundPaintPanel. Pick one thumbnail, choose Paint, Erase Material or Soften Edge and drag over the terrain. The inside ring marks the start of the falloff. Tile Size sets metres per texture repeat. Set as Base applies the selected background material and the automatic rock settings.
- GroundPaint in EngineBridge owns the palette, import, native masks, per-material settings and history. TerrainService updates the automatic rock mask after sculpting. VegetationService preserves authored grass and Rocky Riverbed resolves its own material slot. The renderer overlay is in cmake/GroundPaintOverlay.cmake. See the implementation commit for the exact changed files.
- Temporary stock is nine Wicked terrain choices and Renegade Rocky gravel. Project-relative custom materials supply colour, optional normal and packed surface or separate PBR channels. Final approved artwork can replace this review selection.
- Windows x64 local build uses the existing configured build tree. To reproduce, use `cmake --build BUILD/renegade --config Release --target RenegadeStudio RenegadeRuntime RenegadeRiverTerrainFollowProof`, then repeat using Debug. Run `RenegadeRiverTerrainFollowProof.exe <fresh-output-directory> ground-paint` from each built Tests configuration directory. Both Debug and Release produced GROUND_PAINT_IMPORT_SOFT_EDGE_ERASE_GRASS_HISTORY_REOPEN_PASS on DX12. The proof checks a shared chunk boundary, reversible grass coverage, exact lower masks after erase, stroke undo/redo, softening, cliff exposure after sculpt/flatten, custom PBR maps and cold reopen.
- Both configurations passed `ctest --test-dir BUILD/renegade -C <config> -R "MarkerIconsSourceContract|WD01VegetationSourceContract|S1AInspectorSectionSourceContract|S1BInspectorMigrationSourceContract|RockyRiverbedDeployment" --output-on-failure`. Release grass-palette regression passed. Studio Test Game launched the saved level. The separate Release Runtime also opened the project with `RenegadeRuntime.exe dx12 --project <review-project>` and produced a capture after walking.
- Visually checked the native terrain render and Studio palette. Repeat visual review with final artwork at the intended texel density. No broad populated-level performance benchmark or hosted CI result is claimed for this feature.

# Plant groups and custom foliage

The local source checkpoint is `20acf418a705e9618d532fc6ea71bdb8711bd80d` on `feature/vegetation-grass`. Open **Vegetation → Grass → Painting**. The group selector starts on Grass and weeds. All plants includes 10 grass and weeds, five tropical freshwater plants, five marine plants, ten alien plants and any project imports. Switching groups preserves every plant in the brush.

Studio's **Import Plant Texture** accepts a transparent PNG or an evenly divided PNG sheet with up to 64 cells. The importer writes occupied cells to `Content/Vegetation/Palette/Custom`. An individual PNG placed directly in that folder is discovered when Custom or All plants is selected. Earlier PNGs in the parent Palette directory still appear in Custom. New projects have the folder from creation and earlier projects acquire it when the palette opens. This milestone covers foliage colour and alpha. User-painted terrain PBR materials remain the next milestone.

Changes are in `EngineBridge/include/renegade/bridge/GrassPalette.h`, the GrassPalette, ProjectService and VegetationService bridge sources, `Studio/src/WD01Vegetation.cpp`, the two focused tests and the Alien, Freshwater and Marine folders under `Content/terrain/foliage`. Use `git diff --name-only 8307ff0 20acf41` for all source and asset paths. The signatures and dimensions of all 30 named stock files are covered by the existing vegetation CTest contract. The GPU test decodes the 30 files together, checks species counts and builds a mixed brush, exercises a two-cell project import, and discovers both a manually dropped uppercase-extension PNG and a legacy import.

Windows x64 local verification used the configured native Debug and Release outputs. For each configuration, the MSBuild targets were `RenegadeTextureCook`, `RenegadeEngineBridge`, `RenegadeRiverTerrainFollowProof`, `RenegadeStudio` and `RenegadeRuntime` with `/p:BuildProjectReferences=false` against built dependencies. All three executable directories contained all 30 stock PNGs. Reproduce the focused checks in PowerShell from the checkout:

```powershell
foreach ($cfg in 'Release', 'Debug') {
    cmake --build BUILD/renegade --config $cfg --target RenegadeStudio RenegadeRuntime RenegadeRiverTerrainFollowProof -j 6
    ctest --test-dir BUILD/renegade -C $cfg -R 'MarkerIconsSourceContract|WD01VegetationSourceContract|S1AInspectorSectionSourceContract|S1BInspectorMigrationSourceContract' --output-on-failure
    Push-Location "BUILD/renegade/Tests/$cfg"
    .\RenegadeRiverTerrainFollowProof.exe "$PWD/plant-groups-$(Get-Date -Format yyyyMMddHHmmss)" grass-palette
    Pop-Location
}
```

All four focused contracts passed in both configurations. Both GPU runs passed bending under the cursor and player, steady walking and arms, multi-species painting and erase, Undo/Redo, and cold reopen. A standalone Release run on an isolated Windows desktop completed the 720-frame walk and saved the native player-camera capture. A temporary isolated Studio test controller exercised the production dropdown and thumbnail callbacks on a copy with one project PNG, verified the six visible counts (10 default grass, 31 All plants, 5 freshwater, 5 marine, 10 alien, 1 Custom), and selected grass, alien and custom plants in one brush. The open menu and filtered grid were captured from Studio's native 2D render target. The controller and fixture are local QA files under the ignored BUILD directory. Human review of this dropdown and hosted CI are pending.

# Vegetation workspace and grass palette

The local code checkpoint is `3dfc6eaf05bfbfc53564a85c7ebe72a05674f473` on `feature/vegetation-grass`, based on main `cb6bef622a292b6d451465b475b73130fc44858b`. The Vegetation toolbar button opens Grass, Terrain Painting and Terrain Props. Grass accepts the ten stock cutouts and project PNG imports. Select multiple thumbnails, arm Paint, then drag on terrain. Test Interaction bends the grass under a held left mouse button. The native player capsule bends grass in Runtime. The Terrain Painting section currently exposes the existing material controls. A terrain PBR paint brush is a subsequent milestone, after owner review of Grass.

## Implementation

- `Studio/src/WD01Vegetation.cpp`, `TerrainInspectorSections.cpp`, `TerrainPropPalette.cpp`, `StudioApplication.{h,cpp}`, `RenegadeStudioChrome.{h,cpp}`, `RenegadePhysicsLabStudioChrome.cpp` and `StudioLiveDiagnostics.cpp` own the palette, gesture history, workspace transitions, visibility and diagnostics.
- `EngineBridge/src/GrassPalette.cpp` and `include/renegade/bridge/GrassPalette.h` implement the PNG and sheet importer and content-addressed atlases. `PayloadDigest.h` exposes the existing hash function to the texture library. `ResourceAssetCacheIdentityService.h` uses the shared declaration.
- `EngineBridge/src/VegetationService.cpp`, its public header and `PlayerService.cpp` own terrain hair masks and the native force/collider integration. The ten PNGs live under `Content/terrain/foliage`. Studio, Runtime, EngineBridge and Tests CMake targets build or stage these resources.
- `Tests/GrassPaletteProof.h` extends `RenegadeRiverTerrainFollowProof` with a seeded project. `Tests/TerrainInitializationProof.h` and `Tests/MaximumTerrainRiverProof.h` also pass the scene to the vegetation history capture. Run `git diff --name-only cb6bef6 3dfc6ea` for the full code and asset inventory.
- The pinned Wicked grass shader samples base colour and alpha. Lighting uses the generated blade normals and the renderer's AO buffer when available. The grass atlas requires one RGBA image per species. The limits are 64 selected cutouts per mix and 4096 pixels per render atlas. Many distinct overlapping mixes can add hair instances to a chunk. Performance in a dense production woodland remains unmeasured.

## Player movement regression

The owner observed a steady 75 FPS with slower movement and vibrating placeholder arms around the review patches. On a flat route before the correction, the collision world held 50 updates on the approach. Turning off the player's GPU grass collider produced the same trajectory. The approach averaged 2.77 m/s and the arms moved up to 8.04 mm between frames as input switched between walking and idle. The camera and view rig stayed aligned. Re-requesting an already resident terrain chunk temporarily cleared its readiness. `EngineBridge/src/TerrainPropCollisionWorld.cpp` now recognises the current generated mesh and live native body when the chunk is requested again. Invalidated chunks and bodies awaiting a refresh still require the ordinary terrain update. With the correction the approach, painted patch and far side all averaged 4.49 m/s, with zero movement holds and a maximum vertical arm step of 1.86 mm during walking. The reverse route put the problem inside the painted patch. Before the change it held 50 updates and averaged 2.77 m/s in the grass. After the change it held zero and averaged 4.49 m/s there. The fast lookup reads the generated chunk table only when its worker is idle. The native regression walks the route at 75 updates per second with 60 Hz physics and explicitly checks the invalidated-chunk case.

## Verification on Windows x64 and DX12

Commands for the CMake build, the focused contracts, and a reviewer reproducing the native scene are below. Run the native executable from its staged directory and supply a fresh output directory for each configuration.

```powershell
cmake -S . -B BUILD/renegade
cmake --build BUILD/renegade --config Release --target RenegadeStudio RenegadeRuntime -j 6
cmake --build BUILD/renegade --config Debug --target RenegadeStudio RenegadeRuntime -j 6
ctest --test-dir BUILD/renegade -C Release -R "MarkerIconsSourceContract|WD01VegetationSourceContract|S1AInspectorSectionSourceContract|S1BInspectorMigrationSourceContract" --output-on-failure
cmake --build BUILD/renegade --config Release --target RenegadeRiverTerrainFollowProof RenegadeTerrainPropCollisionWorldTests -j 6
Push-Location BUILD/renegade/Tests/Release
.\RenegadeRiverTerrainFollowProof.exe "$PWD/grass-review-output" grass-palette
Pop-Location
```

The native grass proof passed in both Debug and Release on the RTX 4070 Ti. It retained exact per-mix masks across Undo/Redo, erase, scene save, cold reopen and native updates. A four-cell sheet yielded its two occupied cells. Both layers received a length edit. GPU vertex readback confirmed that the cursor bent one patch while the other stayed still, and the player capsule bent the second patch. The transient cursor force was absent from the saved scene. Both configurations printed `GRASS_CURSOR_AND_PLAYER_GPU_VERTEX_RESPONSE_PASS` and `GRASS_PALETTE_MIX_IMPORT_HISTORY_COLD_REOPEN_PASS`.

The four focused source contracts passed. In native Release Studio the Vegetation toolbar opened, two thumbnails could be selected, and one stroke changed 601 vertices in one chunk. The level was saved and inspected. Standalone Runtime opened the project using `RenegadeRuntime.exe --project <GrassReview.renegade>`. The standalone capture and Studio Test Game both produced a DX12 player-camera image. The local review project is under `BUILD/grass-check/ReviewGrass/GrassReview`; the UI exercise uses a disposable copy. Preserve the project's Content directory when moving it. Independent review of this checkpoint is pending.

# Terrain props P6 and Physics Lab release candidate

P6 is the release milestone for terrain props. Biome design is a separate project. This candidate combines the woodland content and terrain integration at 9b247a7 with the Physics Lab at 6bcc09c on main 9eae100. The Runtime frame restores the vehicle cockpit projection and synchronizes the vehicle trial. Terrain model residency runs whenever a player or camera provides a position, and the experimental switch still controls only cell thinning.

The occupied chassis owns the terrain detail origin while riding, including the interval before its render transform catches up to native physics. Walking resumes using the character body after exit. The native collision-world regression moves the occupied chassis across multiple 32-metre cells while the walking body stays behind. The stock vehicle brake lights still use their authored emissive material. The old private reference van and its UV-specific mask are excluded from this release.

A maintainer can run the existing Runtime handshake workflow in standalone mode with a candidate ZIP release-asset ID, its SHA-256 and the expected Story Flow node. It checks the named executable in a fresh profile, inspects the native package and records 240 measured DX12 frames after warmup. The four normal pull-request jobs keep their existing commands.

The validations below belong to the named feature commits. The combined candidate requires its own Studio and Runtime builds, native woodland and Physics Lab runs, hosted checks and a standalone launch in a clean Windows environment. The original intermittent Debug expansion Undo crash remains recorded in the woodland handoff.

Windows startup without a default playback device now leaves audio unavailable and skips the native reverb call when its voice is absent. The pinned Wicked source remains governed by the existing generated audio overlay. The native missing-endpoint proof applies the scene audio mix after fault injection and retains the unexpected-HRESULT negative control. The original P6 woodland executable reached DX12 device creation and Player Start on the hosted runner before dereferencing the missing reverb voice. See Actions runs 44885103319 and 44885408823 for the debugger evidence.

# Physics Lab local release candidate

Windows x64/DX12 candidate dated 20 November 2026. The local branch is review/physics-lab-v1, integrated with main 9eae100. The original layout, bindings, vehicle and breakables source was checkpointed in 9e779d3. The Studio-to-Test Game authoring bench is b05ec7b. The new feature source is f3d88e0 with the immediate collider hierarchy refresh in 7393b71.

The Lab includes the existing nine-page layout and vehicle controls, live World timing and counts with Step Once, compound colliders, and editable constraint frames. The compound and diagnostic bridge headers live in EngineBridge/include/renegade/bridge. Native adapters live in cmake/RenegadeCompoundCollider.inc and cmake/RenegadePhysicsDiagnostics.inc with their integration in RenegadePhysicsTempAllocator.cmake. Studio owns the Lab controls and viewport presentation. Shared transform commands rebuild edited constraint frames. Tools/VerifyDestructibles.cpp includes the authored scene and compound collider proofs. The current exposure is recorded in docs/JOLT_GAMEPLAY_COVERAGE.md and docs/FEATURE_MATRIX.csv.

## Local verification

The Studio bench preceded the feature changes. It was authored from the raw level using the Studio controls for a hinged door, sensor volume, pinned curtain and breakable barricade. Undo/Redo, Save, Reopen and DX12 Test Game were exercised. The native check on the exact Studio archive recorded 0.482 m of door travel, sensor enter and exit, no measurable pin drift, 0.924 m of free cloth movement, and four released barricade pieces. Test Game from the rebuilt Studio reported DX12 and successful startup of the saved 21-entity snapshot.

The World viewport counted one paused tick and held its pose. The native proof also checked visible pose feedback, duplicate request handling, timing, awake bodies, constraints and contacts. A hinge pivot was moved with the Studio gizmo and undone. The compound proof reopened four primitives on one native body, raycast through the intended opening, then moved and undid a part. In Studio a box and capsule were added, the capsule was moved with the gizmo and the scene saved and reopened. The reopened shapes kept their positions. A sphere added in the final Studio build immediately appeared as a selected hierarchy child.

Commands run from the repository root on DESIGN.

    cmake -S . -B BUILD/clean-release
    cmake --build BUILD/clean-release --config Release --target RenegadeStudio RenegadeRuntime RenegadeVerifyDestructibles RenegadeVerifyGameplayPhysics RenegadeVerifyClothPins RenegadeVehiclePhysicsTests RenegadeSoftBodyPhysicsTests RenegadePhysicsLuaTests RenegadePhase5Gate2CameraTests RenegadeProjectileSimulationTests RenegadeProjectileWorldTests RenegadeRuntimeProjectileSessionTests --parallel 6 -- /p:UseMultiToolTask=true /p:EnforceProcessCountAcrossBuilds=true /p:CL_MPCount=6
    ctest --test-dir BUILD/clean-release -C Release -R "PhysicsLua|SoftBodyPhysics|VehiclePhysics|Phase5Gate2Camera|ProjectileSimulationTests|ProjectileWorld|RuntimeProjectileSession|GovernedLuaRuntimeSourceContract|StudioUiPolicy|JP01PhysicsLabSourceContract" --output-on-failure --timeout 45

Release builds passed. All ten focused checks passed again after the final Studio hierarchy change. The native DX12 runs below passed from the Release Runtime working directory.

    RenegadeVerifyDestructibles.exe --verify-studio-authoring <Studio-authored-wiscene>
    RenegadeVerifyDestructibles.exe --verify-compound-collider <output-wiscene>
    RenegadeVerifyGameplayPhysics.exe
    RenegadeVerifyClothPins.exe <output-wiscene>
    RenegadeVerifyDestructibles.exe <output-wiscene>
    RenegadeVerifyDestructibles.exe <output-wiscene> --ragdoll
    RenegadeVerifyDestructibles.exe --prop-swaps <local-source-assets> <output-wiscene>
    RenegadeVerifyDestructibles.exe --weapon-breaks <local-source-assets> <output-wiscene>

Logs, save/reopen fixtures and screenshots are retained under BUILD/PhysicsLabSignoff/V1Candidate on DESIGN. Record proof-ctest.log for the final focused runs, verification-native-proofs.txt for the native runs, and verified-test-game.log for the DX12 launch. CompoundPartsAfterReopen.jpg and CompoundHierarchyImmediateAdd.jpg show the saved geometry and immediate hierarchy update.

Diagnostics monitor the current Studio scene. Compound authoring supports eight primitive parts on an ordinary rigid body and is intended for paused authoring. Vehicle damage is deferred for project-scale performance data. Hosted CI and independent review of the candidate are pending. Earlier entries below are historical checkpoints.

# Vehicle cockpit camera authoring

Uncommitted on the DESIGN Physics Lab worktree at 9f3fe46. Boardable vehicle chassis have a Cockpit Camera section in the Scene Inspector. It authors chassis-local position, pitch/yaw/roll and vertical FOV. The advanced lens value uses a 36 x 24 mm reference frame and edits the same FOV. Preview uses the shared Runtime pose rules and hides opaque cockpit glass for the render, while Set From View captures the regular perspective viewport. Reset restores the preset or imported marker. Settings support Undo/Redo and use native scene metadata. The Runtime restores the preceding camera projection when it leaves the authored cockpit view.

Changed EngineBridge/include/renegade/bridge/VehicleCockpitCamera.h, Runtime/src/{RuntimeApplication.cpp,RuntimeVehiclePrototype.h}, Studio/src/{StudioApplication.h,StudioApplication.cpp,S1BInspectorSectionMigration.cpp}, Tests/Phase5Gate2CameraTests.cpp, Examples/Vehicles/VEHICLE_TEMPLATE_LIBRARY.md, docs/{ARCHITECTURE.md,FEATURE_MATRIX.csv} and this handoff.

    cmake --build BUILD/clean-release --config Release --target RenegadePhase5Gate2CameraTests RenegadeRuntime RenegadeStudio -- /m:4 /p:BuildProjectReferences=false
    ctest --test-dir BUILD/clean-release -C Release -R "Phase5Gate2Camera|StudioUiPolicy|JP01PhysicsLabSourceContract|S3GovernedLuaRuntimeSourceContract|ProjectileSimulationTests" --output-on-failure --timeout 30

Five targeted tests passed, including capture on a rotated and nonuniformly scaled chassis, the lens-to-FOV round trip, undo/redo and native scene archive reload. All eight starter projects reached Main Level in the updated DX12 Runtime. The Studio Release build was interrupted during linking by the DESIGN reboot. An incremental rebuild completed and the verified executable was staged in the existing Vehicle Template Library. The Studio proof selected a van mesh, previewed 60 degrees, changed to 90 degrees and saved the scene. DX12 Test Game opened the resulting snapshot, applied the 90 degree cockpit framing and returned to chase view. Proof logs and captures are in the CockpitCamera subfolder of the vehicle release.


# Merlin helicopter and Oriole spacecraft starter projects

Uncommitted on the DESIGN Physics Lab worktree at 9f3fe46. The local Vehicle Template Library now offers Merlin and Oriole as editable starter projects. Both include the imported GLB and its rig notes. Their controllers register an ordinary dynamic Jolt body and apply forces at the native fixed step. Merlin applies lift at the authored rotor hub with cyclic, pedals and an altitude-hold collective. Its runtime animates each authored rotor pivot. Oriole uses the 16 nozzle axes and positions to translate and rotate along six axes and cancels gravity on the registered craft. Neutral controls preserve momentum. Boarding starts on a normal-gravity deck. A separate docking or EVA system would be needed for in-space disembark.

The Studio Vehicle page exposes both modes. C++ offers CommandHelicopter, IsHelicopterGrounded and CommandSpacecraft. Governed Lua offers helicopter_command, helicopter_grounded and spacecraft_command through renegade.physics. The control guide is Examples/Vehicles/VEHICLE_TEMPLATE_LIBRARY.md.

Changed CMakeLists.txt, EngineBridge/include/renegade/bridge/FlightVehicleService.h, EngineBridge/src/PhysicsLuaService.cpp, cmake/{RenegadePhysicsTempAllocator.cmake,RenegadePhysicalVtol.inc}, Runtime/src/RuntimeVehiclePrototype.h, Studio/src/RenegadePhysicsLabWorkspace.cpp, Tools/{AuthorAdditionalFlight.cpp,InstallVehicleTemplateLibrary.ps1,NewVehicleProject.ps1}, Examples/Vehicles/VEHICLE_TEMPLATE_LIBRARY.md, docs/{ARCHITECTURE.md,FEATURE_MATRIX.csv} and this handoff.

    cmake --build BUILD/clean-release --config Release --target WickedEngine_common WickedEngine_emb_shaders RenegadeEngineBridge RenegadeAuthorAdditionalFlight RenegadeRuntime RenegadeStudio -- /m:3 /p:BuildProjectReferences=false
    RenegadeAuthorAdditionalFlight.exe merlin <Merlin_H4_Helicopter.glb> <Main.wiscene>
    RenegadeAuthorAdditionalFlight.exe oriole <Oriole_U2_Spacecraft.glb> <Main.wiscene>
    ctest --test-dir BUILD/clean-release -C Release -R "StudioUiPolicy|JP01PhysicsLabSourceContract|S3GovernedLuaRuntimeSourceContract|ProjectileSimulationTests" --output-on-failure --timeout 30

Release targets and all four focused checks passed. The reopened Merlin scene lifted 10.36 m, flew 6.15 m in the forward sample, yawed and landed at its start height. The reopened Oriole scene measured all six axes and maintained 10.33 m/s during the neutral coasting sample without changing altitude. Both author proofs exercised calls through the live Lua binding. The bundled DX12 Runtime loaded Main Level for all eight templates. Independent interactive sessions verified boarding, departure and cockpit views for Merlin and Oriole, plus landing and disembarking for Merlin. Both authored scenes opened in Studio and Test Game launched their DX12 scene snapshots. Local build and verification records are in the AdditionalFlight subfolder of the existing vehicle release.


# Original Workshop Van starter

Uncommitted on the DESIGN Physics Lab worktree on 9f3fe46. Replaced the shareable van with the original Workshop Van generated by Examples/Vehicles/BuildWorkshopVan.py. The scene is authored from its GLB and Renegade-native primitives; it uses the previous template's gameplay, input and Story Flow documents. Both the Blender source and its GLB travel with new Van projects. The old template and verification copies were moved to a local-only folder outside the vehicle release. The installer and project creator require the original asset for the Van preset.

The authored Jolt chassis has four wheel bindings and a separately bound cockpit steering pivot. The Runtime lights a chassis-owned BrakeLights mesh with its M_VehicleBrakeLights material. The legacy atlas mask remains available to existing local demos. The new project uses no atlas, image maps, Sketchfab geometry or Sketchfab textures.

Changed CMakeLists.txt, Tools/AuthorWorkshopVan.cpp, Tools/InstallVehicleTemplateLibrary.ps1, Tools/NewVehicleProject.ps1, Runtime/src/RuntimeVehiclePrototype.h, Examples/Vehicles/{BuildWorkshopVan.py,VEHICLE_TEMPLATE_LIBRARY.md}, docs/ARCHITECTURE.md, docs/FEATURE_MATRIX.csv and HANDOFF.md.

    blender -b --python-exit-code 1 --python Examples/Vehicles/BuildWorkshopVan.py -- <authoring-output>
    cmake --build BUILD/clean-release --config Release --target RenegadeRuntime -- /m:3 /p:BuildProjectReferences=false
    cmake --build BUILD/clean-release --config Release --target RenegadeAuthorWorkshopVan -- /m:3 /p:BuildProjectReferences=false
    RenegadeAuthorWorkshopVan.exe <WorkshopVan.glb> <Main.wiscene>
    ctest --test-dir BUILD/clean-release -C Release -R "StudioUiPolicy|JP01PhysicsLabSourceContract|S3GovernedLuaRuntimeSourceContract|ProjectileSimulationTests" --output-on-failure --timeout 30

Release targets passed. The scene reopened with the named rig and original materials; native testing drove 31.26 m and braked to 0.00031 m/s. The installed creator made a fresh copy which reached Main Level in DX12. The same project opened in Studio and Test Game loaded its DX12 scene snapshot. The installer rejected the legacy van before creating a library. Visual checks covered the cockpit and both exterior views, steering, skid marks and illuminated brake lenses. All four focused tests passed. The locally staged build is available for owner review.


# Reusable vehicle starter projects and tracked tuning

Uncommitted on the DESIGN Physics Lab worktree on 9f3fe46. The starter creator makes independent editable projects for the van, basic VTOL, Lua VTOL, Kestrel, Vesper and Bastion. It retains the authored scene and entity IDs, assigns a new project ID, rebases owned sidecars and refreshes asset hashes without changing canonical registry formatting. The local library lives in BUILD/PhysicsLabSignoff/VehicleTemplateRelease/Vehicle Template Library. The Merlin H4 and Oriole U2 are model-import starters. The installed catalogue is Windows-only and uses a standalone project creator.

Tracked Player Controls now exposes chassis and station layout, suspension, grip, braking, contact mode and Advanced drivetrain tuning. Changes use the Studio command stack and save to chassis metadata. An unbound chassis can use the default nine anchors per side. Existing named anchors and visual tread loops remain supported.

Changed EngineBridge/include/renegade/bridge/{FlightVehicleService,TrackedVehicleTuning}.h, cmake/{RenegadePhysicsTempAllocator.cmake,RenegadeTrackedVehicle.inc}, Runtime/src/RuntimeTrackedVehicle.h, Studio/src/{RenegadePhysicsLabWorkspace,S1BInspectorSectionMigration}.cpp, Tools/{AuthorBastionTank.cpp,InstallVehicleTemplateLibrary.ps1,NewVehicleProject.ps1}, Examples/Vehicles/VEHICLE_TEMPLATE_LIBRARY.md, docs/ARCHITECTURE.md and docs/FEATURE_MATRIX.csv.

    cmake --build BUILD/clean-release --config Release --target RenegadeRuntime RenegadeStudio RenegadeAuthorBastionTank -- /m:4
    ctest --test-dir BUILD/clean-release -C Release -R "StudioUiPolicy|JP01PhysicsLabSourceContract|S3GovernedLuaRuntimeSourceContract|ProjectileSimulationTests" --output-on-failure --timeout 30

All three Release targets and all four focused checks passed. The final tracked proof verified saved torque, drive, brake, reverse and pivot behaviour. In Studio, a fresh Bastion project opened into its editable scene, selected the chassis, displayed all three contact testers and expanded the Advanced drivetrain controls. Fresh Basic VTOL also passed Studio project validation. The final bundled DX12 Runtime entered Main Level for all six fresh templates. The interactive project creator created a new Bastion project from its numbered menu. Studio Test Game launched the bundled DX12 Runtime and loaded the fresh Bastion scene snapshot. See the local release folder for build and verification logs.


# Bastion T9 cannon effects

Uncommitted in the Physics Lab worktree on 9f3fe46. Confirmed Bastion launches create a larger emissive shell with a layered tracer tail and world-space muzzle flash and smoke. The smoke briefly obscures the gunner's sight and drifts away from the muzzle. Confirmed Bastion hits create a scaled version of the shared visual blast with fire, embers, dust, soot and a transient point light. Rendering instances are retired through the existing projectile lifecycle. The effect uses procedural resources and requires no additional asset files.

Changed Runtime/src/RuntimeApplication.cpp, Runtime/src/RuntimeProjectileVisuals.h, Runtime/src/RuntimeGrenadeExplosionFX.h, docs/ARCHITECTURE.md and this handoff.

    cmake --build BUILD/clean-release --config Release --target RenegadeRuntime --parallel 1 -- /p:BuildProjectReferences=false /p:OutDir=<stage>/EffectsRuntime/
    ctest --test-dir BUILD/clean-release -C Release -R "StudioUiPolicy|JP01PhysicsLabSourceContract|S3GovernedLuaRuntimeSourceContract|ProjectileSimulationTests" --output-on-failure --timeout 30

Release built and all four checks passed. The standalone DX12 run opened the existing Bastion trial, fired through the gunner's sight and captured the muzzle flash, sight-obscuring smoke and a separate ground impact blast. The launcher is Play Bastion T9 Effects.cmd in BUILD/PhysicsLabSignoff/BastionCheck on DESIGN.

# Bastion T9 turret trial

Uncommitted in the Physics Lab worktree on 9f3fe46. The tracked demo uses the existing Turret_Yaw and Barrel_Elevation descendants. Mouse aim holds a world-space point at a fixed reference distance while the hull pivots. The authored pivots traverse and elevate at capped rates. Hold Aim for the temporary gunner view. Fire starts a range round from the actual barrel pose in the shared projectile simulation. Each round excludes its firing chassis and the player from contact. The two projected reticles show the intended point and the barrel direction at the same distance.

The existing GameplayInput map supplies Aim, Fire and the look axes. Optional float metadata on Turret_Yaw sets renegade.turret.traverse_deg_s, renegade.turret.elevation_deg_s, renegade.turret.min_elevation_deg and renegade.turret.max_elevation_deg. Defaults are 90 degrees/second traverse, about 49 degrees/second elevation, 10 degrees of depression and 25 degrees of elevation. This trial uses the Bastion mesh's measured muzzle offset and a built-in range-round preset.

Changed Runtime/src/RuntimeBastionTurret.h, Runtime/src/RuntimeTrackedVehicle.h, Runtime/src/RuntimeVehiclePrototype.h, Runtime/src/RuntimeApplication.cpp, Runtime/src/RuntimeApplication.h, Runtime/src/RuntimeProjectileSession.h, Runtime/src/RuntimeProjectileWorld.h, docs/ARCHITECTURE.md, docs/FEATURE_MATRIX.csv and this handoff.

    cmake --build BUILD/clean-release --config Release --target RenegadeRuntime --parallel 1 -- /p:BuildProjectReferences=false /p:OutDir=<stage>/TurretRuntime/
    cmake --build BUILD/clean-release --config Release --target RenegadeProjectileSimulationTests --parallel 1 -- /p:BuildProjectReferences=false
    ctest --test-dir BUILD/clean-release -C Release -R "StudioUiPolicy|JP01PhysicsLabSourceContract|S3GovernedLuaRuntimeSourceContract|ProjectileSimulationTests" --output-on-failure --timeout 30

Windows Release built and all four checks passed. A separate standalone DX12 smoke run opened the existing saved Bastion scene, boarded, exercised mouse yaw and elevation, pivoted the hull while keeping the aim point, switched in and out of the gunner view, and captured a tracer leaving the barrel. No scene serialization changes were required. The first desktop check lost foreground before input and was repeated successfully.

Run Play Bastion T9 Turret.cmd in BUILD/PhysicsLabSignoff/BastionCheck on DESIGN. The next check is the owner playtest for camera feel and traverse tuning. Studio authoring widgets and hosted CI remain open.

# Bastion T9 tracked driving trial

Uncommitted in the Physics Lab worktree on 9f3fe46. The Bastion has a separate local driving range under BUILD/PhysicsLabSignoff/BastionCheck. It uses the authored GLB with a serialized tracked-vehicle tag. TrackedVehicleService registers two groups of nine Jolt wheel stations. The constraint is owned by the native rigid body. The Runtime feeds drive, steering and brake commands and drives the wheels and tread links from the native suspension and track state. The treads follow independent fixed visual loops. The current pass is a Runtime trial with authored bindings.

Changed CMakeLists.txt, EngineBridge/include/renegade/bridge/TrackedVehicleService.h, Runtime/src/RuntimeTrackedVehicle.h, Runtime/src/RuntimeVehiclePrototype.h, Tools/AuthorBastionTank.cpp, cmake/RenegadePhysicsTempAllocator.cmake, cmake/RenegadeTrackedVehicle.inc, docs/ARCHITECTURE.md, and docs/FEATURE_MATRIX.csv.

    cmake -S . -B BUILD/clean-release
    cmake --build BUILD/clean-release --config Release --target WickedEngine_common WickedEngine_emb_shaders --parallel 2 -- /p:BuildProjectReferences=false
    cmake --build BUILD/clean-release --config Release --target RenegadeAuthorBastionTank --parallel 1 -- /p:BuildProjectReferences=false /p:OutDir=<stage>/Author/
    cmake --build BUILD/clean-release --config Release --target RenegadeRuntime --parallel 1 -- /p:BuildProjectReferences=false /p:OutDir=<stage>/Runtime/
    <stage>/Author/RenegadeAuthorBastionTank.exe <stage>/Bastion_T9_Tank.glb <stage>/BastionTank/Content/Scenes/Main.wiscene
    ctest --test-dir BUILD/clean-release -C Release -R "StudioUiPolicy|JP01PhysicsLabSourceContract|S3GovernedLuaRuntimeSourceContract" --output-on-failure --timeout 30

The asset hash matched on transfer. The Windows Release builds and all three focused source checks passed. Native proof verified save/reopen, 14 supported wheel contacts on flat ground, a 27.55 m forward run, a 15.98 m reverse run, braking to rest, and a right pivot with about 1.2 cm of translation and opposite track speeds. The drive input applies the parking brake at rest. The DX12 standalone project opened and the interactive run captured boarding, driving, driver and rear-quarter cameras, and exit. Review the trial in Play Bastion T9.cmd. Further work for general creator exposure is to add tracked authoring controls in Physics Lab. No hosted CI or independent release verification was performed.

# Vesper V9 Physical VTOL

Uncommitted work on 9f3fe46. The separate Vesper V9 demo uses the full imported GLB, a box chassis collider and nacelles posed for hover. Physical VTOL is an authored player-controls mode that applies lift, drag and attitude torque immediately before the Jolt step. Moving turns tilt the lift vector. At rest, yaw pivots the aircraft without requiring a bank. FlightVehicleService exposes the commands and ground query to C++ and PhysicsLuaService exposes the same calls to Lua.

The Runtime pilot has chase, cockpit and rear-quarter cameras. The cockpit uses the authored pilot eye. RuntimeVtolThrusterEffects binds the two exhaust markers on boarding, adds two short-lived additive layers per nozzle and stops emission on touchdown or exit. The demo poses both nacelles vertically.

This pass touches CMakeLists.txt, EngineBridge/include/renegade/bridge/FlightVehicleService.h, EngineBridge/src/PhysicsLuaService.cpp, Runtime/src/RuntimeVehiclePrototype.h, Runtime/src/RuntimeVtolThrusterEffects.h, Studio/src/RenegadePhysicsLabWorkspace.cpp, Studio/src/StudioApplication.cpp, Tests/JP01PhysicsLabSourceContract.cmake, Tools/AuthorVesperVtol.cpp, cmake/RenegadePhysicalVtol.inc, cmake/RenegadePhysicsTempAllocator.cmake, docs/ARCHITECTURE.md, docs/FEATURE_MATRIX.csv, and docs/PHYSICS_LAB_VEHICLE_TEMPLATES.md.

    cmake -S . -B BUILD/clean-release
    cmake --build BUILD/clean-release --config Release --target WickedEngine_common WickedEngine_emb_shaders RenegadeEngineBridge RenegadeRuntimeBootstrap --parallel 1 -- /p:BuildProjectReferences=false
    cmake --build BUILD/clean-release --config Release --target RenegadeAuthorVesperVtol --parallel 1 -- /p:BuildProjectReferences=false /p:OutDir=<stage>/Author/
    cmake --build BUILD/clean-release --config Release --target RenegadeRuntime --parallel 1 -- /p:BuildProjectReferences=false /p:OutDir=<stage>/Runtime/
    cmake --build BUILD/clean-release --config Release --target RenegadeStudio --parallel 1 -- /p:BuildProjectReferences=false /p:OutDir=<stage>/Studio/
    ctest --test-dir BUILD/clean-release -C Release -R "StudioUiPolicy|JP01PhysicsLabSourceContract|S3GovernedLuaRuntimeSourceContract" --output-on-failure --timeout 30

Stage is BUILD/PhysicsLabSignoff/VesperCheck. The author executable imported the GLB and saved and reopened Content/Scenes/Main.wiscene under VesperVtol. The native Jolt proof climbed 10.4 m, settled into hover, pivoted with zero measured drift, banked about 18 degrees while changing course and touched down at rest. All three focused checks passed. The staged DX12 standalone Runtime confirmed boarding, three camera positions, two visible plumes following the nozzles in hover and bank, zero emission when parked after landing, and disembark. Studio built and opened. Standalone flight was visually checked. The Studio Test Game launch still needs interactive verification. No CI or independent release verification was performed.

Project launchers are Play Vesper VTOL.cmd and Open Vesper VTOL in Studio.cmd in the stage folder. The next validation step is the Studio Test Game launch and authoring inspection on the owner seat.

# Advanced fixed-wing authoring

Uncommitted follow-up on 9f3fe46. Physics Lab's Fixed Wing page has a collapsed Advanced panel for per-aircraft tuning and a Part Bindings view. FixedWingTuning.h stores sanitized overrides on the chassis. FixedWingRig.h stores role annotations on descendant transforms. Defaults reproduce the stock Kestrel. RuntimeFixedWing.h resolves roles at registration, uses wheel axle plus tyre radius when a contact marker is absent, and applies the selected tuning and force positions to the airframe. FixedWingAerodynamics.h and the generated RenegadeFixedWing.inc support wing stall angle and landing-gear stiffness and damping. Studio edits use the shared undo stack.

    cmake --build BUILD/clean-release --config Release --target WickedEngine_common WickedEngine_emb_shaders --parallel 2 -- /p:BuildProjectReferences=false
    cmake --build BUILD/clean-release --config Release --target RenegadeRuntime --parallel 1 -- /p:BuildProjectReferences=false /p:OutDir=<stage>/AdvancedRuntime/
    cmake --build BUILD/clean-release --config Release --target RenegadeStudio --parallel 1 -- /p:BuildProjectReferences=false /p:OutDir=<stage>/AdvancedStudio/
    ctest --test-dir BUILD/clean-release -C Release -R "StudioUiPolicy|JP01PhysicsLabSourceContract|S3GovernedLuaRuntimeSourceContract" --output-on-failure --timeout 30
    MSBuild <stage>/AdvancedProof/AdvancedProof.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
    <stage>/AdvancedProof/out/AuthorKestrelRunway.exe <stage>/Kestrel_T7_Trainer.glb <stage>/AdvancedProof/Probe.wiscene --prove-flight

Release physics, Studio and Runtime built and the three focused checks passed. The native probe verified stock takeoff, bank, touchdown and stop. It saved and reopened a renamed explicitly bound wheel without the legacy nose marker, plus changed thrust and fade speed. With authored settings, the same five-second power run reached 19.04 m/s versus stock 13.38 m/s. Studio showed the Advanced controls and applied values in the disposable editor project. The owner reported the feature works as expected. Both existing launchers now use the paired new builds. The original runway project is unchanged. The fixed-wing model uses one forward thrust source with speed-dependent fade and three gear positions. Detailed engine models, arbitrary visual hinge axes and independent CI verification remain outside this slice.

# Physics Lab selection outline

Studio compose now displays the selection outline whenever the scene viewport is available, including with the Physics Lab dock open. The outline retains its existing modal and viewport scissor checks. The Release Studio build, StudioUiPolicy and JP01PhysicsLabSourceContract passed. The trainer screenshot in the open Physics Lab confirms the cyan outline on the selected chassis and its visual parts. The existing Studio launcher now uses the corrected build.

    cmake --build BUILD/clean-release --config Release --target RenegadeStudio --parallel 2 -- /p:BuildProjectReferences=false /p:OutDir=<stage>/OutlineStudio/
    ctest --test-dir BUILD/clean-release -C Release -R "StudioUiPolicy|JP01PhysicsLabSourceContract" --output-on-failure --timeout 30


# Kestrel selection in Studio

Physics Lab's geometry picking now shares the viewport bounds and input policy used by navigation. Overlay markers retain their own policy. A fixed-wing descendant picked in the viewport targets its authored rigid-body chassis. The Lab also follows chassis ownership when the user selects an aircraft child in the hierarchy. Aircraft Vehicle settings expose the existing undoable mass and linear/angular damping controls and the player mode. Driving tests remain in Test Game.

    cmake --build BUILD/clean-release --config Release --target RenegadeStudio --parallel 2 -- /p:BuildProjectReferences=false /p:OutDir=<stage>/SelectionStudio/
    ctest --test-dir BUILD/clean-release -C Release -R "StudioUiPolicy|JP01PhysicsLabSourceContract" --output-on-failure --timeout 30

DX12 Release Studio built and both focused checks passed. In the isolated runway copy, clicking the wing selected Kestrel T7 Chassis with and without Physics Lab open. The mass slider applied a change to 1732.8 kg and Undo restored 740 kg. Captures and logs are under BUILD/PhysicsLabSignoff/FixedWingCheck. The original editor window and runway scene were left alone. The existing Open Kestrel Runway in Studio launcher uses the updated Studio and the already tuned flight Runtime.

# Kestrel flight handling tune

RuntimeFixedWing now holds the requested pitch from mouse up/down, with an attitude correction through the physical elevator. Horizontal mouse movement retains nosewheel steering on the ground. In flight it requests a moderate bank, limits the aileron command and eases the wings back toward level after the turn input ends. W/S and braking bindings are unchanged. The trainer's aerodynamic model and the saved runway scene are unchanged.

    cmake --build BUILD/clean-release --config Release --target RenegadeRuntime --parallel 1 -- /p:BuildProjectReferences=false /p:OutDir=<stage>/TunedRuntime/

Release Runtime linked on DESIGN. The HandlingProbe under BUILD/PhysicsLabSignoff/FixedWingCheck was built through the same v145 authoring project and ran the real mouse-to-flight mapping against the saved Kestrel rig and Jolt. A single virtual mouse-up command at 24.01 m/s produced a sustained climb past 22 m at 1.84 m/s. Repeated virtual right turns peaked at 20.9 degrees of bank and 18.5 degrees per second of roll, then settled within 0.4 degrees of level. This was a native controller proof; the currently open game session was not interrupted to stage the new build. The two launchers now use the tuned Runtime and the previously verified parked Studio.

# Kestrel editor stance and mouse controls

Follow-up on the local 9f3fe46 fixed-wing work. The editor had a dynamic fuselage body but no registered gear rig, so it fell through the intended wheel-contact height. The generated Jolt adapter keeps unregistered fixed-wing bodies in authored poses. Registration switches the body to dynamic and wakes it, preserving its authored mass. AuthorKestrelRunway now verifies 120 fixed steps of editor-style parking before registering the same body and running its takeoff, bank, touchdown and braking proof. The verified runway project was preserved.

Fixed wing uses W/S throttle and the existing gameplay look axes for mouse pitch and turn. Mouse X steers the nose wheel on the ground and banks with a little rudder in the air. The virtual stick returns smoothly to neutral when the mouse stops. Right mouse or Space brakes. Other vehicle controllers retain their previous bindings.

The Wicked wrapper and Windows x64 Release Studio and Runtime builds succeeded using the commands below with isolated ParkedStudio and ParkedRuntime output directories. The extended native proof and both existing JP01 physics and governed Lua source contracts passed. Visual Studio verification captured the wheels above the runway. The corrected Test Game runtime launched and verified boarding, new HUD and throttle during the ground roll. The automated mouse flight check was interrupted by loss of window focus. Captures and logs are under BUILD/PhysicsLabSignoff/FixedWingCheck. Its two Kestrel launchers now target the corrected binaries.

# Kestrel T7 fixed-wing runway milestone

Uncommitted follow-up on 9f3fe46bf7b5c6f255aa2e297c8d2b0e2495377c. The Fixed Wing mode is saved in WISCENE metadata. Existing ground, VTOL and Scripted VTOL modes remain available. The runway demo uses the Kestrel GLB and its named gear, controls and eye marker. Native GLB import and save/reopen passed. The verified GLB SHA256 is 68e9f50634d3597c7f41d26222d0bc5c31ccf39848d6f82432aabe42394a5893.

The bridge additions are FixedWingAerodynamics.h and FixedWingService.h. FlightVehicleService.h owns the serialized mode. The RenegadeFixedWing.inc generated adapter evaluates each registered aircraft and its suspension before every Jolt fixed step. RuntimeFixedWing.h binds the rig and RuntimeVehiclePrototype.h uses the existing boarding, camera and input paths. RenegadePhysicsLabWorkspace.cpp exposes the mode. VehiclePhysicsTests.cpp covers load direction, stall and metadata compatibility. Tools/AuthorKestrelRunway.cpp builds and verifies the exact demo scene. The flight guide, feature matrix and architecture notes describe these limits.

Windows x64 Release commands, with trusted worktree directories provided in the invoking process Git configuration:

    cmake -S . -B BUILD/clean-release
    cmake --build BUILD/clean-release --config Release --target WickedEngine_common WickedEngine_emb_shaders --parallel 2 -- /p:BuildProjectReferences=false
    cmake --build BUILD/clean-release --config Release --target RenegadeRuntimeBootstrap --parallel 2 -- /p:BuildProjectReferences=false
    cmake --build BUILD/clean-release --config Release --target RenegadeRuntime --parallel 1 -- /p:BuildProjectReferences=false /p:OutDir=<stage>/Runtime/
    cmake --build BUILD/clean-release --config Release --target RenegadeStudio RenegadeVehiclePhysicsTests --parallel 1 -- /p:BuildProjectReferences=false /p:OutDir=<stage>/Studio/
    <stage>/Studio/RenegadeVehiclePhysicsTests.exe
    ctest --test-dir BUILD/clean-release -C Release -R "JP01PhysicsLabSourceContract|S3GovernedLuaRuntimeSourceContract" --output-on-failure --timeout 45
    MSBuild <stage>/AuthorKestrelRunway.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
    <stage>/out/AuthorKestrelRunway.exe <stage>/Kestrel_T7_Trainer.glb <stage>/KestrelRunway/Content/Scenes/Main.wiscene --prove-flight

The stage is BUILD/PhysicsLabSignoff/FixedWingCheck. It contains build and native proof logs and the KestrelRunway project. Bootstrap, generated physics, Studio, Runtime and all three focused checks passed. The final native proof took off under aerodynamic forces, reported a 0.436 rad bank and 7.088 m/s lateral velocity change, touched down and stopped on the gear. DX12 standalone and Studio Test Game loaded the trainer. The final Test Game session verified boarding, all three camera modes, takeoff, a visible bank, touchdown on the runway, braking to 0 kt and exit. The same final Runtime boarded and lifted both the native VTOL template and the separate Scripted VTOL fixture. Screenshots and logs are in the stage. The mechanics use 740 kg, tuned low chassis damping, spring/damper ray gear and a trainer-specific aerodynamic configuration. They do not yet include fixed-wing Lua control, a general-purpose aircraft tuning panel, flight viewport guides or the later vehicle presets. Full CI and independent sign-off are pending.

# Project-scripted VTOL

Uncommitted Windows x64 Release follow-up on 9f3fe46 in the Physics Lab worktree. Select Scripted VTOL in the hull's Physics Lab Player Controls and attach Examples/Vehicles/VTOLController.lua as a scene Script. Runtime provides the boarded hull with scoped pilot input and accepts one flight command per update. The sample applies 80 percent thrust and 75 percent yaw and retains the hover target. Native VTOL retains its existing mode and input path. The scripted mode reuses the existing camera and boarding flow. W/S and A/D use the existing movement bindings. Space and C continue to be fixed VTOL keys.

    cmake --build BUILD/clean-release --config Release --target RenegadeRuntimeBootstrap --parallel 2 -- /p:BuildProjectReferences=false
    cmake --build BUILD/clean-release --config Release --target RenegadeRuntime --parallel 2 -- /p:BuildProjectReferences=false /p:OutDir=<isolated-runtime-output>
    cmake --build BUILD/clean-release --config Release --target RenegadeStudio --parallel 2 -- /p:BuildProjectReferences=false /p:OutDir=<isolated-studio-output>
    cmake --build BUILD/clean-release --config Release --target RenegadeS3GovernedLuaRuntimeTests --parallel 1 -- /p:BuildProjectReferences=false
    ctest --test-dir BUILD/clean-release -C Release -R "S3GovernedLuaRuntime|JP01PhysicsLab|VehiclePhysics|S5CoreGameplayApiSourceContract" --output-on-failure --timeout 45

Bootstrap, Runtime and Studio Release builds succeeded after relinking Runtime against the rebuilt bootstrap. All five focused tests passed. BUILD/PhysicsLabSignoff/ScriptedVtolCheck/FlyingVehicle.renegade is a separate copy of the flying template. Native authoring saved and reopened the flight mode and the attachment. The standalone DX12 playtest confirmed boarding with E, takeoff with Space, forward travel with W, descent with C and exit with E. Screenshots and runtime logs are in BUILD/PhysicsLabSignoff/ScriptedVtolEvidence. Studio Test Game, the full CI suite and the wider Physics Lab sign-off remain unverified by this change.


# Cockpit steering and Inspector collision shapes

Uncommitted Windows x64 Release follow-up on 9f3fe46. S1BInspectorSectionMigration.cpp adds the rigid body's shape dropdown to the Physics section. It uses CheckCollisionTarget and SetCollisionCommand. RenegadePhysicsLabWorkspace.cpp and VehicleSteeringService.h author an optional child-entity steering wheel, signed local axis and travel to lock. RuntimeVehiclePrototype.h eases the authored mesh rotation toward the native front axle steer angle, sampled by RenegadeVehicleSteering.inc in the generated Wicked adapter. Runtime uses the imported origin and changes only wheel rotation.

    cmake -S . -B BUILD/clean-release
    cmake --build BUILD/clean-release --config Release --target WickedEngine_common WickedEngine_emb_shaders --parallel 2 -- /p:BuildProjectReferences=false
    cmake --build BUILD/clean-release --config Release --target RenegadeRuntime --parallel 2 -- /p:BuildProjectReferences=false /p:OutDir=<isolated-runtime-output>
    cmake --build BUILD/clean-release --config Release --target RenegadeStudio --parallel 2 -- /p:BuildProjectReferences=false /p:OutDir=<isolated-studio-output>
    cmake --build BUILD/clean-release --config Release --target RenegadeVehiclePhysicsTests --parallel 1 -- /p:BuildProjectReferences=false
    ctest --test-dir BUILD/clean-release -C Release -R "JP01PhysicsLab|VehiclePhysics|StudioUiPolicy|InspectorSection|InspectorMigration" --output-on-failure --timeout 45

Studio and Runtime built successfully and six focused checks passed. In Studio, the van collider switched to Sphere and Undo restored Box. In standalone DX12 GroundVehicle Restored, the cockpit wheel visibly moved through a partial turn, reached lock, and returned to centre. The saved legacy van scene now references the three original GLB image payloads as physical files under its Scenes/VanTextures directory. A fresh native reopen found all four populated material texture slots and the steering wheel binding, and the standalone cockpit rendered textured. The original GroundVehicle asset was preserved. Evidence and logs are in BUILD/PhysicsLabSignoff, and the configured demo is under SteeringWheelRestored in that directory. CI and wider Physics Lab sign-off remain open.

---

# Player Start capsule in the Physics Lab

Uncommitted StudioApplication.cpp follow-up on 9f3fe46. Compose now allows the Player Start capsule wherever the editor viewport is available, including the Physics Lab dock. The gizmo, general marker overlay, selection gesture and camera preview retain their previous Lab visibility rules.

    cmake --build BUILD/clean-release --config Release --target RenegadeStudio --parallel 2 -- /p:BuildProjectReferences=false /p:OutDir=<isolated-output-directory>
    cmake --build BUILD/clean-release --config Release --target RenegadeStudioPlayerStartCacheTests --parallel 2 -- /p:BuildProjectReferences=false
    ctest --test-dir BUILD/clean-release -C Release -R "StudioUiPolicy|JP01PhysicsLab|PlayerStart" --output-on-failure --timeout 45

Both builds and all three tests passed. In DX12 Studio the cyan capsule and chassis wireframes were visible together with Physics Lab open on the Van Chassis. Screenshot evidence is in BUILD/PhysicsLabSignoff/player-start-in-vehicle-lab.jpg.

---

# Vehicle physics preview

Uncommitted Windows x64 Release follow-up on 9f3fe46. RenegadePhysicsLabWorkspace.cpp adds a selected car or motorcycle preview of wheel and suspension geometry, plus the Box chassis collider. Draft slider values affect drawing during a drag. The normal command commits on release. The vehicle templates guide and feature matrix document the new authoring control.

    cmake --build BUILD/clean-release --config Release --target RenegadeStudio --parallel 2 -- /p:BuildProjectReferences=false /p:OutDir=<isolated-output-directory>
    ctest --test-dir BUILD/clean-release -C Release -R "JP01PhysicsLab|VehiclePhysics|StudioUiPolicy" --output-on-failure --timeout 45

The focused Release build and three tests passed. In DX12 Studio the van wheel outlines grew during a paused drag, Undo restored 0.254 m, and leaving the Vehicle page hid the preview. A separate edit to 0.516948 m and its wheel mappings survived save and reopen of the disposable van copy. Screenshot evidence is in BUILD/PhysicsLabSignoff/VehicleWireframeEvidence. Motorcycle visual coverage, broader per-page Test Game checks, and tracked vehicle authoring remain open.

---

# Physics Lab dock follow-up

DESIGN Windows x64 Release EngineBridge and Studio built successfully. The Studio UI policy and soft-body regression passed. In DX12 Studio, right-mouse camera navigation moved the viewport with Physics Lab open. With an oak board selected, Environment and Terrain rendered without the Selected Asset overlap. Add Soft Body showed the rigid-physics message and left the barricade intact. The disposable Ragdoll Impact Playtest and screenshots are in BUILD/PhysicsLabSignoff. The screenshots are under FinalStudioUI. Broader Physics Lab authoring and Test Game coverage remain open.


The dock keeps right-mouse viewport navigation available over the scene. Modal tools, menus and dock controls retain their existing input ownership. The Selected Asset Physics section starts hidden and is cleared before Environment or Terrain claims the Inspector.

Soft Body creation now checks the actual mesh instances through the command shared by Studio, C++ and Lua. A rigid-body object or ancestor, such as the intact barricade, prevents conversion. A shared mesh also prevents conversion because Wicked stores one soft-body pose per mesh. Existing soft-body components can still be edited and removed. The barricade fixture and Studio UI policy checks cover these cases.

---

# Native ragdoll barricade proof

Commit 4456ecb adds the ragdoll trial to Tools/VerifyDestructibles.cpp. The Windows x64 Release proof rebuilt successfully. It identified the right foot as the native impactor at 10.0666 m/s, released four pieces, and cleared the passage ray. The light impact fixture still rattled and returned to rest with its collision intact. The ordinary weight fixture passed again.

    cmake --build BUILD/clean-release --config Release --target RenegadeVerifyDestructibles -- /p:BuildProjectReferences=false

Run RenegadeVerifyDestructibles from the Runtime Release working directory. To generate and verify the ragdoll scene, pass an output WISCENE path followed by --ragdoll. The generated DESIGN playtest is under BUILD/RagdollImpactPlaytest. Its launcher uses the existing Release Runtime.

---


# Destructible rattle and Blender authoring

Implementation commit 519bad161adf6990c6c3932d6f0cd5fa543dfcb3. The native destructible listener now handles sub-threshold rattle separately from pending breaks. The physics scene keeps transient rattle poses for visible pieces and restores their authored pose before break or on expiry. The root collider stays fixed. One shared piece collector accepts nested imported transform groups and excludes mesh and material resources. Runtime passes frame time through PhysicsGameplayService.h. VerifyDestructibles.cpp checks the nested structure and optionally loads a real GLB through the static import candidate service.

DESIGN Windows x64 Release Runtime and both native proof executables built successfully. The DX12 playtest launched. The destructible proof passed with both the built-in scene and a static GLB exported from Blender 5.1.1 and imported under a test wrapper. The light impact moved the visible board between -0.0510 and 0.0326 m, then returned it to rest while collision stayed solid. The strong impact at 11.1708 m/s released four pieces and the native path ray cleared. The existing gameplay physics native proof and all eight focused CTest entries passed.

    cmake --build BUILD/clean-release --config Release --target RenegadeVerifyDestructibles RenegadeVerifyGameplayPhysics RenegadeRuntime --parallel 4
    cmake --build BUILD/clean-release --config Release --target RenegadeVerifyDestructibles -- /p:BuildProjectReferences=false
    ctest --test-dir BUILD/clean-release -C Release -R "^(RenegadePhysicsLuaTests|RenegadeSoftBodyPhysicsTests|RenegadeJP01PhysicsLabSourceContract|RenegadeTerrainPropPhysicsTests|RenegadeTerrainPropCollisionWorldTests|RenegadeS1AInspectorSectionFrameworkTests|RenegadeS1AInspectorSectionSourceContract|StudioUiPolicy)$" --output-on-failure --timeout 45

Run RenegadeVerifyDestructibles from the Runtime Release working directory. To test a Blender export with the existing fixture, pass an output WISCENE followed by the source GLB. Use the four-board dimensions and assembled floor pivot in the [breakable objects guide](docs/BREAKABLE_OBJECTS_GUIDE.md). The import candidate and physics were exercised. The interactive Studio placement and authoring steps remain for hands-on review. Continue with the ragdoll impact and debris lifetime checks.

---


# Impact destructibles and barricade playtest

Implementation commit 88c7678d30e92f3a246974d9f3db8fa441753a7b on feature/physics-lab-vehicles. PhysicsGameplayService.h and PhysicsLuaService.cpp expose authoring, a break request and a scene event queue. RuntimeApplication.cpp prepares and steps the service, and RuntimeScriptRuntime.cpp exposes the request to Level-scoped creator Lua. RenegadePhysicsLabWorkspace.cpp and S1BInspectorSectionMigration.cpp author the impact threshold via the command stack. The generated Jolt adapter owns capture and deferred release. VerifyDestructibles.cpp produces and checks the barricade scene. The upstream pin is unchanged.

On DESIGN, Windows x64 Release Studio and Runtime built with exit code zero. The native destructibles proof verified Undo/Redo, save/reopen, the light weight landing without a break, a hit at 11.1708 m/s releasing four pieces, the passage ray clearing and an explicit Lua break. The native sensor, ray and layer regression passed, as did all eight focused CTest entries. The generated Destructibles Playtest opened in standalone DX12. A post-impact frame showed the left barricade standing and the right barricade down in loose pieces.

    cmake --build BUILD/clean-release --config Release --target RenegadeVerifyDestructibles RenegadeVerifyGameplayPhysics RenegadePhysicsLuaTests RenegadeStudio RenegadeRuntime --parallel 4
    ctest --test-dir BUILD/clean-release -C Release -R "^(RenegadePhysicsLuaTests|RenegadeSoftBodyPhysicsTests|RenegadeJP01PhysicsLabSourceContract|RenegadeTerrainPropPhysicsTests|RenegadeTerrainPropCollisionWorldTests|RenegadeS1AInspectorSectionFrameworkTests|RenegadeS1AInspectorSectionSourceContract|StudioUiPolicy)$" --output-on-failure --timeout 45

Run RenegadeVerifyDestructibles.exe from the built Runtime Release directory. An optional .wiscene output argument saves the intact playtest scene. The local generated playtest uses that scene and the project's normal Test Game Runtime path.

The authoring controls still need an interactive Studio review. Remaining destruction work includes the ragdoll-driven gameplay proof, fracture tools, creator-script break event delivery, damage accumulation, debris lifetime and a debris budget. See [Jolt gameplay coverage](docs/JOLT_GAMEPLAY_COVERAGE.md).

---

# Jolt rigid-body simulation collision filters

Implementation commit e9285dc7bd98415fe2bfacb681a2a699b8fbe326 on feature/physics-lab-vehicles. PhysicsGameplayService.h and PhysicsLuaService.cpp expose authored collision layers and bilateral masks. RenegadePhysicsLabWorkspace.cpp adds the layer and eight mask toggles. S1BInspectorSectionMigration.cpp adds the layer and sensor fields. The generated Jolt overlay implements native pairing and live updates in RenegadeBodyCollisionFilter.inc, RenegadeGameplayContactEvents.inc and RenegadePhysicsTempAllocator.cmake. VerifyGameplayPhysics.cpp exercises these paths on DX12. The upstream pin is unchanged.

DESIGN Windows x64 Release Studio and Runtime built successfully. The exact Runtime build opened the FlyingVehicle project in DX12. RenegadeVerifyGameplayPhysics.exe, run from the built Runtime Release directory, passed mask save/reopen, Undo/Redo, preservation through a ghost round-trip, one-sided collision rejection, legacy defaults, live Lua filter changes and wakeup when support was removed. The earlier sensor event and ray cases passed alongside it. All eight focused CTest entries passed.

    cmake --build BUILD/clean-release --config Release --target RenegadePhysicsLuaTests RenegadeVerifyGameplayPhysics RenegadeTerrainPropPhysicsTests RenegadeTerrainPropCollisionWorldTests RenegadeSoftBodyPhysicsTests RenegadeStudio RenegadeRuntime --parallel 4
    ctest --test-dir BUILD/clean-release -C Release -R "^(RenegadePhysicsLuaTests|RenegadeSoftBodyPhysicsTests|RenegadeJP01PhysicsLabSourceContract|RenegadeTerrainPropPhysicsTests|RenegadeTerrainPropCollisionWorldTests)$" --output-on-failure --timeout 45
    cmake --build BUILD/clean-release --config Release --target RenegadeS1AInspectorSectionFrameworkTests --parallel 4
    ctest --test-dir BUILD/clean-release -C Release -R "^(RenegadeS1AInspectorSectionFrameworkTests|RenegadeS1AInspectorSectionSourceContract|StudioUiPolicy)$" --output-on-failure --timeout 45

The new control layout still needs an interactive Studio review. Build and executable checks were local to the feature branch. Specialized body filters remain pending. Continue with native shape sweeps and overlap queries through the same entity-based C++ and Lua adapter. See [Jolt gameplay coverage](docs/JOLT_GAMEPLAY_COVERAGE.md).

---


# Jolt sensors, contact events and filtered body rays

Implementation commit 27b6e884923cadbb79afb959a9f2bde763b99131 on feature/physics-lab-vehicles. The implementation updates the generated Wicked/Jolt wrapper, EngineBridge public service and Lua bindings, the Physics Lab rigid body controls and the native and Lua test paths. The pinned upstream commit is unchanged.

On DESIGN, the Windows x64 Release Studio and Runtime builds linked. The standalone Runtime opened the existing FlyingVehicle project in DX12. The native sensor fixture used a fresh scene and verified Undo/Redo, save/reopen, five native ray filter cases, a live Lua raycast and native contact subscription, and one contact begin and end while a dynamic body fell through the sensor. Invalid Lua ray arguments now return nil and an error string. The Lua, soft-body, Physics Lab source contract and two terrain collision regressions passed.

    cmake --build BUILD/clean-release --config Release --target RenegadePhysicsLuaTests RenegadeVerifyGameplayPhysics RenegadeTerrainPropPhysicsTests RenegadeTerrainPropCollisionWorldTests RenegadeStudio RenegadeRuntime --parallel 4
    ctest --test-dir BUILD/clean-release -C Release -R "^(RenegadePhysicsLuaTests|RenegadeSoftBodyPhysicsTests|RenegadeJP01PhysicsLabSourceContract|RenegadeTerrainPropPhysicsTests|RenegadeTerrainPropCollisionWorldTests)$" --output-on-failure --timeout 45

Run RenegadeVerifyGameplayPhysics.exe from the built Runtime Release working directory for the native proof. The fixture measured a final falling-body Y of -4.412 m after 85 frames.

Remaining work includes a hands-on Sensor checkbox pass in Studio, larger-scene queue performance and true Jolt collision masks for simulation. Scene layer masks in this slice filter body queries. See [Jolt gameplay coverage](docs/JOLT_GAMEPLAY_COVERAGE.md) for scripting and remaining exposure.

---


# Jolt coverage and cloth pinning

Implementation commit f555bbcb2e7c8f6a3ca1a7726ca1ba8bb1a7e300 on feature/physics-lab-vehicles. Changes cover the soft-body bridge, Lua bindings, Physics Lab buttons, C++ and Lua tests and the native cloth proof tool. Read [Jolt gameplay coverage](docs/JOLT_GAMEPLAY_COVERAGE.md) for the remaining exposure by subsystem.

On DESIGN, Windows x64 Release Studio and Runtime linked successfully. These checks passed:

    cmake --build BUILD/clean-release --config Release --target RenegadePhysicsLuaTests RenegadeSoftBodyPhysicsTests RenegadeStudio RenegadeRuntime --parallel 4
    ctest --test-dir BUILD/clean-release -C Release -R "^(RenegadePhysicsLuaTests|RenegadeSoftBodyPhysicsTests|RenegadeJP01PhysicsLabSourceContract)$" --output-on-failure --timeout 45

The native DX12 cloth proof reopened the scene and advanced 90 simulated frames. The pinned sample remained at Y=4.000 m, while the free sample fell from Y=4.000 m to Y=3.538 m.

Remaining coverage includes visual Studio and Test Game cloth checks against colliders and the adapter work listed in the inventory. Start the next backend slice with body sensors, contact events, collision filtering and queries.

---


# Physics Lab and vehicle templates

Local branch feature/physics-lab-vehicles starts at 3e0c69e9efd9fae57522e43027df82ac9854935d. The previous entries below refer to separate work.

The docked Lab, asset Physics section, template launchers and public flight controller are described in [Physics Lab and vehicle templates](docs/PHYSICS_LAB_VEHICLE_TEMPLATES.md). Use the documented verification and remaining coverage for merge review.

---


# PR #197 UMA/wireframe recovery — 17 November 2026

PR #197 remains draft. Original tip 585407a: both baseline jobs passed;
Studio Debug and Release failed. No merge or release occurred.
Correction: affe8d14ecde5e92302658368dc841b45c3f09e8 composes wireframe on the existing UMA stage into
a distinct final renderer. Both shader targets compile exactly one composed
source. A cold/reconfigure regression rejects the original generator.

Actual WARP: same allocator failure reproduced before correction; corrected
Debug 11/11 and Release 11/11 affected checks pass. Source/help/policy 66/66
pass. Normal NVIDIA allocator and native Release Studio viewport/modal
review pass with test-WARP OFF. Test assertions/deadlines are unchanged.
This is scoped recovery validation, not a new full local 322/335 run.

Read docs/UI_PR197_UMA_RECOVERY_20261117.md and its evidence index.
Next: one corrective push to the existing branch, retain draft, and verify
all four hosted checks SUCCESS at the exact corrected PR head. Do not
recommend merge based on mergeable status or historical local results.
No merge or release is authorized. Existing Wicked overlays are preserved.

---

# Combined UI/wireframe ready for PR — 17 November 2026

Application implementation: 7fd95b5b3607974e7275673fa0bcc3e1b1c6af2a.
Configure/diagnostic stamp: c2eccc33fa1c2e13c06bc50b94f16ffa308d12a5.
Base main fceda08 integrated. The implementation commit adds one final
Screen Editor tooltip render pass and three reviewed test-harness corrections
to the configure checkpoint. The documentation/evidence commit changes no
application inputs. All four tested file hashes are recorded. See
docs/UI_COMBINED_PR_VALIDATION_20261117.md and
docs/evidence/ui-pr-validation/README.md for commands hashes and scope.

Earlier complete CI aggregate builds pass in Debug and Release. The final
Screen tooltip change is rebuilt in Studio, along with five interrupted-link
recovery targets. Complete unfiltered CTest is rerun after that final build:
Debug 321/321; Release 334/334.
Studio/Runtime packages and dependency/registry reopen proofs pass for both.
Both Studio startup-smoke runs exit 0; packaged atlas/manifest hashes match
source and all 65 approved SVGs are present. Final source/help/policy pass:
66/66 including packaged help and the 84-case workflow suite.

Preflight fixed reviewed help fingerprints; kept the example-audio notice;
corrected literal-ellipsis inspector title caching and retained detailed
action help during icon assignment. Final Screen Editor toolbar hints render
after ordinary controls and before child pickers. Native before/after and final-candidate
receipts are indexed above. Owner acceptance of UIFinish7 remains scoped.
The original owner application/project and configured Wicked overlays remain
preserved; Wicked pin unchanged.

Local exFAT validation required explicit CMake configuration with auto-
regeneration suppressed and bounded build workers. The task-local canonical
flow adapter retained all tests and package proofs. No hosted workflow change.
The three reviewed test-only corrections preserve all assertions and deadlines;
full details and initial failures are retained in the receipt. No partial or
aborted run is counted as PASS.

Next: verify final documentation-only diff and current main; push this branch
once and open one combined UI/wireframe PR. Confirm all four hosted jobs are
queued for the exact PR tip. No merge or release is authorized. Avoid an extra
documentation push after opening the PR because CI takes over two hours.

---

# Remaining UI audit corrections completed — 16 November 2026

Implementation/evidence commit: d55b61ac28266c11550e338faa72312175da0b72
on fix/ui-cohesion-pass1-20261116. This follows the already-local whole-scene
wireframe commits fada1d6 / 2ddc4ca. All seven U01-U07 findings have implemented
corrections and scoped native receipts. Read
docs/UI_USABILITY_COMPLETION_20261116.md and its evidence index; the historical
re-audit now links to that follow-up and retains its original findings.

Changed StudioModalWindow and StudioNumericControl (new header/source pairs),
LightPickerWindow, ParticlePickerWindow, ParticleTextureBrowser and
DialogueEditorWindow header/source, S1BInspectorSectionMigration,
StudioApplication header/source, RenegadeProjectHub header/source,
RenegadePhysicsLabWorkspace, RenegadeImGuiBridge,
RenegadeScreenEditorWorkspace, RenegadeStudioChrome header/source,
Studio/CMakeLists and ScreenRenderer/src/ScreenRenderer.cpp.
Architecture and the cohesion/icon feature rows were updated.

Key behavior: readable scrollable modal body with pinned footer; one topmost
Escape per physical press; Dialogue dirty guard retained; embedded Screen host
clip for both input and rendering; Physics matching clip and wrapped tabs;
Hub release gestures; primary-first inspectors with fresh selection scroll;
shared numeric rails/bounded help; approved common-action icons and UI font.
Private picker previews render shaded, then restore main scene wireframe mode.
Authored Screen appearance and native document transactions are preserved.

Final native review app remains open on DESIGN:
BUILD/native/Studio/Release/RenegadeStudio-UIFinish7.exe
SHA256 DB13A9DAA42D17206D7ACCEAE826C4B5BCD8A9F8A274EC7432E07182D2447C5F.
It was built before commit and has configure-time Git stamp 2ddc4ca; identify
it by hash and this implementation commit, not its older UI stamp.
Native review used a disposable seven-entity DialogueReview project and
isolated preferences. No owner project, authored dialogue or Screen saved.
Temporary dirty-name guard test was discarded to the saved conversation.

Release build passed (69.30 s); current four Screen suites were rebuilt and
passed separately; final 71 selected CTests passed (11.96 s); diff check passes.
Exact commands, hashes and binary-specific screenshots are in the completion
receipt. Broad resize/Screen/dirty-guard proof uses UIFinish5; final UIFinish7
adds only note spacing and inspector scroll reset, then separately verifies
those changes, Graph/Hub, numeric help, Physics and all four asset handoffs.
Do not attribute all prototype screenshots to the final binary.

No Debug, hosted CI, push, PR, merge, distribution or release-gate closure.
Narrow modal content intentionally scrolls instead of shrinking all text.
OS DPI 125/150%, all Physics pages at 1024, path-traced transitions and
independent exact-commit verification remain outside these receipts.
Existing configured Wicked overlays remain dirty/unstaged and the gitlink
was not changed. No owner work was removed.
Next: owner review of the open candidate; prepare one combined UI/wireframe PR
only when requested, with the required broader build validation before pushing.

---

# SHOW whole-scene wireframe modes — 16 November 2026

Implementation commit: fada1d6353691a14508b9b748ec8335631984cfb,
on fix/ui-cohesion-pass1-20261116. Owner requested whole-scene modes only
and gave positive feedback during native review.

Changed CMakeLists, cmake/WickedWireframeOverlayFix.cmake, StudioApplication
header/source, RenegadeStudioChrome header/source, Phase5Gate9RenderDiagnostics,
and Tests/Phase5Gate9SourceContract. Documentation: WIREFRAME_VIEW_MODE,
FEATURE_MATRIX, ARCHITECTURE, UPSTREAM_SYNC and evidence/wireframe-show.

SHOW exposes Shaded / Wireframe / Shaded + Wireframe beside independent Grid.
One setter shares native state with diagnostics and persists only editor prefs.
Fixes native overlay edges failing the strict depth test using six dedicated
read-only coplanar PSOs in a generated renderer. Original PSOs remain unchanged.
Do not add the renderer to WickedEngine_common: embedded/external targets compile
it individually. Build common and embedded library before relinking Studio.

Exact commands and hashes are in docs/WIREFRAME_VIEW_MODE.md. Release builds pass;
67/67 focused checks pass (2.58 s); diff check passes. Native Wireframe4 checks
cover all three modes, clean scene/history, SHOW indication and fresh-process
restoration. Final Wireframe5 differs only in ASCII active-state markers.
No hosted CI, push, PR, merge, Runtime distribution or Debug proof.

Existing owner Studio windows, project data and dirty configured Wicked overlays
are preserved. No gitlink change. Complex terrain, transparent overlay parity,
MSAA and path-trace transition native checks remain scoped limitations.
Next: owner review of final candidate; broader checks before any requested PR.

---

# UI usability re-audit and modal lifecycle corrections — 16 November 2026

This supersedes the native-coverage limits in the Audio handoff below where
explicit new checks are recorded. Owner subsequently confirmed Audio-to-asset
switching. Native review now also covers model-row, same-asset and Physics
authoring-target selection. No additional feature duplication was proposed.

Local implementation/evidence commits:
- 6a61ab7b97b9438cbcaddb34c2465e33ee05affe: shared suspended-modal composition,
  immediate cleared-selection presentation, one-time resize inspector refresh.
- d04da64e42880521bed707dc0750de757b20b04e: show Dialogue parent before restoring
  conditional child visibility on return from Edit display.
- 57b58f51361535422db3278cb0f8a04b92f365df: audit, coverage, pinned source inventory
  and screenshots. Read docs/UI_USABILITY_REAUDIT_20261116.md and the coverage CSV.

Latest reviewed executable:
BUILD/native/Studio/Release/RenegadeStudio-Icons9.exe
SHA-256 43D1C97E707BEF733EDCED29EB14FC3A1A2EA45D9B4BBB08C1A50C91B5101C01.
Source is pinned to d04da64 for this receipt; the build predates that commit and
its configure-time Git stamp is not the review identity. Modal GPU before/after
proof is explicitly Icons6/Icons8; Icons9 adds the Dialogue return correction.
See docs/evidence/ui-modal-resize and docs/evidence/ui-dialogue-resume.

Release Build target Icons9 completed exit 0 (CL=/MP6):
MSBuild BUILD/native/Studio/RenegadeStudio.vcxproj /t:Build
/p:BuildProjectReferences=false /p:Configuration=Release /p:Platform=x64
/p:TargetName=RenegadeStudio-Icons9 /v:minimal.
ctest --test-dir BUILD/native -C Release
-R 'SourceContract|StudioUiPolicy|RenegadeDialogue|^RenegadeStoryFlowJourneyUiLayoutTests$'
--output-on-failure: 67/67 pass, 5.70 s. Existing C4834 warning remains.
Native Basic/Advanced/Edit display/resize/Back cases pass on exact Icons9.
Native Icons8 Light/Particle/Dialogue resize and close cases are documented.
Docs staged whitespace check passes. These are selected checks, not hosted CI.

All 65 original UI families are inventoried; 30 have scoped fresh native
observations. Remaining asset states and Runtime/DPI scenarios are explicit in
the CSV. Seven open findings: small-window modal footers/readability (P1);
Physics scroll clipping/tabs; Hub press-time commands; inspector ordering;
remaining visual-role adapters; numeric tooltip overflow; stale Window wording.
Do not claim these remaining findings are fixed or every family native-tested.

Review used copied DialogueReview project and isolated preferences. Disposable
numeric/navigation edits were undone. No owner project, authored dialogue,
authored Screen, Runtime gameplay or save schema changed. The Icons9 review app
was left open. No push, PR, hosted CI, merge or release was performed.

Concurrent wireframe/view-mode work appeared after the reviewed build in
CMakeLists.txt, Studio application/chrome/diagnostics files, source contracts,
a new cmake overlay and docs/WIREFRAME_VIEW_MODE.md. Preserved without staging
in these audit commits. That evolving tree is outside this receipt. Wicked
overlays remain dirty and unstaged; no gitlink update was made by this audit.
Do not rebuild mixed concurrent source and reuse the Icons9 test identity.

---

# Audio-to-asset inspector handoff — 16 November 2026

Implementation/evidence commit: 1663001dd76a2f5962d557182dd149f55e262544
on fix/ui-cohesion-pass1-20261116. This supersedes the toolbar-only correction
below for the new report that Audio stays active after an asset click.

Explicit viewport, legacy/modern hierarchy and completed box selection now
release specialist inspector ownership after updating the existing selection.
Sound/legacy particle/navigation entities retain their typed specialist;
ordinary assets immediately use the Scene inspector. Opening global Audio with
an old ordinary selection remains possible. Physics Lab keeps hierarchy targets.

Release build command (CL=/MP6):
MSBuild BUILD/native/Studio/RenegadeStudio.vcxproj /t:Build
/p:BuildProjectReferences=false /p:Configuration=Release /p:Platform=x64
/p:TargetName=RenegadeStudio-Icons6 /v:minimal
Completed with exit 0, including a final incremental confirmation.
BUILD/native/Studio/Release/RenegadeStudio-Icons6.exe SHA-256:
83FC83470A2798ACA3E5398C7A17DE1C4DA4CD477435583C0509C9BA165EF6ED.
Built before commit; its configure-time Git stamp is not a release identity.

ctest --test-dir BUILD/native -C Release
-R 'SourceContract|StudioUiPolicy|RenegadeDialogue|^RenegadeStoryFlowJourneyUiLayoutTests$'
--output-on-failure
All 67 pass in 12.06 s. git diff --check passes.

Native exact-binary screenshots verify Player selection, opening Audio with
retained selection, and clicking Point Light in the viewport immediately showing
LIGHT // Point Light without a panel click. Reopening Audio and expanding a
hierarchy category also preserve mix ownership until an asset is picked.
The review process closed before model-row, same-asset and Physics native checks;
those are not claimed as passes. Exit cause was not established (no matching
Application Error/WER event found). See docs/evidence/ui-icon-integration.
OS DPI 125/150 percent and owner acceptance remain pending.

The existing Icons5 instance was preserved; it is the previous toolbar-only
build and does not contain this fix. The native check used a separate copied
project/preferences in BUILD/ui-audio-selection-review. No owner project edits,
push, PR, hosted CI, merge or release. Dirty Wicked overlays remain unstaged.

---

# Audio/prefab context correction — owner confirmed 16 November 2026

Implementation commit: 239be816e821e7040a59ffb4d3997f927c93ba37.
StudioApplication.cpp now excludes Audio/particle/navigation and Physics from
the selected-object prefab toolbar and its pointer hit targets. The scene editing
guide/footer follows that context; viewport box-selection feedback is preserved.
Audio had reused Scene mode internally, exposing the toolbar for a retained asset
selection. The owner subsequently confirmed the reported issue was fixed.

Release build BUILD/native/Studio/Release/RenegadeStudio-Icons5.exe passed:
ACFD66371311AC6096A27BBB8DBD75AE406840E248A44946BCE4ED6CD5562EF3.
Same MSBuild configuration as below, TargetName=RenegadeStudio-Icons5.
The same 67-test CTest filter passed in 2.75 s. git diff --check passed.
No new screenshot receipt is claimed for this small owner-confirmed correction.
Docs: UI_ICON_INTEGRATION, ARCHITECTURE and FEATURE_MATRIX.

The owner's Icons4 window had unsaved scene edits: do not close or discard it.
Icons5 uses BUILD/ui-audio-context-review and separate preferences. No owner
project or existing window was closed. No push, PR, hosted CI, merge or release.
The existing dirty Wicked overlays remain unstaged. Remaining UI/DPI scope below
is unchanged; owner confirmation applies to this reported context issue.

---

# Approved icon integration and owner corrections — 16 November 2026

Implementation/evidence commit:
5bfe2d4e2ea2f2412d539d91eae8df4d2baac4f0
on fix/ui-cohesion-pass1-20261116, following UI checkpoint 9eb8410.
Source and asset files are unchanged after that commit. This handoff update is
documentation only. No push, PR, hosted CI, merge or release was performed.

Integrated the owner-approved 65-icon SVG catalogue and one padded atlas,
shared square icon rendering, matching-release input, delayed title tooltips,
keyboard focus and compact native controls across Studio and Story Flow.
Changed StudioIcons, Studio chrome/header, Physics chrome, Story Flow chrome/
layout/render path/workspace and return control, Studio asset installation,
four obsolete source-contract expectations, feature matrix and architecture.
Full detail and binary-specific screenshots: docs/UI_ICON_INTEGRATION.md and
docs/evidence/ui-icon-integration/README.md.

Owner review then requested clear space below Fit and reported Audio disappearing
when a viewport menu opens. A shared 192 logical px Journey header now gives Fit
a 9 px gap above the lower row and keeps canvas hit/scissor bounds aligned.
Audio stays rendered during Perspective/Lit/Show menus; popup input ownership
and foreground composition remain. All three dropdown cases and the row gap
were visually verified in the latest native build.

Latest live review executable:
BUILD/native/Studio/Release/RenegadeStudio-Icons4.exe
SHA-256 E6596CA2670F860C0F05B02AB7AA353BA1EF21034E4374D3AE32C7F491A8E2DC.
It uses BUILD/ui-icon-owner-review/DialogueReview.renegade and separate review
preferences. Existing owner-operated Studio windows were preserved. Do not
close them or discard edits. The reviewed project retains seven entities/two
lights. Previous native receipts also cover hold/cancel/release, tooltips,
keyboard Tab/Enter, narrow 1280/1024 layouts and save/reopen. The binary was
built before commit, so its embedded configure-time Git stamp is not the new
implementation hash; do not treat it as a release/package identity receipt.

Configure: cmake -S . -B BUILD/native -DRENEGADE_EMBED_SHADERS=ON.
Build: MSBuild BUILD/native/Studio/RenegadeStudio.vcxproj /t:Build
/p:BuildProjectReferences=false /p:Configuration=Release /p:Platform=x64
/p:TargetName=RenegadeStudio-Icons4 /v:minimal, with CL=/MP6. Exit 0.
Also built RenegadeStoryFlowJourneyUiLayoutTests; its existing tests pass unchanged.
Combined validation:
ctest --test-dir BUILD/native -C Release -R
'SourceContract|StudioUiPolicy|RenegadeDialogue|^RenegadeStoryFlowJourneyUiLayoutTests$'
--output-on-failure
67/67 pass in 9.73 seconds. This is 60 source contracts, UI policy, five dialogue
suites and Journey UI layout. All 65 atlas crops exactly match the approved PNG
artwork. Source and installed atlas hashes match. git diff --check passes.

Remaining: owner acceptance of the latest live UI, real Windows OS DPI checks at
125%/150%, remaining specialist UI audit families, and hosted Debug/Release CI
only when requested. No universal UI/release gate is certified. The six existing
Wicked overlays and gitlink were not staged or modified by icon integration.
Preserve original dialogue and concurrent arms work.

---

# Studio UI cohesion first pass — 16 November 2026

Isolated UI checkout, branch
`fix/ui-cohesion-pass1-20261116`, based on dialogue checkpoint `7d4839a`.
The original dialogue checkout and concurrent arms work were not modified.
No push, PR, merge or release is authorized/performed by this UI pass.

The owner requested fixing the audited UI before toolbar icon integration.
Changes: shared opaque charcoal/orange action faces and release-only callbacks;
one twelve-row Add menu; world overlays before GUI; final foreground popups and
loading cover; Physics Lab marker suppression; flow-owned Pickup/metadata row;
reserved Audio heading; bounded heading/Character text; scrollbar-safe content
width across Scene/Player, Environment, Terrain/River, Material, Render and
Waterfall. The protected importer keeps its accepted native control types.

Native owner review found an inspector occlusion regression caused by promoting
the whole chrome for popup input. This is corrected by exclusive popup updates
without chrome promotion. The owner also exposed separate Environment/Render
sizing paths; all discovered inspector width paths now use the shared policy.

Release Studio and five dialogue regression targets built successfully. Six
CTest suites pass (UI policy plus dialogue core/assets/save policy/display/
scripting). All 60 source contracts pass after preserving importer types and
updating obsolete menu-count/typography expectations. Existing JP01 Physics Lab,
Marker Icons, S1B and protected importer source contracts pass. Pure policy tests
also pass with g++ C++17; width tests cover 280/360/480 px and tiny bounds.

Latest native binary SHA-256:
`9E7E371B96B0EDBF769535D210DB09F2E9E86780E7862499197072A0F1F221AD`.
It is staged as canonical `BUILD/native/Studio/Release/RenegadeStudio.exe`.
The owner closed the preceding session and authorized reopening. Final review
uses `BUILD/ui-correction-review/DialogueReview.renegade` and isolated local
preferences. Do not close an owner-operated Studio session or discard edits.
Native review confirms complete numeric fields in Environment/Render/Player,
immediate newly placed light inspection, and single-click light selection from
both Environment and Render. The saved review scene reopened with seven entities
and two lights. Narrow layer bits wrap into four columns. Evidence records each
binary separately; earlier menu/Physics screenshots are scoped to their capture.
See `docs/UI_COHESION_PASS1.md` for current scope and acceptance targets.

The six pre-existing Wicked working-tree overlays were copied unchanged into
this independent dependency worktree. Do not stage the Wicked gitlink or its
dirty overlay, change pins, or include unrelated arms work. The old dialogue PR
instructions below remain historical and belong to its separate checkout.

---

# PR #196 CI failure correction - 16 November 2026

Failed head: `7d4839ab1b048a8c423dbed76654b3d86d0eeac5`; Studio run
`43906758099` failed the same four tests in Debug and Release. Both Windows
baseline jobs in run `43906757994` passed. Compilation succeeded; the failure
was in the full CTest run, which the earlier 16-suite selection did not cover.

Corrections:
- DialogueDisplayService.cpp now uses filesystem-relative resolution for both
  path spellings instead of comparing a canonical file with a lexical root.
  The added noncanonical-root regression failed on the old implementation
  (exit 1, invalid Content-relative hint) and passes with the correction.
  This covers equivalent root spellings, including Windows short-path aliases.
  Existing project identity/containment/resource validation remains in place.
- Phase5Gate3SourceContract.cmake now checks the ten-item Add menu, including
  the appended Dialogue action, while retaining Decal/Probe/Projectile checks.
- workflows.json refreshes 17 affected UTF-8/LF whole-file fingerprints after
  reviewing dialogue-only changes in StudioApplication, StudioChrome and the
  architecture notes. All existing evidence needles remain present. Recipe
  steps, acceptance limits and fail-closed hash checks are unchanged.

Validation before updating the PR:
`cmake --build BUILD/native --config Release --parallel 4 --target RenegadeDialogueDisplayTests`
succeeded. `ctest --test-dir BUILD/native -C Release -R 'SourceContract|Assistant|RenegadeDialogue' --output-on-failure`
passed all 70 selected tests in 41.80 seconds (exit 0), including all four CI
failures, the complete offline assistant suite, Git-free packaged knowledge,
and the display-root regression. The local Git safety override was supplied
per process for this ownership-less drive; no production/test checks were
disabled. Python caches generated by these checks are removed before staging.

Changed files: the three code/test files above, the assistant workflow registry,
this handoff and the feature-matrix validation field. Existing native UI acceptance
is unchanged. Hosted Debug/Release reruns are still required at the corrective
head; local passes do not establish four green hosted checks. Keep PR #196
unmerged. Preserve the unrelated UI-audit/arms work and existing Wicked overlay.

---

# Dialogue PR preparation - 16 November 2026

The owner authorized a separate dialogue PR and its four hosted CI checks.
Merge is not authorized. Latest fetched main is `7093cf4da660cebd7c686aac1b44d6e00181f556`
and is already an ancestor of the dialogue branch (0 commits behind). The
previous validated checkpoint is `db14f27`; runtime/editor behaviour is unchanged.

Pre-publication inspection found two registered suites absent from the explicit
CI aggregate. CMakeLists.txt now includes RenegadeDialogueScriptingTests and
RenegadeDialogueDisplayTests in both RenegadeStudioCI and RenegadeDialogueReview.
Configure succeeded; the generated Studio CI project explicitly references all
five dialogue executables. Both newer targets rebuilt successfully and the
same 16-suite regression set passed again in 9.95 seconds (exit 0). Commands:
`cmake -S . -B BUILD/native -DRENEGADE_EMBED_SHADERS=ON`, then
`cmake --build BUILD/native --config Release --parallel 4 --target RenegadeDialogueScriptingTests RenegadeDialogueDisplayTests`,
then the same 16-suite CTest filter recorded below. The feature matrix now
records owner acceptance and pending hosted checks.

Publish the existing feature/dialogue-editor-20261115 branch with a normal
fast-forward push, open a focused PR against main, and verify both Debug/Release
jobs in each automatic Studio and Windows baseline workflow. No workflow edits,
Wicked gitlink changes, unrelated arms work or UI-audit changes are included.
The pre-existing Wicked working-tree overlay/copied baseline remains untouched.
Do not merge or claim hosted CI success until the checks complete.

---

# Basic dialogue beta validation - 16 November 2026

Validated checkout: `00c2146de0668f9a5d3b65cd246535760352df70`.
Functional Studio source: `d38d23eafc7e38c8a86b3ee7fec59d2ff98b2e58`;
subsequent commits contain documentation only. The owner confirmed "works
perfectly" after the Advanced correction, closing its pending owner review.
This supersedes the older live-session/unsaved-state notes below: Studio was
closed at the start of final validation. A fresh isolated review was launched;
input stopped when another window had foreground focus. No owner project or
unsaved conversation was discarded, and no new automated visual proof is claimed.

Release Studio, Runtime and all selected test targets built successfully.
All 16 related CTest suites PASS in 10.07 seconds: dialogue core/assets/save
policy/scripting/display, governed Lua and its source contract, Screen runtime/
renderer/authoring/history/completion, Test Level snapshots and native reload,
game save slots, scene transitions and native window startup. Build/check
process exited 0. No implementation changes were needed during this pass.

Commands (repository root, existing embedded-shader native configuration):
```powershell
cmake --build BUILD/native --config Release --parallel 4 --target RenegadeStudio RenegadeRuntime RenegadeDialogueTests RenegadeDialogueAssetTests RenegadeDialogueSavePolicyTests RenegadeDialogueDisplayTests RenegadeDialogueScriptingTests RenegadeS3GovernedLuaRuntimeTests RenegadeScreenAuthoringSessionTests RenegadeScreenAuthoringCompletionTests RenegadeScreenRendererTests RenegadeRuntimeScreenTests RenegadeTestLevelSnapshotTests RenegadeTestLevelSnapshotRuntimeTests RenegadeRuntimeGameSaveSlotTests RenegadeRuntimeSceneTransitionTests RenegadeNativeWindowSurfaceTests
ctest --test-dir BUILD/native -C Release -R 'RenegadeDialogue|RenegadeS3GovernedLuaRuntime|RenegadeScreenAuthoringSessionTests|RenegadeScreenAuthoringCompletionTests|RenegadeTestLevelSnapshot|RenegadeRuntimeGameSaveSlotTests|RenegadeScreenRendererTests|RenegadeRuntimeScreenTests|RenegadeRuntimeSceneTransitionTests|RenegadeNativeWindowSurfaceTests' --output-on-failure
```

SHA-256 of the validated binaries:
- Studio: `9CBA89A276EC9B1F3FBEAECE9E0380A5D3FEC73C05B57B2A820716706A7663B3`
- Runtime: `AE904D6B689FB31889C5BF6A1F2DBE863B83420BC93EE8AA2B2F955F5B9EB95F`

Basic dialogue implementation and local validation are complete. Existing native
game/display and scripting receipts remain below. This is not all-engine Debug/
Release CI, packaged distribution, or an independent release-checklist pass.
Next integration step is a separately authorized PR/CI and exact-commit review.
No push, PR, CI, merge or release was performed. Preserve the existing Wicked
overlay/copied baseline and unrelated arms/Blender work.

---

# Advanced controls visibility correction - 16 November 2026

Latest LOCAL Studio source: d38d23eafc7e38c8a86b3ee7fec59d2ff98b2e58.
The owner reported that Advanced appeared to expose no controls while an NPC
line was selected. Conditions/outcomes belong to player replies and actions to
outcomes; the existing tree retained Basic mode's collapsed reply state.
DialogueEditorWindow.cpp now expands rule branches when Advanced is enabled,
selects the first player reply when invoked from an NPC with replies, and shows
selection-specific instructions. Show in Advanced also forces rule expansion.
This changes authoring selection/visibility only, not conversation assets/runtime.

Release Studio built successfully. The five focused dialogue CTest suites PASS
in 0.75 seconds. The earlier shared display build has 14 passing suites and native
UI/game evidence below. The new automatic selection needs final native owner
review; the currently open review window still holds the previous UI and the
owner's UNSAVED conversation edits. Preserve it. The updated normal Studio
executable and on-disk review executable are ready for the next launch. The old
loaded review binary was renamed to a timestamped before-advanced backup, leaving
the live process and its unsaved edits intact. Do not restart that active review
session without preserving its edits. In that current session, Advanced then selecting a player
reply exposes the existing tools; selecting an outcome exposes actions.

An annotated guide was generated from the exact selected screenshot. It is a
labelled instructional image, not native software verification. No generated
image is used as implementation evidence. No push, PR, CI, merge or release.

---

# Editable dialogue displays - 16 November 2026

Latest LOCAL engine source: 4e8f1fa44c565411d00ccc2f494ceb6cb9de7927.
Branch feature/dialogue-editor-20261115, main baseline 7093cf4.
Owner required creator-editable dialogue boxes before signoff and approved the
shown direction with "this looks good". This is visual approval, not permission
for a PR, CI, merge or distribution. All work remains local; PR #195 stays closed.

The Dialogue Editor now offers a project display selector and Customise/Edit
Display. New creates a normal project-owned Screen template and opens the existing
native UI editor through its standalone handoff. Speaker/text/reply/navigation
roles are separate from names, content, styles and geometry. The normal Style /
Font, graphic browser, layout, Undo/Redo and save workflow applies. The project
selection and typed packaging roots persist; changing display removes the old
selection's explicit root. The shared panel adapts the template for preview,
Interact and scripted conversations, preserving game logic, audio and memory.

Changed code: DialogueDisplayService.h/.cpp; ScreenService.h/.cpp;
ScreenAuthoringSession.h; TestLevelSnapshotService.cpp; DialoguePanel.h/.cpp;
RuntimeDialogue.cpp; DialogueEditorWindow.h/.cpp; StudioApplication.h/.cpp;
RenegadeScreenEditorRenderPath.h; RenegadeScreenEditorWorkspace.cpp; bridge/test
CMake lists; DialogueDisplayTests.cpp. See docs/DIALOGUE_DISPLAYS.md.

Validation:
- Release Runtime and native Studio builds succeed; the latest delivery targets
  the normal RenegadeStudio.exe without closing the owner's review window.
- 14 focused CTest suites PASS in 7.61 seconds at the final source. Receipt:
  docs/evidence/dialogue-display/README.md.
- Integration tests cover roles, renamed elements, custom styles/geometry, reply
  pagination, Continue/Close, native Screen history/Undo, save/reload, assignment,
  foreign project rejection, missing roles/hidden parents and staged fonts/artwork.
- Native Runtime at 68c7dcc showed a deliberately different project layout using
  Georgia, gold text, a brown opaque background, changed size and screen position.
  Enter selected work -> captain with matching Sael audio and a visible Continue.
- Native Studio at 2de8dff opened the assigned display through Edit display in the
  actual UI editor. Full artboard, Layers, role-bearing text and font controls
  were inspected. The owner's input moved to another window during the final
  Apply/save/return interaction, so automation stopped. This last mouse-driven
  round trip remains for owner review; automated persistence checks passed. The
  owner subsequently returned to the conversation workspace and reported its
  header overlap, confirming the native return handoff was reachable.
- Initial native review exposed the audio-example note overlapping the new
  selector/button, then the selection heading. Final placement is in the preview
  caption area. The editor's return label is now generic Back for either origin.
- Missing resources or renderer rejection keep runtime dialogue playable via the
  built-in display; staging rejects incomplete assets so creators can repair them.

Reproduce:
cmake -S . -B BUILD/native -DRENEGADE_EMBED_SHADERS=ON
cmake --build BUILD/native --config Release --parallel 4 --target RenegadeRuntime RenegadeDialogueDisplayTests RenegadeDialogueScriptingTests RenegadeS3GovernedLuaRuntimeTests RenegadeScreenAuthoringSessionTests RenegadeScreenAuthoringCompletionTests RenegadeScreenRendererTests RenegadeRuntimeScreenTests RenegadeTestLevelSnapshotRuntimeTests RenegadeRuntimeGameSaveSlotTests
cmake --build BUILD/native --config Release --target RenegadeStudio -- /p:BuildProjectReferences=false
ctest --test-dir BUILD/native -C Release -R 'RenegadeDialogue|RenegadeS3GovernedLuaRuntime|RenegadeScreenAuthoringSessionTests|RenegadeScreenAuthoringCompletionTests|RenegadeTestLevelSnapshot|RenegadeRuntimeGameSaveSlotTests|RenegadeScreenRendererTests|RenegadeRuntimeScreenTests' --output-on-failure

No current package-distribution or independent release acceptance is claimed.
The owner then reported the NPC LINE overlap from the earlier review candidate.
The review executable was refreshed from the final normal Studio binary; both
now have identical contents. The example-audio note lives in the preview caption
area, clear of NPC LINE and the display selector. This was visually confirmed
in the refreshed live review at the owner's resolution; see header-fixed.png in
the receipt. The owner currently has unsaved conversation edits open in the
review window: preserve them and do not restart it again. Preserve the
owner's original projects, unrelated arms work and the existing Wicked overlay
state. The separate review project is disposable and contains copied example
recordings; none were generated for this task.

---

# Dialogue scripting beta integration - 16 November 2026

Tested LOCAL engine source: bedcf30f744f2b4ce9c3432b96084573879f6199.
Branch feature/dialogue-editor-20261115; main baseline 7093cf4.
Owner chose to retain the current simple editor for beta and authorized scripting
support. No node redesign, generated voices, push, PR, CI, merge or release was
performed. PR #195 remains closed and unmerged. Original owner project and the
separate arms/weapon work were preserved; Wicked pin/overlays were not changed.

Added renegade.dialogue contract 1 to the existing governed creator Lua runtime:
- Start a saved conversation by NPC EntityRef, exact name or asset ID, optionally
  at an authored line ID. An active conversation cannot be silently replaced.
- Stop/query active state and get/set shared flags and signed integer values.
- Lifecycle events dialogue.started / dialogue.finished / dialogue.closed through
  the existing FIFO, plus the existing authored dialogue.action event.
- State, Screen and voice use the same RuntimeDialogue session as NPC Interact.
  Scene preparation precedes Lua on_start. Setters refresh reply availability
  without replaying the spoken line. Saved-state codec/schema remains unchanged.
- Reject stale/deleted NPC references, ambiguous names, bad line IDs, embedded NUL,
  invalid keys/types, integer overflow and memory growth beyond saved capacity.
- Fix native Runtime startup requested as minimised: wait for a drawable window
  before Run can start engine initialisers with a null graphics device.

Validation at the tested source:
- Release Runtime and focused targets built successfully.
- 13 focused CTest suites PASS in 5.28 seconds; GNU C++17 core 50 checks PASS.
- New integration test executes governed Lua against real saved dialogue assets
  and RuntimeDialogue, not a callback-only mock: conditional permit/refusal,
  authored action event, shared memory/codec, lifecycle counts, deleted NPC,
  duplicate names, invalid inputs and capacity limit.
- Separate native Runtime: minimised launch responsive, restore, on_start starts
  captain by name, stop, start permit by asset/line ID, two start/two close events,
  retained flag/value, matching captain/permit voice clips, graceful exit 0.
  QA uses a deliberate terminal Lua error marker only after all assertions pass;
  this disables the disposable proof script, not a production error acceptance.
- Native receipt: docs/evidence/dialogue-scripting/README.md.
- Initial aggregate could not replace the owner's open Studio executable
  (LNK1104). A separate Studio candidate is built alongside it; the open editor
  is not terminated. RenegadeStudio-DialogueScripting.exe linked successfully
  alongside it. Runtime changes are available to its next Test Game launch.

Reproduce from repo root with the normal embedded-shader configuration:
cmake -S . -B BUILD/native -DRENEGADE_EMBED_SHADERS=ON
cmake --build BUILD/native --config Release --parallel 4 --target RenegadeRuntime RenegadeDialogueScriptingTests RenegadeS3GovernedLuaRuntimeTests RenegadeRuntimeSceneTransitionTests RenegadeNativeWindowSurfaceTests
ctest --test-dir BUILD/native -C Release -R 'RenegadeDialogue|RenegadeS3GovernedLuaRuntime|RenegadeRuntimeSceneTransitionTests|RenegadeTestLevelSnapshot|RenegadeRuntimeGameSaveSlotTests|RenegadeScreenRendererTests|RenegadeRuntimeScreenTests|RenegadeNativeWindowSurfaceTests' --output-on-failure

Creator API and copyable NPC example: docs/DIALOGUE_SCRIPTING.md.
The editor remains the same simple/Advanced authoring UI accepted at ed2e69a.
New package distribution, new owner visual acceptance and independent release
acceptance are not claimed. Automatic conditional NPC continuation, quest-specific
services and graph editing remain outside this beta integration.

---

# Dialogue editor usability review - 16 November 2026

Tested LOCAL engine commit: ed2e69ab410045bbad3b6db384fe80b6bc008c17.
Branch feature/dialogue-editor-20261115; base main 7093cf4.
The owner tested this native build and reported: "that all seems to work now".
PR #195 remains CLOSED unmerged. No commits were pushed and no CI/merge/release
was started. Owner review of this UI does not authorize those further actions.
Other arms/weapon work and the pinned Wicked revision are unchanged.

Current behaviour:
- Studio dark/orange styling and native fonts; distinct hover/pressed/disabled
  feedback, one mouse-release action, tooltips and three numbered workflow steps.
- Valid edits refresh preview without Save. Progress/private memory/voice visits
  remain stable; unfinished fields retain the last valid preview with a hint.
- Preview end/stop keeps a visible terminal card with Restart. A replacement
  button cannot consume the click that ended the previous line.
- Reply rows show their next response or ending. Unused NPC lines explain that
  a reply's Continue to connects them; Add response creates a linked line.
- Local WAV/OGG import adopts a validated copy without overwriting clips. Open
  voice folder and Refresh clips expose local recordings. New lines can stay
  text-only; a visible note identifies example clips and invites creator audio.
  Existing Sael take-2 recordings remain example assets; no new voices generated.
- NPC chooser, attachment status, Save & attach and Test in game provide the
  complete authoring path. Player Start markers are excluded as dialogue targets.
- The Runtime prompt names the project's actual Interact binding (E by default).
- The basic guard example uses literal fixed responses. Its old hidden permit
  flag/actions caused a later Let me through choice to approve entry. Conditional
  behaviour remains tested separately and available through Advanced. The review
  asset was updated without changing the owner's new captain connection.

Changed code in the latest revision: DialogueService.h/.cpp, DialogueEditorWindow.cpp,
RuntimeDialogue.h/.cpp, RuntimeApplication.cpp, DialogueTests.cpp. Earlier local
usability commits added voice adoption, asset tests, the dialog launch integration,
button/tooltip feedback and shared preview input isolation.

Validation:
- GNU C++17 core suite: 50 checks PASS.
- Release Studio/Runtime and aggregate/focused targets built successfully.
- Nine focused CTest suites PASS in 4.94 seconds at the tested engine source.
- Tests cover live unsaved text/connection changes, voice-visit and memory
  preservation, incomplete edits, deleting the displayed line, basic refusal even
  with an old permit flag, Advanced conditions/actions/save memory, clip adoption,
  serialization/snapshot/binding Undo/Redo and native scene reopen.
- Native 7753f94 restored all controls with no outer scrolling. Its pressed Test
  in game button showed depressed fill and the normal launch path saved the
  current conversation/scene and launched the separate Runtime.
- Native ed2e69a owner Test Game log recorded work -> captain and matching audio,
  then gate -> refused and sael-refused.wav, continuation -> opening, and bye ->
  end. Owner reported that the changes work after reviewing the new build.

Commands:
cmake -S . -B BUILD/native -DRENEGADE_EMBED_SHADERS=ON
cmake --build BUILD/native --config Release --parallel 4 --target RenegadeDialogueReview RenegadeRuntimeSceneTransitionTests RenegadeScreenRendererTests RenegadeRuntimeScreenTests
ctest --test-dir BUILD/native -C Release -R 'RenegadeDialogue|RenegadeRuntimeSceneTransitionTests|RenegadeTestLevelSnapshot|RenegadeRuntimeGameSaveSlotTests|RenegadeScreenRendererTests|RenegadeRuntimeScreenTests' --output-on-failure
g++ -std=c++17 -I EngineBridge/include EngineBridge/src/DialogueService.cpp Tests/DialogueTests.cpp -o dialogue-tests
./dialogue-tests

Preserved failures: 4a39dc3 could click through to the replacement preview button
and allowed outer scrolling. 29dcf54 fixed the preview gesture but directly
parenting controls to the scaled window hid them off-screen (owner screenshots).
7753f94 restored the native unit-scale content parent and removed only the outer
scrollbars. The opaque 2D composition resize fix remains; no unwritten suspended
3D targets are sampled. Earlier responsive exits had code 0, not an application
fault receipt. No blanket crash/performance pass is claimed.

Limits: Debug/CI have not run for these local revisions. A new packaged build
has not been verified at ed2e69a; older package receipts retain their original
scope. Runtime was exercised as the actual separate Test Game player, not an
embedded simulation. Controller hardware, live pagination, explicit manual
file-picker save/reopen coverage and populated-inventory native save remain
separate checks. Do not promote these into an independent release-gate PASS.

---

# Renegade engineering handoff

**Current reconciled baseline:** main `618b2310ec7d6a7889d490e93304e39043f7e5ff`, 10 November 2026.
**Wicked pin:** `82557d439a41d104f559b0d3134d160b7bf6154e`.
The prefab/selection branch has been merged as PR #193. Lighting/particles were
already merged as PR #192. There is no unresolved integration conflict between
those streams. Do not restart prefab implementation from an old local checkpoint.

## Completed latest work

PR #193 feature head `cb590f06267e3407633d4e8f68bbde9265dd6ec1` includes ordered
multi-selection, Shift-toggle in hierarchy/viewport, directional box selection,
shared-pivot transforms, batch duplicate/delete, Group/Edit Pieces/Unpack and
native reusable scene assemblies. Scripted prefabs retain typed properties,
resources and modules; placement assigns fresh instance identities and remaps
internal references. Delete/duplicate/Unpack use existing history/companions.
Packaged load preserves authored instance edits rather than replacing them.

Latest validation: Release Studio/Runtime/proof builds; 11 focused checks;
60 source contracts; all six earlier failures rechecked; original geometry proof;
complete functional brazier proof; actual Studio save/card drag/Unpack/Save/Reopen;
package-relative standalone render with four governed scripts started. Evidence:
[functional prefab receipt](docs/evidence/scene-prefab-functional/README.md).

Four pre-merge checks succeeded on `cb590f0`:
[Studio Debug/Release](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/42857306536) and
[Windows baseline Debug/Release](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/42857306652).
Owner approved merging and merged #193 on 10 November. This records source
integration and scoped acceptance, not every independent release gate.

## Current documentation task

Branch: `docs/current-main-reconciliation-20261110`, based on the main SHA above.
Scope: current summaries, roadmap, architecture orientation, feature ledger,
build/upstream guidance, module READMEs, active creator/assistant guides,
documentation index and stale status/encoding reconciliation. No engine code,
workflow behaviour, assets, executable outputs or submodule pointer changes.

The documentation commit uses `[skip ci]` at the owner's explicit request.
Run local documentation, source-contract and assistant trust checks; do not
dispatch CI. Skipped required checks can remain pending. Do not change branch
protection or manufacture a green result to merge documentation.

## Boundaries and known follow-ups

- Placed prefab copies are independent; linked updates/override management are
  not implemented. External entity targets must be included or cleared.
- No explicit prefab entity-count cap; large castle assemblies are untested.
- Plugin/marketplace/third-party shader-editor discussions are proposed work,
  not implemented features or an instruction to begin them.
- Preserve existing castle assets, owner worktrees, private projects and backups.
  The separate prefab and picker test projects are not distributable releases.
- Preserve the pinned Wicked gitlink. Main's CMake-generated sky/weather,
  allocator and particle overlays are documented core-patch decisions, not a
  reason to commit a dirty submodule or reset another checkout.
- Main/alpha distribution and future merges require the owner's approval.
  No fresh alpha distribution is authorized here.

## Working context and history

Read [CURRENT_STATUS](docs/CURRENT_STATUS.md), [documentation index](docs/README.md),
[ROADMAP](docs/ROADMAP.md), [ARCHITECTURE](docs/ARCHITECTURE.md) and the relevant
[FEATURE_MATRIX](docs/FEATURE_MATRIX.csv) rows before acting.
The [complete pre-reconciliation handoff](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/blob/618b2310ec7d6a7889d490e93304e39043f7e5ff/HANDOFF.md) preserves
the earlier commands, failures, snapshots and checkpoints. Dated evidence remains
in the repository. Historical “no merge”, “CI pending” and machine observations
apply to their original checkpoint, not automatically to today's main.

Final follow-up: Wireframe5 fresh-process mode restoration, readable ASCII
markers and corrected shaded overlay were visually verified. Its review window
is left open. Source implementation remains fada1d6353691a14508b9b748ec8335631984cfb.

## Terrain props P0/P1 handover, 17 November 2026

**Branch** `feature/terrain-props-p0-p1`. Implementation head `e4cec253360b94edf3a0bce6c3931e1c2cf841b6`, based on `db8d620f75a5219ae5a3906e2837ed2d4165c741`. The handover commit adds documentation and the verified player capture. Run `git log -1 --format=%H -- HANDOFF.md` on this branch for its separate commit. No push, PR or merge was made.

The isolated checkout retains configured Wicked edits to the two particle shader files, particle shader interop header, emitted-particle source/header, allocator header and four archive-overlay files (`wiECS.h`, `wiScene.cpp`, `wiScene_Components.h`, `wiScene_Serializers.cpp`). They are recreated by configuration. There is no uncommitted parent-repository implementation and neither submodule pointer changed. Other working trees were not reset or reused. Build products and raw logs live under ignored `BUILD/`.

[Data formats, archive decision and service contracts](docs/TERRAIN_PROP_P1.md) · [Actual standalone player capture](docs/evidence/terrain-props/runtime-slice.png). The complete source, test, fixture and contract file list is available with `git diff --name-status db8d620 e4cec25`.

**Local evidence.** Windows x64, Intel i9-13900K, 128 GB installed RAM, RTX 4070 Ti, DX12, MSVC 19.51, CMake 4.4.4. A deterministic technical scene imported the original two-part tree, bush and boulder through the normal static-model committer. The 96 placements produced 128 native objects referencing four distinct mesh IDs and four distinct material IDs. Each mesh was reused 32 times. The four native mesh buffers totalled 3,392 bytes for these deliberately small fixtures. The unmodified scene had one LOD per mesh and no impostor atlas. Adding a tiny second subset to a mesh in the disposable benchmark kept its aligned geometry buffer at 608 bytes. Enabling one native impostor allocated an 864-slice atlas with 14,155,776 bytes of texel payload.

The scene had 64 static bodies. Jolt rays hit trunk and boulder, missed crown and bush, and missed a removed trunk while still hitting the boulder. Regular selection, hierarchy queries and duplicate refused a live projected object. Saving while the projections were live produced a raw WISCENE with both authored witnesses and zero projected objects, meshes, materials or rigid bodies. Studio's document service and the Runtime scene loader rebuilt the same population. Save As retained distinct old and new revisions and preserved placement ID 1 through a scale change. The reopened allocation counter returned 97. A withheld page made both save and open fail with the live scene intact. An injected replacement failure retained the previous revision. A separately interrupted replacement recovered both old files from its journal, then retry committed the new page.

The DDS probe observed a 5,592,552-byte backed texture through native resource state. The shared material test reads the current base-colour slot rather than inferring resolution from the request. An exclusive handle held the texture at its cold size, and unlocking the file allowed promotion.

| Native measurement | Cold | Requested 1024 | Released request |
| --- | ---: | ---: | ---: |
| Resident width | 64 | 1,024 | 64 |
| Actual resident / full mips | 7 / 11 | 11 / 11 | 7 / 11 |
| Resident mip payload, bytes | 21,844 | 5,592,404 | 21,844 |
| DXGI device usage in the shared-material run, bytes | 276,824,064 | 293,601,280 | 293,601,280 |
| Process peak working set in that run, bytes | 565,514,240 | 582,778,880 | 582,778,880 |

The DXGI device-usage counter includes pooled allocations and can stay at its high value after the native mip chain shrinks. These are per-texture mip payloads. Physical memory savings, transient peaks attributable solely to streaming, per-frame disk bandwidth and a representative woodland frame-time budget remain unestablished. The original Release probe captured the three states then faulted during process teardown. The final harness drains renderer pipeline work, joins the jobs and waits for the GPU while the device still exists. The accepted test requires normal process exit as well as the descriptor transitions.

**Remaining limits.** Only the project workflow and minimal resident projection have been exercised. A moved or exported standalone package without its project asset registry, a detached Test Game snapshot outside the project, cross-process edits to the same layer, asset reimport during an active projection, older backup retention under garbage collection and large forest performance need separate proofs. The slice's colours and deliberately low-poly meshes are test content. No streaming budget, impostor content bake, automatic texture cooker, collision-interest radius or creator UI ships in this change. The 60 FPS / 16.67 ms target remains a proposed performance target. Independent review of the exact commit is outstanding.

**Sol's next change is P2 only.**

1. Add layer recipe and mix persistence, seed, stable candidate keys, clearing/tombstone and override records with v1 read compatibility. Add delta-based bridge editing against the current revision. Preserve the placement ID high-water mark across undo, redo and save.
2. Implement a scene-safe live projection update for changed pages. Commit authoring changes as one logical operation, keep the old display on failure and discard cancelled staging. Verify changed bounds and IDs across adjacent cells and edits at terrain edges.
3. Add the Terrain Props palette using the existing asset picker, linked density slider/numeric input per entry, saved mixes, Generate, Paint, Erase, progress and cancellation. Sample by distance and keep one Undo entry per stroke or Generate Apply.
4. Test deterministic output, cross-cell spacing, manual clearings, retained IDs, terrain expansion/reprojection, narrow-panel layout, Save As and reopen. Leave the proposed collision, asset and texture streaming services at the interface stage.

**Before Sol pushes** refresh `origin/main`, check the combined diff and configuration against the new accepted baseline, verify both gitlinks, and review any local dirt in the correct worktree. From a fresh isolated checkout run:

```powershell
git submodule update --init --recursive
git diff --check origin/main...HEAD
git ls-tree HEAD WickedEngine ThirdParty/imnodes
cmake -S . -B BUILD/native -G "Visual Studio 18 2026" -A x64
$targets = @('RenegadeStudio','RenegadeRuntime','RenegadeTerrainPropPageTests','RenegadeTerrainPropResidencyProbe','RenegadeEditorPointLookupFocusedTests','RenegadeProjectDocumentTransactionTests')
foreach ($cfg in 'Debug','Release') {
    cmake --build BUILD/native --config $cfg --target $targets --parallel 3
    if ($LASTEXITCODE) { throw "Build failed in $cfg" }
    ctest --test-dir BUILD/native -C $cfg -R 'Renegade(TerrainProp|EditorPointLookup|ProjectDocumentTransaction)' --output-on-failure
    if ($LASTEXITCODE) { throw "Focused checks failed in $cfg" }
}
cmake -S . -B BUILD/warp -G "Visual Studio 18 2026" -A x64 -DRENEGADE_TEST_FORCE_WARP=ON
cmake --build BUILD/warp --config Release --target RenegadeTerrainPropResidencyProbe --parallel 2
ctest --test-dir BUILD/warp -C Release -R 'RenegadeTerrainProp(ResidencyProbe|SliceProof)' --output-on-failure
```

Run Sol's new P2 behaviour tests, the repository Studio/Runtime Debug and Release preflight, and the UMA/wireframe overlay regression checks on that combined tree. Run the normal editor workflow and the separate player with a populated project, inspect the captures, and record frame-time evidence at matched settings. A green local build alone does not demonstrate the authoring workflow. After pushing, verify all four hosted checks belong to the pushed head. Merge requires the owner's approval.

**Build record.** Final NVIDIA Release focused checks passed 6/6. The standalone Runtime captured frame 60 from the project and exited with code 0. Studio and Runtime built in Debug and Release. Debug focused CTest passed 4/4 and the separate Debug native slice exited 0. The fresh WARP Release build passed both native tests (2/2, Microsoft Basic Render Driver). Local logs under BUILD include final-Release-test.log, validated-Debug-test.log, debug-confirm.log, debug-confirm-exit.log, warp-test.log and validated-capture.log. The additional baseline renderer-overlay CTest exceeded its 60-second timeout twice on E:. The same cold/reconfigure fixture completed successfully on the system temporary drive, exit 0 (overlay-confirm.log). Repeat the registered overlay check before pushing. All feature and handover edits are committed on this branch. The configured WickedEngine worktree shows ten generated overlay edits; its pinned gitlink and the imnodes gitlink are unchanged.

## Terrain props P2 authoring handover, 17 November 2026

Implementation head: `01d7c012b8b4ebe6caacac74eaf80d5eaa91b0cb` on `feature/terrain-props-p0-p1`, following P0/P1 handover `2853b55c55f7d7aeee8aaa8e9f69d2bba02ca2b8`. The final documentation/evidence commit is separate; obtain it with `git log -1 --format=%H -- HANDOFF.md`. Owner acceptance and merge approval remain pending.

**Scope delivered.** P2 authoring only: v2 recipes, per-model density, saved layer mixes, stable candidate keys/bindings, clearings/tombstones and overrides; v1 reads; distance-sampled Generate/Paint/Erase and deliberate terrain reprojection/expansion; finite native terrain snapshots; changed-page live projection with rollback; one history command per Apply/stroke; allocation high-water preserved through Undo/Redo/save. The Terrain Props palette uses governed model selection and linked numeric/slider controls. Failure/cancellation retain the old display. Layer-owner deletion/recovery and divergent/stale revisions are guarded.

[P2 contract and format](docs/TERRAIN_PROP_P2.md), [all 34 implementation paths](docs/evidence/terrain-props-p2/implementation-files.txt), and [native evidence and limitations](docs/evidence/terrain-props-p2/README.md). Collision-interest streaming, full asset/texture streaming, budgets/eviction and cooking remain deferred; their proposed interfaces remain in TERRAIN_PROP_P1.md.

**Baseline and concurrency.** Fresh `origin/main` remained `db8d620f75a5219ae5a3906e2837ed2d4165c741` during final source validation. Combined `git diff --check origin/main...HEAD` passed. Wicked remains `82557d439a41d104f559b0d3134d160b7bf6154e`; imnodes remains `eb36902c892548ef94f88f51ad7e7c9c7058a71c`. The documented ten configuration-generated Wicked overlay edits remain uncommitted; neither gitlink changed. Other working trees and the owner's running editor were preserved.

**Source review.** A separate review of the frozen implementation identified and verified closure of four issues: extreme/nonfinite numeric input, cancellation/work accounting inside dense retained-placement scans, changed bounds for legacy placements outside their historical page cell, and repeated owner discovery after an unchanged resolution failure. The final pure authoring test reports 59,752 assertions.

**Native creator workflow.** Final native scripted runs at inspector widths 520 and 280 generated 1,081 placements, erased to 1,062, verified Undo/Redo and that regeneration respects the clearing, painted to 1,076, cancelled staging, then saved and reopened 1,076. Real shared numeric callbacks exercised FLT_MAX, negative FLT_MAX and NaN. Actual full-window captures were inspected after render/layout stabilization; all numeric fields and lower controls are visible. Private test desktops avoided the owner's mouse/keyboard. The enlarged 1600px capture window is separate from the original measured viewport.

The separate player loaded the exact scene bytes from the final editor proof, captured frame 60 and exited normally with code 0. Scene SHA-256: `04F17C9B0C7414A9E921E6573FDB45C5B76F822943E0DD9BFC7300A137B972DC`; Runtime SHA-256: `2152D68025CE50BB46A16C896C5D95C8C2A6851CB5925335B78654F9DA59EB17`. Its Runtime log contained no warning, error or missing-file matches. The disposable player layout staged the existing default-grass fixture support explicitly; this is ordinary project-mode proof, not arbitrary exported-package closure.

**Performance limitation.** The test models are deliberately tiny. Earlier P0/P1 mesh buffers totalled only 3,392 bytes. Roughly 75 FPS / 13ms intervals under the display cap do not establish headroom or performance with detailed assets. No production woodland FPS, geometry/overdraw budget, shadow cost, physical memory saving or real-asset streaming budget is claimed. A representative uncapped benchmark with the owner's actual assets remains necessary before making those claims.

### Final local validation

| Check | Result / receipt |
| --- | --- |
| Release complete preflight | PASS, including 341/341 registered tests, package archives, LP05 dependency proof and LC01 init/update/move/reopen registry proof; `BUILD/p2-preflight-Release-confirmed/build-result.json` |
| Debug complete preflight | PASS, including 326/326 registered tests, package archives and LP05/LC01 subprocess proofs; isolated p2-preflight-Debug-physical/build-result.json |
| Debug pure authoring | 59,752 assertions, exit 0; `authoring-debug.txt` in evidence |
| WARP Release | 4/4 PASS in 120.85s: native allocator, registered overlay, texture residency and slice; isolated `p2-final56-warp-test.log` |
| Release constructor startup smoke | exit 0; `BUILD/p2-startup-smoke-release.json` |
| Final editor / separate player | PASS as described above; captures in `docs/evidence/terrain-props-p2/` |

The Debug registered suite has 326 cases; fifteen native graphics cases are Release-only by existing registration. The documented direct Debug residency and native slice both exited 0, as did the Debug constructor startup-smoke. Receipt: isolated p2-debug-native-physical/result.json. Debug Studio SHA-256: 65BF18FA0CE13CB152A2690B1CCE4BE4D188519DBB6247377BC768E37073A2F8; probe SHA-256: C47B84EC6CA74FF6E6B73D0EF8BDD6A51857283B6CF3FFC4C4AC5365CA66D7CD.

### Commands and local adaptations

The repository Studio preflight builds both Studio and Runtime, runs the unfiltered registered CTest suite, and assembles/verifies both packages. Its task-local adapter preserved the exact upstream pins and checked the exact ten generated overlay paths and SHA-256 bytes instead of resetting them. No test assertions or deadlines were removed.

```powershell
git fetch origin main
git diff --check origin/main...HEAD
git ls-tree HEAD WickedEngine ThirdParty/imnodes
cmake -S . -B BUILD/native -G "Visual Studio 18 2026" -A x64
powershell -ExecutionPolicy Bypass -File BUILD/Build-P2-Preflight-Local.ps1 -Configuration Release -ArtifactRoot BUILD/p2-preflight-Release-confirmed
ctest --test-dir BUILD/native -C Release -R '^RenegadeAssistantRiverKnowledgeTests$' --output-on-failure
cmake -S . -B BUILD/warp -G "Visual Studio 18 2026" -A x64 -DRENEGADE_TEST_FORCE_WARP=ON
cmake --build BUILD/warp --config Release --target RenegadeTerrainPropResidencyProbe RenegadeGpuUploadBufferTests --parallel 2
ctest --test-dir BUILD/warp -C Release -R 'Renegade(TerrainProp(ResidencyProbe|SliceProof)|GpuUploadBufferTests|RendererOverlayComposition)' --output-on-failure
```

The registered renderer-overlay fixture now performs only its disposable cold/reconfigure work under system temporary storage, keeping logs at the requested build-tree output root. Its original 60-second timeout and UMA/wireframe assertions remain. The negative control still rejects a missing UMA witness.

Validation originally filled the system drive with this task's generated Debug link maps. Only owned generated outputs were moved and regenerable ILK files removed; source/projects/owner files were preserved. The system drive recovered about 55 GiB. Local Debug linking used `/debug /INCREMENTAL:NO`, retaining Debug CRT/assertions and compiler optimization settings. A one-off MSVC C1001 in an existing Story Flow boundary test recovered with the unchanged source in a serial invocation; its registered test passed 1/1. The task-local extra `/MP3` was removed for the final Debug run; standard upstream settings were retained.

The moved validation build used a C-to-E directory junction. Package tests reproduced a working-directory-dependent failure through that junction; the same unchanged executable passed from the physical build directory. All thirteen affected package/fixture tests passed when CTest used the physical directory. The final Debug adapter changes only that test-directory path; the full registered suite is rerun with all checks enabled.

The first frozen Release full suite retained one assistant-knowledge timeout (340/341). Its unchanged isolated retry passed in 3.26s. A complete subsequent preflight passed 341/341, with that test at 2.37s. Earlier disk-full, compiler, path and screenshot-tool failures remain in ignored raw logs; they are not green receipts.

### Remaining scope and next work

Saved mixes are layer-local. The first palette requires one finite terrain and refuses ambiguous multiple authored layers. Legacy key-zero placements remain manual; erase clearings persist until Undo. Cross-process editing, asset reimport during active projection, arbitrary moved/exported populated projects and representative forest performance still need separate proofs. Proceed to later streaming stages only under a new scoped task. No merge without owner approval.


## Terrain props P2 hosted-path correction, 18 November 2026

Native implementation: `59c4e926b4a34e9d1523474e8e06b440ff691867`, following failed hosted PR head `a2522190ff920be4dd48be807b6d10b8f9d7bd56`. The final evidence commit also contains a reviewed architecture-document fingerprint refresh in `Tools/assistant_knowledge/workflows.json`; that exact metadata change was present in both full-preflight working trees. The C++ sources match the implementation commit. Obtain the final documentation/evidence head with `git log -1 --format=%H -- HANDOFF.md`.

**Hosted failure.** Studio compiled in both configurations. The longer hosted checkout path made terrain revision transaction artifacts exceed ordinary Windows file-open limits. Release failed TerrainPropSliceProof and TerrainPropEditTests; Debug failed TerrainPropEditTests. The registered overlay passed in both hosted configurations. The original native edit binary reproduced the hosted staging failure at matched checkout depth.

**Correction and source files.** `EngineBridge/src/ProjectDocumentTransaction.cpp` now uses compact indexed sibling artifacts with journal schema 2, while retaining version-one recovery and its version across an interrupted retry. `Tests/ProjectDocumentTransactionTests.cpp` exercises long digest filenames, unchanged-document filtering, validation, commit, rollback, interrupted recovery, invalid artifact indexes and legacy recovery. `docs/ARCHITECTURE.md` records this choice. The assistant's existing UTF-8/LF fingerprint for that document is refreshed without changing its Surface Type instructions or stale-evidence checks. The original transaction implementation fails the new regression; the correction passes.

| Correction check | Result |
| --- | --- |
| Complete Release preflight | PASS; 341/341 tests, both packages and LP05/LC01 process proofs |
| Complete Debug preflight | PASS; 326/326 tests, both packages and LP05/LC01 process proofs |
| Matched hosted-depth native repro | Original Release edit fails; corrected Release edit/slice and Debug edit exit 0 |
| Long-filename regression negative control | Original code fails four checks; correction passes |
| WARP corrected C++ snapshot | 4/4 PASS in 126.62s; allocator, overlay, residency and slice |
| Fresh editor and separate player | PASS; 1,076 placements retained, exact saved scene bytes, frame 60, exit 0; captures inspected |

**Evidence and commands.** [Correction record, commands and limitations](docs/evidence/terrain-props-p2/ci-path-correction.md). All full-suite assertions and original timeouts remain. The first local correction Release attempt retained three assistant scratch-I/O timeouts; the complete subsequent run uses system-volume scratch database/output arguments for those three knowledge cases. The stale architecture evidence failure discovered on retry is retained separately; the refreshed complete 84-case workflow suite passes. Stock terrain packing used unchanged executables and hash-verified, byte-identical generated products as described in the evidence. No original packer was stopped. The Debug preflight retained the established physical-directory test adapter and nonincremental Debug linking.

**Baseline and scope.** Fresh main is `db8d620f75a5219ae5a3906e2837ed2d4165c741`; Wicked remains `82557d439a41d104f559b0d3134d160b7bf6154e`, imnodes `eb36902c892548ef94f88f51ad7e7c9c7058a71c`. The ten governed configuration overlays remain uncommitted. Collision-interest, full asset/texture streaming and representative full-game forest acceptance remain deferred. Compact artifacts remove avoidable suffix growth from otherwise valid destinations; arbitrary overlong project roots are not newly supported. Hosted validation of the corrected pushed head, independent/owner acceptance and merge approval remain required. No merge is authorized.

## Terrain props P3 source-freeze checkpoint, 18 November 2026

**Implementation candidate; full frozen preflight and acceptance remain pending.** P2 PR #198 was owner accepted and merged as `38196e9869fabd36c9255c64cd248d1000b68285`. P3 proceeds on `feature/terrain-props-p3` from that baseline. The final validation receipt must identify the tested implementation commit. No P3 push, hosted green or merge claim is made here. Do not merge without owner approval.

Implementation covers document-derived shared capsule/box collision, an all-map query BVH, bounded native residency with actual ID reclamation/reserve, player/NPC/vehicle/scripted and wide sleeping-support interests, target terrain readiness, ordinary-world/prop nearest-hit merging, swept projectiles, perception and authored navigation. Collision fit, friction/surface and uniform scale controls use the existing editor mutation/history boundary. Schema 3 reads v1/v2 defaults and keeps placement pages at v2. See [P3 implementation and boundaries](docs/TERRAIN_PROP_P3.md).

Native integration composes guarded build-tree physics, Scene and terrain adapters with the accepted pin. Gitlinks and preexisting generated Wicked bytes are preserved. Terrain interests use the existing generation queue; zero-time simulation holds allow that same queue to prepare collision. No additional scene/physics step, terrain generator or asset/texture streaming service is introduced.

### Observed implementation checks

- Release Studio and Runtime build. Seven focused suites passed: PhysicsLua, ProjectileWorld, Phase6NativeNavigation, TerrainPropEdit, TerrainPropPhysics, TerrainPropCollisionIndex and TerrainPropCollisionWorld. These include actual 65,536 native-ID exhaustion/reuse, publication failure, late revision conflict, anchor replacement, terrain-owner deletion, legacy manifest reads, Undo/Redo and save/reopen.
- Native SliceProof plus CollisionWorld passed 2/2 after the legacy fixture was corrected to emit actual v1 profile fields. The earlier fixture failure is retained.
- Wide sleeping support, native character support identity, removed-support handling and late-global teardown regressions pass. The teardown negative control reproduces Runtime's `0xc0000374`; process-lifetime weak/request bookkeeping fixes it while controllers continue explicitly releasing native bodies.
- Isolated native Studio retained 3,950 placements through Generate, Erase, Undo/Redo, Paint, cancellation, Save As and reopen. Both 520- and 280-pixel Inspector layouts were inspected. The narrow proof also exercises the shared preview-focus callback and visibly frames the orange box around a scaled boulder.
- The separate native Runtime proof passes player support on the selected authored boulder, an NPC wall contact, pending teleport source retention, destination terrain admission and landing. It reports four waiting updates, 3,597 indexed colliding placements, 834 resident prop bodies, 917 total native allocations and 8/8 required terrain chunks. The copied saved scene's SHA-256 is `2bbc6ab7a96ca643c50f5ee510bf922385bfc40705cd03e8b74f48bc32619bbb`. Runtime executable SHA-256 is `64717ebda18e80601e4eeb7e1c2508abcf148dabf1c194a8cc5cd45d4c9f6466`. Its regular close exits 0 with no unexpected warning/error matches. Four expected collision-preparation warnings are counted separately.
- Earlier native attempts retain an incorrect capsule-centre proof assertion, pointer-valued support userdata handling, excessive proof-NPC acceleration and the shutdown crash. The fixed proof uses the pinned foot-origin convention, locked native support identity and the normal acceleration/friction convention. Failures are not green receipts.

Raw attempt logs and local launch/capture/build adapters remain ignored under BUILD. Final full Debug/Release suites, both packages/process proofs, WARP, isolated renderer-overlay checks, deeper Windows-path reproduction, frozen-commit review and final creator/player capture receipts remain outstanding. These technical fixtures establish no production-asset forest performance claim. Asset, geometry and texture streaming remain later stages.


## Terrain props P3 local validation and review candidate, 18 November 2026

Product implementation is `9989b3f1599781c26d7ee5c62287919348976295`, with independent Runtime scene-transition fixture link correction `ed959f9f1aeea98e2311ffd4cd8e8183e367f089`. Final native Studio/Runtime and bridge product hashes match that validation snapshot. The publication also contains four updated source contracts, test-only native crash diagnostics and a Studio CI environment setting; exact tested working-tree source hashes are in the evidence. Obtain the final publication identity with `git log -1 --format=%H -- HANDOFF.md`. Main remains the accepted P2 merge `38196e9869fabd36c9255c64cd248d1000b68285` at final local validation.

**P3 implementation candidate, not an accepted release gate.** Collision fit/friction/surface/scale authoring, bounded native collision residency, actor/destination terrain readiness, all-map queries, swept projectiles, perception and authored navigation are implemented. Asset/geometry/texture streaming and arbitrary hull cooking remain deferred. [Implementation and boundaries](docs/TERRAIN_PROP_P3.md); [validation commands, captures and limitations](docs/evidence/terrain-props-p3/README.md); [changed implementation paths](docs/evidence/terrain-props-p3/implementation-files.txt).

| Local check | Observed result |
| --- | --- |
| Release complete unfiltered CTest | 344/344 PASS in 559.28 seconds |
| Debug registered test set | 328/329 in the initial full run; sole unchanged 250 ms deferred-launch case passed 10/10 targeted rechecks with hash-identical fixtures on fast scratch; all 329 unique cases verified |
| Studio and Runtime packages | Both configurations PASS; LP05 dependency and LC01 init/update/move/reopen process proofs PASS |
| Constructor startup smoke | Debug and Release exit 0 |
| WARP and renderer overlay | 4/4 PASS in 63.91 seconds; overlay 5.35 seconds under its unchanged 60-second timeout; final native Release registered overlay also PASS |
| Hosted-depth native checks | Debug and Release Edit plus Release Slice exit 0 at matched checkout depth |
| Source-contract controls | Four positives and eleven deliberately broken-source negative controls PASS |
| Fresh native editor/player | Both inspector widths and separate exact-scene player PASS; captures inspected; normal close exits 0 |

The editor proof generated 3,933 placements, erased to 3,925, verified history and persistent clearings, painted to 3,950, cancelled staging, saved/reopened 3,950 and focused the collision preview. Inspector widths 280 and 520 were inspected. The enlarged capture window is separate from the measured viewport. The player loaded the exact wide-editor scene, SHA-256 `BB2021057EF1A3C3725CA74AE91C4B561D4F0E95C7F0EDEAAB55597914EAAEC7`. Runtime SHA-256 `1FCF09EC09D9009B81DC68EE67DF6011C1990C12B2C21DB16AFDFFC12B2DB8F5`. It passed native boulder support, NPC wall contact, four pending teleport updates with source support held, destination admission and landing with 8/8 terrain chunks ready. Counts: 3,597 indexed colliding placements; 834 resident props; 917 total native allocations. No unexpected warning/error/missing-file matches; four expected collision-preparation warnings are separate.

**Failure disposition.** The earlier intermittent Debug maximum-terrain access violation remains unexplained. It also occurred in an isolated run after overlapping suites had ended, so concurrency alone is not an established cause. It did not recur in the final full Debug/Release native terrain checks, registered repeat or debugger runs. Those passes are not a root-cause fix. Opt-in first-chance dump capture is verified by an intentional failing process and enabled for Studio CI; normal exception propagation/assertions/timeouts remain. Keep this risk explicit in review. The earlier editor driver's 150-second timeout did not recur in the fresh complete proofs.

Earlier local harness failures from shared fixed temporary directories and overlong replacement scratch roots are retained in ignored raw logs. Current runs use separate short roots and serial configurations. The Debug receipt explicitly preserves its initial failed full run and successful targeted recheck; it does not claim a single 329/329 invocation. Native aggregate build logs/product hashes and separate test-diagnostic rebuild witnesses guard reuse of already compiled products. Three knowledge scratch arguments and the process fixture's hash-identical local execution copies are the documented environment adaptations; no test threshold was raised.

Wicked remains `82557d439a41d104f559b0d3134d160b7bf6154e`; imnodes remains `eb36902c892548ef94f88f51ad7e7c9c7058a71c`. All ten preexisting generated Wicked overlay paths/bytes remain uncommitted and preserved. Technical fixtures establish no performance headroom with production woodland assets. Hosted validation and independent exact-publication/owner acceptance remain pending. No merge without owner approval.

## T1 material texture cooking and native streaming

Implementation commit `adcab60dbd63238d984aa563f0d607f55f77d6bd` follows P3 on merged PR #199. The complete code change list is available with `git diff --name-only 3e0c69e..adcab60`. The shared texture importer, material binding, native file reader and Build Game dependency providers are described in `docs/TEXTURE_PRODUCTS_T1.md`. Stock grass now has one content-addressed cook across Studio, Runtime and the proof executable.

On Windows x64 Release, the six focused cooking, upload, overlay, streaming and terrain checks passed. The five corresponding WARP graphics checks passed. The renderer overlay took 6.65 seconds on hardware and 5.22 seconds on WARP against the existing 60-second limit. Run the registered tests with `ctest --test-dir BUILD/native -C Release -R "MaterialTextureCookTests|GovernedTextureStreamingProof|TerrainPropResidencyProbe|TerrainPropSliceProof|GpuUploadBufferTests|RendererOverlayComposition" --output-on-failure`. The native proof directly requests resolutions and verifies GPU readback across Save As, reimport, project relocation and package binding.

Two production Build Game exports passed their packaged Runtime smoke. The populated scene package contained two revisions, 399 placement pages and three reusable assets. The governed boulder package contained one revision, one page, one model and its texture. All audited products matched their project bytes, and the governed package contained no retained source image. The previous exporter reproduced Runtime exit 24 without the prop dependencies. A private relocated player run of the original 3,950-placement scene passed with four expected collision-readiness messages and zero unexpected warnings, including zero missing grass files. See `docs/evidence/texture-products-t1/runtime-boulder.png` for the actual DX12 frame. The technical assets do not establish production forest performance.

First bind still reads the selected DDS and retained input. Embedded textures in legacy model archives keep their existing native path. T2 memory-pressure policy and whole-library migration are separate work. Full Windows Studio preflight at 111f290 passed 330/330 Debug and 346/346 Release tests. Release startup-smoke exited 0. One earlier intermittent maximum-terrain-preview access violation remains unexplained. Six focused reruns and the final full Debug run passed. Hosted checks and independent acceptance are pending.

The horizontal strip also appears in the pre-T1 P3 capture. In a disposable diagnostic Runtime, native camera rays hit the fixture objects Proof ground and chunk_0_0 at the same world height (y=0). Hiding only the renderable Proof ground plane exposed continuous grass across the strip at the original camera angle. See docs/evidence/texture-products-t1/runtime-plane-hidden.png and runtime-wire.png in the same directory. The latter uses the lower camera angle and shaded wireframe. The original QA scene and shipped Runtime were preserved.

## P4 Runtime cell experiment

Local implementation checkpoint b558a564f10b4f05249b9d7e07facad5f7f85e16 on the merged T1 source tree. Use git show --stat b558a564f10b4f05249b9d7e07facad5f7f85e16 for the six changed paths. Release player experiments set RENEGADE_P3_RUNTIME_PROOF to a writable log path and RENEGADE_P4_RESIDENCY_PROOF=1 on a disposable TerrainProof project. Set RENEGADE_P4_CELL_RESIDENCY=1 for the experimental run and leave it unset for the control. The driver uses the normal player input for the 48 m walk each way after a 198 m teleport. The fixture ground overlay is hidden for both screenshots.

Both runs completed and exited successfully. The control held 4297 prop render objects. The experiment sampled 1539 at the boulder, 1466 after teleport, 1556 at the walk turnaround, 1466 after walking back, and exactly the original 1539 entities upon returning to the boulder. Player movement crossed three cells. The hardware reported 1828847616 bytes of application GPU memory at every milestone in both runs. One sampled run per mode peaked at 1434 versus 1754 MiB working set and 4290 versus 4422 MiB private bytes (experimental versus control). These process-wide samples do not isolate prop allocations. The earlier projection regressions passed with ctest -C Release -R "^(RenegadeTerrainPropEditTests|RenegadeTerrainPropSliceProof)$" --output-on-failure.

The path is opt-in. Distant cells retain representative placements using the original meshes. All cells are still materialised at scene load and all four source meshes remain resident. Remaining gate work includes independently loadable coarse geometry, asset lease and lifetime management, total population bounds, reload/cancellation and representative production-asset visual and performance checks. Full local preflight and independent review are outstanding.

## P4 scale-aware draw distance and measured model-loading hitch

Implementation 407b4cfd2ae831e8c738627dcff5f7588b3de76d follows e648b1b. Run git show --stat on each revision for changed paths. Studio terrain Prop Settings now accepts a unit-scale draw distance in metres. Zero keeps unlimited range. An optional v5 profile field persists the range. V1 through v4 inputs retain their prior behaviour. The renderer applies its native fade and distance checks to coarse, fine and single-model placement parts. This setting does not unload meshes or change gameplay collision.

On Windows x64, build RenegadeTerrainPropEditTests in Release and run ctest -C Release -R "^RenegadeTerrainPropEditTests$" --output-on-failure. Passed including save/read, Undo and a 3x scaled placement with a 125.5 m profile receiving 376.5 m native range.

Private 1920x1080 RenderPath3D benchmark: same seeded 200 supplied trees and 40 rocks, shadows on, route near/far/near. Trees used 360 m and rocks 100 m at unit scale for this experiment. At the far checkpoint, objects passing renderable and range checks fell from 241 to 38; corresponding mesh triangles from 935936 to 167375. These are pre-frustum range counts. Near and return matched exactly in each mode. The far skyline was thinner with these values. GPU allocation remained 780140544 bytes at all checkpoints.

A separate 601-update moving-camera trace crossed 300 m and returned. At z=-224 on return, owner-thread scheduler time reached 4752.5 ms with unlimited range and 4743.02 ms with distance culling. Product reopening and native scene preparation accounted for seconds in the asset log. The expensive work runs in the fine-model acquisition path. Do not treat the last still frame, average FPS, or draw distance as evidence of smooth traversal. Gate remains open pending bounded background preparation, stale-result handling, real-asset standalone player timing, full preflight and independent review.

## P4 background model preparation and standalone player trace

Implementation 5e34fbc58ef18c0974ff6b0ec61f45eb28a2b8dd. See git show --stat for four changed files. The runtime starts at most one detached product preparation at once and polls its future. Only the owner thread installs or evicts render objects. Returning from an obsolete scene or leaving the desired region invalidates the installation. Per-operation staging filenames isolate concurrent uses of the same asset.

Release Studio and Runtime built. Build RenegadeTerrainPropResidencyProbe in Release and run it with PROJECT_ROOT --mesh-detail PROJECT_ID FINE_ID COARSE_ID. The private oak fixture passed departure during load, scene revision during load, return to fine, and revision while fine was resident. Coarse-only views measured 132530 bytes; fine plus coarse 486130. The normal standalone 3950-placement player regression exited zero, reported P3_RUNTIME_PROOF_PASS and had zero unexpected warnings. It logged 105 expected terrain readiness messages on this run.

The native 200-tree / 40-rock moving camera route peaked at 0.9013 ms of scheduler work versus 4743.02 ms for the prior culling run. All three endpoint geometry counts matched. A separate instrumented Release standalone Runtime used the actual physics player and an automated 300 m out-and-back position trace, one metre per frame. Its returned scene settled at 19 meshes, down to 12 at the far checkpoint. Scheduler maxima were 0.482 ms outbound, 0.032 ms inbound and 0.766 ms while settling after return. Maximum full frame intervals were 24.66, 14.99 and 16.82 ms respectively. The initial near-view phase included a 377.35 ms frame interval. Fine detail took about 13 seconds after return to finish loading. The final DX12 window capture showed textured woodland, shadows and a 75 FPS overlay. The diagnostic movement and timing instrumentation were confined to the verification executable. Full preflight, fine-detail latency improvements, total population bounds and independent gate acceptance remain open.

## P4 queue, player speed comparison and population scaling

Changes b6051f7 and c8cb327fd5b5bd5cfeda008036ced5428dca9050 preserve authored placement draw distance when legacy cells return, order new cells by player proximity, share pending work by scene revision and profile, and cap preparation at two jobs. See git show --stat for changed paths. The native oak probe also exercises single-model distance restoration, cancellation on departure and revision change, fine eviction and re-entry. Release Runtime and RenegadeTerrainPropResidencyProbe built. The real-asset probe passed. Run ctest -C Release -R "^(RenegadeTerrainPropSliceProof|RenegadeTerrainPropEditTests)$" --output-on-failure; both passed in 8.36 seconds.

Private standalone DX12 player, 200 supplied trees and 40 rocks, same 300 m out-and-back route. At 75 m/s, two one-worker runs settled 12.87 and 13.00 seconds after stopping. Two two-worker runs settled in 11.48 and 11.54 seconds. Full-frame startup maxima were variable: 377/396 ms with one worker and 1276/282 ms with two. The two-worker repeat had a 156 ms frame during return settlement, while the timed scheduler maximum in that phase was 0.80 ms. At 7.5 m/s, six of seven fine meshes were back on arrival; the last arrived 2.08 seconds later. P99 inbound frame interval was 16.24 ms, maximum 17.04 ms. The outbound maximum was 45.87 ms. These are automated player-position traces on the owner GPU. Fast traces exercised the queue change; the 7.5 m/s trace also included the single-model distance fix.

Native render benchmark at a fixed camera, 1920x1080. Compare one 240-placement tile against nine identical tiles spaced 1024 m apart (2160 placements). Both produced exactly the same RGB frame, 212 range-eligible objects, 1595662 range-eligible triangles, 17 scene meshes and 780140544 device bytes. Scene objects increased from 304 to 2224 and transforms from 307 to 2227. Every additional out-of-range placement still became a native object and transform. The measured blocker is the coarse/single-model projection created at scene restore. The model scheduler bounds fine loads but does not yet bound that initial projection or its steady-state population. Remaining acceptance includes paging native placement creation by view/interest with conservative transformed bounds and fallback coverage, startup tracing, full local preflight and independent review. No hosted run for this checkpoint.

### P5 woodland mesh distance validation

Tested Release libraries from dd4913e70641d2b710205b719fb6a51e1e8c7970 against merged baseline 9eae1003308d39bca68da5cd9329bc41f60bc878 (identical tracked tree). This review changes HANDOFF.md only. The private fixed-seed tile contains 200 trees and 40 rocks. All six final 1080p stationary captures passed. At ground and elevated cameras respectively, normal mesh residency with shadows gave median CPU frame times of 0.578 and 0.529 ms, GPU frame times of 1.391 and 1.465 ms, and shadow pass times of 0.218 and 0.202 ms. Forcing coarse meshes gave GPU medians of 1.800 and 1.911 ms. Each median covers 60 matched frames. Reproduce with the private QA scripts run.ps1 -Count 200 across views 0/1 and (Shadows,Lod) pairs (0,0), (1,0), (1,2), then run python summarize.py. The QA README documents environment and settle settings.

The separate nine-tile player approach measured median 13.34 ms, p99 16.42 ms and worst frame 39.41 ms. A 1175 ms pre-entry spike did not recur in repeats. A roughly 380-482 ms launch gap remains. One exploratory elevated coarse capture crashed in nvwgf2umx.dll; its identical retry passed. Cause unresolved.

Keep the supplied meshes as the visual baseline. All three experimental baked oak representations failed visual review. No impostor ships from this evaluation. Next validation is a moving transition across mesh distance thresholds, including silhouette and shadow continuity at different authored scales. Independent exact-commit verification remains open.

### P5 moving oak transition
Two private Release runs completed 1400 frames each, shadows enabled, using the same supplied oak at scales 0.6, 1.2 and 1.8. Camera z moved from -310 to -35 m, paused, then returned. Ground view upgraded at frames 720/721 during the near pause; elevated view upgraded at 816/817 during retreat (z=-68/-68.5 m). Both downgraded at 873/874 (z=-96.5/-97 m). The three-oak scene changed from 9581 to 33050 renderable triangles. In ground frames 700 and 721 the camera is identical. The canopy outlines and leaf coverage change visibly; trunks and broad shadow footprints persist. This is a visual transition failure despite two successful captures. A held camera and delayed fine residency make the change particularly apparent. The elevated route has no matched-camera upgrade pair. Reproduce using the private motion QA scripts with Count 3, Lod 0, Shadows 1 and View 0/1, draw distance enabled. Review motion.csv and the ground swap.jpg; these captures include synchronous readbacks and do not measure traversal frame times. Next isolate asynchronous readiness from mesh-switch presentation before changing engine behavior.

### P5 oak readiness trace
Three private Release routes (1400 frames each) traced a shared fine-model request for three scaled oaks. The ground and elevated stress captures requested at z=-96 m. Preparation took 1410.52 and 1445.23 ms, followed by 1.93 and 0.44 ms until the first fine batch was enabled. The second batch followed on the next frame. The walking-speed control used 0.1 m steps with a target cadence of 75 Hz and disabled screenshot readbacks. It requested at z=-96 m and first enabled fine geometry at z=-87.6 m, after 1292.83 ms preparation and 2.56 ms from readiness to activation. The reopen log attributed 1045.79 ms to the scene stage and 243.52 ms to product processing. Explicit texture restoration took 0.06 ms. These are worker timings for one model, not application stalls. Both cells returned to coarse on retreat in all runs. Reproduce using the private trace and paced harnesses with Count 3, View 0/1 (paced View 0), Lod 0 and Shadows 1. Run summarize-traces.py to check the event order against motion.csv. Investigate the scene reopen stage for earlier availability and test transition presentation separately at matched camera positions. The walking control recorded activation without image capture.

### P5 preloading control
A private Release harness staged one shared fine oak payload before traversal, then consumed it through the existing page acquisition path. Three oaks at scales 0.6, 1.2 and 1.8 used this payload. Both paced 1400-frame routes passed. Ground/elevated preparation was 1496.73/1374.17 ms before movement. Both first enabled fine geometry on the first eligible frame (340, z=-96 m), finished on frame 341 and restored coarse geometry on frames 1061/1062. The earlier on-demand ground control first upgraded at -87.6 m. At a fixed camera across the threshold, two coarse frames were identical to displayed precision. Coarse-to-fine mean absolute RGB differences were 0.647/0.573 levels across the 1080p image. The gross crown silhouettes persist but foliage and elevated shadow detail change. Scripts build.ps1, run.ps1 (Count 3, Shadows 1, View 0/1) and compare.py reproduce this private staged experiment. It establishes first-use readiness for one shared model; production level loading, re-entry caching, dense-scene memory cost and visual acceptance remain open.

### P5 player spawn preload integration
Local Runtime change 423a1ec modifies RuntimeApplication.cpp and RuntimePropModelResidency.h. The normal player uses model residency and the separately experimental distant cell thinning remains opt-in. The spawn path selects up to two unique fine models by distance in the local nine-by-nine cell area, prepares them before player spawn and consumes each prepared payload on first acquisition. Revision changes clear staged payloads. The count limit is not a byte or time budget.

Built the retained Release RenegadeRuntime.vcxproj with BuildProjectReferences=false, CL_MPCount=2 and /m:1. Clear RENEGADE_P4_CELL_RESIDENCY and launch the Release player with --project pointing at the mixed woodland descriptor and --renegade-ready-event pointing at a fresh named event. For 200 trees and 40 rocks, the retained baseline reached ready in 28.28 s, working set 1194 MiB and peak 1644 MiB. Final candidate reached ready in 22.39 s, working set 1483 MiB and peak 1791 MiB. Sampled working sets three seconds after readiness. The spawn log confirmed two shared models in 2786 ms. Single sequential samples, different cache states, no load-speed claim. The DX12 screenshot shows a populated woodland at Player Start. These runs used a project descriptor and bypassed Studio snapshot creation. The retained Release slice, edit and collision-world CTests passed 3/3 in 8.29 s. Full preflight, independent review and Studio snapshot traversal including moving canopy acceptance remain open. Private run commands, logs and captures are retained in RenegadeP5RuntimeQA.

### Woodland Test Game scene-load failure and P5 ground traversal

Implementation 84c38d13dd32b32fdd7abb25d532a0bddc85f325 on the local review branch fixes Runtime code 24 when Test Game relocates a woodland snapshot. The former Studio launched a player that failed to resolve the terrain revision; the authored scene opened with its complete population. EngineBridge/src/TestLevelSnapshotService.cpp now stages the referenced revision, pages, registry, fine and coarse model products, and governed model resource bindings. Deferred staging uses the captured scene and the selected destination. Tests/TerrainPropSliceProof.cpp reopens both eager and deferred snapshots after moving each away from the original registry. The test exercises cleanup with generated path names. The regular scene save format is unchanged.

Release validation used the retained Windows x64 configuration. From a configured native build run these commands (PowerShell, with MSBuild on PATH).

~~~powershell
foreach ($target in @('EngineBridge/RenegadeEngineBridge','Studio/RenegadeStudio','Tests/RenegadeTerrainPropResidencyProbe','Tests/RenegadeTestLevelSnapshotTests','Tests/RenegadeTestLevelSnapshotRuntimeTests')) {
    msbuild "$env:RENEGADE_NATIVE_BUILD/$target.vcxproj" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /m:1 /v:minimal
    if ($LASTEXITCODE) { throw $target }
}
ctest --test-dir $env:RENEGADE_NATIVE_BUILD -C Release -R 'RenegadeTestLevelSnapshot|RenegadeTerrainProp' --output-on-failure
~~~

All 11 selected tests passed. In a fresh-process woodland probe, moving the session to another volume preserved the 16 model IDs and all 17 terrain files. Detailed native reopen counted 241 rendered objects in both the source and the snapshot (240 props plus ground). The Studio toolbar Play action launched the DX12 child, its native ready signal fired and RuntimeBootstrap reported SUCCESS with exit code 0. Ground-level verification included walking forward, pausing, backing away and returning. The Release editor returned to Test Level completed and removed the session. A repeat from the same open editor also reached ready, exited normally and cleaned its separate session.

The following sequential ground comparisons ran the retained pre-preload and candidate Release players against the same captured woodland descriptor. Each used 120 warmup frames and 1440 measured host frame intervals. In an isolated profile set RENEGADE_G13E13_PERF_CSV to an output CSV, RENEGADE_G13E13_FRAMES to 1440 and RENEGADE_G13E14_PLAYER_TRACE to a disposable position CSV. Launch RenegadeRuntime.exe with --project pointing to the captured TestLevel.renegade and alwaysactive. Repeat in reverse order with the position trace set to NUL to remove the periodic position file writes. The first runs travelled 64.83 and 64.69 metres respectively. RSS is the largest working set sampled at half-second intervals after readiness. The GPU column uses the native DXGI process-local measurement.

| Run | Median ms | p99 ms | Worst ms | Frames over 33 ms | RSS MiB | GPU peak MiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| control | 13.342 | 16.071 | 2508.977 | 7 | 1215 | 921.789 |
| candidate | 13.341 | 16.108 | 1325.087 | 3 | 1805 | 921.727 |
| candidate-repeat | 13.343 | 15.953 | 16.413 | 0 | 1597 | 919.352 |
| control-repeat | 13.345 | 16.090 | 16.495 | 0 | 1196 | 920.789 |

The largest first-pass frames aligned with the position sampling cadence. The ground runs with the position sink set to NUL are the comparison without those disk writes. One candidate benchmark process exited before ready in the NVIDIA user-mode driver. Windows recorded access violation 0xc0000005 in nvwgf2umx.dll at offset 0x117158b. The dump shows DX12 and Runtime frames below the driver. The identical retry completed. The dump does not establish which application call or driver condition caused the access violation. This is an open stability item separate from the repeatable missing-revision exit code 24.

P5 remains open. These ground measurements do not establish elevated silhouette or shadow continuity. The next visual check is a matched ground and elevated approach, hold, retreat and return at an oak detail boundary, with the fine and coarse silhouettes and shadow footprints aligned between views. Full preflight and independent exact-commit review remain outstanding.

On 20 November 2026, Maverick signed off on the Woodland Grounded Test Game fix for exit code 24 (84c38d1).

### P5 moving oak canopy and shadow validation

Local implementation 2360aab1eff9c36c82cfaa0747d4b80c66076841 on review/p5-mesh-validation changes RuntimeApplication and RuntimePropModelResidency, the bridge TerrainPropProjection and TerrainPropDetailProjection, TerrainPropResidencyProbe, and the CMake overlay and shared TerrainPropDetailFade header. The decision is in docs/ARCHITECTURE.md. The validated source plus the associated assistant workflow fingerprint is a04c21528f4c0ae8b463a02ec8fa51569f0ef243.

On Windows x64 with Visual Studio 18 2026, MSBuild 18.6.3 and DX12, configure the retained native build, regenerate embedded shaders, build the common renderer objects, rearchive the embedded renderer and relink the bridge, Runtime and probe. The direct project commands use the same Release/x64 configuration throughout. The variables below name the source checkout, its configured build, the local woodland fixture root, project ID, and registered oak LOD0 and LOD2 IDs.

~~~powershell
cmake -S $src -B $build
foreach ($target in @('WickedEngine/WickedEngine/WickedEngine_common','WickedEngine/WickedEngine/WickedEngine_emb_shaders','EngineBridge/RenegadeEngineBridge','Runtime/RenegadeRuntime','Tests/RenegadeTerrainPropResidencyProbe')) {
    msbuild "$build/$target.vcxproj" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /m:1 /p:CL_MPCount=3 /v:minimal
    if ($LASTEXITCODE) { throw $target }
}
Push-Location "$build/Tests/Release"
./RenegadeTerrainPropResidencyProbe.exe $woodland --mesh-detail $projectId $oakLod0 $oakLod2
Pop-Location
ctest --test-dir $build -C Release -R 'RenegadeTestLevelSnapshot|RenegadeTerrainProp' --output-on-failure
ctest --test-dir $build -C Release -R 'RenegadeAssistant' --output-on-failure
~~~

The native oak probe passed the partial retreat, turn back, retention and mid-fade revision checks. Resident mesh buffer views measured 132530 bytes coarse, 486130 bytes with fine resident, 486130 bytes after reversing and 132530 bytes after releasing on revision change. All 11 terrain/snapshot and five assistant checks passed. The staged review player SHA-256 was 3BD00397262D635B95392E0E0FAC1AA37AFE756C5C7D16231CAB140B1A0BA425. Studio's Test Game toolbar launched its freshly created woodland snapshot, the DX12 ready event fired, RuntimeBootstrap reported SUCCESS, the child exited 0 and Studio removed the snapshot.

Private before-and-after render capture used the unchanged supplied big oak at scales 0.6, 1.2 and 1.8, shadows on, 1920 x 1080. The old executable used the previous distance scheduler. In the native capture runner, set P4_QA_DRAW_DISTANCE=1 and invoke run.ps1 3 0 1 0 and run.ps1 3 0 1 1 for the two views. The player crossed the 96 m cell boundary in approach and retreat, then performed a rapid return. Each camera held its transform for the measured switches. Compare successive saved frames two rendered frames apart in fixed crown and ground-shadow regions. Entries below are the largest mean absolute RGB difference in 8-bit intensity levels, before and after, over the first approach and retreat.

| Region | Approach before / after | Retreat before / after |
| --- | ---: | ---: |
| Ground canopy | 5.0151 / 0.6255 | 5.0151 / 0.6049 |
| Elevated canopy | 4.4016 / 0.5832 | 4.4016 / 0.5690 |
| Elevated ground shadow | 1.4212 / 0.1506 | 1.4212 / 0.1656 |

Near and far endpoint regions at frames 12, 66, 124 and 178 had zero RGB difference in both views. The initial ground control had an unlit sky and was excluded. Repeating the old executable produced matching sky and settled frames. The owner comparison clip uses this repeated control. Private assets, readbacks and harness scripts remain with the local oak validation run.

The previous and candidate Release players separately ran the captured 200-tree/40-rock woodland. Each used 120 warmup and 1440 measured frames at the same settings. The position trace used NUL. Control and candidate respectively recorded median host intervals of 13.345 and 13.339 ms, p99 of 15.993 and 16.047 ms, worst intervals of 16.550 and 16.476 ms, and zero frames over 33 ms. Peak process-local GPU memory was 919.352 and 919.414 MiB. One pair of runs establishes performance for this route. The short overlap can draw both models in a transitioning cell.

The visual and standalone checks cover the oak transition. Overall P5 awaits full frozen Debug and Release preflights with package proofs and independent verification of the exact candidate. The previously recorded intermittent NVIDIA startup failure is still open.

On 20 November 2026, Maverick signed off on the P5 oak canopy and ground-shadow transition shown in Woodland_Oak_Transition.mp4, covering the ground and elevated approach, retreat and turn-back views. The reviewed implementation is 2360aab1eff9c36c82cfaa0747d4b80c66076841, with the measurements and Studio Test Game results recorded in 242e1f28997327df74d61bfdcb267560e874f548.

### Terrain props, Test Game storage and standalone woodland

Maverick signed off the woodland Test Game and canopy/shadow transition on 20 November. The independent source and focused-test review covers the storage correction at f70fbf2d650d8bec83b479a73967350403492833 and standalone dependency collection at 39727d737e8450202987b369e66343dcbc22fead. The candidate including runtime shutdown ordering and the corrected deferred process fixture is d48541e11c524f395534e329189b076415359dd0.

- Test Game captures terrain revision references on the owning thread and measures unique revision, page, model and governed texture files before choosing where to write the snapshot. EngineBridge/src/TestLevelSnapshotService.cpp, TestLevelSnapshotStorage.h, TerrainPropDocument.cpp and its bridge header implement the change. Tests/TestLevelSnapshotTests.cpp covers the 4 GiB reserve.
- Release Build Windows Game originally rejected a generated texture name when inspecting a governed reusable model. EngineBridge/src/ReusableAssetDependencyService.cpp, ReusableAssetPlacementService.cpp, WisceneEmbeddedResources.h and the ReusableAssetService bridge header now identify names with nonempty image data inside the specific model's WISCENE archive. Tests/ReusableAssetTests.cpp covers exact names, empty entries and truncation. Independent inspection found 72 material texture references in 16 woodland models and verified the embedded image for each reference within its own product. An additional native model import with a missing loose emissive texture still emitted a required texture dependency.
- The private authoring project was copied for the export test. Only the copy has a level.complete route from its woodland level to Complete Game, which the production export smoke requires. The Release controller staged and launched the named game. The packaged DX12 child reported successful integrity, completed its Story Flow and exited 0. Final promotion rejected an unexpected empty log.txt. A Windows file-open trace recorded model preparation during exit. The runtime change at 9d8686c56258c1b811d5b35018a691ce4740a3e8 joins outstanding terrain model preparation before closing the Wicked job system. Runtime/src/RuntimeApplication.cpp, RuntimePropModelResidency.h and main_Windows.cpp own the change.
- The earlier f70fbf2 native Debug aggregate built. Unfiltered CTest passed 329/330. RenegadeTestLevelRuntimeProcessTests handed its deferred-ready and restart tests a descriptor containing only the word fixture. Tests/TestLevelRuntimeProcessTests.cpp now writes a versioned descriptor with a stable project ID and the actual test scene. The production validator and the invalid-scene and cancellation assertions remain active.
- Run the Studio Windows build script for Release and Debug. On the review host the CMake build uses two workers. Run the unfiltered CTest suites and both pairs of package proofs. From Release Studio, export the private woodland, relocate the complete exported folder, then launch the named executable with a fresh profile and an unrelated working directory. Match the woodland's 17 terrain files and 16 model products to the retained snapshot and count all 240 placements. Record the resulting build and package receipts before declaring the gate closed.
- The earlier DX12 startup failure is separate from the reproducible packaging failure. A clean-machine launch and the redistribution status of the planned stock library remain P6 acceptance items. Observed temporary model writes under a packaged game's GameData/Intermediate/Imports also need a write-protected installation check.

Creator documentation is in docs/TERRAIN_PROPS_CREATOR_GUIDE.md.


### Read-only woodland export validation

The read-only placement fix is dc17d6ab63c474cf416771a5719a83972ee4e0fe. The previous named game passed package integrity and then failed scene loading with exit code 24 because reusable placement tried to create GameData/Intermediate/Imports. A write-denied disposable installation reproduced the failure. EngineBridge/src/ReusableAssetPlacementService.cpp now passes the governed WISCENE bytes and their logical texture directory directly to the native archive. The configure-time change is in cmake/TerrainPropArchive.cmake and EngineBridge/patches/ReusableModelMemoryArchive.patch. Tests/ReusableAssetTests.cpp exercises compressed, uncompressed and truncated headers. The source and core overlay were independently reviewed on this commit. A clean-index patch check and repeat configuration matched the expected patched header hash bbb8a3d42bf385f6d8fb424bc7f742541c2c215a780f0503758678a4b6b81f28.

The following local exercises used the candidate Release Runtime and Studio build controller rebuilt from the changed source, with existing native libraries for unchanged dependencies. The full frozen Debug and Release preflight is tracked separately.

| Exercise | Result |
| --- | --- |
| Reusable product archive | RenegadeReusableAssetTests passed in Release. The corrected deferred-ready/restart test also passed in Debug when run with ctest -C Debug -R RenegadeTestLevelRuntimeProcessTests --output-on-failure. |
| Test Game snapshot | CreateDeferred and FinalizeDeferredScene captured the woodland. The reloaded snapshot contained 17 terrain revision/page files, 16 model IDs and the same 241 detailed objects as the source. Snapshot cleanup succeeded and the native process exited 0. |
| Build Windows Game | The Release Studio controller staged the named game, completed the authored Story Flow on DX12, passed post-smoke checks and promoted the folder. The controller exited 0. |
| Relocated game | The named executable started with a fresh user profile from an empty unrelated directory. Runtime reported package-relative launch, package integrity PASS and DX12. After 120 warmup frames, 240 measured host-loop frames averaged 13.338 ms, with p99 13.753 ms. The executable exited 0. |
| Protected installation | A separate copy denied file and directory creation and rejected an explicit sentinel write. The standalone passed package integrity, entered Main Level on DX12 and exited 0. Its 240 measured frames averaged 13.339 ms, p99 13.694 ms. The player-camera capture shows the rendered tree canopies, trunks and ground shadows. No game-folder log or placement directory appeared. |
| Terrain content | Both copies contained the 240 placements. The 17 revision/page products and 16 models matched their source snapshot hashes. Neither installation contained SourceAssets. |

The promoted executable SHA-256 was BCCE86B2423939A0765129C2BC1B6CA8866F377B21B2B89B5A83EC777078F280. The relocated and protected manifests both hashed to 3EB24C356939E51E552BD4657DC6AB594FCDA50AFFC6BCB80A66D08E16C8BEDA. The native protected-install capture hashed to 48B9B65362CFB6BD7DC795DACBCD086AEE701B7B4810AB683DC0003A34F2B520. Private artwork and native captures stay with the local test project.

For reproduction, save a woodland with a complete Story Flow route and run Build Windows Game in Release. Move the entire promoted folder to an unrelated location, launch its named game with no project argument from an empty working directory and a new local profile, then repeat with writes denied beneath a disposable installation. Inspect RuntimeBootstrap.log and compare the exported model and page hashes against the saved project. The separate clean-machine and stock-content requirements remain open.

## Woodland starter and collision generation closeout

The approved P5 woodland uses the authored meshes and their distant representations. The 29-asset starter uses redistributable examples and has independent source records. The licensed private benchmark trees stay in the owner project.

Two startup requests could put the same terrain chunk in one pending generation batch. The camera and collision worker then wrote the same grass index buffer concurrently. A cold Release player reproduced the failure. The native crash dump resolved to Wicked's terrain worker and HairParticleSystem::CreateFromMesh. Commit bf361c7ad5d5f3ab7e8b81b57dcab60b75a723cd makes each chunk unique within the current batch and converts absolute priority invalidation coordinates into camera-relative offsets exactly once. The terrain initialization test starts with overlapping collision and camera interests and validates live published chunk identities. The separate earlier code-24 fix remains covered by the read-only installation and snapshot checks.

| Check on DESIGN | Verified result |
| --- | --- |
| Windows Release and Debug | Both Studio and Runtime built. The unfiltered suites passed all 346 Release tests and all 330 Debug tests. Both Studio executables passed their startup smoke check. All four package ZIP hashes matched and the dependency and asset-registry proofs passed in both configurations. The first run finished Release before a Debug build error. An initial Debug suite subsequently hit an access violation during expansion Undo in the terrain initialization check. Three direct reruns using that binary and the final full Debug run passed on the same source revision. The initial dump is retained on DESIGN in the stock-debug-final evidence directory. The cause of the one-off access violation has not been established. |
| Stock palette | All 29 near meshes and 46 additional LOD meshes reopened through Renegade. All 110 material texture bindings were present on the near models. The catalog names the trunk, branch and canopy material groups and records the wind attributes available on each tree. |
| Editable art | The Blender file reopened with 29 asset collections and 90 packed images. The procedural oak leaf litter regenerated to the same GLB digest. |
| Editable woodland | The initial 206 placements and 54 backdrop trees give 260 saved placements in 15 pages, with 52 near/distant model assets and four saved mixes. The eight tree profiles use trunk capsules. The six rock profiles use fitted boxes. |
| Test Game | The production deferred snapshot and Studio process launcher reached the player-ready event, exited 0 and removed the snapshot session. The Release check reopened 558 source objects with all 52 model dependencies. The extracted project ZIP passed the same launch from a separate location. The Debug runtime also passed a native Test Game launch. |
| Exported woodland | Build Windows Game promoted the sample using the matching engine revision. All 143 manifest entries matched. A separately installed copy rejected writes and ran 240 DX12 frames with a fresh user profile. The camera capture shows the tree stand, undergrowth, rocks and clearing. The earlier 240-placement benchmark also passed relocation and write-denied installation on this source. |

The ready-to-import art, source files, notices and catalog are in Library/TerrainProps/WoodlandStarter on the local content branch. The editable project, Blender file, standalone player and portable archives are in the RenegadeTerrainProps folder on DESIGN. The content commit 97ae0f23d726fe34413888dee65a5b3170520183 includes the tree material-group and wind inventory. The content additions do not change a build target or native code.

The final package manifest SHA-256 is 1ad7c9d7355858fa5ce2cb386474a0e6a3c416b3f40b204ba5ba346ee7b47a61. The standalone includes the stock notices in GameData/Content/Data/WoodlandCredits.txt. Build Windows Game still marks the package as a development build. Hosted checks on the intended PR head and a standalone launch on a separate clean Windows machine remain release gates.

## Deferred Physics Lab work

Workshop Van vehicle damage has been removed from the active Physics Lab change. Revisit after profiling frame time and memory in representative Renegade projects. The prototype sources, reapply patch and two proof projects are preserved in the local RenegadeArchive under WorkshopVanDamage-20261120.

Removal verification on Physics Lab HEAD 9f3fe46. The Windows x64 Release build relinked WickedEngine_common, WickedEngine_emb_shaders, RenegadeAuthorWorkshopVan, RenegadeRuntime, RenegadeVehiclePhysicsTests and RenegadePhase5Gate2CameraTests through cmake --build BUILD/clean-release --config Release with /m:1 /p:BuildProjectReferences=false. Seven focused Release CTests passed with ctest --test-dir BUILD/clean-release -C Release -R "VehiclePhysicsTests|Phase5Gate2CameraTests|ProjectileSimulationTests|S3GovernedLuaRuntimeSourceContract|StudioUiPolicy|JP01PhysicsLabSourceContract|SoftBodyPhysicsTests" --output-on-failure --timeout 30.

The default Blender export contains 38,994 triangles and no morph targets. The native author saved and reopened the stock van and its Jolt drive and brake proof passed. The stock Workshop Van and the cockpit camera verification project both reached Main Level in the rebuilt standalone DX12 Runtime. The active Studio bundle already predated the prototype. The generated Jolt source and both Runtime binaries lack the prototype binding. Build and proof logs are under BUILD/PhysicsLabSignoff/VehicleTemplateRelease/DamageRemovalVerification. The archive reapply patch passed git apply --check. No hosted CI or merge was requested.

## Imported Breakables Range

Uncommitted on the Physics Lab worktree at 9f3fe46. PhysicsGameplayService.h, RenegadeDestructibles.inc and Tools/VerifyDestructibles.cpp add an intact-visual role and the native swap fixture. The architecture, breakables guide and feature matrix record the exposure. Existing assembled barricades retain their collider and rattle rules. The intact-swap path uses convex debris shaped from the authored meshes, with a box fallback for degenerate hulls and a smaller release impulse. The first box-only DX12 trial was visibly poor in review. The corrected trial embeds preview textures and uses a matte floor.

    cmake --build BUILD/clean-release --config Release --target WickedEngine_common WickedEngine_emb_shaders RenegadeVerifyDestructibles RenegadeRuntime -- /m:1 /p:BuildProjectReferences=false
    RenegadeVerifyDestructibles.exe --prop-swaps <stage>/SourceAssets <project>/Content/Scenes/Main.wiscene
    RenegadeVerifyDestructibles.exe <stage>/baseline.wiscene
    RenegadeVerifyDestructibles.exe <stage>/ragdoll.wiscene --ragdoll

Windows Release built. The reopened scene starts with one visible intact mesh and one static body per prop. The crate, barrel and explosive barrel released 7, 7 and 5 convex bodies at 10.54, 10.22 and 11.01 m/s. After 340 native steps the lowest visible point across the debris was approximately 2 cm below the floor. Native assembled, rattle, script and ragdoll regressions passed. The separate DX12 launcher loaded the range and the captures show the textured intact props, release, and settled pieces. No missing-texture warning occurred in the new run.

The user assets were prepared locally with their original UVs, 1K preview map copies and centred fragment groups. The stage is BUILD/PhysicsLabSignoff/BreakablePropSwap. Play Breakables Range.cmd uses the matching standalone Runtime. SourceAssets, asset preparation, proof logs, and ConvexCaptures are kept in that stage. The proof uses impact weights for all three props. Breakables page authoring, a debris fade lifecycle and the explosive barrel gameplay effect remain separate work. Keep future authoring compatible with both grouped and loose imported meshes.

Final verification rebuilt RenegadePhysicsLuaTests and RenegadeSoftBodyPhysicsTests in Release and passed all eight targeted entries using ctest --test-dir BUILD/clean-release -C Release -R "VehiclePhysicsTests|Phase5Gate2CameraTests|ProjectileSimulationTests|S3GovernedLuaRuntimeSourceContract|StudioUiPolicy|JP01PhysicsLabSourceContract|SoftBodyPhysicsTests|PhysicsLuaTests" --output-on-failure --timeout 30.

## Breakable weapon hits and environmental impacts

Uncommitted at Physics Lab HEAD 9f3fe46. PhysicsGameplayService.h and the generated Jolt contact listener add persisted opt-in hit points and per-play-session damage. RuntimeApplication.cpp and RuntimeProjectileAim.h route confirmed hitscan and travelling projectile contacts into the existing release queue. PlayerViewHandAnimation.h emits one contact at 35 percent of an accepted independent-hands swing. Melee tests the first contact within 2.25 m with 12 to 20 hit points of damage based on charge. RuntimeProjectileWorld.h skips the hidden intact visual and hidden break fragments in mesh-based weapon queries. Ordinary non-breakable contact handling and the existing Lua event layout are preserved. Breakables in RenegadePhysicsLabWorkspace exposes Break Speed and Hit Points with Undo/Redo. Selecting a visible child resolves the nearest eligible rigid-body root. Tests/JP01PhysicsLabSourceContract.cmake registers the ninth page.

    cmake --build BUILD/clean-release --config Release --target WickedEngine_common WickedEngine_emb_shaders RenegadeVerifyDestructibles -- /m:1 /p:BuildProjectReferences=false
    cmake --build BUILD/clean-release --config Release --target RenegadeRuntime RenegadeStudio RenegadePhysicsLuaTests RenegadeProjectileWorldTests RenegadeRuntimeProjectileSessionTests -- /m:2 /p:BuildProjectReferences=false
    RenegadeVerifyDestructibles.exe --weapon-breaks <stage>/SourceAssets <output.wiscene>
    RenegadeVerifyDestructibles.exe --prop-swaps <stage>/SourceAssets <output.wiscene>
    RenegadeVerifyDestructibles.exe <output.wiscene>
    RenegadeVerifyDestructibles.exe <output.wiscene> --ragdoll
    ctest --test-dir BUILD/clean-release -C Release -R "^(RenegadeProjectileSimulationTests|RenegadeProjectileWorldTests|RenegadeRuntimeProjectileSessionTests|RenegadePhysicsLuaTests|RenegadeSoftBodyPhysicsTests|RenegadeS3GovernedLuaRuntimeSourceContract|StudioUiPolicy|RenegadeJP01PhysicsLabSourceContract|RenegadePhase5Gate2CameraTests|RenegadeVehiclePhysicsTests)$" --output-on-failure --timeout 30

The Windows Release builds and ten focused CTests passed. All four native fixtures exited zero after the temporary break-event ABI extension was removed. The weapon proof checked Undo/Redo and archive reopen; one contact from an accepted animated swing; melee rejection outside reach; a crate surviving its first hitscan contact and breaking after a travelling projectile; duplicate-queue rejection; a second melee strike breaking the barrel; and a falling weight breaking the explosive-barrel model. A final ray passed through the space where the intact crate had been. The local Weapon Breakables project reached its level in the rebuilt standalone DX12 Runtime. The native fixture drives the same weapon helpers used by Runtime; it does not simulate equipping and operating a specific weapon asset with keyboard input. Stage and receipts are in BUILD/PhysicsLabSignoff/BreakablePropSwap.

The final Studio build resolved a viewport click on the crate Intact Mesh to the Crate Breakable root and displayed its authored 20 Hit Points and 0 Break Speed. WeaponStudio-RootSelected.jpg records the selection. Test Game from that Studio build launched its matching DX12 Runtime against a Studio-generated snapshot and reached Weapon Breakables. The bootstrap receipt is weapon-studio-test-game.log.

## Explosive oil drum

Uncommitted on Physics Lab HEAD 9f3fe46. PhysicsGameplayService.h and RenegadeDestructibles.inc add persisted, optional Blast Speed. A successful release uses the intact collider centre and the debris mesh bounds to send fragments up and outward. The existing break event and the non-explosive release path retain their behaviour. RuntimeApplication.cpp and RuntimeProjectileVisuals.h reuse the existing short-lived blast presentation and age its effects with or without a player. RenegadePhysicsLabWorkspace.cpp exposes the setting with Undo/Redo. Tools/VerifyDestructibles.cpp authors 5.5 m/s on the oil drum and checks a single blast at the collider centre with outward fragment motion. The authored debris is the asset's five surviving pieces.

    cmake --build BUILD/clean-release --config Release --target WickedEngine_common WickedEngine_emb_shaders RenegadeRuntime RenegadeStudio RenegadeVerifyDestructibles RenegadePhysicsLuaTests RenegadeProjectileWorldTests RenegadeRuntimeProjectileSessionTests -- /m:3 /p:BuildProjectReferences=false
    ctest --test-dir BUILD/clean-release -C Release -R "^(RenegadeProjectileSimulationTests|RenegadeProjectileWorldTests|RenegadeRuntimeProjectileSessionTests|RenegadePhysicsLuaTests|RenegadeSoftBodyPhysicsTests|RenegadeS3GovernedLuaRuntimeSourceContract|StudioUiPolicy|RenegadeJP01PhysicsLabSourceContract|RenegadePhase5Gate2CameraTests|RenegadeVehiclePhysicsTests)$" --output-on-failure --timeout 30

Windows Release built successfully and all ten focused CTests passed. All four VerifyDestructibles runs passed (weapon-breaks, prop-swaps, legacy Lua, and ragdoll). Studio Test Game reached the DX12 level using a saved snapshot. The foreground standalone DX12 replay was visually confirmed from the intact drum through smoke and airborne pieces to settled debris, with the crate and wooden barrel intact. Evidence and matching launchers Play Barrel Blast.cmd and Open Barrel Blast in Studio.cmd are under BUILD/PhysicsLabSignoff/BreakablePropSwap. BarrelBlastReplay frames 036 through 045 show the effect and spread. The player's F8 binding replays the drop. Test Game was observed launching, and standalone was used for the captured result.

## Breakables authoring and debris timing

Uncommitted on Physics Lab HEAD 9f3fe46. PhysicsGameplayService.h, RenegadeDestructibles.inc and the generated Jolt wrapper store Debris Stay and Debris Fade on the breakable root. The timer runs from release. It uses native per-instance opacity, preserves debris collision during the fade and removes each fragment entity and its body at expiry. Existing data defaults to keeping debris for the session. The intact-mesh command marks one selected child and hides the other render meshes under the same static transform root. Undo restores the roles and render flags. Studio exposes the timing sliders and assignment button. Existing assembled assets continue to use their piece meshes directly.

    cmake --build BUILD/clean-release --config Release --target WickedEngine_common WickedEngine_emb_shaders RenegadeRuntime RenegadeStudio RenegadeVerifyDestructibles RenegadePhysicsLuaTests RenegadeProjectileWorldTests RenegadeRuntimeProjectileSessionTests -- /m:3 /p:BuildProjectReferences=false
    ctest --test-dir BUILD/clean-release -C Release -R "^(RenegadeProjectileSimulationTests|RenegadeProjectileWorldTests|RenegadeRuntimeProjectileSessionTests|RenegadePhysicsLuaTests|RenegadeSoftBodyPhysicsTests|RenegadeS3GovernedLuaRuntimeSourceContract|StudioUiPolicy|RenegadeJP01PhysicsLabSourceContract|RenegadePhase5Gate2CameraTests|RenegadeVehiclePhysicsTests)$" --output-on-failure --timeout 30

The corrected Windows Release build and all ten focused CTests passed. All four native fixtures passed. The imported fixture used the intact-mesh command, verified Undo/Redo and reopened from WISCENE. The oil drum retained its five live bodies while fading, removed all five at expiry, and left the untimed crate debris present. The staged review sets the oil drum to 5 seconds of stay and 1.5 seconds of fade. Open Debris Review in Studio.cmd and Play Debris Review.cmd are in BUILD/PhysicsLabSignoff/BreakablePropSwap. The build and proof logs use the debris-controls prefix.
The Studio panel showed all five numeric fields and the intact assignment action. Clicking the button on a different fragment transferred the visible role. Undo restored the oil drum shell and its 5 and 1.5 second values. DebrisStudioPanel.jpg, DebrisStudioAssignMesh.jpg and DebrisStudioFinal.jpg capture the sequence. The focused standalone DX12 replay showed the intact drum, blast, settled debris, partial fade and cleared floor with the crate and wooden barrel intact. Capture frames are under DebrisReviewFocused. The capture used Wicked's alwaysactive argument to keep advancing across desktop focus changes.

## Explode on Break in the asset inspector

Uncommitted at Physics Lab HEAD 9f3fe46. DestructibleBlastPreset.h and RenegadeDestructibles.inc share the release path's fragment bounds and constraint checks with a preset command. S1BInspectorSectionMigration.cpp shows Explode on Break for eligible static bodies and resolves the authored root when selecting a child mesh. The toggle writes the existing blast speed field (5.5 m/s on, zero off), preserving the break input and debris timing. Undo restores the exact previous speed. A previously authored blast can be removed after geometry edits. Runtime still owns visual presentation and only emits it after successful release.

Windows x64 Release built the existing generated WickedEngine_common, WickedEngine_emb_shaders, RenegadeVerifyDestructibles, RenegadeStudio, RenegadeRuntime and RenegadePhysicsLuaTests projects with MSBuild /p:Configuration=Release /p:Platform=x64 /p:VisualStudioVersion=18.0 /p:BuildProjectReferences=false /m:3. See explode-inspector-build-final.log. The four native fixtures passed, including ordinary-collider rejection with a break speed set, missing-trigger rejection, compound fragment eligibility, a custom blast speed restored by Undo, and WISCENE reopen for physical and HP triggers. The same ten focused Release CTests listed for debris timing passed.

In Studio, the oil drum checkbox toggled off and on while its break speed stayed at 6 m/s. The ordinary floor checkbox was disabled and clicking it left it unchecked. Both oil drum edits were undone after verification. The matching standalone DX12 replay showed the oil drum blast, scattered debris and timed fade. Captures and logs use the ExplodeInspector or explode-inspector prefix under BUILD/PhysicsLabSignoff/BreakablePropSwap. Launch the review with Open Explode Preset in Studio.cmd.

## Physics Lab authoring walkthrough before the remaining tools

The review branch integrated main 9eae100 at bc82418. The merge retains the cockpit camera update and main's terrain prop residency update in the same Runtime frame. Both original vehicle and terrain evidence entries are preserved. The pre-merge Physics Lab checkpoint is 9e779d3. The Wicked and imnodes gitlinks stayed at their pinned commits.

Tools/VerifyDestructibles.cpp now offers --studio-authoring-fixture <output.wiscene> and --verify-studio-authoring <saved.wiscene>. The first writes raw door, hinge marker, sensor volume, cloth mesh and barricade geometry with a floor, player start and test weights. The second exercises the actual saved Studio authoring through native Jolt. It runs from the Runtime working directory. No geometry package is required.

On DESIGN the preceding verified Physics Lab Studio build was used to add and bind the door's hinge, set an ordinary box as a sensor, add the curtain soft body and pin its top row, and set the static barricade's break speed. Undo/Redo was exercised while configuring the four setups. The scene saved and reopened through Studio. Native readback measured 0.482 m lateral door swing, sensor enter and exit with the weight settling at Y 0.17 m, zero pinned-node displacement with 0.925 m combined free-node Y/Z displacement, and four barricade pieces released. Studio Test Game reached the DX12 scene. The player pushed the hinged door open, and the sensor weight, hanging cloth and broken boards were visible. The short first process probe expired during Test Game preparation. The actual Runtime then started and its updated PASS bootstrap receipt was verified.

Evidence is under BUILD/PhysicsLabSignoff/V1Candidate and uses the Bench or authoring-bench prefix. One UI issue surfaced. In a narrow Constraint dock the second body picker's lower entries clip until the dock is scrolled upward. The walkthrough succeeded with that adjustment. The exact merged candidate still needs its own build and regression pass before publication. The owner identified Physics diagnostics and Step Once, compound collider authoring, and viewport constraint editing as remaining feature work.
