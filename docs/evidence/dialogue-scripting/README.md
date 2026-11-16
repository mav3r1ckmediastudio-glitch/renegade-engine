# Local dialogue scripting receipt - 16 November 2026

Engine source: `bedcf30f744f2b4ce9c3432b96084573879f6199`.
Runtime Release SHA-256:
`BB650BFDCFB7C4B8786C849C690105154D83DF0E15FCE2A1CC72B8C9B127A68E`.
Isolated feature branch; no push/CI/PR/merge/release authorization was inferred.

## Automated checks

13 Release CTest suites passed in 5.28 seconds: governed Lua runtime and source
contract, new dialogue scripting integration, dialogue core/assets/save policy,
Screen runtime/renderer, Test Level snapshot and snapshot Runtime, game save slots,
native scene transition and native window surface. The portable GNU C++17 core
suite also passed 50 checks.

DialogueScriptingTests drives the production governed Lua VM and RuntimeDialogue
using saved project assets, scene metadata and EntityRefs. It covers on_start,
both conditional reply outcomes, dialogue.action back into Lua, lifecycle event
counts, missing/ambiguous conversations, bad line IDs, strict argument types,
integer limits, embedded NUL, deleted NPC liveness, memory capacity and codec
round-trip. It is headless and does not alone establish native UI acceptance.

## Separate native Runtime

A disposable copy of the owner-reviewed guard project contains a Level script.
The original project and its custom work -> captain connection were untouched.
No new audio was generated. The script starts captain by conversation name and
line ID, stops it after three game seconds, starts permit by asset/line ID, stops
it, then checks two started/two closed events and shared flag/value retention.

The final executable was launched minimised, stayed responsive for three seconds,
restored without foreground input injection, executed the script and closed
gracefully with code 0. A QA-only frame-trace setting permits the background game
to advance without taking the owner's input focus.

Observed receipt:

```text
MINIMISED_START_ALIVE
Renegade Runtime: governed creator Lua started 1 script instance(s) for the active Level.
Dialogue voice: Content/Audio/Dialogue/Guard/sael-captain.wav
Dialogue voice: Content/Audio/Dialogue/Guard/sael-permit.wav
DIALOGUE_LUA_NATIVE_PASS starts=2 closes=2 shared_state=7
NATIVE_SCRIPT_EXIT=0
NATIVE_SCRIPT_AND_MINIMISED_RESTORE_PASS
```

The terminal PASS marker is intentionally raised through Lua `error()` only
after all QA assertions pass, because the governed VM does not expose global
print. Its expected diagnostic disables only the disposable script. No unexpected
Lua diagnostic is accepted as a pass. Production examples use valid API calls.

## Failures preserved and scope

The first minimised launch at `fdcff58` crashed with access violation before a
graphics device existed. `bedcf30` adds a drawable-window guard before Run; the
final minimised/restore/native proof above passed. Two fixture-only issues were
corrected separately: the manually staged companion needed a final newline, and
the test script initially attempted unavailable global print. A transient log
sharing conflict in the proof reader was fixed by opening with read/write sharing.

The original Studio executable was locked by the owner's open editor, causing
LNK1104 in the aggregate build. A separate candidate avoids closing or replacing
that live process. `RenegadeStudio-DialogueScripting.exe` linked successfully in
the same Release folder using the existing references and a different target
name. The already accepted editor UI is unchanged by this scripting
work. This receipt covers a local standalone Runtime launch from a project,
not a newly packaged distribution, broad performance proof or independent beta
release gate. Prior UI/native receipts retain their original scope.
