---
id: ctpe-g0sl
status: open
deps: [ctpe-b1r2, ctpe-847v, ctpe-d6m2]
links: []
created: 2026-10-14T03:22:18Z
type: task
priority: 1
parent: ctpe-6m89
tags: [libresprite, assets, animation, props]
---
# Run real LibreSprite-to-game visual and failure acceptance

Export an actual template animation using shipped LibreSprite script, compile/import into Vulkan game, exercise two distinct concave props, terrain/structure support, colored/emissive decoration and save/reopen. Capture display evidence and run relevant batteries.

## Acceptance Criteria

Actual host + actual game end-to-end, RGB/indexed/offset/linked/hidden metadata coverage; materials/roles/colors/phase/emission stable and correct. Invalid data/atomic capacity cases pass; script mocks alone insufficient. Report import/runtime cost and Vulkan/SPIR-V validation; negotiate GPU window.


## Notes

**2026-10-14T03:54:31Z**

Actual LibreSprite1.3 host-to-package-to-Creader offline round trip passes on shipped RGBA/indexed examples;25 newchecks + full validated baseline battery pass. Actual Vulkan game import/display, atomic mixed-role capacity rollback, save/reopen phase/emission ownership and visual evidence not yet implemented. Keep open; mocks/package exports alone do not satisfy acceptance.
