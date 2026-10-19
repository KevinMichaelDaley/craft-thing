---
id: ctpe-2lx4
status: closed
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


## Notes

**2026-10-14T03:54:31Z**

Complete: deterministic JSON+aligned RGBA PNG+lossless typed maps+compiledCPRP, strict palettes/role compatibility/invariance/variants/path/integrity/version/timing checks, atomic Linux directory exchange publication. Allocation-free C preloaded reader validates whole-file CRC/header/layout/material/role/float records and explicit namespace conversion. Renderer agreed first32-byte appearance ABI + byte60 optional surface mask; separate HDR emission/alpha.25 newchecks pass, sanitizedC5/5 pass, full validated make test exit0. Forward-loop helper is preliminary; bounded GPU animation/tag/pause persistence remains ctpe-d6m2, never collider animation.
