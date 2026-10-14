# Prop package v1 and actual LibreSprite host

The tested host is official LibreSprite **1.3 Linux x86_64 AppImage**, upstream
tag `v1.3` / source `cdb12a323ce5ec4fce3512bc6590a90228f15bb9`.
Release SHA256:
`fd90bddda8135fe00f4a9a9c8c9ebd48f9b07e8782ab269be5c9efb1a851af30`.
The installer verifies it before extracting; it never changes system programs.
Sources: [release](https://github.com/LibreSprite/LibreSprite/releases/tag/v1.3),
[pinned scripting implementation](https://github.com/LibreSprite/LibreSprite/tree/v1.3/src/app/script/api),
[ASE format](https://github.com/aseprite/aseprite/blob/main/docs/ase-file-specs.md).

Actual host tests establish `app.activeSprite`, named hidden/locked layers,
`layer.cel`, linked cel images/offsets, RGBA bytes, indexed palette content and
`saveAs(path,true)`. SaveFileCopyAs captures unsaved edits without changing the
original source bytes or active document filename. A fresh nonce path ensures a
failed save cannot reuse stale frame duration/tag data. Active per-cel SHA256,
offsets/frame indices and dimensions are verified against the new snapshot before
packing. The pinned API does not expose
frame durations/tags directly or the current upstream `fs` interface. The script
therefore writes a separate lossless ASE snapshot and a request via verified
`storage.set(value,key,domain)` / `storage.save(key,domain)` APIs. The offline
adapter reads durations, tags, offsets and every layer from **that active snapshot**.
It never reads old saved pixels as export content.

The real host silently discards **fractional indexed palette alpha** on load.
v1 deliberately rejects that authoring format; convert such documents to RGBA
before opening/exporting. Indexed transparency uses the actual transparent
palette index; remaining palette entries must be opaque. RGBA art/emission can
use fractional alpha; physical labels must always be exact opaque colors.
The request adapter checks a saved indexed source for this incompatibility as
well as checking the snapshot. Unsupported grayscale/tilemap inputs are errors.

## Artist workflow

1. Install once: `python3 scripts/libresprite/install_host.py /tmp/craft-libresprite`.
2. Open `assets/props/template.aseprite` or either editable animated example.
3. Paint `physics.material` with `material.gpl`, `physics.role` with `role.gpl`,
   and independent `color` / `emissive` layers. Keep metadata hidden if useful.
4. Run `scripts/libresprite/export_prop.js` from LibreSprite's script menu.
   The console prints `CRAFT_PROP_REQUEST=...` and the exact packing command.
5. Run that command, e.g. `python3 scripts/props/pack_prop.py --request
   ~/.config/libresprite/craft-prop-export.request --output /path/to/example.prop`.

This is a two-step script plus supported offline adapter; the stock host cannot
spawn an arbitrary Python process from JavaScript. The snapshot is staging data,
not a claimed ready-to-place runtime scene. Current game placement and emission
integration remain separate tickets. Metadata mistakes are diagnosed by the
packer before a previous exported package is replaced.

By default the snapshot is a `.craft-export.<nonce>.aseprite` sibling of the document.
An unsaved document needs a first save to choose a directory or an explicit
`CRAFT_PROP_SNAPSHOT` path in the script. Unsaved *edits* are captured in either
case. The script never flattens, hides, crops, trims or saves over the original.
Ignored nonreserved reference/art layers are retained in the snapshot but not
included in the package. Reserved names must be unique globally, including across
groups. The package uses transparent missing cels, never implicit frame holds.
Layers/cels must use normal blending/255 opacity: transparency belongs in pixels.

## Channels and identity

`assets/props/schema-v1.json` is the authoritative exact legend. Canonical prop
materials are separate from terrain and body enums: wood is canonical2,
terrain7, body2; dense wood canonical4, terrain8, body4. Mortar canonical3 maps
only to rigid/structural body3; mortar debris canonical9 is granular terrain6.
No palette assigns density/mass or floating behavior; composition determines it.

Material/role labels are independent of albedo. A transparent material pixel is
a hole even if `color` paints over it. Invisible occupied pixels require explicit
`allow_invisible_physics`. Decoration may emit without material or visible color.
Object labels use opaque RGB little-endian uint24; transparent or ID0 means
implicit grouping. They are **authored groups, not connected-component labels**.
Four-connected physical components within a rigid role/object remain distinct
bodies. Authoring anchor hints are opaque white; only structural pixels may have
them, and runtime actual terrain contact still determines support.

v1 implementation supports **one visual texel per physical pixel** only; non1:1
scales are rejected rather than resampling metadata. A future version must supply
an explicit owner-index plane for higher visual resolution. Pivots are integer
cell coordinates, top-left x-right/y-down. Optional markers with content are
rejected until a typed marker legend/importer is implemented, never discarded.

Options contain a stable `asset_uuid` when supplied. Otherwise deterministic UUID
derives from dimensions and first-frame physical records; geometry edits produce
a new default identity. For stable edited assets, set an explicit UUID once.
Runtime immutable identity is asset UUID + authored object + source pixel/frame;
runtime body IDs/storage generations are separate lifetime identifiers.

## Animation and package

Each frame has integer1..65535ms duration, aligned full-canvas appearance PNGs,
typed physical maps and preserved animation tags (first/last/direction/repeat).
`visual_only` compares material/role/object/anchor records in **every** frame and
diagnoses the first changed frame/pixel. Explicit `state_variants` preserves
different metadata maps for separately selected placement states. It never
authorizes render-frame collider/mass changes.

Color RGB is sRGB decoded once to linear float32; alpha stays separate.
Emission is `linear(RGB)*alpha*emission_multiplier`, independently of albedo.
Normals are signed normalized rest-spaceXYZ; absent defaults [0,0,1]. Roughness
grayscale red/255 defaults0.8. Relief grayscale red/255 *relief_scale is scene-cell
shading height, not collision deformation. Scalar channels must be grayscale.

A package is a directory containing canonical versioned `manifest.json`, aligned
RGBA8 PNGs, material `.u16`, role/anchor `.u8`, object `.u32`, and `asset.cprop`.
All maps are row-major little-endian. Manifest SHA256 covers every referenced file;
paths are single local filenames with no symlinks/traversal. Validation checks
hashes, PNG CRC/format/dimensions/decoded length, map lengths and codes, exact
canonical registry, animation policy/bounds and compiled agreement. Export stages
and validates first, then publishes with atomic rename (Linux `renameat2` exchange
when replacing); if atomic exchange is unavailable the old package remains and
the operation fails. Identical input yields byte-identical files.

Preload budget is256MiB compiled data per package, at most4096frames and4194304
pixels per frame. Split larger scenes into packages. There is no runtime
per-frame file parsing/allocation/pixel readback. Offline pixel scanning is authoring
work; it must not be copied into the live physics path.

## Compiled byte layout

Every integer/f32 is little-endian, no native struct padding. Header64bytes:

| Offset | Meaning |
| --- | --- |
|0|ASCII CPRP|
|4|u32 version1|
|8|u32 exact byte length|
|12,16|u32 width,height|
|20|u32 frame count|
|24|u32 flags: bit0 state_variants, others invalid|
|28|u32 frame table offset64|
|32|u32 frame-data offset64+16*frames|
|36,40|u32 physical stride8, appearance stride64|
|44|u32 IEEE CRC32 of all file bytes except this4byte checksum field|
|48|16byte RFC4122 asset UUID|

Frame descriptor16bytes: u32 duration, relative frame-data offset, physical byte
length, appearance byte length. Frames are dense and contiguous. Each frame has
all physical records followed by all appearance records.

Physical record8bytes: canonical materialu16, roleu8, anchoru8, authored objectu32.
Appearance record64bytes (16floats): colorRGB/roughness, restnormalXYZ/relief,
emissionRGB/reserved0, alpha/reserved0/reserved0/reserved0. The first32bytes match
the renderer's `fr_appearance_texel_t`; upload must preserve the independent
emission/alpha data as well. Physics maps never use gamma-converted texture paths.

`fr_prop_asset_open` validates a caller-preloaded immutable buffer, returns an
allocation-free view and preserves the previous view on failure. It rejects
unknown material/role combinations, corrupt CRC/layouts, invalid floats and
visual-only metadata changes. Pixel/appearance lookup is bounded and explicit.
Current elapsed-time helper selects full-document forward-loop frames only;
tag policy, pause clock persistence and game GPU updates belong toctpe-d6m2.

## Runtime integration proposal (not implemented)

An authored placement transaction will upload exact physical maps and independent
appearance buffers to GPU staging. Request includes validated immutable asset,
selected variant, canonical signed64 chunk/local origin, pivot, transaction ID
and reject-overlap policy. GPU passes label four-connected rigid-capable pixels
within role/object, reduce composition/COM/inertia, count body/geometry/graph and
appearance needs, reserve all pools, then publish once all capacities/contacts
validate. Failure leaves both simulation and visual ownership unchanged.

Terrain writes ordinary material cells without synthetic pins. Structures require
the real mechanically derived graph and terrain-contact anchors; hints cannot
invent support. Rigid births use exact sparse16x16materialtiles with holes/mixed
composition. Source pixel→body resttexel provenance transfers via the extraction
birth hook. No CPU runtime flood fill, per-texel CPU light objects or per-frame
collider rebuild is allowed. Renderer GPU emitter aggregation must retain energy,
RGB and source extent; its16explicituserpointlimit cannot truncate a painted
emission map. Transactional authoring entrypoint is new integration work after
ct-wuci, not a patch to stale serial extraction shaders.
