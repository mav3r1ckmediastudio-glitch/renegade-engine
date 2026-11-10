# Particle lifetime controls: bounded core decision

The owner agreed lifetime colour/size curves as the next Particle Picker priority,
with Lua access. This extends the accepted movement/variation shader integration.

Renegade owns two-to-eight ordered lifetime keys, validation, sampling, preset
schema 6, scene metadata, optional GPU lookup generation, native editor controls
and governed Lua. Keys span normalized lifetime 0..1. Size is a base-size
multiplier; RGBA multiplies the emitted colour/tint and controls opacity. When
enabled the curve replaces the native linear size scaling and fade-peak profile.
Disabling restores both native controls without losing either authored setup.
Stationary looping sprites have no particle lifetime and retain their settings.

The bounded core patch exposes replacement of the existing derived opacity
texture, identifies a two-slice 1D RGBA lookup, selects the existing optional
simulation permutations and binds the lookup at their spare t1 resource slot.
The existing appended movement constant region supplies one reserved flag.
There is no native class-layout, particle-layout, constant-buffer-size, archive
version or submodule-pointer change. Field-free/curve-free emitters use the
original native shaders. Colour is applied to output vertices from the immutable
spawn colour; it must never accumulate into particle.color on each timestep.

The lookup is derived and rebuilt from serialized Renegade metadata after native
load/Undo/Redo/duplication. Missing optional shaders produce an explicit error.
No CPU particle readback or scene-wide entity inventory is used in production.
Required checks: interpolation and key validation; v1-5 neutral migration; schema
6 and scene cold reopen; composite edit/Undo/Redo/duplicate; Lua atomicity and
instance isolation; native GPU curve samples with all simulation permutations;
actual Studio authoring and standalone player proof. Independent exact-tree
verification remains required before any release gate closes.
