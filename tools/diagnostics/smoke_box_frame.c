#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include "ferrum/fire/gas_gpu.h"
#include "../../src/app/level.c"

int main(void) {
    setvbuf(stdout,NULL,_IOLBF,0);char err[256]={0},directory[]="build/smoke_stage_probe_XXXXXX";
    if(!mkdtemp(directory)) return 2;
    dc_level_view_t *view=dc_level_view_create(directory,314,err,sizeof(err));
    if(!view||!dc_level_view_setup_smoke_box(view,err,sizeof(err))) {fprintf(stderr,"Smoke setup: %s\n",err);return 1;}
    fr_gas_gpu_clock_t before={0},after={0};bool okay=true;
    for(uint32_t frame=0;okay&&frame<8;frame++) {
        float dt=frame<4?1.f/60.f:.25f;dc_gpu_tick_capture_t capture={0};
        fr_gas_gpu_clock(view->gpu,&before);uint64_t begin=SDL_GetPerformanceCounter();
        okay=dc_gpu_set_tick_seconds(view->gpu,dt)&&dc_gpu_tick_capture(view->gpu,&capture,err,sizeof(err))&&dc_gpu_present_chunks(view->gpu,err,sizeof(err));
        fr_gas_gpu_clock(view->gpu,&after);
        double total=(SDL_GetPerformanceCounter()-begin)*1000./SDL_GetPerformanceFrequency();
        printf("Smoke stage frame%u dt%.6f total%.3f rigid%.3f fluid%.3f mpm%.3f ms; gas substeps%llu\n",frame,dt,total,
            capture.stages[0].gpu_ns/1e6,capture.stages[1].gpu_ns/1e6,capture.stages[2].gpu_ns/1e6,
            (unsigned long long)(after.substeps-before.substeps));
    }
    if(!okay) fprintf(stderr,"Smoke stage failure: %s\n",err);
    dc_level_view_destroy(view,err,sizeof(err));return okay?0:1;
}
