---
id: ct-ha8q
status: closed
deps: [ct-gf6q, ct-1n9n, dun-c459, dun-08qq, dun-sbk9, dun-spbq]
links: [dun-c459, dun-sbk9, dun-08qq]
created: 2026-10-11T05:44:09Z
type: task
priority: 1
parent: ct-fx1b
tags: [gpu, structural]
---
# Quarter-native structural visual battery, buoyancy handoff and budgets

Integrate marked structural controls, load/stress/failure visualization and paced Vulkan scenes into test_ui and test_quarter_native. Complete submerged-structure load sampling and post-collapse handoff to existing material-derived buoyancy/drag; link existing water and extraction tickets rather than duplicating sink/float rules.

## Acceptance Criteria

Visual scenes show two concave rigid bodies held by a sideways beam with an angled brace; removing the brace concentrates stress then causes delayed material-specific failure and both bodies fall. Also show symmetric/eccentric bridge loads, wood splitting vs stone cracking vs crumbling, submerged wood draft/stone sinking derived from composition, and chunk-boundary save/reload. Density is never a manual sink/float toggle. Water/material conservation, deterministic timing, stable resting frames, Vulkan validation, timestamps and bounded storage are checked at quarter-native resolution; existing headless/UI suites pass.


## Notes

**2026-10-12T21:47:16Z**

Compiling RED for actual quarter-native L/U concave loads on a sideways beam plus angled brace: require GPU stress display, intact support, stress rise after brace removal, delayed cascade and visible BMP artifacts. Initial RED fails missing structural overlay, before scene placement; remaining dependencies and visual acceptance stay incomplete.

**2026-10-12T22:22:29Z**

Actual Wayland quarter-native scripted demo now passes placed/marked structure creation, brace removal, delayed two-concave collapse, overlay cycling and reset. Submerged anchored wood transfers density-derived net buoyancy (-8640 external / +8640 reaction) into GPU support graph. Existing coupled-material visual fixture now has real connected bedrock supports rather than floating fixed-wall assumptions; mud/fracture/movement smoke passes.

**2026-10-12T23:01:09Z**

Full native UI and quarter-native battery passes: 442 reported cases including normal headless stage, zero failures and zero Vulkan validation errors. Actual paced scenes cover L/U concave loads, angled-brace removal/delayed cascade, wood/stone/mortar failure through ground contact, centered/eccentric seam load sharing, exact next-tick rendered checkpoint restore, density-derived float/sink, conserved water and identical paused frames. Native quarter smoke: 57.63 physics ticks/s (55.1 adaptive frames/s); stage GPU rigid 2.0/fluid 4.1/granular 4.1 ms. Automatic 16-component benchmark: 12.981 ms rigid/structure, 23.407 ms whole; 67.726 MiB mapped and 107.196 MiB device. Final normal rerun includes the newly wired two-case wet handoff test before closure.

**2026-10-12T23:07:46Z**

Final corrected make test battery passes 261/261, including both dry-to-wet support-load and sound-detachment handoff cases. Full native/UI/quarter battery passes 442 reported cases; spirv-val and strict C11/-Werror builds clean. Existing fixed representation bounds are documented and larger extraction is separately tracked; no runtime CPU physics fallback.
