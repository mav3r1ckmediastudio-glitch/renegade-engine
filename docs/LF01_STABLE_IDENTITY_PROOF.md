# LF01 — Stable Identity and Document Envelope

## Proof boundary

LF01 establishes the durable reference primitives required before LP02 Story
Flow. It does not implement Story Flow, gameplay state, save games, project-wide
transactions, cooking, packaging, or creator UI.

The proof owns three identities:

- **Project ID** — a generated UUID-v4 stored in the `.renegade` descriptor.
  Studio opening a legacy v1 descriptor backfills the missing ID once. Runtime
  inspection fails closed until that migration has occurred.
- **Document ID** — a generated UUID-v4 stored in a small
  `renegade-document` v1 envelope. A project-relative path is retained only as a
  diagnostic hint and may change without changing the document ID.
- **Persistent entity ID** — a generated UUID-v4 stored in Wicked's serialized
  `MetadataComponent` under `renegade.persistent_entity_id`. Runtime builds an
  ID-to-current-Wicked-entity index after scene load.

## Required behaviour

- IDs are canonical lowercase UUID-v4 values.
- Missing scene IDs are reported by validation and assigned before scene save.
- Malformed and duplicate scene IDs fail scene save with an actionable error.
- A normal duplicate receives new IDs for every newly created Wicked entity;
  Undo/Redo restores those assigned IDs.
- Project identity survives display-name and folder changes.
- Document identity survives path-hint changes.
- Entity identity survives WISCENE save/reload while Wicked remaps its runtime
  entity number.
- Duplicate document IDs and cross-project document ownership fail validation.

## Deliberate limits

The v1 document-envelope writer is a proof primitive, not the LF02 transactional
project save system. It does not provide backup, rollback, multi-document dirty
tracking, migration chains, or atomic project-wide commit. Those remain LF02.

Existing WISCENEs are not rejected on open for lacking persistent IDs. They are
made identity-complete at the next successful Save. Existing project descriptors
are migrated only by mutable Studio `OpenProject`; read-only Runtime inspection
never silently edits a project.

## Automated evidence

`RenegadeIdentityTests` proves:

1. UUID generation and malformed-value rejection.
2. Legacy project-ID backfill and persistence across folder/name changes.
3. Document-envelope round trip, retargeting, ownership, and duplicate checks.
4. Missing, malformed, and duplicate entity-ID diagnostics.
5. New identity on Duplicate with identity-preserving Undo/Redo.
6. Scene save/reload and stable-ID resolution after Wicked entity remapping.
