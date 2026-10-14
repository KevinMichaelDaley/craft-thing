# GPU rigid component extraction

`dc_gpu_enable_automatic_components` enables extraction before the ordinary rigid
solve. Eligibility includes stone, wood and dense wood; granular materials remain
in the MPM path. The default headless context remains author controlled.

Four-connected known material cells share a root across warm chunk seams. The
authoritative chunk registry supplies connectivity, including cached chunks outside
the mapped viewport. A component touching an unknown neighboring chunk remains
terrain: cold space cannot prove detachment. Cached births retain their world
anchors; dynamics outside mapped simulation residency follow the existing streaming
freeze policy. Every birth uses variable geometry in a GPU-owned double-bank arena appended to
the body storage. A pixel body keeps one stable body ID and stores a four-bit
material for every cell in an occupied tile; zero denotes a hole. Its flagged
piece count records the occupied volume, and its geometry pointer addresses the
packed arena. Sparse 16×16 tiles and a directory retain exact occupied material
cells without allocating every cell in a large empty bounding rectangle. Size,
concavity and rectangle count no longer reject extraction.

A shared-memory workgroup connects the eligible cells of each chunk, then parallel
boundary tasks join roots across known seams. Union/find parent traversal and CAS
retries follow dependent graph links within each task; there is no lane that visits
an entire component or serially orders every root. Hierarchical prefix scans rank
candidate masks and reserve body slots, collision-free persisted birth serials and
arena ranges deterministically. Existing geometry is copied cooperatively to the
alternate bank. Source invocations mark occupied tiles and pack their own material
nibbles. Workgroups reduce integer weighted first moments and compensated central
second moments for material-derived mass, COM and inertia. Bodies publish only after
these preflighted phases, then source invocations clear exactly their own material
cells. Water remains at its
world cells for the existing conservative displacement pass. Pool saturation leaves
the source untouched for a later retry. No runtime CPU component solve or copyback
is involved.

Graph checkpoints preserve automatic-extraction activation and cold wood material.
Native level creation enables the pipeline, including for earlier saved worlds.
Acceptance cases cover isolated gravity, conservative cold boundaries, final-support
removal, wet mixed-material warm seams, saturated retry, composition checkpoints
and exact motion after cold eviction/remap. Snapshot slots retain one authoritative
record per world coordinate, including when streaming relocates that coordinate.
The body pool preserves source ownership until its rigid/MPM descendants disappear;
known MPM lifetime reclamation work is tracked by ct-3srq.

Parallel root masks reuse connectivity scratch after convergence. Prefix scans
process masks and the actual directory length instead of scanning a full pixel
array for each candidate. A GPU dirty/pending word writes zero indirect dispatches
for unchanged terrain;
explicit material edits, uploads, page changes, origin changes and restore invalidate
it. A representable component blocked by full storage keeps the word pending for
retry. Ordinary fluid evolution still updates body forces every tick. This adds no
allocation and performs no runtime CPU physics copyback.

Version-five graph checkpoints serialize occupied bodies' geometry as a canonical
blob with relative offsets, validate material codes and occupied counts before
installation, then relocate pointers into the destination arena. Older graph
checkpoints retain their previous checksum and load path; version-four dense pixel
geometry is upgraded on loading. Explicit diagnostics
enumerate occupied pixels with their local coordinates and material codes; fixed
convex/compound diagnostics reject pixel bodies instead of truncating them.

Pixel occupancy, rendering, clipping, hydrodynamics, bounds, fracture and contact
queries consume the packed geometry. The extraction cursor retries deferred roots
fairly. The visual regression exercises two distinct concave bodies, including a
mixed-material comb, collisions, rotation and paused-frame stability.

Pixel hydrodynamics uses one cooperative workgroup per body to detect nearby water.
Wet bodies compact into a GPU dispatch list. Separate workgroups reconstruct each
hydrostatic sample column with parallel boundary, volume and occupancy reductions.
Body workgroups distribute cell clipping and reduce buoyancy, drag and torque before
the existing gravity/prediction steps. Work counters expose total cells, maximum
per-lane visits and participating lanes for explicit tests. Geometry and water
queries retain their exact material/volume semantics.

Contact generation retains one scalar invocation per ordinary candidate and uses
one 64-lane workgroup per pixel candidate. Reset writes both indirect dispatches
on the GPU; pixel work is omitted when no enabled pixel body exists. A mapped,
full-sized terrain tile can be rejected only after all its cells are certified
noncontactable. Partial tiles and cold boundaries retain the conservative path.
Rasterization builds an ordered, conservative body list per 16×16 tile using
current and swept bounds, including adjacent fluid faces. Geometry, contact
resolution and fluid cadence remain unchanged.

`make test_pixel_performance` measures the exact 480×270 visual scene with the
quarter-native configuration and fluid interval two. It requires mean rigid GPU
time below 14 ms and whole-tick GPU time below 1/60 second. Run timing checks
without `DC_VK_VALIDATE`; run correctness and visual checks with validation.
The cold complexity sweep retains a 14 ms whole-rigid-frame assertion and currently
fails on the first active frame; profiling separates extraction from the first
solver/contact work. `component_profile` enables additional GPU timestamps only for
explicit capture tests. Ordinary frames perform no profiling readback. The ticket
remains in progress for cold timing, broader capacity sweeps and parallel pixel
fracture; its existing serial BFS/packing path is not claimed complete.
