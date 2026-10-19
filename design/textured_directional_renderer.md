# Textured solids and directional lighting

## Goal and ownership

Keep the simulation's material index authoritative while giving each solid pixel
its own appearance. Wood has continuous grain and rings; stone has color and
surface relief; brick and mortar surfaces can carry stains, cracks and wear.
Combine point lights and sunlight into a directional lighting field with sharp
pixel-scale occlusion and optional soft penumbrae. “Noita-style” describes the
intended visual result, not a claim about that game's implementation.

Implement on `feat/material-light-renderer` in its separate worktree. Extraction
continues independently on main under ct-wuci. Integrate through documented
appearance ownership hooks after its parallel transaction pipeline stabilizes;
do not replace that pipeline with the older implementation in this branch.

## Three independent layers

1. Physics: existing material, mass, geometry and world/body identity. Keep
   `dc_cell_t`, material codes, collision geometry and simulation resolution.
2. Appearance: an explicit linear albedo/color channel plus optional normal,
   relief and roughness data. Every visual texel resolves to a physics owner and
   its material pixel; an atlas or texture never determines physical material.
3. Lighting: uncolored visibility/transmission and RGB directional radiance.
   Recompute illumination from lights and current occluders without baking it
   into albedo or modifying simulation cells.

The current pattern shader already reconstructs coverage and density. Preserve
its conservative within-cell composition and diagnostic overlays while replacing
normal-mode fixed material colors. Keep a deliberate unlit/reference mode for
physics diagnostics; update legacy color assertions to use that mode explicitly.

## Appearance channel and texel addressing

Use renderer-owned sidecar storage, not spare physics flags or fluid words.
Terrain addresses texels by stable world chunk/cell coordinates. Each body stores
a stable appearance identity and a rest-space mapping; inverse-transform screen
samples into the body, resolve its exact occupied material pixel, then fetch
appearance. Rotate relief normals with the body. Holes and boundaries come from
physics coverage, not the texture's RGB or a fabricated opacity mask.

Start with one appearance texel per physics pixel. The API must also permit
multiple visual texels per physical pixel through an explicit owner-index mapping;
greater display detail must never add collision mass or alter pixel connectivity.
Mixed-material bodies retain a material index for every occupied pixel. Terrain
and body material namespaces differ today: use explicit conversion at this
boundary, never assume their numeric IDs coincide.

Provide deterministic texture generation keyed by immutable appearance identity
and rest/world coordinates. Generate coherent wood grain/rings, restrained stone
color and relief, and brick/mortar style variants. A visual grout pattern may be
cosmetic; if grout has distinct physical behavior its mapped pixel must actually
have the appropriate physical material. Imported textures use the same mapping
and explicit color channel. No remote image-generation dependency is needed for
the initial procedural implementation.

Keep generated seeds/parameters and authored per-texel overrides separate. Loaded
older worlds get deterministic defaults. Persist explicit appearance data in a
versioned, validated sidecar; seeds alone cannot recover painted discoloration or
damage. Extraction transfers the source appearance mapping atomically with birth.
Fracture partitions it without resampling or recoloring survivors. Structural
release, eviction/reload, rebasing and save/reopen preserve it. Reused page/body
slots cannot inherit the previous occupant's texture. GPU compute transfers and
reductions remain parallel; no per-frame CPU appearance copyback.

## Occlusion and directional field

Build a separate occlusion/transmission channel from the current terrain and
exact rigid/structural occupancy. It is independent of albedo brightness: black
wood and white wood cast the same shadow. Preserve one-pixel walls, concave holes,
mixed-material boundaries and moving/rotating silhouettes. Material optical
properties are renderer metadata, separate from density and mechanical strength.
Water can use bounded absorption/transmission, with no claim of full refraction.
Surface relief affects shading, not silhouette collision or object-scale shadows.

Prototype a conservative hierarchical opacity representation with parallel mip
construction and light-to-tile culling. Evaluate point-light visibility and sun
visibility on the GPU before aggregating illumination. Empty/full hierarchy nodes
can be skipped/accepted; ambiguous nodes descend to actual pixel coverage so a
thin wall cannot disappear in a coarse level. Every receiver/light query executes
in parallel with other queries. Avoid a single invocation iterating all lights,
all receivers or all cells. Use bounded traversal/iteration budgets and make
unfinished visibility conservative rather than silently leaking light. Local
ordered ray dependencies must be documented and measured, never described as a
parallel whole-scene scan. Tune exact sun scans versus hierarchical traversal
using measurements, not assumptions about the hardware.

Use a small angular radiance representation during transport, then resolve RGB
energy and RGB direction moments (`L0`, `Lx`, `Ly`, `Lz`) into the lighting channel.
RGB moments preserve different directions for differently colored sources.
Retain energy when opposite lights cancel the first directional moment; do not
normalize the net vector and discard ambient energy. Choose 8/16 angular bins and
formats from accuracy/memory tests. Accumulate in linear HDR, shade using the
texture normal/roughness, then tone-map/encode display RGB once. Light radius,
intensity, height and attenuation have documented units and bounded behavior at
the emitter. Sun direction, intensity and angular extent are independent inputs.

Begin with direct lighting and occlusion, no temporal feedback or indirect bounce.
Add soft source sampling only after hard-shadow correctness. Later indirect
transport or radiance cascades require a separate measured ticket; they are not
necessary to deliver the requested color and directional-light channels.

## Streaming, invalidation and composition

Cache appearance in world/rest space and light fields in world-indexed tiles.
Camera movement must not reseed grain or smear shadow history. Dirty causes are
explicit: material/appearance edits, body poses, extraction/fracture, light edits,
sun changes, page residency and reload. Unchanged scenes skip work through GPU
dirty masks and indirect dispatch. Light caches use generation/ownership checks.

Sources beyond the viewport still contribute if their influence overlaps known
resident data. Establish a lighting halo and explicit known-sky boundary input
for sunlight. Unknown occlusion beyond available world data is not transparent
air; defer or conservatively limit visibility. A missing page cannot create a
bright leak. Record this limitation rather than promising infinite-range sun
queries from a finite resident cache. Reload must converge to the same result.

Compose lit opaque surfaces using the existing exact coverage. Keep granular/wet
appearance and water composition compatible, then overlay markers, material IDs,
rigid IDs and structural diagnostics without lighting their semantic colors.
Presentation can run while physics is paused. Test four or more unchanged frames
bitwise to prevent the previous shimmering regression. No temporal accumulation
is enabled until motion/reprojection and stable-history ownership are tested.

## Delivery and acceptance

Stages: ownership/storage contract; coherent textures; conservative occlusion;
point/sun directional field; lit composition and controls; extraction/persistence
integration; visual/performance acceptance and main integration.

Ticket epic: `ctr-6fii`.

| Order | Ticket | Delivery |
| --- | --- | --- |
| 1 | ctr-9svu | Appearance sidecars and texel/material ownership |
| 2 | ctr-8y4b | Coherent wood, stone and brick/mortar textures |
| 3 | ctr-9k5m | Conservative GPU occlusion/transmission hierarchy |
| 4 | ctr-pbtf | Point/sun transport and directional RGB field |
| 5 | ctr-7tl5 | Lit composition, unlit diagnostics and controls |
| 6 | ctr-ddtf | Parallel extraction/fracture and persistence integration |
| 7 | ctr-21eh | Visual battery and measured renderer scaling |
| 8 | ctr-qlsd | Combined validation and coordinated main integration |

Storage precedes texture and occlusion work; those converge at lit composition.
Appearance transfer and final integration depend on ct-wuci. Other renderer
stages can proceed while that ticket is still running.

Create a reproducible displayed scene with two different concave textured bodies,
terrain, stone/wood/brick-mortar surfaces, differently colored point lights and a
sun. Record views before/after motion and fracture, narrow shadow gaps, opposite
lights, an emitter inside a hole, wet material, and stationary close-ups. Check
stable paused frames and stable appearance under camera rebase/reload. Color
variation and relief must be visible, not merely exposed as unused API fields.

Follow repository RED-GREEN-REFACTOR and run headless, UI, quarter-native and
SPIR-V/Vulkan validation. Compare identical physics states in unlit/lit modes.
Measure texture generation, occlusion, transport and composition separately on
this machine using Vulkan without validation for timing. Initial reference is
the actual quarter-native 480x270 view, sun plus four visible point lights; target
added mean renderer GPU cost <=2ms and bounded cold/dirty work. Also report 1/4/16
light and viewport/complexity sweeps, VRAM, tail latency and actual displayed
FPS. Keep the existing physics performance assertions unchanged. If the whole
frame exceeds 60Hz, optimize or file concrete remaining work; do not claim 60FPS
from a renderer-only budget. The separate dense-fluid issue ct-irzt remains open.

Agent owns and pushes its branch, and coordinates GPU test windows with the
extraction agent. Integration against completed extraction changes is an explicit
ticket: run merged validation before merging to main. Do not push renderer work
directly to main or overwrite extraction commits from a stale base.
