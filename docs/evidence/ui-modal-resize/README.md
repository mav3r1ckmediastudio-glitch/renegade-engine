# Suspended-scene modal resize receipt

Before binary: RenegadeStudio-Icons6.exe
83FC83470A2798ACA3E5398C7A17DE1C4DA4CD477435583C0509C9BA165EF6ED.
Open Light Picker from Add with an ordinary asset selected, resize the native
window from maximized to 1280x720, then restore it. Main viewport stripes persist
while the picker remains open; closing it restores the scene but leaves inspector
rows scrambled until selection refresh. before-resize-corruption and
before-inspector-stale record those separate failures.

The main scene Update/Render is suspended for light/particle/dialogue modals.
ResizeBuffers replaces its 3D targets, but Light/Particle Compose still sampled
the main render result. Dialogue already used a safe 2D backdrop. All three now
share that composition branch. The private preview remains the only active 3D
renderer. Close-frame ownership stays suspended until a fresh main Scene update.

Creating a light also cleared selection without refreshing hierarchy/inspector.
That presentation is now refreshed immediately. Native resize requests a
one-time complete inspector refresh, consumed after the native Window update,
including the modal's 2D update. It is not a per-frame inventory rebuild.

After binary: BUILD/native/Studio/Release/RenegadeStudio-Icons8.exe
3B5E413C4A61956D37C4DE3957D45B9E2136EC260A1463F42DDA72C4515725E2.
This was built before commit; its embedded configure-time Git stamp is not an
exact release identity receipt.

Native checks on this binary:
- Light creation: select Player, open Add > Point Light, resize to 1280x720,
  restore. The selection clears consistently in hierarchy and inspector;
  background remains opaque, with no unwritten main-scene image sampled.
- Existing light: open its Colour / Light Picker, resize, restore, close.
  LIGHT // Point Light remains selected with ordered inspector rows and a
  clean main viewport. after-light-edit-close records the result.
- Particle: open Add > Particle Emitter, resize, restore, close. A working private
  preview and safe background remain. after-particle-resize records the modal.
- Dialogue: open the existing Gate Guard conversation, resize, restore, close.
  Conversation and authored display remain intact; scene resumes cleanly.

Release build command: MSBuild BUILD/native/Studio/RenegadeStudio.vcxproj
/t:Build /p:BuildProjectReferences=false /p:Configuration=Release
/p:Platform=x64 /p:TargetName=RenegadeStudio-Icons8 /v:minimal, CL=/MP6.
Exit 0. Existing C4834 warning remains.
ctest --test-dir BUILD/native -C Release
-R 'SourceContract|StudioUiPolicy|RenegadeDialogue|^RenegadeStoryFlowJourneyUiLayoutTests$'
--output-on-failure: 67/67 pass in 2.81 s. git diff --check passes.

Uses a disposable copied project/preferences; no owner project was edited.
No hosted CI, PR, push, merge or release. Wicked overlays and gitlink unchanged.
This does not fix the separately observed small-window modal footer clipping,
Physics page title overlap on scroll, or all remaining UI audit families.
Window-size checks are not certification of OS DPI 125/150 percent or path-traced
preview mode. No scene/save schema or Runtime gameplay code changed.
