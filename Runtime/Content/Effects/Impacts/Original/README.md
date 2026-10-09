# Renegade original impact VFX: integration record (2026-10-09)

Source: owner-supplied impact vfx.zip, SHA256 74eaf3e33a0cdc9c882c99b12a828547df8a5bc745b92a104a311ee6ee83a856
12 owner-supplied independent replacements for private Unity KNIFE artwork.

Core textures are embedded as Win32 RCDATA resources 7310-7321 and lazily loaded into native Wicked textures. They need no Downloads paths, private defaults project or auxiliary files in Build Game.
The accepted water crown and native water ripple behaviour are unchanged.

| Bundled file | Source ZIP member | Type | SHA256 |
|---|---|---|---|
| BloodImpact8x8.png | blood atlas 8x8.png | 8x8 animated impact spray | bba04c84052b7f5ca27178da19b3125fa73ed5eb514d68958e793ef03cb079d5 |
| ConcreteMarks2x2.png | concrete bulletholes.png | 2x2 static decals | 9967df6e232bab9471c7f47fa9a58dca68b34cddad45b1c16de3f9a8012f90a0 |
| DirtMarks2x2.png | dirt bulletholes.png | 2x2 static decals | ccf15c1eb182263de0be30c9c48bcd2fb57d2e32371de9ff71e5c5495e9601a6 |
| GlassDebris2x2.png | glass atlas.png | 2x2 fragment variations | 4ba04c25d2960d7114f62a04a796e3673d1ff434456224969fc45f2f2e5a9ec8 |
| GlassMarks2x2.png | glass bulletholes.png | 2x2 static decals | b7daa067da7e2d1b30c7fe3e1e64210f11deea1af37661cf4be1b3ce18182e0a |
| MetalMarks2x2.png | metal bulletholes.png | 2x2 static decals | 24011908d29dda7fea86a0df2d5523f3b2bb93101769d0be4c9385090399edbb |
| SkinEntryMarks2x2.png | Renegade_Skin_Entry_Wounds_REWORK_2x2_2048_RGBA.png | 2x2 stuck projectile wounds | 0ecb8f2c250ba7c828b8a94d28274af187c7d975237a44226f63bdce15e4b397 |
| RockDebris2x2.png | rock atlas.png | 2x2 fragment variations | 71a646a4fd917e300d99e542e37c27c6f2b0cd22670380bab841f0df9a6ab5af |
| SkinMarks2x2.png | skin bulletholes.png | 2x2 static character decals | 92a99dfdfd3fd0f3bab441d2b37f85cdb9dca92f22f95be7e098ccf609564dc8 |
| StoneMarks2x2.png | stone bulletholes.png | 2x2 static decals | 8ebb1646bd3d82bdc55aaaf7819bf8f605793f8f96fad2ae80fb5dd01c5b1780 |
| WoodDebris2x2.png | wood atlas.png | 2x2 fragment variations | 0a6efd06fd21c0ce8306170cd19b0eb6feddd53763b2722ab47a9ad962f727b1 |
| WoodMarks2x2.png | wood bulletholes.png | 2x2 static decals | 9f52042ffb8c80ea7196bca34d6fe4d1cb205f2f0acbb970d4c585cd3b1d432f |

## Wiring
Metal, wood, concrete, stone, dirt, glass, Character surfaces select one 2x2 static bullet hole. Sticking projectiles on Character use the entry-wound variant. The 8x8 blood atlas animates as a 64-frame splash. Glass, rock, wood debris use 2x2 static fragments. The procedural water crown is not a texture in this pack.
Colour RGBA is preserved rather than tinting static marks black. Animated and static atlases are different uses.
The legacy optional donor lookup remains only as a compatibility fallback when the core texture cannot load.
No PNG was copied from the KNIFE Unity directory. The assets are the exact user-supplied bytes. Distribution still requires the usual final originality/rights review.

## Validation
Build Release targets RenegadeRuntime, RenegadeImpactAudioTests, RenegadeRuntimeProjectileSessionTests and run associated CTests. Verify the original marked surfaces in Test Game and packaged Build Game before merging.

## Owner-authored impact SFX and core packaging
This integration replaces the default impact **visuals** only. The owner confirms that the 14 impact WAV files in `Downloads/impact sfx` and `Downloads/impact sfx 2` were created by the owner and are **original Renegade content, not Unity-pack audio**. Those WAVs can already be imported into the governed project impact-audio bank and included in an exported game using that bank. Automatic core-library bundling of these owner-authored WAVs is still a separate implementation task; it is **not** a need to replace their contents or resolve third-party rights. No new editor asset-browser panel is claimed by this change.
