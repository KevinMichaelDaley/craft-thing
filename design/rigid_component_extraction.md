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
Native activation, saturated retries, streamed ownership and wet seam composition
remain acceptance work in progress. Four cases prove isolated gravity, conservative
cold boundaries, removal of the final support and checkpoint continuation.
