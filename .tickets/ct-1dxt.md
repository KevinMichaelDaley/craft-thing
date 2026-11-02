---
id: ct-1dxt
status: in_progress
deps: []
links: []
created: 2026-11-02T02:14:30Z
type: bug
priority: 0
assignee: kmd
parent: ct-exwr
---
# Diagnose invalid gas packet after sustained live beaver fire-ray use

Live old demo PID41100 exited with Invalid Vulkan gas state at physical cell177077 after about921 seconds presentation time. Preserve build/beaver_play_anGXgA and user-demo-80ms-live.log. Capture exact invalid state, cast/burn history and saved evidence; reproduce serially, fix root numerical/conservation cause without clamps. Validate cache optimization independently against pressure/thermal and long burns before safe reopen.

## Acceptance Criteria

Exact invalid fields/stage and reproducible regression; RED/GREEN fix preserving mass, energy, pressure and artwork; isolated long burning stability and comparable frame measurements; no competing user game or overwritten original save.


## Notes

**2026-11-02T02:16:50Z**

Preserved exact original save copy under build/agents/beaver-player/sustained/gas-failure-original and SHA256 manifest; source untouched. Original log records only four cast emissions wall8903,11775,887937,909539ms; fatal logged after movement wall921045. All70 saved gas chunks finite and satisfy cold mass/pigment/internal-energy checks; common clock13119 physical steps/substeps218.650011403s. Thus failed candidate not represented by a trivially malformed persisted packet. Physical error cell177077 is x437,y276 in640-wide active grid. Old binary clone cannot restore save due separate pre-enable gas admission bug, filed separately; diagnostic replay enables matching gas before chunk service to isolate numerical failure.

**2026-11-02T03:18:25Z**

Cross-machine WIP handoff: focused reproducer is test_native_trace_buoyancy_preserves_external_work_without_underflow in tests/vulkan/gas_gpu_tests.c. Run tools/with-vulkan make build/gas_display_tests then SDL_VIDEODRIVER=wayland DC_GAS_TEST_FILTER=trace_buoyancy DC_GAS_FAILURE_DIAGNOSTICS=1 tools/with-vulkan ./build/gas_display_tests. RED fails Invalid Vulkan gas state at cell0. Current unverified shader algebra change avoids multiplying tiny extensive impulse*momentum before division by mass. Follow-up test no longer trips invalid-state but STILL FAILS conservation: actual center E6.52359520304e-25 vs expected1.33829466118e-24; actual work9.24446373306e-33 vs expected6.85935190179e-25. Therefore root diagnosis/fix is incomplete; do not mark GREEN or safe reopen. Investigate pressure/gravity work cancellation, staged buoyancy/finish, walls and sparse publication. User explicitly requested push now; full GPU suite not run. Saved-world replay first physical step reproduced exact live cell177077 with flags100. Original saved chunks remain local build/agents/beaver-player/sustained/gas-failure-original with SHA256 manifest; numerical packet is embedded in portable regression test. No live process remains.
