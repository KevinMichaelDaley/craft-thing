---
id: ct-q6fr
status: in_progress
deps: []
links: []
created: 2026-10-17T07:51:26Z
type: task
priority: 1
parent: ct-hhmy
tags: [vulkan, rigid, dynamic-capacity]
---
# Design dynamic GPU body ABI and safe growable resource ownership

## Acceptance Criteria

Audit every fixed64 body assumption; separate runtime body slabs/metadata from structural node/member limits; specify GPU-addressed or equivalent growable storage, generation/ID ownership, resource requests and fence-safe resize/descriptor lifetime. Host allocation coordination is allowed; CPU physics solving or ordinary geometry readback is not.

