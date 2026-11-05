# Rocky riverbed standalone build repair, 4 November 2026

## Observed failure

Actual Studio Build Game on recovery commit 891ac9151b751fa34c1498bd419e41adaeb84f6d
rejected a maximum-expanded, Rocky-painted river scene with an outside_project
dependency on Content/terrain/riverbed/rocky_rugged/rocky_basecolor.png.
No successful packaged game or downloadable alpha is established by the earlier
four green CI jobs and native or editor tests. The failed build evidence remains
preserved in the release audit.

## Repair boundary

The Rocky base-color, DirectX normal and surface maps already belong to the
installed Studio and Runtime content. Declare these three exact resources in
the Windows game build catalogue. During detached dependency inspection and
scene cooking, rebind only the named Rocky terrain material when every original
map has the full built-in path suffix and byte identity with its declaration.
Require the complete three-map set before changing any texture reference.
Exported scene names use the package-local content root.

Do not alter the external dependency rejection policy, load GPU resources in
the detached cook, rewrite creator scene archives, change terrain authoring,
modify Wicked, merge main or distribute a public/tester release.

## Verification and provenance

WindowsGameBuildProjectTests exercises actual serialized scenes and the build
dependency collector: absent/incomplete declarations, changed bytes, unrelated
names and same-basename custom resources remain blocked or unmodified. Verified
resources use package-local cooked names; retained verified bytes support a
removed installation; dependency discovery leaves author archives byte-identical.

Actual repaired Studio Build Game, a moved standalone launch and river traversal,
current-commit CI, local native checks, fresh archive launch and independent
review remain required. These are pending at this implementation checkpoint.

Earlier successful editor workflows may be retained only as explicitly identified
evidence from their executed revision. Source-equivalence review must state the
new target revision, unchanged inputs, bounded applicability and retained
limitations. Failed or unfinished workflows cannot become passes through reuse.
The existing dirty HANDOFF and protected owner projects/backups are preserved.
