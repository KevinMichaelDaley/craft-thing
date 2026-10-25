#ifndef FR_REACTIVE_CHECKPOINT_GPU_H
#define FR_REACTIVE_CHECKPOINT_GPU_H
#include "ferrum/fire/reactive_archive.h"
#include "structural_store_internal.h"
struct fr_reactive_gpu;
/** Collect immutable body and resident/pending chunk channels at a cold boundary. */
bool fr_gpu_reactive_checkpoint_collect(dc_gpu_t *gpu,const dc_chunk_t *chunks,
    uint32_t count,fr_reactive_archive_t *out,char *err,uint32_t cap);
/** Validate immutable checkpoint bindings and preflight detached GPU state before
 * the live physical checkpoint is changed. Failure preserves its entire source. */
bool fr_gpu_reactive_checkpoint_prepare(dc_gpu_t *gpu,const fr_reactive_archive_t *archive,
    const dc_chunk_t *chunks,const dc_structural_store_payload_t *payload,const uint32_t *pixels,uint32_t pixel_words,
    struct fr_reactive_gpu **out,uint32_t ***pending_handles,char *err,uint32_t cap);
/** Publish already prepared storage after the corresponding physical install. */
void fr_gpu_reactive_checkpoint_publish(dc_gpu_t *gpu,struct fr_reactive_gpu *prepared);
/** Release pending immutable chunk channels when a checkpoint is replaced. */
void fr_gpu_reactive_checkpoint_clear_pending(dc_gpu_t *gpu);
/** Preflight a pending physical packet before replacement and bind its already
 * allocated parcels afterward. Only immutable file packet metadata is used. */
bool fr_gpu_reactive_checkpoint_preflight_chunk(dc_gpu_t *gpu,const dc_chunk_t *chunk,
    uint32_t index,char *err,uint32_t cap);
bool fr_gpu_reactive_checkpoint_bind_chunk(dc_gpu_t *gpu,uint32_t slot,uint32_t index,
    char *err,uint32_t cap);
/** Release one consumed packet while the caller removes its physical chunk. */
void fr_gpu_reactive_checkpoint_remove_pending(dc_gpu_t *gpu,uint32_t index,uint32_t last);
#endif
