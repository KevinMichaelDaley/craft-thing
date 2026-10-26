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

**2026-10-26T04:48:33Z**

Trace-boundary GREEN after6cd6a82: four injected100ms setup-log delays incorrectly counted as~101ms fence in RED; fixed capture immediately aftervkWaitForFences reports0.5-4.4ms despite delays. Full actual native45/45. Corrected1000 trace largest65ms frame hasgasGPU23.469ms/renderfence1.181ms, with~36ms in post-fence trace-output region, not evidence of compositor stall. One accepted interval/0retries/fixedstorage-RSS/0majorfaults. Full-frame timer retains all elapsed cost; traced runs are diagnostic only. Broad serialized make-i test stillrunning after parallel CPU builds; exactlythetwo known ctr-ojvy authored-stage failures observed sofar, finalcounts pending. Best uninstrumented26.898/30.325 remains strict-worst RED.

**2026-10-26T04:50:45Z**

Broad regression completed aftertracefix:162 C groups764passes and exactly2known ctr-ojvy authored-stage failures; all Python checks pass, zero compiler warnings/errors. Axis-specialization RED validates same state/clock/dispatch count but requires distinct actual X/Y pipeline binds: baseline repeated23vs23 fails expected22vs23. No current performance completion claim.

**2026-10-26T04:56:33Z**

Axis-specialized transport afterREDc876882: actual bind comparison22vs23 repeated pipelines proves distinct X/Y kernels; state/clock exact and dispatchcount unchanged. Full selected256-thread native46/46, core+sync1/1, sharedstorage gate2/2 acrosssix variants. Audited open1000 improves26.466mean30.572worst; invalid0, stock/work drift small, fourpausedcapturesidentical. Still strict worstRED; no clock/resolution/pressure-schedule changes.

**2026-10-26T05:18:15Z**

Rejected uncommitted full-atlas publication: slower27.456/31.824 and new quiet-mapped assertion reproduces illegal inactive candidate publication. Unsafe backend removed entirely; source runtime restored to917a1bf. Permanent stronger ownership regression27313db verifies active plume changes while disjoint quiet mapped and dormant stock stay byte-identical, with mid-run admission and both canonical publication modes; native1/1 pluscore+sync1/1 pass. No unsafe backend implementation was pushed; RED history records the experiment. Existing46-case runtime suite was alreadypassing unchanged production code. Next quiet-air policy trial keeps all existing quality assertions.

**2026-10-26T05:31:35Z**

Rejected quiet-air0.1 trial afteree659fb: wake controls1/1, practical5/5, enclosure1/1 and core+sync1/1 passed, but actual1000 26.879/31.052 had no gain and more drift (species6.62e-5, energy-ledger-4.74e-5, short enclosure8.6ppm). All runtime policy changes removed; original0.05 retained. New wake regression is explicitly for that ORIGINAL policy:0.025sleeps/.075wakes plusindependent heat/pressure wake, finite10ppm mass and unchangedclock; actualnative1/1 passes. Existing assertions unchanged. Cold single-scalar activity diagnostic is test-only, never runtime scheduling. Next exact activity lookup reuse; performance target remains RED.

**2026-10-26T05:45:40Z**

Exact activity lookup variant afterRED2bc7f55:49/49 selected native tests and core+sync1/1 clean; pressure-only comparison usesvelocity0/gravity0, gates, pageadmission, disjointquiet+dormant stock, exactfields/CFL/clock andGPUroute1vs0. Original0.05/.001 criteria unchanged. Actual1000 26.711/31.381 gives no demonstrated frame gain; remains opt-in/off in selected profile. Next lean metadata preparation targets repeated neighbor EOS work while keeping source/candidate/cache ownership unchanged.

**2026-10-26T06:18:10Z**

Lean metadata afterRED045b7ed:50/50 native, practical5/5, enclosure1/1 and core+sync1/1. Initial shortcut failedtightgates(.5%density,.325velocityRMS); zero-gravitydiagwasexact, isolating owningEOS vscompactneighbor buoyancy evaluation. Retaining BOTH forecasts once/cell makes gravitycase BYTE-identical,52vs51dispatches, exactquiet/dormant/CFL/clock. Existing fusion testpinsLEAN0 tokeepitsoriginal one-fewer-dispatch assertion; newleancomparison retains tightdensity/energy1e-5,pigment1e-4,velocity.01,stock1e-4 bounds plusexactownership. Actual1000 27.489/31.925 slower, stocks/finite/pause pass; leanbackendremains opt-in/off. Next remeasureexisting64threadconfiguration before adding newalgorithm knobs.

**2026-10-26T06:55:44Z**

512-thread line variant afterRED276065f:deviceeligible(minsubgroup8,maxinvocations1024), compiledlocal512/scratch30976 static3/3, core+sync1/1, practical5/5 andfullnative51/51. Actualfused51229.224/32.805 andsplit51229.468/33.956 are slower; staysopt-in/off. Remeasuredsplit25626.976/31.213 also slowerthanselectedfused25626.466/30.572. No time/resolution/physics-schedule change. Next exactzero-energy exchange fastpath; preparedtestscaffolding remainsunstaged untilREDcheck.

**2026-10-26T13:34:18Z**

Zero-energy opt-in after RED934f81f: full actual Wayland native52/52, practical5/5, enclosure1/1 and core+sync1/1 clean. Exact state/CFL/clock retained; nonzero exchanges retain compensated energy. Audited open1000 27.190mean32.115worst provides no frame gain, so remains OFF in selected profile. Strict performance remains RED; investigating device-local GPU-only work storage next.

**2026-10-26T13:40:37Z**

Device-only working storage after REDb244b0d:53/53 actual native, core+sync1/1, same51 dispatches and exact ownership/CFL/clock. Explicit cold GPU zero initialization; canonical stock unchanged. Permanent closed2/open2 x1000 actual Wayland battery PASSES strict mean AND worst<30ms:11.749/19.406,11.781/15.921,26.354/29.011,26.411/29.714ms. All finite/stock/pause gates pass; evidence build/smoke_box_measurements_lxOmeu and design/smoke_ventilation_box.md. Selected256 profile adds DEVICE_WORK1; all slower experiments stayoff. Remaining smoke edit/checkpoint/usability integration still open.

**2026-10-26T13:48:24Z**

Cold checkpoint subset GREEN: native exact packet/geometry/work/valve roundtrip1/1 and core+sync1/1; invalid final-cell input leaves canonical stock unchanged. Atomic chunk format9 adds gas inside the existing prefix checksum/rename transaction, immutable queue copy, no sidecar authority. New gas-stream1/1 plus legacy stream7/7, reactive3/3 and thermal3/3 pass. Future-version rejection now correctly tests10; versions1..8 remain supported. App save/load/stream ownership and geometry edits are still unfinished; no completion claim.

**2026-10-26T14:44:57Z**

Placement persistence/streaming now GREEN: actual displayed4/4 includes exact all-chunk/valve/clock reopen,20-chunk pan-away/back and new-slot ambient without stale pigment/valves, plus missing gas admission rejection with saved chunk/body hashes unchanged. Cold GPU new-slot initialization now works in offscreen atlas slots too, without page dependency or GPU metadata readback; focused core+sync1/1, full native55/55, placement4/4. Canonical clock import never rewinds newer accepted time. Pushed through326b340; committing offscreen refinement next. Geometry edits remain active work; performance repeat after integration still required.

**2026-10-26T15:09:42Z**

Geometry functional checkpoint after RED5483717:56/56 native, focused core+sync1/1, actual placement5/5 including roof carving and real plume escape. Physical voids refresh on GPU; solid insertion displaces to open neighbors and inaccessible pockets retain finite stock until opened; oxygen drift2.74e-9 relative across30 intervals. Inspected capture also shows correct detached stone L, so no extraction shortcut. Stronger edited/dynamic1000 gate is RED43.786mean61.582worst despite finite/stock/pause pass. Trace200 shows3 gas submissions (geometry+cold cache+pressure), ~29ms gas wait plus native body work. Optimizing local cache repair and unchanged-geometry work next; module stays open and selected static budget is not claimed for edits.

**2026-10-26T16:02:02Z**

Local-cache optimization after RED94a03c5: both publication modes produce byte-identical fixture fields, exact quiet owners/clock, and32 vs40 submissions over8 edited intervals; non-direct repeated state copies also fall. Dispatches452 vs444 include necessary cheap pressure-clear work, so the new test measures submissions/copies rather than treating all kernels as equal. Full actual native57/57, core+sync1/1, placement5/5; cold-I/O module extraction checkpoint1/1 and placement5/5. Serialized full repo163 C groups765passes plus exactly2known ctr-ojvy failures; Python checks pass, prerequisite builds warning-free. Edited1000 improves to33.989/37.636ms but remains RED. One early experimental run rejected a new vacuum cell at accepted step16/~0.400s; subsequent1000 traces/runs pass finite audits but do not prove it fixed. Failure-only packet diagnostics added. LOCAL_GEOMETRY remains opt-in/off in selected launcher; canonical device-storage experiment next.

**2026-10-26T16:36:33Z**

Confirmed and fixed intermittent vacuum rejection: last accepted cell had all species/soot/momentum/pigment/E zero; candidate only retainedE1.141e-37 or6.434e-37. Practical zero-mass cleanup uses8 FP32 eps of ambient full-cell energy, not an exact-underflow chase. Vulkan RED masks148/151 vs215; GREEN215, core+sync1/1, positive subnormal colors/ordinary gas/compensated-zero unchanged and larger invalid energy retained/rejected. Device-only state55,050,240bytes +262,144cold staging has exact active/new/quiet/dormant state/CFL/clock, checkpoint/rejected-state core checks and placement5/5; combined full actual native59/59. New edited1000 is finite/audit/pause pass34.119mean62.878worst; device-state gave no demonstrated performance gain and remains opt-in/off. LOCAL_GEOMETRY also stays opt-in; strict frame budget RED. Comparing existing split transport backend under the dynamic workload next.

**2026-10-26T17:12:42Z**

Rejected optional subgroup16 trial: configured and actual native subgroup16 verified, but stiff eight-step field-quality diagnostic failed (density relative L1 .01275, pigment .01898) and actual displayed carved1000 was slower, 36.754ms mean /70.119ms worst. Trial preserved in c0ef623 and reverted with RED diagnostic; selected runtime restored to 7fc7e48. No acceptance thresholds weakened. Edited dynamic-scene performance remains RED; next narrow experiment is tiled explicit transport with existing implicit fallback.

**2026-10-26T17:18:50Z**

Tiled explicit transport RED d1a175c implemented and tested on actual Wayland: reference packet byte-identical, complete clock/stock pass. Rejected for performance: 8x8/64threads 46.935/52.387ms; 8x8/256threads45.359/51.576;64x2/256threads37.063/40.403 (carved100 actual complete frames). Shared memory and halo duplication outweighed smaller shader. Full experiment and reversals preserved in git; selected transport unchanged. Next investigate multigrid smoothing, rather than enabling a slower transport.

**2026-10-26T17:25:24Z**

Rejected shorter multigrid fine smoothing without weakening quality gates.4/4 omega1 failed reference flow;8/8 omega1.5 passed flow/checkerboard but sealed-neighbor density deviation.02523 exceeded.01;8/12 omega1.5 worsened neighbor.19188;8/12 original1.8 grew checkerboard variance7.61e-6 vs5.90e-7initial. Default12/12 control wall deviation.000638 passes. Experiment history preserved then reverted. Next remove duplicate line-rate work while retaining accepted numerics.

**2026-10-26T17:33:59Z**

Actual Wayland core+synchronization validation3/3 passes: valve-only opening releases previously trapped gas; route-only refresh preserves restored partial physical void and clock; optional duplicate-rate scan elimination preserves eight-step stiff snapshots and dormant owners byte-for-byte. Gate fix uses a separate route-dirty flag, without forcing material-volume resampling on valve-only edits. Native GREEN pushed checkpoint. Complete-frame performance still RED: optional reuse carved1000 34.785/37.370ms; cached-neighbor trial35.107/37.681ms. Neither promoted into selected launcher; no numerical/resolution/clock concessions.

**2026-10-26T17:47:53Z**

Latest actual six-run Wayland battery (build/smoke_box_measurements_EnEWi4): closed11.653/18.909 and11.377/20.135 PASS; open25.787/31.647 and25.837/31.173 worst-frame RED; carved32.687/38.953 and32.575/40.150 RED. All6000 frames finite/meaningful inventory drift and all24 paused captures identical. LOCAL_GEOMETRY now selected by permanent launcher after61/61 native plus core/sync gates; improves edited mean from earlier43.8ms to32.6ms, but is NOT final performance acceptance. Practical-only implicit shader specialization4/4 binary+1/1 exact native passes, pure-implicit remains slower/off. Group-mask trial61/61 but35.762/40.187; preserved/reverted. Working next on warm geometry+gas submission fusion and tail attribution; no numerical thresholds relaxed.

**2026-10-26T17:52:21Z**

Fused warm geometry opt-in passes actual native and core+sync:8 edited intervals8submissions vs16, byte-identical packets/clock and unchanged quiet owners. Actual carved1000 improved31.695ms mean but43.840worst, still RED. Permanent end-of-run attribution shows worst frame557; scripted combined-valve/roof action frame8 was26.813ms, so remaining tail is sustained simulation, not hidden paint cost. Fusion remains opt-in pending full acceptance. Next user-authorized practical approximation experiment: scalar practical energy updates instead of repeated compensated expansions, retaining persistent layout, finite/stock/reference/clock gates and unchanged pressure work/resolution.

**2026-10-26T18:10:55Z**

Rejected energy simplification trials, preserved then reverted. Scalar FP32 violates selected-profile A/B bound (density.01416, velocityRMS.718). Paired FP32 passes same bound (density/color/velocity identical, relativeE4.19e-15),4 actual core+sync wall/checkerboard/variable-time gates, and several1000-frame conservation/pause gates, but no meaningful speed win31.641/36.216ms and one diagnostic run rejected early before precise packet capture. Repeated diagnostic paired run finite32.444/38.930; original-energy fusion-only diagnostic finite32.630/37.857. No unstable approximation promoted. Full time-series confirms one accepted step,zero retry,one submit per frame; GPU init12.25ms pressure7.22 finish2.49 commit2.10, so next target is fine-grid smoothing cost with a bounded weighted-Jacobi kernel, keeping quality/clock/stock gates unchanged.

**2026-10-26T18:15:35Z**

Weighted-Jacobi fine smoother rejected by unchanged reference-flow checks:8 pre/post gave densityL1.5967/velocityRMS194.5;16 pre/post still.2102/66.7. No parameter/quality weakening and no performance pass claimed. Full experiment retained then reverted. Original12/12SOR remains selected. Profiling shows finish+transactional commit move full64-byte packets plus32-byte caches; next exact optimization target is sparse per-cell GPU bank ownership, retaining quiet/dormant owners and rejection atomicity while eliminating publication copies. This is internal current-module optimization, not future chemistry or bonfire expansion.

**2026-10-26T18:43:25Z**

Exact sparse-owner backend GREEN: actual native63/63 and displayed placement5/5 pass with owners+fused geometry, including cold policy transitions, failed-interval atomicity, offscreen new slots, pan/save/reopen, trapped gas and airbrush extraction. Focused core+sync gates clean. Tiny proportional tile-owner table avoids page lookups/strided cache tags; runtime performs guarded GPU owner flips, cold GPU gather preserves existing save packets. Actual carved1000 mean29.651/worst43.272ms: mean now under30, strict worst still RED, backend remains opt-in. Compiler diagnostics show selected hybrids~5.6-5.9k instructions SIMD8 and0 spills; no speculative spill claim. Next reduce diagnostic flag contention / inspect tails without changing accepted numerics.

**2026-10-26T18:53:37Z**

Grouped repair diagnostics preserved flags and passed core/sync but worsened actual carved1000 to33.906/170.681; reverted complete trial. Standalone explicit shader policy specialization now passes compiled-shader checks and exact native/core split/fused checks; split transport30.326/56.450 still does not beat selected hybrid, so split not promoted. Next exact pressure optimization keeps12/12 sweeps and halo but assigns one row per invocation instead of two dynamically indexed rows, targeting register/code overhead without numerical-policy changes.

**2026-10-26T19:26:54Z**

GPU-local geometry activity GREEN: actual native64/64 plus displayed placement5/5 pass with sparse owners+fused geometry+local activity; focused core/sync4cases clean. Dirty flags live in separate GPU words from owner tags, preserve per-cell valves, include affected halos, and bypass broad geometry wake scans without CPU lists/readback. Exact A/B54vs53dispatches. Actual carved1000 30.127/37.055ms remains RED; not promoted. Prior volume-cache lookup actual30.503/36.744; one-row SOR exact but33.365/44.066, rejected/reverted. Full repository serialized regression is currently running after parallel prerequisite build; no other measured/GPU jobs overlap.

**2026-10-26T19:45:54Z**

Partial-cut-cell physics fix GREEN: native RED1998ea4 proved half blockage doubled density; RED16457c0 also locks full/cached geometry conservation. Both modes now retain density.0012 and temperature300 at void.5 and1/65536, conserving mixture/color/energy without advancing geometry clock. Four actual core+sync gates clean including trapped pockets and exact fusion/local activity. Actual650K moving-stone1000 finite/inventory/pause checks pass;29.716mean34.413worst still performance RED, max final pressure131.97 versus prior359.38. Pressure tolerance/sweep policy unchanged. Previous full repository run163groups765passes plus only2known ctr-ojvy,zero build warnings; full native/UI regression is being rerun after this physical remap change.
