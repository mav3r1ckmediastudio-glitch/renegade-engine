# Original Renegade core impact audio (2026-10-09)

The owner created all 14 recordings. They are not Unity KNIFE assets. The distribution copy is licensed by the owner for Renegade's own default library.
The original recordings in Downloads remain unchanged. The glass WAVs originally contained three separate impacts; the distribution versions preserve only the first event with a short terminal fade.
Water WAVs retain the first/main splash with the quieter tail, removing late material that risks sounding like a second splash. No other WAV was modified.

Each recording is a WAV PCM16 stereo 48kHz, embedded as a Windows RCDATA resource within RenegadeRuntime; per-project governed bank entries always override the corresponding core surface.
Surfaces: Metal, Wood, Concrete, Stone, Dirt, Glass, Water, two different recordings each. No arbitrary fallback sound is assigned to Default or Character.

| Recording | Surface | Clip | Resource ID | Bytes | SHA256 |
|---|---|---:|---:|---:|---|
| bullet_hitting_metal.wav | Metal | 1 | 7400 | 384078 | a2a945cbed5e980a56bb938c4d624246258a6cf9fc2e8b3e11cac73226f37fbf |
| bullet_hitting_metal2.wav | Metal | 2 | 7401 | 860238 | 7aab7d3fcc97064180a7535b87e3c2ba77438b02a97e758bad5d725a6c8706ca |
| bullet_hitting_wood.wav | Wood | 1 | 7402 | 860238 | 551f4cf38c359f887e55c7ba87440881e0f1f56457acae2defbb461f0102a227 |
| bullet_hitting_wood2.wav | Wood | 2 | 7403 | 860238 | bcd94abcea43e3e2852fb82ecab5a31baa1dd5a9ea0e671becc19561e9029312 |
| bullet_hitting_concrete.wav | Concrete | 1 | 7404 | 860238 | 1af37a058017cdb1548ccef6593849ad13dd0306938c86f823c95303456bec7f |
| bullet_hitting_concrete2.wav | Concrete | 2 | 7405 | 860238 | 1f291f527d175e468a02b4351cf395b6e68de0657443e56290e81b2f32a91820 |
| bullet_hitting_rock.wav | Stone | 1 | 7406 | 860238 | fb821674cc28bbf1971064d5b5a145edb6a354423da4e896142e1bf9d903fc91 |
| bullet_hitting_stone2.wav | Stone | 2 | 7407 | 860238 | 3cc18265eb2bcdb284acde45873a19855e4786ee20065b7fdc36e26d929c4e69 |
| bullet_hitting_dirt.wav | Dirt | 1 | 7408 | 860238 | d2f1045de1d93ef5dcde4c31f7f5e5377447672909b17b6d08aa99d4a4361d0a |
| bullet_hitting_dirt2.wav | Dirt | 2 | 7409 | 860238 | 7574b5d0b20ae55c8ccf5a0e254f48872368ebd92decdb622593b575a3fa53e9 |
| bullet_hitting_glass.wav | Glass | 1 | 7410 | 249644 | f37c132f0f654ed6fbb5f9a9da8e67a19047c6f0c7edf63242c7119a9a799562 |
| bullet_hitting_glass2.wav | Glass | 2 | 7411 | 249644 | ba5e054c3e32f8fd33f328294165cb88133957916117dfeed133f306bc832a85 |
| bullet_hitting_water.wav | Water | 1 | 7412 | 311084 | df06b38dfbde1aa9b9d85c5c6cf05d4c4e3b9bf6092dc7f533e1591dd4800f18 |
| bullet_hitting_water2.wav | Water | 2 | 7413 | 387884 | 3ed66751a3a7a2b7dd9f095a64c5618d494b6bcbac2d100ac2f3614a5453e32b |

## Project library compatibility
Existing projects with a project-authored impact audio bank retain their own recordings, even if those are older glass or water versions. To update such a project, reimport corrected WAVs through the governed audio asset importer and rebind the audio bank. Never patch .rasset files directly.
New projects with no bank use this embedded default library. The package needs no downloaded WAVs.
