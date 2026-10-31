---
id: ct-li7d
status: open
deps: []
links: [ct-sqpj, ct-42nt]
created: 2026-10-31T21:02:16Z
type: bug
priority: 1
assignee: kmd
parent: ctfs-81vw
tags: [fire, thermal, bonfire]
---
# Bound hot char parcel temperatures in the displayed bonfire

At full smoke_profile on X11, the displayed eight-log bonfire reaches 3526.7 K in a char-bearing wood parcel by 52.6 s while max gas remains about 1600 K. Starter average stays about 1650 K, so this is a local solid/char transient, distinct from the gas hotspot already addressed by ct-sqpj. Investigate parcel energy/capacity and reaction/radiation ledgers; do not use an unaccounted temperature clamp or extra fuel.

## Acceptance Criteria

RED GPU test reproduces a char-bearing parcel temperature runaway and captures its heat, mass, and capacity ledger. GREEN physically accounts for reaction heat, carried-away enthalpy, radiation, and shrinking capacity without a clamp; repeated displayed quarter-native full-profile bonfires retain 8/8 dry, 0/8 wet, plausible solid and gas peaks, and full make test passes.

