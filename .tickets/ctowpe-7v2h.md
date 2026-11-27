---
id: ctowpe-7v2h
status: in_progress
deps: []
links: []
created: 2026-11-27T14:44:08Z
type: bug
priority: 1
assignee: kmd
parent: ctowpe-t43r
---
# Honor sleeping room ownership on world transfer requests

A clean sleeping slot with a confirmed owner can remain in the chunk table while its checkpoint trails world time. Transfers must only pin authoritative sources and must wait for destination replay before binding.


## Notes

**2026-11-27T14:47:06Z**

Both endpoint tests reproduced early pinning. After checking ownership before slot lookup and source pinning, all 27 timing tests pass. A native transfer regression covering replay, result delivery and stock conservation is queued after the exclusive measurements.
