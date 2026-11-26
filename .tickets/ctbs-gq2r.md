---
id: ctbs-gq2r
status: in_progress
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

**2026-11-26T18:39:02Z**

Newly admitted beavers bind registered fur, textile and tail profiles directly to the authored body's existing rest texels in one arena batch. Cloth uses one profile for both robe and hat. The packed physics IDs and dry mass are unchanged. The reactions now consult the registry, and chunk, particle and snapshot validation enforce body-only profile ownership. RED built and failed only the new profile checks. Exact Vulkan body checks passed 6/6, including compensated contact heat, water debit, reaction inventory and byte-exact moved-body checkpoint restore. The GPU profile probe matched 256 CPU cases. A 9ec69b25 checkpoint restored body 2147483649 generation 1 and wood parcel 910 byte for byte, including 39322 Q16 and compensated heat 263.838911568953. Native sandbox and full-suite acceptance are pending.

**2026-11-26T18:50:41Z**

Final focused native run passed all seven player runtime cases and all three authored spawn cases. The cloth calibration retains a larger share of the same finite chemical budget as char. A real pose reversal preserved visible singeing at 7 robe and 11 hat Q16 char, and the control player's parcels were untouched. An isolated Vulkan exposure gave fur 9830 and cloth 29492 Q16 bound water, exactly matching the source debit in both cases. The native fur parcel burned from 33424 to 33290 Q16 fuel and its saved parcel and atmosphere restored byte for byte. The keyboard visual fixture now sets its own clear aim point. Six Vulkan body tests, 256 CPU/GPU rate cases, ordinary wood/tinder/oil/ash controls and fixture contracts passed. Exact full suite is queued, so this ticket stays in progress.
