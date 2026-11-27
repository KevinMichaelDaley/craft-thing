---
id: ctcdo-rcs9
status: in_progress
deps: [ctdot-kbys, ctols-xhuy]
links: []
created: 2026-11-27T04:17:01Z
type: task
priority: 1
assignee: kmd
parent: ctoew-5swd
tags: [offscreen, streaming, physics]
---
# Conserve stock and face work across resident and offscreen seams

A camera eviction can advance a sealed stationary tile. Adjacent open resident and offscreen owners still need a conservative exchange before they can advance independently.

## Design

Bind both face endpoints to confirmed owner generations and elapsed intervals. Commit paired water and bulk-liquid deltas, gas species and pressure work, and grain handoffs at a common boundary. Recover a failed endpoint from confirmed state without replaying the other endpoint.

## Acceptance Criteria

Native Vulkan tests conserve water, oil, gas species and grain mass in both directions over repeated pans. Exercise a missing neighbor, mismatched epochs, a failed publication and a cold restart. Keep the existing sealed route working.


## Notes

**2026-11-27T04:20:10Z**

The archived split-workspace conservation tests and interface shader start at 8dc17268, with projected liquid face coupling in b26cd351 and streamed gas cases in 88b8e9c8. Reuse their split-versus-monolithic test geometry. Map the exchange to the current shared device, durable owner generations, bulk-liquid packets and conservative gas state before enabling open camera evictions.

**2026-11-27T04:47:30Z**

The archived split-river, reverse-flow and vertical seam fixtures give reference geometries. Its transfer_gas routine moves material ID 6, now mortar debris, so use the current five-species gas field and its pressure and energy state. For liquids, the current bulk packet also owns oil and slime momentum and retained inventory. Exercise the pair at one confirmed boundary with matching ownership generations before enabling open camera evictions.
