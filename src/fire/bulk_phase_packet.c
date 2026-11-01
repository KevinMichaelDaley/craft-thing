#include <math.h>
#include <string.h>

#include "ferrum/fire/bulk_phase.h"

enum { BULK_HEADER_BYTES=16u, BULK_HASH_BYTES=8u,
       BULK_LEGACY_PAYLOAD_BYTES=FR_BULK_VOLUME_CHANNELS*DC_CHUNK_CELLS*sizeof(uint32_t),
       BULK_PAYLOAD_BYTES=FR_BULK_CHUNK_CHANNELS*DC_CHUNK_CELLS*sizeof(uint32_t) };

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
static size_t payload_offset(uint32_t channel_index,uint32_t cell_index) {
    return BULK_HEADER_BYTES+((size_t)channel_index*DC_CHUNK_CELLS+cell_index)*sizeof(uint32_t);
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
static uint32_t channel_word(const fr_bulk_phase_chunk_t *phase,uint32_t index,uint32_t cell) {
    if(index<FR_BULK_VOLUME_CHANNELS) return channel(phase,index)[cell];
    float value=index==4u?phase->oil_excess[cell]:phase->slime_excess[cell];
    uint32_t bits=0;memcpy(&bits,&value,sizeof(bits));return bits;
}
static void set_channel_word(fr_bulk_phase_chunk_t *phase,uint32_t index,uint32_t cell,uint32_t bits) {
    if(index<FR_BULK_VOLUME_CHANNELS) {mutable_channel(phase,index)[cell]=bits;return;}
    float value=0.f;memcpy(&value,&bits,sizeof(value));
    if(index==4u) phase->oil_excess[cell]=value;else phase->slime_excess[cell]=value;
}
size_t fr_bulk_phase_chunk_bytes(void) {
    return BULK_HEADER_BYTES+BULK_PAYLOAD_BYTES+BULK_HASH_BYTES;
}
static bool valid_cell(const dc_chunk_t *chunk,uint32_t index,uint32_t oil,uint32_t slime,
                       uint32_t oil_retained,uint32_t slime_retained) {
    return (uint64_t)chunk->cells[index].fluid_mass+oil+slime<=FR_BULK_CELL_FULL_FP&&
        oil_retained<=oil&&slime_retained<=slime;
}
static bool valid_heat(uint32_t oil,uint32_t slime,float oil_excess,float slime_excess) {
    return isfinite(oil_excess)&&isfinite(slime_excess)&&
        (oil>0u||oil_excess==0.f)&&(slime>0u||slime_excess==0.f);
}
bool fr_bulk_phase_chunk_encode(const dc_chunk_t *chunk,
    const fr_bulk_phase_chunk_t *phase,uint8_t *bytes,size_t capacity) {
    size_t length=fr_bulk_phase_chunk_bytes();
    if(!chunk||!phase||!bytes||capacity<length) return false;
    for(uint32_t i=0;i<DC_CHUNK_CELLS;i++)
        if(!valid_cell(chunk,i,phase->oil_fp[i],phase->slime_fp[i],
            phase->oil_retained_fp[i],phase->slime_retained_fp[i])||
            !valid_heat(phase->oil_fp[i],phase->slime_fp[i],
                phase->oil_excess[i],phase->slime_excess[i])) return false;
    memcpy(bytes,"BLP2",4);put32(bytes+4,2u);put32(bytes+8,DC_CHUNK_CELLS);put32(bytes+12,0u);
    for(uint32_t k=0;k<FR_BULK_CHUNK_CHANNELS;k++) for(uint32_t i=0;i<DC_CHUNK_CELLS;i++)
        put32(bytes+payload_offset(k,i),channel_word(phase,k,i));
    uint64_t hash=packet_hash(bytes,length-BULK_HASH_BYTES);
    for(uint32_t i=0;i<BULK_HASH_BYTES;i++) bytes[length-BULK_HASH_BYTES+i]=(uint8_t)(hash>>(i*8u));
    return true;
}
bool fr_bulk_phase_chunk_decode(const dc_chunk_t *chunk,const uint8_t *bytes,
    size_t length,fr_bulk_phase_chunk_t *out) {
    if(!chunk||!bytes||!out||length<BULK_HEADER_BYTES+BULK_HASH_BYTES) return false;
    bool legacy=length==BULK_HEADER_BYTES+BULK_LEGACY_PAYLOAD_BYTES+BULK_HASH_BYTES&&
        memcmp(bytes,"BLP1",4)==0&&get32(bytes+4)==1u;
    bool current=length==fr_bulk_phase_chunk_bytes()&&
        memcmp(bytes,"BLP2",4)==0&&get32(bytes+4)==2u;
    if((!legacy&&!current)||
       get32(bytes+8)!=DC_CHUNK_CELLS||get32(bytes+12)!=0u) return false;
    uint64_t expected=0;
    for(uint32_t i=0;i<BULK_HASH_BYTES;i++) expected|=(uint64_t)bytes[length-BULK_HASH_BYTES+i]<<(i*8u);
    if(expected!=packet_hash(bytes,length-BULK_HASH_BYTES)) return false;
    for(uint32_t i=0;i<DC_CHUNK_CELLS;i++) {
        uint32_t oil=get32(bytes+payload_offset(0u,i));
        uint32_t slime=get32(bytes+payload_offset(1u,i));
        uint32_t oil_retained=get32(bytes+payload_offset(2u,i));
        uint32_t slime_retained=get32(bytes+payload_offset(3u,i));
        if(!valid_cell(chunk,i,oil,slime,oil_retained,slime_retained)) return false;
        if(current) {
            uint32_t oil_bits=get32(bytes+payload_offset(4u,i));
            uint32_t slime_bits=get32(bytes+payload_offset(5u,i));
            float oil_heat=0.f,slime_heat=0.f;
            memcpy(&oil_heat,&oil_bits,sizeof(oil_heat));
            memcpy(&slime_heat,&slime_bits,sizeof(slime_heat));
            if(!valid_heat(oil,slime,oil_heat,slime_heat)) return false;
        }
    }
    for(uint32_t k=0;k<(current?FR_BULK_CHUNK_CHANNELS:FR_BULK_VOLUME_CHANNELS);k++)
        for(uint32_t i=0;i<DC_CHUNK_CELLS;i++)
        set_channel_word(out,k,i,get32(bytes+payload_offset(k,i)));
    if(legacy) {memset(out->oil_excess,0,sizeof(out->oil_excess));
        memset(out->slime_excess,0,sizeof(out->slime_excess));}
    return true;
}
