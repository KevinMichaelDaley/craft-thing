# GPU rigid component extraction

`dc_gpu_enable_automatic_components` enables extraction before the ordinary rigid
solve. The existing GPU root hooking pipeline uses a rigid-material predicate:
stone, wood and dense wood. The default headless context remains author controlled.

Four-connected resident material cells share a root across warm page seams. Each
candidate verifies resident page world coordinates. A component touching a missing
page or viewport boundary remains terrain: cold space cannot prove detachment.
Small roots retain the existing exact rectangle representation when possible.
Other roots use variable geometry in a GPU-owned double-bank arena appended to
the body storage. A pixel body keeps one stable body ID and stores a four-bit
material for every local bounding-grid cell; zero denotes a hole. Its flagged
piece count records the occupied volume, and its geometry pointer addresses the
packed arena. Size, concavity and rectangle count no longer reject extraction.

A single GPU commit orders candidates deterministically, reserves a free body slot
and a unique persisted birth serial, computes material-derived properties, publishes
the owned body, then clears exactly its source material cells. Water remains at its
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

Parallel root masks reuse the label-hooking scratch buffer after convergence. A
single ordered commit scans those masks, preserving deterministic birth IDs without
serially visiting every cell. A GPU dirty/pending word skips unchanged terrain;
explicit material edits, uploads, page changes, origin changes and restore invalidate
it. A representable component blocked by full storage keeps the word pending for
retry. Ordinary fluid evolution still updates body forces every tick. This adds no
allocation and performs no runtime CPU physics copyback.

Version-four graph checkpoints serialize occupied bodies' geometry as a canonical
blob with relative offsets, validate material codes and occupied counts before
installation, then relocate pointers into the destination arena. Older graph
checkpoints retain their previous checksum and load path. Explicit diagnostics
enumerate occupied pixels with their local coordinates and material codes; fixed
convex/compound diagnostics reject pixel bodies instead of truncating them.

The ct-wuci implementation is in progress. Pixel occupancy, rendering, clipping,
hydrodynamics, bounds and contact queries consume the packed geometry. Remaining
acceptance work includes pixel fracture, fair/scalable extraction, complex visual
regression and the measured quarter-native capacity/complexity sweep.
