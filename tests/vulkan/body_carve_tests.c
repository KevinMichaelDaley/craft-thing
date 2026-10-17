#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "compound_fixture.h"
#include "../../src/vulkan/gpu_internal.h"
#include "ferrum/renderer/gpu.h"
#include "ferrum/structural/gpu.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_air_carves_colored_body_and_splits_remaining_components(void) {
    char err[256];dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));
    dc_chunk_colors_t *colors=calloc(1,sizeof(*colors));ASSERT_TRUE(c&&colors);
    for(unsigned y=20;y<23;y++) for(unsigned x=20;x<29;x++) {
        unsigned i=y*64+x;c->cells[i].material=DC_MATERIAL_WOOD;
        colors->terrain[i][0]=x/64.f;colors->terrain[i][1]=.25f;colors->terrain[i][3]=1;
    }
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,c,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_renderer_upload_chunk_colors(g,0,colors,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_record_t *b=g->body_mapped;
    b->body.x_fp=b->local_x_fp=20<<16;b->body.y_fp=b->local_y_fp=20<<16;
    b->body.vx_fp=1<<16;b->body.vy_fp=0;b->angular_velocity=0;
    ASSERT_TRUE(dc_gpu_paint_material(g,24,21,1,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    unsigned total=0,live=0;float mass=0;
    for(unsigned slot=0;slot<DC_GPU_BODY_CAPACITY;slot++) {
        b=(dc_gpu_body_record_t*)g->body_mapped+slot;if(!b->body.active) continue;
        live++;dc_gpu_body_pixel_t pixels[27];unsigned n=0;dc_gpu_body_motion_t motion;
        ASSERT_TRUE(dc_gpu_read_pixel_shape(g,b->body.id,pixels,27,&n,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_body_motion(g,b->body.id,&motion,err,sizeof(err)));
        ASSERT_EQ(b->body.vx_fp,1<<16);ASSERT_EQ(b->body.vy_fp,0);
        const uint32_t *shape=(uint32_t*)g->body_mapped+b->compound_reserved;
        for(unsigned i=0;i<n;i++) {
            unsigned cell=(pixels[i].y%16)*16+pixels[i].x%16;
            unsigned tile=(pixels[i].y/16)*shape[2]+pixels[i].x/16;
            unsigned word=b->compound_reserved+shape[8+tile]+1+cell/8;
            const float *rgb=dc_gpu_shape_colors(g,word)+(cell%8)*4;
            ASSERT_TRUE(fabsf(rgb[0]-(20+pixels[i].x)/64.f)<1e-6f);
            ASSERT_EQ(rgb[1],.25f);ASSERT_EQ(rgb[3],1.f);
        }
        total+=n;mass+=motion.mass;
    }
    ASSERT_EQ(live,2u);ASSERT_EQ(total,22u);ASSERT_TRUE(fabsf(mass-13.2f)<.001f);
    ASSERT_TRUE(fr_gpu_save_structure(g,"build/carved_body.dcs",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,24,21,20,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    for(unsigned slot=0;slot<DC_GPU_BODY_CAPACITY;slot++)
        ASSERT_TRUE(!((dc_gpu_body_record_t*)g->body_mapped)[slot].body.active);
    ASSERT_TRUE(fr_gpu_load_structure(g,"build/carved_body.dcs",err,sizeof(err)));
    total=0;for(unsigned slot=0;slot<DC_GPU_BODY_CAPACITY;slot++)
        total+=((dc_gpu_body_record_t*)g->body_mapped)[slot].body.active!=0;
    ASSERT_EQ(total,2u);dc_gpu_destroy(g);free(c);free(colors);PASS();
}
static void test_air_carves_compound_wall_without_filling_its_cavity(void) {
    char err[256];dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,c,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    dc_gpu_compound_shape_t shape=fixture_u();
    ASSERT_TRUE(dc_gpu_spawn_compound_body(g,fixture_body(7,20,20),&shape,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,21,24,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    uint32_t id;
    ASSERT_TRUE(dc_gpu_read_occupancy(g,21,24,&id,NULL,err,sizeof(err)));ASSERT_EQ(id,0u);
    ASSERT_TRUE(dc_gpu_read_occupancy(g,28,24,&id,NULL,err,sizeof(err)));ASSERT_EQ(id,0u);
    dc_gpu_body_motion_t motion;ASSERT_TRUE(dc_gpu_read_body_motion(g,7,&motion,err,sizeof(err)));
    ASSERT_TRUE(fabsf(motion.mass-159*.6f)<.001f);
    dc_gpu_destroy(g);free(c);PASS();
}
int main(void) {
    RUN(test_air_carves_colored_body_and_splits_remaining_components);
    RUN(test_air_carves_compound_wall_without_filling_its_cavity);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
