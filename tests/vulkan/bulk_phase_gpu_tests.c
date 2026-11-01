#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/fire/bulk_phase_gpu.h"

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
    dc_gpu_destroy(gpu);free(chunk);free(after);free(phase);PASS();
}
int main(void) {
    RUN(test_gpu_chunk_keeps_distinct_bulk_phase_and_clears_reused_slot);
    RUN(test_water_solver_respects_full_oil_cell_capacity);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
