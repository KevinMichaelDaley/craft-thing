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
static inline bool bulk_phase_momentum_valid(uint32_t oil,uint32_t slime,
    float oil_x,float oil_y,float slime_x,float slime_y) {
    return isfinite(oil_x)&&isfinite(oil_y)&&isfinite(slime_x)&&isfinite(slime_y)&&
        (oil>0u||(oil_x==0.f&&oil_y==0.f))&&
        (slime>0u||(slime_x==0.f&&slime_y==0.f));
}

#endif
