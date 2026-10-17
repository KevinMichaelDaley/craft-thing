#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../src/vulkan/gpu_internal.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_partial_granular_support_motion_does_not_release_tree_but_cut_does(void) {
    dc_gpu_t *g=NULL;char err[256];dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    chunk->cells[29*64+30]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    chunk->cells[30*64+30]=chunk->cells[29*64+30];
    chunk->cells[29*64+29].material=DC_MATERIAL_DIRT;
    chunk->cells[30*64+29].material=DC_MATERIAL_DIRT;
    chunk->cells[31*64+30].material=DC_MATERIAL_DIRT;
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    unsigned removed=30*64+29;
    chunk->cells[removed]=(dc_cell_t){0};chunk->particles[removed]=(dc_mpm_particle_t){0};chunk->particle_count--;
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_t body;
    ASSERT_TRUE(!dc_gpu_read_body_id(g,0x80000001u,&body,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,29,29,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(!dc_gpu_read_body_id(g,0x80000001u,&body,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,30,30,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_pixel_t pixel;unsigned count=0;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,&pixel,1,&count,err,sizeof(err)));
    ASSERT_EQ(count,1u);dc_gpu_destroy(g);free(chunk);PASS();
}

static void test_full_soil_and_gravel_remain_packed_after_settling(void) {
    dc_gpu_t *g=NULL;char err[256];dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    for(unsigned material=DC_MATERIAL_DIRT;material<=DC_MATERIAL_GRAVEL;material++) {
        *chunk=(dc_chunk_t){0};
        for(unsigned y=0;y<64;y++) for(unsigned x=0;x<64;x++) {
            if(y>=48||x==23||x==40) chunk->cells[y*64+x].material=DC_MATERIAL_STONE;
            else if(y>=24&&x>=24&&x<40) chunk->cells[y*64+x].material=material;
        }
        ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
        for(unsigned tick=0;tick<64;tick++) ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
        unsigned filled=0,doubled=0;uint64_t mass=0;
        for(unsigned i=0;i<DC_CHUNK_CELLS;i++) {
            filled+=chunk->cells[i].material==material;
            doubled+=chunk->particles[i+4096].mass_fp!=0;
            mass+=chunk->particles[i].mass_fp+chunk->particles[i+4096].mass_fp;
        }
        printf("material%u packed cells%u secondary%u mass%llu\n",material,filled,doubled,(unsigned long long)mass);
        ASSERT_EQ(mass,(uint64_t)384*(material==DC_MATERIAL_DIRT?78643u:176947u));
        ASSERT_EQ(filled,384u);ASSERT_EQ(doubled,0u);
    }
    dc_gpu_destroy(g);free(chunk);PASS();
}

static void test_dense_leaf_contacts_cannot_weaken_stone_floor(void) {
    dc_gpu_t *g=NULL;char err[256];dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(unsigned y=48;y<64;y++) for(unsigned x=0;x<64;x++) chunk->cells[y*64+x].material=DC_MATERIAL_STONE;
    for(unsigned y=6;y<48;y++) for(unsigned x=28;x<36;x++) {
        unsigned cell=y*64+x;chunk->cells[cell].material=DC_MATERIAL_LEAF_DEBRIS;
        dc_chunk_particle_init(chunk->particles+cell,chunk->coord,cell,DC_MATERIAL_LEAF_DEBRIS);
        chunk->particles[cell+4096]=chunk->particles[cell];
        chunk->particles[cell+4096].id_lo+=4096;chunk->particle_count+=2;
    }
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    dc_gpu_body_shape_t shape={.count=4,.material=DC_GPU_BODY_WOOD,
        .vertices={{0,0},{6*65536,0},{6*65536,16*65536},{0,16*65536}}};
    dc_gpu_body_t body={.id=77,.active=1,.x_fp=29*65536,.y_fp=6*65536,.width=6,.height=16};
    dc_gpu_world_body_t world_body={.body=body};
    ASSERT_TRUE(dc_gpu_spawn_convex_body(g,world_body,&shape,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,77,0,0,2,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    float deepest=0;
    for(unsigned tick=0;tick<80;tick++) {
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
        dc_gpu_body_record_t *b=g->body_mapped;
        float bottom=b->body.y_fp/65536.f+b->center[1]+fabsf(sinf(b->angle))*3+fabsf(cosf(b->angle))*8;
        if(bottom>deepest) deepest=bottom;
    }
    printf("dense-leaf floor deepest body point %.4f (floor48)\n",deepest);
    ASSERT_TRUE(deepest>46.f);ASSERT_TRUE(deepest<48.2f);
    dc_gpu_destroy(g);free(chunk);PASS();
}

int main(void) {
    RUN(test_partial_granular_support_motion_does_not_release_tree_but_cut_does);
    RUN(test_full_soil_and_gravel_remain_packed_after_settling);
    RUN(test_dense_leaf_contacts_cannot_weaken_stone_floor);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
