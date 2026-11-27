# Location Zones and location-name announcements

Implementation checkpoint: 27 November 2026. Local automated contracts pass; native authoring and standalone visual acceptance are still pending. No hosted CI or independent acceptance gate is claimed.

## Author a location

In the Level editor choose **Add > Location Zone**, then click its placement. The new zone is selected and its Inspector opens. Give it a name, a type such as location, quest, dungeon, POI or event (custom text is accepted), and optionally choose an image. The chosen image belongs to this location independently of its type.

The placement supplies the compass anchor automatically. The native zone remains an invisible gameplay box, with the existing move/resize/history controls. Width/height/depth define entry and discovery, independently of the announcement's horizontal range.

Enable **Show location name nearby** and set:
- **Announce range (m)**: horizontal XZ distance from the location anchor.
- **Total display time (s)**: the entire announcement, including its fades.
- **Text animation**: None, Fade in / out, Glitch or Wobble.

None shows static text for the selected duration. Fade changes opacity. Glitch adds brief flicker and small displacement; it does not slice or recolour individual glyphs. Wobble moves the line vertically. Advanced exposes fade durations, effect strength and speed. Fades are capped to half the total duration so short announcements can still reach full opacity. Reduced motion keeps the opacity envelope and suppresses glitch/wobble movement.

By default a name appears once per Level visit. **Repeat announcement on return** requires leaving beyond the range plus a small margin (at least two metres) before another entry can trigger. Overlapping entries are ordered by nearest distance, then stable ID, and queued serially (up to eight waiting) so names do not overlap. The registry accepts up to 512 locations. Deleting an active location retires its text; leaving the Level clears location state.

Advanced also provides **Show on compass**, **Show before discovery / Show after entering zone**, and the project-owned Lua source. Discovery happens on physical zone entry, rather than at the outer announcement radius. Discovery currently lasts for this Level visit only; persistent save-game discovery is not implemented.

## Style the on-screen text

The UI Designer's Basic assets include **Location name**. This inserts an ordinary editable Text layer carrying the location-announcement role. Adjust its font, colour, size, alignment and placement with the usual Text controls; save it with the gameplay HUD. Its sample text in the editor is replaced with the active location name during gameplay.

If the active HUD has no such layer, the shared screen renderer supplies a centred white Text layer with a dark shadow. Levels without an authored HUD also receive that same default through the shared renderer.

The compass editor starts in Basic mode, showing presentation and range settings. Location Zones supply its world targets. Manual targets, source headings, filters and specialist controls remain in Advanced. Existing authored targets are retained.

## Scripting

C++ uses WorldLocationService and the same LocationProfile / LocationAnnouncementSettings as native zones. Native zone creation and settings use zones::Create and zones::Edit; their metadata persists in WISCENE. The renderer samples LocationAnnouncementFrame through SampleLocationText.

Governed Lua exposes renegade.locations (contract version 1). Use a native Location Zone entity's stable ID to get/configure its registered location. Native locations are registered before on_start callbacks. Later transform updates follow the native anchor while preserving Lua overrides; a new authored profile/name reapplies authored settings.

~~~lua
local locations = renegade.locations
local id

return {
    on_start = function(self)
        id = assert(locations.register {
            name = "Old Mine",
            type = "dungeon",
            position = {x = 10, y = 0, z = 30},
            icon = "Content/UI/Icons/mine.png",
            range = 40,
            duration = 5,
            animation = "fade",
            fade_in = 0.4,
            fade_out = 0.6,
            strength = 6,
            speed = 2,
            announcement = true,
            repeat_on_return = false,
            show_on_compass = true,
            after_discovery = false
        })
    end
}
~~~

The example registers a logical location point, not a new physical zone box. A native Location Zone supplies the physical box and automatic entry/discovery handling.

Operations:
- register(settings) returns the ID, or nil and an error.
- configure(id, patch) changes the same settings, returning true or false and an error.
- get(id) returns the settings, including read-only ID.
- state(id) returns near, discovered and announced flags.
- discover(id) explicitly reveals the location for the current Level visit.
- announce(id) explicitly queues its current name, even with automatic announcement disabled.
- hide_announcement() clears the active name and pending queue.
- remove(id) retires a logical point. A placed zone owns its native point and restores it at the next sync; disable its announcement/compass flags to hide it.

Use animation strings none, fade, glitch or wobble. Invalid settings, non-finite numbers and escaping/missing icon paths are rejected. Native icon import copies the image into project-owned Content. The scene dependency walker reports location icons as Texture resources.

The stock Location Zone script forwards entry/exit as location.entered / location.exited; discovery emits location.discovered with sender stable ID and location name payload. Lua-created points can use discover explicitly because they have no physical box.

## Scope and remaining acceptance

Gameplay minimap rendering, persistent discovery across save-game reloads and cross-project scene-prefab icon remapping are not claimed. Screen Text-role prefab serialization is covered by automated tests. Native visual checks, standalone playback and resolution checks remain required before owner acceptance.

Existing gameplay zone behaviour IDs 0-12 are preserved; Location is appended as 13. The existing thirteen presets are integrated from the gameplay-zones foundation.
