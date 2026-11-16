# Dialogue display-editor return receipt

Before: RenegadeStudio-Icons8.exe SHA-256
3B5E413C4A61956D37C4DE3957D45B9E2136EC260A1463F42DDA72C4515725E2.

Open the existing Gate Guard conversation, choose Edit display, inspect Advanced
Screen panels, resize the native window to 1280x720, restore, then Back. Hidden
Dialogue fields reappear over Voice and other basic controls. The overlap
persists after settling. before-return-overlap records that state.

Native Window::SetVisible(true) propagates visibility to children. The
resumeAfterDisplay branch refreshed conditional visibility first, then showed
the parent, undoing those choices. Normal Open already refreshed after showing.
Resume now fits the canvas, shows the Window, then refreshes current fields and
NPC targets. Basic/Advanced and selected-row visibility remain authoritative.
The draft, display assignment and authored Screen appearance are retained.

After: BUILD/native/Studio/Release/RenegadeStudio-Icons9.exe SHA-256
43D1C97E707BEF733EDCED29EB14FC3A1A2EA45D9B4BBB08C1A50C91B5101C01.
Built before commit; configure-time Git stamp is not release identity evidence.

Native checks on this exact binary:
- Basic conversation -> Edit display -> Back: voice controls remain ordered;
  unrelated advanced controls stay hidden (after-basic-return).
- Enable Advanced -> Edit display -> Back: current reply/outcome controls stay
  visible and correctly placed (after-advanced-return).
- Return to Basic -> Edit display -> 1280x720 -> maximize -> Back: current reply
  and Continue field are ordered, with advanced presentation/voice controls
  hidden where inapplicable (after-resized-display-return).

MSBuild BUILD/native/Studio/RenegadeStudio.vcxproj /t:Build
/p:BuildProjectReferences=false /p:Configuration=Release /p:Platform=x64
/p:TargetName=RenegadeStudio-Icons9 /v:minimal, CL=/MP6: exit 0.
ctest --test-dir BUILD/native -C Release
-R 'SourceContract|StudioUiPolicy|RenegadeDialogue|^RenegadeStoryFlowJourneyUiLayoutTests$'
--output-on-failure: all 67 pass in 5.70 s. git diff --check passes.
No new bespoke tests mirror this ordering; the native round trip is the
behavioural regression receipt.

A copied review project/preferences was used. No dialogue text, authored Screen,
owner project or save schema was changed. No Runtime gameplay change, hosted CI,
push, PR, merge or release. The current binary also includes the prior modal
resize fix; its GPU before/after receipt remains scoped to Icons6/Icons8.
