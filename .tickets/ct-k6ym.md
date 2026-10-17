---
id: ct-k6ym
status: in_progress
deps: []
links: [ct-tu55]
created: 2026-10-17T05:25:05Z
type: bug
priority: 1
tags: [vulkan, presentation, synchronization]
---
# Chain acquired swapchain transitions to semaphore waits

Explicit synchronization validation on displayed native640x360 frames reports WRITE_AFTER_READ on acquire/layout transition and PRESENT_AFTER_WRITE from frame0, before diagnostic pair profiling. Frames finish; this is not evidence of historical crash cause.

## Acceptance Criteria

Compiling RED presentation regression proves initial swapchain transfer transition chains from the acquire semaphore transfer wait stage. Preserve mailbox, finite acquire, image semaphores, render fence and unchanged15second watchdog. GREEN displayed synchronization validation has no WSI hazards; full UI/quarter correctness and native timing remain unchanged in quality.


## Notes

**2026-10-17T05:32:52Z**

Targeted GREEN: acquire transition sourceTRANSFER removes WRITE_AFTER_READ; final PRESENT transition destinationBOTTOM_OF_PIPE removes PRESENT_AFTER_WRITE. Compiling regressions failed separately before each fix. Unchanged15second three-context/270frame presentation watchdog now passes both scope assertions. Normal displayed native640x360 scene with explicit synchronization validation has0VUID/SYNC diagnostics; its wall-time gate predictablyfails withvalidation enabled and is not used as performance acceptance. Full combined regression pending current optimizer checkpoint.

**2026-10-17T05:34:56Z**

Corrected acquire and final presentation scopes also validate the180-frame displayed/frozen-pair diagnostic:0VUID and0SYNC hazards,15candidates profiled. This confirms earlier378diagnostics were WSI scope issues, not the diagnostic body/activity snapshot path or evidence of a driver hang. Full combined optimizer regression remains pending.
