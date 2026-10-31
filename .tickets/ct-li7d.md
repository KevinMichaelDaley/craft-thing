---
id: ct-li7d
status: closed
deps: []
links: [ct-sqpj, ct-42nt, ct-w5rq]
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


## Notes

**2026-10-31T22:48:20Z**

Physical data used for the hot char curve and wood product ledger: IAEA Thermophysical Properties of Materials graphite heat capacity rises toward about 2 kJ/(kg K) at 2000-2500 K (https://www-pub.iaea.org/MTCD/Publications/PDF/IAEA-THPH_web.pdf); UNIDO Industrial Charcoal Making from Forestal and Agricultural Waste reports wood charcoal mass yields spanning about 19-26% of dry feed for several feedstocks (https://downloads.unido.org/ot/48/03/4803900/10001-15000_11923.pdf). The game uses graphite as a high-temperature char proxy and a 20% wood char yield; total wood chemical energy stays 18 kJ per mass unit, with 5.6 kJ retained in char and 12.4 kJ primary release.

**2026-10-31T23:16:31Z**

Final GREEN: removed forced small-body burn floor for char while keeping it for wood; hot-char enthalpy uses a conserved temperature-dependent capacity above 1800 K and is shared by CPU state, GPU reaction, thermal sampling, and body sampling. Wood yield changed in locked manifest from 25% to 20% char, shifting the balanced 18 kJ/kg chemical ledger from 7/11 to 5.6/12.4 kJ/kg char/primary heat. Isolated GPU char ledger peaked 2401.8 K, reacted 55/500 fixed-point units, and conserved reaction heat within 1 J; CPU combustion and reactive-state tests passed. Two full-profile 1920x1080 X11 quarter-native displayed runs passed 8/8 dry, 0/8 wet; sampled max char 2422.6/2441.3 K and max gas 2134.2/1967.7 K. Strict 1000-frame benchmark passed twice: means 16.566/16.590 ms, worst 28.747/20.376 ms. Final tools/with-vulkan make -j8 test passed exit 0. Separate pre-existing standalone hot-skin failures tracked as ct-w5rq.
