# G13B original VFX artwork provenance
2026-10-26. No SHADERSOURCE/Unreal artwork was copied or used as image-generation input.

AeratedWater.png and ImpactSplash.png: newly generated using the built-in image_gen tool, transparent output requested and generated alpha preserved. No CLI/API fallback. These are original game textures, not fake review screenshots. Native review images and motion captures always come from the running DX12 proof. The generated files are stored here as project assets; they are not runtime downloads.

AeratedWater prompt: Original game VFX waterfall diffuse/opacity texture, orthographic front-facing aerated white falling water, broad connected soft mottled whitewater core, varied optical density, detailed microdroplets, soft broken side edges, top/bottom continue out of frame, flat even illumination, only water on genuine transparent background, no scene/rocks/pool/text, no striped threads/fabric/smoke/glow. Portrait1024x1536.
ImpactSplash prompt: Original photorealistic water impact splash sprite, broad low irregular upward bursting fan, fine foamy water, overlapping turbulent splashes and many delicate microdroplets with transparent gaps, airy feathered spray, cool neutral white, flat even light, genuine transparent background, no ground/pool/rocks/text, no smoke/spheres/symmetry.

spray.png, mist.png and rock_original.png: original deterministic procedural artwork from Tools/prepare_placeable_waterfall_assets.py. The script stages these plus the two image-generated textures. waterNormalMap.jpg for the optional pool fixture comes from the pinned WickedEngine/Content/models stock artwork; it is not new Renegade art and is not copied into this asset folder.

Hashes (SHA256):
- AeratedWater.png: 33bc77e746a03f58338a0196a64b9f4305484c202836c37babee450251251eb4
- ImpactSplash.png: c85da954bf5ecbd1bed688131ea390a61ecfd5ae2d3def50994303bbdcc8196e
- mist.png: 4165cfe42d906ae7f2c80d241212d70e89c96a09ebaf20fee35599d6adbe0519
- rock_original.png: 74f1991d5e271dfd90263ff195d5adb7120b78ff87c33870a26e897ff0138870
- spray.png: fd8409d36c0a12269a13b6551c5b087b26948f351d02f48e901238a5f45f2520

## Front-steam revision - owner feedback (2026-10-26)
SteamMist.png is original built-in image_gen artwork (no CLI/API fallback). Generated alpha preserved; not derived from SHADERSOURCE. Final prompt: "Original game VFX texture: fine steam-like waterfall mist on genuine transparent background. Square1024x1024. A very soft diffuse elongated irregular wisp of pale white water vapour, with delicate feathered tendrils, billowing fine turbulence and wide translucent gaps. Airy finely atomised water mist. Low contrast, low opacity, neutral white only, no grey or black pigment, absolutely no opaque centre. Shape loosely rising diagonally upward, fading away in every direction before canvas edges. No individual water balls or droplets, no spherical puffs, no round cloud, no explosion, no smoke stack, no scenery or ground, no text. Flat even illumination, no shadows. Designed for overlapping low-opacity particle billboards in front of a waterfall; entirely original artwork."
Runtime low opacity, expansion, random rotation/lifetime and a neutral tint produce the intended mist. The photographic water's RGB is converted into optical density; neutral sheet RGB avoids compositing grey photographic edge pigments. No review screenshot was generated/retouched.
SteamMist.png SHA256: de963ca3391f1aba0baa453410029f445a745cbbb07d050752d4d8018827ceb4


WaterWisp.png: original image_gen transparent RGBA artwork generated 2026-10-26, original local result exec-f4ddddf9-c1c8-4f4d-83ff-3754c6d0a041.png. Prompt: one isolated small wispy fragment of aerated falling water, fine turbulent whitewater, short descending trails dissolving into atomised spray, irregular feathered silhouette with translucent gaps, cool white microtexture, diffuse light, transparent margins. No surroundings, pool, balls, smoke puff, long solid rope, opaque centre, black/grey paint, shadow, text or hard rectangle. Not copied from SHADERSOURCE.
