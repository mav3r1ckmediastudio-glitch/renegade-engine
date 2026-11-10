# Reading the feature matrix

FEATURE_MATRIX.csv is a capability/evidence ledger, reconciled against main 618b231
on 10 November 2026. It is not a blanket feature-completion certificate.

- pinned_upstream_sha can record the upstream version used for a historical
  feature proof. The current engine pin is in UPSTREAM_SYNC.md and the gitlink.
  Do not overwrite every historical evidence pin with a newer untested value.
- verification_result records the current integration status where established,
  with remaining scope limits. Passing aggregate application CI is not a native
  visual/parity test of every component named in the row.
- “Not verified” remains valid when full native/backend/family parity is unproved.
  A merged implementation does not turn it into PASS.
- Owner acceptance, independent exact-commit verification and distribution
  approval are separate. Preserve each explicitly.
- REN-EDIT-SEL-001 describes the initial geometry/usability gate; the current
  functional scope is REN-PREFAB-FUNCTIONAL-001. There is no missing functional
  prefab task inferred from the old first-gate boundary.
- Particle preset schema 7 includes earlier fields/lifetime versions and the
  shapes/timing extension. Particle Lua includes those controls; older “no curves”
  wording is superseded.
- Original impact art/audio #182 supersedes a requirement for private KNIFE
  defaults as universal engine resources. Optional private content retains its
  own licensing restrictions.

See CURRENT_STATUS.md, the relevant subsystem guide and the scoped receipt before
making a parity, performance, release or API-completeness claim.
