#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "dungeoncraft/gpu.h"
#include "ferrum/fire/bulk_phase_gpu.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/reactive_state.h"
#include "ferrum/fire/thermal_gpu.h"

enum { BENCH_WIDTH=480, BENCH_HEIGHT=270, BENCH_FILMS=64,
       BENCH_WARMUP=8, BENCH_SAMPLES=20 };

static double seconds_now(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC,&now);
    return (double)now.tv_sec+(double)now.tv_nsec*1e-9;
}

static bool seed_stones(dc_gpu_t *gpu,bool coated,char *err,uint32_t cap) {
    for(uint32_t i=0u;i<BENCH_FILMS;i++) {
        uint32_t x=4u+(i%8u)*7u,y=4u+(i/8u)*7u;
        fr_reactive_state_t state;
        if(!fr_reactive_state_default(DC_MATERIAL_STONE,65536u,0u,300.f,
            1000u+i,&state,err,cap)) return false;
        if(coated) {
            state.coating_oil_fuel_fp=55000u;
            if(!fr_reactive_state_add_heat(&state,
                fr_reactive_state_energy_at_temperature(&state,500.f)-
                fr_reactive_state_energy(&state),err,cap)) return false;
        }
        if(!fr_gpu_reactive_seed_cell(gpu,0,y*64u+x,&state,err,cap)) return false;
    }
    return true;
}

static double measure_steps(dc_gpu_t *gpu,char *err,uint32_t cap) {
    for(uint32_t i=0u;i<BENCH_WARMUP;i++)
        if(!fr_thermal_gpu_step(gpu,1.f/60.f,err,cap)) return -1.;
    double start=seconds_now();
    for(uint32_t i=0u;i<BENCH_SAMPLES;i++)
        if(!fr_thermal_gpu_step(gpu,1.f/60.f,err,cap)) return -1.;
    return (seconds_now()-start)*1000./BENCH_SAMPLES;
}

int main(void) {
    char err[256]={0};dc_gpu_t *gpu=NULL;
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_bulk_phase_chunk_t *empty=calloc(1,sizeof(*empty));
    fr_bulk_phase_chunk_t *out=calloc(1,sizeof(*out));
    if(!chunk||!empty||!out) goto fail;
    for(uint32_t i=0u;i<BENCH_FILMS;i++) {
        uint32_t x=4u+(i%8u)*7u,y=4u+(i/8u)*7u;
        chunk->cells[y*64u+x].material=DC_MATERIAL_STONE;
    }
    if(!dc_gpu_create(&gpu,BENCH_WIDTH,BENCH_HEIGHT,
           "build/shaders/pattern.comp.spv",err,sizeof(err))||
       !dc_gpu_upload_chunk(gpu,0,chunk,err,sizeof(err))||
       !dc_gpu_set_page(gpu,0,0,0,err,sizeof(err))||
       !seed_stones(gpu,false,err,sizeof(err))||
       !fr_bulk_phase_gpu_import_chunk(gpu,0,chunk,empty,err,sizeof(err))||
       !fr_thermal_gpu_enable(gpu,err,sizeof(err))) goto fail;
    double first_control=measure_steps(gpu,err,sizeof(err));
    if(first_control<0.||!seed_stones(gpu,true,err,sizeof(err))) goto fail;
    double first_coated=measure_steps(gpu,err,sizeof(err));
    if(first_coated<0.||!seed_stones(gpu,false,err,sizeof(err))||
       !fr_bulk_phase_gpu_import_chunk(gpu,0,chunk,empty,err,sizeof(err))) goto fail;
    double baseline=measure_steps(gpu,err,sizeof(err));
    if(baseline<0.||!seed_stones(gpu,true,err,sizeof(err))) goto fail;
    double coated=measure_steps(gpu,err,sizeof(err));
    if(coated<0.||!fr_bulk_phase_gpu_export_chunk(gpu,0,out,err,sizeof(err))) goto fail;
    uint64_t released=0u;
    for(uint32_t i=0u;i<DC_CHUNK_CELLS;i++) released+=out->oil_fp[i];
    printf("quarter_native=%ux%u resident_stones=%u samples=%u "
        "first_control_ms=%.3f first_coated_ms=%.3f "
        "baseline_ms=%.3f coated_ms=%.3f overhead_ms=%.3f "
        "released_oil_q16=%" PRIu64 "\n",BENCH_WIDTH,BENCH_HEIGHT,
        BENCH_FILMS,BENCH_SAMPLES,first_control,first_coated,
        baseline,coated,coated-baseline,released);
    dc_gpu_destroy(gpu);free(out);free(empty);free(chunk);
    return released>0u?0:1;
fail:
    fprintf(stderr,"coating native benchmark failed: %s\n",err);
    dc_gpu_destroy(gpu);free(out);free(empty);free(chunk);
    return 1;
}
