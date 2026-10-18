#include <stdio.h>
#include <stdlib.h>

#include "dungeoncraft/gpu.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_vacated_terrain_grain_does_not_leave_a_rebonding_tag(void) {
    dc_gpu_t *g=NULL;char err[256];dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    unsigned source=20*64+30;
    chunk->cells[source]=(dc_cell_t){.material=DC_MATERIAL_DIRT,.flags=DC_CELL_LOADED_TERRAIN};
    dc_chunk_particle_init(chunk->particles+source,chunk->coord,source,DC_MATERIAL_DIRT);
    chunk->particles[source].vy_fp=65536;chunk->particle_count=1;
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    for(unsigned tick=0;tick<12;tick++) ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_EQ(chunk->cells[source].material,DC_MATERIAL_AIR);
    ASSERT_EQ(chunk->cells[source].flags,0u);
    uint64_t mass=0;
    for(unsigned i=0;i<DC_CHUNK_CELLS;i++) {
        if(chunk->cells[i].material==DC_MATERIAL_DIRT) ASSERT_EQ(chunk->cells[i].flags,0u);
        mass+=chunk->particles[i].mass_fp+chunk->particles[i+4096].mass_fp;
    }
    ASSERT_EQ(mass,78643u);dc_gpu_destroy(g);free(chunk);PASS();
}

static void test_loose_grain_cannot_bridge_terrain_across_a_chunk_seam(void) {
    dc_gpu_t *g=NULL;char err[256];dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_create(&g,128,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    chunk->cells[30*64+63]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    chunk->cells[31*64+63]=chunk->cells[30*64+63];
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    *chunk=(dc_chunk_t){.coord={1,0}};
    chunk->cells[30*64].material=DC_MATERIAL_DIRT;
    for(unsigned y=30;y<64;y++)
        chunk->cells[y*64+1]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    ASSERT_TRUE(dc_gpu_upload_chunk(g,1,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,1,0,1,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,63,31,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_pixel_t pixel;unsigned count=0;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,&pixel,1,&count,err,sizeof(err)));
    ASSERT_EQ(count,1u);ASSERT_EQ(pixel.material,DC_GPU_BODY_WOOD);
    ASSERT_TRUE(dc_gpu_download_chunk(g,1,chunk,err,sizeof(err)));
    ASSERT_EQ(chunk->cells[30*64].material,DC_MATERIAL_DIRT);
    ASSERT_EQ(chunk->cells[30*64].flags,0u);
    ASSERT_EQ(chunk->particles[30*64].mass_fp,78643u);
    dc_gpu_destroy(g);free(chunk);PASS();
}

int main(void) {
    RUN(test_vacated_terrain_grain_does_not_leave_a_rebonding_tag);
    RUN(test_loose_grain_cannot_bridge_terrain_across_a_chunk_seam);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
