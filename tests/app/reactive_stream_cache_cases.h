#ifndef DC_REACTIVE_STREAM_CACHE_CASES_H
#define DC_REACTIVE_STREAM_CACHE_CASES_H

static bool rewrite_parked_source(dc_streamer_t *writer, const dc_stream_result_t *source,
                                 dc_chunk_owner_t owner) {
    dc_stream_result_t reply = {0};
    bool okay = dc_stream_request_save_takeover(writer, source, owner, 37u) &&
        receive(writer, &reply) && reply.kind == DC_STREAM_SAVED && dc_chunk_owner_equal(reply.replaces_owner, owner);
    dc_stream_result_release(&reply);
    return okay;
}

static void check_sleeping_slot_replay(bool transfer) {
    char directory[] = "build/camera_sleeping_reversal_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = camera_test_view(directory, false, err, sizeof(err));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(view && chunk);
    dc_chunk_coord_t coord = {view->origin.x - HALO_CHUNKS - 1, view->origin.y};
    dc_chunk_owner_t owner = {13u,7u};
    dc_chunk_time_t time = {view->world_seconds - .0625, true};
    camera_seal_room(chunk, coord);
    unsigned center = 32u * DC_CHUNK_SIDE + 32u;
    chunk->cells[center].fluid_mass = 14000u;
    dc_stream_result_t source = {.kind=DC_STREAM_LOADED,.coord=coord,.chunk=chunk,
        .generation=1u,.time=time};
    ASSERT_TRUE(rewrite_parked_source(view->stream, &source, owner));
    uint32_t slot;
    uint64_t generation;
    ASSERT_TRUE(dc_chunk_table_begin_load(&view->table, coord, &slot, &generation) &&
        dc_gpu_upload_chunk(view->gpu, slot, chunk, err, sizeof(err)) &&
        dc_chunk_table_finish_load(&view->table, slot, generation) &&
        dc_chunk_table_set_active(&view->table, slot, false));
    view->table.slots[slot].time = time;
    view->table.slots[slot].checkpoint_owner = owner;
    view->camera_offscreen = (dc_camera_offscreen_t){.coord=coord,.owner=owner,
        .phase=DC_CAMERA_OFFSCREEN_STORED,.time=time};
    uint32_t neighbor_slot = 0;
    if (transfer) {
        dc_chunk_coord_t neighbor = {coord.x + 1, coord.y};
        *chunk = (dc_chunk_t){.coord=neighbor};
        chunk->cells[32u * DC_CHUNK_SIDE].fluid_mass = 100u;
        ASSERT_TRUE(dc_chunk_table_begin_load(&view->table, neighbor, &neighbor_slot, &generation) &&
            dc_gpu_upload_chunk(view->gpu, neighbor_slot, chunk, err, sizeof(err)) &&
            dc_chunk_table_finish_load(&view->table, neighbor_slot, generation));
        view->table.slots[neighbor_slot].time = (dc_chunk_time_t){view->world_seconds,true};
        int64_t x = neighbor.x * DC_CHUNK_SIDE, y = neighbor.y * DC_CHUNK_SIDE + 32;
        ASSERT_TRUE(dc_level_view_queue_transfer(view,x,y,x-1,y,1u,DC_GPU_TRANSFER_SCALAR,err,sizeof(err)));
        ASSERT_TRUE(view->transfer_pending && !view->destination_bound);
    }
    ASSERT_TRUE(dc_level_view_move(view,-1,0) &&
        dc_level_view_wait_visible(view,30000u,err,sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_find(&view->table, coord, &slot));
    ASSERT_EQ(view->table.slots[slot].time.seconds, view->world_seconds);
    ASSERT_TRUE(dc_gpu_download_chunk(view->gpu,slot,chunk,err,sizeof(err)));
    ASSERT_EQ(camera_stock(chunk,NULL,false),14000u);
    ASSERT_TRUE(chunk->cells[center].fluid_mass < 14000u);
    if (transfer) {
        dc_gpu_transfer_state_t state;
        ASSERT_TRUE(dc_level_view_transfer_result(view,&state));
        ASSERT_EQ(state,DC_GPU_TRANSFER_BLOCKED);
        ASSERT_TRUE(!view->table.slots[neighbor_slot].pinned && !view->table.slots[slot].pinned);
        ASSERT_TRUE(dc_gpu_download_chunk(view->gpu,neighbor_slot,chunk,err,sizeof(err)));
        ASSERT_EQ(camera_stock(chunk,NULL,false),100u);
    }
    dc_level_view_discard(view);
    free(chunk);
    PASS();
}

static void test_reversal_replays_a_sleeping_slot_before_residency(void) {
    check_sleeping_slot_replay(false);
}

static void test_world_transfer_resolves_against_replayed_room(void) {
    check_sleeping_slot_replay(true);
}

static void check_two_evicted_sealed_rooms(bool sleep_and_prefetch, bool check_cadence, bool reopen) {
    char directory[] = "build/camera_two_rooms_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = camera_test_view(directory, false, err, sizeof(err));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(view && chunk);
    dc_chunk_coord_t rooms[] = {view->origin, {view->origin.x + 1, view->origin.y + 1}};
    dc_offscreen_t *reference[2] = {0};
    dc_stream_result_t sources[2] = {0};
    dc_streamer_t *writer = NULL;
    uint32_t center = 32u * DC_CHUNK_SIDE + 32u;
    for (unsigned i = 0; i < 2u; ++i) {
        uint32_t slot;
        uint64_t generation;
        camera_seal_room(chunk, rooms[i]);
        chunk->cells[center].fluid_mass = 14000u + i;
        if (reopen) {
            chunk->marker_count = 1u;
            chunk->markers[0] = (dc_marker_t){32 * DC_FLUID_FULL, 32 * DC_FLUID_FULL, 501u+i, DC_MARKER_INSIDE};
        }
        ASSERT_TRUE(dc_chunk_table_begin_load(&view->table, rooms[i], &slot, &generation) &&
            dc_gpu_upload_chunk(view->gpu, slot, chunk, err, sizeof(err)) &&
            dc_chunk_table_finish_load(&view->table, slot, generation) &&
            dc_chunk_table_mark_dirty(&view->table, slot));
        if (reopen) {
            fr_bulk_phase_chunk_t *bulk = calloc(1, sizeof(*bulk));
            dc_chunk_colors_t *colors = calloc(1, sizeof(*colors));
            ASSERT_TRUE(bulk && colors);
            bulk->oil_fp[center] = 2000u+i;
            bulk->slime_fp[center] = 500u+i;
            colors->water[center][1] = .375f;
            colors->water[center][3] = 1.f;
            ASSERT_TRUE(fr_bulk_phase_gpu_import_chunk(view->gpu, slot, chunk, bulk, err, sizeof(err)) &&
                fr_renderer_upload_chunk_colors(view->gpu, slot, colors, err, sizeof(err)));
            free(colors);
            free(bulk);
        }
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
        if (reopen) {
            ASSERT_TRUE(dc_level_view_step_timed(view, .03125f, err, sizeof(err)) &&
                dc_level_view_tick(view, err, sizeof(err)));
            dc_chunk_coord_t origin = view->origin;
            unsigned offset = view->camera_offset_x;
            char obstacle[256];
            snprintf(obstacle, sizeof(obstacle), "%s/%s", directory, DC_WORLD_ROOM_RECORD);
            ASSERT_EQ(mkdir(obstacle, 0700), 0);
            ASSERT_TRUE(!dc_level_view_destroy(view, err, sizeof(err)));
            ASSERT_TRUE(strstr(err, "needs recovery"));
            ASSERT_TRUE(!dc_world_save_open(directory, err, sizeof(err)));
            ASSERT_EQ(rmdir(obstacle), 0);
            view = dc_level_view_create(directory, 42, err, sizeof(err));
            ASSERT_TRUE(view && view->world_seconds == 2.03125);
            for (unsigned i = 0; i < 2u; ++i) {
                ASSERT_TRUE(dc_stream_request_load(view->stream, rooms[i], i + 1u) &&
                    receive(view->stream, &sources[i]) && sources[i].kind == DC_STREAM_LOADED);
                reference[i] = dc_offscreen_create(view->gpu, rooms[i], err, sizeof(err));
                ASSERT_TRUE(reference[i] && dc_offscreen_admit(reference[i], &sources[i], i + 1u, err, sizeof(err)));
            }
            writer = dc_stream_create(dc_world_save_directory(view->save, true), 42, 4);
            dc_chunk_owner_t stale = sources[0].replaces_owner;
            ++stale.serial;
            ASSERT_TRUE(writer && rewrite_parked_source(writer, &sources[0], stale));
            while (view->origin.x < origin.x)
                ASSERT_TRUE(dc_level_view_move(view, 1, 0));
            while (view->origin.y > origin.y)
                ASSERT_TRUE(dc_level_view_move(view, 0, -1));
            ASSERT_TRUE(dc_level_view_pan_pixels(view, (int)offset, 0));
        }
        ASSERT_TRUE(dc_level_view_pan_pixels(view, -2 * VIEW_WIDTH, 0) &&
            dc_level_view_pan_pixels(view, -VIEW_WIDTH, 0));
        if (reopen) {
            uint64_t until = SDL_GetTicks64() + 30000u;
            bool rejected = false;
            do {
                rejected = !dc_level_view_tick(view, err, sizeof(err));
                if (!rejected) SDL_Delay(1);
            } while (!rejected && SDL_GetTicks64() < until);
            ASSERT_TRUE(rejected && strstr(err, "Offscreen eviction source changed"));
            ASSERT_EQ(view->world_seconds, 2.03125);
            ASSERT_TRUE(!dc_level_offscreen_owns(view, rooms[0]));
            ASSERT_TRUE(rewrite_parked_source(writer, &sources[0], sources[0].replaces_owner));
        }
        ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
        ASSERT_TRUE(!dc_level_chunk_visible(view, rooms[0]) && !dc_level_chunk_visible(view, rooms[1]));
        ASSERT_TRUE(dc_level_offscreen_owns(view, rooms[0]) && dc_level_offscreen_owns(view, rooms[1]));
    }
    ASSERT_TRUE(dc_level_view_step_timed(view, .0625f, err, sizeof(err)) &&
        dc_level_view_tick(view, err, sizeof(err)) &&
        dc_level_view_pan_pixels(view, -2 * VIEW_WIDTH, 0) &&
        dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    for (unsigned i = 0; i < 2u && reopen; ++i) {
        ASSERT_TRUE(dc_offscreen_advance(reference[i], view->world_seconds, 16u, err, sizeof(err)));
        ASSERT_EQ(reference[i]->time.seconds, view->world_seconds);
    }
    for (unsigned i = 0; i < 2u; ++i) {
        uint32_t slot;
        ASSERT_TRUE(!dc_level_offscreen_owns(view, rooms[i]) &&
            dc_chunk_table_find(&view->table, rooms[i], &slot) &&
            dc_gpu_download_chunk(view->gpu, slot, chunk, err, sizeof(err)));
        ASSERT_EQ(view->table.slots[slot].time.seconds, check_cadence ? 2.3125 : reopen ? 2.09375 : 2.0625);
        ASSERT_EQ(camera_stock(chunk, NULL, false), 14000u + i);
        ASSERT_TRUE(chunk->cells[center].fluid_mass < 14000u + i);
        if (reopen) {
            dc_stream_result_t expected = {0};
            ASSERT_TRUE(dc_offscreen_snapshot(reference[i], rooms[i], i + 1u, &expected, err, sizeof(err)));
            dc_stream_result_t published = {0};
            ASSERT_TRUE(dc_stream_request_load(writer, rooms[i], i+1u) && receive(writer, &published) &&
                published.kind == DC_STREAM_LOADED && published.time.seconds == expected.time.seconds &&
                dc_chunk_owner_equal(published.replaces_owner, sources[i].replaces_owner));
            ASSERT_EQ(memcmp(expected.chunk->cells, published.chunk->cells, sizeof(chunk->cells)), 0);
            for (unsigned cell = 0; cell < DC_CHUNK_CELLS; ++cell) {
                dc_face_velocity_t a = expected.chunk->face_velocity[cell], b = published.chunk->face_velocity[cell];
                if (a.x != b.x || a.y != b.y) {
                    snprintf(err,sizeof(err),"room %u cell %u expected velocity (%.9g,%.9g), published (%.9g,%.9g)",
                        i,cell,a.x,a.y,b.x,b.y);
                    ASSERT_TRUE(false);
                }
            }
            ASSERT_EQ(memcmp(expected.chunk->face_velocity, chunk->face_velocity, sizeof(chunk->face_velocity)), 0);
            ASSERT_EQ(expected.chunk->marker_count, chunk->marker_count);
            ASSERT_EQ(memcmp(expected.chunk->markers, chunk->markers, chunk->marker_count*sizeof(dc_marker_t)), 0);
            ASSERT_EQ(expected.chunk->particle_count, chunk->particle_count);
            ASSERT_TRUE(published.bulk && published.colors);
            ASSERT_EQ(memcmp(expected.bulk, published.bulk, sizeof(*expected.bulk)), 0);
            ASSERT_EQ(memcmp(expected.colors, published.colors, sizeof(*expected.colors)), 0);
            for (unsigned cell = 0; cell < DC_CHUNK_CELLS; ++cell) {
                ASSERT_EQ(chunk->cells[cell].material, published.chunk->cells[cell].material);
                ASSERT_EQ(chunk->cells[cell].fluid_mass, published.chunk->cells[cell].fluid_mass);
            }
            dc_stream_result_release(&published);
            dc_stream_result_release(&expected);
            dc_stream_result_release(&sources[i]);
        }
    }
    if (writer) dc_stream_destroy(writer);
    for (unsigned i = 0; i < 2u; ++i) dc_offscreen_destroy(reference[i]);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    if (reopen) {
        dc_level_view_t cold = {0};
        cold.save = dc_world_save_open(directory, err, sizeof(err));
        ASSERT_TRUE(cold.save && dc_world_save_load_clock(cold.save, &cold.world_seconds, err, sizeof(err)) &&
            dc_camera_catalog_load(&cold, err, sizeof(err)));
        ASSERT_TRUE(!dc_camera_catalog_active(&cold));
        dc_world_save_close(cold.save);
    }
    free(chunk);
    PASS();
}

static void test_two_evicted_sealed_rooms_return_at_the_world_clock(void) {
    check_two_evicted_sealed_rooms(false, false, false);
}

static void test_distant_rooms_sleep_and_prefetch_before_camera_return(void) {
    check_two_evicted_sealed_rooms(true, false, false);
}

static void test_parked_rooms_reopen_from_confirmed_epochs(void) {
    check_two_evicted_sealed_rooms(true, false, true);
}

static void test_middle_band_finishes_its_due_interval(void) {
    check_two_evicted_sealed_rooms(false, true, false);
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
