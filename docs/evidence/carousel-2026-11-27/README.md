# Carousel local verification, 27 November 2026

Windows x64 / DX12 Release on DESIGN.

The standalone Runtime opened a fresh scaffold review project with three carousel entries. Entry A displayed its forest-path caption, entry B displayed its bridge caption and entry C had no caption. The saved screenshots show the exact application output after the caption refresh correction and review-layout correction. The Continue button was reduced and placed below the caption area. The review carousel uses the same free child layout and bottom caption anchors as newly created carousels.

- `runtime-A.jpg`: image A and caption A below the picture.
- `runtime-B.jpg`: image B and caption B below the picture.
- `runtime-C.jpg`: image C with no caption or caption background.

The first visual attempt exposed a real presentation error: Runtime's ordinary Text refresh replaced the current slide caption with the empty authored Text value. Carousel captions now use a separate transient presentation value, so ordinary controller refreshes retain the current image/caption pairing. A separate review-fixture error inherited a grid container and placed the caption over the image; the corrected fixture uses the actual new-carousel defaults.

The focused CTest run after the correction passed `RenegadeScreenAuthoringSessionTests`, `RenegadeScreenRendererTests`, `RenegadeRuntimeScreenTests` and `RenegadeProjectUiScaffoldTests`. `RenegadeScreenAuthoringCompletionTests` also passed after recompilation. Authoring coverage includes schema upgrade, one-step creation/history, edit Undo/Redo, multiline UTF-8 and config punctuation round-trip, blank captions, typed image dependencies, invalid paths/IDs/durations, caption bounds, copy/paste and deletion/Undo.

The reused local build had missing header dependency records and stale executable links. All source files transitively including the changed Screen headers were explicitly recompiled, and affected application/test executables were freshly linked. Earlier stale-binary failures do not establish final results.

These are scoped local checks. Exported-package playback, Debug, hosted CI and independent release verification are not established by this receipt.

## Native editor controls

Opened the saved Title screen through Story Flow in the rebuilt Studio. The Basic palette exposes Carousel. Selecting the authored carousel opens the native entry editor with the current picture and caption. Exercised Previous/Next, moved an image/caption pair up and down, imported an external PNG through the governed texture workflow, removed that temporary entry and saved. The reopened screen retained A/caption A, B/caption B and C/blank. Cancel preserved the screen file byte for byte. Created a new carousel through Basic and observed both the image and caption child; Undo removed both, Redo restored both and a final Undo returned the review screen to its original seven widgets. Studio Preview displayed the same correct caption below its image. The editor capture is editor.jpg.

A final placement correction reserves room for the caption when dropping a new carousel near the bottom of the canvas. Move up/down stop at the list boundaries. Studio was rebuilt again for this bounded authoring correction; the bridge and Runtime were unchanged. Studio UI policy checks passed.

Verified the final build by dragging Basic Carousel to the canvas bottom-right. Creation succeeded at (800, 362), size (480, 270), with the linked caption ending at the 720-pixel canvas boundary. placement.jpg records the actual editor result. Undo returned the review screen to seven widgets. Reopened the entry editor and verified Move up disabled on the first entry and Move down disabled on the last; Previous/Next navigation still cycles through entries.
