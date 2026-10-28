#define main authored_rigid_existing_tests
#include "reactive_cell_gpu_tests.c"
#undef main
#include "ferrum/physics/authored_rigid_gpu.h"
#include "../../src/vulkan/runtime_body_gpu.h"
#include "ferrum/structural/gpu.h"
#ifdef FR_REACTIVE_PRESENTATION_TEST
#include <SDL.h>
#endif

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
#ifdef FR_REACTIVE_PRESENTATION_TEST
    ASSERT_TRUE(dc_gpu_set_presentation_focus(g,(dc_presentation_focus_t){12,12,24,24}));
    ASSERT_TRUE(dc_gpu_set_overlay(g,DC_GPU_OVERLAY_RIGID_IDS));
    ASSERT_TRUE(dc_gpu_present_chunks_steps(g,0,err,sizeof(err)));SDL_Delay(500);
#endif
    ASSERT_TRUE(fr_gpu_save_structure(g,"build/authored_objects.dcs",err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_load_structure(g,"build/authored_objects.dcs",err,sizeof(err)));
    count=0;
    for(uint32_t slot=0;slot<dc_gpu_body_capacity(g);slot++) {
        dc_body_storage_owner_t *owner=dc_gpu_body_owner(g,slot);
        if(owner->state==DC_BODY_OWNER_LIVE&&owner->source_transaction.low==42u&&
           !owner->source_transaction.high&&(owner->source_object==101u||owner->source_object==102u)) count++;
    }
    ASSERT_EQ(count,2u);
    ASSERT_TRUE(dc_gpu_remove_body(g,first,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    chunk->cells[20*64+30]=(dc_cell_t){.material=DC_MATERIAL_WOOD};
    chunk->cells[20*64+31]=(dc_cell_t){.material=DC_MATERIAL_WOOD};
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err)));
    uint32_t ordinary=0;
    for(uint32_t slot=0;slot<dc_gpu_body_capacity(g);slot++) {
        dc_body_storage_owner_t *owner=dc_gpu_body_owner(g,slot);
        if(owner->state==DC_BODY_OWNER_LIVE&&!owner->source_transaction.low&&
           !owner->source_transaction.high&&!owner->source_object) ordinary++;
    }
    ASSERT_EQ(ordinary,1u);
    free(chunk);free(objects);dc_gpu_destroy(g);PASS();
}
static void test_chunk_seam_joins_only_matching_object_identities(void) {
    char err[256]={0};dc_gpu_t *g=NULL;
    ASSERT_TRUE(dc_gpu_create(&g,128,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    uint32_t cells=DC_GPU_CHUNK_SLOTS*DC_CHUNK_CELLS,*objects=calloc(cells,sizeof(*objects));
    ASSERT_TRUE(chunk&&objects);
    for(uint32_t slot=0;slot<2;slot++) {
        memset(chunk,0,sizeof(*chunk));chunk->coord=(dc_chunk_coord_t){slot,0};
        for(uint32_t y=20;y<24;y++) {
            uint32_t local=y*64u+(slot?0u:63u);
            chunk->cells[local].material=DC_MATERIAL_WOOD;objects[slot*4096u+local]=y<22?101u:102u;
        }
        ASSERT_TRUE(dc_gpu_upload_chunk(g,slot,chunk,err,sizeof(err))&&dc_gpu_set_page(g,slot,0,slot,err,sizeof(err)));
    }
    ASSERT_TRUE(fr_gpu_admit_rigid_objects(g,44,objects,cells,err,sizeof(err)));
    uint32_t count=0;
    for(uint32_t slot=0;slot<dc_gpu_body_capacity(g);slot++) {
        dc_body_storage_owner_t *owner=dc_gpu_body_owner(g,slot);
        if(owner->state!=DC_BODY_OWNER_LIVE||owner->source_transaction.low!=44u) continue;
        uint32_t occupied=0;ASSERT_TRUE(dc_gpu_read_pixel_shape(g,owner->runtime_id,NULL,0,&occupied,err,sizeof(err)));
        ASSERT_EQ(occupied,4u);count++;
    }
    ASSERT_EQ(count,2u);free(chunk);free(objects);dc_gpu_destroy(g);PASS();
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
static void test_large_concave_arm_and_touching_load_keep_exact_geometry(void) {
    char err[256]={0};dc_gpu_t *g=NULL;ASSERT_TRUE(context(&g,err,sizeof(err)));
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    uint32_t cells=DC_GPU_CHUNK_SLOTS*DC_CHUNK_CELLS,*objects=calloc(cells,sizeof(*objects));
    ASSERT_TRUE(chunk&&objects);
    for(uint32_t y=10;y<52;y++) for(uint32_t x=20;x<28;x++) {
        bool cup=y<=15&&(x==20||x==27||y==15),stem=y>=16&&(x==23||x==24);
        bool load=x>=21&&x<=22&&y>=13&&y<=14;
        if(!cup&&!stem&&!load) continue;
        chunk->cells[y*64+x].material=DC_MATERIAL_WOOD;
        objects[y*64+x]=load?2u:1u;
    }
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_admit_rigid_objects(g,45,objects,cells,err,sizeof(err)));
    uint32_t seen=0;
    for(uint32_t slot=0;slot<dc_gpu_body_capacity(g);slot++) {
        dc_body_storage_owner_t *owner=dc_gpu_body_owner(g,slot);
        if(owner->state!=DC_BODY_OWNER_LIVE||owner->source_transaction.low!=45u) continue;
        dc_gpu_body_pixel_t pixels[90];uint32_t count=0;
        ASSERT_TRUE(dc_gpu_read_pixel_shape(g,owner->runtime_id,pixels,90,&count,err,sizeof(err)));
        ASSERT_EQ(count,owner->source_object==1u?90u:4u);
        if(owner->source_object==1u) for(uint32_t i=0;i<count;i++)
            ASSERT_TRUE(!(pixels[i].x>=1u&&pixels[i].x<=2u&&pixels[i].y>=3u&&pixels[i].y<=4u));
        seen++;
    }
    ASSERT_EQ(seen,2u);free(chunk);free(objects);dc_gpu_destroy(g);PASS();
}
int main(void) {
    RUN(test_touching_authored_objects_remain_independent_real_bodies);
    RUN(test_invalid_object_texel_rejects_before_any_publication);
    RUN(test_chunk_seam_joins_only_matching_object_identities);
    RUN(test_large_concave_arm_and_touching_load_keep_exact_geometry);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
