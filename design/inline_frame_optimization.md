# Displayed frame regression investigation

The interactive quarter-native build uses 480×270 physics and 480×270 rendering,
then nearest-neighbor enlarges the image four times to 1920×1080. The 640×448
simulation allocation includes rounded resident chunks and a one-chunk halo;
it is not a higher-resolution simulation of each displayed pixel. The separate
640×360 target remains a separate performance task.

The saved level03 repeated-air-cut replay includes brush submission, simulation,
rendering and presentation in its full-frame timer. Lighting remains disabled
by default, and can be enabled with `--lighting` or F8. Optimizations retain the
two granular substeps, eight rigid substeps, eight solver iterations, fluid
cadence, arbitrary component geometry, custom colors and growing body storage.

The default profile now specializes granular phases, fuses compatible grid work,
uses the gather stencil, and alternates the two existing particle buffers between
substeps. The final substep restores the canonical particle buffer. Commit still
publishes cell metadata but no longer copies the particle pool. Cold uploads and
external particle deletions maintain both pools. Fifteen coupled reference ticks
compare complete downloaded chunks byte for byte, including erasure, reseeding,
reload and colored leaf debris.

Contact dispatch uses 16 lanes and literal four-vertex typed pixel modules;
general convex contact modules retain eight vertices. Conservative terrain
presence is computed once per solver batch rather than scanning every chunk for
every pair and substep. A packed body-boundary cache remains an opt-in experiment
(`DC_CONTACT_BODY_CACHE=1`), with no allocation when disabled. Its exact geometry
tests pass, but it did not improve the complete saved-scene frame time.

Safe solver apply work overlaps independent reductions. Zero reductions may be
skipped only when they do not omit structural dependency capture. Structural
SOLVE and IMPULSE variants retain the capture work even at zero load. Reference
overrides remain available through `DC_SOLVER_APPLY_OVERLAP=0`,
`DC_SOLVER_SKIP_ZERO_REDUCTIONS=0`, `DC_MPM_KERNELS=generic`,
`DC_MPM_GATHER_STENCIL=0`, `DC_MPM_FUSED_GRID=0`, and `DC_MPM_PARTICLE_SWAP=0`.

Caching divergence in the four-word pressure stencil was rejected: the complete
fluid specialization oracle detected a floating-point mismatch. Production
retains inline divergence evaluation and the original weight-only stencil.
The payload packing helper and diagnostic remain available for investigation.

All timings below used the actual Intel Iris Xe Vulkan device, a Wayland window,
a fresh copy of `world_chunks_level_piece03_equilibrium`, the level03 prop, and
`DC_BENCHMARK_AIR_BRUSH=225,42,5`. Builds and other GPU tests were not concurrent.

| Revision/configuration | Frames | Mean ms | Worst ms |
| --- | ---: | ---: | ---: |
| Before granular particle-buffer alternation | 500 | 28.158 | 34.860 |
| Particle-buffer alternation | 500 | 24.525 | 32.906 |
| Color address hoist, explicit profile | 500 | 24.158 | 29.915 |
| Defaults with original divergence restored | 500 | 23.976 | 29.559 |
| Longer repeat of those defaults | 1000 | 24.408 | 30.167 |
| Cooperative staged color copies | 1000 | 24.467 | 29.815 |
| Cooperative copies, repeat | 1000 | 24.960 | 30.562 |
| Fused visible albedo pass | 1000 | 24.079 | 29.334 |
| Fused albedo, repeat | 1000 | 24.133 | 29.147 |

The final two consecutive 1000-frame runs pass the strict worst-frame 30 ms
gate. Earlier isolated passes were insufficient; their failed repeats are
retained above. Each replay also verifies that four paused captures are
identical. These measurements do not establish the separate 640×360 target or
the optional propagated-lighting budget, nor guarantee every scene stays below
30 ms. The final worst-frame margin on this machine is less than one millisecond.

Staged color resolution now uses eight lanes per eight-texel word during body
carving. Each team reads its marker before a subgroup barrier permits alpha to
be overwritten. Padding teams participate, and subgroups smaller than eight
retain the serial implementation. Actual Vulkan synchronization validation
verifies both bank directions, zero fill, exactly 24 cooperative texel copies,
and three address/capacity calculations for three words. All 12 full carving
cases pass, including concavities, custom colors, leaves and structural members.

The unlit renderer refreshes each visible cell's palette and shades that cell
in one invocation. Surface shading only reads that cell's palette entry, so it
does not need the previous whole-view refresh dispatch and inter-pass barrier.
Generation statistics and paused behavior remain intact. `DC_ALBEDO_FUSED=0`
retains the two-pass reference; GPU tests compare exact pixels and verify one
visible dispatch. Propagated lighting keeps its separate transport path.

Reproduction (use a new world directory for every replay):

```sh
cp -a world_chunks_level_piece03_equilibrium build/frame_replay_world
env SDL_VIDEODRIVER=wayland DC_BENCHMARK_AIR_BRUSH=225,42,5 \
  ./tools/with-vulkan ./build/dungeoncraft_quarter_native \
  --scene assets/props/level_piece03.prop/asset.cprop \
  --world-dir build/frame_replay_world --benchmark-frames 1000 \
  --benchmark-capture build/frame_replay.bmp
```

The executable exits nonzero if the worst measured complete frame reaches 30 ms.
Use repository diagnostics, rather than changing physical cadence or reducing
geometry, to investigate a failed replay.

The final full `make -j8 -i test` run reports 660 passing cases, zero compiler
warnings, and two pre-existing authored-stage failures tracked in `ctr-ojvy`
(component-plan getter stub and absent-normal handling). They are not treated
as passing or removed from the suite.

The UI streaming smoke initially failed in both default and explicit reference
modes because it assumed a sand grain remained in its painted cell after a
physics step. Diagnostics showed identical saved/reloaded velocities and marker
counts, with that cell already air before saving. The smoke now compares the
complete captured post-step cell plane and every live particle byte, in addition
to its existing velocity, marker, halo, negative-coordinate and process-restart
checks. Both modes pass this stronger persistence check. Engine behavior was
unchanged by that test correction.

The complete Wayland `test_ui` rerun passes all commands, including streaming,
both concave-body visual scenes, material controls and coupled-water smokes.
The complete `test_quarter_native` run also passes all commands, including
solver, buoyancy, component extraction, structural cases, equal-time cadence
and graphical checks. The final quarter-native smoke reports 83.6 MiB mapped
VRAM and 111.6 MiB device-only VRAM. Its short startup-inclusive timing is a
different fixture from the warmed, saved-scene full-frame replay above.

## Bonfire continuation, 2026-10-28

The fullscreen bonfire has a separate failing total-frame acceptance ticket,
`ctfs-5gil`. Corrected diagnostic timestamps now retain all physical and gas
submissions through presentation. Short, real-clock coarse profiles on this
machine measured approximately111ms before thermal specialization,95.552ms mean
and98.797ms worst after specialization, and93.875/96.911ms after sharing each
fresh thermal sample's parcel lookup with its metadata lookup. Neither boundary
caching nor grouped solver reductions improved the full frame, so both retain
their existing defaults. No flame history is reused.

Complete scene replacement previously left the water-work latch enabled by
generated water that no longer existed. The replacement now retires that work
only when every physical slot has a known dry upload, with no intervening fluid
simulation or authoring mutation. Markers, face velocity, free water, and enabled
sources keep work active. Painting water invalidates the dry evidence and wakes
the solver. The native coarse profile then measured81.136ms mean/83.613ms worst
at unchanged480x270 display/physics with640x448 halo. These32-frame diagnostic
runs are not the1000-frame performance acceptance and do not meet30ms.

Filled rectangular pixel-body pairs now use their exact outer faces with up to
two endpoint constraints. The occupied-count/bounds-area equality proves there
are no holes; concave bodies keep their pixel collision path. Witness pixels
still supply each material and feature identity. A64x64 filled pair changed from
64 constraints/16 workgroups to2 constraints/1 workgroup, while the corresponding
concave fixture retained64 constraints/16 workgroups. Focused contact tests pass
12/12 and rotated scalar-reference comparisons7/7. The real-clock fullscreen
profile improved to74.580ms mean/76.701ms worst over32 frames. The displayed
bonfire initially passed, but subsequent displayed repeats failed the all8-log
ignition requirement. Consequently the rectangle path is experimental and off
by default (`DC_CONTACT_RECTANGLES=1` enables it for investigation). It is not
accepted as a default performance improvement. The full manifold remains the
default. A later thermal scheduling skip also failed the graphical requirement
and was discarded. Performance acceptance remains open.

Repeating with the original manifold also exposed deep-pit ignition sensitivity.
The authored structural rim is now shallow enough to admit side air above the
kindling, preserving the wood, starter, floor, and combustion coefficients.
Two shallow-rim runs ignited all8 dry logs and no wet logs by approximately31s
and44s. The latter exposed an additional test-ordering error: the scorer required
20s after the last log ignited, rather than20s of aggregate burning anywhere in
the pile. The new scorer observes that window independently, while still
requiring every log's existing fuel/flaming/visible-char checks, monotone chemical
consumption, at least10J consumed in the final5s, and active flames at completion.
Its regression first failed2/3, then passed3/3. The corrected native battery
passed with all8 logs at21.8s. Rigid versus thermal/gas clock coupling is separately
tracked in `ctfs-b6cz`; it has not been fixed by these optimizations.

The final default32-frame fullscreen profile is79.871ms mean/81.297ms worst,
with rectangle contacts disabled. The complete regression suite and native
water4/4 pass, alongside the corrected displayed bonfire battery. `ctfs-6t7k`
is closed; `ctfs-5gil` remains open because the30ms budget is still unmet.
