#define main authored_rigid_existing_tests
#include "reactive_cell_gpu_tests.c"
#undef main
#include "ferrum/physics/authored_rigid_gpu.h"
#include "../../src/vulkan/runtime_body_gpu.h"
#include "ferrum/structural/gpu.h"

static void test_touching_authored_objects_remain_independent_real_bodies(void) {
    char err[256]={0};dc_gpu_t *g=NULL;ASSERT_TRUE(context(&g,err,sizeof(err)));
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    const uint32_t cells=DC_GPU_CHUNK_SLOTS*DC_CHUNK_CELLS;
    uint32_t *objects=calloc(cells,sizeof(*objects));ASSERT_TRUE(chunk&&objects);
    for(uint32_t y=20;y<24;y++) for(uint32_t x=20;x<24;x++) {
        chunk->cells[y*64u+x].material=DC_MATERIAL_WOOD;
        objects[y*64u+x]=x<22?101u:102u;
    }
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_admit_rigid_objects(g,42,objects,cells,err,sizeof(err)));
    uint32_t first=0,second=0,count=0;
    for(uint32_t slot=0;slot<dc_gpu_body_capacity(g);slot++) {
        dc_body_storage_owner_t *owner=dc_gpu_body_owner(g,slot);
        if(owner->state!=DC_BODY_OWNER_LIVE||owner->source_transaction.low!=42u||owner->source_transaction.high) continue;
        if(owner->source_object==101u) first=owner->runtime_id;
        if(owner->source_object==102u) second=owner->runtime_id;
        uint32_t occupied=0;ASSERT_TRUE(dc_gpu_read_pixel_shape(g,owner->runtime_id,NULL,0,&occupied,err,sizeof(err)));
        ASSERT_EQ(occupied,8u);count++;
    }
    ASSERT_EQ(count,2u);ASSERT_TRUE(first&&second&&first!=second);
    ASSERT_TRUE(fr_gpu_save_structure(g,"build/authored_objects.dcs",err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_load_structure(g,"build/authored_objects.dcs",err,sizeof(err)));
    count=0;
    for(uint32_t slot=0;slot<dc_gpu_body_capacity(g);slot++) {
        dc_body_storage_owner_t *owner=dc_gpu_body_owner(g,slot);
        if(owner->state==DC_BODY_OWNER_LIVE&&owner->source_transaction.low==42u&&
           !owner->source_transaction.high&&(owner->source_object==101u||owner->source_object==102u)) count++;
    }
    ASSERT_EQ(count,2u);
    free(chunk);free(objects);dc_gpu_destroy(g);PASS();
}
static void test_invalid_object_texel_rejects_before_any_publication(void) {
    char err[256]={0};dc_gpu_t *g=NULL;ASSERT_TRUE(context(&g,err,sizeof(err)));
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    uint32_t cells=DC_GPU_CHUNK_SLOTS*DC_CHUNK_CELLS,*objects=calloc(cells,sizeof(*objects));
    ASSERT_TRUE(chunk&&objects);
    chunk->cells[20*64+20].material=DC_MATERIAL_WOOD;
    objects[20*64+20]=1;objects[20*64+21]=2;
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(!fr_gpu_admit_rigid_objects(g,43,objects,cells,err,sizeof(err)));
    dc_gpu_extraction_stats_t stats;ASSERT_TRUE(dc_gpu_read_extraction_stats(g,&stats));
    ASSERT_EQ(stats.owned_bodies,0u);
    free(chunk);free(objects);dc_gpu_destroy(g);PASS();
}
int main(void) {
    RUN(test_touching_authored_objects_remain_independent_real_bodies);
    RUN(test_invalid_object_texel_rejects_before_any_publication);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
