#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/vulkan/gpu_internal.h"
#include "../../src/vulkan/runtime_body_gpu.h"
#include "ferrum/renderer/gpu.h"
#include "ferrum/structural/gpu.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static bool colors_match(dc_gpu_t *g) {
    char err[128];unsigned count=0;double mass=0;
    for(unsigned child=0;child<4;child++) {
        uint32_t id=0x80000001u+child,n=0;
        dc_gpu_body_pixel_t pixels[400];dc_gpu_body_motion_t motion;
        if(!dc_gpu_read_pixel_shape(g,id,pixels,400,&n,err,sizeof(err)) ||
            !dc_gpu_read_body_motion(g,id,&motion,err,sizeof(err))) return false;
        dc_gpu_body_record_t *body=NULL;
        for(unsigned slot=0;slot<DC_GPU_BODY_CAPACITY;slot++) {
            dc_gpu_body_record_t *b=(dc_gpu_body_record_t*)g->body_mapped+slot;
            if(b->body.active&&b->body.id==id) body=b;
        }
        if(!body) return false;
        const uint32_t *geometry=(const uint32_t*)g->body_mapped+body->compound_reserved;
        unsigned origin_x=(child%2)==0?10:40;
        for(unsigned i=0;i<n;i++) {
            unsigned cell=(pixels[i].y%16)*16+pixels[i].x%16;
            unsigned tile=(pixels[i].y/16)*geometry[2]+pixels[i].x/16;
            uint32_t word=body->compound_reserved+geometry[8+tile]+1+cell/8;
            const float *color=dc_gpu_shape_colors(g,word)+(cell%8)*4;
            if(fabsf(color[0]-(origin_x+pixels[i].x)/64.f)>1e-6f ||
                fabsf(color[1]-(10+pixels[i].y)/64.f)>1e-6f ||
                color[2]!=.37f || color[3]!=1.f) return false;
        }
        count+=n;mass+=motion.mass;
    }
    return count==800 && fabs(mass-2160)<.01;
}
static void test_two_pixel_fractures_relocate_colors_and_restore_them(void) {
    char err[256]={0};dc_gpu_t *g=NULL;
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    dc_chunk_colors_t *colors=calloc(1,sizeof(*colors));ASSERT_TRUE(chunk&&colors);
    for(unsigned y=10;y<30;y++) for(unsigned x=10;x<60;x++) {
        if(x>=30&&x<40) continue;
        unsigned i=y*64+x;chunk->cells[i].material=DC_MATERIAL_STONE;
        colors->terrain[i][0]=x/64.f;colors->terrain[i][1]=y/64.f;
        colors->terrain[i][2]=.37f;colors->terrain[i][3]=1;
    }
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_renderer_upload_chunk_colors(g,0,colors,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_impact_fracture(g));
    for(unsigned i=0;i<2;i++) {
        uint32_t *impact=dc_gpu_body_solver_state(g,i)+52u;
        impact[0]=impact[1]=1000000;impact[2]=1;
        impact[3]=0x80000001u+i;
    }
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    bool fractured=colors_match(g);
    ASSERT_TRUE(fr_gpu_save_structure(g,"build/fractured_colors.dcs",err,sizeof(err)));
    dc_gpu_destroy(g);g=NULL;
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_load_structure(g,"build/fractured_colors.dcs",err,sizeof(err)));
    bool restored=colors_match(g);
    dc_gpu_destroy(g);free(chunk);free(colors);
    ASSERT_TRUE(fractured);ASSERT_TRUE(restored);PASS();
}
static void test_bulk_color_snapshot_matches_each_chunk_readback(void) {
    char err[256]={0};dc_gpu_t *g=NULL;
    dc_chunk_colors_t *saved=calloc(2,sizeof(*saved));
    dc_chunk_colors_t *paint=calloc(1,sizeof(*paint));
    dc_chunk_colors_t *direct=calloc(1,sizeof(*direct));
    ASSERT_TRUE(saved&&paint&&direct);
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    for(uint32_t slot=0;slot<2u;slot++) {
        for(uint32_t cell=0;cell<DC_CHUNK_CELLS;cell++) {
            paint->terrain[cell][0]=(float)(cell%64u)/64.f;
            paint->terrain[cell][1]=(float)slot/2.f;
            paint->terrain[cell][2]=.25f;
            paint->terrain[cell][3]=1.f;
            paint->water[cell][0]=(float)(cell%7u)/7.f;
            paint->water[cell][1]=.125f;
            paint->water[cell][2]=.5f;
            paint->water[cell][3]=1.f;
        }
        ASSERT_TRUE(fr_renderer_upload_chunk_colors(g,slot,paint,err,sizeof(err)));
    }
    ASSERT_TRUE(fr_renderer_capture_chunk_colors(g,saved,2u,err,sizeof(err)));
    for(uint32_t slot=0;slot<2u;slot++) {
        ASSERT_TRUE(fr_renderer_download_chunk_colors(g,slot,direct,err,sizeof(err)));
        ASSERT_TRUE(memcmp(saved+slot,direct,sizeof(*direct))==0);
    }
    dc_gpu_destroy(g);free(saved);free(paint);free(direct);PASS();
}
int main(void) {
    RUN(test_two_pixel_fractures_relocate_colors_and_restore_them);
    RUN(test_bulk_color_snapshot_matches_each_chunk_readback);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
