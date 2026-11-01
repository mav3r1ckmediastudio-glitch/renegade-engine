# Bounded core-patch decision: 3D body cache

Implementation study, not owner approval. Read WHOLE_BODIES investigation first.

The analytic whole-body proof records about 40-42 ms GPU-frame time in the
thin-layer 1080p reference views. It evaluates repeated thermal geometry,
hashes and trigonometry in camera/shadow samples. This is not release-ready.

Cache the immutable Cumulus shape in a genuine 128-cubed RGBA16F texture.
Its four channels hold four macro-body variants as continuous signed fields,
not rendered images. Preserve metre-scale density ramps, shared camera/shadow
sampling, internal water-content variation and fine world-space erosion.
The authored template is shared by the analytic reference and cache generator.

Cache static placement attributes in a small structured GPU buffer:
64x64 sites, two float4 values each. Tile extent is 192 km at 3 km spacing.
The site coordinates remain world-space, including negative cells. Every
4x4 site block has balanced admission ranks; height/width/rotation seeds are
independent of Coverage. The buffer replaces repeated per-sample hashing
and trigonometry. This finite attribute tile is not a skybox or billboard.

Use two bounded cloud-pass bindings: t5 for the 3D field and t6 for site data.
Initialize these derived resources only when the new Cumulus path is needed.
Generate/cache once after native cloud noise is ready, including mip transitions.
Register the generator in the offline compiler so both embedded DX12 and
SPIR-V packages contain it. Never depend on runtime compilation in a package.

Source of truth is an authored CMake overlay plus authored HLSL. Do not edit
only generated Wicked files. No fork pointer, weather GPU/archive layout,
native noise RGB, old scene fallback, Moon/scattering or river code changes.

The analytic reference remains available only to explicit local proofs.
A cached single-body proof must be compared from below, side and above.
Then measure full-field GPU cost and capture error on both APIs at both
resolutions. Reject visible cache artifacts or lost structure. Refit admission
for this whole-body model and validate held-out geometry and poses.
All required application builds and Runtime/Studio checks must be rerun for
the final source before a new owner preview. No later gates or Snow work.

This decision authorizes the bounded implementation within the owner's
existing cloud-architecture task. It is not acceptance of appearance,
performance or Gate 1 completion.
