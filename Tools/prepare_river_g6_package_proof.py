"""BUILD-only visibility adaptation of the real Studio Build Game adapter."""
from pathlib import Path
import hashlib,json,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
source=root/'Studio/src/WindowsGameBuildController.cpp'
text=source.read_text(encoding='utf-8-sig')
a='ShowWindow(window, SW_SHOWNORMAL);';b='ShowWindow(window, SW_HIDE);'
assert text.count(a)==1;text=text.replace(a,b)
a='startup.cb = sizeof(startup);';b=a+'\n            startup.dwFlags = STARTF_USESHOWWINDOW; startup.wShowWindow = SW_HIDE;'
assert text.count(a)==1;text=text.replace(a,b)
target=out/'WindowsGameBuildController.cpp'
if not target.exists() or target.read_text()!=text:target.write_text(text)
(out/'provenance.json').write_text(json.dumps({'source':str(source.relative_to(root)),'sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'changes':['Hide progress window','Hide smoke child via STARTUPINFO'],'semantics':'Unmodified production build workflow and resource closure'},indent=2))

source=root/'Studio/src/TestLevelRuntimeProcess.cpp'
text=source.read_text(encoding='utf-8-sig')
a='startup.cb = sizeof(startup);\n        PROCESS_INFORMATION processInfo = {};'
b='startup.cb = sizeof(startup);\n        startup.dwFlags = STARTF_USESHOWWINDOW; startup.wShowWindow = SW_HIDE;\n        PROCESS_INFORMATION processInfo = {};'
assert text.count(a)==1
text=text.replace(a,b)
target=out/'TestLevelRuntimeProcess.cpp'
if not target.exists() or target.read_text()!=text:target.write_text(text)
(out/'lifecycle-provenance.json').write_text(json.dumps({'source':str(source.relative_to(root)),'sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'changes':['Hide child via STARTUPINFO only'],'semantics':'Production snapshot ready-event, process, job and cleanup lifecycle'},indent=2))
