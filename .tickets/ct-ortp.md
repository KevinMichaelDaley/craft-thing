---
id: ct-ortp
status: in_progress
deps: [ct-2p38, ct-xsll]
links: []
created: 2026-11-01T19:29:54Z
type: task
priority: 1
parent: ct-exwr
tags: [player, beaver]
---
# Integrate beaver movement, jump and world collision into live application

Connect player action intents to live simulation and renderer. Keyboard A/D moves horizontally and Space jumps; establish controller-neutral movement and target intents. Resolve current editor camera/input conflicts explicitly.

## Acceptance Criteria

Beaver is visible in actual application and moves/collides against existing material terrain and rigid world. Grounded jump, release/press semantics, frame-rate-independent simulation, streaming edges and spawn safety covered. Controls are documented; camera follows player without unintended editor actions. Shared interfaces work for multiple player instances.


## Notes

**2026-11-01T23:45:12Z**

Runtime prerequisite implemented in continuation: RED c6d29640 compiles and fails all 5 real Vulkan drive cases; GREEN e65f69ef passes 5/5. fr_player_drive_gpu_create/submit binds unique player identities to existing actual body ID/generation, applies horizontal velocity and press jumps on GPU using real solver support, clears consumed support, suppresses duplicate 64-bit sequences, validates whole batches and ignores stale/missing bodies. No CPU body readback/transform edits. Refactor f94ffbc9 shares C/GLSL command layout, validates actual device limits and records dependencies. Backend is currently compiled by dedicated test target; main/SDL/spawn/fixed-step scheduling is NOT integrated. Ticket remains in_progress with ct-xsll dependency outstanding. Current combined Vulkan target has 6 cases including actual trimmed wet/hot reactive parcel preservation; log build/agents/beaver-player/continuation/targeted-final.log.
