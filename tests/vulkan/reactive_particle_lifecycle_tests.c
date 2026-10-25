#define main reactive_ownership_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main

#include "dungeoncraft/leaf_material.h"

static dc_chunk_t *loose_leaf_chunk(uint32_t source,bool secondary) {
    dc_chunk_t *c=calloc(1,sizeof(*c));
    if(!c) return NULL;
    uint32_t index=source+(secondary?DC_CHUNK_CELLS:0u);
    dc_chunk_particle_init(c->particles+index,c->coord,source,DC_MATERIAL_LEAF_DEBRIS);
    c->particles[index].flags=DC_LEAF_COLOR_FLAG|0x345678u;
    c->particle_count=1;c->cells[source].material=DC_MATERIAL_LEAF_DEBRIS;
    return c;
}
static void test_erasing_and_repainting_same_particle_id_does_not_restore_old_heat(void) {
    char err[256]={0};dc_gpu_t *g=NULL;uint32_t source=10*64+20;
    dc_chunk_t *c=loose_leaf_chunk(source,false);ASSERT_TRUE(c);
    ASSERT_TRUE(context(&g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t state;ASSERT_TRUE(fr_reactive_state_from_particle(c->particles+source,800,801,&state,err,sizeof(err)));
    state.coating_oil_fuel_fp=500;state.ignition_threshold=.7f;state.ignition_integral=.4f;
    ASSERT_TRUE(fr_gpu_reactive_seed_particle(g,source,&state,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,20,10,0,DC_MATERIAL_AIR,err,sizeof(err)));
    fr_reactive_record_t record;bool found=true;
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,source,&record,&found,err,sizeof(err)));ASSERT_TRUE(!found);
    ASSERT_TRUE(dc_gpu_paint_material(g,20,10,0,DC_MATERIAL_LEAF_DEBRIS,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,c,err,sizeof(err)));
    ASSERT_TRUE(c->particles[source].mass_fp>0u);ASSERT_EQ(c->particles[source].id_lo,source+1u);
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,source,&record,&found,err,sizeof(err)));ASSERT_TRUE(!found);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(c);dc_gpu_destroy(g);PASS();
}
static void test_secondary_promotion_moves_state_and_retains_leaf_pigment(void) {
    char err[256]={0};dc_gpu_t *g=NULL;uint32_t source=10*64+20,secondary=source+4096;
    dc_chunk_t *c=loose_leaf_chunk(source,true);ASSERT_TRUE(c);
    ASSERT_TRUE(context(&g,err,sizeof(err)));bool restored=false;
    ASSERT_TRUE(dc_gpu_upload_chunk_checkpoint(g,0,c,false,&restored,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t state;ASSERT_TRUE(fr_reactive_state_from_particle(c->particles+secondary,800,901,&state,err,sizeof(err)));
    state.coating_oil_fuel_fp=500;state.ignition_threshold=.7f;state.ignition_integral=.4f;
    ASSERT_TRUE(fr_gpu_reactive_seed_particle(g,secondary,&state,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,20,10,0,DC_MATERIAL_WATER,err,sizeof(err)));
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,source,&record,&found,err,sizeof(err)));ASSERT_TRUE(found);
    ASSERT_TRUE(memcmp(&record.state,&state,sizeof(state))==0);
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,secondary,&record,&found,err,sizeof(err)));ASSERT_TRUE(!found);
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,c,err,sizeof(err)));
    ASSERT_EQ(c->particles[source].flags,DC_LEAF_COLOR_FLAG|0x345678u);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(c);dc_gpu_destroy(g);PASS();
}
static void test_replacing_leaf_with_soil_does_not_inherit_pigment_as_water_or_reactions(void) {
    char err[256]={0};dc_gpu_t *g=NULL;uint32_t source=10*64+20;
    dc_chunk_t *c=loose_leaf_chunk(source,false);ASSERT_TRUE(c);
    ASSERT_TRUE(context(&g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t state;ASSERT_TRUE(fr_reactive_state_from_particle(c->particles+source,800,1001,&state,err,sizeof(err)));
    state.coating_oil_fuel_fp=500;state.ignition_threshold=.7f;state.ignition_integral=.4f;
    ASSERT_TRUE(fr_gpu_reactive_seed_particle(g,source,&state,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,20,10,0,DC_MATERIAL_DIRT,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,c,err,sizeof(err)));
    ASSERT_EQ(c->particles[source].material,DC_MATERIAL_DIRT);ASSERT_EQ(c->particles[source].flags,0u);
    fr_reactive_record_t record;bool found=true;
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,source,&record,&found,err,sizeof(err)));ASSERT_TRUE(!found);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(c);dc_gpu_destroy(g);PASS();
}
static void test_halo_transfer_and_secondary_promotion_preserve_both_parcels(void) {
    char err[256]={0};dc_gpu_t *g=NULL;uint32_t source=20*64+63,secondary=source+4096,destination=8192+20*64;
    dc_chunk_t *left=loose_leaf_chunk(source,false),*right=calloc(1,sizeof(*right));ASSERT_TRUE(left&&right);
    left->coord=(dc_chunk_coord_t){-1,0};right->coord=(dc_chunk_coord_t){0,0};
    dc_chunk_particle_init(left->particles+source,left->coord,source,DC_MATERIAL_LEAF_DEBRIS);
    left->particles[secondary]=left->particles[source];left->particles[secondary].id_lo|=0x40000000u;left->particle_count=2;
#ifdef FR_REACTIVE_PRESENTATION_TEST
    ASSERT_TRUE(context(&g,err,sizeof(err)));
#else
    ASSERT_TRUE(dc_gpu_create(&g,128,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
#endif
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,left,err,sizeof(err))&&dc_gpu_upload_chunk(g,1,right,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,left->coord,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,1,0,UINT32_MAX,err,sizeof(err)));
    fr_reactive_state_t a,b;ASSERT_TRUE(fr_reactive_state_from_particle(left->particles+source,600,1101,&a,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_state_from_particle(left->particles+secondary,700,1102,&b,err,sizeof(err)));
    a.ignition_threshold=b.ignition_threshold=.8f;a.ignition_integral=.4f;b.ignition_integral=.5f;
    ASSERT_TRUE(fr_gpu_reactive_seed_particle(g,source,&a,err,sizeof(err))&&fr_gpu_reactive_seed_particle(g,secondary,&b,err,sizeof(err)));
    dc_gpu_transfer_t transfer={.from_x=63,.from_y=20,.to_x=64,.to_y=20,.kind=DC_GPU_TRANSFER_PARTICLE};
    ASSERT_TRUE(dc_gpu_queue_transfer(g,transfer,err,sizeof(err)));
    dc_gpu_transfer_state_t outcome;
    ASSERT_TRUE(dc_gpu_try_transfer(g,&outcome,err,sizeof(err)));ASSERT_EQ(outcome,DC_GPU_TRANSFER_PENDING);
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,source,&record,&found,err,sizeof(err)));ASSERT_TRUE(found);
    ASSERT_TRUE(memcmp(&record.state,&a,sizeof(a))==0);
    ASSERT_TRUE(dc_gpu_set_page(g,1,0,1,err,sizeof(err))&&dc_gpu_try_transfer(g,&outcome,err,sizeof(err)));
    ASSERT_EQ(outcome,DC_GPU_TRANSFER_APPLIED);
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,destination,&record,&found,err,sizeof(err)));ASSERT_TRUE(found);
    ASSERT_TRUE(memcmp(&record.state,&a,sizeof(a))==0);
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,source,&record,&found,err,sizeof(err)));ASSERT_TRUE(found);
    ASSERT_TRUE(memcmp(&record.state,&b,sizeof(b))==0);
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,secondary,&record,&found,err,sizeof(err)));ASSERT_TRUE(!found);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(left);free(right);dc_gpu_destroy(g);PASS();
}
int main(void) {
    RUN(test_erasing_and_repainting_same_particle_id_does_not_restore_old_heat);
    RUN(test_secondary_promotion_moves_state_and_retains_leaf_pigment);
    RUN(test_replacing_leaf_with_soil_does_not_inherit_pigment_as_water_or_reactions);
    RUN(test_halo_transfer_and_secondary_promotion_preserve_both_parcels);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
