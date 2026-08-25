# Renegade Story Flow — Gate 10 Runtime/Build/Standalone Closeout

**Status:** active implementation and verification

**Gate 10 baseline:** `1243a5da6f661e624a4672df47b8cdf12288f02f`
(`Story Flow Gate 9: Journey recovery and UX closeout (#100)`).

**Implementation branch:** `feature/story-flow-gate10-closeout`

**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## 1. Purpose

Gate 10 is the final Story Flow programme closeout. It does not add another
Journey UI redesign. It proves that the creator-facing Story Flow authored in
Studio is the same semantic Flow that survives persistence, reaches Runtime,
feeds the Windows build closure and executes in the promoted standalone game.

The locked end-to-end product proof remains:

`Hub -> Story Flow -> Screens/Levels -> Save -> Close -> Reopen -> Runtime -> Build Windows Game -> standalone`

Gate 9 visual/interaction acceptance is already complete and is not repeated as
Gate 10 work.

## 2. One consolidated owner acceptance

Gate 10 is deliberately **not** split into multiple owner-facing build gates.
The persistence, identity, parity, dependency and transaction requirements are
proved through existing deterministic tests plus the integrated Gate 10 build
regression and exact-head CI.

The project owner receives **one final packaged Release candidate** after the
automated proof is green. Another owner build is required only if a real defect
changes behaviour that cannot be established by automated proof.

## 3. Existing proof retained

Gate 10 reuses accepted tests rather than cloning their coverage:

- `RenegadeStoryFlowGate9ENonlinearAuthoringTests` — nonlinear branch, merge and
  loop authoring; shared stable route identities; Undo/Redo; save/reopen parity;
- `RenegadeStoryFlowPresentationTests` — presentation/layout persistence and
  proof that layout changes do not mutate Runtime Flow semantics;
- `RenegadeRuntimeFlowTests` — Runtime traversal, stable-ID moved-content
  resolution and structured missing/ambiguous route failures;
- Story Flow Level/Screen reference tests — stable document identity remains
  authoritative while paths are repairable hints;
- `RenegadeStoryFlowGate8EOutcomeParityTests` — Screen actions remain symbolic,
  Story Flow owns destinations, mismatches fail closed and repaired state
  survives save/reopen;
- `RenegadeProjectDocumentTransactionTests` and project transaction regressions
  — failure injection, rollback/recovery and no half-written multi-document
  state;
- LP05/LP06 dependency/build/package regressions — deterministic dependency
  closure, governed identity companions, package integrity and promoted
  standalone verification.

Gate 10 does not require the owner to manually reproduce those isolated tests.

## 4. Build/Runtime disagreement found by Gate 10

The accepted editor and Runtime support first-class Screen Flow destinations,
but the pre-Gate-10 Windows build smoke still carried an LP06-era assumption:

- it required a legacy project-level startup Screen;
- after Game Start it required every nonterminal Flow destination to be a Level;
- it fabricated repeated `level.complete` outcomes until Complete Game;
- the standalone smoke command was driven by a Level-completion count rather
  than the authored Flow's actual outcome sequence.

That made a valid modern Flow-native project such as
`Game Start -> Title Screen -> Level -> Complete Game` reject during Windows
build preparation even though Studio and Runtime could represent the journey.
This is a genuine Gate 10 parity defect and is in scope for correction.

## 5. Gate 10 correction

Windows build preparation now derives a bounded deterministic verification path
from the authoritative startup Flow itself.

The correction:

- requires the startup Story Flow, not a legacy project-level startup Screen;
- searches authored outgoing outcomes in deterministic order;
- replays every candidate through `FlowInterpreter` under the same default state
  so unavailable/ambiguous conditional paths are rejected rather than guessed;
- accepts Level, Screen and other legal Flow destinations instead of requiring
  a Level-only chain;
- records the exact authored outcome sequence as build smoke input;
- retains the older Level completion count only as compatibility evidence for
  accepted LP06 regressions;
- passes the exact outcomes to the staged standalone as `--flow-outcome`
  arguments;
- allows the packaged Flow-native Runtime to record smoke PASS/FAIL and exit
  after the supplied outcomes reach, or fail to reach, Complete Game;
- leaves Runtime Flow semantics unchanged: this pathfinder chooses a bounded
  verification route only and does not mutate or nominate a gameplay main path.

## 6. Integrated Gate 10 regression

`RenegadeWindowsGameBuildProjectTests` retains its existing LP06 Level-only
regression and then transforms the disposable project into the current
Flow-native architecture:

1. retain the stable startup Flow;
2. clear legacy `startup_scene`, `startup_screen_id` and `startup_screen`;
3. insert the governed Runtime Screen as a first-class Story Flow destination;
4. route `Game Start -> Title Screen`;
5. route Screen `play -> Level One` and Screen `quit -> Quit`;
6. retain Level One -> Level Two -> Complete Game;
7. save and reopen the Flow-native project descriptor;
8. require Windows build preparation to derive exactly
   `play`, `level.complete`, `level.complete` for its deterministic Complete Game
   smoke path;
9. require dependency closure and the Windows package plan to contain the
   authoritative Flow, Runtime Screen, Levels and Scene identity companions.

This directly proves the build pipeline no longer depends on the superseded
project-level startup Screen/Scene compatibility fields.

## 7. Gate 10 acceptance matrix

| Requirement | Automated authority | Owner retest |
|---|---|---|
| Journey/Graph semantic identity | Gate 9E nonlinear + shared authoring tests | No repeat |
| Save/close/reopen semantic parity | Gate 9E / authoring persistence tests | Integrated final check only |
| Layout cannot alter Flow semantics | Story Flow presentation tests | No repeat |
| Stable IDs survive moved content | Runtime/reference resolution tests | No repeat |
| Screen action/route parity | Gate 8E outcome parity tests | Integrated final check only |
| Missing/ambiguous content fails closed | Runtime/reference/dependency tests | No repeat |
| Transaction rollback/recovery | Project transaction tests | No repeat |
| Screen/Level build dependency closure | Gate 10 extended Windows build regression | No repeat |
| Authored outcome sequence reaches packaged Runtime | Gate 10 build preparation + standalone smoke | One final standalone check |
| Exact-head Debug/Release build health | GitHub CI | No manual rebuild |
| Creator-facing integrated parity | packaged Release candidate | **One final owner acceptance** |

## 8. Final owner acceptance

After the exact Gate 10 head is green, provide one packaged Release candidate.
The owner should use one representative project containing at least one governed
Screen and Level in the executable Story Flow.

The final human acceptance is intentionally narrow:

1. save the representative Story Flow, close the project and reopen it;
2. confirm the same executable Screen/Level journey is still present;
3. run the project and confirm the intended main path can enter the Screen and
   Level content normally;
4. invoke **Build Windows Game once**;
5. launch the promoted standalone and confirm it follows the same authored
   journey and its expected Screen actions/Level progression;
6. confirm there is no missing-content fallback, identity substitution or
   editor/standalone disagreement.

Previously accepted Gate 9 typography, navigation, Inspector and Journey visual
behaviour are not part of this retest unless Gate 10 changes them unexpectedly.

## 9. Completion rule

Gate 10 and the Story Flow programme are complete only when:

- the integrated Gate 10 implementation and all retained relevant regressions
  pass on the exact branch head;
- authoritative Windows Debug/Release CI is green;
- the packaged standalone smoke reproduces its exact authored expected Flow
  trace;
- the project owner passes the one final packaged Release/standalone acceptance;
- any required independent exact-head review reports no unresolved blocker;
- final documentation records the accepted exact head without weakening the
  product contract.

Do not merge automatically before owner acceptance.
