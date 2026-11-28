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


## In-editor Studio modal correction

The original entry editor was a Windows dialog. An intermediate recolour still retained OS controls and was rejected during owner review. Both are superseded by ScreenCarouselEditor deriving from the existing StudioModalWindow. The final editor is drawn within the Studio window and uses shared Renegade buttons with SetAccentOutlineStyle, the normal checkbox and numeric field, native TreeList and texture resource preview. There is no separate Edit carousel HWND or carousel dialog resource. editor.jpg now records this final in-editor window, including the outline buttons approved during owner review.

StudioMultilineTextInput is a native Studio widget with literal Unicode, line breaks, selection, clipboard and local Undo/Redo. A focused-only WM_CHAR route and the existing GUI typing gate keep underlying Screen shortcuts inactive. Closing, changing screen or disabling the workspace clears ownership. Asynchronous image selection/import uses weak ownership, a generation check and stable target entry identity before returning to the GUI safe point. The standard modal provides a scrollable body and pinned actions. Footer drawing restores clipping for each shared control; numeric drafts are captured before the native outside-click cancellation path.

The final Windows x64 Release Studio was rebuilt and freshly linked after all changes. The workspace class layout change required recompiling all 40 transitively dependent Studio source files because this reused build lacks complete header tracking. The final incremental rebuild after the numeric-draft correction passed, and RenegadeStudioUiPolicyTests passed again. Earlier carousel schema/renderer/runtime suites are unchanged by this Studio-only replacement and retain their previously recorded results.

Exercised the final in-editor modal on the review project: selected A/B rows, moved B up and back down while retaining its paired caption, typed a replacement caption, entered a literal newline, pasted accented text, and used local text Undo/Redo. Saved a two-line caption (Native caption test / Second line: cafe with accented e), duration 3 and shuffle true, closed/reopened the modal, saved the Screen and inspected its serialized UTF-8 bytes, duration and shuffle. Imported an external PNG through the file picker and governed workflow, observed the fourth preview entry, then removed it. Edited a temporary caption and pressed Escape: the saved Screen bytes were unchanged. One Screen Undo restored the entire saved deck edit; saving returned the review Screen byte for byte to its original baseline, including A/caption A, B/caption B, C/blank, two seconds and shuffle off. Studio is left with that restored carousel editor open.

The initial automated keyboard attempt was interrupted by foreground changes and did not establish keyboard results. After restoring Studio focus, the final keyboard, clipboard, save and byte-comparison checks above passed. No exported-package, Debug, hosted CI, push, merge or independent release verification was performed in this correction.
