"""Ensure Renegade owner-authored impact sounds ship as valid single-shot core defaults."""
from pathlib import Path
import hashlib
import struct
import sys
import wave

root=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[1]
folder=root/"Runtime/Content/Audio/Impacts/Original"
rc=(root/"Runtime/RuntimeImpactTextures.rc.in").read_text(encoding="utf-8")
cpp=(root/"EngineBridge/src/ImpactAudioService.cpp").read_text(encoding="utf-8")
hdr=(root/"EngineBridge/include/renegade/bridge/ImpactAudioService.h").read_text(encoding="utf-8")
runtime=(root/"Runtime/src/RuntimeApplication.cpp").read_text(encoding="utf-8")
doc=(folder/"README.md").read_text(encoding="utf-8")
files=[
(7400,"bullet_hitting_metal.wav",1),
(7401,"bullet_hitting_metal2.wav",1),
(7402,"bullet_hitting_wood.wav",2),
(7403,"bullet_hitting_wood2.wav",2),
(7404,"bullet_hitting_concrete.wav",3),
(7405,"bullet_hitting_concrete2.wav",3),
(7406,"bullet_hitting_rock.wav",4),
(7407,"bullet_hitting_stone2.wav",4),
(7408,"bullet_hitting_dirt.wav",5),
(7409,"bullet_hitting_dirt2.wav",5),
(7410,"bullet_hitting_glass.wav",6),
(7411,"bullet_hitting_glass2.wav",6),
(7412,"bullet_hitting_water.wav",7),
(7413,"bullet_hitting_water2.wav",7),
]
assert len(list(folder.glob("*.wav")))==14
for number,name,surface in files:
 path=folder/name
 data=path.read_bytes()
 with wave.open(str(path),"rb") as w:
  assert (w.getframerate(),w.getsampwidth(),w.getnchannels())==(48000,2,2),name
  seconds=w.getnframes()/w.getframerate()
  assert .9<seconds<=10,name
  if surface==6:assert seconds<=1.31,name
  if surface==7:assert seconds<=2.03,name
 assert hashlib.sha256(data).hexdigest() in doc,name
 assert f'{number} RCDATA "@CMAKE_SOURCE_DIR@/Runtime/Content/Audio/Impacts/Original/{name}"' in rc,name
 assert f'{{{surface},{number},' in cpp,name
assert "if (!bank.surfaces[item.surface].empty()) continue;" in cpp
assert "useCoreDefaults = false" in hdr
assert "impactAudioError, true))" in runtime
print("Owner-authored Renegade impact SFX: PASS (14 PCM WAVs, 4 trimmed recordings, hashes, built-in resource routes, bank override)")
