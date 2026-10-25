#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ferrum/fire/gpu.h"
#include "../../src/vulkan/gpu_internal.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s [%s]\n",__LINE__,#e,err);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static bool context(dc_gpu_t **g,char *err,uint32_t cap) {
#ifdef FR_REACTIVE_PRESENTATION_TEST
    return dc_gpu_create_window(g,640,448,960,540,"build/shaders/pattern.comp.spv",err,cap)&&
        dc_gpu_set_viewport(*g,0,0,480,270)&&dc_gpu_set_display_zoom(*g,2);
#else
    return dc_gpu_create(g,64,64,"build/shaders/pattern.comp.spv",err,cap);
#endif
}
static bool present(dc_gpu_t *g,char *err,uint32_t cap) {
#ifdef FR_REACTIVE_PRESENTATION_TEST
    for(uint32_t i=0;i<4;i++) if(!dc_gpu_present_chunks(g,err,cap)) return false;
    printf("Native Wayland Vulkan: four presented frames, 480x270 physics/render, 640x448 halo\n");
#else
    (void)g;(void)err;(void)cap;
#endif
    return true;
}
static void test_cells_retain_sparse_state_and_brush_replacement_retires_it(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    c->coord=(dc_chunk_coord_t){-7,12};
    for(uint32_t i=0;i<512;i++) c->cells[i].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,c,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_record_t record;bool found=true;
    memset(&record,0xa5,sizeof(record));fr_reactive_record_t untouched=record;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,4,&record,&found,err,sizeof(err)));
    ASSERT_TRUE(!found&&memcmp(&record,&untouched,sizeof(record))==0);
    fr_reactive_state_t state;
    for(uint32_t i=0;i<512;i++) {
        ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,i%300,500+i,100+i,&state,err,sizeof(err)));
        state.ignition_threshold=.7f;state.ignition_integral=.3f;
        ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,i,&state,err,sizeof(err)));
    }
    for(uint32_t i=0;i<512;i+=17) {
        ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,i,&record,&found,err,sizeof(err)));
        ASSERT_TRUE(found);ASSERT_EQ(record.state.parcel_id_low,100+i);
        ASSERT_EQ(record.state.bound_water_fp,i%300);ASSERT_EQ(record.state.ignition_integral,.3f);
        fr_reactive_owner_t owner=fr_reactive_cell_owner(c->coord,i);
        ASSERT_TRUE(memcmp(&record.owner,&owner,sizeof(owner))==0);
    }
    /* Reusing an ID for another cell cannot duplicate mass or history. */
    ASSERT_TRUE(!fr_gpu_reactive_seed_cell(g,0,0,&state,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,1,0,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,1,&record,&found,err,sizeof(err)));ASSERT_TRUE(!found);
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,2,&record,&found,err,sizeof(err)));ASSERT_TRUE(found);
    ASSERT_TRUE(dc_gpu_paint_material(g,1,0,0,DC_MATERIAL_WOOD,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,1,&record,&found,err,sizeof(err)));ASSERT_TRUE(!found);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(c);dc_gpu_destroy(g);PASS();
}
static void test_extraction_and_next_arena_flip_keep_exact_state(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    for(uint32_t y=10;y<14;y++) for(uint32_t x=10;x<14;x++) c->cells[y*64+x].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t state;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,900,700,91,&state,err,sizeof(err)));
    state.coating_oil_fuel_fp=500;state.ignition_threshold=.8f;state.ignition_integral=.4f;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,11*64+11,&state,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,1,1,&record,&found,err,sizeof(err)));
    ASSERT_TRUE(found);ASSERT_TRUE(memcmp(&record.state,&state,sizeof(state))==0);
    ASSERT_EQ(record.owner.kind,FR_REACTIVE_OWNER_BODY);ASSERT_EQ(record.owner.key[0],0x80000001u);
    ASSERT_EQ(record.owner.key[1],1u);ASSERT_EQ(record.owner.key[2],1u);ASSERT_EQ(record.owner.key[3],1u);
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,11*64+11,&record,&found,err,sizeof(err)));ASSERT_TRUE(!found);
    ASSERT_TRUE(dc_gpu_set_body_motion(g,0x80000001u,0,0,DC_GPU_BODY_KINEMATIC,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,30,30,0,DC_MATERIAL_STONE,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,1,1,&record,&found,err,sizeof(err)));
    ASSERT_TRUE(found);ASSERT_TRUE(memcmp(&record.state,&state,sizeof(state))==0);
    ASSERT_TRUE(dc_gpu_paint_material(g,10,11,0,DC_MATERIAL_AIR,err,sizeof(err)));
    uint32_t remaining=0;ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,NULL,0,&remaining,err,sizeof(err)));
    ASSERT_EQ(remaining,15u);
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,1,1,&record,&found,err,sizeof(err)));
    ASSERT_TRUE(found);ASSERT_TRUE(memcmp(&record.state,&state,sizeof(state))==0);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(c);dc_gpu_destroy(g);PASS();
}
static void test_mpm_pool_swaps_preserve_parcel_and_leaf_pigment(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    uint32_t source=10*64+20;dc_chunk_particle_init(c->particles+source,c->coord,source,DC_MATERIAL_LEAF_DEBRIS);
    c->particles[source].flags=0x87c31eu;c->particles[source].vx_fp=65536;c->particle_count=1;
    c->cells[source].material=DC_MATERIAL_LEAF_DEBRIS;
    ASSERT_TRUE(context(&g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t state;ASSERT_TRUE(fr_reactive_state_from_particle(c->particles+source,800,301,&state,err,sizeof(err)));
    state.ignition_threshold=.9f;state.ignition_integral=.6f;
    ASSERT_TRUE(fr_gpu_reactive_seed_particle(g,source,&state,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,c,err,sizeof(err)));
    uint32_t destination=UINT32_MAX,live=0;
    for(uint32_t i=0;i<DC_MPM_PARTICLES_PER_CHUNK;i++) if(c->particles[i].mass_fp) {destination=i;live++;}
    ASSERT_EQ(live,1u);ASSERT_TRUE(destination!=source);ASSERT_EQ(c->particles[destination].flags,0x87c31eu);
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,destination,&record,&found,err,sizeof(err)));
    ASSERT_TRUE(found);ASSERT_TRUE(memcmp(&record.state,&state,sizeof(state))==0);
    ASSERT_EQ(record.owner.kind,FR_REACTIVE_OWNER_PARTICLE);
    ASSERT_EQ(record.owner.key[0],c->particles[destination].id_lo);ASSERT_EQ(record.owner.key[1],c->particles[destination].id_hi);
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,source,&record,&found,err,sizeof(err)));ASSERT_TRUE(!found);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(c);dc_gpu_destroy(g);PASS();
}
int main(void) {
    RUN(test_cells_retain_sparse_state_and_brush_replacement_retires_it);
    RUN(test_extraction_and_next_arena_flip_keep_exact_state);
    RUN(test_mpm_pool_swaps_preserve_parcel_and_leaf_pigment);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
