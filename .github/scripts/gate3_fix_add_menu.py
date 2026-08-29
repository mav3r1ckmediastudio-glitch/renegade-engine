from pathlib import Path

chrome = Path('Studio/src/RenegadeStudioChrome.cpp')
contract = Path('Tests/Phase5Gate3SourceContract.cmake')

text = chrome.read_text(encoding='utf-8')
old_counts = '5, 4, 5, 4, 2, 3'
new_counts = '5, 4, 8, 4, 2, 3'
old_bound = 'else if (activeMenu_ == 2 && item >= 0 && item < 6)'
new_bound = 'else if (activeMenu_ == 2 && item >= 0 && item < 8)'

if text.count(old_counts) != 1:
    raise SystemExit(f'expected exactly one ADD popup count tuple, found {text.count(old_counts)}')
if text.count(old_bound) != 1:
    raise SystemExit(f'expected exactly one ADD item bound, found {text.count(old_bound)}')
text = text.replace(old_counts, new_counts, 1)
text = text.replace(old_bound, new_bound, 1)
chrome.write_text(text, encoding='utf-8')

ctext = contract.read_text(encoding='utf-8')
needle = '''    "Action::CreateDecal"\n    "Action::CreateEnvironmentProbe")'''
replacement = '''    "Action::CreateDecal"\n    "Action::CreateEnvironmentProbe"\n    "5, 4, 8, 4, 2, 3"\n    "activeMenu_ == 2 && item >= 0 && item < 8")'''
if ctext.count(needle) != 1:
    raise SystemExit('Gate 3 chrome contract insertion anchor missing or ambiguous')
ctext = ctext.replace(needle, replacement, 1)
contract.write_text(ctext, encoding='utf-8')
