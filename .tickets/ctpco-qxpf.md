---
id: ctpco-qxpf
status: in_progress
deps: []
links: [ct-yctv, ct-q3hh, ctwi-7ukj, ctcio-105w]
created: 2026-11-26T22:27:13Z
type: bug
priority: 1
assignee: kmd
parent: ct-exwr
tags: [player, vulkan, input]
---
# Separate player cast refusals from execution failures

The live player loop ignores failed fire-bolt and ice-storm results. Routine blocked placements need to leave the game running, while a transport, allocation or device failure must reach the caller before the world continues.

## Acceptance Criteria

Live cast rejects blocked and unloaded locations without effect or stale errors. Injected Vulkan work failures propagate to the caller, preserve the backend diagnostic and cannot trigger an automatic duplicate cast. Keep existing public success and refusal behavior and test both spells.


## Notes

**2026-11-26T22:29:22Z**

RED drove the real player control and cast-frame loop. A failed bolt and a failed storm both returned successful progression and submitted the second movement command. Both ordinary refusal cases left their diagnostic in the healthy step buffer. All four new checks failed.

**2026-11-26T22:34:02Z**

GREEN and refactor: the real player step passes all four failure/refusal controls, retaining the original backend diagnostic and stopping before a second movement submission on failure. Rejections leave the caller error untouched and both paths consume the event once. Six footprint tests, the headless player family, tool and recipe checks, and the four ASan/UBSan player-loop cases passed. Native backend injection and visible player actions compiled and are queued.

**2026-11-26T22:41:29Z**

Exact f69cbd0a passed both native injected backend tests. The real launch path distinguishes an unloaded footprint from a failed readback, and a refused Vulkan storm submission preserves its diagnostic and does not wet the target. The visible player-actions regression is queued.
