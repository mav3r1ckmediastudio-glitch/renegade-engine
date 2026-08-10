# LP06 Package Notice Inputs

LP06 Gate 2 treats redistribution notices as governed package inputs rather
than strings embedded in build code.

The staging service currently requires version-controlled or explicitly pinned
sources for:

- the player-facing `ReadMe.txt`;
- Renegade product licence/notice policy;
- Wicked Engine licence;
- Wicked Engine third-party notices;
- DirectX Shader Compiler licence;
- DirectX Shader Compiler third-party notices.

Each caller-supplied input carries a component label and a provenance label
beginning with `repo:` or `pinned:`. Gate 2 copies the source bytes verbatim,
hashes them, records them in the package manifest, and emits a deterministic
component inventory.

## Test-fixture rule

`Tests/fixtures/lp06_gate2/Fixture-Legal-Notice.txt` is intentionally a
placeholder. It must never be treated as satisfying commercial or public
redistribution obligations. Its only purpose is to prove the Gate 2 mechanism
without inventing or silently substituting legal text.

## Distribution boundary

A Gate 2 staging success is **not** a redistribution approval. Public packaging
still requires the real pinned notice inputs, an explicit Renegade product
licence/notice policy, creator-content licensing responsibility, and a proven
Visual C++ Runtime deployment policy. Gate 2 must fail closed when a required
notice input is absent; it does not decide whether supplied legal text is
legally sufficient.
