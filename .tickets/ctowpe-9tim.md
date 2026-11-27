---
id: ctowpe-9tim
status: in_progress
deps: []
links: []
created: 2026-11-27T14:38:46Z
type: bug
priority: 1
assignee: kmd
parent: ctowpe-t43r
---
# Keep a sleeping receipt when the camera returns just before shutdown

A camera move changes visibility before the next streaming tick. If shutdown falls in that interval, preserve the acknowledged sleeping owner and its older confirmed time when reopening.

