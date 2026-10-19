# RA-01 Knowledge Prototype — Handoff

Date: 2026-10-13
Repository working branch: feature/p3-original-impact-vfx
Current HEAD at implementation: 60634cec26feb7ffc2d0f941d3381865fa3b41c7
Commit/PR/push: NONE. No existing tracked source, worktree changes, or collaborator changes overwritten.

## Changed (untracked addition only)
- Tools/assistant_knowledge/knowledge.py
- Tools/assistant_knowledge/facts.json
- Tools/assistant_knowledge/legacy_guides.json (copy of A1's pinned, six source-checked guides, explicitly historical)
- Tools/assistant_knowledge/README.md
- Tools/assistant_knowledge/HANDOFF.md
- Tests/assistant_knowledge/test_knowledge.py
- Generated BUILD/assistant_knowledge.sqlite (outside source tracking)

## Commands and evidence
- `C:\Python314\python.exe Tools/assistant_knowledge/knowledge.py build --repo <repo-root>`: PASS. 195 docs, 7520 passages, 6 historical guides, 5 fact cards source-verified.
- `C:\Python314\python.exe Tools/assistant_knowledge/knowledge.py ask --repo <repo-root> 'How do I make my window use glass impacts?'`: PASS. Returned source-verified surface card, reference-only historic notes separated.
- `C:\Python314\python.exe Tools/assistant_knowledge/knowledge.py ask --repo <repo-root> --strict 'Can you inspect my selected object?'`: PASS. Source-backed SelectionService fact with clear no-bridge limitation.
- `C:\Python314\python.exe -m unittest discover -s Tests/assistant_knowledge -p test_knowledge.py -v`: PASS (8 tests; 1.165 sec).

## Risks and open work
- The source checkout contains unrelated dirty files; this work deliberately was not committed to the active projectile feature branch.
- Source-literal matching is not a substitute for UI/runtime acceptance. The initial facts are narrow.
- Qwen and native Studio dock are NOT connected. The knowledge module is a CLI prototype, not a shipped feature.
- Data is version-sensitive; rebuilding on clean reviewed integration commit and independent QA is required.
- Retest with actual project queries, source line anchors and abstention quality before exposing to end users.

Next: revalidate historical guides against an exact intended build, expand curated facts with native evidence, integrate read-only retrieval into the assistant orchestrator, then run owner acceptance.
