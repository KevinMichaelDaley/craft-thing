# Granular bearing in the rigid solver

Rigid contacts with individual, freely moving MPM particles cannot by themselves
represent the bearing strength of a deep, grounded bed. The rigid solver builds
a support field before its substeps, reusing the MPM grid and force scratch.
Each occupied sample links downward within the MPM kernel footprint. Parallel
pointer doubling resolves terrain support and accumulates supported mass and
depth in logarithmic dispatches. Air and missing pages do not anchor a bed.
There is no CPU component walk or fixed supported-depth limit.
Support links require an occupied adjacent row; the wider interpolation kernel
does not transmit bearing across a clear air row. Otherwise a grain frozen
inside the trunk could keep it suspended above an excavated gap.

Grounded mineral grains supply a foundation response to upward-facing contacts.
Ungrounded grains, deeply overlapping grains, and flexible leaf flakes retain
the finite-mass two-way coupling. This prevents trapped debris from suspending
a body. Contact preconditioning counts grounded bearing rows as binding rigid
constraints, rather than diluting their response by the mass of one grain.

The bearing estimate uses supported mass per sample, bed depth, projected body
width and material friction. Pressure above this estimate adds a small bounded
settlement allowance. This is an approximate drained bearing model, not a full
soil constitutive model; the width approximation uses the projected body bounds.
Particle mass, transfer capacity and packing rules remain in MPM.

Existing overlap in an embedded tree's loaded configuration is a contact rest
offset. The solver resists additional compression; it does not lift the tree
out of its initial contact overlap. Otherwise soil enters each vacated cell,
establishes a higher support surface and repeatedly jacks the tree upward.
Grounded contacts are hard below yield; keeping the loose-grain spring
compliance would absorb its elastic compression into the resting offset every
substep and create permanent settling even below yield. Overload settlement
remains explicit and rate limited.

The equilibrium regression reproduces a rise of 10.5 pixels without the rest
offset. Its extended version checks late drift over 360 ticks, final speed, no
upward displacement, no soil refill and conserved soil mass.

`dense_support_tests` exercises wood and stone on settled dirt, gravel and sand,
an extreme overload, exact grain mass, trapped leaf contacts and partial root
support removal. `level03_support_tests` exercises the authored tree scene,
single-cell dirt deletion, packing, mass conservation and subsequent trunk cuts.
Run GPU executables sequentially, including graphical tests. Set
`DC_EQUILIBRIUM_ONLY=1` to select the embedded-root and air-gap regressions.

`tools/demo_level_piece03` uses `world_chunks_level_piece03_equilibrium` for a
fresh authored scene. Earlier edited and collapsed saves remain in their
original directories; `--world-dir` can select one explicitly.
