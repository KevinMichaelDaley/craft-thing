# Explicit structural companion proposal

This is a producer extension proposal, not a claim that the current LibreSprite
exporter emits mechanical graphs. Exporter/game structural placement stays gated
on its implementation and `ctpe-847v` / `ctpe-g0sl` integration acceptance. The
already shipped material/role/object/anchor layers remain valid physical labels.

An optional `structure.json` companion describes the explicitly authored member
and joint network for one selected physical state variant. An aligned
`structure.members.u32` plane assigns each structural pixel its member ID, with
zero outside structural pixels. Files are package-local, nonsymlink filenames,
covered by the manifest's SHA256 table and validated before staging. Their absence
is an actionable missing-metadata error when structural pixels exist. An anchor
hint never silently supplies graph topology, stiffness, or a terrain root.

Schema name `craft.prop.structure`, version1. Required fields:

| Field | Meaning |
| --- | --- |
| asset_uuid, state_variant | Exact selected package/state identity; metadata cannot attach to another state. |
| width, height, member_plane | Exact physical canvas and LE-u32 aligned ownership filename. |
| coordinate_space | `asset_rest`: integer cell origin/pivot from the package, optional fixed16.16 node locations. |
| nodes | Stable nonzero IDs/generations and rest positions. |
| members | Stable IDs/generations, endpoint node IDs, named structural profile, explicit section/grain parameters and exact owned pixel correspondence. |
| joints | Stable IDs/generations, endpoint node IDs, existing typed joint kind, normal/orientation, stiffness and interface capacities. |
| anchors | Stable IDs/generations, node and typed interface, explicit terrain contact footprint/material and capacities. |

Node coordinates are translated into canonical signed64 chunk plus local fixed
coordinates by the GPU placement preflight; no chunk*64 signed64 multiplication.
The input pivot remains a placement reference, never the mass center or a root.
Every carry, plane extent, count, ID/generation, endpoint and numeric coefficient
is validated. Graph capacities are diagnosed explicitly rather than truncating
members or fusing disconnected authored geometry to fit a pool.

The member plane must cover exactly structural-role pixels, refer to existing
members and agree with declared rest geometry/material properties. A paint label
does not authorize using a filled rectangle instead of the actual collider. A
section model that the existing mechanical solver cannot represent is diagnosed
as unsupported until a physically justified extension is tested. Mixed assemblies
may use multiple material members with explicit joints; unsupported section
material mixtures cannot silently average strength or drop a material.

Anchors specify intended contact constraints; runtime checks actual ordinary
terrain and its generation/residency. Missing terrain cannot become support.
UNKNOWN contact freezes the whole authoring transaction. The same all-role
reservation/publication gate protects bodies, exact geometry, graph and renderer
appearance/provenance/emission. State changes are explicit transactions; visual
animation does not rebuild this graph or its colliders every frame.

The producer should validate the companion with the package compiler, retain its
source and expose a clear artist workflow. The physics feature worker owns GPU
consumption; the resumed exporter worker owns actual package/schema/script
production. Final acceptance uses real companion assets, including sideways and
angled supports, delayed release, material fracture and pending save/reopen.
