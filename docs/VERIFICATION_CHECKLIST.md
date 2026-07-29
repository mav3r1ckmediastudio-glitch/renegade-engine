# Verification Checklist

## Identification

- [ ] Record repository, branch, and exact commit.
- [ ] Confirm the requested acceptance criteria.
- [ ] Confirm the verifier did not implement the reviewed change.

## Repository integrity

- [ ] Recursive clone succeeds.
- [ ] `WickedEngine` resolves to the SHA in `docs/UPSTREAM_SYNC.md`.
- [ ] No unrelated files or generated artefacts are committed.
- [ ] No secrets or machine-specific absolute paths are committed.
- [ ] Wicked source changes, if any, have an approved patch record.

## Build and automated checks

- [ ] Run documented configuration and build commands.
- [ ] Run relevant automated tests.
- [ ] Record machine and toolchain versions.
- [ ] Record failures, warnings, and skipped checks.

## Behavioural checks

- [ ] Run the affected workflow.
- [ ] Save, close, and reopen serialized state when relevant.
- [ ] Run standalone output when gameplay/runtime behaviour is affected.
- [ ] Compare parity claims with the pinned Wicked Editor.

## Visual checks

- [ ] Inspect affected rendering/UI on intended hardware.
- [ ] Compare expected and observed screenshots or captures.
- [ ] Treat visual failure as failure even when automated checks pass.

## Result

Choose one:

- [ ] PASS
- [ ] PASS WITH LIMITATIONS
- [ ] FAIL

Record evidence, limitations, and follow-up tasks in `HANDOFF.md`.
