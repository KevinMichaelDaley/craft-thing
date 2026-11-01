#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/fire/bulk_phase_gpu.h"
#include "ferrum/fire/thermal_gpu.h"

static int g_pass=0,g_fail=0;
#define RUN(fn) do {printf("RUN  %s\n",#fn);fn();printf("OK   %s\n",#fn);} while(0)
#define ASSERT_TRUE(expr) do {if(!(expr)){printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_gpu_chunk_keeps_distinct_bulk_phase_and_clears_reused_slot(void) {
    char err[256]={0};dc_gpu_t *gpu=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_bulk_phase_chunk_t *phase=calloc(1,sizeof(*phase));
    fr_bulk_phase_chunk_t *out=calloc(1,sizeof(*out));ASSERT_TRUE(chunk&&phase&&out);
    chunk->cells[17].fluid_mass=30000;
    phase->oil_fp[17]=20000;phase->slime_fp[17]=15536;
    phase->oil_retained_fp[17]=1234;phase->slime_retained_fp[17]=4321;
    ASSERT_TRUE(dc_gpu_create(&gpu,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err))&&
        dc_gpu_upload_chunk(gpu,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(gpu,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_bulk_phase_gpu_import_chunk(gpu,0,chunk,phase,err,sizeof(err))&&
        fr_bulk_phase_gpu_export_chunk(gpu,0,out,err,sizeof(err)));
    ASSERT_EQ(memcmp(phase,out,sizeof(*phase)),0);
    phase->oil_fp[17]++;
    ASSERT_TRUE(!fr_bulk_phase_gpu_import_chunk(gpu,0,chunk,phase,err,sizeof(err))&&
        fr_bulk_phase_gpu_export_chunk(gpu,0,out,err,sizeof(err)));
    ASSERT_INT_EQ(out->oil_fp[17],20000);
    phase->oil_fp[17]--;
    phase->oil_retained_fp[17]=20001u;
    ASSERT_TRUE(!fr_bulk_phase_gpu_import_chunk(gpu,0,chunk,phase,err,sizeof(err))&&
        fr_bulk_phase_gpu_export_chunk(gpu,0,out,err,sizeof(err)));
    ASSERT_INT_EQ(out->oil_retained_fp[17],1234);
    phase->oil_retained_fp[17]=1234u;
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu,0,chunk,err,sizeof(err))&&
        fr_bulk_phase_gpu_export_chunk(gpu,0,out,err,sizeof(err)));
    ASSERT_INT_EQ(out->oil_fp[17],0);ASSERT_INT_EQ(out->slime_fp[17],0);
    dc_gpu_destroy(gpu);free(chunk);free(phase);free(out);PASS();
}
static void test_water_solver_respects_full_oil_cell_capacity(void) {
    char err[256]={0};dc_gpu_t *gpu=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    dc_chunk_t *after=calloc(1,sizeof(*after));fr_bulk_phase_chunk_t *phase=calloc(1,sizeof(*phase));
    ASSERT_TRUE(chunk&&after&&phase);
    const uint32_t source=5u*64u+30u,destination=6u*64u+30u;
    chunk->cells[source].fluid_mass=FR_BULK_CELL_FULL_FP;
    chunk->cells[destination+64u].material=DC_MATERIAL_STONE;
    chunk->cells[destination-1u].material=DC_MATERIAL_STONE;
    chunk->cells[destination+1u].material=DC_MATERIAL_STONE;
    phase->oil_fp[destination]=FR_BULK_CELL_FULL_FP;
    ASSERT_TRUE(dc_gpu_create(&gpu,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err))&&
        dc_gpu_upload_chunk(gpu,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(gpu,0,0,0,err,sizeof(err))&&
        fr_bulk_phase_gpu_import_chunk(gpu,0,chunk,phase,err,sizeof(err))&&
        dc_gpu_fluid_step(gpu,err,sizeof(err))&&
        dc_gpu_download_chunk(gpu,0,after,err,sizeof(err)));
    ASSERT_INT_EQ(after->cells[destination].fluid_mass,0);
    phase->oil_fp[destination]=40000u;
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu,0,chunk,err,sizeof(err))&&
        fr_bulk_phase_gpu_import_chunk(gpu,0,chunk,phase,err,sizeof(err))&&
        dc_gpu_fluid_step(gpu,err,sizeof(err))&&
        dc_gpu_download_chunk(gpu,0,after,err,sizeof(err)));
    ASSERT_TRUE(after->cells[destination].fluid_mass>0u&&
        after->cells[destination].fluid_mass<=FR_BULK_CELL_FULL_FP-40000u);
    dc_gpu_destroy(gpu);free(chunk);free(after);free(phase);PASS();
}
static void test_dense_oil_exchanges_with_water_in_sealed_vertical_pair(void) {
    char err[256]={0};dc_gpu_t *gpu=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    dc_chunk_t *after=calloc(1,sizeof(*after));
    fr_bulk_phase_chunk_t *bulk=calloc(1,sizeof(*bulk)),*out=calloc(1,sizeof(*out));
    ASSERT_TRUE(chunk&&after&&bulk&&out);
    const uint32_t upper=20u*64u+30u,lower=21u*64u+30u;
    for(uint32_t y=19u;y<=22u;y++) for(uint32_t x=29u;x<=31u;x++)
        if(x!=30u||y==19u||y==22u) chunk->cells[y*64u+x].material=DC_MATERIAL_STONE;
    chunk->cells[lower].fluid_mass=FR_BULK_CELL_FULL_FP;
    bulk->oil_fp[upper]=FR_BULK_CELL_FULL_FP;
    ASSERT_TRUE(dc_gpu_create(&gpu,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err))&&
        dc_gpu_upload_chunk(gpu,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(gpu,0,0,0,err,sizeof(err))&&
        fr_bulk_phase_gpu_import_chunk(gpu,0,chunk,bulk,err,sizeof(err))&&
        fr_bulk_phase_gpu_step(gpu,1.f/60.f,err,sizeof(err))&&
        dc_gpu_download_chunk(gpu,0,after,err,sizeof(err))&&
        fr_bulk_phase_gpu_export_chunk(gpu,0,out,err,sizeof(err)));
    ASSERT_TRUE(out->oil_fp[upper]<FR_BULK_CELL_FULL_FP&&out->oil_fp[lower]>0u);
    ASSERT_TRUE(after->cells[upper].fluid_mass>0u&&
        after->cells[lower].fluid_mass<FR_BULK_CELL_FULL_FP);
    ASSERT_EQ((uint64_t)out->oil_fp[upper]+out->oil_fp[lower],FR_BULK_CELL_FULL_FP);
    ASSERT_EQ((uint64_t)after->cells[upper].fluid_mass+after->cells[lower].fluid_mass,
        FR_BULK_CELL_FULL_FP);
    ASSERT_EQ((uint64_t)out->oil_fp[upper]+after->cells[upper].fluid_mass,
        FR_BULK_CELL_FULL_FP);
    ASSERT_EQ((uint64_t)out->oil_fp[lower]+after->cells[lower].fluid_mass,
        FR_BULK_CELL_FULL_FP);
    dc_gpu_destroy(gpu);free(chunk);free(after);free(bulk);free(out);PASS();
}
static void test_water_heat_follows_water_exchanged_with_oil(void) {
    char err[256]={0};dc_gpu_t *gpu=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    dc_chunk_t *after=calloc(1,sizeof(*after));
    fr_bulk_phase_chunk_t *bulk=calloc(1,sizeof(*bulk));
    float *water_heat=calloc(4096u,sizeof(*water_heat));
    ASSERT_TRUE(chunk&&after&&bulk&&water_heat);
    const uint32_t upper=20u*64u+30u,lower=upper+64u;
    for(uint32_t y=19u;y<=22u;y++) for(uint32_t x=29u;x<=31u;x++)
        if(x!=30u||y==19u||y==22u) chunk->cells[y*64u+x].material=DC_MATERIAL_STONE;
    chunk->cells[lower].fluid_mass=FR_BULK_CELL_FULL_FP;
    bulk->oil_fp[upper]=FR_BULK_CELL_FULL_FP;
    ASSERT_TRUE(dc_gpu_create(&gpu,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err))&&
        dc_gpu_upload_chunk(gpu,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(gpu,0,0,0,err,sizeof(err))&&
        fr_thermal_gpu_enable(gpu,err,sizeof(err))&&
        fr_thermal_gpu_seed_water(gpu,0,lower,100.f,err,sizeof(err))&&
        fr_bulk_phase_gpu_import_chunk(gpu,0,chunk,bulk,err,sizeof(err))&&
        fr_bulk_phase_gpu_step(gpu,1.f/60.f,err,sizeof(err))&&
        dc_gpu_download_chunk(gpu,0,after,err,sizeof(err))&&
        fr_thermal_gpu_read_water_chunk(gpu,0,water_heat,4096u,err,sizeof(err)));
    ASSERT_TRUE(after->cells[upper].fluid_mass>0u&&after->cells[lower].fluid_mass>0u);
    ASSERT_TRUE(water_heat[upper]>0.f&&water_heat[lower]>0.f);
    ASSERT_TRUE(fabsf(water_heat[upper]+water_heat[lower]-100.f)<.001f);
    dc_gpu_destroy(gpu);free(chunk);free(after);free(bulk);free(water_heat);PASS();
}
static void test_normal_tick_moves_imported_oil(void) {
    char err[256]={0};dc_gpu_t *gpu=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_bulk_phase_chunk_t *bulk=calloc(1,sizeof(*bulk)),*out=calloc(1,sizeof(*out));
    ASSERT_TRUE(chunk&&bulk&&out);
    const uint32_t upper=20u*64u+30u,lower=21u*64u+30u;
    for(uint32_t y=19u;y<=22u;y++) for(uint32_t x=29u;x<=31u;x++)
        if(x!=30u||y==19u||y==22u) chunk->cells[y*64u+x].material=DC_MATERIAL_STONE;
    bulk->oil_fp[upper]=FR_BULK_CELL_FULL_FP;
    chunk->cells[lower].fluid_mass=FR_BULK_CELL_FULL_FP;
    ASSERT_TRUE(dc_gpu_create(&gpu,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err))&&
        dc_gpu_upload_chunk(gpu,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(gpu,0,0,0,err,sizeof(err))&&
        fr_bulk_phase_gpu_import_chunk(gpu,0,chunk,bulk,err,sizeof(err))&&
        dc_gpu_tick_step(gpu,err,sizeof(err))&&
        fr_bulk_phase_gpu_export_chunk(gpu,0,out,err,sizeof(err)));
    ASSERT_TRUE(out->oil_fp[lower]>0u);
    dc_gpu_destroy(gpu);free(chunk);free(bulk);free(out);PASS();
}
static void test_oil_crosses_horizontal_chunk_seam_conservatively(void) {
    char err[256]={0};dc_gpu_t *gpu=NULL;
    dc_chunk_t *left=calloc(1,sizeof(*left)),*right=calloc(1,sizeof(*right));
    fr_bulk_phase_chunk_t *source=calloc(1,sizeof(*source));
    fr_bulk_phase_chunk_t *out_left=calloc(1,sizeof(*out_left)),
        *out_right=calloc(1,sizeof(*out_right));
    ASSERT_TRUE(left&&right&&source&&out_left&&out_right);
    const uint32_t row=30u;
    source->oil_fp[row*64u+63u]=FR_BULK_CELL_FULL_FP;
    ASSERT_TRUE(dc_gpu_create(&gpu,128,64,"build/shaders/pattern.comp.spv",err,sizeof(err))&&
        dc_gpu_upload_chunk(gpu,0,left,err,sizeof(err))&&
        dc_gpu_upload_chunk(gpu,1,right,err,sizeof(err))&&
        dc_gpu_set_page(gpu,0,0,0,err,sizeof(err))&&
        dc_gpu_set_page(gpu,1,0,1,err,sizeof(err))&&
        fr_bulk_phase_gpu_import_chunk(gpu,0,left,source,err,sizeof(err))&&
        fr_bulk_phase_gpu_step(gpu,1.f/60.f,err,sizeof(err))&&
        fr_bulk_phase_gpu_export_chunk(gpu,0,out_left,err,sizeof(err))&&
        fr_bulk_phase_gpu_export_chunk(gpu,1,out_right,err,sizeof(err)));
    uint64_t right_total=0;
    for(uint32_t i=0;i<DC_CHUNK_CELLS;i++) right_total+=out_right->oil_fp[i];
    ASSERT_TRUE(right_total>0u);
    uint64_t total=0;
    for(uint32_t i=0;i<DC_CHUNK_CELLS;i++)
        total+=(uint64_t)out_left->oil_fp[i]+out_right->oil_fp[i];
    ASSERT_EQ(total,FR_BULK_CELL_FULL_FP);
    dc_gpu_destroy(gpu);free(left);free(right);free(source);free(out_left);free(out_right);PASS();
}
static void test_rigid_body_displaces_oil_upward_without_loss(void) {
    char err[256]={0};dc_gpu_t *gpu=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_bulk_phase_chunk_t *bulk=calloc(1,sizeof(*bulk)),*out=calloc(1,sizeof(*out));
    ASSERT_TRUE(chunk&&bulk&&out);
    const uint32_t source=30u*64u+20u,target=29u*64u+20u;
    for(uint32_t y=0;y<64u;y++) {
        chunk->cells[y*64u+19u].material=DC_MATERIAL_STONE;
        chunk->cells[y*64u+21u].material=DC_MATERIAL_STONE;
    }
    chunk->cells[31u*64u+20u].material=DC_MATERIAL_STONE;
    bulk->oil_fp[source]=FR_BULK_CELL_FULL_FP;
    ASSERT_TRUE(dc_gpu_create(&gpu,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err))&&
        dc_gpu_upload_chunk(gpu,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(gpu,0,0,0,err,sizeof(err))&&
        fr_bulk_phase_gpu_import_chunk(gpu,0,chunk,bulk,err,sizeof(err))&&
        dc_gpu_set_rigid_solver(gpu,true));
    dc_gpu_body_t body={.x_fp=20*(int32_t)FR_BULK_CELL_FULL_FP,
        .y_fp=30*(int32_t)FR_BULK_CELL_FULL_FP,.width=1,.height=1,.id=200,.active=1};
    ASSERT_TRUE(dc_gpu_spawn_body(gpu,body,err,sizeof(err))&&
        dc_gpu_set_body_motion(gpu,200,0,0,DC_GPU_BODY_KINEMATIC,err,sizeof(err))&&
        dc_gpu_rigid_step(gpu,err,sizeof(err))&&
        fr_bulk_phase_gpu_step(gpu,1.f/60.f,err,sizeof(err))&&
        fr_bulk_phase_gpu_export_chunk(gpu,0,out,err,sizeof(err)));
    ASSERT_EQ(out->oil_fp[source],0u);
    ASSERT_EQ(out->oil_fp[target],FR_BULK_CELL_FULL_FP);
    dc_gpu_destroy(gpu);free(chunk);free(bulk);free(out);PASS();
}
static void test_closed_rigid_displacement_retains_oil_overflow(void) {
    char err[256]={0};dc_gpu_t *gpu=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_bulk_phase_chunk_t *bulk=calloc(1,sizeof(*bulk)),*out=calloc(1,sizeof(*out));
    ASSERT_TRUE(chunk&&bulk&&out);
    const uint32_t source=30u*64u+20u;
    chunk->cells[29u*64u+20u].material=DC_MATERIAL_STONE;
    chunk->cells[31u*64u+20u].material=DC_MATERIAL_STONE;
    chunk->cells[30u*64u+19u].material=DC_MATERIAL_STONE;
    chunk->cells[30u*64u+21u].material=DC_MATERIAL_STONE;
    bulk->oil_fp[source]=FR_BULK_CELL_FULL_FP;
    ASSERT_TRUE(dc_gpu_create(&gpu,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err))&&
        dc_gpu_upload_chunk(gpu,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(gpu,0,0,0,err,sizeof(err))&&
        fr_bulk_phase_gpu_import_chunk(gpu,0,chunk,bulk,err,sizeof(err))&&
        dc_gpu_set_rigid_solver(gpu,true));
    dc_gpu_body_t body={.x_fp=20*(int32_t)FR_BULK_CELL_FULL_FP,
        .y_fp=30*(int32_t)FR_BULK_CELL_FULL_FP,.width=1,.height=1,.id=201,.active=1};
    ASSERT_TRUE(dc_gpu_spawn_body(gpu,body,err,sizeof(err))&&
        dc_gpu_set_body_motion(gpu,201,0,0,DC_GPU_BODY_KINEMATIC,err,sizeof(err))&&
        dc_gpu_rigid_step(gpu,err,sizeof(err))&&
        fr_bulk_phase_gpu_step(gpu,1.f/60.f,err,sizeof(err))&&
        fr_bulk_phase_gpu_export_chunk(gpu,0,out,err,sizeof(err)));
    ASSERT_EQ(out->oil_fp[source],FR_BULK_CELL_FULL_FP);
    ASSERT_EQ(out->oil_retained_fp[source],FR_BULK_CELL_FULL_FP);
    dc_gpu_destroy(gpu);free(chunk);free(bulk);free(out);PASS();
}
int main(void) {
    RUN(test_gpu_chunk_keeps_distinct_bulk_phase_and_clears_reused_slot);
    RUN(test_water_solver_respects_full_oil_cell_capacity);
    RUN(test_dense_oil_exchanges_with_water_in_sealed_vertical_pair);
    RUN(test_water_heat_follows_water_exchanged_with_oil);
    RUN(test_normal_tick_moves_imported_oil);
    RUN(test_oil_crosses_horizontal_chunk_seam_conservatively);
    RUN(test_rigid_body_displaces_oil_upward_without_loss);
    RUN(test_closed_rigid_displacement_retains_oil_overflow);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
