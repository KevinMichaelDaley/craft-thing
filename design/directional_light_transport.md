# Directional radiance transport

The Vulkan renderer stores incoming RGB radiance in directional beam
buffers, including vertical RGB first moments. Point sources
inject emissive energy at their cells. The sun injects energy at explicitly
certified sky boundaries. Unknown residency blocks transport.

Three deterministic stages transport cardinal direct light, diagonal sky/point
light with diffuse injection, then cardinal diffuse light. Sky entering on
diagonals carries 35 percent of sun radiance per direction, representing the
diffuse sky hemisphere. Point emission on diagonals uses half the cardinal
share. Every stage adds its RGB energy and signed moments to the final field.
Diagonal paths let canopy shadows open up with distance; lateral transport
fills beneath them instead of preserving the widest silhouette as a column.
These are within-frame stages: previous frames do not accumulate residual
light or flicker. The two intermediate beam buffers are reused across stages.

Every row, column and diagonal is solved with associative affine prefix scans in
128-lane tiles. Tile totals are scanned recursively and offsets propagated
back to cells. There is no CPU pixel traversal or serial per-row GPU walk.
Persistent buffers are allocated with the renderer; paused unchanged scenes
skip transport through the existing GPU dirty gate.

Leaves transmit 92 percent of incident light and loose leaf fragments transmit
96 percent, including moving bodies. Other solids remain opaque. Air retains
99.9 percent per cell; diagonal steps account for both axes. Direct beams
retain 99.5 percent forward radiance and diffuse beams retain 99 percent,
with one percent injected into the next stage's four directions.
Thus underground illumination declines with path length and absorption instead
of receiving a constant ambient floor. The configured propagation reach bounds
source work. Three stages are an approximation, not a converged global
illumination solver. Surface normals only affect final appearance after actual
transport has produced the directional field.

The final field stores RGB energy and signed RGB first moments independently
of material/albedo. Opposing lights preserve energy while their moments cancel.
The two beam buffers require 256 bytes per scene cell, plus scan totals and the
64-byte final field. GPU cost and memory therefore need measurement before
raising simulation resolution; renderer performance remains separately tracked.

Regression coverage includes a wide eight-pixel-thick foliage canopy: light
below it must recover by at least ten percentage points with distance,
relative to the unobstructed field at the same coordinates. Sealed opaque
layers still block all sky light; no ambient floor is added underground.
