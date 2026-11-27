#define _POSIX_C_SOURCE 200809L
#include "ferrum/fire/bulk_phase_gpu.h"
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
    view.table.slots[index].time = (dc_chunk_time_t){.seconds = 4.5, .known = true};
    fr_reactive_state_t state;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,8192,800,80,&state,err,sizeof(err)));
    state.coating_oil_fuel_fp=300;state.ignition_threshold=.7f;state.ignition_integral=.4f;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(view.gpu,index,0,&state,err,sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_mark_dirty(&view.table,index)&&dc_chunk_table_set_active(&view.table,index,false));
    ASSERT_TRUE(schedule_saves(&view,err,sizeof(err)));
    dc_stream_result_t result={0};ASSERT_TRUE(receive(view.stream,&result));ASSERT_EQ(result.kind,DC_STREAM_SAVED);
    ASSERT_TRUE(dc_chunk_table_finish_save(&view.table,index,result.generation));dc_stream_result_release(&result);
    ASSERT_TRUE(dc_stream_request_load(view.stream,chunk->coord,generation));ASSERT_TRUE(receive(view.stream,&result));
    ASSERT_EQ(result.kind,DC_STREAM_LOADED);ASSERT_TRUE(result.reactive);ASSERT_EQ(result.reactive->count,1u);
    ASSERT_TRUE(result.time.known && result.time.seconds == 4.5);
    ASSERT_TRUE(result.bulk);ASSERT_INT_EQ(result.bulk->oil_fp[0],0);
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
    ASSERT_TRUE(view.table.slots[index].time.known && view.table.slots[index].time.seconds == 4.5);
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view.gpu,index,0,&record,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(memcmp(&record.state,&state,sizeof(state))==0);
    ASSERT_TRUE(present(view.gpu,err,sizeof(err)));
    dc_stream_destroy(view.stream);dc_gpu_destroy(view.gpu);dc_chunk_table_destroy(&view.table);free(chunk);PASS();
}
static void test_level_save_and_load_restore_bulk_liquid_channel(void) {
    char err[256]={0},directory[]="build/bulk_level_stream_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t view={0};dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_bulk_phase_chunk_t *bulk=calloc(1,sizeof(*bulk)),*out=calloc(1,sizeof(*out));
    ASSERT_TRUE(chunk&&bulk&&out);
    chunk->coord=(dc_chunk_coord_t){-5,-7};chunk->cells[17].fluid_mass=10000;
    bulk->oil_fp[17]=20000;bulk->slime_fp[17]=10000;
    bulk->oil_retained_fp[17]=1234;bulk->slime_retained_fp[17]=4321;
    ASSERT_TRUE(context(&view.gpu,err,sizeof(err))&&dc_chunk_table_init(&view.table,DC_GPU_CHUNK_SLOTS));
    view.stream=dc_stream_create(directory,123,4);ASSERT_TRUE(view.stream);
    snprintf(view.body_path,sizeof(view.body_path),"%s/rigid_bodies.bin",directory);
    uint32_t index=0;uint64_t generation=0;
    ASSERT_TRUE(dc_chunk_table_begin_load(&view.table,chunk->coord,&index,&generation)&&
        dc_gpu_upload_chunk(view.gpu,index,chunk,err,sizeof(err))&&
        dc_gpu_set_page(view.gpu,0,0,index,err,sizeof(err))&&
        dc_chunk_table_finish_load(&view.table,index,generation)&&
        fr_bulk_phase_gpu_import_chunk(view.gpu,index,chunk,bulk,err,sizeof(err))&&
        dc_chunk_table_mark_dirty(&view.table,index)&&
        dc_chunk_table_set_active(&view.table,index,false)&&
        schedule_saves(&view,err,sizeof(err)));
    dc_stream_result_t result={0};ASSERT_TRUE(receive(view.stream,&result));
    ASSERT_EQ(result.kind,DC_STREAM_SAVED);
    ASSERT_TRUE(dc_chunk_table_finish_save(&view.table,index,result.generation));
    dc_stream_result_release(&result);
    ASSERT_TRUE(dc_stream_request_load(view.stream,chunk->coord,generation)&&
        receive(view.stream,&result));
    ASSERT_EQ(result.kind,DC_STREAM_LOADED);ASSERT_TRUE(result.bulk);
    ASSERT_INT_EQ(result.bulk->oil_fp[17],20000);
    dc_stream_result_release(&result);
    ASSERT_TRUE(dc_gpu_upload_chunk(view.gpu,index,chunk,err,sizeof(err)));
    view.table.slots[index].state=DC_SLOT_LOADING;++view.table.slots[index].generation;
    ASSERT_TRUE(dc_stream_request_load(view.stream,chunk->coord,view.table.slots[index].generation));
    uint64_t deadline=SDL_GetTicks64()+5000u;
    while(view.table.slots[index].state==DC_SLOT_LOADING&&SDL_GetTicks64()<deadline) {
        ASSERT_TRUE(process_results(&view,err,sizeof(err)));SDL_Delay(1);
    }
    ASSERT_EQ(view.table.slots[index].state,DC_SLOT_ACTIVE);
    ASSERT_TRUE(fr_bulk_phase_gpu_export_chunk(view.gpu,index,out,err,sizeof(err)));
    ASSERT_EQ(memcmp(out,bulk,sizeof(*out)),0);
    dc_stream_destroy(view.stream);dc_gpu_destroy(view.gpu);dc_chunk_table_destroy(&view.table);
    free(chunk);free(bulk);free(out);PASS();
}

static bool publish_timed_chunk(dc_level_view_t *view, dc_chunk_coord_t coord,
                                uint32_t *slot, char *err, uint32_t cap) {
    uint64_t generation;
    if (!dc_chunk_table_begin_load(&view->table, coord, slot, &generation) ||
        !dc_stream_request_load(view->stream, coord, generation)) return false;
    uint64_t deadline = SDL_GetTicks64() + 5000u;
    do {
        if (!process_results(view, err, cap)) return false;
        if (view->table.slots[*slot].state == DC_SLOT_ACTIVE) return true;
        SDL_Delay(1);
    } while (SDL_GetTicks64() < deadline);
    return false;
}

static bool structure_time_gpu(dc_gpu_t **gpu, char *err, uint32_t cap) {
    return dc_gpu_create(gpu, 64, 64, "build/shaders/pattern.comp.spv", err, cap);
}

static void test_staged_structure_reopen_retains_times_when_pending_slots_move(void) {
    char directory[] = "build/structure_level_time_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t view = {0};
    view.save = dc_world_save_open(directory, err, sizeof(err));
    view.stream = dc_stream_create(directory, 42, 4);
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(view.save && view.stream && chunk);
    ASSERT_TRUE(dc_world_save_begin(view.save, err, sizeof(err)));
    const char *staging = dc_world_save_directory(view.save, true);
    ASSERT_TRUE(dc_stream_stage(view.stream, staging));
    ASSERT_TRUE(structure_time_gpu(&view.gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_init(&view.table, DC_GPU_CHUNK_SLOTS));
    const dc_chunk_coord_t coords[] = {{-7, 2}, {4, 5}, {9, -6}};
    const uint16_t materials[] = {DC_MATERIAL_STONE, DC_MATERIAL_WOOD, DC_MATERIAL_DENSE_WOOD};
    const dc_chunk_time_t times[] = {
        {.known = true}, {.seconds = 15.5, .known = true}, {.known = false}};
    for (unsigned i = 0; i < 3u; ++i) {
        uint32_t slot;
        uint64_t generation;
        chunk->coord = coords[i];
        chunk->cells[0].material = materials[i];
        ASSERT_TRUE(dc_chunk_table_begin_load(&view.table, coords[i], &slot, &generation));
        ASSERT_TRUE(dc_gpu_upload_chunk(view.gpu, slot, chunk, err, sizeof(err)));
        ASSERT_TRUE(dc_chunk_table_finish_load(&view.table, slot, generation));
        view.table.slots[slot].time = times[i];

        chunk->cells[0].material = DC_MATERIAL_AIR;
        const dc_stream_chunk_time_t wrong = {.seconds = 99.0, .known = true};
        ASSERT_TRUE(dc_stream_request_save_snapshot(view.stream, chunk, NULL, NULL, NULL,
                                                    NULL, NULL, wrong, generation));
    }
    for (unsigned i = 0; i < 3u; ++i) {
        dc_stream_result_t result = {0};
        ASSERT_TRUE(receive(view.stream, &result));
        ASSERT_EQ(result.kind, DC_STREAM_SAVED);
        dc_stream_result_release(&result);
    }
    char path[256], resaved[256];
    snprintf(path, sizeof(path), "%s/%s", staging, DC_WORLD_BODY_RECORD);
    ASSERT_TRUE(fr_gpu_save_structure_with_chunk_times(view.gpu, path, &view.table, err, sizeof(err)));
    ASSERT_TRUE(dc_player_inventory_stage(view.save, view.saved_inventory, err, sizeof(err)));
    ASSERT_TRUE(dc_world_save_commit(view.save, err, sizeof(err)));
    dc_stream_destroy(view.stream);
    dc_gpu_destroy(view.gpu);
    dc_chunk_table_destroy(&view.table);
    dc_world_save_close(view.save);

    view = (dc_level_view_t){0};
    view.save = dc_world_save_open(directory, err, sizeof(err));
    view.stream = dc_stream_create(directory, 42, 4);
    ASSERT_TRUE(view.save && view.stream && dc_chunk_table_init(&view.table, DC_GPU_CHUNK_SLOTS));
    ASSERT_TRUE(structure_time_gpu(&view.gpu, err, sizeof(err)));
    snprintf(path, sizeof(path), "%s/%s", directory, DC_WORLD_BODY_RECORD);
    ASSERT_TRUE(fr_gpu_load_structure(view.gpu, path, err, sizeof(err)));
    uint32_t slot;
    ASSERT_TRUE(publish_timed_chunk(&view, coords[1], &slot, err, sizeof(err)));
    ASSERT_TRUE(view.table.slots[slot].time.known && view.table.slots[slot].time.seconds == 15.5);

    snprintf(resaved, sizeof(resaved), "%s/resaved.dcs", directory);
    ASSERT_TRUE(fr_gpu_save_structure_with_chunk_times(view.gpu, resaved, &view.table, err, sizeof(err)));
    ASSERT_EQ(dc_gpu_validation_errors(view.gpu), 0u);
    dc_gpu_destroy(view.gpu);
    dc_chunk_table_destroy(&view.table);
    view.gpu = NULL;
    ASSERT_TRUE(structure_time_gpu(&view.gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_init(&view.table, DC_GPU_CHUNK_SLOTS));
    ASSERT_TRUE(fr_gpu_load_structure(view.gpu, resaved, err, sizeof(err)));
    for (unsigned i = 0; i < 3u; ++i) {
        ASSERT_TRUE(publish_timed_chunk(&view, coords[i], &slot, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_download_chunk(view.gpu, slot, chunk, err, sizeof(err)));
        ASSERT_EQ(chunk->cells[0].material, materials[i]);
        dc_chunk_time_t restored_time = view.table.slots[slot].time;
        ASSERT_EQ(restored_time.known, times[i].known);
        ASSERT_EQ(restored_time.seconds, times[i].seconds);
    }
    printf("Staged checkpoint recovered chunk times 0 and 15.5 s plus an unknown after resave\n");
    ASSERT_EQ(dc_gpu_validation_errors(view.gpu), 0u);
    dc_stream_destroy(view.stream);
    dc_gpu_destroy(view.gpu);
    dc_chunk_table_destroy(&view.table);
    dc_world_save_close(view.save);
    free(chunk);
    PASS();
}

int main(void) {
    if (!getenv("DC_TEST_STRUCTURE_TIME_ONLY")) {
        RUN(test_level_save_and_load_publish_the_same_reactive_worker_channel);
        RUN(test_level_save_and_load_restore_bulk_liquid_channel);
    }
    RUN(test_staged_structure_reopen_retains_times_when_pending_slots_move);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
