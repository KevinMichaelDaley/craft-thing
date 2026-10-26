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

**2026-10-25T21:35:14Z**

Offset sealed-wall regression GREEN: restricting coarse correction to fully open aggregates reduces sealed-neighbor density disturbance11.6% to2.53e-8, with exactly zero escaped smoke. All4 actual displayed practical cases pass, including checkerboard damping and12-second variable-clock/no-retry/stock/finite-state gate (worst gas call20.456ms). Coarse blocks with internal gates/solids retain fine-grid wall topology. Full regression764 passes plus exactly2 known ctr-ojvy failures. Next sustained untraced real-scene timing; performance/module remain open.

**2026-10-25T21:37:45Z**

Sustained actual-display1000-frame elapsed-clock open/vent run (no GPU trace, staged-rate experiment off) completes without numerical failure:55.239ms mean64.393ms worst.100-frame blocks settle55.7–56.1ms after first48.9ms block; no delayed tail runaway. Four paused captures identical; final capture inspected: smoke mixes and escapes vent, reservoir checkerboard gone. Original retry stall removed; steady gas cost remains over30ms. Next smaller bounded coarse budget and practical transport work reduction; no stock/time/resolution drop and no module closure.

**2026-10-25T21:44:08Z**

Practical64-byte scratch (duplicate mass/thermal coefficients removed) passes4 actual displayed practical gates unchanged;120-interval worst call17.941ms. Sustained actual untraced1000-frame open run improves53.448/61.982 to50.170ms mean59.897ms worst, four paused captures identical. Still RED vs30ms. Next line-local whole-axis kernel fusion to reuse scratch/cache and remove separate factor/forward/back/apply dispatch traffic; no new gameplay module or altered clock/stock.

**2026-10-25T21:47:17Z**

Opt-in whole-axis practical transport fusion passes all4 actual displayed practical gates (finite12-second variable clock, inventory/energy drift, checkerboard damping, offset-wall isolation). Sustained actual untraced1000 open frames improve50.170/59.897 to45.100ms mean56.411ms worst, four paused captures identical. Same factor/forward/back/apply work synchronized within each independently owned line, no CPU field/list solve or stock/time drop. Target still RED. Next practical shared-tile pressure smoothing to reduce full-grid dispatch/cache traffic.

**2026-10-25T21:56:31Z**

Rejected optional shared-block smoothing experiment:4 inner over-relaxed sweeps failed isolated-neighbor density20% and pressure bound; monotone2-inner also failed; global pre-smoothing plus2-inner post-smoothing passed long pressure bound but still disturbed sealed neighbor2.5% vs locked1% gate. Removed experimental path; retained passing global48-fine V-cycle. No gate weakened. Next cold end-of-benchmark velocity/inventory audit to target actual sustained active-state cost rather than further blind variants.

**2026-10-25T22:16:48Z**

Cold parallel GPU audit GREEN: actual ambient+moving-color test proves totals/velocity bins and exact state+clock preservation; core+sync audit case zero VUID. Permanent --benchmark-gas-audit runs before loop clock and after measured frames, reads32 scalar summaries only. Audited1000 displayed frames44.721/54.353ms; invalid0, species max drift6.303e-5 relative, pigment2.046e-7, energy-minus-work−4.666e-5; T293.15..367.40K, Pmax64.59 vs51.91 initial, vmax41.49.179072 gas cells:33548 below.05,29103 below1,86486 below10,29935 above10 cells/s. Net-force wake count167200 exceeds raw149345, so balance-sleep is not justified. Next CFL-safe bounded explicit per-line path with implicit fallback;30ms still RED.

**2026-10-25T22:26:09Z**

Hybrid local-CFL transport GREEN: independent safe-line explicit flux oracle passes inside stiff global interval; all5 practical actual-display gates pass. GPU chooses at most3 CFL<=.9 stages per line, implicit fallback for stiff/long/hardware-ineligible lines;640-cell shared workspace is a fast-path threshold, never a simulation cap. Audited1000 actual untraced frames38.763ms mean49.387ms worst (previous44.721/54.353); invalid0, species5.28e-5/pigment1.04e-6/energy-minus-work−2.84e-5 relative drift. T293..367K. No stock/time/resolution loss.30ms still RED; profiling remaining steady-state stages next.

**2026-10-25T22:39:57Z**

Completed tail timing instrumentation: parser4/4 passes, actual200-frame trace attributes late frame~40.5ms to init~14ms, pressure~11ms, residual~0.6ms, finish~3ms and commit~2.6ms, plus publish/waits/render. Hybrid late counts908 explicit lines/1225 local stages,20 implicit,160 inactive; one interval and two submits, no retries. Target now remaining pressure/bandwidth/energy-update overhead; not another convergence retry stall. Next practical float energy updates under unchanged3% long-run energy budget; strict compensated diagnostics remain intact.

**2026-10-25T22:45:05Z**

Rejected ordinary-float energy update experiment:5 practical cases passed and audited1000 stock/finite bounds passed, but39.005/46.708ms did not show a clear mean gain vs38.763/49.387; restored compensated updates. Rejected two smaller pressure V-cycles(32 total fine vs48): offset chamber disturbed7.29% and long pressure bound failed atstep119; restored passing one-cycle48-fine configuration. No acceptance thresholds weakened. Next eliminate redundant publication copies via transactional buffer ownership rotation, preserving numerical method and locking rejection/policy roundtrips first.

**2026-10-25T22:59:36Z**

Direct guarded publication GREEN: warm practical interval uses1 GPU submission vs2; exact state+clock preserved after rejected huge interval, successful recovery and exact practical/implicit/HLLC policy roundtrips pass. All5 practical displayed gates pass. Audited1000 actual untraced frames36.344ms mean45.945ms worst, zero invalid cells; species7.36e-5/pigment1.13e-6/energy-ledger−5.25e-5 relative drift, T293..362K. Canonical ownership transfers cold to guarded working bank in practical mode; final redundant bank copy+fence removed without numerical changes.30ms still RED. Next32-lane checkerboard SOR mapping removes half-idle SIMD work, same cells/arithmetic/sweeps.

**2026-10-25T23:09:26Z**

SOR32-lane checkerboard mapping preserves same cells/arithmetic/48 sweeps;5 practical, independent acoustic reference, dense/sparse parity and transactional publication displayed tests pass. Audited1000 actual run36.009mean43.953worst, stock/finite bounds pass. Onlysmallgain vs36.344/45.945;30ms still RED. Next exact overlap-halo temporal blocking of two global sweeps, distinct from rejected approximate block GS, with immutable source/doublebuffer output and original active-set mask. Lock numerical comparison and dispatch-budget RED before implementation.

**2026-10-25T23:20:51Z**

Exact temporal SOR GREEN:34 indirect dispatches vs106 and BYTE-identical global-solver snapshot across valve changes;5 practical, transactional publication/recovery, independent acoustic reference and dense/sparse parity displayed tests pass. Same48 numerical fine sweeps. Audited1000 actual run34.838mean43.431worst; stock/finite budgets pass.30ms remains RED. Next same32 coarse sweeps in one parallel workgroup for small coarse grids, hardware/workspace fallback to original global coarse solver; no global gas capacity limit or reduced physical time.

**2026-10-25T23:30:54Z**

Coarse workgroup implementation preserves BYTE-identical reference snapshot,7 direct vs70 dispatches and all30 supported actual-display native tests pass. Audited1000 actual frames34.954mean40.595worst, stock/finite bounds pass; no clear mean gain vs34.838/43.431, retained opt-in for comparison, not promoted. Next16x16 output tiles for exact temporal fine sweeps to amortize halo/coefficient duplication, same48 fine/32 coarse iterations and immutable-source semantics.30ms still RED.

**2026-10-25T23:57:07Z**

Discarded larger temporal tile variants after full audited runs:AoS36.534/42.727 andSoA37.262/43.252, both slower despite reference equality. Practical compact normal float mode improves audited1000 to33.500mean39.247worst, invalid0/species4.72e-5/pigment7.16e-7/energy-ledger−2.45e-5 relative drift. All30 displayed native and transactional publication tests pass; strict denormal-preserving modes unchanged. Still over30ms. Next bounded GPU h2/h4/h8 pressure hierarchy; lock flow-error/stock/finite/clock and dispatch-budget tests first, no exact residual loop or CPU field solver.

**2026-10-26T00:09:33Z**

Multilevel checkpoint077eb1e: native33/33 and practical5/5 pass; audited1000 actual display32.348mean38.016worst, invalid0 and small stock/energy-ledger drift; still budget RED. Fine8+8 failed stability; damped1.4 failed wall/long-run gates; coarse2 sweeps failed late recovery atstep110. All reductions discarded; retain12fine pre/post and4Jacobi per intermediate level. Next specialize level shaders without changing numerical schedule.

**2026-10-26T00:25:14Z**

Latest displayed1000: specialized hierarchy31.706/51.371; practical pressure threshold1e-4 improves31.111/49.407; coarse temporal fusion31.876/48.933 no clear gain, kept opt-in. All stock/finite audits pass. Y-axis metadata fusion33.215/55.897 was slower and discarded. RHS+metadata fusion now BYTE-identical52vs53 dispatches; full35 and core+sync comparison underway before timing. Permanent input wall/thread-CPU/event diagnostics6d87f75, parser5/5. Strict complete-frame budget remains RED.

**2026-10-26T00:46:44Z**

Packed pressurec09442f is numerically exact(native36/36), but audited1000 scalar32.819/39.777 and vector-pair32.923/56.424 are slower than30.922/37.864 unpacked; vector experiment discarded and packing stays opt-in/off. Next skip pressure halo tiles with no gas, caching only GPU tile occupancy across valid geometry; lock exact-state and reduced tile-work comparison before enabling.

**2026-10-26T01:15:30Z**

Experimental partial coarse topology/short smoothing WIP: fixed4-node GPU connectivity and child-mask face reconciliation prevent false wall links. Concave reference/work gate passes77vs179dispatches,.172%massL1,.581%pigment,.540velocityRMS; practical stability remains RED(3/5: sealed neighbor6.48% and long-interval pressure bound fail). Not enabled by default/not baseline. Next conservative pairwise viscous stabilization, preserving physical clock, finite inventory and mechanical/thermal accounting; existing stability thresholds unchanged.

**2026-10-26T01:28:18Z**

Short-solve relaxation1.7 failed flow RMS4.38;1.6 passed flow(.277RMS) but still failed sealed-neighbor/long-run stability. Viscous stabilization passed approximate flow but failed same stability gates and all viscosity code was discarded. Retain validated24-fine-sweep backend; partial-topology/short solve remains explicit WIP, not enabled. Next compress only derived pressure rows with original iteration schedule, preserving original physical gas state precision and capacity.

**2026-10-26T01:59:05Z**

Compressed rows27a342e: actual native39/39 and core+sync1/1 clean; audited1000 actual display29.171mean38.003worst, invalid0 and small stock/work drift. Mean now below30, strict worst RED. Tick-fence REDb38a3e1 confirmed queue-wide wait1 per standalone tick; dedicated-fence implementation native tick/capture1/1 passes queue-idle0 and exact+2 physical intervals. Full repo regression162groups764passes plus exactly2known ctr-ojvy authored-stage failures; all Python including new6-case trace suite pass. Quarter-native display rebuild/test in progress before new timing.

**2026-10-26T03:49:53Z**

Direct-source transport6a40303 pushed:42/42 native and1/1 core+sync, exact fields/CFL/clock, warm copies2.29MB vs20.64MB. Actual audited open1000:28.770mean33.292worst, invalid0 and species2.99e-5/pigment5.12e-7/energy-ledger-9.93e-6 relative drift, four paused captures identical. Strict worst remains RED; copy optimization has no demonstrated frame gain. Next hybrid explicit/fallback fusion RED74cef9a: baseline53 dispatches; local native51 vs53 byte-identical, sync and full-frame measurement underway. No change to pressure iteration schedule or physical clock.

**2026-10-26T03:55:54Z**

Hybrid fusion40739da pushed:43/43 native, core+sync1/1; audited open1000 28.339mean32.363worst, invalid0, stocks/paused checks pass. Halo-row loading refactor also43/43 but28.351/32.181, no material gain. Trial12-pre/8-post smoothing fails checkerboard damping and sealed-neighbor4.05% gate (3/5 practical); discarded immediately, retained12+12 and existing thresholds. Strict complete-frame worst remains RED.

**2026-10-26T04:03:03Z**

Shared scratch GREEN after523f1bc: fused hybrid SPIR-V37120->30976bytes, permanent32KiB gate2/2 and full native43/43, core+sync1/1. Explicit, factor and prefix phases reuse barrier-separated shared memory with identical state/clock. Audited open1000 27.844mean33.534worst, invalid0 and small stock/work drift; strict worst RED. Separate metadata dispatch trial28.923/34.639 slower; retain fused metadata for next diagnostics.

**2026-10-26T04:06:04Z**

Fused RHS refactor reuses owning metadata pressure head and avoids computing then overwriting moved-neighbor stage. Exact metadata comparison1/1, core+sync1/1, full native43/43. Audited open1000 improves27.278mean31.436worst; invalid0, species3.18e-5/pigment2.97e-7/energy ledger-1.09e-5 relative drift, four paused captures identical. No pressure-schedule or quality-limit change. Strict worst remains RED.

**2026-10-26T04:17:58Z**

Compiler diagnostic tooling: named actual-display selected5 gas pipelines show0 spills/fills; hybrid is SIMD8,6185instructions/19loops, fused RHS SIMD8/2184instructions, finish SIMD8/2438instructions, fine pressure SIMD16/499instructions. Earlier unnamed dump included huge spills in OTHER startup pipelines and was interrupted, not a test pass. Permanent wrapper strips only debug metadata for selected cache misses, retains arithmetic, restores built SPIR-V byte-for-byte; captured evidence build/smoke_shader_stats_YDfYng.2/2 parser tests. Diagnostic timing is excluded from performance claims. Crossover1/4/6 trials offered no clear gain; restored validated3stage.

**2026-10-26T04:26:16Z**

Practical-only specialization of hybrid transport/grouped finish/fused RHS: strict fallback branches remain in original diagnostic pipelines. CPU dispatch already admits these specialized pipelines only for practical mode. Exact native transport/metadata and field+CFL comparisons pass; core+sync comparisons clean; full native43/43. Audited open1000 26.898mean30.325worst, invalid0, species3.96e-5/pigment4.70e-7/energy-ledger-2.06e-5 relative drift and four paused captures identical. Strict worst still RED by0.325ms; no pressure-schedule, clock, resolution or acceptance-limit change.

**2026-10-26T04:30:53Z**

Narrow compute destinations after REDb29a68d: actual barrier scopes10 vs40, same51dispatches and exact fields/clock. Full native44/44 and core+sync1/1 clean. Actual audited open1000 27.076mean31.450worst, finite/stock/pause pass; no demonstrated frame gain, retained opt-in and strict worst remains RED. Taking full1000-frame stage/wait trace next; performance requirement unchanged.
