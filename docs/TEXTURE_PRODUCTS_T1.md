# T1 material texture products

On Windows, importing a texture from a material slot records how the shader uses
it. The import creates a versioned DDS alongside the original encoded bytes in
the texture's governed `.rasset`. The same resolver serves Studio, reusable model
placements, terrain props and packaged material bindings. The project stores the
stable texture ID on the material. Moving the project does not require a machine
path in the scene or the recipe.

| Material slot | Cooked DDS | Mip treatment |
| --- | --- | --- |
| Base colour and emissive | BC7 sRGB | Filter colour in linear light |
| Alpha-tested base colour | BC7 sRGB | Use the material's shader cutoff to preserve alpha coverage |
| Normal | BC5 UNORM | Preserve tangent-space red and green orientation |
| Surface and occlusion | BC7 UNORM | Filter linear data |

PNG, JPEG, BMP and TGA use the pinned DirectXTex CPU encoder. Compatible DDS
products with a complete mip chain retain their compressed bytes. The reader
checks dimensions, mip count, block sizes, byte range and SHA-256 before native
use. Small or unsupported textures and slots with a different recipe keep their
resident source representation. Existing v1 assets, decoded UI resources and
textures embedded directly in native model archives keep their existing path.
A material texture imported with a cutout recipe retains its original cutoff
through reimport. Changing that cutoff uses the resident fallback until a
matching texture product is authored.

## Runtime and reimport

The pinned Wicked resource manager accepts initial DDS data and a bounded
reader over the product's file generation. The first bind reads the original
encoded source during preparation and reads the full selected DDS once, then
uploads its coarse mip chain. Subsequent mip requests perform real range reads
through Wicked's usual worker. The prepared binding shares that initial load
across uses of the same semantic. Global resolution, pressure policy and
per-frame scheduling remain owned by Wicked.

Reimport publishes through the existing resource transaction. New bindings use
the replacement digest, while old jobs finish against their read-only Windows
handle. The logical cache identity includes the resolved project or package
location. Two copies of the same project can therefore have distinct live native
resources even when their asset IDs and content are identical. On exFAT,
Windows can expose a pending-delete temporary file during live replacement.
Windows releases it after the old binding and queued mip work finish.

## Stock terrain grass

Studio, Runtime and the terrain proof stage the same three 4K maps. One shared
cook is keyed by the four source-image hashes, the packer source, tile count and
recipe version. The receipt records all three output hashes. An interrupted or
damaged cook is repaired on the next build. A missing destination is restored
from the cache. Stock grass keeps its existing pixels, filenames and package
locations.

## Checks

`RenegadeMaterialTextureCookTests` decodes the compressed mip chain and checks
sRGB filtering, normal orientation, cutout coverage at two thresholds, valid
DDS passthrough, rejection of truncation and resident fallback.

`RenegadeGovernedTextureStreamingProof` imports and reopens the governed model,
checks resource sharing across 32 boulders, measures mip I/O and GPU residency,
and compares GPU readbacks across reimport, relocation and package binding.
It also saves and reopens an authored prop revision with Save As, and checks
that dependency discovery follows the revision to the model and its texture.
The proof requests resolutions directly and measures residency and byte
identity. Forest performance requires representative production assets.

## Measured hardware run

On an RTX 4070 Ti with D3D12, 32 placements of the test boulder (scale 1–4)
shared one mesh, material and governed texture. The test imported a 512 × 512
TGA with known pixel values, requested a resolution and measured the resulting
native texture. The following sample came from commit `adcab60`.

| Resolution request | Native width | Resident mips | Resident mip bytes | Cumulative native worker reads |
| --- | ---: | ---: | ---: | ---: |
| Initial bind | 128 | 8 | 21,872 | 0 |
| Near | 512 | 10 | 349,552 | 2 reads, 436,960 bytes |
| Far | 128 | 8 | 21,872 | 3 reads, 458,832 bytes |

Wicked uploads a replacement mip chain during each transition. The worker byte
count therefore includes overlapping levels. The first prepare/bind took 3.05 ms
in this warm-process sample. It read the entire 349,700-byte DDS during initial
validation and processed the retained 1,048,594-byte source. The process working
set rose from 565,633,024 to 566,788,096 bytes across that bind. Renderer memory
allocation covers the scene and upload heaps, so use the native mip sizes above
when comparing texture residency. GPU readbacks matched across project copy,
closed-project move and package binding. Reimport changed the new texture while
the old live binding still read its original pixels.

Build Game follows each authored prop layer to its revision, placement pages and
reusable model products. It also discovers stable material texture references in
those models. Old scenes can contain relative paths to the Studio copy of stock
grass. When a reserved stock path is missing, Runtime resolves the same filename
under its own bundled Content directory.
