#include <stdio.h>
#include <stdlib.h>
#include "../../src/vulkan/gpu_internal.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_corner_connected_pixels_detach_as_one_component(void) {
    dc_gpu_t *g=NULL;char err[256];dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(unsigned y=30;y<64;y++) chunk->cells[y*64+32]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    chunk->cells[29*64+31]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    chunk->cells[28*64+30]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_t body;
    ASSERT_TRUE(!dc_gpu_read_body_id(g,0x80000001u,&body,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,32,30,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_pixel_t pixels[4];unsigned count=0;
    bool extracted=dc_gpu_read_pixel_shape(g,0x80000001u,pixels,4,&count,err,sizeof(err));
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    bool cleared=chunk->cells[29*64+31].material==DC_MATERIAL_AIR&&chunk->cells[28*64+30].material==DC_MATERIAL_AIR;
    dc_gpu_destroy(g);free(chunk);
    ASSERT_TRUE(extracted);ASSERT_EQ(count,2u);ASSERT_TRUE(cleared);PASS();
}

static void test_removing_granular_root_support_releases_entire_tree(void) {
    const unsigned materials[2]={DC_MATERIAL_DIRT,DC_MATERIAL_GRAVEL};
    for(unsigned trial=0;trial<2;trial++) {
        dc_gpu_t *g=NULL;char err[256];dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
        chunk->cells[29*64+30]=(dc_cell_t){.material=DC_MATERIAL_LEAVES,.flags=DC_CELL_LOADED_TERRAIN};
        chunk->cells[30*64+30]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
        chunk->cells[31*64+30].material=materials[trial];
        ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
        dc_gpu_body_t body;
        ASSERT_TRUE(!dc_gpu_read_body_id(g,0x80000001u,&body,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_paint_material(g,30,31,0,DC_MATERIAL_AIR,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
        dc_gpu_body_pixel_t pixels[2];unsigned count=0;
        bool extracted=dc_gpu_read_pixel_shape(g,0x80000001u,pixels,2,&count,err,sizeof(err));
        dc_gpu_destroy(g);free(chunk);
        ASSERT_TRUE(extracted);ASSERT_EQ(count,2u);
    }
    PASS();
}

static void test_settled_tree_falls_again_when_supporting_terrain_detaches(void) {
    dc_gpu_t *g=NULL;char err[256];dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(unsigned y=56;y<64;y++) for(unsigned x=0;x<64;x++)
        chunk->cells[y*64+x]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    for(unsigned y=30;y<35;y++) for(unsigned x=20;x<44;x++)
        chunk->cells[y*64+x]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    for(unsigned y=35;y<56;y++) for(unsigned x=42;x<44;x++)
        chunk->cells[y*64+x]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    for(unsigned y=12;y<23;y++) for(unsigned x=25;x<31;x++)
        chunk->cells[y*64+x].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    for(unsigned tick=0;tick<90;tick++) ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_t tree;
    ASSERT_TRUE(dc_gpu_read_body_id(g,0x80000001u,&tree,err,sizeof(err)));
    int settled_y=tree.y_fp;ASSERT_TRUE(settled_y>16*65536);
    ASSERT_TRUE(dc_gpu_paint_material(g,42,40,1,DC_MATERIAL_AIR,err,sizeof(err)));
    for(unsigned tick=0;tick<60;tick++) ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_t platform;dc_gpu_rigid_solver_stats_t stats;
    ASSERT_TRUE(dc_gpu_read_body_id(g,0x80000001u,&tree,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_id(g,0x80000002u,&platform,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_rigid_solver_stats(g,&stats));
    printf("tree %.4f -> %.4f platform %.4f solver flags %u\n",settled_y/65536.,tree.y_fp/65536.,platform.y_fp/65536.,stats.overflow);
    dc_gpu_destroy(g);free(chunk);
    ASSERT_TRUE(tree.y_fp>settled_y+5*65536);
    ASSERT_TRUE(platform.y_fp>35*65536);ASSERT_EQ(stats.overflow,0u);PASS();
}
static void test_corner_component_propagates_across_four_chunk_seam(void) {
    dc_gpu_t *g=NULL;char err[256];dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_create(&g,128,128,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    for(unsigned cy=0;cy<2;cy++) for(unsigned cx=0;cx<2;cx++) {
        *chunk=(dc_chunk_t){.coord={cx,cy}};
        for(unsigned y=0;y<64;y++) for(unsigned x=0;x<64;x++) {
            unsigned wx=cx*64+x,wy=cy*64+y;
            if((wx==63&&wy==63)||(wx==64&&wy==64)||(wx==65&&wy>=65))
                chunk->cells[y*64+x]=(dc_cell_t){.material=wx==65?DC_MATERIAL_STONE:DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
        }
        ASSERT_TRUE(dc_gpu_upload_chunk(g,cy*2+cx,chunk,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(g,cx,cy,cy*2+cx,err,sizeof(err)));
    }
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,65,65,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_pixel_t pixels[4];unsigned count=0;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,pixels,4,&count,err,sizeof(err)));
    ASSERT_EQ(count,2u);dc_gpu_destroy(g);free(chunk);PASS();
}
int main(void) {
    RUN(test_removing_granular_root_support_releases_entire_tree);
    RUN(test_corner_connected_pixels_detach_as_one_component);
    RUN(test_corner_component_propagates_across_four_chunk_seam);
    RUN(test_settled_tree_falls_again_when_supporting_terrain_detaches);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
