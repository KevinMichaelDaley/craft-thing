---
id: ct-kpxz
status: in_progress
deps: []
links: []
created: 2026-10-30T09:21:01Z
type: feature
priority: 1
assignee: kmd
parent: ctfs-5gil
---
# Run validated practical gas transport without float32 DenormPreserve on NVIDIA

RTX A2000 Vulkan reports shaderDenormPreserveFloat32=false. The accepted practical bonfire policy rejects after GPU startup; implicit policy fails with gas interval below clock precision. Build a separate practical transport shader variant with explicit finite-stock handling for devices lacking this float control, select by queried capability, and validate gas budgets and displayed plume/bonfire behavior at half-native.

## Acceptance Criteria

X11 half-native bonfire presents complete frames at normal physical clock with practical transport and finite O2/fuel/energy, no shader feature validation errors. Numerical conservation and small colored stock tests pass on NVIDIA; existing preserve-denorm path remains unchanged on supporting devices. Benchmark frame timing at 960x540 is recorded.


## Notes

**2026-10-30T09:35:11Z**

Compact practical gas transport admitted on RTX A2000 without shaderDenormPreserveFloat32; X11 displayed bonfire reaches 32 frames at 960x540. Focused NVIDIA tests for practical safe line, colored subnormal stock, massless underflow energy, and vorticity plume pass. Vulkan validation layer and spirv-val are absent from this machine, so formal shader validation remains unverified; frame budget also remains open in parent ticket.
