#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "dungeoncraft/gpu.h"
#include "ferrum/fire/gas_gpu.h"

/* Permanent actual-display whole-frame probe. Cold fixture imports are not
 * timed; each measured frame includes events, full mechanics, gas and present. */
int main(int argc,char **argv) {
    uint32_t frames=argc>1?(uint32_t)strtoul(argv[1],NULL,10):120u;
    if(!frames||frames>100000u) return 2;
    char err[256]={0};dc_gpu_t *g=NULL;bool okay=dc_gpu_create_window(&g,640,448,960,540,
        "build/shaders/pattern.comp.spv",err,sizeof(err));
    /* SDL normally queues these as events; a diagnostic must remain stoppable
     * even when investigating a solver that has not returned to event polling. */
    signal(SIGINT,SIG_DFL);signal(SIGTERM,SIG_DFL);
    if(okay) okay=dc_gpu_set_viewport(g,64,64,480,270)&&dc_gpu_set_display_zoom(g,2);
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    if(!chunk) okay=false;
    for(uint32_t y=0;okay&&y<7;y++) for(uint32_t x=0;okay&&x<10;x++) {
        chunk->coord=(dc_chunk_coord_t){x,y};
        okay=dc_gpu_upload_chunk(g,y*10+x,chunk,err,sizeof(err))&&dc_gpu_set_page(g,x,y,y*10+x,err,sizeof(err));
    }
    free(chunk);
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    if(okay) okay=fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err))&&fr_gas_gpu_set_gravity(g,900,err,sizeof(err));
    uint32_t policy=argc>2&&strcmp(argv[2],"implicit")==0?FR_GAS_PRESSURE_IMPLICIT:FR_GAS_PRESSURE_HLLC;
    if(okay) okay=fr_gas_gpu_set_pressure_policy(g,policy,err,sizeof(err));
    fr_gas_state_t *states=calloc(4096,sizeof(*states));double *volumes=malloc(4096*sizeof(*volumes));
    if(!states||!volumes) okay=false;
    if(okay) okay=fr_gas_gpu_read_chunk(g,22,states,4096,err,sizeof(err));
    for(uint32_t y=0;okay&&y<64;y++) for(uint32_t x=0;x<64;x++) {
        uint32_t i=y*64+x;volumes[i]=1;
        if(x>=24&&x<40&&y>=40&&y<56) {
            /* Immutable finite initial packet, no perpetual smoke source. */
            states[i].species[FR_GAS_EXHAUST]=.0004;
            states[i].energy+=.0004*model.heat_capacity_cv[FR_GAS_EXHAUST]*650.;
            states[i].pigment[0]=.0004;states[i].pigment[2]=.0002;
        }
    }
    if(okay) okay=fr_gas_gpu_seed_chunk(g,22,states,volumes,4096,err,sizeof(err));
    free(states);free(volumes);
    double total=0,worst=0,gas_total=0;uint32_t measured=0;
    for(uint32_t i=0;okay&&i<frames+20;i++) {
        uint64_t begin=SDL_GetPerformanceCounter();SDL_Event event;while(SDL_PollEvent(&event))
            if(event.type==SDL_QUIT) {snprintf(err,sizeof(err),"Display closed");okay=false;}
        if(!okay) break;
        okay=dc_gpu_tick_step(g,err,sizeof(err));uint64_t gas_begin=SDL_GetPerformanceCounter();
        if(okay) okay=fr_gas_gpu_step(g,1.f/60.f,err,sizeof(err));
        uint64_t gas_end=SDL_GetPerformanceCounter();
        if(okay) okay=dc_gpu_present_chunks(g,err,sizeof(err));
        double ms=(SDL_GetPerformanceCounter()-begin)*1000./SDL_GetPerformanceFrequency();
        if(i>=20&&okay) {total+=ms;gas_total+=(gas_end-gas_begin)*1000./SDL_GetPerformanceFrequency();if(ms>worst) worst=ms;measured++;}
    }
    fr_gas_gpu_clock_t clock={0};if(g) fr_gas_gpu_clock(g,&clock);
    if(measured) printf("Displayed gas full frame: %u frames mean%.3f worst%.3f gas%.3f ms; %llu substeps\n",
        measured,total/measured,worst,gas_total/measured,(unsigned long long)clock.substeps);
    if(!okay) fprintf(stderr,"Gas frame failed: %s\n",err);
    dc_gpu_destroy(g);return !okay?1:(total/measured>=30.||worst>=30.?3:0);
}
