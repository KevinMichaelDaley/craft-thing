#ifndef DC_STREAM_PACKET_H
#define DC_STREAM_PACKET_H

#include <math.h>
#include <stdlib.h>

#include "dungeoncraft/stream.h"

typedef enum { JOB_LOAD, JOB_SAVE } job_kind_t;
typedef struct {
    job_kind_t kind;
    dc_chunk_coord_t coord;
    uint64_t generation;
    const char *staging;
    dc_chunk_t *chunk;
    dc_chunk_colors_t *colors;
    fr_reactive_chunk_t *reactive;
    uint8_t *thermal;
    size_t thermal_bytes;
    fr_gas_checkpoint_chunk_t *gas;
    uint8_t *bulk_packet;
    size_t bulk_bytes;
    dc_stream_chunk_time_t time;
    dc_chunk_owner_t replaces_owner;
} stream_job_t;

/** Write an owned job as one immutable, checked chunk file. */
bool dc_stream_write_packet(const char *root, uint64_t seed, const stream_job_t *job);
/** Load one checked packet or generate a missing chunk at the requested coordinate. */
bool dc_stream_read_packet(const char *root, uint64_t seed, const dc_ground_profile_t *ground,
    const char *staging, dc_stream_result_t *result);

static inline bool dc_stream_valid_completed_time(double seconds) {
    return isfinite(seconds) && seconds >= 0.0;
}

static inline void dc_stream_release_channel(fr_reactive_chunk_t *channel) {
    if (!channel) return;
    fr_reactive_chunk_release(channel);
    free(channel);
}

#endif
