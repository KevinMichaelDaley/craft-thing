#define _POSIX_C_SOURCE 200809L
#define main reactive_view_fixture_main
#include "../vulkan/reactive_cell_gpu_tests.c"
#undef main
#include "../../src/app/level.c"

static bool receive(dc_streamer_t *stream,dc_stream_result_t *result) {
    uint64_t deadline=SDL_GetTicks64()+5000u;
    do {if(dc_stream_poll(stream,result)) return true;SDL_Delay(1);} while(SDL_GetTicks64()<deadline);
    return false;
}
static void test_level_save_and_load_publish_the_same_reactive_worker_channel(void) {
    char err[256]={0},directory[]="build/reactive_level_stream_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t view={0};dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    chunk->coord=(dc_chunk_coord_t){-5,-7};chunk->cells[0].material=DC_MATERIAL_WOOD;chunk->cells[0].flags=DC_CELL_LOADED_TERRAIN;
    ASSERT_TRUE(context(&view.gpu,err,sizeof(err))&&dc_chunk_table_init(&view.table,DC_GPU_CHUNK_SLOTS));
    view.stream=dc_stream_create(directory,123,4);ASSERT_TRUE(view.stream);
    snprintf(view.body_path,sizeof(view.body_path),"%s/rigid_bodies.bin",directory);
    uint32_t index=0;uint64_t generation=0;
    ASSERT_TRUE(dc_chunk_table_begin_load(&view.table,chunk->coord,&index,&generation));
    ASSERT_TRUE(dc_gpu_upload_chunk(view.gpu,index,chunk,err,sizeof(err))&&dc_gpu_set_page(view.gpu,0,0,index,err,sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_finish_load(&view.table,index,generation));
    fr_reactive_state_t state;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,8192,800,80,&state,err,sizeof(err)));
    state.coating_oil_fuel_fp=300;state.ignition_threshold=.7f;state.ignition_integral=.4f;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(view.gpu,index,0,&state,err,sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_mark_dirty(&view.table,index)&&dc_chunk_table_set_active(&view.table,index,false));
    ASSERT_TRUE(schedule_saves(&view,err,sizeof(err)));
    dc_stream_result_t result={0};ASSERT_TRUE(receive(view.stream,&result));ASSERT_EQ(result.kind,DC_STREAM_SAVED);
    ASSERT_TRUE(dc_chunk_table_finish_save(&view.table,index,result.generation));dc_stream_result_release(&result);
    ASSERT_TRUE(dc_stream_request_load(view.stream,chunk->coord,generation));ASSERT_TRUE(receive(view.stream,&result));
    ASSERT_EQ(result.kind,DC_STREAM_LOADED);ASSERT_TRUE(result.reactive);ASSERT_EQ(result.reactive->count,1u);
    ASSERT_TRUE(memcmp(&result.reactive->attachments[0].record.state,&state,sizeof(state))==0);dc_stream_result_release(&result);
    ASSERT_TRUE(fr_gpu_reactive_import_chunk(view.gpu,index,chunk,NULL,err,sizeof(err)));
    fr_reactive_record_t record;bool found=true;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view.gpu,index,0,&record,&found,err,sizeof(err))&&!found);
    view.table.slots[index].state=DC_SLOT_LOADING;++view.table.slots[index].generation;
    ASSERT_TRUE(dc_stream_request_load(view.stream,chunk->coord,view.table.slots[index].generation));
    uint64_t deadline=SDL_GetTicks64()+5000u;
    while(view.table.slots[index].state==DC_SLOT_LOADING&&SDL_GetTicks64()<deadline) {
        ASSERT_TRUE(process_results(&view,err,sizeof(err)));SDL_Delay(1);
    }
    ASSERT_EQ(view.table.slots[index].state,DC_SLOT_ACTIVE);
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view.gpu,index,0,&record,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(memcmp(&record.state,&state,sizeof(state))==0);
    ASSERT_TRUE(present(view.gpu,err,sizeof(err)));
    dc_stream_destroy(view.stream);dc_gpu_destroy(view.gpu);dc_chunk_table_destroy(&view.table);free(chunk);PASS();
}
int main(void) {
    RUN(test_level_save_and_load_publish_the_same_reactive_worker_channel);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
