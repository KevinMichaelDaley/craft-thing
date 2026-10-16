---
id: ct-nwfz
status: in_progress
deps: []
links: []
created: 2026-10-16T19:34:23Z
type: chore
priority: 1
tags: [build, toolchain, vulkan]
---
# Keep pinned Vulkan developer tools and diagnostics reproducible in repository

Replace reboot-lost /tmp toolchain with checked-in bootstrap, pinned checksums, persistent repo-local install/cache, activation wrapper and diagnostics. User explicitly requires tools included in repo.

## Acceptance Criteria

Compiling failing toolchain contract tests first; pinned tool bootstrap works without root; offline reuse after reboot; Make and subprocess tools use same repo paths; no hardcoded /tmp or driver replacement. Document bootstrap and run appropriate CPU/SPIR-V verification.

