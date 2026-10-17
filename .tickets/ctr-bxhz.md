---
id: ctr-bxhz
status: closed
deps: []
links: []
created: 2026-10-17T09:31:06Z
type: bug
priority: 1
tags: [physics, structural, contacts]
---
# Finish the structural visual collapse after the right concave body settles

User reports right stoneU floats afterotherfalls in firstwood/stone concave structural scene. Reproduce actualVulkan displayedfixture beyond current180ticks and inspect geometric support/contact state. Add observed failing regression then fix physicalcausewithoutfakeground snapping, serialwholebody scans or disabling concavities.


## Notes

**2026-10-17T09:42:36Z**

Observed RED8417026: original180post-brace ticks end rightU airborne/no currentcontact, omega0.0286713 and vx0.35156/vy0.05029; new settlement assertion fails. Longer600tick run reaches stablefloorrest without any production physics change. GREEN verified actualdisplay under core+sync validation: omega0, v(0,0), COMx182.325 inside geometric floor-support span[174.342,190.341], actual vertices within0.05cell contactskin of floor100, preserved mass340.2. Final manifold may legitimatelyomit contactskin-separated stationary witnesses, so support assertion independently uses exact compoundvertices against knownflatfloor as well as contactrecords. Both originalfall assertions retained, and finishedframe nowrequires rightU motionless and supported. No VUID/SYNC diagnostics. Log build/structural-settlement-green.log.
