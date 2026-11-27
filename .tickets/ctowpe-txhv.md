---
id: ctowpe-txhv
status: in_progress
deps: []
links: []
created: 2026-11-27T11:04:37Z
type: bug
priority: 1
assignee: kmd
tags: [testing, streaming]
---
# Exercise smoke restore with authoritative and superseded gas records

## Acceptance Criteria

Smoke restoration recovers exact archived gas when the stream record has no replacement claim. A verified replacement with missing gas must refuse smoke restoration and preserve the saved files.

