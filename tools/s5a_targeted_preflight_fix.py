from pathlib import Path

path = Path("Studio/src/S4BScriptAttachmentInspector.cpp")
text = path.read_text(encoding="utf-8")
old = """                            restored = static_cast<std::size_t>(\n                                std::distance(controls.sources.begin(), selected));\n"""
new = """                            restored = static_cast<std::size_t>(\n                                selected - controls.sources.begin());\n"""
if text.count(old) != 1:
    raise SystemExit("S5A targeted preflight: chooser index seam changed")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
