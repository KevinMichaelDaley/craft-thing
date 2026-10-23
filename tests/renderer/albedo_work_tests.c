#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/vulkan/gpu_internal.h"
#include "ferrum/renderer/gpu.h"
#include "ferrum/renderer/occlusion_gpu.h"

static int g_pass,g_fail;
static bool recording;
static unsigned full_grid_dispatches;
void __real_fr_renderer_record(dc_gpu_t *);
VKAPI_ATTR void VKAPI_CALL __real_vkCmdDispatch(VkCommandBuffer,uint32_t,uint32_t,uint32_t);
void __wrap_fr_renderer_record(dc_gpu_t *g) {
    recording=true;__real_fr_renderer_record(g);recording=false;
}
VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdDispatch(VkCommandBuffer c,uint32_t x,uint32_t y,uint32_t z) {
    if(recording&&x==128u*128u/64u&&y==1u&&z==1u) full_grid_dispatches++;
    __real_vkCmdDispatch(c,x,y,z);
}
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static dc_gpu_t *scene(char *err,uint32_t cap) {
    dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    if(!chunk||!dc_gpu_create(&g,128,128,"build/shaders/pattern.comp.spv",err,cap)) {free(chunk);return NULL;}
    for(unsigned p=0;p<4;p++) {
        memset(chunk,0,sizeof(*chunk));chunk->coord=(dc_chunk_coord_t){p%2,p/2};
        if(p==3) chunk->cells[2*64+2].material=DC_MATERIAL_STONE;
        if(!dc_gpu_upload_chunk(g,p,chunk,err,cap)||!dc_gpu_set_page(g,p%2,p/2,p,err,cap)) {
            free(chunk);dc_gpu_destroy(g);return NULL;
        }
    }
    free(chunk);
    if(!dc_gpu_set_viewport(g,0,0,64,64)||!fr_renderer_enable(g,err,cap)||!fr_renderer_set_mode(g,FR_RENDERER_ALBEDO)) {
        dc_gpu_destroy(g);return NULL;
    }
    return g;
}
static void test_albedo_work_tracks_visible_pixels_and_optical_queries_stay_fresh(void) {
    char err[256]={0};dc_gpu_t *g=scene(err,sizeof(err));ASSERT_TRUE(g);
    full_grid_dispatches=0;
    ASSERT_TRUE(fr_renderer_render(g,err,sizeof(err)));
    unsigned hidden_work=full_grid_dispatches;
    fr_occlusion_node_t node;
    ASSERT_TRUE(fr_renderer_read_opacity(g,66,66,&node));ASSERT_EQ(node.maximum,1.f);
    ASSERT_TRUE(dc_gpu_paint_material(g,66,66,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(fr_renderer_render(g,err,sizeof(err)));
    ASSERT_TRUE(fr_renderer_read_opacity(g,66,66,&node));ASSERT_EQ(node.maximum,0.f);
    dc_gpu_destroy(g);ASSERT_EQ(hidden_work,0u);PASS();
}
static void test_zero_coverage_does_not_alias_body_slot_sixty_four(void) {
    char err[256]={0};dc_gpu_t *g=scene(err,sizeof(err));ASSERT_TRUE(g);
    uint32_t *before=calloc(128u*128u,4),*after=calloc(128u*128u,4);ASSERT_TRUE(before&&after);
    ASSERT_TRUE(fr_renderer_render(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(g,before,128u*128u,err,sizeof(err)));
    dc_gpu_body_shape_t shape={.count=4,.material=DC_GPU_BODY_STONE,.vertices={{0,0},{4*65536,0},{4*65536,4*65536},{0,4*65536}}};
    for(unsigned id=1;id<=65;id++) {
        dc_gpu_world_body_t b={.body={.id=id,.active=1,.width=4,.height=4,.x_fp=(id==65?10:100)*65536,.y_fp=(id==65?10:100)*65536}};
        b.chunk=(dc_chunk_coord_t){b.body.x_fp/(64*65536),b.body.y_fp/(64*65536)};
        b.body.x_fp%=64*65536;b.body.y_fp%=64*65536;
        ASSERT_TRUE(dc_gpu_spawn_convex_body(g,b,&shape,err,sizeof(err)));
    }
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    memset(g->occupancy_mapped,0,128u*128u*5u*4u);
    ASSERT_TRUE(fr_renderer_render(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(g,after,128u*128u,err,sizeof(err)));
    uint32_t original=before[10*64+10],rendered=after[10*64+10];
    dc_gpu_destroy(g);free(before);free(after);ASSERT_EQ(original,rendered);PASS();
}
int main(void) {
    RUN(test_albedo_work_tracks_visible_pixels_and_optical_queries_stay_fresh);
    RUN(test_zero_coverage_does_not_alias_body_slot_sixty_four);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
