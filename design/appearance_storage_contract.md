# Appearance storage contract

`fr_appearance_owner_t` and `fr_appearance_texel_t` are renderer records. They do
not extend `dc_cell_t`, change material codes, or contribute mass/coverage.
`fr_appearance_resolve` uses a row-major physical material map. A visual scale N
maps an N by N block to one physical pixel; all visual samples retain that owner
index. Material zero remains a hole regardless of an imported texture's color.
Terrain material IDs and body material IDs use explicitly different namespaces.

`fr_appearance_sidecar_t` binds caller-owned fixed buffers. Each visual texel
stores 32 bytes of linear RGB, roughness, XYZ normal, and relief, plus four bytes
of physical owner index and one byte of validity. HDR color is permitted;
non-finite values, negative color, and roughness outside zero through one are
rejected. Validity is reset when storage is rebound. Identity and generation
must both match on reads, so recycled pool slots cannot expose old overrides.
Initialization/rebinding and import are explicit operations, never ordinary-frame
CPU copies or allocations. The per-texel CPU functions are reference/import
contracts; renderer generation and transfer dispatches operate on GPU storage.

The standalone owner coordinates are immutable world/rest coordinates. Runtime
streaming provenance must use canonical signed64 chunk coordinates plus local
cell coordinates rather than overflowing signed64 through chunk times64. A
body's stable appearance identity is distinct from its runtime body ID. Inverse
rigid transforms resolve current samples to rest space; normals rotate forward.
Generation governs storage lifetime, not procedural texture seeds. Viewport
coordinates and physical pose never reseed an appearance.

Defaults are deterministic and valid for older saves; explicit color/surface
overrides must subsequently be stored in the versioned appearance sidecar.
Seed-only save support is insufficient for painted texels. Persistence and
parallel extraction/fracture transfer remain ticket ctr-ddtf and require the
completed ct-wuci transaction implementation. Its birth hook supplies one source
atlas index and one destination sparse16x16 tile cell per invocation. Transfers
must preserve explicit values and preflight appearance capacity atomically with
geometry before publishing the body. No old serial extraction shader is changed
on the renderer branch.

## Authored appearance input

`fr_appearance_source_t` is the native Vulkan view of the existing compiled
prop's 64-byte little-endian record, not a second asset format. Its first32
bytes are exactly `fr_appearance_texel_t`. Linear HDR emission RGB occupies
bytes32 through43, byte44 is reserved, visual alpha is at byte48, bytes52
through59 are reserved, and the surface override mask is at byte60. Mask bits
normal1, roughness2, and relief4 match the exporter. Renderer generation and
material defaults supply each absent surface channel; the exporter placeholder
roughness must never override that decision. Imported color always stays
independent of the physical material namespace.

`fr_appearance_import` is the per-texel CPU reference for this resolution. It
preserves authored linear color, HDR emission, and alpha without encoding or
clamping energy, accepts in-place conversion, and never modifies output on
failure. Non-finite or negative color/emission, alpha outside zero through one,
invalid effective surface values, and unknown mask bits fail. Only effective
surface values are inspected: absent authored channels cannot override their
renderer defaults. Alpha zero does not remove a physical pixel, and emission
does not change its density or coverage. The runtime admission shader must
perform the same bounded conversion independently for each source pixel.

The physics feature transaction supplies the shared status, compiled physical
and appearance buffers, and64-byte source-to-destination records using native
Vulkan buffer handles plus byte offsets/extents. Its16-byte immutable provenance
is appearance identity64, source pixel32, and source frame32. Renderer admission
reserves appearance, provenance, and emission capacity before the shared
publication gate; transfer writes unpublished data; publication only observes
the globally successful transaction. Geometry publication cannot proceed if
the renderer participant fails. These integration callbacks and persistent
identity/storage tables remain open work under ctr-ddtf; the reference import
function does not provide GPU admission or persistence by itself.
