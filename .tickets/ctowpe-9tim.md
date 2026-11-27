---
id: ctowpe-9tim
status: closed
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


## Notes

**2026-11-27T14:42:51Z**

The reproduced close/reopen failure restored no receipt after a one-chunk reversal. After the fix all 25 timing tests and the Vulkan parked-room recovery case passed on Xephyr :93.
