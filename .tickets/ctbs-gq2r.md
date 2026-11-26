---
id: ctbs-gq2r
status: open
deps: []
links: [ctlo-ldb7, ct-xsll]
created: 2026-11-26T17:59:52Z
type: task
priority: 1
assignee: kmd
parent: ct-exwr
tags: [player, materials, reactive]
---
# Replace beaver wood aliases with explicit fur and textile reaction profiles

The native asset manifest explicitly identifies the shared wood profiles of fur, robe, hat and tail as gameplay placeholders. The body shape stores packed collision material nibbles, while reactive checkpoint owner validation currently requires the single terrain profile returned for that nibble. This prevents authored surface-specific behavior from surviving admission and reload independently of the collision class.

## Design

Add a compatible surface-profile identity along the canonical parcel path under the existing physical owner. Preserve the current body ABI and ordinary terrain material validation. Keep source art, frame mapping, and per-player ownership intact.

## Acceptance Criteria

Fur and textile equipment have explicit registered profiles with distinct, tested heat and moisture response on the live beaver. The chosen profile survives animation, mirror, movement and save/reload without resetting quantities or changing the owner. Legacy saves and unrelated wood parcels remain valid. Verify exact material stock and heat accounting on isolated CPU and Vulkan cases, the rendered sandbox and the full test suite.


## Notes

**2026-11-26T18:00:04Z**

Source audit locates the aliases in assets/player/beaver/manifest.json and src/player/beaver_asset.c. src/vulkan/reactive_checkpoint_gpu.c currently rejects a stored body parcel when its material differs from the default profile for the shape nibble. Test the admission and reload contract together before changing surface coefficients.
