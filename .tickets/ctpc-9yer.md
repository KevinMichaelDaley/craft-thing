---
id: ctpc-9yer
status: open
deps: [ctpc-xa7p]
links: []
created: 2026-11-26T15:32:41Z
type: bug
priority: 1
assignee: kmd
parent: ctsl-7ay0
tags: [fire, coating, vulkan]
---
# Carry fractional coating reaction through terminal fuel quanta

After proportional unit debits, an initially balanced submerged CPU film still retains 2 oil, 2 jelly and 7 oxidizer Q16 after 900 simulated seconds at 60 Hz. Flooring a Q0.10 progress field discards every step once total fuel reaches four units. The current mixture eligibility would also abandon a surviving fuel species near the last debit.

## Design

Preserve subquantum reaction progress at supported cadences and settle the last mixed fuel units without conflating an unmixed film with bound-fuel chemistry. Retain save compatibility and debit finite oxidizer, heat and reaction products.

## Acceptance Criteria

With sufficient bound oxidizer, a submerged ignited mixed film reaches zero fuel in repeated normal-cadence CPU and Vulkan runs. Verify partitioned time steps, exact finite inventory and persistence, and retain the ordinary-oil dousing control.

