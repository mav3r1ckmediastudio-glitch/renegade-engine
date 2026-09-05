# ADR 0003: Versioned Renegade Project Descriptors

**Status:** Accepted
**Date:** 2026-07-30

## Context

Phase 3 needs a project identity above individual WISCENE files. The Project
Hub must be able to create, reopen, and list projects without making the UI
responsible for filesystem rules or Wicked serialization details.

The first format must be readable, versioned, project-relative, simple enough
to validate with the pinned dependency, and independent of the final public
product name.

## Decision

Use a human-readable INI descriptor with the `.renegade` extension and a
mandatory integer format version.

Version 1 contains:

```ini
format = renegade-project
version = 1

[project]
name = Example Project
startup_scene = Content/Scenes/Main.wiscene
```

The descriptor lives at the project root:

```text
Example Project/
├── Example Project.renegade
├── Content/
│   └── Scenes/
│       └── Main.wiscene
└── Saved/
```

Rules:

- `startup_scene` is project-relative and cannot escape the project root.
- WISCENE remains the native scene format.
- `ProjectService` owns create, open, validation, recent-project ordering, and
  error reporting.
- Studio state such as the recent-project list is outside the project
  descriptor.
- A project version mismatch fails clearly; future changes require migration
  handling and an ADR update.
- Asset identities, import metadata, build settings, and package profiles are
  intentionally deferred to their owning phases.

The implementation uses the pinned Wicked `wi::config::File` parser and writer.
No new third-party serialization dependency is introduced.

## Consequences

- Project Hub controls remain thin UI clients of `EngineBridge`.
- Projects are portable folders with no machine-specific absolute paths in
  their descriptor.
- The `.renegade` extension is an internal working-title format identifier and
  may require an approved migration if the final product name changes.
- Phase 3 can add scene tabs and project settings without changing WISCENE.
