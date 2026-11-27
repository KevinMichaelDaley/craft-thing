---
id: ctgcs-wy3j
status: closed
deps: []
links: [ctsg-6060]
created: 2026-11-27T02:04:51Z
type: task
priority: 1
assignee: kmd
parent: ctlo-r2ap
tags: [gas, vulkan, tests]
---
# Preserve pigment admission at the gas carrier precision boundary

On 8e45829e the native publication suite rejects the explicit and implicit color-transport controls and the two-chunk color seam before their first step. The input gives each affected cell equal 0.1 exhaust and pigment. Admission reports Gas pigment has no admitted carrier.

## Design

Pin a focused CPU and Vulkan reproducer at the equal-carrier boundary and trace the double input, represented packet, and remainder separately. Preserve the carrier invariant for invalid stock and the existing closed-budget and seam assertions for valid input.

## Acceptance Criteria

The three colored transport cases reach their existing physics checks and pass under Vulkan validation. Include a tight invalid-carrier case with unchanged admitted state after rejection.

## Notes

**2026-11-27T02:08:47Z**

The CPU source trace identifies the representation mismatch before any Vulkan write. Seed packing rounds pigment to one float but retains an exhaust residual. With equal double inputs of 0.1, the revalidated packet decodes pigment 0.10000000149011612 and carrier 0.099999999999999978. The strict admissible upper bound is 0.10000000000000034. Keep strict validation on the original input and on the published packet when choosing the representable pigment.

**2026-11-27T02:20:08Z**

RED compiled and reproduced packet revalidation failure on equal pigment and carrier. Seed admission now retains nearest-float pigment where valid and otherwise selects the greatest float within the represented exhaust-plus-soot inventory. The original inputs and decoded packets still pass strict thermodynamics. Three host tests passed, including atomic rejection of a barely over-limit original value and rejection of corrupt checkpoint pigment. All three passed ASan and UBSan, all 26 gas numerics and fixture cases passed, and all 143 direct make test build prerequisites compiled. The explicit, implicit, two-chunk seam, and compensated checkpoint fixtures all passed on the RTX A2000 with core and synchronization validation on a4b5e887. Exact refactor native repetition and full make test are queued.

**2026-11-27T02:33:22Z**

The exact 1c3c6f1a native rerun also passed all four RTX A2000 cases with core and synchronization validation: explicit transport, implicit transport, the two-chunk color seam, and compensated checkpoint restore. The complete make test run remains queued.
