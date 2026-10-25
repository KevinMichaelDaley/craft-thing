# Interactive heat and wetting bench

Delivered visible milestone: `ctfs-2hu1`, before smoke or burning.
The scene uses matching stone/wood contact benches, one dry and one receiving
water. Temperature and wetness have separate overlays, independent of authored
color. A finite contact probe lets the player move heat into nearby matter.

The single shared C/GLSL parameter source is
`include/dungeoncraft/thermal_parameters.h`. Ambient is 293.15 K. The probe starts
at 350 K with heat capacity 200, holding 11,370 energy units above ambient.
The initial hot stone is also 350 K. Heat exchange consumes finite reservoir
energy; temperatures are not clamped. By the diffusion maximum principle, these
sources keep this first module below the existing 373.15 K boiling threshold.
Steam, gas convection and ignition remain later visible features rather than
being cosmetically represented here.

Thermal face flux uses harmonic conductivity and the accepted physical interval,
with a four-neighbor monotonicity bound for large time steps. Water uptake is at
most 0.06 mass units per second across all four faces, bounded by available liquid
and the substrate's material-derived capacity. The same Q16 mass and proportional
enthalpy leave liquid and enter bound water. Dry reference mass is metadata, not
another heat inventory. Pause and rendering alone do not advance either process.

RED reference checks lock material-derived wet heat capacity, signed conductive
flux, four-face stability, finite probe energy, wetting mass/enthalpy conservation,
preserved parcel/hazard history and transactional invalid-input/pause behavior.
The runtime module must additionally pass GPU parity, real display interaction,
capture, whole-frame measurements and the edits/persistence it exposes. A passing
CPU reference alone does not complete the visible milestone.

## Interactive bench

`tools/with-vulkan ./build/dungeoncraft_quarter_native --heat-bench` starts a
fresh placement world. Hold H and the left mouse button for finite probe contact;
F7 cycles temperature, wetness and authored color. Existing material brushes,
camera controls, pause/single-step and reset remain available. The left wood
control is dry; the right starts with real material-derived bound water. These
controls demonstrate conductive heat and wet heat capacity. The liquid extension
now stores excess enthalpy separately from pigment and solid parcel state. Its
displayed reference checks pass exact Q16 uptake, conductive cooling, and heat
and pigment conservation during actual water movement. Wetting is committed
before a fresh conduction snapshot, avoiding simultaneous-transfer overshoot.
Both ordinary fan-out and subgroup prefix transport carry the same channel;
default-disabled shader fixtures bind an explicit zeroed thermal control.
Wood/stone painting admits canonical parcels, while natural owner handoffs keep
their extensive heat with the parcel. The scoped checkpoint/streaming paths
below preserve the bench's source, solid state, liquid heat and authored colors
through quitting/reopening and camera eviction. Reset creates an explicit fresh
world. Arbitrary-body heat exchange and combustion remain later systems.

The native bench test passes probe heating, pre-wet state, overlay switches and
four identical paused frames. Actual captures are
`build/screenshots/thermal_bench_temperature.bmp` and
`build/screenshots/thermal_bench_wetness.bmp`. The normal executable also ran on
the real Wayland display and produced `build/screenshots/thermal_cli_proto.bmp`.
Its first 120 complete frames measured **10.808 ms mean / 36.530 ms worst**;
a traced 180-frame run measured **10.574 / 45.921 ms**. Both **fail** the 30 ms
worst-frame target. The largest traced tail included 20.553 ms input processing
and 25.342 ms presentation/physical work. These failures remain part of the
record; no module or epic performance acceptance is claimed.

Liquid-stage regression boundary (`e8e22ea`): the full headless battery reports
718 C passes, the same two `ctr-ojvy` authored-stage failures, and zero compiler
warnings (`build/thermal_water_full_suite.log`). Four real Wayland liquid cases
pass with core/synchronization validation, including erasure and physical-slot
replacement. Separate existing thermal, bench, transport, prefix, pigment and
partial-recipient regressions report 20 passes without validation errors. These
checks cover the liquid substage, not completion of this visible milestone.

Newly painted wood/stone now obtain canonical ambient parcels on the GPU when
the thermal module is enabled. Before physical mutation, the host reserves a
bounded brush rectangle using only arena counters and a unique 64-bit identity
range. GPU lanes construct state and publish handles; existing same-material
parcels retain history. Device copies grow storage rather than imposing a parcel
quota. Three native cases check canonical state/probe parity, replacement identity
and five reuse cycles of a 613-cell brush; sixteen existing arena, ownership,
brush, liquid and bench cases also pass without Vulkan validation errors. This
is the bench's wood/stone admission subset, not all future materials or owners.

Hover feedback now reports temperature in kelvin and material-relative wetness.
The visual adapter (`thermal_feedback_gpu.c` / `thermal_overlay.comp`) consumes
read-only thermal snapshots and the finite probe reservoir. The solver no longer
binds a framebuffer. A cold single-point inspection API is diagnostic only;
gameplay draws the values directly on the GPU. Actual captures show 317.1 K /
100.0% for heated water and about 296.0 K / 0.0% for the probed dry bench wood.
The saturated test fixture derives water quantity from the profile's actual
moisture capacity (wood is 0.5, not the mistaken initial 0.35 assumption).

`tools/with-vulkan make test_heat_bench` builds/runs thirteen CPU thermal,
precision and worker checks and twenty-two displayed Vulkan checks serially.
These visual cases also belong to
`test_ui` and `test_quarter_native`; the CPU reference belongs to `test`.
The first 18-case run passes with core/synchronization validation in
`build/thermal_feedback_module_display.log`; the additional placement pour passes
with the original bench case in `build/thermal_pour_red_display.log`. Remaining
The subsequent checkpoint, camera and precision coverage is recorded below.

The placement-path pour case also passes: a radius-three water brush above
warmed wood injects 29 finite water units. After one real physical interval,
the four-chunk reference region retains exactly 1,900,544 Q16 water quanta and
energy differs from its explicit ambient-water input by -0.000042149 units
(locked absolute tolerance 0.02). The wood cools from 295.968906 K to 295.936791 K,
absorbs 16 quanta and retains identity. After another second, the actual wetness
capture shows about 294.7 K / 3.8% and water flowing into the basin:
`build/screenshots/thermal_bench_water_pour.bmp`. The cold snapshot helper passes
the explicit logical chunk coordinates required by the physical download/export
API. This adds one integration case to the standalone/display batteries.

## Current complete-frame display evidence

`tools/bench_heat_bench 1000` is the permanent reproducible tool. It builds before
timing, then runs two dry-control trials and two trials with one finite placement
water pour at measured frame 8. It creates fresh worlds and serializes all four
real Wayland Vulkan jobs; no builds run during measurement. The existing repeated
air-erosion input is preserved. Three RED/GREEN input tests lock strict visible
bounds, unchanged output on malformed input, and exactly one pour versus repeated
erosion after the unchanged eight-frame warmup.

Current results (`build/heat_bench_measurements_aaT6PO`, with aggregate log
`build/thermal_repeated_display_benchmark.log`):

| Trial | Frames | Mean ms | Worst ms |
|---|---:|---:|---:|
| Dry 1 | 1000 | 13.771 | 15.309 |
| Dry 2 | 1000 | 13.712 | 15.252 |
| Finite pour 1 | 1000 | 14.183 | 15.553 |
| Finite pour 2 | 1000 | 14.163 | 16.039 |

All four satisfy strict complete-frame mean and worst below 30 ms, and each
reports four identical paused captures. Visible physics/render remain 480x270,
active halo 640x448, eight rigid substeps/eight Jacobi iterations/two MPM steps.
Propagated lighting is off. Two earlier current dry runs also pass
(13.612/17.986 and 13.644/15.788 ms); the historical prototype failures above
remain recorded. These measurements cover the heat bench, not the epic's later
bonfire/launcher/Greek-fire scenarios and not unfinished ownership/save behavior.

The subsequent full default battery finishes **727 C passes**, the same two
`ctr-ojvy` authored-stage failures and **zero compiler warnings** in
`build/thermal_feedback_full_suite.log`. This is the safe checkpoint before the
now-needed bench save/restore transaction work. No DCS8 format was enabled by
the heat/feedback/benchmark changes.

Final persistence/precision validation and displayed timings are recorded below.

## Heat-bench checkpoint integration

The scoped DCS8 extension stores logical FRW1 reaction records and a THR1 liquid
and finite-source record beneath the existing outer checksum. THR1 has a
48-byte little-endian header and one logical-coordinate/4096-float record per
physical chunk. It locks profile ABI, ambient temperature and water heat
capacity. Zero liquid mass must have exactly zero excess heat; full enthalpy
must be nonnegative and finite. Parse/allocation errors preserve caller output.

Restore prepares detached reaction and thermal resources before installing any
physical chunk or publishing roots. Every pending file parcel is allocated up
front, including chunks outside the viewport; later streaming binds prepared
handles through the parallel kernel. Four actual-display transaction tests pass
exact parcel/liquid/source restoration into another slot, future-version
rejection and fourteen allocation-failure rollbacks against a newer live world.
The two extracted-body/pending negative-coordinate cases pass, as do four
interactive bench cases including reopening and explicit fresh-run reset.
Legacy structure save/stream regression passes 8+5 cases. The full default
battery reports 736 C passes, the same two tracked `ctr-ojvy` authored-stage
failures and zero compiler warnings/errors in
`build/thermal_persistence_full_suite.log`.

The extended pour regression found a genuine transport rounding defect: a full
withdrawal computed through reciprocal division could retain tiny excess heat
after its mass reached zero. Full withdrawal now transfers the exact inventory.
The real placement pour passes sixty subsequent physical steps with per-step
checks for orphan heat, and successfully writes its strict validated checkpoint.
Per-chunk DCC8 streaming now carries a validated immutable THR1 liquid packet
beside its physical/color/reaction packet and the outer checksum. Its source
field is canonically empty; the finite world probe remains solely in DCS8.
Three worker cases pass immutable mixed-channel round trips, invalid inventory
rejection without replacing a valid file, corruption/future-version rejection
and ambient compatibility for older chunk files. Existing reaction/world stream
regressions pass 3+7 cases. The real camera test evicts the initial region by
twenty chunks and returns: liquid heat bytes and the finite source are exact.
The inspected capture `build/screenshots/thermal_bench_stream_resume.png`
shows the restored water at 296.1 K and 100% wetness.

## Precision regression caught by the extended reference

The sixty-step closed pour initially lost 0.027087608 energy units, outside the
existing absolute 0.02 bound, while keeping exactly 1,900,544 water quanta.
A common-quantum conductive-flux trial still lost 0.024460072 and was removed.
The final module retains low-order full-enthalpy accumulation instead: ABI 3
uses two formerly reserved words for a normalized heat remainder and its
accumulation correction, leaving the state at 28 words. The parallel shader
uses explicit error-free two-sum operations; CPU reference/temperature/split
and budget readers consume the complete energy expansion. Tiny transfers are
retained rather than cut off, and no FP64 device feature is required.

The native extended pour now closes at -0.000357833 units under the unchanged
0.02 bound, with exact water mass. Four precision references pass 100,000
sub-ULP additions (error -2.52e-10), split/serialization, invalid-input rollback
and ABI 2 migration. Existing ABI 1/2 zero-reserved records remain readable;
nonzero old reserved bits and future ABI versions remain rejected. The previous
full-width ABI 2 particle-birth check now explicitly reads an actual ABI 2
packet, preserving that migration test rather than dropping its assertion.
Native core/water/checkpoint cases also pass, including nonzero remainder
restoration and fourteen allocation-failure rollbacks. The post-change full
default suite reports 740 C passes, exactly the two known `ctr-ojvy` failures
and zero compiler warnings/errors (`build/thermal_precision_full_suite.log`).
The permanent 35-case native/reference heat battery passes with core and
synchronization validation (`build/thermal_precision_heat_battery.log`). A
strengthened repeat also passes both extracted-body/pending cases with both
nonzero compensation words and the four precision references
(`build/thermal_precision_owner_display.log` / `thermal_precision_owner_reference.log`).
The earlier 736+2 full result predates the precision change.

## Final displayed gate

The final `tools/bench_heat_bench 1000` run after persistence/precision changes
passes all four actual Wayland Vulkan trials. Build work finished before timed
runs, and no other GPU jobs overlapped them. Evidence is in
`build/heat_bench_measurements_RbPxN4` and
`build/thermal_final_display_benchmark.log`.

| Trial | Complete frames | Mean ms | Worst ms |
|---|---:|---:|---:|
| Dry 1 | 1000 | 13.436 | 21.115 |
| Dry 2 | 1000 | 13.422 | 15.621 |
| Finite pour 1 | 1000 | 13.884 | 15.700 |
| Finite pour 2 | 1000 | 13.889 | 15.742 |

Every mean and worst is strictly below 30 ms; all four runs produce four
identical paused captures. Each pour occurs exactly once at frame 8 through
the existing placement API. Physics/render remain 480×270, active halo 640×448,
eight rigid substeps/eight Jacobi iterations/two MPM substeps and the same
physical clock. Propagated lighting is off. The source and material budgets
remain real simulation state, including save/reopen/reset/camera interactions.
This delivers the scoped heat/wetting module. It does not complete the broader
heat/phase-change/owner tickets or certify the future bonfire/siege/Greek-fire
scenarios; those retain their own functional and performance gates.
