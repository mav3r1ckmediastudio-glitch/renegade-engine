from pathlib import Path

p = Path('Studio/src/RenegadeProjectHub.cpp')
s = p.read_text(encoding='utf-8')

def one(old, new, label):
    global s
    count = s.count(old)
    if count != 1:
        raise SystemExit(f'{label}: expected 1 match, got {count}')
    s = s.replace(old, new, 1)

one(
    'const XMFLOAT4 LowerNew = XMFLOAT4(464.0f, 606.0f, 730.0f, 765.0f);',
    'const XMFLOAT4 LowerNew = XMFLOAT4(464.0f, 606.0f, 1155.0f, 765.0f);',
    'full-width new project tile')

start = s.find('        const std::array<XMFLOAT4, 3> recentRects = {LowerRecent0, LowerRecent1, LowerRecent2};')
end_anchor = '        mask(1191.0f, 383.0f, 287.0f, 286.0f, wi::Color(5, 14, 19, 250));'
end = s.find(end_anchor, start)
if start < 0 or end < 0:
    raise SystemExit('secondary recent card block not found')
s = s[:start] + s[end:]

one(
    'bordered(XMFLOAT4(sx(560.0f), sy(337.0f), sx(1112.0f), sy(554.0f)), SurfaceRaised, CyanSoft);',
    'bordered(XMFLOAT4(560.0f, 337.0f, 1112.0f, 554.0f), SurfaceRaised, CyanSoft);',
    'modal coordinate scaling')

one(
    'text("NEW PROJECT", 625.0f, 653.0f, 18, TextStrong, 1.2f, 0.13f);\n        text("CREATE A NEW RENEGADE PROJECT", 625.0f, 684.0f, 8, Muted, 0.6f, 0.10f);',
    'text("NEW PROJECT", 637.0f, 653.0f, 18, TextStrong, 1.2f, 0.13f);\n        text("CREATE A NEW RENEGADE PROJECT", 637.0f, 684.0f, 8, Muted, 0.6f, 0.10f);',
    'new project tile typography')

p.write_text(s, encoding='utf-8')

for temporary in [
    Path('.github/workflows/pr58-gate3-visual-correction.yml'),
    Path('Tools/pr58_gate3_visual_correction.py'),
]:
    if temporary.exists():
        temporary.unlink()
