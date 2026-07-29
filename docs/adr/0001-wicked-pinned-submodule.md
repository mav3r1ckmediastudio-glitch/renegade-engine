# ADR 0001: Pin Wicked Engine as a Submodule

**Status:** Accepted
**Date:** 2026-07-29

## Context

Renegade needs the complete Wicked source and history while keeping product-owned
code separate, preserving licence identity, and making phase-boundary upgrades
explicit.

## Decision

Track Wicked at `/WickedEngine` as a Git submodule pinned to an exact commit.
Use the official repository until an unavoidable core patch requires a dedicated
Wicked fork. Renegade development belongs outside the submodule by default.

## Consequences

- Recursive clone is required.
- Upstream changes cannot silently enter an active phase.
- Wicked history remains available in its own repository.
- Product code stays easy to distinguish from upstream code.
- Core changes require a separate fork and patch ledger rather than casual edits.
