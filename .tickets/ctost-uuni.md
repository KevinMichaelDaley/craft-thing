---
id: ctost-uuni
status: closed
deps: [ctost-pneh, ctcos-3q4u]
links: []
created: 2026-11-27T02:45:45Z
type: task
priority: 1
assignee: kmd
parent: ctoew-5swd
tags: [offscreen, streaming, world-clock]
---
# Resolve coherent snapshot ownership between the structure store and offscreen stream

When a matching structural checkpoint restores terrain, the level skips DCC colors, reactive state and heat, but still imports DCC gas and bulk liquid without checking that those channels belong to the chosen physical owner. A promoted offscreen snapshot can also collide with a pending structural copy.

## Design

Derive source authority from the handoff or committed save, with verified physical channel correspondence. Retire older pending copies only after the matching generation publishes. Keep legacy structure restore semantics when the files have no verified common epoch.

## Acceptance Criteria

Native tests pick a coherent owner when pending structural terrain and streamed stock differ, preserve matching gas and bulk channels, reject stale acknowledgements, recover interrupted publication, and cold reload the updated tile without reviving old topology.

## Notes

**2026-11-27T03:14:13Z**

DCS v11 now includes gas and bulk liquid from the same selected physical owner as terrain, elapsed time, colors, reactive records, and water heat. The level archive selects current resident slots and retains pending records across failed GPU publication. Earlier DCS versions still accept compatible streamed fluids when the physical records match and their known times agree. Verified 52 targeted host checks, five codec checks under ASan and UBSan, and six RTX A2000 level stream checks under Vulkan core and synchronization validation. All 625 build prerequisites of make test are ready. Full execution remains queued behind the previous exact revisions. Durable offscreen supersession is tracked in ctcos-3q4u.

**2026-11-27T03:21:59Z**

The targeted Vulkan run passed 22 checks across level streaming, structural storage, thermal and reactive checkpoints, and offscreen workspaces. The four level snapshot cases passed again after moving the unchanged checksum calculation with the channel format. The final codec passed five sanitizer checks. The exact code revision for make test is queued behind the earlier GPU suites.

**2026-11-27T03:57:29Z**

The durable takeover now selects the exact owner across stream and structure archives, retries failed publications without dropping the previous snapshot, and preserves legacy fallback. Verified cold restart and restored gas, liquid, parcel and elapsed-time state on the RTX fixtures. The camera route and seam integration remain under ctoew-5swd.
