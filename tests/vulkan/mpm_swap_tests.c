#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/vulkan/gpu_internal.h"
#include "dungeoncraft/leaf_material.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static int marker_order(const void *a,const void *b) {return memcmp(a,b,sizeof(dc_marker_t));}

static void test_swapping_preserves_authoritative_state_across_edits_and_reload(void) {
    char err[256]={0};dc_gpu_t *grids[2]={0};
    dc_chunk_t *initial=calloc(1,sizeof(*initial)),*states[2]={calloc(1,sizeof(**states)),calloc(1,sizeof(**states))};
    ASSERT_TRUE(initial&&states[0]&&states[1]);
    for(unsigned y=20;y<28;y++) for(unsigned x=20;x<44;x++)
        initial->cells[y*64+x].material=y%3u==0u?DC_MATERIAL_LEAF_DEBRIS:
            x%2u?DC_MATERIAL_DIRT:DC_MATERIAL_SAND;
    for(unsigned y=8;y<48;y++) for(unsigned x=1;x<63;x++) initial->cells[y*64+x].fluid_mass=DC_FLUID_FULL;
    for(unsigned x=0;x<64;x++) initial->cells[58*64+x].material=DC_MATERIAL_STONE;
    dc_chunk_seed_particles(initial);
    for(unsigned p=0;p<DC_MPM_PARTICLES_PER_CHUNK;p++)
        if(initial->particles[p].mass_fp&&initial->particles[p].material==DC_MATERIAL_LEAF_DEBRIS)
            initial->particles[p].flags=DC_LEAF_COLOR_FLAG|0x11aa55u;
    for(unsigned variant=0;variant<2;variant++) {
        ASSERT_EQ(setenv("DC_MPM_PARTICLE_SWAP",variant?"1":"0",1),0);
        ASSERT_TRUE(dc_gpu_create(&grids[variant],64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_upload_chunk(grids[variant],0,initial,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(grids[variant],0,0,0,err,sizeof(err)));
    }
    ASSERT_EQ(unsetenv("DC_MPM_PARTICLE_SWAP"),0);
    ASSERT_TRUE(grids[1]->mpm_swap_commit_pipeline!=VK_NULL_HANDLE);
    ASSERT_TRUE(grids[1]->mpm_swapped_descriptor&&grids[1]->mpm_swapped_descriptor!=grids[1]->descriptor);
    for(unsigned frame=0;frame<15;frame++) {
        for(unsigned variant=0;variant<2;variant++) {
            dc_gpu_t *g=grids[variant];
            if(frame==3) ASSERT_TRUE(dc_gpu_paint_material(g,32,24,20,DC_MATERIAL_AIR,err,sizeof(err)));
            if(frame==5) ASSERT_TRUE(dc_gpu_paint_material(g,16,20,2,DC_MATERIAL_DIRT,err,sizeof(err)));
            if(frame==7) {
                dc_chunk_t *empty=calloc(1,sizeof(*empty));ASSERT_TRUE(empty);
                ASSERT_TRUE(dc_gpu_upload_chunk(g,0,empty,err,sizeof(err)));free(empty);
            }
            if(frame==9) ASSERT_TRUE(dc_gpu_upload_chunk(g,0,initial,err,sizeof(err)));
            dc_gpu_tick_capture_t capture;
            ASSERT_TRUE(dc_gpu_tick_capture(g,&capture,err,sizeof(err)));
            ASSERT_TRUE(dc_gpu_download_chunk(g,0,states[variant],err,sizeof(err)));
            qsort(states[variant]->markers,states[variant]->marker_count,sizeof(dc_marker_t),marker_order);
        }
        bool equal=memcmp(states[0],states[1],sizeof(*states[0]))==0;
        if(!equal) {
            const unsigned char *a=(const unsigned char *)states[0],*b=(const unsigned char *)states[1];
            for(size_t at=0;at<sizeof(*states[0]);at++) if(a[at]!=b[at]) {
                printf("frame%u first state mismatch byte%zu (particles at%zu): %02x != %02x\n",
                    frame,at,offsetof(dc_chunk_t,particles),a[at],b[at]);break;
            }
        }
        printf("frame%u exact%d particles%u\n",frame,equal,states[1]->particle_count);
        ASSERT_TRUE(equal);
        if(frame==3||frame==7||frame==8) ASSERT_EQ(states[1]->particle_count,0u);
    }
    for(unsigned variant=0;variant<2;variant++) {dc_gpu_destroy(grids[variant]);free(states[variant]);}
    free(initial);PASS();
}
int main(void) {
    RUN(test_swapping_preserves_authoritative_state_across_edits_and_reload);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
