---
id: ctowpe-xshl
status: closed
deps: []
links: []
created: 2026-11-27T14:41:11Z
type: bug
priority: 1
assignee: kmd
parent: ctowpe-t43r
---
# Catch up an acknowledged sleeping slot on an immediate camera reversal

Exercise the next live streaming tick when a newly visible sleeping slot still occupies the table and its confirmed checkpoint trails the world. Verify the native solver advances the material before foreground admission.


## Notes

**2026-11-27T14:43:05Z**

The Vulkan regression first failed with a visible slot still at its saved timestamp. Synchronization validation passed after the fix with all 14000 units retained, water transported from the source cell, and the returned slot exactly at world time. All 25 level timing tests passed.
