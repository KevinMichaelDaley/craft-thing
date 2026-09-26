#include <stddef.h>

#include "dungeoncraft/stream.h"

dc_streamer_t *dc_stream_create(const char *directory, uint64_t seed,
                                uint32_t queue_capacity) {
    (void)directory; (void)seed; (void)queue_capacity; return NULL;
}
bool dc_stream_request_load(dc_streamer_t *stream, dc_chunk_coord_t coord,
                            uint64_t generation) {
    (void)stream; (void)coord; (void)generation; return false;
}
bool dc_stream_request_save(dc_streamer_t *stream, const dc_chunk_t *chunk,
                            uint64_t generation) {
    (void)stream; (void)chunk; (void)generation; return false;
}
bool dc_stream_poll(dc_streamer_t *stream, dc_stream_result_t *result) {
    (void)stream; (void)result; return false;
}
void dc_stream_result_release(dc_stream_result_t *result) { (void)result; }
void dc_stream_destroy(dc_streamer_t *stream) { (void)stream; }
