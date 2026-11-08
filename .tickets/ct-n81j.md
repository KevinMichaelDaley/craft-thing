---
id: ct-n81j
status: closed
deps: []
links: []
created: 2026-11-01T23:46:30Z
type: bug
priority: 2
assignee: kmd
tags: [tests, physics]
---
# Reconcile corrupt pixel geometry regression with valid ash material 15

Unhidden default runner in beaver continuation exposes tests/vulkan/pixel_geometry_tests.c:102: test_corrupt_tiled_geometry_is_rejected expects nibble material 15 to be rejected, but shared material_density.h now explicitly assigns body material 15 to ash. Binary reports 6 passed, 1 failed. Existing physics/test sources are unchanged by beaver work. Log build/agents/beaver-player/continuation/suite-095.log. This is separate from known ct-lc9o/ct-il37; no assertion has been disabled or weakened.

## Acceptance Criteria

Preserve valid ash geometry and meaningful corrupt-geometry rejection coverage using an actually malformed packet; do not reject assigned material IDs or remove corruption checks. Run pixel_geometry_tests and full suite. Verify material namespace assumptions across analogous fixtures.


## Notes

**2026-11-07T20:30:55Z**

Fixed in 79b1df3a: pixel_geometry_tests corrupt-tile case now corrupts the tile header (tile claiming row 1 of a single-row body) instead of material nibble 15, which is valid ash inventory. 7/0 green; full make -k test run green (203 suites). No assertion weakened.
