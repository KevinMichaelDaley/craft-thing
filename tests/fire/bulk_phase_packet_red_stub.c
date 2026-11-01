#include "ferrum/fire/bulk_phase.h"
size_t fr_bulk_phase_chunk_bytes(void) {return sizeof(fr_bulk_phase_chunk_t)+16u;}
bool fr_bulk_phase_chunk_encode(const dc_chunk_t *chunk,
    const fr_bulk_phase_chunk_t *phase,uint8_t *bytes,size_t capacity) {
    (void)chunk;(void)phase;(void)bytes;(void)capacity;return false;
}
bool fr_bulk_phase_chunk_decode(const dc_chunk_t *chunk,const uint8_t *bytes,
    size_t length,fr_bulk_phase_chunk_t *out) {
    (void)chunk;(void)bytes;(void)length;(void)out;return false;
}
