# Static model import verification — 14 October 2026

Current source inspected: native Add menu entry, GLB/FBX file picker, isolated candidate preview, Asset name, Folder/Import to, role/tags, IMPORT ASSET and committed-card reveal/failure distinction. Historical mouse-driven evidence recorded in docs/MODEL_IMPORTER_REBUILD.md and docs/MODEL_IMPORT_DESTINATIONS.md; custom-folder native evidence also inspected in BUILD/import-folder-handoff.md.

Native proof executable: BUILD/renegade/Release/RenegadeModelImporterRebuildGraphicsProof.exe, run from its directory. Arguments: Tests/fixtures/Importer/static_triangle.glb, external_uri_triangle.glb and disposable BUILD/assistant-import-proof-20261014. Result exit 0, contrasting preview pixels 13169/4186, placement/Undo/Redo/save/reopen PASS; asset a47cb2ef-c800-4ae2-9cd7-b5452b3726df.

Second run: static_textured_cube.fbx, external_uri_triangle.glb and BUILD/assistant-import-fbx-proof-20261014. Result exit 0, preview pixels 37617/35456; FBX MISSING/CHANGED TEXTURE REJECTION PASS; RETAINED REIMPORT CLIPS/ACTIONS PASS; placement/Undo/Redo/save/reopen PASS; asset d9494f7c-8657-411c-9c0d-b26bef0eb8cb.

Executable SHA-256: 9d1a1d0af4a4c66baaf1933fc711258f3fd70984db44eced8d81169da5fb43e2

These runs use generated fixtures and existing native binaries, not a newly rebuilt editor or fresh beginner mouse-driven test. No user's project/scene modified, no UI edits and no automatic asset/scene creation by the Living Core. They do not prove arbitrary FBX packs, character retargeting or native picker support for GLTF/OBJ/BLEND.
