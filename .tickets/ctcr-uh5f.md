---
id: ctcr-uh5f
status: in_progress
deps: []
links: [ct-1i55, ctcio-105w, ctcri-jd9z]
created: 2026-11-26T22:43:13Z
type: bug
priority: 1
assignee: kmd
parent: ct-exwr
tags: [player, input, sdl]
---
# Make controller seat rebinds transactional and share the join path

The direct join API creates another seat after a disconnect. SDL Start separately assigns the replacement device even when connection or binding setup fails. New-seat input admission can also mutate the router before rejecting a reserved device.

## Acceptance Criteria

Both paths reuse the disconnected seat, preserving body, inventory and context. Input capacity failure leaves the old session and vacant seat unchanged and can be retried. Reject reserved IDs before admission and keep duplicate joins idempotent.


## Notes

**2026-11-26T22:46:07Z**

RED reproduced three production host failures: direct rejoin sought a new player staging chunk, SDL Start assigned device 43 despite exhausting its binding table, and the disconnected-seat sentinel was accepted as a device. Eight existing controls passed, and the Vulkan rejoin case compiled.

**2026-11-26T22:50:57Z**

GREEN and refactor: eleven production-session host cases pass normally and with ASan/UBSan. A failed Start leaves the vacant poison-free router byte-for-byte intact, and a subsequent Start maps the replacement device. Direct rejoin preserves the physical binding, generation, inventory and UI context. The main app, controls, native actions and split fixtures compile. The native replacement replay and full suite remain queued. Failures later in new-body GPU admission are tracked under ctcri-jd9z.
