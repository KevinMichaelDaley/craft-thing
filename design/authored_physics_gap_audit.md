# Authored physics capability audit and transaction contract

Feature owner: `ctpf-rvcy`; audit: `ctpf-753m`. This audit concerns new authored
inputs and ownership. Kernel parallelization stays with `ct-wuci` and `ct-tu55`.

## Observed behavior and missing acceptance

| Capability | Existing evidence | Missing feature |
| --- | --- | --- |
| Explicit mechanical graph, sideways/angled force and moment transfer | `structural_gpu.c`, equilibrium/buckling/impact tests | No second solver required. |
| Terrain anchors and UNKNOWN residency | root-identity, streaming and submerged-load tests | Painted anchor hints cannot create graph or support; arbitrary authored shapes require explicit graph metadata. |
| Delayed damage, root loss, wood splitting, stone fracture, mortar debris | `structural_failure.glsl`, fracture/failure/material visual tests | Existing rectangular member conversion has no immutable asset/object/source-texel ownership. |
| Exact mixed rigid geometry, holes, arbitrary connectivity | component/pixel tests and extraction birth passes | No direct canonical map placement API; terrain extraction cannot enforce separate authored role/object groups. |
| Structural collision authoring | `fr_gpu_place_structure`: rectangular convex proxy, length/depth at most 16, horizontal stone-only marking | Exact arbitrary painted collision, member ownership and atomically mixed terrain/rigid/structure placement absent. |
| Source reservation/rollback | structure placement backup and conversion reserve-before-retire | Not a shared body/shape/graph/appearance transaction; no cross-channel idempotence. |
| Save/reopen graph and geometry | version-five graph checkpoint tests | Authored UUID/object/source-frame/provenance and authored transaction replay absent. |
| Appearance/emission | renderer separate 32-byte appearance ABI; exporter compiled 64-byte records | GPU admission/provenance hooks are pending on renderer branch. Do not install a duplicate texture allocator. |

## Submission envelope

`fr_authored_request_t` uses immutable preloaded selected-state arrays, not a
renderer-frame collider replacement. Canonical physical records are 8-byte
little-endian material-u16/role-u8/anchor-u8/object-u32; roles are terrain1,
structure2, rigid3 and decoration4. Appearance records are the exporter's 64-byte
linear appearance/emission/alpha/surface-mask contract. Both arrays have exactly
width*height records; 4,194,304 is the preloaded canvas limit, not a promise that
every runtime pool admits that canvas. Source frame is immutable provenance;
state variant is an explicit physical placement selection.

World location is signed64 chunk plus canonical local cell in [0,64). Pivot is
an integer authored reference: canvas top-left = origin - pivot. Every extent and
chunk carry is preflighted without chunk*64 arithmetic. Pivot never defines mass
center, buoyancy or support. Asset UUID and transaction ID must be nonzero.
The CPU envelope check is bounded; it performs no per-cell validation, component
solve, property reduction or runtime GPU readback.

Structural placement additionally requires the explicit mechanical graph and an
aligned little-endian u32 member-ID plane. Per-pixel ownership must agree with
actual member rest geometry and material composition. Missing metadata is a
diagnostic, never an implicit rectangle or support pin. This extends producer
metadata beyond the current exported package; renderer/exporter integration must
keep that unmet contract visible until explicit graph metadata is available.

## GPU transaction passes

1. Validate canonical codes, channel floats, extents, mapped chunk generations,
   overlap, structure/member-plane agreement and actual terrain-contact evidence.
   UNKNOWN input defers the entire transaction without publication.
2. Label rigid cells by four-connectivity within role/object identity. Diagonal
   adjacency alone does not connect cells. Separate disconnected islands remain
   separate bodies even under one object label. Structural ownership is explicit
   member/graph metadata, not inferred from color or arbitrary painted topology.
3. Parallel tile masks/scans and cooperative material-property reduction derive
   body/shape/graph and appearance/provenance/emission demand. Adopt the optimizer's
   common geometry/property helpers after its checkpoint; preserve Body544-byte
   and sparse16x16 geometry ABI rather than replacing optimized kernels.
4. Reserve all participant pools into a transaction plan. A GPU failure bitfield
   gates every participant's commit; preflight never clears terrain/source cells.
   Transaction ID plus immutable input identity rejects changed-input replay and
   makes completed replay idempotent. IDs/generations cannot attach old ownership
   to reused slots. A failed reservation retains the sole prior authoritative state.
5. Transfer exact physical/material and source appearance records into reserved
   output ownership, then publish through one shared GPU gate. Terrain remains
   ordinary terrain and is eligible for automatic extraction; no extraction
   exemption, synthetic pin or forced float/sink override. Invalidate extraction
   and occupancy work only after publication.

Renderer admission consumes the shared transaction status, canonical appearance
source buffer and per-source destination map. Proposed provenance is16bytes:
appearance identity-u64, source pixel-u32, source/reference frame-u32. The persisted
appearance identity table maps asset UUID+authored object+placement instance;
transaction identity retains that instance through replay. Runtime body IDs and
storage generations remain separate. Signed64 chunk/local source ownership survives
rebasing. Decoration outside physical occupancy has asset-instance rest ownership;
it may render/emit without adding collision or mass. Renderer admission participates
for decoration/emission-only texels too. The16-word status reserves provenance
demand8, emission demand9, appearance instance10 and generation11/12. Destination
records are64bytes; geometry_word is an absolute sparse shape-header word,
rest_x/y resolve the target tile directory and tile_cell is0..255. Terrain and
decoration use UINT32_MAX body/shape sentinels. Buffer views carry byte offsets,
so source planes and status may share one allocation. Color alpha never changes
physical occupancy.

## Required follow-on tests

`ctpf-iueo`: exact concave mixed props; diagonal/disconnected islands and object
partitions; explicit structural shape mapping and real roots; ordinary detached
terrain; overlap/capacity/UNKNOWN/generation/replay rollback across all channels.

`ctpf-9jiw`: sound root detachment versus actual split/crack/crumble; rotated
fragment-origin source maps; material/mass/momentum and work conservation; equal
simulated-time damage/grace; saturated whole-transaction retries. Existing fracture
algorithms are reused, not reimplemented serially.

`ctpf-5h0n`: versioned source and replay records; save/reopen during damage/grace
and pending reservation; rebase/eviction/recycled-slot rejection; coherent old-save
defaults and corrupt ownership rejection. `ctpf-ti75` retains combined visible
acceptance and coordinated integration with renderer/exporter.
