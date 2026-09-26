---
id: dun-tgjx
status: open
deps: []
links: []
created: 2026-09-26T17:13:13Z
type: task
priority: 2
assignee: kmd
parent: dun-oa1i
tags: [gpu, fluid, streaming]
---
# Batch GPU boundary flux proposals for streamed chunks

The world-anchored transfer API now resolves diagnostic transfers to pinned GPU slots, but the fluid/sand simulation still lacks a batched producer for proposals crossing the outer resident edge. Build a GPU proposal buffer keyed by signed chunk coordinate and generation, resolve conflict-free batches in compute, and carry pending flux until destination chunks arrive. Keep ordinary physics on GPU without per-frame copyback or one submission per proposal.

## Acceptance Criteria

Long-run fluid and particle scenes cross a temporarily absent chunk boundary without lost mass or duplicate updates; camera moves and slot reuse cannot redirect proposals; a benchmark compares batch throughput to resident interior flux.

