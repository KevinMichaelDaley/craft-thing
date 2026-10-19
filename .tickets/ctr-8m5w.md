---
id: ctr-8m5w
status: closed
deps: []
links: []
created: 2026-10-17T07:53:17Z
type: chore
priority: 1
parent: ctr-6fii
tags: [renderer, integration, vulkan]
---
# Merge committed optimizer checkpoint into renderer branch

User requests merging existing optimizer changes into renderer worktree. Merge committed main7b65fd5 only; exclude dynamic body-pool/diagnostic WIP. Preserve lit renderer and optimized reference draw, private shader ABI, WSI synchronization and new parallel physics paths.

## Acceptance Criteria

Resolve conflicts without replacing either implementation, rebuild merged GPU/shaders, run CPU/headless and renderer integration/visual checks with explicit synchronization validation, record any pre-existing feature/performance limitations, and push renderer branch. This is checkpoint integration, not closure of ct-wuci or final ctr-qlsd acceptance.

## Notes

**2026-10-17T08:47:31Z**

Integration complete: renderer fb395f4 plus optimizer main7b65fd5, preserving lit renderer and optimized reference draw plus WSI barrier scopes. Fresh warning-clean builds; make test_gpu_tools PASS; all28 SPIR-V modules validate; full make test PASS with DC_VK_VALIDATE=1 and synchronization validation (no VUID/SYNC hazards); scene_gpu and displayed renderer_visual PASS with same validation; full make test_ui and make test_quarter_native PASS without validation. Isolated renderer performance remains RED10.563ms vs unchanged2ms and is recorded on ctr-21eh; no final renderer/performance completion claim. Newer dynamic storage prototypes intentionally excluded. Logs/capture under build/renderer-merge-* and build/screenshots/renderer_merge_lit_motion.png. GPU released to optimizer.
