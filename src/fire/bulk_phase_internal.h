#ifndef FERRUM_FIRE_BULK_PHASE_INTERNAL_H
#define FERRUM_FIRE_BULK_PHASE_INTERNAL_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

static inline bool bulk_phase_heat_valid(uint32_t oil,uint32_t slime,
    float oil_excess,float slime_excess) {
    return isfinite(oil_excess)&&isfinite(slime_excess)&&
        (oil>0u||oil_excess==0.f)&&(slime>0u||slime_excess==0.f);
}

#endif
