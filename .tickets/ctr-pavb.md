---
id: ctr-pavb
status: closed
deps: []
links: [ctr-r0zy]
created: 2026-10-18T00:27:06Z
type: bug
priority: 2
---
# Save the active scene on SIGTERM and SIGINT through normal shutdown

The interactive demo currently exits directly on termination signals and can lose active unsaved edits. Signal handlers should request exit through the main loop, which then flushes the existing scene save path.

## Acceptance Criteria

Termination signals request normal shutdown without Vulkan or file IO in the signal handler; edited active chunks, rigid bodies and colors are saved; regression proves the checkpoint contains the latest edits.


## Notes

**2026-11-02T18:15:53Z**

RED 95a1b36f, GREEN b72a9bbe. dc_app_install_shutdown_handlers/consume in src/app/shutdown.c (sigaction + volatile sig_atomic_t only, no Vulkan/file IO in handler); main.c installs after parsing and consumes at frame top so exit flows through dc_level_view_destroy. shutdown_tests 3/0: both signals request once and consume cleanly, reinstall safe, and a SIGINT-driven shutdown restores carved cells, a material edit, the extracted island rigid 0x80000001 and the color pixel. Live demo runs: timeout SIGTERM/SIGINT on dungeoncraft_quarter_native exit 0 (not signal-killed) and flush chunk saves to the world dir. Extraction quirk documented: brush extraction fires like the island test, with the second same-frame paint at the carve center committing the brush transaction.
