# P2 equipment and action framework

Design authority: [PLAYER_ARMS_COMBAT_FRAMEWORK](PLAYER_ARMS_COMBAT_FRAMEWORK.md)
and [ROADMAP](ROADMAP.md). P2 is in progress; the accepted shotgun remains the
compatibility reference. This document does not close a release gate.

## Implementation sequence

1. Small assembly picker UX repair: dedicated arms/weapon folder eligibility and
   saved recipe roles, preserving existing assemblies and command history.
2. Shared equipment/action definition and staged gameplay state with independent
   hand reservations, semantic presentation requests, cancellation and pause/reset.
3. Governed project equipment assets, save/reopen and starting-loadout assignment
   in the existing Player Inspector/prefab workflow.
4. Runtime equipment ownership, equip/unequip, primary/alternate use, charge/release
   and reload integration; retain native Wicked animation as presentation authority.
5. Test Level snapshots, transitive packaging and owner gameplay verification.

Do not replace Player movement, physics, camera ownership, native skeleton
animation or existing damage/event boundaries. P3 still owns projectiles/impacts;
P4 owns the complete firearm reference. Starting loadout and later ammo policies
must use these common definitions rather than a second shotgun-only controller.

## Current implementation

`CollectFirstPersonPartChoices` is bridge-owned. Only available imported `.rasset`
parts in `Content/Player/Arms/` or `Content/Player/Weapons/`, or parts explicitly
referenced by saved assembly recipes, enter the relevant picker. Nested folders
work; prefix lookalikes, missing records and completed assemblies are excluded.
A combined-folder pack works when its saved recipe identifies each part's role.
Brand-new combined packs still need a future explicit role/import authoring route.
No filenames are guessed and no existing product is moved. Full project-relative
labels distinguish duplicate filenames. Combo user-data preserves stable identity
when the two filtered lists have different row numbers.

`EquipmentActionState` is a pure gameplay foundation with typed hand use and
semantic actions. Each actor has independent channels. Prepare, windup, hold,
active and recovery phases emit events; conflicting hands cannot reserve together.
Zero/nonfinite/negative update time cannot advance phases. Hold requires release;
cancellation is allowed only before active when authored. Reset clears reservations
and queued events. Definitions reject invalid timing and duplicate action kinds.
This foundation is not yet connected to the Runtime shotgun or persisted equipment
assets; an authored animation string is a semantic action, not a clip filename.

## Validation

- Assembly settings regression includes 100 unrelated models, nested dedicated
  folders, shared-pack recipe roles, missing assets, misleading folder prefixes
  and malformed provenance.
- Equipment action regression checks independent sword/shield hands, two-hand
  exclusion, active event counts, hold/release, cancellation restrictions, pause,
  invalid definitions, instant actions and reset.
- Native picker visual/save/reopen and Windows build evidence is recorded in
  HANDOFF.md at the implementation checkpoint.

Remaining: durable equipment identity/assets, starting loadout, Runtime integration,
semantic animation binding and end-to-end snapshot/package acceptance. No P2 gate
completion or new firearm/projectile capability is claimed by this foundation.
