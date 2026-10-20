# Renegade Assistant RA-02/RA-03/RA-04 first integration — implementation checkpoint

**Date:** 2026-10-12
**Status:** Local Debug build PASS and RA-02 source contract PASS on 2026-10-12. Studio startup smoke PASS. Native dock visual and interaction acceptance still OPEN; no release approval. **2026-10-12 design supersession:** owner-approved living energy orb replaces procedural digital face as the future manifestation; real-time orb and cross-monitor windows are not built.

## Scope implemented in working tree

- Native wiGUI right-hand **assistant dock** alongside the Inspector (reserved layout width, not viewport overlay) when the Studio Level Editor is open on a sufficiently wide window.
- WINDOW > SHOW/HIDE ASSISTANT toggle, in-panel HIDE button, and a stored open/closed editor preference.
- Small **original procedural digital face** (no third-party character assets), brief response bubble and transcript. **Historical RA-03 rendering seam only:** the owner has since approved an original **living luminous energy orb** with a central audio-reactive speaking waveform, **no face or lip sync**. Do not confuse the old face placeholder with the approved runtime target.
- Locally stored conversation and current guide step in `Saved/Assistant/`, with bounded history, Older/Newer navigation, and no cloud/history upload.
- Native **four-step humanoid animation retargeting guide** tied to the Studio Inspector's existing HUMANOID / RETARGET, AUTO-MAP, Bone Slot/Target, IMPORT + RETARGET and ANIMATION sections. Back/Next keeps the written step visible while the user works in the viewport; completion is **not automatically inferred**.
- Optional **loopback-only Qwen3-1.7B V3.1 chat** via the experimental `http://127.0.0.1:8383/chat` service, with non-blocking background request. If unavailable, the native guide remains usable and the assistant must say so.
- The assistant has **no editor observation bridge, scene creation/editing tools, runtime gameplay privileges, web retrieval or TTS/STT**. No claim that it can see the user's selection.

## Safety and accurate status

This is not a production AI release. V3.1 showed false statements in previous qualitative evaluations. The optional local model is deliberately isolated from Studio mutation. A production support-only policy, verified engine capability registry, source-link renderer, model-output grounding and link validation are still required. Do not expose the localhost test service publicly.

## Verified local build and test results (2026-10-12)

- Local CMake Debug build: first linker pass reported unresolved WinHTTP symbols because CMake regenerated the project while that build was in flight; an immediate incremental rebuild using the refreshed link target PASSED.
- Built executable: BUILD/renegade/Studio/Debug/RenegadeStudio.exe (68,709,376 bytes).
- Dedicated RA-02 CTest: 1/1 PASSED (0.03 seconds).
- Direct MSVC syntax checks of RenegadeAssistantDock.cpp, RenegadeStudioChrome.cpp and StudioApplication.cpp: PASSED; an unrelated nodiscard warning remains.
- Launched the new Debug Studio executable; process remained responsive and reached the startup identity / Project Hub screens. The smoke-test Studio instance was closed cleanly.
- Native dock interactive visuals have NOT yet been owner-tested. A separate Runtime from another worktree was active and was left untouched.
- No CI, PR, commit, model promotion, or live engine deployment.

## Manual validation after successful local Debug build

1. Launch the freshly built `RenegadeStudio.exe` using its correct output-folder working directory. Open an existing test project.
2. Open WINDOW > SHOW ASSISTANT. Confirm the viewport contracts to leave space for both the ordinary Inspector **and** the assistant. Check scene interaction, camera, resize and asset browser.
3. Confirm the digital face/bubble are visible, and buttons/input remain responsive. Hide and reopen the dock from WINDOW.
4. Type `Alright?`, then `Help me retarget animations`. Verify the guide has four accurate steps, Back/Next works, source text is not misleading, and the Inspector remains operable.
5. Close/reopen Studio, reopen the project, and check the saved chat/guide step and dock preference. Verify safe behaviour when no `8383` listener exists.
6. With experimental test server online, ask a general conversational question; confirm the response arrives asynchronously and no modal input lockup occurs.
7. Ask `Create a sci-fi level for me`; assistant must decline and offer user-led guidance. Verify no object/game creation occurs. Record any paraphrase that incorrectly agrees as a reliability bug.
8. Test at normal/high DPI, narrow window, long conversation, and a complex level. Record screenshots and performance before owner visual acceptance.

## Known limitations / follow-up

- Basic text cards and paging are an **MVP**, not the full rich/searchable Markdown chat. Add scrolling, selectable URLs, manual/YT citation chips and snippets after UX review.
- The narrow-window fallback currently hides the dock when width is below 1300 logical pixels, rather than providing an adaptive pop-out layout. **Independent detachable native Windows chat and avatar windows**, drag-to-second/third-monitor placement, snap/dock, mixed-DPI and off-screen recovery are approved requirements **not yet implemented**.
- The approved **living orb**, speaking waveform, motion/colour states and optional voice remain **RA-03/RA-07 follow-up**; the procedural face only proves the native rendering seam. **No face or lip-sync pipeline** should be added. A standalone animated orb prototype precedes integration.
- Qwen integration currently depends on the separate V3.1 **experimental** localhost service. It is not embedded or distributed as part of Studio.
- History is local to the Studio working directory. Project-specific isolation and deletion/privacy UI still need to be designed.
- No CI run, branch merge, commit or PR is requested as part of this initial checkpoint. Local build and owner testing are separate gates.

## Source of truth

- Design: `docs/RENEGADE_ASSISTANT_ROADMAP.md`
- Native UI and guide: `Studio/src/RenegadeAssistantDock.h/.cpp`
- Integration: `Studio/src/StudioApplication.h/.cpp`, `Studio/src/RenegadeStudioChrome.h/.cpp`, `Studio/CMakeLists.txt`
- Local build log: `BUILD/ra02-assistant-studio-build.log` (untracked artefact)

Keep the core Player/Projectile programme and all existing external AI assets undisturbed.
