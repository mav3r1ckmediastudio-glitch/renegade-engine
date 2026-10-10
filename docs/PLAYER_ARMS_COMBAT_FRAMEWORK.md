# Player Arms & Combat Framework

**Status:** Canonical design authority for the Alpha Playability programme.
**Scope:** Player first-person presentation, equipment/actions, ranged combat, directional melee, bows, crossbows, magic, projectiles, impact feedback and combat feel.
**Underlying rule:** extend the accepted Renegade Player; do not create a second player controller, physics world, animation runtime or damage/event system.

## Implementation checkpoint - 2026-10-05

PR #178 merged into main on 2026-10-05 at `7105a95ddcc103d6a024a17fddefd62f705a86b3`.
All four pre-merge Windows checks passed: Studio Debug/Release and baseline
Debug/Release. The owner confirmed the expected player/shotgun behaviour in the
existing v2 game project and explicitly accepted the PR's functionality.
This records functional acceptance of the merged scope, not completion of every
P1 requirement or of the full Alpha Playability combat programme.

The reusable shotgun/player baseline is integrated. Generic equipment, reserve
ammunition, world hits/projectiles, damage, recoil, inventory and the remaining
combat families still require their planned gates. Player authoring UI/UX must be
revisited; see [PLAYER_AUTHORING_UX_FOLLOWUP](PLAYER_AUTHORING_UX_FOLLOWUP.md).

## Product goal

Renegade needs a combat framework broad enough for a conventional FPS and for
Heathen-style combat without implementing each weapon family as a separate player.
The same architecture must support firearms, swords, axes, maces, spears, shields,
bows, crossbows, throwable items, tools and spellcasting.

The core design rule is:

> The hands perform actions; equipped items and abilities define what those actions mean.

Combat mechanics and first-person presentation are separate. The world Player
owns movement, collision and authoritative position. The View Rig owns what the
player sees. Items/abilities own action rules. Projectiles/damage own simulation.
SFX, particles, camera motion, recoil and animation present those actions.

## Ownership model

~~~text
Player
├── World Controller
│   ├── Wicked/Jolt character capsule
│   ├── movement / sprint / jump
│   ├── health / damage receiver
│   └── interaction
├── Camera
│   └── First Person View Rig
│       ├── Primary Hand
│       ├── Off Hand
│       ├── Two-Hand Support
│       └── procedural presentation layers
├── Combat Controller
│   ├── aim / target solution
│   ├── action state
│   ├── hand reservation
│   └── damage / event routing
└── Equipment / Abilities
    ├── Item or Ability Definition
    ├── Action Definitions
    ├── Projectile Definition
    ├── Impact/Surface Profile
    └── Animation / Audio / VFX bindings
~~~

The View Rig is presentation-only. It must never become a second movement or
collision controller. World-space hit, projectile and damage rules remain
authoritative outside the first-person rendering layer.

## Hand model

Both hands are first-class from the first implementation. An item declares a
hand-usage policy such as PrimaryOnly, OffHandOnly, EitherHand, TwoHanded or
PrimaryWithSupport.

This must permit pistol + empty hand, pistol + knife, pistol + flashlight,
sword + shield, sword + spell, dual weapons, rifle, bow, crossbow and staff +
off-hand spell without special-casing the Player controller.

Two-handed actions reserve both hands for the relevant action phase. Supporting
hands may provide grip/IK/presentation without becoming a second gameplay owner.

## Generic action model

Items and abilities expose semantic actions rather than hard-coded clip names.
The initial common vocabulary includes Equip, Unequip, Idle, Walk, Sprint, Aim,
PrimaryAction, SecondaryAction, Reload, Charge, Release, Melee, Block, Parry,
Cast, Use and Inspect.

Actions may be instantaneous or multi-stage. A generic staged action can use:
Prepare -> Wind-up -> Hold/Charge -> Active/Release -> Recovery -> Ready.
Cancellation, interruption and transition rules belong to the action definition.

## Animation and first-person presentation

The Arms Rig requests semantic actions and uses native Wicked animation.
Gameplay code must not request asset-specific filenames.

Authored animation is layered with procedural presentation where appropriate:

~~~text
authored pose
+ movement sway / bob
+ breathing
+ aim offset
+ recoil / weapon impulse
+ impact deflection
+ camera-relative positioning
~~~

Existing Renegade crossfade principles apply. Hard cuts are not the normal
transition model. Future layers/additive animation may extend this design, but
must not introduce a second skeleton evaluator.

First-person rendering must minimise wall clipping and FOV distortion. The exact
rendering technique is an implementation decision, but the world simulation must
remain independent from any first-person-only depth/FOV treatment.

## Firearms

Firearms must support semi, burst and automatic modes; magazine/chamber/reserve
state; reload; dry fire; ADS; spread; recoil/recovery; muzzle position; shell
ejection; hitscan and physical projectile modes; and surface-aware impacts.

Aiming should normally be camera-intent first, with a muzzle/world-path check so
the player cannot fire through cover merely because the camera can see around it.
Physical projectiles originate at the weapon muzzle and travel toward the solved
aim point.

Weapon feel is a composed event, not a raycast:

~~~text
input
-> arm/weapon impulse
-> muzzle flash / light / smoke
-> camera recoil
-> shot and mechanical audio
-> projectile / hit
-> surface impact VFX/SFX/decal
-> target reaction
-> recovery
~~~

Camera recoil and weapon recoil are separate tunable channels. Procedural recoil
should allow weapons to differ without requiring a unique animation for every
shot.

## Projectile framework

Projectiles are reusable gameplay entities/records with origin, direction,
velocity, gravity policy, lifetime, collision policy, owner/source identity,
damage payload and impact profile.

The framework must support bullets, arrows, bolts, thrown objects, fireballs,
ice shards and other physical or simulated projectiles. It must allow later
extensions such as penetration, ricochet, recoverable arrows and elemental
payloads without changing the Player controller.

## Directional melee

Directional melee is a first-class combat mode, not a single Melee action.

Initial attack directions:
- left-to-right slash;
- right-to-left slash;
- overhead;
- thrust;
- optional diagonal variants where the weapon profile supports them.

Initial defensive directions:
- guard left;
- guard right;
- guard high;
- centre/thrust guard.

Input resolves intended direction from mouse/controller movement and stance.
The equipped melee profile decides which attacks, guards and transitions exist.

A melee action uses explicit phases:
Ready -> Wind-up -> optional Hold/Feint -> Active Strike -> Contact/Miss ->
Recovery. Feint windows, cancel rules, parry windows, stamina cost and recovery
are data-driven rather than hidden in the Player.

Collision follows the weapon path. During the active window Renegade tracks
relevant blade/head/point sockets between frames and performs swept collision.
A sword tip, axe head, spear point or shield face can therefore have meaningful
reach and contact behaviour rather than using a centre-camera raycast.

Melee contact can evaluate weapon region, direction, relative velocity, target
surface/material and defensive state. This creates the seam for armour, weapon
mass, hit reactions and later penetration/deflection rules.

Directional blocking must be more than isBlocking. Incoming attack direction,
guard orientation and tolerance determine whether a weapon parries/blocks.
Shields are area-based and intentionally more forgiving. A blocked attack can
deflect/rebound and transition into recovery rather than playing through as if
nothing happened.

The framework must leave room for:
- feints;
- parries;
- chambers/counters;
- attack chains;
- shield bash;
- two-handed grips;
- stamina hooks;
- weapon-specific reach/mass;
- weapon/surface-specific impact responses.

Sword + shield is the reference directional-melee proof because it exercises
independent hands, attacks, defence, collision, impact feedback and animation.

## Surface and impact response

Impact presentation is selected from the actual contact surface/profile.
Flesh, armour, wood, stone, metal and other materials can produce different
audio, particles, decals, damage modifiers and reactions.

A sword striking plate should not present like a sword striking flesh; an arrow
hitting timber should not present like a bullet hitting stone.

## Bows

Bows are two-handed charge/release weapons. Draw amount is a real gameplay value,
normally 0..1, and may influence projectile velocity, trajectory, damage,
animation pose, bow deformation, tension audio and aim stability.

The expected loop is Raise -> Nock -> Draw -> Hold/Aim -> Release -> Recovery.
Arrows use the shared projectile framework and should be capable of embedding,
bouncing or breaking according to surface/projectile policy. The architecture
must permit future recoverable, fire, poison, explosive or utility arrows.

## Crossbows

Crossbows use a staged state machine rather than pretending to be bows with
different numbers. A reference flow is Fired/Unloaded -> Cock -> Load Bolt ->
Ready -> Aim -> Fire.

Different crossbows may provide different cocking/reload mechanisms. The generic
action framework must therefore support multi-stage reloads useful to firearms
as well as crossbows.

## Magic

Spells use the same hand/action/effect framework without being disguised weapons.
Supported cast families should include projectile, beam, continuous/channel,
area, targeted and self-cast abilities.

A spell can define hand usage, mana/resource cost, charge time, cast/release
actions, projectile/beam/area behaviour, animation, audio, particles, lights and
camera/hand impulses.

Examples that must remain architecturally possible include sword + spell, shield
+ spell, dual spells, staff + off-hand spell and weapon enchantment.

Chargeable magic follows the same generic staged-action principles as bows:
Begin -> Charge/Hold -> Release/Channel -> Recovery. Charge state may drive
particle/light intensity and pose without contaminating gameplay ownership.

## SFX, VFX and action effect stacks

Each action can bind a presentation/effect stack. A firearm Fire action might
bind animation, shot/mechanical sounds, muzzle particle/light, camera impulse,
weapon impulse, projectile and casing eject. A bow Release binds string audio,
animation, light camera impulse and an arrow projectile. A spell Cast can bind
hand VFX/light, audio, camera impulse and a spell projectile.

Audio, particles, lights and decals are not cosmetic afterthoughts: they are part
of combat acceptance and must have deterministic gameplay trigger points while
remaining presentation-side effects.

## Damage and Character integration

Player attacks and NPC attacks use the same governed damage/event boundaries.
The Player gains a real health/death state rather than special-case immunity.
NPC attacks can therefore kill the Player; Player attacks can damage and kill
Characters; HUD and reactions consume the same state.

The first complete combat loop is:
spawn -> equip -> encounter -> attack/defend -> health changes -> one actor dies
-> death state -> restart/respawn.

## Authoring model

The creator-facing product should expose reusable definitions rather than forcing
combat tuning into code. Expected data products include Item/Ability definitions,
Action definitions, Projectile profiles and Impact/Surface profiles. Exact file
formats and service names are implementation details to be gated separately.

The editor should expose semantic animation/audio/VFX assignment, hand usage,
action phases/timings, projectile behaviour, damage/range/ammo/resource values
and melee directional sets without requiring normal creators to write Lua.

Lua may invoke high-level governed combat actions/events later, but ordinary
weapon execution must not depend on user scripts.

## Alpha Playability gates

- **P1 — First-person Arms Rig:** camera-mounted rig, primary/off-hand sockets,
  two-hand support, movement presentation and Test Level/Build Game parity.
- **P2 — Equipment & Action Framework:** equip/unequip/use/alternate/charge/
  release/reload, hand reservation and semantic animation ownership.
- **P3 — Projectile & Impact Framework:** reusable physical projectiles, hitscan
  seam, collision, damage routing, surface SFX/VFX/decals and target reactions.
- **P4 — Firearm reference:** pistol with ADS, ammo/reload, recoil, muzzle effects,
  audio, hits and Character damage.
- **P5 — Directional Melee:** directional attacks/guards, swept weapon collision,
  sword + shield, block/parry/feint seams, impact deflection and stamina hooks.
- **P6 — Bow reference:** draw amount, nock/draw/release, physical arrow,
  gravity/velocity variation and embedding/bounce policy.
- **P7 — Crossbow reference:** staged cock/load/fire lifecycle and bolt projectile.
- **P8 — Magic reference:** at least one charged projectile spell and one
  continuous/channel spell using the same hand/effect framework.
- **P9 — Dual-hand proof:** deliberately mix independent hand systems without
  Player special cases, such as sword + shield and weapon/spell combinations.
- **P10 — Combat feel & playable loop:** owner-led tuning of responsiveness,
  recoil, sway, animation timing, impact weight, particles, SFX, camera motion,
  melee timing, parries and defence until combat feels good; validate death,
  HUD, restart and independently packaged Runtime parity.

## Acceptance policy

Automated tests prove state transitions, ownership, projectile/damage behaviour,
persistence and regressions. They cannot prove satisfying combat feel.

P10 therefore requires a dedicated owner gameplay session. A technically correct
weapon is not accepted merely because it applies the expected damage. Firearms,
directional melee, bows, crossbows and magic each need direct play testing.

No gate may bypass the accepted Player controller, Character/damage seams,
Wicked physics/navigation/animation authority, governed event boundaries or
Build Game/Test Level lifecycle.

After the reference combat families are working, performance hardening includes
the open rigged-character FPS issue (#169), multi-NPC combat, animation cost,
projectile pressure and realistic actor/population budgets.
