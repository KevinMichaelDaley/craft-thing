---
id: ctpf-5h0n
status: open
deps: [ctpf-iueo, ctpf-9jiw]
links: []
created: 2026-10-14T03:55:22Z
type: task
priority: 1
parent: ctpf-rvcy
tags: [physics, structural, fracture, authoring]
---
# Persist authored physics provenance and pending failure transactions

Versioned graph/body/source ownership and transaction state, including countdowns/fragments, immutable asset/source coordinates and rest mappings, coherent defaults for older saves.

## Acceptance Criteria

Rebase/eviction/save/reopen preserves physical state, appearance/emission source mapping and pending/idempotent publication. Corrupt/unsupported ownership rejected atomically with no reused-slot ghosts.

