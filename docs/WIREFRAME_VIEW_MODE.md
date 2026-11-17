# Whole-scene viewport modes

SHOW > View Mode offers Shaded, Wireframe and Shaded + Wireframe.
The active mode is marked and SHOW remains accented while wireframe is enabled.
Editor Grid remains an independent SHOW toggle. The choices apply to all native
scene meshes, independent of selection; editor markers are separate aids.

Mode changes use the existing native renderer and are editor preferences through
ProjectService, never level metadata, undo commands or Runtime settings.
The Render diagnostics selector shares the same setter. Path tracing suspends
wireframe and returning to realtime restores the preference. Scene/workspace
diagnostic resets retain the editor preference.

## Bounded core-patch decision — 16 November 2026

Native visual testing exposed missing shaded wireframe edges. The pinned
wiRenderer.cpp selects PSO_object_wire variants using DSSTYPE_DEFAULT, whose
reverse-Z comparison is strictly GREATER and writes depth/stencil. An overlay
on an already-rendered surface is coplanar, so these edges fail that test.
Studio cannot replace an internally chosen native PSO through its adapter.

On Windows, cmake/WickedWireframeOverlayFix.cmake consumes the existing
UMA-corrected RenegadeWickedFix/wiRenderer.cpp stage and writes the distinct
wiRendererWireframe.cpp final stage. Both raw and intermediate renderers
are header-only; embedded/external targets compile only the final source.
This preserves the coherent-UMA upload policy alongside the wireframe change.
On other platforms the wireframe stage consumes the pinned source. It adds
six dedicated overlay PSOs using DSSTYPE_DEPTHREAD (GREATER_EQUAL, no depth
writes, no stencil writes), selected only by DRAWSCENE_WIREFRAME_OVERLAY.
Original shaded and wireframe-only PSOs retain their original state.
No submodule source or gitlink change is required; existing dirty overlays
are preserved. Anchor mismatches fail configuration. Reconfiguration only writes
the generated source when its contents change.

Conflict area: native wireframe PSO creation and DrawScene PSO selection.
This correction is suitable for proposing upstream. No shader ABI, scene schema,
Runtime preference or normal shaded pipeline changes are introduced.

## Validation

Implementation: fada1d6353691a14508b9b748ec8335631984cfb.
Release build uses CL=/MP6 and the following commands:

- cmake -S . -B BUILD/native -DRENEGADE_EMBED_SHADERS=ON
- MSBuild BUILD/native/WickedEngine/WickedEngine/WickedEngine_common.vcxproj /t:Build /p:BuildProjectReferences=false /p:Configuration=Release /p:Platform=x64 /v:minimal
- MSBuild BUILD/native/WickedEngine/WickedEngine/WickedEngine_emb_shaders.vcxproj /t:Build /p:BuildProjectReferences=false /p:Configuration=Release /p:Platform=x64 /v:minimal
- MSBuild BUILD/native/Studio/RenegadeStudio.vcxproj /t:Build /p:BuildProjectReferences=false /p:Configuration=Release /p:Platform=x64 /p:TargetName=RenegadeStudio-Wireframe5 /v:minimal

All exit 0. Build both Wicked targets before relinking Studio: building common
alone does not refresh the embedded renderer library. Final generated projects
contain one renderer in embedded/external targets and none in common, verified
against the generated project files. Earlier duplicate-object build attempts
were corrected and are not the final configuration. The generated renderer
SHA256 remains CCF9226F77D6A9E298DC5F4C2360DC672F72C3BA4E9E01F328CAFE38DDDB4E0A
across reconfiguration.

ctest --test-dir BUILD/native -C Release -R
'SourceContract|StudioUiPolicy|RenegadeDialogue|^RenegadeStoryFlowJourneyUiLayoutTests$'
--output-on-failure: 67/67 pass in 2.58 seconds. git diff --check passes.

Native Wireframe4 candidate SHA256:
97D0BD0DCAA88D9E0AA7A0389FF8CFD68FA946123B48421460D54BDE0BA913C6.
Actual native screenshots verify fresh-process restoration of overlay mode;
visible coplanar cube triangle edges; returning to normal shaded surfaces;
whole-scene wireframe; SHOW accent and exclusive active mode. Scene remains
clean and command-history counts remain unchanged.

Final Wireframe5 candidate only replaces an unsupported check glyph with ASCII
[x]/[ ] markers. SHA256:
B0D0F9C2EE4AB9BDA98963B53E1DEDCAD0906B43F940D115CBD199C88D909ECB.
Both artifacts were built before the source commit; their embedded configure-time
Git stamp is not an exact release identity receipt.

Evidence: docs/evidence/wireframe-show. Existing owner-operated Studio windows,
projects and dirty Wicked overlays were preserved. No push, hosted CI, merge,
distribution, Debug configuration or independent release-gate claim.

Native overlay covers opaque scene meshes. Sky, particles and editor markers
retain their own native presentation. Transparent-surface overlay parity,
path-tracing transition interaction, large terrain/complex scenes and MSAA
variants were not separately exercised by this scoped native review.


Final Wireframe5 native follow-up: fresh-process Wireframe restoration, ASCII active
markers, exclusive overlay selection and visible coplanar triangle edges all verified.
The latest review process is left open with SHOW displaying the overlay selection.

## PR #197 composition recovery — 17 November 2026

The original generated-file collision discarded the UMA memory fix and failed
hosted Studio Debug/Release on the software GPU despite hardware preflight
passing. The distinct composition stages above correct that integration error.
RenegadeRendererOverlayComposition validates cold/repeated configuration,
both target source selections, stable output and both patch policies.
RENEGADE_TEST_FORCE_WARP defaults OFF and permits isolated Windows test builds
to exercise the unchanged allocator/render path on actual WARP.
See [reproduction and recovery evidence](UI_PR197_UMA_RECOVERY_20261117.md).
Historical build hashes and results above describe earlier implementations.
