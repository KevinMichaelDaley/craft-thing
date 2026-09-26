#ifndef DUNGEONCRAFT_STREAM_H
#define DUNGEONCRAFT_STREAM_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/chunk.h"

typedef struct dc_streamer dc_streamer_t;

typedef enum { DC_STREAM_LOADED, DC_STREAM_SAVED, DC_STREAM_FAILED } dc_stream_result_kind_t;

typedef struct {
    dc_stream_result_kind_t kind;
    dc_chunk_coord_t coord;
    uint64_t generation;
    dc_chunk_t *chunk;
} dc_stream_result_t;

/** Start a worker thread for procedural generation and chunk disk I/O. */
dc_streamer_t *dc_stream_create(const char *directory, uint64_t seed,
                                uint32_t queue_capacity);

/** Queue a nonblocking chunk load or generation. */
bool dc_stream_request_load(dc_streamer_t *stream, dc_chunk_coord_t coord,
                            uint64_t generation);

/** Queue a nonblocking save using a private copy of the chunk. */
bool dc_stream_request_save(dc_streamer_t *stream, const dc_chunk_t *chunk,
                            uint64_t generation);

/** Poll one completed result without waiting for disk I/O. */
bool dc_stream_poll(dc_streamer_t *stream, dc_stream_result_t *result);

/** Free a loaded chunk held by a result. */
void dc_stream_result_release(dc_stream_result_t *result);

/** Stop the worker, join it, and release queued work. */
void dc_stream_destroy(dc_streamer_t *stream);

#endif
