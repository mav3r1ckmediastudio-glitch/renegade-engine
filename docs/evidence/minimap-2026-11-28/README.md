# DESIGN scoped minimap verification, 28 November 2026

Local Windows x64 Release on RTX 4070 Ti / DX12. Runtime and Studio built with the pinned Wicked checkout; vendor source/gitlink was not changed by this task. Source is on feature/ui-loading-media, based on accepted local ammo checkpoint a365a9c. No hosted CI or push.

## Automated checks

RenegadeMinimapTests, RenegadeGameplayInputTests, RenegadeWorldNavigationTests, RenegadeWorldLocationTests, RenegadeScreenRendererTests, RenegadeScreenHealthPrefabTests and RenegadeScreenAmmoPrefabTests were rebuilt/relinked and passed. Minimap tests cover projection/rotation/aspect, distance from the player after full-map panning, nearby filters, entity-following resolution, invalid settings/resources, Screen persistence/history, portable dependencies, native camera ownership/north alignment, old-input migration and actual governed Lua execution.

MarkerIcons, Phase5Gate3, Phase6Gate3, CharacterWorkflow and EditorPointLookup source contracts plus StudioUiPolicy passed (6/6). AssistantWorkflowSuite is recorded in assistant-workflow.txt after the final documentation/hash refresh.

## Native checks

An isolated generated review project contains a Player Start, floor, three geometric buildings, one passive Enemy Character and town/questgiver/vendor Location Zones. Artwork is a generated coordinate reference; it is not offered as production art.

- live.png: overhead building placement agrees with world positions; default nearby enemy/questgiver/vendor/location markers and player heading are visible.
- world-map.png / panned.png: M artwork, wheel zoom and drag pan retain registration of artwork, locations and player. W while open produced identical before/after map pixels. Close, M and Esc returned to gameplay; W then moved the player and shifted live map geometry/markers.
- custom-icons.png / custom-full-map.png: fresh Runtime uses the Studio-saved artwork, independent full map, imported player/enemy icons and imported TrueType font. Placeholder cyan icons were deliberately selected to make replacement visible.
- ui-history.json: real modal Save changes the saved HUD; one toolbar Undo restores the original exact bytes and Redo restores the saved exact bytes, measured after the normal idle autosave.
- palette.json: Basic Minimap creates a second ordinary layer; one Undo restores the exact document. Imported font persists.
- editor.jpg / editor-live.jpg: final native settings window with shared Renegade outline controls and editor samples.

An initial native attempt found a missing shader root signature and exposed a stale local executable link. Both were corrected, consumers explicitly relinked and fresh Runtime captures repeated. Fixture-only startup and NPC capability setup errors were also corrected. These failed attempts are not acceptance evidence.

Normal Studio is installed and left open for owner testing. Default live mode remains the stock insertion behaviour. Documentation records live camera/marker bounds and unsupported features. Debug, exported standalone package, large-scene performance, Vulkan and independent release verification remain pending.
