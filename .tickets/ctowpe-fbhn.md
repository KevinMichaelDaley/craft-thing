---
id: ctowpe-fbhn
status: closed
deps: []
links: []
created: 2026-11-27T11:19:23Z
type: task
priority: 1
assignee: kmd
parent: ctowt-7388
tags: [offscreen, streaming, save]
---
# Flush already queued elapsed water work on close during a shared pressure cycle

## Acceptance Criteria

Closing during a confirmed partial water cycle with the remaining elapsed step already queued persists both owners at the common completion time and matches a monolithic solve after reopening. Closing must not invent elapsed time or publish an unconfirmed interval.


## Notes

**2026-11-27T11:21:25Z**

All four paired GPU cases passed on Xephyr, including close with three pending steps and comparison of both reopened owners to the monolithic reference. The unconfirmed receipt case still preserves the prior save.
