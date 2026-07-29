# Renegade Repository Instructions

These rules apply to Codex and other coding agents working in this repository.

## Canonical context

Read these files before changing code:

1. `README.md`
2. `docs/PROJECT_CHARTER.md`
3. `docs/ARCHITECTURE.md`
4. `docs/ROADMAP.md`
5. `HANDOFF.md`
6. The relevant rows in `docs/FEATURE_MATRIX.csv`

The repository is project memory. Do not rely on an earlier chat as the only
source for a requirement or decision.

## Scope and architecture

- Wicked Engine is pinned at the commit recorded in `docs/UPSTREAM_SYNC.md`.
- Prefer changes in `Studio`, `EngineBridge`, `Runtime`, `Tools`, or `Tests`.
- Do not modify the `WickedEngine` submodule pointer or upstream source without
  an approved upstream-sync task or documented core-patch decision.
- UI code must call stable bridge services; it must not spread direct Wicked
  internals through panels.
- Keep work bounded to one clear outcome with acceptance criteria.
- Do not silently broaden platform scope beyond Windows x64/DX12.

## Required completion evidence

- Build or test the intended configuration when the environment supports it.
- Record exact commands and results.
- Test save/reload for serialized editor state.
- Test the standalone player for gameplay-facing changes.
- Require visual inspection for visual results.
- Compare parity claims with the original Wicked Editor.
- A visual or behavioural failure overrides nominal automated success.

## Handoff

Before handing work to another AI or human:

- Update `HANDOFF.md` with the exact commit, changed files, commands, results,
  risks, and next task.
- Update `docs/FEATURE_MATRIX.csv` for any feature exposure change.
- Update architecture decisions when a durable choice changes.
- Do not mark a release gate complete until a different AI or human verifies
  the exact commit using `docs/VERIFICATION_CHECKLIST.md`.

Never put secrets, credentials, personal access tokens, or machine-specific
absolute paths in committed files.
