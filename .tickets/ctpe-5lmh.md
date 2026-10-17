---
id: ctpe-5lmh
status: closed
deps: []
links: []
created: 2026-10-14T03:22:18Z
type: task
priority: 1
parent: ctpe-6m89
tags: [libresprite, assets, animation, props]
---
# Define multilayer prop schema, palettes and authoring fixtures

Define material/role/object legends, color/emission/normal/roughness/relief channels, frame timing/tags/pivot/scale, visual-only physics invariance and explicit state variants. Provide template and representative fixtures.

## Acceptance Criteria

Exact canonical material mappings, alpha/holes, multiple namespaces, metadata hidden/locked layers, compatible role/material combinations and explicit higher-visual-resolution owner mapping documented. Fixtures include mixed concavities, supported structural pixels and unequal animation durations.


## Notes

**2026-10-14T03:54:31Z**

Complete: assets/props/schema-v1.json exact canonical material/role legends; GPL palettes; editable RGBA and indexed example animations (two different concave mixed-material bodies, marked supported structure/terrain, emission-only decoration, unequal90/170ms durations, idle tag), blank template and deterministic generator. Coordinates and1:1 v1 mapping explicit; higher-resolution metadata resampling rejected until explicit owner plane exists. Full validated make test and offline/real-host batteries pass.
