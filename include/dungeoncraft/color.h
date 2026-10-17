#ifndef DUNGEONCRAFT_COLOR_H
#define DUNGEONCRAFT_COLOR_H
#include <math.h>
#include <string.h>
#include "dungeoncraft/chunk.h"

/** Linear RGB and an exact zero/one validity word, independent of material. */
typedef struct {
    float terrain[DC_CHUNK_CELLS][4];
    float water[DC_CHUNK_CELLS][4];
} dc_chunk_colors_t;

/** Reject malformed color records before publishing saved GPU state. */
static inline bool dc_color_records_valid(const float *records,size_t count) {
    for(size_t i=0;i<count;i++) {
        uint32_t valid;memcpy(&valid,records+i*4+3,4);
        if(valid!=0 && valid!=0x3f800000u) return false;
        for(unsigned c=0;c<3;c++)
            if(!isfinite(records[i*4+c]) || records[i*4+c]<0) return false;
    }
    return true;
}
/** Hash serialized color bytes without altering floating-point values. */
static inline uint64_t dc_color_hash(uint64_t hash,const void *bytes,size_t size) {
    const uint8_t *p=bytes;
    for(size_t i=0;i<size;i++) hash=(hash^p[i])*1099511628211ull;
    return hash;
}
#endif
