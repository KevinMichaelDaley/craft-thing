---
id: ctoae-mi91
status: closed
deps: [ctoae-hgsd]
links: []
created: 2026-11-27T04:29:47Z
type: task
priority: 1
assignee: kmd
parent: ctoew-5swd
tags: [offscreen, streaming, world-clock]
---
# Align the confirmed clock of proven idle chunks before foreground edits

The loader preserves an idle snapshot’s older completed time. If the returned tile receives new material at that older epoch, a later offscreen catch-up can include time before the material existed.

## Design

Resolve the authoritative stream or structure channels first. Prove that the tile remained independent and idle before advancing its epoch without solver work. Preserve the confirmed time of any tile whose history cannot be proven.

## Acceptance Criteria

Keep a sealed idle tile on disk while the world clock advances, return it, add fluid and evict it again. Confirm that the new fluid advances only for elapsed time after the edit, including a cold reload and a competing stale stream record.


## Notes

**2026-11-27T04:29:47Z**

publish_loaded_chunk receives a stream result, but the structure store can replace its physical channels. Base the idle decision on the actual owner selected by the loader.

**2026-11-27T04:57:04Z**

The native two-screen pan confirms the dry room returns at world time 2.0625, all 4,096 water cells are unchanged on admission at the edit epoch, and one 0.03125-second workspace step redistributes all 327,680 water units before the 2.09375-second cold reopen. With an authoritative modern structure archive and a conflicting generation, the dry vacuum chamber aligns to 8.0 seconds and the wet chamber keeps its 3.0-second epoch even though the other stream packet claims 6.0. Both regressions passed with RTX core and synchronization validation. 80 host cases, 19 ASan/UBSan cases, build dependency checks, and 23 core Vulkan cases passed.
