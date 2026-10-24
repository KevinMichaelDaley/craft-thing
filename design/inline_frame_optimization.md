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
