---
id: ctfs-dmxv
status: in_progress
deps: [ctfs-2hu1]
links: [ctlo-r2ap, ctlo-h9of]
created: 2026-10-25T02:54:23Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, modular, gameplay]
---
# Deliver a visible smoke ventilation box as the next modular system

Build only the gas/oxygen prerequisite needed for the first ordinary terrain bonfire, after the interactive thermal system is delivered. Use a permanent smoke-box scene and injection/vent interaction rather than requiring all future gas/liquid/Greek-fire paths.

## Design

See the modular delivery section of design/fire_smoke_reactive_materials.md. This user-authorized sequence takes precedence over completing broad horizontal foundation tickets first.

## Acceptance Criteria

RED-GREEN-REFACTOR. The placement demo can inject finite colored smoke into an enclosure, open/close a real vent and show buoyant/sideways flow and oxygen concentration. Demonstrate variable-density/expansion behavior with reference-validated pressure/CFL policy; conserve species/energy/pigment and distinguish dilution from oxygen consumption. Walls and chunk edges behave correctly; renderer works with propagated lighting off. Actual displayed Vulkan tests, screenshots, pause/seed/reset checks and full-frame timings are mandatory. No placeholder smoke animation, second authoritative gas solver, silent inventory loss or implementation of submerged Greek-fire coupling yet. Keep broad ctlo-r2ap/ctlo-h9of open for remaining scope; report/push this visible system before combustion.


## Notes

**2026-10-25T02:56:00Z**

Start after the interactive heat/wetting system is runnable, displayed-tested and pushed. Deliver a real smoke/ventilation system before adding combustion; keep future submerged Greek-fire coupling explicitly open in the broader gas ticket.

**2026-10-25T07:57:31Z**

Starting only this visible module after ctfs-2hu1 delivered/pushed7626e67: finite colored smoke enclosure with real vent and oxygen feedback. First RED tests will lock sealed heating/compression/expansion and CFL pressure policy against a small reference before Vulkan production. Broad gas/render tickets remain open for combustion and submerged Greek-fire scope; no future chemistry/owner expansion in this slice.

**2026-10-25T08:31:09Z**

Reference substage RED258de51 now locally GREEN6/6 with zero compiler warnings: mixture EOS/molar oxygen, sealed heating with finite energy, reversible compression/expansion and boundary work, finite colored packet debit/mixing, contact-preserving HLLC vs HLLE pressure release, species/energy/pigment conservation and transactional CFL rejection/pause. This is a cold CPU oracle only; no runtime CPU field solver/readback/list is being added. Production sound/substep policy has NOT yet been selected and no smoke-box completion or visual/performance claim is made. Next strengthen2D wall/vent and independent reference comparisons, then the GPU visible box.

**2026-10-25T09:53:53Z**

Vulkan transport GREEN checkpoint: actual Wayland2/2 cases pass with subgroup CFL reduction, exact resting/pause state, independent-reference colored wall-opening transport, energy error3.43876891e-05 and red error-2.10703321e-08. Existing thermal feedback actual-display2/2 also passes; zero compiler warnings. Parallel GPU banks retain canonical stock on invalid intervals; no runtime field/component readback. Interactive buoyancy, calibrated complete-frame performance and persistence remain acceptance work; this ticket stays in_progress.

**2026-10-25T10:22:59Z**

Implicit acoustic numerical checkpoint: CPU3/3 reference cases and actual Wayland4/4 GPU cases pass, keeping original HLLC tests. Compensated pressure removes a float residual floor at unchanged1e-7 tolerance; reduced scalar convergence feedback only, no runtime CPU field solve. Full-field performance is still RED (first compensated diagnostic193.265ms), with precomputed-stencil/converged-row optimization under test. Interactive controls, vacuum/geometry admission, persistence/streaming and final repeated1000frame performance remain unfinished; no ticket completion claim.

**2026-10-25T10:58:51Z**

Sparse Vulkan numerical checkpoint:6/6 actual Wayland cases pass with core AND synchronization validation enabled, no VUID/SYNC errors. Includes exact resting state under both policies, reference-matched implicit colored transport, and8192-cell cross-chunk color/pressure transport. Seam energy error-9.50787216e-06 and red error9.98786254e-08. GPU compaction has capacity for every tile, no CPU list/cell readback or discarded stock. Last measured full-frame40.809/58.540ms remains RED; fused advection/metadata performance is under measurement. Interactive box, vacuum/geometry admission, streaming/persistence, long timing runs remain open.

**2026-10-25T12:59:32Z**

Heat milestone remains closed. Smoke runtime now has finite colored reservoirs, real GPU face valves, molecular-O2 overlay, and automatic physical-clock advancement. Eleven native Vulkan gas/reference/conservation cases pass, and the actual placement scene test passes. Pushed f3e00bf includes dry cold-scene factory and cached implicit thermodynamics. Full-frame performance is still RED: short real-display probes ~33–48ms at 1/60s and 265–420ms for 0.25s catch-up. GPU-only activity compaction is under validation; no changed clock, state truncation, fixed cap or reduced solver tolerance. Remaining current-module work includes required performance, supported edit/ownership geometry, persistence/streaming and final repeated displayed acceptance; bonfire has not started.

**2026-10-25T16:12:22Z**

Pushed18261fe adds exact GPU dependency footprints for RHS/activity/convergence classification. Actual native14/14 pass, including20-step byte-exact dense/sparse comparison across gravity and valve changes; displayed enclosure1/1. Core+synchronization validation14/14 also passes with DC_VK_VALIDATE=1 VK_VALIDATION_VALIDATE_SYNC=1 and zero VUID/SYNC errors. Best short16x2 sweep diagnostic23.782ms mean34.348ms worst remains RED; strict repeated1000-frame acceptance is NOT met. Current optimization removes a redundant finish-submission wait using GPU residual/error guards. No bonfire work or clock/resolution/stock/tolerance changes.
