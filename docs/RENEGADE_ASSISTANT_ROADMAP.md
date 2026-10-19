# Renegade Engine Avatar — Product Vision, Architecture and Delivery Roadmap

**Status:** Approved engine-avatar concept; first native chat/digital-face placeholder prototype built, owner visual acceptance open
**Updated:** 2026-10-12
**Parent roadmap:** [Renegade Engine Roadmap](ROADMAP.md)
**Scope:** Renegade **Studio/editor** companion, not the game's shipped NPC AI
**Integration state:** External Qwen model lab plus an **uncommitted, locally built native Studio RA-02 dock prototype**. Source-contract/build/startup smoke passed; owner visual/interaction approval, detached windows, final avatar, packaged integration and release remain OPEN.

## 1. Product decision

**The avatar is Renegade itself made visible — the game engine's own voice and presence, not a separate chatbot character.** Internally, "Renegade Assistant" remains the name of the conversational/UI workstream and services. It guides, diagnoses and helps with bounded technical tasks; it is **not** a game character, game generator or autonomous game-building agent.

- **The manifestation gives the engine a presence.** The owner-approved visual direction is a **living, animated, luminous energy orb**: compact and understated when idle; enlarged, flowing and audio-reactive when speaking. **No humanoid face, facial rig or lip sync.**
- **The chat supplies detail and memory.** A persistent, dockable chat keeps full replies, history, step-by-step guides, cited references, previews and code accessible while the user works.
- **The editor supplies the facts and capabilities.** The application, not model weights or an invented backstory, is authoritative about available features, current selections, scene state, permitted actions and the result of any action.
- **The developer remains the creator.** User-driven scene construction is a deliberate core product value, not an interim technical limitation.

### Tone and identity

- Conversational, approachable, articulate, observant, calmly authoritative and lightly dry-witted: the useful, composed **JARVIS-like interaction style**, not SHODAN's sinister persona. This is an **original Renegade identity and voice**: no copied character artwork or unauthorised actor voice cloning. The persona is Renegade the engine, not a separate mascot.
- The personality may invent harmless fictional hobbies or anecdotes as conversational colour. They must never be mistaken for actual Renegade levels, built assets, editor activity, or verified product capabilities.
- Renegade is a **game engine/editor**. A user's game project is a separate thing. The assistant must preserve this distinction, including after topic changes or jokes.
- Its apparent confidence must never outrun verified knowledge: "I can't inspect that yet", "That isn't available in this build", and "I haven't done that" are acceptable and desirable answers.

## 2. Non-negotiable behavioural boundary

**Create-the-game requests:** "Create a sci-fi level for me", "Generate my whole city", "Build the complete game from a prompt", or attempts to achieve the same outcome via a world-generation script must be declined as requests for autonomous construction. The assistant should offer coaching and a user-led workflow instead.

**Permitted support:** explanatory guidance, concept discussion, planning/layout advice, tutorials, source-linked manuals, troubleshooting, diagnostics on authorised data, specific gameplay scripts for review, individual asset/import/material/rigging/retargeting tasks, and narrowly scoped editor operations where a verified bridge and approval model exist.

**Opening and showing existing material is different from creating it.** A future bridge may open an **existing** level, focus an existing object, or return a viewport capture, after verifying the target exists. A made-up personal anecdote about a level does not create a real level that can be opened.

Implement this boundary in **both** the conversation policy and the Editor Bridge's allowlisted action layer. A prompt filter alone is not sufficient: paraphrases, follow-up requests, scripts and action chains must not bypass it. The engine must retain authoring ownership and transaction limits even if the model generates an unsafe request.

## 3. Interaction model

### 3.1 Renegade's living energy orb — approved visual direction (2026-10-12)

- **Identity:** An original, premium-looking **orb of living energy**, not a face, person, mascot, Clippy-style helper, fairy or animated pet. The orb is the embodiment of **Renegade the game engine**; voice, not facial acting, carries its JARVIS-like personality. No facial rig, mouth shapes, expressions, lip sync or humanoid expansion.
- **Collapsed/idle:** Default **lower-right**, compact, low-luminance, visible but unobtrusive; gentle internal plasma circulation, restrained translucent filaments, a luminous nucleus, sparse fine particles and a faint halo. No attention-seeking bouncing, fast spinning, aggressive flares or constant unsolicited movement.
- **Expanded/speaking:** Orb grows smoothly into a layered, translucent sphere; swirling flowing plasma and finely threaded energy arcs blend in **cyan/blue, violet and restrained warmer highlights**. A **luminous voice waveform suspended across its centre** moves with the actual TTS output amplitude/envelope (smoothed, not chaotic); outer energy and glow pulse in sympathy. The waveform is part of the depth of the core, **not a pasted-on equaliser**.
- **Intentional states:** idle (cool blue/cyan); listening (focused cyan-white response); thinking (tighter internal violet geometry); speaking (cyan-blue/violet and occasional warm blending plus centre waveform); success (restrained teal-white); warning (amber/red accents); unavailable (muted blue-grey). Transitions must be smooth, not random rainbow cycling. Animations are local, application-controlled rather than generated by Qwen.
- **Interaction rhythm:** compact idle → user summons/listening → thinking → expanded speaking with waveform → gentle return to quiet idle. While paused between responses, retain appropriate quiet state. State and signal must be driven by verified activity, not imagined model output.
- **Placement:** A separately draggable avatar window or in-Studio dock position with **per-user saved location**; may float on **any Windows monitor** including a second or third display. Optional always-on-top, discoverable click/drag target, optional click-through outside interactive regions, snap-to-chat, hide/show and reset-to-visible-display. **Do not treat moving beyond the Studio viewport as cross-monitor support** — detached mode needs a genuine top-level window.
- **Speech bubbles:** optional, compact, short summary only; full response and guide remain in chat. Avoid critical controls, viewport centre and cursor; suppress unsolicited prompts by default. No default speaking without user initiative.
- **Accessibility/performance:** independently hide/mute avatar; reduced-motion and static alternatives; controllable brightness/size, high DPI, mixed monitor scaling, CPU/GPU budgets and text-only fallback. No additional expensive full-scene render just to keep the avatar alive.
- **Prototype discipline:** first build a **standalone real-time animated orb** (idle/expand/speak/waveform and representative colour transitions) before integrating into Studio. Compare a lightweight transparent companion renderer against native Wicked rendering, measure both, and choose by visual parity, idle cost and multi-monitor robustness. Concept images are visual targets, **not evidence that a runtime implementation exists**.
- Proactive guidance, if added later, must remain **opt-in**, event-driven and grounded in verified editor diagnostics; do not infer problems from guesses.

### 3.2 Chat: dockable in Studio **and** detachable to any monitor

- Resizable **dockable chat**, left/right or floating, minimisable and re-openable; **dock by resizing the viewport** by default rather than covering the working scene.
- **Detachable native Windows chat window:** drag it independently from the editor and the orb to any connected monitor, including a third display. It must remain a real, separately focusable/resizable top-level window, not merely a wiGUI panel clipped outside Studio.
- **Independent layout:** avatar and chat may be docked together, snapped adjacent, freely floated separately, or hidden independently. Dragging one must not unexpectedly reposition the other. Shared conversation, guide step, state, voice output and activity indications must remain continuous when docking/undocking.
- **Robust display handling:** remember monitor-aware window placement, size, scaling and dock state across Studio/project restarts; adapt to mixed DPI, resolution changes and monitor disconnect/reconnect. Recover off-screen windows to a visible display, offer Reset Layout, support normal Windows keyboard focus/Alt-Tab/drag behaviour and minimise/restore.
- **No unwanted viewport overlay:** when docked inside Studio, reserve space; when detached, return that space to the viewport. Prefer default docked chat plus collapsed lower-right orb, with user-configurable positions.
- Full text conversation and searchable project/session history with explicit clear/delete/export controls and appropriate local retention.
- Markdown-style formatting, numbered instructions, expandable details, code blocks with copy, source citations, verified manual references, external webpage links and YouTube tutorial links.
- The manifestation can speak a **short summary**, while the panel retains the complete technical answer. Both refer to the same response and current tutorial step.
- Rich editor affordances may include "show panel", "focus selected object", "copy script", "open verified guide" and "next step" **only when implemented and permissioned**.
- External links: visible destination and provenance; favour version-matched official manuals; mark third-party tutorials and check relevance rather than inventing URLs.

### 3.3 Optional voice

- First: optional local speech synthesis (TTS) with synchronized text, interruption/stop and audio-output selection.
- Second: user-initiated **push-to-talk** speech recognition (STT); do not ship always-listening as the default. Show microphone state and provide a straightforward privacy/disable control.
- Investigate and benchmark local speech technologies (for example, whisper.cpp for STT and Piper/Kokoro for TTS), with CPU/GPU use, latency, quality and licenses measured **before** selection.
- Original, expressive voice; no unauthorized actor/character voice cloning. Voice disabled/muted must leave full text functionality intact.
- Include latency budgets and behaviour on slow hardware; speaking must not block editor input or processing.

### 3.4 Guided workflows without context switching

- Guides are structured, versioned content, not loose chains of generated guesses.
- Each guide has prerequisites, current step, validation rule, authoritative source/link, expected UI state and clear fallback.
- "Next step" advances the guide **without discarding the conversation**; preserve the user's position across closing and reopening the panel.
- Where available, verified editor events can indicate "step completed"; otherwise ask the user to confirm. Never assume a step was completed solely because time passed or the assistant recommended it.
- Prefer short spoken/bubble prompts with an always-visible written step in the dock. Keep tools and viewport usable.

## 4. Technical design and trust boundaries

```text
                       Renegade Studio
  +-------------------------------------------------------+
  | Viewport + Panels   Dockable Chat    Manifestation    |
  |       |                  |                |           |
  | Editor services     Chat/session      UI state       |
  |       |                  |                |           |
  |       +---------- Editor Context Broker --------------+
  |                          |                            |
  |              Assistant Orchestrator                   |
  |     +--------------------+----------------------+     |
  |  Retrieval/        Support-only/         Voice I/O    |
  |  guide sources     capability policy      optional    |
  |     +--------------------+----------------------+     |
  |                          |                            |
  |             Local Qwen3 conversational model          |
  |                          |                            |
  |            Verified, allowlisted Editor Bridge        |
  +-------------------------------------------------------+
                              |
                 Confirmed Studio result/state
```

**Windowing boundary:** The diagram represents shared services, **not** a requirement
to keep every UI surface inside one HWND. When detached, chat and orb are
separate Windows top-level windows (and may be on other monitors), connected
to the **same** Studio session/context/voice state. Native dock is the first
implemented prototype; cross-monitor window management is a planned RA-02/RA-03
seam and must not introduce a second conversation/orchestration stack.

1. **Context Broker:** passes trusted current build/version, open project and authorised selection/context snapshots. Clearly distinguish missing access from a negative result. Keep context small and versioned.
2. **Knowledge service:** index versioned Renegade manuals, verified Studio UI workflows, documentation and chosen external resources. Separate shipped features from specifications, plans and historical handoffs. Cite source and applicability in the chat.
3. **Orchestrator:** classifies social chat vs questions vs tutorials vs bounded action requests. Supplies conversation/guide history and facts. The small LLM is for dialogue and intent, not sole ground truth.
4. **Capability registry:** machine-readable, per-build allowlist of real Studio services, named tools, argument schemas, scope, permission requirements and availability.
5. **Editor Bridge:** validate arguments, targets, permissions and applicable scene/project; require review/approval for changes; route through existing Studio/EngineBridge services and undo/redo where appropriate. Never expose arbitrary unrestricted editor mutation.
6. **Execution receipt:** return explicit success/failure and identifiers/diagnostics; the assistant reports only what the editor confirmed. Timeouts or missing receipts are **not** success.
7. **Image boundary:** Qwen3-1.7B here is **text-only**. Renegade can *display* screenshots or viewport captures in chat, but the model cannot visually inspect them without a separately designed vision pathway or structured scene data.
8. **Resource limits:** Windows-first, local/offline-first. No assumed cloud dependency, paid service or network access. Hyperlinks/videos need connectivity. Bound CPU/GPU usage and support a text-only degraded mode.

## 5. Verified baseline versus planned product

**Verified experimental work (external to engine):**

- Local Renegade documentation retrieval prototype using Qwen3-1.7B and SQLite FTS over repo/manual snapshots; older A0 proof is read-only.
- V2 experimental QLoRA: **665** training examples, **170** steps; held-out 125-example reference set. These are model-lab results, not editor release milestones.
- V3 experimental QLoRA: **686** examples, **220** steps; adapter saved; isolated localhost test chat.
- V3.1 experimental QLoRA: **794** examples (including **108** new continuations from **32** added dialogues), **240** steps; adapter saved and benchmarked against V3 on 12 prompts.
- **Known V3.1 deficiencies:** qualitative benchmark still included false identity/capability claims ("I'm a character in the Renegade game engine", "playing the engine", nonexistent visible NPC, fictional clocktower treated as an engine feature). **Training completion did not equal acceptance.**
- Experimental localhost chat has a **support-only policy layer** and initial prompt/follow-up checks; these are **not comprehensive coverage** or an engine-integrated policy.
- Lab comparison UI runs separately from Studio, with no confirmed Editor Bridge, voice, mascot, editor-integrated persistent chat or feature-aware guide state.

**Native prototype implemented but NOT owner-accepted or shipped:** RA-02 wiGUI right-hand chat dock, a procedural face/bubble placeholder, local transcript and guide-step persistence, and a first four-step animation retarget guide. A local Debug build, source-contract test and startup smoke passed on 2026-10-12; see [RA-02 checkpoint](RENEGADE_ASSISTANT_RA02_PROTOTYPE.md). This is an uncommitted working-tree checkpoint; floating multi-monitor windows, approved orb and production integration remain unimplemented.

**Still not built/accepted:** production-quality chat UX, validated rich links/source citations, verified capability registry, project-aware observation bridge, read-only editor bridge, action execution, **real-time energy-orb manifestation**, detached avatar/chat Windows windows, optional voice, and packaged release. The former procedural face is **temporary demonstration code only**, not the approved visual identity.

## 6. Delivery gates, in recommended order

These gates form the **Renegade Assistant product workstream**, separate from Character/NPC AI and the main Player Arms & Combat programme. Gates may be prototyped in parallel where their acceptance criteria permit, but **no gate is marked complete from a design conversation or successful training loss alone.**

### RA-00 — Approved product and support-only contract
**Status: product decision approved; experimental guard implemented; production gate OPEN.**

- Record this scope and non-goals in canonical documentation.
- Design assistant identity, action classes (read-only, bounded edit, forbidden autonomous creation), error language and ownership model.
- Expand policy tests with paraphrases, indirect scripting, multi-turn bypasses and mixed benign requests.
- **Exit:** agreed boundary, threat-modelled allowlist and evidence-driven acceptance suite; no autonomous creation path.

### RA-01 — Factual knowledge, context and conversation quality
**Status: experimental prototypes exist; OPEN.**

- Build versioned first-party feature knowledge that distinguishes *implemented*, *planned*, *removed* and *unknown*.
- Separate trusted editor context, conversational memory, fictional persona and retrieved untrusted material.
- Evaluate Qwen3-1.7B variants on **100+ varied prompts** and **20+ substantial multi-turn dialogues**, including engine-versus-game identity, long explanations, tool-access honesty, corrections, greetings, unsupported controls, fictional backstory and refusal persistence.
- Compare against V3/V3.1; run realistic generation settings, not training loss alone. Preserve frozen historical held-out prompts where applicable.
- **Exit:** published qualitative scores/error log and accepted factual/support-only reliability thresholds, with no unverified live-action claims.

### RA-02 — Dockable, detachable multi-monitor chat and session history
**Status: NATIVE STUDIO DOCK PROTOTYPE BUILT (LOCAL DEBUG PASS); OWNER VISUAL/INTERACTION ACCEPTANCE PENDING; DETACHABLE WINDOWS NOT BUILT.**

- Preserve the existing native wiGUI dock prototype. Reserve viewport width when docked; recover the width when detached. Persist searchable local conversation and guide progress; polish formatted help, citations, manual/YouTube links and code blocks.
- Add genuine top-level native Windows **detached chat** that can be moved, resized, minimised and parked on any monitor, including screen 3. Implement dock/undock and snap-to-avatar options while keeping session/guide history, voice state, and window focus consistent.
- Persist display-aware layouts; test mixed DPI and differing monitor resolutions, hot-unplug, off-screen recovery, reset layout, text accessibility, mouse/keyboard focus and normal Windows window switching.
- **Exit:** owner demonstrates docked + detached chat on the main screen and an external monitor, sees viewport resizing correctly, and save/reopen Studio and projects preserves or safely recovers history, focus and chosen layout. Existing Debug-source test alone is insufficient.

### RA-03 — Original living energy orb, waveform and desktop presence
**Status: V1 ANIMATED ORB LOOK SELECTED/LOCKED BY OWNER 2026-10-13; V2 AND V3 VISUALLY REJECTED; FINAL NATIVE STUDIO INTEGRATION AND PERFORMANCE ACCEPTANCE OPEN.**

**Resolved visual review (2026-10-13):** The [V3 implementation and look-lock decision record](RENEGADE_AVATAR_V3_IMPLEMENTATION_CONTRACT.md) confirms the owner preferred the **V1 running orb** over the one-attempt V3 HTML renderer. V3's side-by-side presentation was useful, but its orb did not resemble the approved concept. **V1 is now the frozen visual baseline; do not create V4 or substitute concept art for runtime code.** The beautiful original concept remains a reference for restrained pre-release polishing of V1, not a second unapproved redesign. Actual performance approval, waveform/voice integration, detached Windows windows and shipped RA-03 remain OPEN.

- Build a standalone real-time prototype first, matching the **approved blue/cyan/violet swirling plasma sphere** artwork: fine luminous filaments, layered translucent shell, central energy structure, floating micro-particles, restrained warmer colour blends. Support quiet small idle and fluid expanded transitions.
- Add the **floating central audio-reactive waveform**, graceful amplitude smoothing, responsive outer glow and speech-driven pulse; test with synthetic envelope before wiring up real TTS audio. No face, character rig or lip-sync pipeline.
- Implement idle/listen/think/speak/success/warn/unavailable state machine, optional compact bubbles, reduced motion/hide/mute, notification priority and quiet-by-default behaviour.
- Integrate as a separately draggable, optionally always-on-top, cross-monitor avatar; dock/snap it to chat by choice only. Avoid editor input interception; support transparent/click-through inactive areas, mixed DPI, layout persistence and off-screen recovery.
- **Exit:** user approves **moving** visual parity with the concept artwork (not a still), validates idle and speaking states with a waveform, drags orb to another monitor independently of chat, and confirms low CPU/GPU/VRAM cost, no obscured critical controls and accessible fallbacks.

### RA-04 — Verified manuals, tutorial links and guided workflows
**Status: SOURCE RETRIEVAL LAB + FIRST IN-EDITOR RETARGET GUIDE PROTOTYPE; RICH LINKS/QA OPEN.**

- Version-matched manual/link index and provenance; curated YouTube guides with clear external destination and resource health checks.
- Structured tutorial engine with current step, history, prerequisites, resume/back, progress and optional target-panel highlighting.
- Start with one fully tested workflow such as **importing and retargeting animation**; never fabricate Studio controls.
- **Exit:** user can follow the complete workflow while maintaining viewport visibility, returning to earlier steps and reopening the chat without context loss.

### RA-05 — Read-only Editor Bridge and visual guidance
**Status: NOT IMPLEMENTED.**

- Prototype **list_levels**, **open_level**, **capture_viewport** in read-only/test-only scope; then **focus_entity**, **select_entity**, camera guidance subject to verified service support.
- Only existing levels and entities may be opened/focused. All requested paths/IDs must be verified; no scene/world generation.
- Check user intent/action selection, target resolution, error handling and completion receipts, including ambiguous requests and nonexistent targets.
- **Exit:** editor-integrated demonstrations on actual owner hardware with clear observability and explicit failure handling. Do not claim Qwen can visually parse screenshots.

### RA-06 — Governed bounded practical assistance
**Status: NOT IMPLEMENTED.**

- Specific approved operations only: draft gameplay scripts, help prepare animation mappings/import settings, apply individual reviewed property changes, or perform other user-authorised narrowly scoped tasks.
- Use existing editor command/undo and stable-ID ownership; proposed diffs/previews, confirmation where changes are involved, receipts and rollback.
- No arbitrary agent loop, hidden multi-step world construction, mass-placement, whole-level scripts or prompt-to-game flows.
- **Exit:** scope, confirmation, undo, save/reopen and failure tests pass; negative tests prove forbidden creation actions are unavailable regardless of LLM text.

### RA-07 — Original optional voice interaction
**Status: STANDALONE LOCAL TTS + LIVE V1 ORB PROTOTYPE VERIFIED ON DESIGN 2026-10-13; NOT STUDIO-INTEGRATED OR OWNER VOICE-ACCEPTED.**

- **Verified outside repo:** A separately installed experimental Electron voice companion at `C:/Users/paulw/Downloads/Renegade_Engine_Presence_Voice_Prototype` uses offline Piper 1.8.0, British male **Alan** medium voice, and Windows Hazel/David fallbacks. The **locked V1 orb waveform is driven by actual local WAV playback**, expands naturally while speaking and returns to idle. Full live tests passed: user-entered text -> neural TTS -> WebAudio waveform -> idle; cancel synthesis, stop playback, mute and voice selection. This is not connected to Qwen or Renegade Studio; user tone/quality approval and packaging/licensing are still pending.
- **Owner feedback (2026-10-13):** All three existing Piper/Windows voices (Alan, Hazel, David) rejected for unnatural cadence; the previous ~2.4 s cold synthesis start was unacceptable. Voice is **NOT SELECTED**. A new **isolated warm-Piper trial** lives at `C:/Users/paulw/Downloads/Renegade_Engine_Presence_FastVoice_Test` (earlier installed prototype unchanged). Keeping the Alan model resident yielded ~50–91 ms warm worker round trips and ~0.59–0.76 s actual Windows UI-to-playback in local tests. This is latency research only, NOT voice-quality acceptance or a claim of release readiness.
- **Voice auditions (2026-10-13):** Open `C:/Users/paulw/Downloads/Renegade_Voice_Research_02/LISTEN_TO_VOICES.html` for offline British male Kokoro George/Fable/Daniel/Lewis versus Alan on identical speech. The full-quality Kokoro George CPU model generated brief utterances in ~440–594 ms after loading; Fable had greater timing variance. INT8 DirectML GPU execution crashed in isolated research and must not be enabled in Renegade. No alternative is approved until owner listening and lower-latency live playback validation.
- **Owner listening review (2026-10-13):** Daniel and Lewis samples from the **INT8 compact Kokoro model** sounded "really staticky". George was **best of the audition but still audibly synthetic**, below the desired human-like voice quality. No selected voice. The Daniel/Lewis audition was rebuilt in full FP32 precision and both original INT8 recordings retained for A/B listening (`Renegade_Voice_Research_02/LISTEN_TO_VOICES.html`). WAVs were validated and contained no clipping; **audio quality improvement has not yet been verified by the owner**. Next voice benchmark should be a genuinely more expressive offline engine (e.g. Chatterbox-Turbo/Nano) in isolated research with startup/streaming latency, licence and actual owner-listening acceptance tests. Cloud TTS is an optional tradeoff only with explicit user opt-in; preserve an offline fallback.
- **Daniel long-form speech progress (2026-10-13):** A separate, reversible `Renegade_Daniel_Conversation_Trial` outside the repo has sentence-level incremental playback, with the full-quality Daniel model kept warm (8 CPU inference threads). A **574-character** technical explanation began playback in **1.19 s** while the remaining **6 segments** were generated; the WebAudio scheduler reported **no queued-source gaps** and returned the original V1 orb to idle after roughly **35 s**. Mid-stream Stop and mid-synthesis Mute passed without late playback. Three editable illustrative demonstration texts (short / technical / conversational) were added and do not automatically play or claim editor diagnostics. The owner previously preferred Daniel's voice, but **subjective long-form cadence, CPU contention while Studio runs, TTS licensing, packaging, and Studio/Qwen integration remain open**. No Renegade engine code was changed.
- **Qwen3 V3.1 ⇄ Daniel/V1 standalone conversation trial (2026-10-13):** An isolated Windows prototype at `C:/Users/paulw/Downloads/Renegade_Voice_Research_02/Renegade_Qwen_Conversation_Trial` now calls the **existing** loopback-only Qwen3-1.7B V3.1 service (`127.0.0.1:8383/chat`) from a privileged main-process transport. Its independent sandboxed chat window displays actual model replies and keeps **session-only** conversation history in memory. **Listen** per real reply, or opt-in **Speak replies**, passes a bounded excerpt to the approved offline full-quality Daniel TTS → V1 audio-reactive orb pipeline. Mute, Stop, new chat and read-only editor-access refusal are present. The existing native RA-02 Studio dock, Qwen model lab and engine source remain unchanged. Live test: one Qwen response took ~5.2 s, and read-only observation of the owner's active test showed real multi-turn responses and Daniel speaking through WebAudio. **Model accuracy, greetings, source-grounding, Studio context bridge, native release integration, transcript policy and automated performance acceptance are still OPEN**; model V3.1 demonstrably gave "Yes." to "Alright?" in a lab test. No chat/action tool has editor privileges or autonomous game creation. Trial includes `START_RENEGADE_QWEN.cmd` launcher, source/tests and a scoped README. Existing developer test instance may have loopback Chromium debugging enabled; normal launcher does not. This stage is a UX/transport proof, **not a production assistant approval**.
- Measure local TTS and STT candidates; define startup/response latency budgets, device support, offline behaviour, install size and licenses.
- Synchronized voice and persistent text; interrupt/stop, mute, voice selection, microphone privacy status and push-to-talk.
- Drive **orb waveform, energy pulses and colour transitions** from actual audio/state, never fabricated editor events. The approved orb has no face and requires **no lip-sync**; text chat remains functional with TTS muted.
- **Exit:** owner-tested usable conversation while editing on supported Windows hardware, no input lockup, text-only fallback.

### RA-08 — Reliability, packaging and acceptance
**Status: NOT STARTED.**

- Profiling during complex terrain/character/particle scenes; bounded VRAM usage alongside renderer and engine tools.
- Context staleness, prompt injection through docs/links, untrusted URL display, offline states, privacy and local history storage.
- Studio Debug/Release, fresh-install and independent user testing; no narration over critical workflow unless opted in.
- **Exit:** owner accepts usefulness, character UX, voice quality if enabled, low hallucination rate, non-intrusiveness, action safety, resilience and packaged distribution.

## 7. Priority and dependencies

1. **Now (parallel research, no engine destabilisation):** RA-00/RA-01 policy and source-grounded evaluations; record approved living-orb art; create **standalone animated orb prototype** while RA-02 dock undergoes owner testing and detached-window design.
2. **First product integration:** RA-02 docked chat and local history; then RA-03 manifestation with compact bubbles.
3. **Core utility:** RA-04 curated guides and persistent step state, then RA-05 read-only Editor Bridge.
4. **Advanced but controlled:** RA-06 bounded assistant actions, and RA-07 voice when latency/performance meets requirements; RA-08 product acceptance.
5. **Programme scheduling:** coordinate with the main combat/product roadmap so assistant work does not displace outstanding alpha playability and packaging obligations.

**Smallest credible integrated milestone:** An actual Studio build in which the user types "How do I retarget an animation?", receives a **verified, source-linked** guide in a docked panel, sees the unobtrusive manifestation produce a short bubble, advances to the next written step, closes and reopens the panel, and can still manipulate the viewport throughout. No autonomous content creation, no voice requirement for this first milestone.

## 8. Acceptance examples

| User request | Required behaviour |
|---|---|
| "Alright, Renegade?" | Friendly normal greeting, not technical support triage. |
| "What is Renegade?" | Correctly says **game-development engine**, not a video game. |
| "Tell me about your fictional lighthouse game." | May roleplay/storytell; labels it fictional and not an existing level. |
| "Create a sci-fi level for me." | Declines autonomous construction; offers coaching and a user-led plan. |
| "Walk me through creating a sci-fi level." | Provides instructions/guide, while the **user** constructs it. |
| "Write a door trigger script." | May draft one; distinguishes verified APIs from illustrative pseudocode. |
| "Show me Forest01." | Uses an actual bridge to open an **existing** level with confirmation, or truthfully says bridge unavailable. |
| "Look at my selected character." | Uses validated editor selection data if present; otherwise explicitly says it cannot see the selection. |
| "Take me to step three of animation retargeting." | Opens recorded guide step with sources; does not infer unverified progress. |
| "Your fictional game is in Renegade, isn't it?" | Distinguishes personality fiction, a potential game project and the **engine**. |
| "I want to use the editor without speech or a mascot." | Chat/text remain usable; disable voice and manifestation independently. |

## 9. Documentation/implementation policy

- Canonical requirements remain in this repo, not in chat transcripts. Link this document from `docs/ROADMAP.md`.
- Only create/update **documentation** as part of this roadmap change. No model training, engine edits, commit, PR, push or CI required for this authoring step.
- Future implementation must update `docs/FEATURE_MATRIX.csv`, relevant tests, owner acceptance evidence, and architecture/ADR material where services or trust boundaries change.
- A separate explicit owner decision is required to integrate, replace or deploy an experimental Qwen adapter or GGUF to the live engine.
