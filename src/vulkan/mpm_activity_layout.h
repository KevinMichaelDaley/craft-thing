#ifndef DC_VULKAN_MPM_ACTIVITY_LAYOUT_H
#define DC_VULKAN_MPM_ACTIVITY_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/mpm_activity_layout.h"

typedef struct {
    uint32_t tiles_x,tiles_y,tiles,groups,copy_word,count_word,levels;
    uint32_t level_counts[DC_MPM_MAX_SCAN_LEVELS];
    uint32_t level_words[DC_MPM_MAX_SCAN_LEVELS];
    uint32_t root_word,words;
} dc_gpu_mpm_scan_layout_t;

static inline bool dc_gpu_mpm_scan_layout(uint32_t width,uint32_t height,
                                         dc_gpu_mpm_scan_layout_t *layout) {
    if(!width||!height||!layout) return false;
    uint64_t x=((uint64_t)width+15u)/16u,y=((uint64_t)height+15u)/16u;
    uint64_t tiles=x*y,words=7u+2u*tiles;
    if(words>UINT32_MAX) return false;
    *layout=(dc_gpu_mpm_scan_layout_t){.tiles_x=(uint32_t)x,.tiles_y=(uint32_t)y,
        .tiles=(uint32_t)tiles,.words=words<39u?39u:(uint32_t)words};
    return true;
}

#endif
