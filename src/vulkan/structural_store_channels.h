#ifndef DC_STRUCTURAL_STORE_CHANNELS_H
#define DC_STRUCTURAL_STORE_CHANNELS_H
#include "reactive_checkpoint_gpu.h"
#include "thermal_gpu_internal.h"
#include "revolute_gpu_internal.h"
#include "structural_store_fluids.h"
enum {
    DC_STRUCTURE_TIME_VERSION = 10,
    DC_STRUCTURE_FLUID_VERSION = 11,
    DC_STRUCTURE_OWNER_VERSION = 12
};
/** Cold checkpoint extension transaction; every allocation precedes publish. */
typedef struct {
    uint8_t *reactive_bytes,*thermal_bytes,*joint_bytes,*time_bytes;
    size_t reactive_length,thermal_length,joint_length,time_length;
    uint8_t *fluid_bytes;
    size_t fluid_length;
    uint32_t fluid_count;
    dc_checkpoint_fluid_t *fluids;
    uint8_t *owner_bytes;
    size_t owner_length;
    dc_chunk_owner_t *owners;
    uint32_t version;
    dc_chunk_time_t *chunk_times;
    fr_reactive_archive_t reactive;
    fr_thermal_checkpoint_t thermal;
    struct fr_reactive_gpu *prepared_reactive;
    struct fr_thermal_gpu *prepared_thermal;
    struct fr_revolute_gpu *prepared_joints;
    uint32_t **prepared_handles;
} dc_checkpoint_channels_t;
/** Collect and frame immutable logical channels without changing the live world. */
bool dc_checkpoint_channels_collect(dc_gpu_t *gpu,const dc_chunk_t *chunks,const uint32_t *slots,uint32_t count,
    dc_checkpoint_channels_t *out,char *err,uint32_t cap);
/** Add an optional clock record for each chunk in snapshot order. */
bool dc_checkpoint_channels_set_times(dc_checkpoint_channels_t *channels,
                                      const dc_chunk_time_t *times,uint32_t count);
/** Encode gas and bulk from the same physical owners as the snapshot. */
bool dc_checkpoint_channels_set_fluids(dc_gpu_t *gpu, dc_checkpoint_channels_t *channels,
    const dc_chunk_t *chunks, const uint32_t *slots, uint32_t count, const bool *gas_admitted,
    char *err, uint32_t cap);
/** Decode checked clock bytes against the number of embedded physical chunks. */
bool dc_checkpoint_channels_decode_times(dc_checkpoint_channels_t *channels,uint32_t count);
/** Preserve pending owner IDs and issue new IDs for captured level slots. */
bool dc_checkpoint_channels_set_owners(const dc_gpu_t *gpu, dc_checkpoint_channels_t *channels,
    const uint32_t *slots, uint32_t count, bool level_save);
/** Validate the owner table against the ordered physical records. */
bool dc_checkpoint_channels_decode_owners(dc_checkpoint_channels_t *channels, uint32_t count);
/** Extend the outer snapshot checksum with canonical lengths and channel bytes. */
uint64_t dc_checkpoint_channels_hash(uint64_t hash,const dc_checkpoint_channels_t *channels);
/** Hash the structure record and its ordered channel payloads. */
uint64_t dc_gpu_structure_checksum(dc_structural_store_header_t header,
    const dc_structural_store_payload_t *payload, const uint32_t *pixels,
    const dc_chunk_t *chunks, const float *body_colors, const dc_chunk_colors_t *colors,
    const dc_checkpoint_channels_t *channels);
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
