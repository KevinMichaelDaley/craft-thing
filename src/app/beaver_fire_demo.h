#ifndef DC_BEAVER_FIRE_DEMO_H
#define DC_BEAVER_FIRE_DEMO_H

#include "level.h"

#define DC_BEAVER_FIRE_LOGS 10u
typedef struct {uint32_t x,y,width,height,object;} dc_beaver_fire_log_t;
typedef struct {
    uint32_t width,height,ground,player_x,player_y,count;
    float temperature_K;
    dc_beaver_fire_log_t logs[DC_BEAVER_FIRE_LOGS];
} dc_beaver_fire_plan_t;

/** Derive one shared scene plan: rightmost massive pile, unlit ambient wood,
 * player feet on the support line, and enough facing ray reach. */
bool dc_beaver_fire_plan(uint32_t width,uint32_t height,dc_beaver_fire_plan_t *out);
/** Configure the authoritative gas field before saved chunk admission. */
bool dc_beaver_fire_configure(dc_level_view_t *view,char *err,uint32_t cap);
/** Cold construct the fresh named demo and enable real gas/combustion/ray input.
 * Restored worlds preserve their existing wood, fuel and ignition histories. */
bool dc_beaver_fire_setup(dc_level_view_t *view,char *err,uint32_t cap);

#endif
