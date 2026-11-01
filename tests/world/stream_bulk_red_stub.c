#include "dungeoncraft/stream.h"
bool dc_stream_request_save_bulk(dc_streamer_t *stream,const dc_chunk_t *chunk,
    const dc_chunk_colors_t *colors,const fr_reactive_chunk_t *reactive,
    const float *water_excess,const fr_gas_checkpoint_chunk_t *gas,
    const fr_bulk_phase_chunk_t *bulk,uint64_t generation) {
    (void)stream;(void)chunk;(void)colors;(void)reactive;(void)water_excess;
    (void)gas;(void)bulk;(void)generation;return false;
}
