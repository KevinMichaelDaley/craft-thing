#ifndef DC_STRUCTURAL_STORE_CHANNELS_H
#define DC_STRUCTURAL_STORE_CHANNELS_H
#include "reactive_checkpoint_gpu.h"
#include "thermal_gpu_internal.h"
#include "revolute_gpu_internal.h"
/** Cold checkpoint extension transaction; every allocation precedes publish. */
typedef struct {
    uint8_t *reactive_bytes,*thermal_bytes,*joint_bytes;
    size_t reactive_length,thermal_length,joint_length;
    uint32_t version;
    fr_reactive_archive_t reactive;
    fr_thermal_checkpoint_t thermal;
    struct fr_reactive_gpu *prepared_reactive;
    struct fr_thermal_gpu *prepared_thermal;
    struct fr_revolute_gpu *prepared_joints;
    uint32_t **prepared_handles;
} dc_checkpoint_channels_t;
/** Collect and frame immutable logical channels without changing the live world. */
bool dc_checkpoint_channels_collect(dc_gpu_t *gpu,const dc_chunk_t *chunks,uint32_t count,
    dc_checkpoint_channels_t *out,char *err,uint32_t cap);
/** Extend the outer snapshot checksum with canonical lengths and channel bytes. */
uint64_t dc_checkpoint_channels_hash(uint64_t hash,const dc_checkpoint_channels_t *channels);
/** Write/read exact framed extension bytes; read owns temporary buffers only. */
bool dc_checkpoint_channels_write(FILE *file,const dc_checkpoint_channels_t *channels);
bool dc_checkpoint_channels_read(FILE *file,uint32_t version,dc_checkpoint_channels_t *channels);
/** Decode inventories, validate bindings and prepare all GPU resources. */
bool dc_checkpoint_channels_prepare(dc_gpu_t *gpu,dc_checkpoint_channels_t *channels,
    const dc_chunk_t *chunks,uint32_t count,const dc_structural_store_payload_t *payload,
    const uint32_t *pixels,uint32_t pixel_words,char *err,uint32_t cap);
/** Publish prepared channels with their already validated physical packets. */
void dc_checkpoint_channels_publish(dc_gpu_t *gpu,dc_checkpoint_channels_t *channels,
    dc_chunk_t *chunks,dc_chunk_colors_t *colors,uint32_t count);
/** Destroy uncommitted resources and temporary bytes, never live resources. */
void dc_checkpoint_channels_discard(dc_gpu_t *gpu,dc_checkpoint_channels_t *channels);
#endif
