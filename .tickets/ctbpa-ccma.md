---
id: ctbpa-ccma
status: in_progress
deps: []
links: [ct-vkik, ct-rrwn, ctlo-2q4f]
created: 2026-11-26T21:24:07Z
type: bug
priority: 1
assignee: kmd
parent: ct-exwr
tags: [player, world, save]
---
# Preserve earned player inventory when reopening a saved world

An authored tree cell and its removal survive a completed sandbox save, but the earned material balance currently lives only in the player session. A native 3c94b591 repro harvested one wood cell for 39,322 Q16, completed shutdown and reopened the world. The cell remained air and the restored player balance was zero.

## Design

Review the level save and persisted body identity together. Record exact owned material balances under stable player identity with the world save. Retain balances for a seat that rejoins and account for creation or placement against the same material ledger. Preserve legacy world loading.

## Acceptance Criteria

Two local players retain distinct exact balances through a completed save and reopen. A surviving harvest is credited once and a paid placement is debited once. A failed save or invalid inventory record preserves a consistent world and player state. A legacy world loads with an empty inventory.


## Notes

**2026-11-26T21:24:07Z**

The native reproduction extends the existing authored tree harvest/reopen case in tests/app/play_spawn_tests.c. The added assertion compiled and failed on the reopened balance (0 versus 39,322 Q16) while the persisted cell was still air. Reproduction log is /tmp/craft-harvest-inventory-reload.log.

**2026-11-26T21:43:01Z**

The pre-fix native suite kept the cut terrain and lost the restored Q16 balance. The save now stages dirty chunks, the body checkpoint and a validated inventory record under the same world writer. It synchronizes the staged files and publishes the committed transaction with recovery on the next open if publication stops. The two-seat fixture verifies harvest, one paid terrain placement, a save with one seat absent and a rejoin on a different controller. Four visible play cases and four filesystem/inventory cases passed, including ASan/UBSan. The extra native precommit failure fixture and broad suite are running or queued.
