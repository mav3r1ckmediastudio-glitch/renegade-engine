# Light picker and saved behaviours

## Owner-approved scope

One native colour/light picker contains Point, Spot, Directional and Area light
types; colour wheel and numeric colour entry; project saved colours; brightness,
range and shadows; six square icon buttons (Flame, Electrical Fault, Constant,
Rotating, Pulsing and Strobe); behaviour settings; saved project light presets;
Update Preset and Save As New. Keep placing defaults on. Right mouse finishes
placement and the entire held gesture is consumed before fly-camera input.
The visible hint is "RMB to finish".

Add-menu light entries all open this picker. An existing light exposes
COLOUR / LIGHT PICKER in its Inspector. Editing its draft is isolated; Apply to
Light changes that instance. Saving/updating a preset never mutates placed lights.
Stock starting presets are labelled built-in and become project files only
when explicitly saved. Their shadows default off, as with native new lights.

## Creator workflow

1. Choose Add and any light entry to open Light Picker.
2. Choose a light type, saved preset, or labelled built-in starting preset.
3. Set colour, brightness, range and shadows. Optional Shape / volumetrics
   reveals cone angles, radius, area dimensions and volumetric controls.
4. Choose a behaviour icon. Its strength, speed, movement and random timing
   controls apply to that behaviour; irrelevant controls are disabled.
5. Save As New creates a project preset. Update Preset explicitly replaces the
   selected project preset. Save Colour stores a colour swatch.
6. Place Light arms surface placement. Each left click creates an independent
   configured light through one Undo action. RMB ends the placement gesture.
7. Select an existing light and reopen its picker to edit/save that setup.
8. Save the level normally to retain the scene and scripting companion.

Rotating is most visible on Spot/Area lights; rotating an ideal point source
does not change its illumination. Flame movement affects shadows only when the
light casts them. Brightness is native intensity (not watts); radius, length and
height preserve Wicked's corresponding shape semantics.

## Persistence and script authority

EngineBridge/LightPresetService owns UI-independent preset data, project
ownership checks, sanitisation and explicit-operation enumeration. Versioned
.rlight files live under Content/Lights, named by UUID. Saved colours use
Content/Lights/SavedColours.rcolours. Writes use the project transaction boundary
and containment policy.

Buttons configure Content/Scripts/Renegade/LightBehaviour_v1.lua as a normal
governed SCRIPT attachment in the scene's .rscripts companion. One stock
behaviour owns a light at a time. Constant removes that stock attachment,
preserving unrelated scripts. The metadata enum remains editable through normal
SCRIPT properties. Project source copies are authoritative and never silently
overwritten by picker adoption.

LightSetupCommand pairs native light creation/editing and scripting changes in
one Scene history entry. MakeDeleteLightSetupCommand captures corresponding
attachments so deleting/Undo/Redo cannot lose behaviour when orphan pruning runs.
Native duplicate companion handling assigns new entity and script instance IDs;
per-entity random timing therefore stays independent. Duplicate names use
numbered suffixes and strip legacy repeated Copy suffixes.

The governed renegade.light API provides get_state/set_state using generation-
checked opaque EntityRefs. It exposes only animatable intensity/colour and local
position/rotation, rejecting nonfinite values and zero quaternions before
mutation. Range, shadows and unexposed flags are preserved. Scripts restore
authored values on stop/reset. Runtime Pause freezes time.

The 256x220 live preview owns a separate native Scene, camera, meshes, light and
governed Lua instance. It uses the exact stock source and behaviour parameters,
never temporary mutations of the open level. It updates/renders only while the
picker is visible. Preset lists rebuild only on Open/Save/Search, not per frame.
No upstream or WISCENE-format change is required.

## Validation

RenegadeLightPickerTests exercises project preset/colour cold reads, explicit
update, cross-project rejection, script metadata, creation/Undo/Redo, scripting
and native scene round-trips, behaviour replacement/removal, deletion history,
every actual Lua mode, pause/stop restoration, stale references and invalid
numeric writes. Existing light and governed Lua tests cover preserved native
authoring/runtime boundaries. Native Studio visual/input/save/reopen and Test
Game checks are recorded in HANDOFF.md; compile alone is not acceptance.

No merge, CI submission or alpha distribution is part of this local owner task.

The picker owns the modal input/frame boundary: the populated Level retains its
last rendered frame while the private preview updates at 30 Hz. Static previews
settle then cache. Main-scene 3D rendering resumes when the picker closes. Marker
selection and specialist workspace routing yield to placement; the finishing RMB
gesture is consumed until released, preventing accidental fly-camera movement.
