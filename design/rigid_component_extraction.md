# GPU rigid component extraction

`dc_gpu_enable_automatic_components` enables extraction before the ordinary rigid
solve. The existing GPU root hooking pipeline uses a rigid-material predicate:
stone, wood and dense wood. The default headless context remains author controlled.

Four-connected resident material cells share a root across warm page seams. Each
candidate verifies resident page world coordinates. A component touching a missing
page or viewport boundary remains terrain: cold space cannot prove detachment.
Roots exceeding 256 cells or a 16-cell side, or requiring more than four exact
non-overlapping material rectangles, remain unchanged. These bounds match the
existing body representation and do not approximate unsupported geometry.

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

Larger or more complex exact extraction is tracked by ct-wuci; those deferred
roots remain terrain until a supported representation is available.
