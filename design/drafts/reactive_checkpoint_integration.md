# Deferred reactive structure-checkpoint integration

The user redirected development toward one visibly testable system at a time.
This exact draft is preserved instead of extending persistence across every
owner before delivering heat/wetting, smoke/ventilation and the terrain bonfire.

`reactive_checkpoint_integration.patch` preserves the unlinked checkpoint helper,
its header, pending-channel storage fields and transactional upload guard. It
has not been connected to the DCS format or validated as an integrated restore.
Do not enable it by applying the patch alone. It depends on the detached storage
factory and GPU body-compaction helper on the fire/smoke branch.

The draft prepares BODY and currently resident CELL/PARTICLE entries from
immutable checkpoint packets in detached storage. Pending chunks retain owned
CPU file packets. The proposed install guard allows the physical checkpoint to
be copied before the prepared root is published. Required remaining work includes
format framing/checksums, all failure cleanup, pending-channel lifecycle,
publication ordering, future-version rollback and allocation-failure tests.

The two new tests in `tests/vulkan/reactive_checkpoint_gpu_tests.c` intentionally
remain RED: the existing structure checkpoint drops extracted-body reaction state
and pending negative-coordinate chunk channels. They are available through
`build/reactive_checkpoint_display_tests`, separate from the passing default
battery. Ticket `ctlo-2q4f` stays in progress. The FRW1 archive codec is tested,
but it is not a completed gameplay or structure-checkpoint feature.
