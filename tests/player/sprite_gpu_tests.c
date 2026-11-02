#include <stdio.h>
#include <stdlib.h>

#include "ferrum/renderer/gpu.h"
#include "ferrum/renderer/sprite_gpu.h"
#include "../../src/vulkan/runtime_body_gpu.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(x) do {if(!(x)){printf("FAIL %s:%d: %s (%s)\n",__FILE__,__LINE__,#x,err);g_fail++;return;} } while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++
static dc_gpu_t *g;static char err[256];
static void test_real_renderer_follows_gpu_body_and_rejects_stale_generation(void) {
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));free(chunk);
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    dc_gpu_world_body_t b={.body={.id=71,.active=1,.width=4,.height=4,.x_fp=10*65536,.y_fp=10*65536,.vx_fp=65536}};
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,b,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    uint32_t generation=0;
    for(uint32_t s=0;s<dc_gpu_body_capacity(g);s++) {
        dc_body_storage_owner_t *o=dc_gpu_body_owner(g,s);
        if(o->runtime_id==71) generation=o->generation;
    }
    ASSERT_TRUE(generation);
    fr_renderer_sprite_pixel_t pixels[]={{0xff0000ffu,1},{0xff00ff00u,2}};
    ASSERT_TRUE(fr_renderer_sprite_atlas(g,pixels,2,err,sizeof(err)));
    fr_renderer_sprite_t pose={.body_id=71,.generation=generation,.width=2,.height=1};
    ASSERT_TRUE(fr_renderer_sprite_poses(g,&pose,1,err,sizeof(err)));
    ASSERT_TRUE(fr_renderer_set_mode(g,FR_RENDERER_ALBEDO));
    ASSERT_TRUE(fr_renderer_render(g,err,sizeof(err)));
    uint32_t image[64*64];ASSERT_TRUE(dc_gpu_readback(g,image,64*64,err,sizeof(err)));
    ASSERT_EQ(image[10*64+10],0xff0000ffu);ASSERT_EQ(image[10*64+11],0xff00ff00u);
    pose.flip=1;ASSERT_TRUE(fr_renderer_sprite_poses(g,&pose,1,err,sizeof(err)));
    ASSERT_TRUE(fr_renderer_render(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(g,image,64*64,err,sizeof(err)));ASSERT_EQ(image[10*64+10],0xff00ff00u);
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));ASSERT_TRUE(fr_renderer_render(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(g,image,64*64,err,sizeof(err)));ASSERT_EQ(image[10*64+11],0xff00ff00u);
    pose.generation++;ASSERT_TRUE(fr_renderer_sprite_poses(g,&pose,1,err,sizeof(err)));
    ASSERT_TRUE(fr_renderer_render(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(g,image,64*64,err,sizeof(err)));ASSERT_TRUE(image[10*64+11]!=0xff00ff00u);
    pose.width=3;ASSERT_TRUE(!fr_renderer_sprite_poses(g,&pose,1,err,sizeof(err)));
    PASS();
}
int main(void) {
    if(!dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err))) {puts(err);return 1;}
    RUN(test_real_renderer_follows_gpu_body_and_rejects_stale_generation);
    dc_gpu_destroy(g);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
