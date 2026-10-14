# Feature checkpoint and recovery handoff

Branch: `feat/structural-fracture-features`; worktree:
`/home/kmd/craft-thing-physics-features`. No main merge is authorized without
parent coordination and combined tests. All sibling files were read-only.

Verified feature checkpoint `367c60b`:

- Audit `ctpf-753m` closed; explicit capability gaps and shared admission ABI
  agreed with optimizer and renderer.
- Six bounded CPU request-envelope checks pass.
- Four authored GPU staging checks passed with Vulkan validation before the
  device stall; authored shader passes `spirv-val --target-env vulkan1.3`.
- GPU staging does canonical channel validation and exact four-connected
  role/object labels. It does not publish gameplay objects or grant reservations.
- All default/headless binaries rebuild cleanly with `-Wall -Wextra -Werror`;
  CPU-only envelope6 and structural mechanics11 checks pass after the snapshot.
- `ctpf-iueo`, release/fracture `ctpf-9jiw`, persistence `ctpf-5h0n` and combined
  acceptance `ctpf-ti75` remain unfinished. The feature epic is not complete.

## Pending tests: not an observed behavioral RED

Component-plan bbox/tile demand/tentative slot tests and absent-normal default
tests compile. The new component getter is a RED-phase stub, not a completed
feature. Their attempted run timed out45seconds (`exit124`) while the previously
verified staging GPU implementation was unchanged. No acceptance failure was
observed; do not call this timeout a successful RED verification.

Optimizer also saw unchanged hydro fence stalls, and renderer confirmed successful
context/pipeline creation followed by a stall on a96-texel texture submission.
The team stopped further GPU retries pending external device recovery. My timed
out log was file-buffered, so its precise wait was not independently localized.
Full feature headless/UI/quarter/native correctness remains unverified and must
run after recovery. No property-helper or fracture-kernel draft was adopted.

## CPU-only integration repro

`ctpf-b819` captures a real package-reader mismatch: valid152-byte package passes,
but correctly checksummed reserved-word1.0 and negative-zero variants are accepted
by the sibling reader. The standalone reproducible test is
`tests/integration/prop_channel_contract_tests.c`; it is intentionally not added
to the feature default suite while the exporter dependency is unmerged.

Compile against the read-only exporter tree:

```
cc -std=c11 -O2 -Wall -Wextra -Werror \
  -I/home/kmd/craft-thing-prop-export/include \
  tests/integration/prop_channel_contract_tests.c \
  /home/kmd/craft-thing-prop-export/src/assets/prop.c -lm \
  -o /tmp/prop_channel_contract_tests
/tmp/prop_channel_contract_tests
```

Observed result:1pass/2fail. Renderer `3933374` already rejects nonzero reserved
bits in its resolver. The producer owner should integrate these cases and tighten
its canonical reader, rather than physics editing the sibling exporter.

## Resume order

1. Coordinate an exclusive GPU window after recovery. Use line-buffered output
   and finite timeouts; verify the existing staging4checks first, then actually
   observe the new plan/default-normal RED failures before their implementations.
2. Finish parallel root bbox/tile-demand and deterministic tentative slot plans.
   Preserve all-role rollback; a plan is not a published body.
3. Adopt the optimizer's cooperative property helper only after its verified
   checkpoint. Draft `pixel_properties.glsl` is not GPU-verified or released.
4. Implement joint body/shape/graph/renderer admission, replay and publication.
   Renderer owns the channel/provenance/emission allocator and callback adapter.
5. Adapt rectangle-based structural capture/self-weight/failure assumptions before
   publishing exact pixel proxies; arbitrary graph/grain directions require a
   rest-space normal/offset in the common parallel fracture request.
6. Follow release/provenance/persistence tickets and run combined real displays,
   conservation, rollback, snapshots and full regressions before integration.

`ctpf-tojk` and `design/prop_structural_companion_v1.md` define the missing explicit
graph/member-plane producer. Existing role/object/anchor maps alone cannot claim
game-ready structures; `ctpe-847v` and `ctpe-g0sl` integration stays gated on this
producer and actual physics publication.
