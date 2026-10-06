# P3 projectile inspection preview — 2026-10-06

Add → Projectile now includes an automatically refreshed 512×320 model view.
Select a mesh, adjust scale or XYZ rotation and inspect the result immediately.
The default side view places +Z flight direction to the right. Turn Left/Right,
Look Up/Down, Side/Rear View, Zoom In/Out and Fit Model operate only on the
inspection camera; they do not alter the saved projectile. Physical model size
is displayed in metres after uniform scale. Scale changes retain the current
framing so their visual effect is apparent; Fit Model reframes when needed.

Studio reuses the private ModelImportPreview scene. Asset preparation runs only
when model/project identity changes; scale/rotation update a private transform
root without reimporting. The preview uses the same scale/Euler convention as
RuntimeProjectileVisuals. Physics, scripts, characters and animation are disabled
on the private appearance copy. Source model and level are not edited. Preview
resources are released when the window is hidden or the project changes; stale
models/errors are cleared before selecting a new model. A required model preview
must be ready before Save As New is enabled. Meshless definitions remain valid.

The rendered label bypasses ordinary control tint/background blur. Player Start
physical/wire markers are suppressed while the projectile editor is visible;
the wire marker is also suppressed during model import to avoid covering its
rendered view. Projectile editor input owns the modal authoring interaction.

Release Studio build uses CL=/MP4 and:
cmake --build BUILD/renegade --config Release --target RenegadeStudio --
/m:2 /p:BuildProjectReferences=false /verbosity:minimal.
Initial build PASS 77.56s; correction rebuild PASS 20.00s; final label correction
build PASS 15.67s. Targeted ctest command:
ctest --test-dir BUILD/renegade -C Release -R
'Projectile|EquipmentAsset|EquipmentActionState|PlayerViewRig|TestLevelSnapshot'
--output-on-failure: 10/10 PASS 2.03s. No Wicked source/pin change.

Weapon/palm launch sockets, click placement, imported FIRESPOT resolution and
Runtime launch-origin replacement remain the next increment. Hitscan/Beam,
effects and surface profiles are still open. P3 is not closed; independent
exact-commit verification remains required. This preview is static appearance
inspection; flight/effect preview is not implemented.

Native evidence on the disposable shotgun project: supplied Arrow model appears
in the side view with its tip right. Turn Right changes the rendered view; scale
1→2 visibly doubles the model, and X rotation 0→30 degrees tilts it immediately.
Fit and Zoom In visibly reframe without altering the authored scale/rotation.
Save As New created ArrowPreviewProof (63750c6e-86b7-43c1-9a56-3ac67c355f15)
with visual_scale=2 and visual_rotation_degrees=[30,0,0]. Screenshots retained in
BUILD/p3-preview-{side,orbit,scale,rotation,fit,zoom}.png. Default shotgun
assignment remains the original Arrow. Standalone source regression passed
firing, ground impact, pause, dry fire, reload and reset in 11.52s. An earlier
reset key was missed while Studio startup stole focus; rerun with stable focus
passed. Appearance values are validated before renderer transforms are updated;
invalid values clear the preview and disable Save, with recovery on correction.

Cold Studio restart → Starting Equipment → Weapon Projectiles →
ArrowPreviewProof → Edit Copy loaded scale 2, rotation [30,0,0] and the rendered
model successfully. Selecting a different projectile in that picker did not
apply it to the weapon. Typed scale 500 cleared the image and disabled Save;
correcting to 2 restored the view. Final polish suppresses the selected level
gizmo/outline and player-camera inset, displays Custom / saved projectile on
Edit Copy, uses plain-language number help, and clears invalid images to a
neutral background. Final targeted tests: 10/10 PASS 2.34s; diff check PASS.

Final Studio build after overlay/background/help polish: PASS 15.64s.
Saved appearance values remain the existing schema v2; this increment introduces
no new persisted format or Runtime launch rule. Editor layout targets the current
Windows/DX12 desktop; responsive compact layout and mouse-drag orbit remain open.

Final native cold reopen confirms Custom / saved projectile, scale 2, rotation
30 degrees and an unobstructed rendered arrow: p3-preview-final-cold-reopen.png.
Typing 500 then clicking Save produced no new .rprojectile file (count unchanged);
neutral invalid-state image and plain-language bounds were visually checked.
Correcting to 2 restored the preview: p3-preview-final-{invalid,recovery}.png.
