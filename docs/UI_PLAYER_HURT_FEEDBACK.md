# Player hurt feedback

In UI Designer, open Elements > BASIC > Player hurt feedback (also Gameplay > COMBAT). Select its ordinary Image layer and choose Hurt feedback settings. It is a gameplay HUD component. It stays transparent when the player is healthy and no hit is active.

Each channel has its own switch: brief hit flash, smooth edge vignette, incoming-direction icons, low-health overlay, pain sound, low-health heartbeat and optional camera shake. Shake and heartbeat are off on insertion. No default pain/heartbeat audio is bundled: empty clips are silent. Choose your own WAV/OGG using the sound pickers. The editor's audition button plays the chosen clips only in the private sample.

Use Front / Left / Right / Behind / Zone to try a hit without starting a game. Low health sample previews 15% health without changing the saved HUD or player. The sample shares Runtime rendering. Camera shake is visible in gameplay; the neutral editor sample does not contain a 3D camera.

## Custom appearance and editing

Choose separate hit, edge, direction and low-health artwork. Alpha PNG is useful for overlays. Hit/direction and low-health have independent tint pickers; white preserves the artwork's original colours. Use generic visuals clears the four artwork overrides. Clear sounds clears both clips.

Opacity, hold/fade times, icon size/radius, threshold, pulse rate, volume/cooldown and shake strength/duration are editable. Overall intensity scales all channels. The component is a normal movable/resizable Image layer with ordinary opacity. Its geometry maps proportionally across the actual gameplay viewport: a full-design rectangle reaches every screen edge regardless of the HUD Fit/Fill/Stretch setting or window aspect ratio. Duplicate it and enable different channels if they need independent layout or tint. It does not intercept gameplay clicks.

Save applies one Screen history operation; Cancel discards the settings draft. Imported resources are copied into project Content. Screen save/reopen and portable package publishing/adoption retain/remap all six image/audio dependencies. Adding the prefab to a legacy Screen upgrades its optional schema atomically with insertion.

## Damage and lifecycle

The native accepted-damage path supplies actual clamped damage. Successful NPC attacks provide their world source bearing; blocked/rejected attacks do not trigger feedback. Zone/source-less damage never invents a direction. Indicators rotate against the current camera heading.

Healing does not produce a hit. Load, scene/player replacement and respawn reset transient state. Dead/unavailable players have no active hurt effects. Pause freezes gameplay decay and stops hurt audio; hidden HUD/map presentation stops audio and shake. Reduced motion suppresses full-screen flash, pulsing and camera shake, retaining steady edge/low-health feedback and directions.

Shake is presentation-only: it is added after the final player/vehicle camera and gameplay aiming, before native visibility/render constants. The next gameplay update restores the original camera pose. It does not move the player or change a projectile's aim.

## Governed Lua

The same settings are available through C++ ScreenHurtFeedback / HurtFeedbackService and renegade.hurt (contract_version 1). Authored view IDs appear in views(). Runtime changes are transient; save persistent choices in the Screen editor.

```lua
return {
  on_start = function(self)
    for _, id in ipairs(renegade.hurt.views()) do
      assert(renegade.hurt.configure(id, {
        flash_opacity = 0.06,
        edge_opacity = 0.4,
        low_threshold = 0.25,
        camera_shake = false,
        pain_sound = "" -- silent until a project clip is chosen
      }))
    end
  end
}
```

| Function | Behaviour |
| --- | --- |
| create(settings) | Creates a transient view; returns its ID or nil/error. |
| configure(id, settings) | Atomic partial update; returns false/error for invalid values. |
| get(id), views() | Current settings / registered IDs. |
| remove(id) | Removes a created view or disables an authored view for the session. |
| state() | available, alive, health_fraction, hit_sequence, seconds, last_hit_age. |
| hit(amount, optionalWorldBearing) | Explicit presentation pulse; **does not damage the player**. Bearing is degrees around world Y, zero along +Z. |
| clear() | Clears transient hits without changing health. |

Boolean keys: enabled, hit_flash, edge_overlay, directional, low_health, pain_enabled, heartbeat_enabled, camera_shake.

Numeric keys: flash_opacity, edge_opacity, hold, fade, direction_duration, direction_size, direction_radius, low_threshold, low_opacity, pulse_hz, pain_volume, pain_cooldown, heartbeat_volume, heartbeat_period, shake_strength, shake_duration, intensity. Geometry keys: x, y, width, height, opacity. Colours: colour and low_colour as eight RGBA hex digits. Resource keys: hit_image, edge_image, direction_image, low_image, pain_sound, heartbeat_sound; empty clears an override, otherwise an existing project Content path is required.

Limits: 16 registered views, eight concurrent hits, six-second retained hit history, pulse maximum 3Hz, shake maximum 0.15 metres / two seconds. No damage-number text, hit-stop, gamepad rumble or default human pain clips were added.

## Scoped verification

See evidence/player-hurt-2026-11-28 for local Windows x64 Release/DX12 proof. Automated bridge/Runtime/Lua/persistence/package tests and native Studio/Runtime checks are recorded separately. Debug, exported distribution, larger scenes and independent release verification remain separate gates.

The disposable Hurt Feedback Review project uses E for a sample hit with health unchanged. Normal gameplay uses accepted combat/zone damage automatically. H is an existing opt-in QA Runtime switch and is not a public game control; R has no special role.
