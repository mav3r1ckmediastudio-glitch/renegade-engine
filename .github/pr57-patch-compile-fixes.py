from pathlib import Path


def load(path):
    return Path(path).read_text(encoding="utf-8")


def save(path, text):
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(path, old, new):
    text = load(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one target, found {count}")
    save(path, text.replace(old, new, 1))


path = "Studio/src/CreatorAssetDragPreview.cpp"
replace_once(
    path,
    '#include "renegade/bridge/CreatorAssetWorkflowService.h"\n',
    '#include "renegade/bridge/CreatorAssetWorkflowService.h"\n'
    '#include "renegade/bridge/CreatorModelImportRecipe.h"\n',
)
replace_once(
    path,
    "if (!CreatePreview(session, assetId, normalized, surface, error))",
    "if (!CreatePreview(*session, assetId, normalized, surface, error))",
)

text = load(path)
if '#include "renegade/bridge/CreatorModelImportRecipe.h"' not in text:
    raise RuntimeError("CreatorModelImportRecipe declaration include missing")
if "CreatePreview(*session, assetId" not in text:
    raise RuntimeError("StudioSession pointer is not dereferenced for preview creation")

print("PR57_COMPILE_FIXES_APPLIED")
