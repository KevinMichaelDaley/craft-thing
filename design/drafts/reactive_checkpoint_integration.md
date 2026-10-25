# Preserved reactive structure-checkpoint integration draft

## Heat-bench integration status

The original patch below is historical and must not be reapplied over the live
implementation. The interactive heat bench exposed a real persistence need:
quitting/reopening must retain its finite probe, parcel state and moving water
enthalpy. The live DCS8 extension now frames the tested FRW1 and THR1 codecs
under the outer snapshot checksum. It validates and allocates detached reaction
storage, liquid storage and pending chunk bindings before publishing either
root. All pending file parcels are allocated up front; streaming binds prepared
handles through the parallel kernel rather than allocating runtime identities.

Actual Wayland transaction coverage passes exact parcel/liquid/source restore
into a different slot, future-version rejection, and fourteen injected address
allocation failures that leave the newer live state untouched. Extracted-body,
pending-chunk and frontend regression verification is still underway at this
checkpoint. Broader owner admission and all of `ctlo-2q4f` remain incomplete.

## Historical safe-boundary record

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
battery. Ticket `ctlo-2q4f` remains open. The FRW1 archive codec is tested,
but it is not a completed gameplay or structure-checkpoint feature.

The visible heat/wetting bench now has displayed interaction and repeated
sub-30-ms complete-frame evidence. Its exposed save/reload requirement makes
the checkpoint subset necessary next, including liquid enthalpy and the finite
probe reservoir. Audit/reuse this draft within that module; it still does not
authorize claiming all future owner/material admission or this broad ticket
complete. The preserved patch remains unlinked at this checkpoint.
