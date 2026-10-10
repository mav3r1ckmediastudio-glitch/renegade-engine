# P3 creator workflow - agreed direction, 2026-10-06

Owner discussion establishes the following target; this is a design, not a
claim that every capability is implemented. Health must not block this work.

Add -> Projectile opens a Renegade editor independent of Player selection.
Create from a template, import a mesh through the existing model importer,
choose a registered library model, or edit a copy of a saved projectile.
Save registered .rprojectile assets in Content/Projectiles. Reference assets
by stable ID; propagate required dependencies through snapshot/package closure.

Simple mode exposes template, name, appearance, speed, gravity and impact.
Advanced sections progressively expose mesh orientation/scale, collision size,
damage, lifetime, multiple attached effects, trails, lights, sound and impacts.
Templates target Arrow, Quarrel/Bolt, Tracer Bullet, Fireball, Beam/Laser and
Thrown. Templates are editable copies. Mesh is optional. Beam/hitscan settings
must not pretend to be travelling ballistic objects.

A weapon launch socket records position AND orientation relative to a weapon
or bone. Support imported named markers/bones (including FIRESPOT convenience)
and editor-created sockets. Rotatable 3D weapon preview supports surface click
placement, precise translation/rotation gizmos, direction arrow and test fire.
Animated sockets follow their selected bone. Palm sockets may live on an arms
rig for spells. Multiple sockets support shotgun barrel policies explicitly.

P2 action assignment connects fire mode (Hitscan/Projectile/Beam), projectile
where applicable, launch socket and animation launch moment. Picker defaults
to Content/Projectiles and only valid projectile assets. Aim/near-cover rules
must prevent a muzzle behind cover shooting through it. Loaded bow arrow hides
at the release marker; cancelled draw launches nothing.

Shared impact profiles combine shot type with material surface category.
World materials declare Metal/Wood/Concrete/etc.; response chooses decal,
particles and sound. Bullet holes are bounded decals, not permanent mesh edits.
Moving surfaces need attachment or an explicit fallback. HUD enemy-hit markers
remain separate from arbitrary world-contact feedback. No usable health claim.

Use Wicked rendering, scene transforms, particles, trails, lights, audio and
intersection APIs beneath Renegade bridge services. Renegade owns assets,
timing, movement/lifecycle, impacts and UX. Do not copy sample frame-count
movement or hiding effects far away as a production lifecycle.

Delivery:
1. User arrow FBX/textures through existing import pipeline; saved mesh-linked
   projectile with scale/orientation; live shotgun visual proof and dependency
   snapshot/package tests. Existing shotgun is temporary arrow launch fixture.
2. Dedicated Add/editor/library entry and actual 3D visual preview.
3. Authorable sockets, P2 binding and animated launch timing; near-cover proof.
4. Basic hitscan using common attributed contact pipeline.
5. Impact persistence/decals, effects, beams and advanced presets in bounded
   increments. Bow+arms owner can supply from Unreal after export needs settle.

Each increment requires serialized reopen, native inspection and relevant
standalone proof. Full P3 gate remains open until independent exact-head review.
