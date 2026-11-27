#ifndef DC_REACTIVE_STREAM_CAMERA_CASES_H
#define DC_REACTIVE_STREAM_CAMERA_CASES_H

static void test_camera_eviction_keeps_the_published_owner_across_archive_rewrites(void) {
    char directory[] = "build/camera_durable_owner_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t view = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk && structure_time_gpu(&view.gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(view.gpu));
    ASSERT_TRUE(dc_chunk_table_init(&view.table, DC_GPU_CHUNK_SLOTS));
    view.stream = dc_stream_create(directory, 42, 4);
    ASSERT_TRUE(view.stream);
    snprintf(view.body_path, sizeof(view.body_path), "%s/%s", directory, DC_WORLD_BODY_RECORD);

    const dc_chunk_coord_t coord = {-9, 7};
    chunk->coord = coord;
    chunk->cells[23].fluid_mass = 14000;
    uint32_t slot;
    uint64_t generation;
    ASSERT_TRUE(dc_chunk_table_begin_load(&view.table, coord, &slot, &generation));
    ASSERT_TRUE(dc_gpu_upload_chunk(view.gpu, slot, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_finish_load(&view.table, slot, generation));
    view.table.slots[slot].time = (dc_chunk_time_t){2., true};
    ASSERT_TRUE(dc_chunk_table_mark_dirty(&view.table, slot));
    ASSERT_TRUE(dc_chunk_table_set_active(&view.table, slot, false));
    ASSERT_TRUE(schedule_saves(&view, err, sizeof(err)));
    ASSERT_EQ(view.table.slots[slot].state, DC_SLOT_SAVING);
    ASSERT_TRUE(save_level_structure(&view, err, sizeof(err)));

    dc_stream_result_t reply = {0};
    ASSERT_TRUE(receive(view.stream, &reply));
    ASSERT_EQ(reply.kind, DC_STREAM_SAVED);
    dc_chunk_owner_t written = reply.replaces_owner;
    dc_stream_result_release(&reply);
    ASSERT_TRUE(dc_stream_request_load(view.stream, coord, generation));
    ASSERT_TRUE(receive(view.stream, &reply));
    ASSERT_EQ(reply.kind, DC_STREAM_LOADED);
    ASSERT_EQ(reply.chunk->cells[23].fluid_mass, 14000u);
    ASSERT_TRUE(reply.time.known && reply.time.seconds == 2.);
    ASSERT_TRUE(dc_chunk_owner_equal(written, reply.replaces_owner));
    dc_stream_result_release(&reply);
    dc_stream_destroy(view.stream);
    dc_gpu_destroy(view.gpu);
    dc_chunk_table_destroy(&view.table);
    free(chunk);

    view.gpu = NULL;
    ASSERT_TRUE(structure_time_gpu(&view.gpu, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_load_structure(view.gpu, view.body_path, err, sizeof(err)));
    ASSERT_TRUE(view.gpu->checkpoint_chunk_count == 1 && view.gpu->checkpoint_owners);
    const dc_chunk_owner_t archive = view.gpu->checkpoint_owners[0];
    ASSERT_EQ(dc_gpu_validation_errors(view.gpu), 0u);
    dc_gpu_destroy(view.gpu);
    ASSERT_TRUE(dc_chunk_owner_valid(written));
    ASSERT_TRUE(dc_chunk_owner_equal(written, archive));
    PASS();
}

static dc_level_view_t *camera_test_view(const char *directory, bool restored, char *err, uint32_t cap) {
    dc_level_view_t *view = calloc(1, sizeof(*view));
    if (!view) return NULL;
    view->origin = view->mapped_origin = (dc_chunk_coord_t){0, -20};
    view->world_seconds = 2.;
    view->save = dc_world_save_open(directory, err, cap);
    view->stream = dc_stream_create(directory, 42, DC_GPU_CHUNK_SLOTS * 2u);
    snprintf(view->body_path, sizeof(view->body_path), "%s/%s", directory, DC_WORLD_BODY_RECORD);
    bool okay = view->save && view->stream && context(&view->gpu, err, cap) &&
        dc_gpu_set_viewport(view->gpu, DC_CHUNK_SIDE, DC_CHUNK_SIDE, VIEW_WIDTH, VIEW_HEIGHT) &&
        dc_chunk_table_init(&view->table, DC_GPU_CHUNK_SLOTS) && dc_gpu_enable_automatic_components(view->gpu);
    if (okay && restored) okay = dc_world_save_load_clock(view->save, &view->world_seconds, err, cap) &&
        fr_gpu_load_structure(view->gpu, view->body_path, err, cap);
    if (okay) return view;
    dc_level_view_discard(view);
    return NULL;
}

static uint64_t camera_stock(const dc_chunk_t *chunk, const fr_bulk_phase_chunk_t *bulk, bool oil) {
    uint64_t total = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
        total += oil ? bulk->oil_fp[i] : chunk->cells[i].fluid_mass;
    return total;
}

static void test_two_screen_camera_pan_advances_and_reopens_its_evicted_owner(void) {
    char directory[] = "build/camera_offscreen_pan_XXXXXX", err[256] = {0}, temporary[256];
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = camera_test_view(directory, false, err, sizeof(err));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    fr_bulk_phase_chunk_t *bulk = calloc(1, sizeof(*bulk));
    ASSERT_TRUE(view && chunk && bulk);
    const dc_chunk_coord_t coord = view->origin;
    chunk->coord = coord;
    for (uint32_t y = 0; y < DC_CHUNK_SIDE; ++y) {
        for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x) {
            if (x && y && x + 1 < DC_CHUNK_SIDE && y + 1 < DC_CHUNK_SIDE) continue;
            chunk->cells[y * DC_CHUNK_SIDE + x] = (dc_cell_t){
                .material = DC_MATERIAL_STONE, .flags = DC_CELL_LOADED_TERRAIN};
        }
    }
    const uint32_t center = 32u * DC_CHUNK_SIDE + 32u;
    bulk->oil_fp[center] = 8000u;
    chunk->cells[center].fluid_mass = 14000u;
    uint32_t slot;
    uint64_t generation;
    ASSERT_TRUE(dc_chunk_table_begin_load(&view->table, coord, &slot, &generation));
    ASSERT_TRUE(dc_gpu_upload_chunk(view->gpu, slot, chunk, err, sizeof(err)));
    ASSERT_TRUE(fr_bulk_phase_gpu_import_chunk(view->gpu, slot, chunk, bulk, err, sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_finish_load(&view->table, slot, generation));
    view->table.slots[slot].time = (dc_chunk_time_t){2., true};
    ASSERT_TRUE(dc_chunk_table_mark_dirty(&view->table, slot));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));

    snprintf(temporary, sizeof(temporary), "%s/chunk_%016" PRIx64 "_%016" PRIx64 ".bin.tmp",
        directory, (uint64_t)coord.x, (uint64_t)coord.y);
    ASSERT_EQ(mkdir(temporary, 0700), 0);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 2 * VIEW_WIDTH, 0));
    ASSERT_TRUE(!dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(strstr(err, "Chunk worker load or save failed"));
    ASSERT_TRUE(dc_chunk_table_find(&view->table, coord, &slot));
    ASSERT_TRUE(view->table.slots[slot].dirty && !dc_level_offscreen_owns(view, coord));
    ASSERT_EQ(rmdir(temporary), 0);
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(!dc_chunk_table_find(&view->table, coord, NULL));
    ASSERT_TRUE(dc_level_offscreen_owns(view, coord));
    ASSERT_TRUE(dc_level_view_step_timed(view, .03125f, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_step_timed(view, .03125f, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_EQ(view->world_seconds, 2.0625);

    ASSERT_TRUE(dc_level_view_pan_pixels(view, -2 * VIEW_WIDTH, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_find(&view->table, coord, &slot));
    ASSERT_TRUE(!dc_level_offscreen_owns(view, coord));
    ASSERT_TRUE(view->table.slots[slot].time.known && view->table.slots[slot].time.seconds == 2.0625);
    ASSERT_TRUE(dc_gpu_download_chunk(view->gpu, slot, chunk, err, sizeof(err)));
    ASSERT_TRUE(fr_bulk_phase_gpu_export_chunk(view->gpu, slot, bulk, err, sizeof(err)));
    ASSERT_EQ(camera_stock(chunk, bulk, false), 14000u);
    ASSERT_EQ(camera_stock(chunk, bulk, true), 8000u);
    ASSERT_EQ(dc_gpu_validation_errors(view->gpu), 0u);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));

    view = camera_test_view(directory, true, err, sizeof(err));
    ASSERT_TRUE(view && dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_find(&view->table, coord, &slot));
    ASSERT_TRUE(view->table.slots[slot].time.known && view->table.slots[slot].time.seconds == 2.0625);
    ASSERT_TRUE(dc_gpu_download_chunk(view->gpu, slot, chunk, err, sizeof(err)));
    ASSERT_TRUE(fr_bulk_phase_gpu_export_chunk(view->gpu, slot, bulk, err, sizeof(err)));
    ASSERT_EQ(camera_stock(chunk, bulk, false), 14000u);
    ASSERT_EQ(camera_stock(chunk, bulk, true), 8000u);
    ASSERT_EQ(dc_gpu_validation_errors(view->gpu), 0u);
    dc_level_view_discard(view);
    free(bulk);
    free(chunk);
    PASS();
}

#endif
