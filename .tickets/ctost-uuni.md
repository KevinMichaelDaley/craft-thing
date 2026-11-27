---
id: ctost-uuni
status: in_progress
deps: [ctost-pneh]
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
