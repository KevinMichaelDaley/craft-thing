#ifndef DC_REACTIVE_STREAM_CACHE_CASES_H
#define DC_REACTIVE_STREAM_CACHE_CASES_H

static void check_two_evicted_sealed_rooms(bool sleep_and_prefetch, bool check_cadence) {
    char directory[] = "build/camera_two_rooms_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = camera_test_view(directory, false, err, sizeof(err));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(view && chunk);
    dc_chunk_coord_t rooms[] = {view->origin, {view->origin.x + 1, view->origin.y + 1}};
    uint32_t center = 32u * DC_CHUNK_SIDE + 32u;
    for (unsigned i = 0; i < 2u; ++i) {
        uint32_t slot;
        uint64_t generation;
        camera_seal_room(chunk, rooms[i]);
        chunk->cells[center].fluid_mass = 14000u + i;
        ASSERT_TRUE(dc_chunk_table_begin_load(&view->table, rooms[i], &slot, &generation) &&
            dc_gpu_upload_chunk(view->gpu, slot, chunk, err, sizeof(err)) &&
            dc_chunk_table_finish_load(&view->table, slot, generation) &&
            dc_chunk_table_mark_dirty(&view->table, slot));
        view->table.slots[slot].time = (dc_chunk_time_t){view->world_seconds, true};
    }
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)) &&
        dc_level_view_pan_pixels(view, 2 * VIEW_WIDTH, 0) &&
        dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(dc_level_offscreen_owns(view, rooms[0]));
    ASSERT_TRUE(dc_level_offscreen_owns(view, rooms[1]));
    if (check_cadence) {
        uint64_t deadline = SDL_GetTicks64() + 25000u;
        dc_level_offscreen_handoff_t *lease = dc_level_offscreen_lookup(view, rooms[1]);
        while (lease && !lease->admitted && SDL_GetTicks64() < deadline) {
            ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
            SDL_Delay(1);
        }
        ASSERT_TRUE(lease && lease->admitted);
        ASSERT_TRUE(dc_level_view_step_timed(view, .125f, err, sizeof(err)) &&
            dc_level_view_tick(view, err, sizeof(err)) && dc_level_view_tick(view, err, sizeof(err)));
        ASSERT_EQ(view->world_seconds, 2.125);
        ASSERT_EQ(lease->workspace->time.seconds, 2.);
        ASSERT_TRUE(dc_level_view_step_timed(view, .125f, err, sizeof(err)) &&
            dc_level_view_tick(view, err, sizeof(err)));
        for (unsigned i = 0; i < 4u; ++i) ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
        ASSERT_EQ(view->world_seconds, 2.25);
        ASSERT_EQ(lease->workspace->time.seconds, view->world_seconds);
    }
    if (sleep_and_prefetch) {
        ASSERT_TRUE(dc_level_view_pan_pixels(view, 2 * VIEW_WIDTH, 0) &&
            dc_level_view_pan_pixels(view, VIEW_WIDTH, 0) &&
            dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
        uint64_t deadline = SDL_GetTicks64() + 5000u;
        while ((dc_level_offscreen_owns(view, rooms[0]) || dc_level_offscreen_owns(view, rooms[1])) &&
               SDL_GetTicks64() < deadline) {
            ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
            SDL_Delay(1);
        }
        ASSERT_TRUE(!dc_level_offscreen_owns(view, rooms[0]) && !dc_level_offscreen_owns(view, rooms[1]));
        ASSERT_TRUE(dc_level_view_pan_pixels(view, -2 * VIEW_WIDTH, 0) &&
            dc_level_view_pan_pixels(view, -VIEW_WIDTH, 0) &&
            dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
        ASSERT_TRUE(!dc_level_chunk_visible(view, rooms[0]) && !dc_level_chunk_visible(view, rooms[1]));
        ASSERT_TRUE(dc_level_offscreen_owns(view, rooms[0]) && dc_level_offscreen_owns(view, rooms[1]));
    }
    ASSERT_TRUE(dc_level_view_step_timed(view, .0625f, err, sizeof(err)) &&
        dc_level_view_tick(view, err, sizeof(err)) &&
        dc_level_view_pan_pixels(view, -2 * VIEW_WIDTH, 0) &&
        dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    for (unsigned i = 0; i < 2u; ++i) {
        uint32_t slot;
        ASSERT_TRUE(!dc_level_offscreen_owns(view, rooms[i]) &&
            dc_chunk_table_find(&view->table, rooms[i], &slot) &&
            dc_gpu_download_chunk(view->gpu, slot, chunk, err, sizeof(err)));
        ASSERT_EQ(view->table.slots[slot].time.seconds, check_cadence ? 2.3125 : 2.0625);
        ASSERT_EQ(camera_stock(chunk, NULL, false), 14000u + i);
        ASSERT_TRUE(chunk->cells[center].fluid_mass < 14000u + i);
    }
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    free(chunk);
    PASS();
}

static void test_two_evicted_sealed_rooms_return_at_the_world_clock(void) {
    check_two_evicted_sealed_rooms(false, false);
}

static void test_distant_rooms_sleep_and_prefetch_before_camera_return(void) {
    check_two_evicted_sealed_rooms(true, false);
}

static void test_middle_band_finishes_its_due_interval(void) {
    check_two_evicted_sealed_rooms(false, true);
}

static void test_saturated_sleeping_rooms_all_return_at_the_world_clock(void) {
    enum { ROOM_COUNT = 17u };
    char directory[] = "build/camera_saturated_rooms_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = camera_test_view(directory, false, err, sizeof(err));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(view && chunk);
    dc_chunk_coord_t rooms[ROOM_COUNT];
    unsigned center = 32u * DC_CHUNK_SIDE + 32u;
    for (unsigned i = 0; i < ROOM_COUNT; ++i) {
        rooms[i] = (dc_chunk_coord_t){view->origin.x + (int)(i % 5u), view->origin.y + (int)(i / 5u)};
        camera_seal_room(chunk, rooms[i]);
        chunk->cells[center].fluid_mass = 14000u + i;
        uint32_t slot;
        uint64_t generation;
        ASSERT_TRUE(dc_chunk_table_begin_load(&view->table, rooms[i], &slot, &generation) &&
            dc_gpu_upload_chunk(view->gpu, slot, chunk, err, sizeof(err)) &&
            dc_chunk_table_finish_load(&view->table, slot, generation) &&
            dc_chunk_table_mark_dirty(&view->table, slot));
        view->table.slots[slot].time = (dc_chunk_time_t){view->world_seconds, true};
    }
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    for (unsigned i = 0; i < 3u; ++i) ASSERT_TRUE(dc_level_view_pan_pixels(view, 2 * VIEW_WIDTH, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    for (unsigned i = 0; i < ROOM_COUNT; ++i) ASSERT_TRUE(!dc_level_offscreen_owns(view, rooms[i]));
    ASSERT_TRUE(dc_level_view_step_timed(view, .03125f, err, sizeof(err)) &&
        dc_level_view_tick(view, err, sizeof(err)));
    for (unsigned i = 0; i < 3u; ++i) ASSERT_TRUE(dc_level_view_pan_pixels(view, -2 * VIEW_WIDTH, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000u, err, sizeof(err)));
    for (unsigned i = 0; i < ROOM_COUNT; ++i) {
        uint32_t slot;
        ASSERT_TRUE(dc_chunk_table_find(&view->table, rooms[i], &slot) &&
            dc_gpu_download_chunk(view->gpu, slot, chunk, err, sizeof(err)));
        ASSERT_EQ(view->table.slots[slot].time.seconds, 2.03125);
        ASSERT_EQ(camera_stock(chunk, NULL, false), 14000u + i);
        ASSERT_TRUE(chunk->cells[center].fluid_mass < 14000u + i);
    }
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    free(chunk);
    PASS();
}

static void test_camera_keeps_rooms_on_opposite_sides_of_the_view(void) {
    char directory[] = "build/camera_opposite_rooms_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = camera_test_view(directory, false, err, sizeof(err));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(view && chunk && dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    dc_chunk_coord_t rooms[2];
    for (unsigned i = 0; i < 2u; ++i) {
        rooms[i] = view->origin;
        if (i) rooms[i].x += VIEW_CHUNKS_X - 1u;
        uint32_t slot;
        ASSERT_TRUE(dc_chunk_table_find(&view->table, rooms[i], &slot));
        camera_seal_room(chunk, rooms[i]);
        chunk->cells[32u * DC_CHUNK_SIDE + 32u].fluid_mass = 12000u + i;
        ASSERT_TRUE(dc_gpu_upload_chunk(view->gpu, slot, chunk, err, sizeof(err)) &&
            dc_chunk_table_mark_dirty(&view->table, slot));
        ASSERT_TRUE(dc_level_view_pan_pixels(view, i ? -VIEW_WIDTH : 2 * VIEW_WIDTH, 0) &&
            dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    }
    ASSERT_TRUE(rooms[0].x < view->origin.x && rooms[1].x >= view->origin.x + VIEW_CHUNKS_X);
    ASSERT_TRUE(dc_level_offscreen_owns(view, rooms[0]) && dc_level_offscreen_owns(view, rooms[1]));
    ASSERT_TRUE(dc_level_view_step_timed(view, .0625f, err, sizeof(err)) &&
        dc_level_view_tick(view, err, sizeof(err)));
    for (unsigned i = 0; i < 2u; ++i) {
        uint32_t slot;
        ASSERT_TRUE(dc_level_view_pan_pixels(view, i ? 2 * VIEW_WIDTH : -VIEW_WIDTH, 0) &&
            dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
        ASSERT_TRUE(dc_chunk_table_find(&view->table, rooms[i], &slot) &&
            dc_gpu_download_chunk(view->gpu, slot, chunk, err, sizeof(err)));
        ASSERT_EQ(view->table.slots[slot].time.seconds, 2.0625);
        ASSERT_EQ(camera_stock(chunk, NULL, false), 12000u + i);
    }
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    free(chunk);
    PASS();
}

#endif
