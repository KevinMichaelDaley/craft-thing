#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "ferrum/fire/thermal_gpu.h"

#include "bonfire.h"
bool dc_bonfire_admit(dc_gpu_t *gpu,char *err,uint32_t cap) {
    dc_bonfire_plan_t plan;
    Uint64 started=SDL_GetTicks64();
    if(!dc_bonfire_plan(&plan)||!fr_gpu_place_structure(gpu,&plan.pit,err,cap))
        return false;
    if(getenv("DC_BONFIRE_STARTUP_TRACE"))
        fprintf(stderr,"Bonfire admission structure: %llums\n",
                (unsigned long long)(SDL_GetTicks64()-started));
    if(!dc_gpu_admit_authored_rigid_cells(gpu,err,cap)) return false;
    if(getenv("DC_BONFIRE_STARTUP_TRACE"))
        fprintf(stderr,"Bonfire admission rigid: %llums\n",
                (unsigned long long)(SDL_GetTicks64()-started));
    return fr_thermal_gpu_enable(gpu,err,cap);
}
