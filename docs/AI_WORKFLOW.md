# AI and Human Workflow

## Principle

The repository—not an AI conversation—is the project memory. Codex, ChatGPT,
Claude, and humans work from the same committed requirements and evidence.

## Task contract

Each task must provide:

- One bounded outcome.
- Starting branch and exact commit.
- Files or subsystem in scope.
- Acceptance criteria.
- Required build, test, visual, and documentation evidence.
- Explicit exclusions.

A normal work unit should fit approximately one to three focused days.

## Implementer workflow

1. Read `AGENTS.md`, the current `HANDOFF.md`, relevant architecture/ADR files,
   and relevant feature-matrix rows.
2. Confirm the exact starting commit and working tree.
3. Implement only the bounded outcome.
4. Add or update tests and documentation with the change.
5. Run the required checks.
6. Update `HANDOFF.md` with files, commands, results, risks, and next step.
7. Identify anything not tested rather than implying success.

## Verify claims about Wicked against the pin

Every statement about how Wicked behaves must be read in `/WickedEngine` at the
pinned SHA before it is relied on. Two Phase 3 build cycles were lost to this:

- A third-party documentation site indexed an older commit and described an
  implementation, symbols, and line numbers that do not exist in the pin. See
  `docs/UPSTREAM_SYNC.md`.
- An assumption about Wicked's frame structure — that a virtual render hook
  returns with attachments still bound — was written into a code comment as
  though it were established fact, and the feature silently rendered nowhere.

An assumption recorded confidently is more dangerous than an open question. If
a claim has not been read in the pinned source, mark it unverified in
`HANDOFF.md` rather than stating it.

## Verifier workflow

Release-gate work must be checked by a different AI or human:

1. Check out the exact implementation commit.
2. Review requirements and diff without relying on the implementer's chat.
3. Run `docs/VERIFICATION_CHECKLIST.md`.
4. Record PASS, PASS WITH LIMITATIONS, or FAIL with evidence.
5. Turn failures into bounded follow-up tasks.
6. Do not hide implementation fixes inside verification unless explicitly
   reassigned as implementer.

## Handoff requirements

`HANDOFF.md` must always state:

- Exact branch and commit.
- Phase and task outcome.
- Changed files.
- Commands and observed results.
- Unverified claims.
- Known risks.
- Next bounded task.

## Conflict prevention

- `AGENTS.md` and `CLAUDE.md` point to the same canonical documents.
- Durable decisions belong in ADRs, not tool-specific prompt files.
- Do not overwrite another agent's uncommitted work.
- Re-read upstream state before external writes.
- Never commit credentials, private tokens, personal data, or hidden reasoning.
