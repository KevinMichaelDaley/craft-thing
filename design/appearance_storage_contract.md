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
