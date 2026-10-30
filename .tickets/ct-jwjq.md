---
id: ct-jwjq
status: closed
deps: []
links: []
created: 2026-10-30T08:30:01Z
type: bug
priority: 1
assignee: kmd
---
# Allow Vulkan probe tests on available hardware GPU

The bounded diagnostic probe hardcodes Intel vendor 0x8086 and reports no device on a working NVIDIA RTX A2000 display session. Select a non-CPU Vulkan compute device with required features; keep explicit Intel-only mode for vendor diagnostics.

## Acceptance Criteria

Mocked NVIDIA probe reaches device feature validation; CPU-only probe remains rejected; reactive profile GPU test executes on this machine; default make test advances past the Intel-only selection stop.


## Notes

**2026-10-30T09:35:11Z**

Mock NVIDIA probe and CPU rejection tests pass; reactive profile GPU test runs on the RTX A2000; make test now advances beyond prior Intel vendor selection. X11 display is available on this machine, while Wayland socket is not.
