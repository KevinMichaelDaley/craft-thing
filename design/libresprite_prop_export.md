# LibreSprite multilayer prop and scene export

## Deliverable and isolation

Provide a real LibreSprite script, an offline validation/packing library and a
game-side asset reader/placement path. A multilayer animated document becomes a
versioned prop/scene package with synchronized physical and visual channels.
This is authored pixel content, not a flattened sprite that guesses physics from
its visible colors. Work in `feat/libresprite-prop-export` under
`/home/kmd/craft-thing-prop-export`; main extraction and the textured renderer have
separate agents/worktrees. Coordinate shared contracts and GPU test windows.

No LibreSprite executable was found on PATH during planning. Establish a pinned
locally installed/built LibreSprite and run an actual compatibility probe before
claiming exporter support. Use its JavaScript scripting interface; do not assume
Aseprite Lua API compatibility. Official references consulted:

- [LibreSprite scripting API](https://github.com/LibreSprite/LibreSprite/blob/master/SCRIPTING.md)
- [LibreSprite command-line options](https://github.com/LibreSprite/LibreSprite/blob/master/src/app/app_options.cpp)

Current upstream documents layer/cel/image access, image data/PNG encoding and
filesystem operations; command-line source exposes script and sprite-sheet/data
export options. Pin and verify the actual build: documentation alone is not proof
that a released binary supports every operation. Probe frame durations, tags,
linked cels, palette interpretation and document snapshots in particular. A
supported batch adapter may supply missing metadata, but the delivered user entry
point must still be a working LibreSprite script. No mocked-host-only acceptance.

## Authoring contract

Reserve exact channel names, optionally inside one named export group. Each
channel uses the same canvas, coordinates, frames and pivot. Metadata layers are
read even when hidden or locked. Never blend their palette colors with artwork.
Unrecognized art/reference layers are ignored only by a documented rule; duplicate
reserved layers and ambiguous groups are errors.

| Layer | Meaning | Encoding/default |
| --- | --- | --- |
| physics.material | Authoritative material at each physical pixel | Exact supplied palette or explicit indexed mapping; transparent = no material |
| physics.role | Terrain, structural, rigid body or visual decoration | Exact supplied palette; no inferred role from visible color |
| physics.object | Optional stable authored object/assembly grouping | Integer IDs, independent of connected component labels |
| color | Authored albedo/visual RGBA | sRGB color, explicit alpha; decoded to renderer linear appearance |
| emissive | Authored emission color and intensity mask | RGB plus alpha mask and manifest HDR intensity multiplier; absent = zero |
| normal | Optional rest-space surface normal | Documented tangent-space RGB encoding; absent = flat |
| roughness | Optional surface response | Explicit scalar channel; absent = material default |
| relief | Optional shading height | Explicit scalar scale/units; does not change collision geometry |
| physics.anchor | Optional author-intended structural anchor hints | Validated against actual terrain contacts; cannot invent support |
| markers | Optional pivot/attachment/light/spawn annotations | Typed legend and stable IDs, never overloaded material pixels |

Ship palettes, a template document, layer legend and a small animated example.
Material mappings use canonical material names plus a registry/schema version.
Never equate terrain and rigid material enum numbers; importer conversion is
explicit. Unsupported materials and role/material combinations are errors,
including granular pixels in a rigid-only body. No invented density, mass or
float/sink flag: actual composition determines those properties and buoyancy.

Each physical pixel has an unambiguous material and role. Holes stay holes even
when art surrounds them. Decoration can have color/emission without material.
Invisible occupied pixels require an explicit authoring flag so an accidental
missing color cel cannot silently create an invisible wall. Physical metadata
uses exact opaque labels: reject antialiased/partial-alpha/unknown colors rather
than choosing a nearby material. Normalize RGBA and indexed documents through the
same legend; preserve real alpha independently from transparent palette indices.

Resolve cel offsets into the shared full canvas. Linked cels retain their content
and timing; missing cels follow a documented transparent-or-explicit-hold rule,
not an accidental nearest frame. Do not independently trim channels. Optional
atlas packing uses one shared frame rectangle and origin for every channel.

## Geometry, grouping and animation

Coordinates start at the canvas top left, x right/y down; default one authored
physical pixel equals one simulation cell. Record rational visual/physical scale
and pivots explicitly. Do not resample metadata to resize art. A higher-resolution
color plane needs an explicit mapping to physical pixel indices.

Within a role/object ID, four-connected rigid-capable material pixels form bodies
using exact sparse geometry, retaining mixed materials, holes and concavities.
Disconnected components do not become one rigid piece merely because their
visible art overlaps; explicit assembly relationships are separately named.
Structural pixels instantiate marked structural components and terrain-contact
anchors through the existing graph mechanism. Terrain is ordinary world material
and remains eligible for automatic extraction if it is actually detached; the
asset role must not secretly suppress gravity. Validate overlapping placement and
capacity before publishing either physical or appearance state.

Export every frame's channels, exact integer millisecond duration, animation tags,
loop direction/count, canvas/pivot and object identities. Default policy is
`visual_only`: color, emission and surface data animate while physical material,
role and object occupancy remain invariant. The validator compares metadata in
all frames and rejects changes with a precise frame/pixel diagnosis unless an
explicit supported policy is selected. Never silently discard metadata frames or
replace collision/mass every render frame. Preserve changing metadata in an
explicit state-variant package for separately selected placement states; dynamic
collider/mass animation is a future policy requiring conserved transactions and
dedicated tests, not an implicit behavior of this exporter.

Runtime clocks advance from elapsed time, not render-frame count. Choose visual
frame without touching physics state; retain deterministic timing across variable
frame cadence, pause/resume and save/reload. Default game pause freezes visual
animation too so stable-frame renderer tests stay meaningful.

Emission is separate from albedo. Import it as renderer emission data with stable
owner/texel identity; coordinate the renderer's actual light-source/field API.
Emission must visibly self-illuminate and contribute to the lighting field once
renderer integration is available. Do not spawn one CPU-updated point light per
bright texel. Preserve total energy and document any bounded clustering/transport
representation. Body-attached emission moves and fractures with its owner.

## Package and failure behavior

Use a human-readable versioned JSON manifest and aligned PNG appearance channels,
with typed lossless material/role/object maps for runtime use. Document binary
word order, dimensions, offsets, hashes, canonical names and schema migration.
Manifest records source/tool/exporter versions, canvas, scale, pivot, frames,
durations/tags, per-frame channel paths, material legend, object/group identities,
physics-animation policy and emission/relief units. Never encode material IDs
through gamma-converted RGB texture upload.

Validate dimensions/counts/integer overflow and decoded lengths, animation bounds,
palette labels, object ownership, duplicate roles, channel alignment and file
integrity. Restrict referenced paths to the package. Unsupported schema/data is
diagnosed, not partially installed as a scene. Export to staging and publish only
after complete validation; a failed export/import preserves the previous package
and game state. Repeated export of identical content is deterministic. Read actual
source metadata without destructively flattening, hiding, trimming or saving over
the artist's original document. Unsaved edits need a verified snapshot path or a
clear, actionable save requirement; never quietly export an older saved version.

Keep the host adapter small and the channel/manifest logic testable off-host.
An offline CPU export/validation loop is acceptable authoring work; game-sized
runtime placement/transfers use the existing GPU-parallel physics pipeline.
Runtime preloads validated assets; no per-frame file parsing, allocation, pixel
readback or CPU connected-component solve. Compile static channel data once and
use a bounded GPU animation/placement path for large scenes.

## Acceptance and rollout

Ticket stages: schema/fixtures and real-host probe; LibreSprite script; package
validator/compiler; game import and role instantiation; renderer/animation/emission
integration; end-to-end acceptance and coordinated integration.

Epic: `ctpe-6m89`.

| Ticket | Delivery |
| --- | --- |
| ctpe-5lmh | Schema, channel palettes and authoring fixtures |
| ctpe-iytx | Pinned real LibreSprite compatibility probe |
| ctpe-b1r2 | Nondestructive in-app exporter script |
| ctpe-2lx4 | Package validator/compiler and C asset reader |
| ctpe-847v | Exact terrain/structural/rigid placement |
| ctpe-d6m2 | Animation clock, appearance and emissive integration |
| ctpe-g0sl | Real host-to-game visual and failure acceptance |
| ctpe-bj97 | Coordinated combined integration |

Final integration depends on ct-wuci. Renderer epic `ctr-6fii` and its appearance
ABI ticket `ctr-9svu` live on the sibling renderer branch; coordinate that external
dependency explicitly before closing appearance/emission integration. Do not
manufacture local copies of renderer tickets with independently changing status.

Follow RED-GREEN-REFACTOR for C and script/packing behavior, compile/fail meaningful
tests before implementation, and run relevant repository batteries. Include RGB
and indexed inputs, hidden metadata, cel offsets, linked/missing cels, unequal
durations, loop tags, exact transparent holes, mixed materials, two distinct
concave rigid objects, structural support plus terrain, emission-only decoration,
invalid palette labels, alignment errors, malicious/invalid paths and atomic
failure/capacity rollback. Include a real animated LibreSprite document exported
using the shipped script in the actual pinned host, then loaded/displayed by the
Vulkan game. Mocks alone do not satisfy that test. Persist/reload an instance and
verify materials, visual colors, animation phase and emission ownership.

Renderer integration follows its appearance ABI, not a duplicate incompatible
color format. Extraction integration follows completed ct-wuci, not stale serial
shaders in this checkout. Agents coordinate these contracts and GPU test windows.
Push verified milestones to this branch; final main integration requires combined
tests and parent coordination. Keep unmet runtime dependencies open and report
them precisely rather than claiming that exported files alone are usable props.
