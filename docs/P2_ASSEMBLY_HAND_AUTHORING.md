# First-person hand assembly authoring

Select Player Start, open ASSEMBLY, then HAND / MELEE SETUP. Existing saved
assemblies reopen their retained parts, grips, hand roots and animation bindings.
The main window retains paired firearm authoring and primary grip/view transforms.

## Replace the sword

Import a static model with Asset role Weapon into any Content folder (the legacy
Content/Player/Weapons folder is also recognised). Choose it as Weapon product
and LOAD PARTS. Keeping the same arms preserves shield, hand roots, directional
bindings and timing. Adjust primary parent, position and rotation in the main
window; adjust Sword scale in Attachments. Preview several strikes.
SAVE AS NEW preserves the original assembly; SAVE CHANGES replaces its product.
SAVE CHANGES retains the identity used by existing equipment presentations.
SAVE AS NEW creates a new identity; update or create equipment presentations to
reference that assembly before playing it.
A different arms skeleton resets bindings because bone/clip indices cannot be
assumed compatible. No automatic retargeting is claimed.

## Independent hands

Choose an off-hand static mesh in Attachments. Select each hand's attachment bone
and primary/off-hand layer roots explicitly. Layer roots must define disjoint
arms; attachment bones must belong to their respective subtrees.
Shield position, rotation and scale are independent. Weapon meshes must be static;
the arms provide the animation. Paired firearms still require both native tracks.

Directional strikes maps Left, Right, Down and Stab Charge/Hold/Release: twelve
slots, all assigned or all NONE. Block / attack variants maps shield Raise/Hold/
Lower and up to four fallback Attack variants. Assign Idle in the main window.
Each independent source clip has exactly one role. Missing, duplicated or invalid
bindings disable preview/save and report the validation error.

The preview action dropdown includes every authored attack variant, directional
stage and shield stage. Preview with shield held keeps the independent off-hand
in its hold pose while the selected primary action is scrubbed or played.
Preview composition uses the same masked native clips and bounded avoidance as
Runtime. It does not run equipment phases or damage.

## Timing and collision

Full charge is 0.1-10 seconds; chain window is 0-2 seconds; released queue lifetime
is 0.05-5 seconds. Defaults remain 1, 0.30 and 0.75 seconds. Queue lifetime starts
on release and uses gameplay time. Equipment preparation/windup/recovery are
authored separately in EQUIPMENT; these assembly timings do not bypass them.

Enable Avoid sword / shield clipping and define a blade capsule (base, tip,
radius) and shield box (center, half-extents). Coordinates are in each weapon
attachment's local space before mesh scale. Maximum correction bounds the
presentation arm's displacement; it is not a world collision or damage shape.
The existing solver can fall back to authored pose at unreachable contacts.
Inspect several stages with the shield held after changing mesh or grip.

All assembly changes use draft Undo/Redo, automatic preview refresh and the
existing journaled asset save. Save waits for a current preview and checks hashes
of all retained parts, including the off-hand. Save the level after assigning a
new assembly to Player Start. Snapshot/package dependency collection remains
governed by retained part identities. Older schema-v1/v2 recipes retain defaults.

## Scope

No damage, stamina, parry or NPC collision is added. Unbound movement/aim actions
are not newly routed by this authoring work. A two-handed weapon needs suitable
animations and a different ownership configuration. Broader panel UX remains
an owner review item; independent exact-commit verification remains required.
