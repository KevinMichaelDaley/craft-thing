# Locked fire/smoke inputs and pre-feature baseline

Ticket: `ctlo-ck1i`. Fixture schema 1 is stored in
`assets/fixtures/fire_smoke_v1.json`; its final canonical sorted-JSON SHA256 is
`fbaf568bd7a4d2693696073ff455afb232843e4eb0dc1021802c07568ceded95`.
This locks reference inputs before reactive runtime work. The numbers are
synthetic gameplay parameters, not measured physical material data.

`tools/diagnostics/fire_fixture.py` validates the manifest, rejects invalid material
identity/composition/energy, exports exact little-endian material/role/object/wetness
maps, copies the complete scene manifest and records per-channel hashes. It does
not pretend those maps are already a runtime implementation of fire or a catapult.
`make test_fire_fixtures` checks five contracts including deterministic geometry,
the real ballistic path, distinct ash identity, controls and invalid inputs.

## Frozen units and cases

The visible grid is 480x270; the 640x448 allocation includes a 64-cell halo and
chunk rounding. A rigid tick is 1/60 second with eight substeps and eight Jacobi
iterations; MPM retains two substeps. Existing rigid gravity is equivalent to
900 cells/second squared and the velocity bound to 240 cells/second. Reactions
must consume accepted level simulation seconds once, independent of render-only
frames and of rigid inner iterations. Tests for that adapter belong to reactive
state/heat integration; this baseline does not change mechanical time.

The bonfire includes a 16x4-cell wood pile, thin wood sticks, a three-cell stone
pit and four trees with a maintained-wet control. Its duration is 180 seconds.
Still and +/-1-cell/second crosswind controls, a dry-fuel bridge, a broken stone
ring and a warmth probe are fixed in the manifest. The warmth band is 298.15 to
318.15 K over seconds 20-120, from a 293.15 K ambient.

The siege wall is 8x80 cells at x=154, y=150. The pivot is at (104,188), with a
36-cell arm and a four-cell-radius coal ball. At the intended real release,
the ball center is (104,148) and its target measured velocity is (210,0) cells/s.
Its unconstrained ballistic arrival is about 0.238 seconds later at y=173.51,
inside the wall. The arm's initial rotation is -0.83 radians; it releases its
drive latch at two seconds and the projectile latch on the actual angular
crossing. Target coordinates/velocities are measurements, never scripted writes.
The coal case runs 60 seconds, targets 10-35% wall loss, and retains a structural
core; the maintained-wet wall has saturation 0.95.

Greek fire is a continuous phase carried by a real three-piece wood cup, not a
rigid ball with a fluid name. The inherited coal object is removed. Its initial
16 fuel-mass units and 6.857142857 bound-oxidizer units reconcile with the locked
70/30 compound composition. The ordinary-oil control also has 16 fuel units.
The dry delayed-load ensemble has 64 distinct fixed seeds, a ten-second delay
and a 48/64 minimum ignition count; a stone carrier remains a control. Dousing,
submersion, foliage, an actor proxy, a coin and an ingredient have fixed inputs.

All cases use the documented first-second 25% visible coverage bound; smoke
coverage starts at opacity 0.2, flame coverage at linear emission 0.1. The final
performance gate remains two consecutive 1000-frame displayed runs whose mean
and worst complete-frame times are below 30 ms. Future implementation must
preserve these inputs or record an explicit design revision; test thresholds
cannot be relaxed to conceal a regression.

## Actual display measurements, 2026-10-24

Intel Iris Xe (TGL GT2), Vulkan, Mesa 26.0.2 kisak, native Wayland. No other
dungeoncraft process, concurrent build or GPU test ran during measurements.
No runtime source or shader differed from the parent performance baseline.
The new worktree compiled its own executable using the checksum-pinned SDK.

The existing level03 scene was loaded from fresh ordinary copies of the protected
equilibrium world, with repeated radius-five air cuts at (225,42). Lighting was
off, all runtime performance settings were defaults, and physics/render was 1:1.
The protected source `rigid_bodies.bin` SHA256 remained
`07115a01c9e50c90b946529ca89a3d31fd0e37a699b08bc1b20c1bd7571a12f8`.

| Pre-feature displayed run | Frames | Mean ms | Worst ms | Strict gate |
| --- | ---: | ---: | ---: | --- |
| Fresh copy 1 | 1000 | 23.284 | 48.363 | FAIL |
| Fresh copy 2 | 1000 | 23.088 | 55.194 | FAIL |

Both verified four identical paused captures. They ran at commit `ee657db` with
the initial fixture-manifest hash recorded in their report; the later fixture
correction distinguishes the Greek phase/carrier and does not affect this
existing level03 replay. These are a baseline of the common engine, not proof
that any new reactive scenario is implemented or passes. The previously recorded
24.079/29.334 and 24.133/29.147 passing runs do not erase these failing repeats.
Tail variability is also tracked in `ct-irzt`; the final feature gate stays open.

A separate instrumented 180-frame replay measured 23.116 ms complete-frame mean
and 29.122 ms worst. GPU timestamp averages/maxima in milliseconds:

| Region | Mean | Maximum |
| --- | ---: | ---: |
| Rigid | 9.790 | 12.504 |
| Fluid | 5.414 | 8.049 |
| MPM | 4.517 | 5.451 |
| Renderer | 0.690 | 1.249 |
| Extraction, contained in rigid | 0.257 | 2.692 |
| Broadphase, contained in rigid | 0.097 | 0.131 |
| Solver, contained in rigid | 7.773 | 8.998 |

This timestamp diagnostic is not a substitute for either strict 1000-frame run.
It observed up to 353 pairs and 461 contacts, with zero contact overflow.
The separate displayed native smoke reported 83.6 MiB mapped VRAM, zero mapped
system memory and 111.6 MiB device-only VRAM. Its 60 startup-inclusive ticks took
0.463 seconds, with a 45.4 ms maximum; startup is not hidden in the warmed average.

The manifest locks fuel-product partition, oxidizer accounting, retained char
energy and matching Greek/oil initial inventory. Actual gas/fuel/heat ledgers are
not implemented yet and are not reported as measured zero-error results. Their
runtime counters and conservation oracles are explicit downstream requirements.

Final validation: `make test_fire_fixtures` passes all five contracts. The complete
`make -j8 -i test` run reports 660 passing C cases plus the new Python checks,
zero compiler warnings and only the same two pre-existing authored-stage failures
at lines 67/135, tracked in `ctr-ojvy`. No existing assertion was relaxed. This
fixture/baseline milestone does not claim that the final frame-budget gate passes.

## Reproduction

```sh
make test_fire_fixtures
python3 tools/diagnostics/fire_fixture.py --output-dir build/fire_maps_new
./tools/with-vulkan make -j8 build/dungeoncraft_quarter_native
env SDL_VIDEODRIVER=wayland python3 tools/diagnostics/fire_fixture.py \
  --baseline-world /path/to/unchanged/world_chunks_level_piece03_equilibrium \
  --output-dir build/fire_baseline_new
```

The output directory must not already exist. The tool makes independent world
copies, retains raw logs/captures/device details and writes a JSON report after
each run. It rejects unexpected `DC_` overrides and exits nonzero when the strict
budget fails; no source world is rewritten. See existing `quarter_app_frame_trace`
for the separate timestamp diagnostic and `--smoke-native` for buffer statistics.
