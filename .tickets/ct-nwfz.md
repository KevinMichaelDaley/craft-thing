---
id: ct-nwfz
status: closed
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


## Notes

**2026-10-16T19:37:36Z**

Added committed pinned Ubuntu24.04amd64 package lock/bootstrap/wrapper and Make targets. SHA256-verified downloads and extracted compiler/headers/loader/validation layer/tools persist under ignored .tools/vulkan, independent of /tmp and make clean. Verified fresh online install, offline reuse,5 CPU contracts (bad download/cache rejection, environment/driver preservation, pinned manifest), C11 header/link smoke, shader compilation+SPIR-V validation, and11 existing CPU structural mechanics tests. Bootstrap serialized by repository lock; tests wired into make test. Full GPU regression is intentionally deferred to optimizer after bounded recovery probe under ct-wuci/ct-tu55; setup verification submits no GPU work.
