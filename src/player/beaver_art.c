#include <string.h>

#include "beaver_internal.h"
#include "beaver_imported.h"

bool beaver_art_source(uint32_t source,beaver_art_pixel_t *out) {
    if (!source || source>FR_BEAVER_PIXELS || !out || !beaver_regions[0][source-1]) return false;
    *out=(beaver_art_pixel_t){.region=beaver_regions[0][source-1]};
    return true;
}

bool beaver_art_frame(fr_player_action_t action,uint32_t frame,uint32_t pixel,beaver_art_pixel_t *out,uint32_t *source) {
    if (frame>=8 || pixel>=(action==FR_PLAYER_CAST?FR_BEAVER_MAX_PIXELS:FR_BEAVER_PIXELS) || !out || !source) return false;
    uint32_t rest=action==FR_PLAYER_CAST?beaver_cast_sources[frame][pixel]:beaver_sources[frame][pixel];
    if (!rest || !beaver_art_source(rest,out)) return false;
    memcpy(out->color,(action==FR_PLAYER_CAST?beaver_cast_rgba[frame]:beaver_rgba[frame])+pixel*4,4);
    *source=rest;
    return true;
}

void beaver_art_pose(const beaver_art_pixel_t *pixel,fr_player_action_t action,
    uint32_t frame,int32_t *x,int32_t *y) {
    if (action==FR_PLAYER_WALK) {
        static const int32_t stride[6]={-2,0,2,2,0,-2};
        static const int32_t lift[6]={0,-1,-1,0,1,1};
        if (pixel->region==FR_BEAVER_FUR && *y>=32) {
            *x+=*x<25?stride[frame]:-stride[frame];
        } else if (pixel->region==FR_BEAVER_HAT || *y<24) *y+=lift[frame];
        else if (pixel->region==FR_BEAVER_TAIL) *y+=stride[frame]/2;
        else *x+=stride[frame]/2;
    }
}
