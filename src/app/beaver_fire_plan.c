#include "beaver_fire_demo.h"

bool dc_beaver_fire_plan(uint32_t w,uint32_t h,dc_beaver_fire_plan_t *out) {
    if(!out||w<320||h<200||w>8192||h>8192) return false;
    dc_beaver_fire_plan_t p={.width=w,.height=h,.ground=h-32,.player_x=w/6,
        .player_y=h-32-40,.count=DC_BEAVER_FIRE_LOGS,.temperature_K=293.15f};
    uint32_t width=w/4,height=h/27,center=w-12-width/2;
    for(uint32_t n=0;n<p.count;n++) {
        uint32_t log_width=width-n*(width/15);
        p.logs[n]=(dc_beaver_fire_log_t){center-log_width/2,p.ground-(n+1)*height,
            log_width,height,100+n};
    }
    *out=p;return true;
}
