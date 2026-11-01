#include <string.h>

#include "ferrum/fire/bulk_phase.h"

enum { BULK_HEADER_BYTES=16u, BULK_HASH_BYTES=8u,
       BULK_PAYLOAD_BYTES=4u*DC_CHUNK_CELLS*sizeof(uint32_t) };

static void put32(uint8_t *bytes,uint32_t value) {
    for(uint32_t i=0;i<4u;i++) bytes[i]=(uint8_t)(value>>(i*8u));
}
static uint32_t get32(const uint8_t *bytes) {
    uint32_t value=0;
    for(uint32_t i=0;i<4u;i++) value|=(uint32_t)bytes[i]<<(i*8u);
    return value;
}
static uint64_t packet_hash(const uint8_t *bytes,size_t count) {
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=0;i<count;i++) {hash^=bytes[i];hash*=UINT64_C(1099511628211);}
    return hash;
}
static const uint32_t *channel(const fr_bulk_phase_chunk_t *phase,uint32_t index) {
    switch(index) {
    case 0: return phase->oil_fp;
    case 1: return phase->slime_fp;
    case 2: return phase->oil_retained_fp;
    default: return phase->slime_retained_fp;
    }
}
static uint32_t *mutable_channel(fr_bulk_phase_chunk_t *phase,uint32_t index) {
    switch(index) {
    case 0: return phase->oil_fp;
    case 1: return phase->slime_fp;
    case 2: return phase->oil_retained_fp;
    default: return phase->slime_retained_fp;
    }
}
size_t fr_bulk_phase_chunk_bytes(void) {
    return BULK_HEADER_BYTES+BULK_PAYLOAD_BYTES+BULK_HASH_BYTES;
}
static bool valid_cell(const dc_chunk_t *chunk,uint32_t index,uint32_t oil,uint32_t slime) {
    return (uint64_t)chunk->cells[index].fluid_mass+oil+slime<=FR_BULK_CELL_FULL_FP;
}
bool fr_bulk_phase_chunk_encode(const dc_chunk_t *chunk,
    const fr_bulk_phase_chunk_t *phase,uint8_t *bytes,size_t capacity) {
    size_t length=fr_bulk_phase_chunk_bytes();
    if(!chunk||!phase||!bytes||capacity<length) return false;
    for(uint32_t i=0;i<DC_CHUNK_CELLS;i++)
        if(!valid_cell(chunk,i,phase->oil_fp[i],phase->slime_fp[i])) return false;
    memcpy(bytes,"BLP1",4);put32(bytes+4,1u);put32(bytes+8,DC_CHUNK_CELLS);put32(bytes+12,0u);
    size_t offset=BULK_HEADER_BYTES;
    for(uint32_t k=0;k<4u;k++) for(uint32_t i=0;i<DC_CHUNK_CELLS;i++,offset+=4u)
        put32(bytes+offset,channel(phase,k)[i]);
    uint64_t hash=packet_hash(bytes,length-BULK_HASH_BYTES);
    for(uint32_t i=0;i<BULK_HASH_BYTES;i++) bytes[offset+i]=(uint8_t)(hash>>(i*8u));
    return true;
}
bool fr_bulk_phase_chunk_decode(const dc_chunk_t *chunk,const uint8_t *bytes,
    size_t length,fr_bulk_phase_chunk_t *out) {
    if(!chunk||!bytes||!out||length!=fr_bulk_phase_chunk_bytes()||
       memcmp(bytes,"BLP1",4)!=0||get32(bytes+4)!=1u||
       get32(bytes+8)!=DC_CHUNK_CELLS||get32(bytes+12)!=0u) return false;
    uint64_t expected=0;
    for(uint32_t i=0;i<BULK_HASH_BYTES;i++) expected|=(uint64_t)bytes[length-BULK_HASH_BYTES+i]<<(i*8u);
    if(expected!=packet_hash(bytes,length-BULK_HASH_BYTES)) return false;
    for(uint32_t i=0;i<DC_CHUNK_CELLS;i++) {
        uint32_t oil=get32(bytes+BULK_HEADER_BYTES+i*4u);
        uint32_t slime=get32(bytes+BULK_HEADER_BYTES+(DC_CHUNK_CELLS+i)*4u);
        if(!valid_cell(chunk,i,oil,slime)) return false;
    }
    size_t offset=BULK_HEADER_BYTES;
    for(uint32_t k=0;k<4u;k++) for(uint32_t i=0;i<DC_CHUNK_CELLS;i++,offset+=4u)
        mutable_channel(out,k)[i]=get32(bytes+offset);
    return true;
}
