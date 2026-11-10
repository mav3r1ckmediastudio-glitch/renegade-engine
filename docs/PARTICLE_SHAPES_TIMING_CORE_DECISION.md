# Particle completion: bounded core decision
Owner explicitly requested completion of emitter shapes and effect timing, with
Lua parity. The bounded emission extension supplies point, legacy box, sphere,
cone, disc, ring and cylinder sampling. Renegade owns validated authoring state,
metadata, UI, preset codec and Lua. Native consumes nonserialized shape values
and one appended float4 parameter region; reserved movement flags z/w select
shape/surface. Existing optional emission permutations are reused. Point/box
without variation preserve the stock emission path and RNG sequence. No native
particle/archive format or upstream-pointer change. All new binaries and shaders
must be staged together in a separate preview directory.

Timing is entirely Renegade-owned. A scene-scoped validated root index advances
one shared clock per effect. It gates emission and stationary visibility, queues
bounded start-of-cycle bursts, and lets existing particles finish. No additional
scene entities or CPU particle readback. Discovery shares the existing emitter
component pass and point membership uses native lookups. Runtime controls share
the same clock; native scene metadata retains authoring values for save/reload.
Checks must cover native GPU distribution/orientation, Lua, migration, composite
history, exact timeline boundaries, native picker and standalone player.
