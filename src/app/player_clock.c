#include <math.h>

#include "player_clock.h"

bool dc_player_clock_plan(double a,double t,bool f,dc_player_clock_plan_t *out) {
    if(!out||!isfinite(a)||!isfinite(t)||a<0||t<=0) return false;
    if(f) {*out=(dc_player_clock_plan_t){1,0};return true;}
    const double epsilon=t*1e-9;
    double steps=floor((a+epsilon)/t);
    uint32_t count=steps>2?2u:(uint32_t)steps;
    double remainder=a-steps*t;
    if(remainder<0) remainder=0;
    *out=(dc_player_clock_plan_t){count,remainder};return true;
}
