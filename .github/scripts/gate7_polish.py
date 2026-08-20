from pathlib import Path

path = Path('Studio/src/RenegadeProjectLoadingOverlay.cpp')
text = path.read_text(encoding='utf-8')
old = '''        case Phase::RestoringAssets:
            if (total == 0)
                return 0.72f;
'''
new = '''        case Phase::RestoringAssets:
            if (total == 0)
                return 0.35f;
'''
if text.count(old) != 1:
    raise SystemExit(f'expected one unknown-total progress seam, found {text.count(old)}')
text = text.replace(old, new, 1)
old = '''                    << " governed resources restored from project metadata.";
'''
new = '''                    << " governed resources prepared from project metadata.";
'''
if text.count(old) != 1:
    raise SystemExit(f'expected one governed-resource detail seam, found {text.count(old)}')
text = text.replace(old, new, 1)
path.write_text(text, encoding='utf-8')
print('Gate 7 progress polish applied')
