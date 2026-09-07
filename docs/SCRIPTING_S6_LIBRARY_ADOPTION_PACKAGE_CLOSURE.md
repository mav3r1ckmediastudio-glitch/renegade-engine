# Renegade Scripting S6 — Library Adoption & Package Closure

Status: integrated implementation / final acceptance gate

S6 is intentionally delivered as one end-to-end scripting gate rather than six
independent PRs. Package identity is not useful without closure, closure is not
creator-facing without adoption, adoption is unsafe without update/conflict
rules, and none of it is complete until Test Level and Build Game consume the
same project-owned result.

## Creator contract

S6 adds reusable script packages to the existing ACTION, SCRIPT and GLOBAL
SCRIPT source workflow. It does not create a second scripting editor.

An installed package is immutable input. Selecting a compatible Library row and
pressing **ADD** causes Renegade to:

1. re-read and validate the package manifest and every declared Lua file;
2. resolve the selected entry's complete deterministic dependency closure;
3. copy that closure into the project's `Content/Scripts/Library/...` tree in
   one `ProjectDocumentTransaction`;
4. persist a project-owned adoption lock containing package/version, source
   identity and the adoption hash baseline;
5. re-evaluate the adopted project copy through the normal restricted S4A
   metadata path; and
6. perform the normal S4 attachment command through the shared Undo/Redo stack.

After adoption, the project copy is authoritative. Runtime execution never
searches the installed Library and never loads package files directly from it.

## S6A — package identity and provenance

The installed package manifest filename is:

`renegade-script-package.json`

Schema v1 uses:

```json
{
  "schema": "renegade-script-package",
  "schema_version": 1,
  "package_id": "renegade.example.movement",
  "package_version": "1.0.0",
  "display_name": "Movement Pack",
  "files": [
    {
      "path": "Movement.lua",
      "content_hash": "fnv1a64:0000000000000000",
      "dependencies": ["Shared/Helper.lua"]
    },
    {
      "path": "Shared/Helper.lua",
      "content_hash": "fnv1a64:0000000000000000",
      "dependencies": []
    }
  ],
  "entries": ["Movement.lua"]
}
```

The example hashes are placeholders. Real manifests must contain the exact
FNV-1a64 hash of each declared Lua file.

Package IDs are stable filesystem-safe identifiers. Source IDs are derived
deterministically from package identity plus normalized package-relative path,
so an update preserves the source identity used by script attachments.

Library-derived `ScriptSourceBinding` provenance records:

- `InstalledLibrary` provenance kind;
- package ID;
- package version; and
- immutable entry-file content hash.

The project persists its adoption baseline at:

`Content/Scripts/.renegade/library-lock.json`

The lock is update/conflict authority, not runtime immutability authority.

## S6B — deterministic closure

Package dependency edges are explicit manifest declarations. S6 never scans Lua
source text for path-looking strings and never executes gameplay Lua while
calculating package closure.

Closure resolution is deterministic, rejects missing declarations and cycles,
and produces a stable project-relative ordering. The selected entry is excluded
from its own dependency list; every transitive member becomes a governed
`ScriptModule` dependency on the resulting binding.

The project destination for an installed package member is:

`Content/Scripts/Library/<package-id>/<package-relative-path>`

This keeps the entire governed runtime closure below the already-authoritative
project scripting tree.

## S6C — transactional adoption

Before writing anything, S6 validates:

- project/manifest/entry arguments;
- manifest schema and package identity;
- path containment and traversal safety;
- duplicate/case-colliding package paths;
- declared dependency existence;
- immutable package file hashes;
- destination ownership; and
- the current project copy against its previous adoption baseline.

Package files and the updated lock are committed in one
`ProjectDocumentTransaction` using the project containment root and normal
`Intermediate/Transactions` journal location.

A pre-existing project file that is not already owned by the S6 lock is a
collision, not permission to overwrite it.

## S6D — Creator Library in Studio

`ScriptAuthoringService::EnumerateProjectSources()` continues to be the single
source catalogue used by the established S4 ACTION/SCRIPT/GLOBAL SCRIPT UI.
S6 merges compatible installed-package entries into that catalogue.

Rows are grouped using normal metadata categories:

- `LIBRARY / ...` for available installed entries;
- `LIBRARY ADOPTED / ...` for the installed version already adopted; and
- `LIBRARY UPDATE / ...` when the installed package version differs from the
  recorded project baseline.

Pressing **ADD** is the adoption boundary. There is no background package copy
and no implicit Runtime lookup.

When an adopted package update changes the binding carried by an already-live
attachment, S6 refreshes that binding using the existing
`MakeReplaceScriptSourceCommand` / `CommandService` path so Studio dirty state
and Undo/Redo remain authoritative.

## S6E — Test Level and Build Game parity

### Test Level

The existing Test Level snapshot already copies the project-owned
`Content/Scripts` tree. Adopted package members live beneath that tree, so Test
Level automatically receives the same entry and transitive module bytes without
an installed-Library dependency.

### Build Game

On a successful scene scripting save, S6 registers these project-owned files in
the project's existing typed Always Include dependency declarations:

- the scene `.rscripts` companion as `generated_data`;
- every attached script source as `script`; and
- every governed `ScriptModule` dependency as `script`.

`ProjectDependencyProvider` already turns those declarations into normal
required dependency-graph roots, so Build Game uses the existing LP05/LP06
packaging path rather than a second script-specific packager.

### Runtime

S3's governed Lua runtime remains unchanged. `package`, `io`, `os`, `debug` and
other unrestricted libraries remain unavailable. `require()` resolves only the
`ScriptModule` dependencies already declared on the attachment. S6 simply gives
that runtime the complete adopted project closure.

## S6F — updates, conflicts and diagnostics

A project-owned adopted file may be edited by the creator. That is a supported
state.

When a current adopted file differs from its recorded baseline, S6:

- leaves the creator's bytes untouched;
- continues to expose the project copy as project authority;
- reports `S6_LIBRARY_LOCAL_CONFLICT`; and
- refuses an installed package update that would overwrite those bytes.

A clean project copy may be updated transactionally to a different installed
package version. The deterministic source ID remains stable while version,
content-hash provenance and flattened dependency closure move to the selected
package version.

Malformed packages remain non-fatal to catalogue enumeration: they produce
structured S6 diagnostics and are not offered as adoptable rows. Missing package
files, hash mismatch, dependency cycles and destination collisions fail closed.

## Installed Library location

For development/portable installs the installed script root can be overridden
with:

`RENEGADE_SCRIPT_LIBRARY`

Without an override S6 looks for `Library/Scripts` beneath the current Renegade
process working directory. An absent Library root is valid and simply produces
no Library rows.

## Final integrated acceptance

S6 is accepted only when one final branch CI proves the integrated stack:

1. package schema/hash validation;
2. deterministic transitive closure and cycle rejection;
3. transactional first-use adoption;
4. durable reopen/adopted-state detection;
5. clean package update with stable source identity;
6. creator-modified project copy blocks overwrite and preserves exact bytes;
7. unowned project destination collision fails closed;
8. existing ACTION/SCRIPT/GLOBAL SCRIPT authoring paths compile with Library
   adoption integrated;
9. Test Level still uses the project-owned script snapshot;
10. Build Game receives `.rscripts`, script and module roots through the normal
    dependency graph; and
11. the governed S3 Runtime retains declared-module-only `require()` semantics.

Owner acceptance then verifies the visible Creator Library rows, ADD/adoption,
Test Level behavior and packaged Build Game behavior from the final artifact.
