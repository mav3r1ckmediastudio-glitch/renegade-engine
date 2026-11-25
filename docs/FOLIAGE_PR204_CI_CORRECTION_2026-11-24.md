# Foliage PR 204 CI correction - 24 November 2026

Failed PR head: `b118cdcafa8e6f8828ba10466072ea94b7865884`.
GitHub-tested merge: `932b1d9fb3f67209097415eaa3cce136f256f2c6`.
Correction: `ce3f6ee6f31786c8d302ab503c8534f158c10757`.

## Hosted failure

Windows baseline run 45447646579 passed both configurations. Studio run
45447646591 compiled both configurations, then failed only
`RenegadeAssistantWorkflowSuite`: Release job 138635813252 and Debug job
138635814210. The same four subtest failures affected two Surface Type
questions, directly and through the read-only assistant bridge. Other registered
Studio tests completed successfully or were explicitly skipped by their gates.

The foliage architecture documentation changed the whole-file evidence hash
recorded for `surface-type-v1`. Its source controls, command, native acceptance
receipt and Surface Type architecture section are unchanged. Reviewed the full
architecture diff against main: only foliage sections changed. Audited every
workflow evidence fingerprint: this was the only stale digest.

## Correction

`Tools/assistant_knowledge/workflows.json` renews that single UTF-8/LF SHA-256
from `52117c3b4d99c3629face5a21d1a4b0df6d43776bf75d3196b3d8d9016b12b4b`
to `05192193d71a87a313b67112dd57c2b04f393d691e03579f10e1694f7fcf99f7`
and records the bounded review. No runtime, rendering, importer, upstream,
workflow routing, validation policy or test assertions changed.

## Local reproduction and verification

Windows Python 3.14; fresh isolated database for each suite run. Expanded the
sparse checkout to include tracked Library scripts and Runtime headers required
by the existing assistant suite; no tracked changes from materialization.

- `python Tests/AssistantWorkflowSuite.py --repo . --output <qa-output>/assistant-ci-reproduced`:
  exit 1; 84 tests, exactly the same four Surface Type subtest failures as CI.
- `python Tests/AssistantWorkflowSuite.py --repo . --output <qa-output>/assistant-ci-after`:
  exit 0; all 84 tests pass, 20.625 seconds. Includes corrupt-source rejection,
  stale-evidence abstention and line-ending/content-change validation.
- `python Tests/AssistantPackagedKnowledgeTests.py --repo . --output <qa-output>/assistant-package-after`:
  exit 0; Gitless identity, three routed answers, existing workflows, stale-index
  rejection and changed-source rejection pass.
- `git diff --check`: pass.

These checks address the observed failure. Fresh hosted Debug/Release results
are still required on the latest pushed head. No merge or release approval.
