---
id: ctpf-b819
status: open
deps: []
links: []
created: 2026-10-14T04:50:49Z
type: bug
priority: 2
tags: [prop, exporter, renderer]
---
# Align compiled prop appearance validation with the shared source record ABI

Canonical package reader currently accepts positive nonzero reserved float words44/52/56 even though compiled prop v1 requires zero bits. Renderer import3933374 and GPU authoring reject nonzero reserved words, including negative zero. Explicit normal and absent-normal semantics also need consistency with renderer default resolution; raw absent channels must not become overrides.

## Acceptance Criteria

Malformed reserved words and invalid explicit normals rejected with output unchanged; absent surface channels retain renderer default semantics; valid real exported packages continue to load. Add actual compiling/observed RED coverage in exporter worktree and coordinate ctpe runtime integration.


## Notes

**2026-10-14T04:55:38Z**

Observed CPU RED independently of GPU: /tmp/craft-prop-channel-contract-tests.c compiled against read-only sibling prop.c. Valid152-byte canonical package passes; correctly checksummed variants with reserved float1.0 at44 and negative-zero bits at52 are wrongly accepted, yielding1pass/2fail. No sibling files changed. Producer owner should move these cases into its reader tests and tighten reserved-bit validation.
