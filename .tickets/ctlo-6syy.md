---
id: ctlo-6syy
status: in_progress
deps: [ctlo-x5gv, ctlo-3ndt, ctfs-0zw5, ctfs-ermw, ctfs-b6cz, ct-xuqp]
links: []
created: 2026-10-24T22:06:19Z
type: feature
priority: 1
parent: ctlo-d7vm
tags: [fire, smoke, physics]
---
# Build an assembled siege launcher and real burning projectile release

Author supported constructed frame/base parts, independent pivoted arm and a real latch/pouch/release path using existing supported authoring APIs. Add finite coal/char projectile material and ignition.

## Design

Follow design/fire_smoke_reactive_materials.md. Runtime implementation must use RED-GREEN-REFACTOR and repository tests/diagnostics; this turn creates the roadmap only.

## Acceptance Criteria

Dry cold mechanism cycles without collapsing; a burning-coal load launched promptly does not ignite a sound launcher. Launch velocity follows constrained mechanical work and contact/release, without teleporting projectile or welding the arm to terrain. Record flight/impact/recoil and per-part loads; persist mechanism state. Displayed fixture uses real rigid bodies and support graph, not a visual animation.


## Notes

**2026-10-28T02:40:35Z**

User-approved next visible module after ordinary bonfire regression is green and code is pushed: assembled catapult and constructed-wall demo, then Greek fire. Implement real generation-safe two-body pivot prerequisite first, then supported mechanism, real release and finite burning coal shot against dry/wet constructed walls. No fake arm animation or projectile teleport. Work inline; no subagent requested.

**2026-10-28T21:17:40Z**

2026-10-28 resumed branch. Coal/char body mapping and extracted reactive parcel admission now GREEN on ctfs-ermw, including GPU mass and checkpoint tests. Supported catapult frame, real load/release, rigid elapsed clock, burn damage and displayed dry/wet wall fixture remain outstanding under existing child/blocker tickets.

**2026-10-28T23:36:17Z**

Grounded triangular wood base, supported upright, distinct stone target wall, centered rigid pivot, balanced cocking tension latch, separate pouch release, finite hot/cold coal and wall-impact fixture implemented. Release changes no body pose/velocity; first rigid step advances arm about 0.15 rad with ~0.25 rad/tick angular velocity, projectile reaches target and increases wall load. Headless 7/7 and display 2/2 pass. Renderer arm appearance defect filed as ct-xuqp; progressive burn damage remains ctlo-3ndt. Full make test reaches Intel-only reactive_profile_gpu_tests and stops because this host has no Intel compute device; continue-mode suite is running.

**2026-10-28T23:47:20Z**

Final verification: build/catapult_scene_tests 7/7 and build/catapult_scene_display_tests 2/2 pass. Standard make test passed through catapult and renderer tests, then failed at reactive_profile_gpu_tests because device enumeration reported no Intel hardware compute device. An optional make -i continuation passed later reactive/terrain/body-color targets, with only additional Intel-only initialization failures through body_carve_tests; it was stopped because the hardware-gated full suite cannot pass on this host. Branch changes pushed to origin.

**2026-10-29T01:55:22Z**

2026-10-28: Thermal overlay disabled for normal ALBEDO view, resolving plasma air and hidden arm. Cocked arm now launches hot coal rightward at ~2.98 cells/tick; projectile crosses one chunk and contacts wall. 256x128 game-view display regression and 1024x512 live replay pass. Progressive burn damage remains ctlo-3ndt.

**2026-10-29T06:55:06Z**

Corrected final game-resolution fixture: wall is at x=180, 28 cells tall (twice prior 14-cell target), with wood battlements only on its far/interior side at x=198..239 near the ground. Grounded triangular launcher releases via latch and pouch; finite hot coal clears the wall and strikes wood at x≈209. Far-side wood reaches 1061.5 K, burning flag set, fuel 3932→3921 over 180 ticks. Normal LIT 256x128 view scaled to a 1024x512 window played through to completion without thermal debug overlay; scene tests 9/9 and display tests 2/2 pass. Launcher remains intact through the burn; progressive structural burn damage is still tracked by ctlo-3ndt.

**2026-10-29T06:57:28Z**

Final make test after refactor passed all preceding suites, including catapult scene 9/9 and game-view display 2/2, then stopped at reactive_profile_gpu_tests: device enumeration found no Intel hardware compute device on this host. This is the same host hardware gate previously recorded; no catapult regression was observed.
