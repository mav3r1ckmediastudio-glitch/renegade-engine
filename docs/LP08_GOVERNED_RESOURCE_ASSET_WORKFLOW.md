# LP08 — Governed Resource Asset Workflow

Status: **programme contract established; Gate 1 active**.

Authoritative starting `main` baseline:
`892cde5bcc94ccb08f9354b31da9e453d377ae12`
(`Add LP07 Gate 6 packaged Runtime acceptance (#51)`).

Wicked remains pinned at
`3a800b7134aafe58461093c8abb2e274d4e64033`.

Architectural authority remains
`docs/adr/0004-renegade-asset-model-and-managed-metadata.md`.

## Why LP08 exists

LP07 established the first complete governed imported-asset lifecycle for model
content: stable identity, catalogue state, source provenance, explicit safe
reimport, reusable placement, dependency closure, Windows packaging and Runtime
resolution.

LP08 must **generalise that lifecycle, not clone it once per file type**.

The pinned Wicked revision already exposes images, sounds, Lua scripts, video and
font styles through one `wi::resourcemanager` family. Renegade therefore treats
these as adapters behind one governed non-model resource-asset contract.
Textures/images are the first material-facing acceptance path because they are
immediately consumed by existing Wicked `MaterialComponent` texture slots.
Audio, video, Lua and fonts join the same identity/import/reimport/package
lifecycle without pretending that their later gameplay/authoring UX is already
complete.

## Exact pinned format truth

At the pinned Wicked revision, `wi::resourcemanager` advertises:

- **Image/texture:** JPG, JPEG, PNG, BMP, DDS, TGA, HDR;
- **Audio:** WAV, OGG;
- **Script:** LUA;
- **Video:** MP4, raw H264;
- **Font style:** TTF.

Renegade must not claim formats merely because the Asset Browser recognises an
extension. In particular, the current browser classifies MP3 as audio while the
pinned Wicked resource manager does not advertise MP3. Gate 1 corrects that
false-positive unless a separate proven decoder seam is deliberately added.

The current `AssetType` vocabulary also lacks a Font type. Gate 1 adds one rather
than misclassifying font resources as generic data.

## Public asset model

`.rasset` remains the Renegade-owned imported-asset abstraction. LP07's existing
model `.rasset` is a version-1 concrete model payload containing internal Wicked
WISCENE data. LP08 may generalise the container for additional resource payload
kinds only through an explicitly versioned, backwards-compatible contract.

LP08 must not reinterpret existing LP07 model assets or invalidate their parser,
identity, reimport recipe or Runtime behaviour.

Conceptually:

```text
external source              governed imported asset        consumer
castle_wall.png      ->       castle_wall.rasset       ->    Material texture slot
battle_theme.ogg     ->       battle_theme.rasset      ->    later Audio authoring
quest.lua            ->       quest.rasset             ->    later Script lifecycle
intro.mp4            ->       intro.rasset             ->    later Video authoring
ui.ttf               ->       ui.rasset                ->    later Font/UI authoring
```

The product asset owns the accepted Runtime payload. The retained source remains
editor/reimport input and must not leak into a standalone build merely because it
is needed for freshness validation.

## Programme outcome

A creator must be able to:

1. import a supported non-model resource into the project once;
2. receive one stable governed asset identity and source/product provenance;
3. see the asset in the registry/metadata-backed Asset Browser with honest type,
   source format and current/stale/missing state;
4. explicitly reimport a changed source without changing the product asset ID;
5. survive a failed reimport with the previous successful product authoritative;
6. use a governed texture from a real Wicked material texture slot through
   Renegade-owned authoring code and preserve it through Save/Open;
7. build a Windows game whose dependency closure packages the current governed
   resource product rather than the original retained source; and
8. run the named packaged Runtime using the current accepted resource payload.

Audio/video/Lua/font **asset lifecycle support** is part of LP08. Full audio
component authoring, script lifecycle/events, video-player UI and typography/UI
systems remain later subsystem programmes.

## Standing architecture rules

### One asset identity system

LC01 remains the sole stable asset/provenance authority. LP08 does not create a
resource-specific ID database.

### One common resource seam

Renegade owns a UI-independent resource contract over pinned Wicked resource
loading. File-type adapters expose capability and evidence; Studio never calls
raw Wicked resource loading as its authoring API.

### Format support must be proven

Asset Browser classification, importer acceptance and Runtime support must agree.
A format is not supported because its extension looks familiar.

### Last-good product wins

Import/reimport candidates become authoritative only after validation and
transactional commit. Failed replacement preserves previous product, registry
and creator metadata.

### Explicit reimport only

Source freshness can become stale automatically; product replacement remains an
explicit creator action.

### Stable references beat paths

Consumers reference stable Renegade asset IDs. Paths remain display/recovery
hints and package locations, not durable identity.

### LP05/LC01/LP06 stay authoritative downstream

LP08 adds providers/adapters to the existing dependency/build system. It does not
invent a second package list or bypass build freshness rules.

### No source leakage

Retained PNG/DDS/WAV/OGG/LUA/MP4/H264/TTF source inputs are editor-side reimport
inputs. Standalone output contains only the accepted governed product and the
Runtime support actually required to consume it.

## Gate 1 — Common resource seam and format truth

### Goal

Establish one Renegade-owned non-model resource capability/import boundary and
make browser/support claims match the exact pinned Wicked revision.

### Required behaviour

- add a UI-independent resource source-format/capability contract;
- derive/lock the accepted format table against pinned Wicked evidence;
- retain image, audio, script, video and font as distinct resource classes;
- add `AssetType::Font` and correct browser labels/classification;
- remove the unsupported MP3 claim unless a real tested decoder seam exists;
- validate source containment and preserve source bytes unchanged;
- provide deterministic supported/unsupported/mismatched-format rejection;
- expose enough neutral evidence for later governed import validation;
- no stock Wicked Editor UI;
- no `.rasset` schema mutation yet;
- no material mutation yet.

### Gate 1 acceptance

- fixed format-table tests exactly match the pinned resource-manager contract;
- browser classification agrees with that contract for every LP08 extension;
- representative image/audio/Lua/video/font inputs classify through one service;
- unsupported and extension/content mismatch cases fail closed where reliable
  signature validation exists;
- source bytes remain unchanged;
- existing LP07 model import/reimport/package tests remain green;
- Debug/Release CI and pinned-Wicked baseline pass.

## Gate 2 — Generic governed resource `.rasset` transaction

### Goal

Create one reusable imported-resource product transaction for non-model content
without breaking LP07 model `.rasset` compatibility.

### Required behaviour

- define a versioned resource payload kind/container contract;
- preserve backwards read/use of LP07 model `.rasset` v1;
- project-owned retained source under `SourceAssets`;
- governed product under `Content`;
- stable LC01 source/product IDs and import provenance;
- deterministic source format/importer/settings recipe;
- transactional product + registry + catalogue metadata commit;
- useful derived metadata appropriate to the resource class (for example image
  dimensions/mips/format where reliably available);
- first-import failure leaves no half-asset.

### Gate 2 acceptance

A representative PNG is the primary proof, with at least one fixture from each
other common resource class proving the same generic transaction. Reopen retains
IDs/provenance and existing LP07 model assets remain byte/behaviour compatible.

## Gate 3 — Texture-to-material authoring and persistence

### Goal

Make a governed texture asset a real creator-facing dependency rather than only
a catalogue entry.

### Required behaviour

- extend Renegade `MaterialService` with curated Wicked material texture slots;
- assign/remove a governed texture by stable asset ID through Renegade commands;
- cover core PBR slots first: base colour, normal, surface/ORM, emissive,
  displacement and occlusion as supported by pinned Wicked;
- resolve the current accepted product, not retained source;
- Undo/Redo material texture assignment;
- WISCENE Save/Open preserves the durable Renegade reference and material result;
- moved source does not break an already-current product;
- Asset Browser selection/assignment uses Renegade UI, never stock Wicked UI.

### Gate 3 acceptance

A fixed textured-material fixture proves assign -> render/resource resolve ->
Undo/Redo -> Save/Open -> same stable texture reference and visual material
resource. Existing scalar PBR material editing remains unchanged.

## Gate 4 — Explicit reimport and multi-resource lifecycle

### Goal

Generalise LP07's safe source-update behaviour across the LP08 resource classes.

### Required behaviour

- source change reports stale without overwriting product;
- explicit reimport replays stored recipe and retains stable IDs;
- successful reimport updates accepted payload/hash/derived metadata;
- failed reimport preserves last-good product and provenance;
- moved/missing recovery uses existing LC01 semantics;
- texture consumers resolve the new product without manual reference repair;
- WAV/OGG, LUA, MP4/H264 and TTF use the same generic reimport transaction.

### Gate 4 acceptance

PNG update -> stale -> reimport -> same ID -> material resolves new texture is the
primary behavioural proof. Representative audio/script/video/font fixtures prove
stable generic lifecycle and last-good preservation without claiming their later
full authoring UX.

## Gate 5 — Dependency, package and Runtime acceptance

### Goal

Prove the governed resource lifecycle crosses LP05/LC01 -> LP06 -> named Runtime.

### Required behaviour

- scene/material dependency closure resolves stable texture asset IDs to current
  governed products;
- source freshness is checked without packaging retained sources;
- stale governed products fail owner build closed and preserve prior output;
- current resource products enter the normal deterministic package manifest;
- original creator sources do not leak into `GameData`;
- named Runtime resolves the packaged current texture/resource payload;
- direct promoted executable launch works after the source project is removed;
- representative non-texture resources prove generic package/Runtime loadability
  through a bounded test seam without pretending later gameplay authoring exists.

### Gate 5 acceptance

The decisive texture proof deliberately saves a scene against an older texture
payload, reimports a changed source without resaving the scene, builds through
the real Windows owner workflow, and proves the staged and promoted Runtime use
the current packaged product by stable asset identity/hash. Source files remain
absent from the package. Audio/Lua/video/font product loading remains regression
proof for the common resource contract.

## Explicit exclusions

LP08 does not add:

- full audio emitter/mixer/reverb authoring;
- Lua gameplay lifecycle, events, autocomplete or hot reload;
- video-player/editor timeline UX;
- font typography/layout/UI authoring;
- universal thumbnails/previews;
- background import queues;
- `.rentity` authoring;
- automatic destructive reimport;
- GPU texture compression/cooking policy beyond what pinned Wicked already
  consumes;
- commercial package cooking/signing/encryption;
- stock Wicked Editor windows;
- Wicked source/pin changes without a separate justified core-patch decision;
- LC01 identity/schema replacement; or
- changes to `Tools/Windows-Build.Common.ps1`.

## What follows LP08

Once LP08 closes, new file/resource classes should normally be adapters and
proofs against this common lifecycle rather than new multi-gate asset systems.
The remaining Phase 4 work can then concentrate on previews/thumbnails,
background jobs/error UX and any deliberately deferred resource-class polish
before Renegade moves deeper into scene/render and gameplay authoring.
