---
id: ctdot-kbys
status: closed
deps: [ctcos-3q4u]
links: []
created: 2026-11-27T03:48:56Z
type: task
priority: 1
assignee: kmd
parent: ctoew-5swd
tags: [offscreen, streaming, world-clock]
---
# Give camera evictions a durable offscreen owner

The level can now admit a cold structure checkpoint to a workspace and publish a checked replacement. During play, camera eviction still writes a resident chunk and frees its slot without creating an offscreen owner that can advance. A resident structure save currently writes the new identity into the file and does not make that identity available to the eviction route.

## Design

Transfer the elected sleeping owner and its immutable physical snapshot into a shared workspace once its persisted source is recoverable. Keep screen admission blocked only while that owner has an outstanding transfer and resume from the last confirmed time after failure. Bind the camera and structure store to the same durable identity without reopening the world or bypassing body and seam ownership.

## Acceptance Criteria

Drive demotion and promotion from the normal camera tick over two view widths, prove each physical chunk has one owner at each boundary, and verify time and stock across a failed demotion, a committed return and a cold restart.

## Notes

**2026-11-27T04:22:35Z**

RTX A2000 native case passes with core and synchronization validation enabled. The camera pans two view widths using dc_level_view_tick, retains the dirty resident when the first write fails, transfers to exactly one offscreen owner, advances from 2.0000 to 2.0625 seconds, and retains that offscreen owner after an obstructed return write. Promotion and cold reopen retain 14,000 water and 8,000 oil units at 2.0625 seconds. The second camera test verifies that the worker receipt and rewritten structure archive agree. All 11 displayed streaming cases, 79 focused host cases and 19 ASan/UBSan cases passed. The automatic route elects one sealed stationary owner. Open seams, active rigids and native frame budget remain under ctoew-5swd.
