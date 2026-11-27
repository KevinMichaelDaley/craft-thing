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

#endif
