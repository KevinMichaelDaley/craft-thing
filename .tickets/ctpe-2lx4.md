---
id: ctpe-2lx4
status: open
deps: [ctpe-5lmh, ctpe-b1r2]
links: []
created: 2026-10-14T03:22:18Z
type: task
priority: 1
parent: ctpe-6m89
tags: [libresprite, assets, animation, props]
---
# Validate and compile prop packages with a game asset reader

Implement host-independent schema validation and deterministic packing, typed lossless metadata maps, texture channels and C11 runtime reader with overflow/path/bounds/version/integrity checks.

## Acceptance Criteria

RED-GREEN malformed palette/frames/alignment/schema/path/count tests, stable repeated exports, exact decode, failed install preserves prior package. Runtime uses canonical names/explicit conversions, no gamma-transformed material IDs or frame-time file parsing.

