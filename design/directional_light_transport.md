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
light or flicker. Per-direction local prefixes and the summed incoming beam
from the preceding stage are reused across stages.

Every row, column and diagonal is solved with associative affine prefix scans in
32-lane tiles. Tile totals are scanned recursively and offsets propagated
back to cells. There is no CPU pixel traversal or serial per-row GPU walk.
Devices supporting compute subgroup relative shuffle use register prefix
composition within subgroups, then scan subgroup totals. Other devices retain
the shared-memory implementation with identical transport equations.
`DC_LIGHTING_SHARED_SCAN=1` selects that fallback for diagnostic comparisons.
Phase and transport-stage branches are pipeline specialization constants.
Point emission and propagation reach are seeded once per cell, and empty
padded diagonal tiles write identity totals without scanning absent pixels.
Persistent buffers are allocated with the renderer; paused unchanged scenes
skip transport through the existing GPU dirty gate.

Leaves transmit 92 percent of incident light and loose leaf fragments transmit
96 percent, including moving bodies. Other solids remain opaque. Clear water
does not contribute to solid optical occupancy: its bulk mass previously
received leaf-like extinction at every pixel, producing artificial shafts.
Water retains its independently composed color and surface appearance; it
injects no radiance. Air retains
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
Persistent local prefixes, prior-stage sums and source seeds require 120 bytes
per scene cell, plus scan totals and the
64-byte final field. GPU cost and memory therefore need measurement before
raising simulation resolution; renderer performance remains separately tracked.
Intermediate RGB energy and affine transmission retain FP32 range and
precision. Vertical RGB direction ratios use signed ten-bit components
(at most 1/1022 quantization error per packing); final RGB moments remain FP32.
This avoids FP16 overflow for HDR emitters. Subgroup scans retain FP32 working
values in registers rather than round each prefix composition.

`renderer_performance_tests` measures complete simulation-plus-render frames
at 480x270 and 640x360 using the same native physics settings (16 pressure
sweeps). The acceptance is less than 30 ms per complete frame on this machine;
the former 2 ms renderer-only goal is superseded. Per-stage GPU times diagnose
cost, and `DC_RENDERER_PASS_PROFILE=1` enables extra per-pass timestamps.
Do not describe runs overlapping another GPU application as isolated.

Regression coverage includes a wide eight-pixel-thick foliage canopy: light
below it must recover by at least ten percentage points with distance,
relative to the unobstructed field at the same coordinates. Sealed opaque
layers still block all sky light; no ambient floor is added underground.
