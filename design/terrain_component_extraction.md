# Terrain component extraction

Terrain membership and material are separate concepts. The Vulkan connectivity
graph includes every tagged solid terrain pixel, including soil, gravel, sand
and debris; air and water are excluded. Loose granular pixels with terrain flag
zero are excluded from connectivity, bonds and extracted body mass. Untagged
rigid-capable paint retains its existing automatic body creation behavior.
Cardinal and diagonal connections cross resident chunk boundaries using the
existing component union pass.

Authored terrain starts fixed, including disconnected components in a loaded
scene. Loading records its bonds. Subsequent brush edits or particle occupancy
changes invalidate connectivity. A modified component that becomes isolated
from the surrounding terrain is published as a single pixel body. A connection
through tagged soil is an actual graph connection, rather than a material-specific
support exception. Tagged soil inside a crown belongs to the crown and cannot
anchor it to itself. Already broken or dissolved grains remain independent
particles and collide with the resulting body normally. They cannot bridge
terrain components, including across chunk seams. Components reaching unknown
resident boundaries remain deferred.

Extraction copies the whole component's pixel material IDs and authored colors.
Body material IDs distinguish soil, gravel, sand and leaf debris. Rendering maps
them back to their terrain materials, and persistence validates their respective
densities. Mass reductions use hundredths so sand's density of 2.65 survives
save/load validation. Included granular particles are removed when the body is
published, preventing a second simulation representation at the old location.
Save/load and prop bond restoration preserve a cleared terrain flag rather than
reapplying it from the source asset. Particle transport clears obsolete cell
flags when the cell's material changes, so vacated terrain locations cannot
retag subsequently arriving loose grains.

This graph determines when terrain becomes a body. Granular bearing contacts
described in `granular_bearing.md` determine how an existing moving body rests
on particles; they do not decide whether terrain is connected.

Regressions cover a mixed-material detached crown and connected stump, material
retention through save/load, and the actual level03 scene: a buried cut remains
connected while a higher complete cut releases the crown without removing its
embedded soil pixels from the fixture.
