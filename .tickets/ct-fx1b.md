---
id: ct-fx1b
status: closed
deps: [ct-touk, ct-i5a4, ct-il9b, ct-0iq3, ct-gf6q, ct-1n9n, ct-ha8q]
links: []
created: 2026-10-11T05:43:13Z
type: epic
priority: 1
tags: [gpu, structural, graph, rigid, fracture]
---
# Anchored structural bodies with mechanical support graphs and material-dependent collapse

Introduce explicitly placed or marked structural bodies attached to ordinary terrain. Resolve load paths with force-and-moment balance, including sideways members and angled bracing, accumulate simulated-time support failure, and atomically convert detached islands to rigid bodies or material-appropriate debris. Preserve material-derived density/buoyancy, geometry, mass, momentum and streamed ownership. Design: design/structural_bodies.md. The user first requested the design/roadmap, then explicitly authorized the GPU runtime implementation.


## Notes

**2026-10-11T05:47:24Z**

Design-only session per explicit user choice: explicitly placed/marked structures, ordinary terrain retained as roots. Full proposal saved in design/structural_bodies.md. Seven child tickets carry analytic tests, GPU graph/contact loads, delayed failure/atomic conversion, material fracture/crumble, persistence, and a visible quarter-native battery with material-derived buoyancy. Dependencies are acyclic; ct-touk is the first ready implementation ticket. No runtime structural behavior is claimed or implemented in this session.

**2026-10-12T23:07:47Z**

Implementation complete for the bounded GPU engine representation: all seven child tickets and water/extraction/impact prerequisites are closed after analytical, transactional, streaming/persistence, paced Wayland visual and hardware budget acceptance. Full UI/quarter run: 442 reported passes and final normal: 261/261; zero Vulkan validation errors. Interactive preview: build/dungeoncraft_quarter_native --structural-demo. Known follow-ups include inactive MPM history reclamation (ct-3srq) and larger/more complex automatic extraction (ct-wuci); neither is silently approximated or claimed implemented.
