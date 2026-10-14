# Directional RGB field and visibility contract

Appearance uses independent 32-byte linear color/surface records. Visibility
uses a 16-byte node `(minimum opacity, maximum opacity, known residency, reserved)`
and one-pixel leaves. Parallel four-child reductions preserve odd-sized edges.
Unknown leaves have opacity one regardless of caller color or nominal opacity.
A coarse empty node can be skipped; mixed nodes must descend to actual pixels.
No hierarchy operation averages away a one-pixel wall.

The direct-light field stores four linear HDR RGB vectors in four vec4 records:
L0 energy, Lx, Ly and Lz first moments. Alpha components are reserved. Colored
sources retain separate RGB direction moments; equal opposite sources can cancel
Lx without erasing L0. Direct-source queries retain exact direction before
cooperative reduction instead of quantizing source directions into angular bins.
This is sufficient for the initial direct-transport stage; an indirect angular
transport representation would require separately validated storage.

Point source position, height and influence radius are in scene pixels. Radius
bounds influence and permits early culling. Intensity and RGB are nonnegative
linear relative radiance, independent of albedo. The intended attenuation is
`intensity * (1 - distance_squared / radius_squared)^2 / max(distance_squared, .25)`
inside the radius, zero outside. The quarter-pixel squared denominator bounds
the value at the emitter. Height contributes to both attenuation and the incoming
Z direction. Sun intensity is uniform incoming radiance, with a normalized XYZ
direction and explicit known-sky boundary bits (left, right, top, bottom).

The prototype supports sixteen explicit point lights plus one sun. It does not
silently truncate imported emissive maps. Their potentially much larger source
sets need parallel compaction, spatial culling and batching during prop integration.
An emissive prop texel is a separate linear HDR RGB value, derived once from
sRGB input and multiplied by its authored strength/alpha. It never changes the
physical material and never reuses albedo as emitted radiance.

Each receiver/source visibility query runs independently on the GPU. Along a
single ray, voxel traversal is necessarily ordered; it has a bounded iteration
budget and returns no contribution if visibility cannot be certified. Outside
resident data is unknown, except for an explicitly certified sky boundary.
Sources outside a viewport may contribute when their visibility path lies within
resident input. There is no infinite-world sunlight promise from a finite cache.

These channel modules are initial GPU contracts and diagnostic harnesses.
Scene extraction from current terrain/body coverage, GPU dirty invalidation,
normal rotation and normal-mode frame composition remain subsequent integration
work. The reference/import uploads are explicit setup/edit operations, not
ordinary-frame CPU scene copies. Diagnostic getters are not the rendering path.
