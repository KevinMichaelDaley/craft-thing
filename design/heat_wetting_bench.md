# Interactive heat and wetting bench

Active visible milestone: `ctfs-2hu1`. Deliver this module before smoke or burning.
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

## Current interactive prototype

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
Natural edits and persistence remain under development. The prototype refuses restored worlds until
the required thermal checkpoint path is validated, rather than silently
discarding their heat history. This restriction must be resolved for acceptance.

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

`tools/with-vulkan make test_heat_bench` now builds/runs six reference checks and
thirteen displayed Vulkan checks serially. These visual cases also belong to
`test_ui` and `test_quarter_native`; the CPU reference belongs to `test`.
The first 18-case run passes with core/synchronization validation in
`build/thermal_feedback_module_display.log`; the additional placement pour passes
with the original bench case in `build/thermal_pour_red_display.log`. Remaining
ownership/edit/save interactions are still acceptance work, not waived gates.

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

Remaining in this milestone: complete liquid regression/interaction coverage,
temperature/wetness inspection, natural edits/owner admission and the persistence
the exposed scene needs, plus reference budgets and passing displayed timing.
