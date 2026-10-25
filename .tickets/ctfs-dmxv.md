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

RED-GREEN-REFACTOR. The placement demo can inject finite colored smoke into an enclosure, open/close a real vent and show buoyant/sideways flow and oxygen concentration. Demonstrate variable-density/expansion behavior with a stable, bounded-work practical pressure policy. Preserve meaningful species/pigment inventory, measure reasonable long-run numerical/energy drift, and distinguish dilution from oxygen consumption. Exact residual convergence, perfect per-frame energy and dense/sparse byte identity are diagnostic modes, not gameplay acceptance blockers. Negligible roundoff reconciliation is allowed; it must not trigger endless retries. Diagnose and remove delayed stalls using actual elapsed-clock frame/substep/retry, GPU/wait, activity and resource time series. State must stay finite/nonnegative and look convincing without runaway pressure/energy/velocity. Walls and chunk edges behave correctly; renderer works with propagated lighting off. Actual displayed Vulkan tests, screenshots, pause/seed/reset checks and sustained full-frame timings remain mandatory. Do not drop simulated time, meaningful stock, resolution or hide a stall with an average. Reject/document unsupported extreme parameter combinations at admission when justified. No placeholder smoke animation, second authoritative gas solver or submerged Greek-fire coupling yet. Keep broad ctlo-r2ap/ctlo-h9of open for remaining scope; report/push this visible system before combustion.


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

**2026-10-25T18:12:26Z**

Recovery after unrelated PC crash preserved existing work. Pushed2d663c3 corrects experimental large-CFL transport: independent counterflow RED51449d6 now passes with unsplit normalized-matrix/affine subgroup tridiagonal solve. Actual Wayland native19/19 pass, cold numerical+fixture21/21 pass and registered in normal test target. Default remains prior policy; core/sync validation and full elapsed-clock performance still pending. No interrupted benchmark treated as passed; no bonfire work.

**2026-10-25T20:37:27Z**

Latest user direction authorizes bounded practical pressure work, approximate residuals and negligible numerical cleanup; exact convergence/bit identity/perfect per-frame energy are diagnostics, not gameplay blockers. Fresh strict real-display40-frame trace: frame9 32.296ms,1 accepted,0 retries,5 submits; frame15 61.271ms as activity expands; frame47 723.408ms,6 accepted,2 retries,50 submits and673.359ms gas-fence wait. Acquire~0.05–0.07ms, present fence~1ms. Gas storage128576208bytes and RSS192380KiB stay constant, no major faults. Mean455.064/worst1404.065ms. Work expansion makes elapsed dt grow, then convergence/retry/substep catch-up multiplies GPU submissions. Next practical policy bounds pressure work and reconciles negligible roundoff without endless retries, preserving physical clock and meaningful stock.

**2026-10-25T21:05:39Z**

Practical thermal prototype passes two actual Wayland/Vulkan gates: unsupported force admission and 120 variable intervals spanning12 physical seconds with no retries, fixed256 SOR sweeps, full clock, finite T/P/velocity and meaningful stock/drift bounds. Worst64x64 gas call20.862ms; this is NOT full-frame acceptance.32/64 bounded sweeps failed density bounds. Practical thermal channel advects internal heat, pressure changes bulk momentum, with Mach0.5 guard and explicit numerical/mechanical work ledger. Strict compressible modes remain diagnostics. Next actual scene timing/visuals and current validation; save/edit/stream scope and milestone remain open.

**2026-10-25T21:10:27Z**

Opt-in GPU coarse pressure prototype (8x8 aggregate,128 coarse/128 fine sweeps) passes120-interval stability. Real40-frame trace49.102ms mean75.891ms worst, four paused captures identical. No retry/catch-up growth. Screenshot inspected: rising colored plumes, but fine-grid reservoir density artifacts remain; visual and complete-frame acceptance NOT passed. Practical activity tolerances retain stock and pass stability. Next pressure/transport coupling artifact and budget work; no other module started.

**2026-10-25T21:15:39Z**

Residual V-cycle48 fine sweeps +128 coarse sweeps passes all3 actual-display practical gates. Wide256-lane factorization improves real40-frame trace41.935ms mean63.417ms worst (previous48.708/72.383), fixed one interval/two submissions, no retries. Pressure~13ms, transport init~29ms at3040 tiles; target remains RED. Extreme kinetic_scale1/gravity900 diagnostics remain available with DC_GAS_EXTREME_STRESS=1; user-authorized practical admission rejects unresolved scale heights. Next stage face coefficients once in parallel instead of repeated scattered per-line cache gathers; no CPU field solve/readback.

**2026-10-25T21:31:51Z**

Actual Wayland/Vulkan supported native battery27/27 PASS with DC_VK_VALIDATE=1 and VK_VALIDATION_VALIDATE_SYNC=1, wide/fused/coarse/staged-rate diagnostics enabled. Zero VUID, Validation Error or SYNC-HAZARD lines. This validates the pre-matrix-reuse checkpoint; new offset-wall case and matrix reuse remain queued. Full make -i test is executing its complete recipe serially; only the two known ctr-ojvy authored-stage failures seen so far. Latest frame budget remains RED41.935/63.417ms; no default promotion/module closure.
