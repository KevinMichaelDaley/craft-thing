#ifndef DC_REACTIVE_STREAM_BOND_CASES_H
#define DC_REACTIVE_STREAM_BOND_CASES_H

static bool camera_has_live_bond(dc_gpu_t *gpu, uint32_t first, uint32_t second) {
    const uint32_t *graph = dc_gpu_structural_data(gpu) + dc_gpu_structural_graph_offset(gpu);
    for (unsigned i = 0; i < 192u; ++i) {
        const uint32_t *bond = graph + 30000u + i * 20u;
        if (bond[0] && ((bond[3] == first && bond[4] == second) ||
                        (bond[3] == second && bond[4] == first)) &&
            bond[1] < dc_gpu_body_capacity(gpu) && bond[2] < dc_gpu_body_capacity(gpu) &&
            dc_gpu_body_record(gpu, bond[1])->body.id == bond[3] &&
            dc_gpu_body_record(gpu, bond[2])->body.id == bond[4]) return true;
    }
    return false;
}

static void test_camera_transfers_an_assembly_spanning_two_chunk_pages(void) {
    char directory[] = "build/camera_bonded_pan_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = camera_test_view(directory, false, err, sizeof(err));
    ASSERT_TRUE(view);
    const dc_chunk_coord_t origin = view->origin;
    for (unsigned tile = 0; tile < 2; ++tile) {
        dc_chunk_t chunk = {0};
        camera_seal_room(&chunk, (dc_chunk_coord_t){origin.x + tile, origin.y});
        unsigned seam = tile ? 0u : DC_CHUNK_SIDE - 1u;
        for (unsigned y = 1; y + 1 < DC_CHUNK_SIDE; ++y)
            chunk.cells[y * DC_CHUNK_SIDE + seam] = (dc_cell_t){0};
        uint32_t slot;
        uint64_t generation;
        ASSERT_TRUE(dc_chunk_table_begin_load(&view->table, chunk.coord, &slot, &generation));
        ASSERT_TRUE(dc_gpu_upload_chunk(view->gpu, slot, &chunk, err, sizeof(err)));
        ASSERT_TRUE(dc_chunk_table_finish_load(&view->table, slot, generation));
        view->table.slots[slot].time = (dc_chunk_time_t){view->world_seconds, true};
        ASSERT_TRUE(dc_chunk_table_mark_dirty(&view->table, slot));
    }
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    fr_structural_gpu_graph_t graph = {.node_count = 3u, .member_count = 2u};
    for (unsigned i = 0; i < 3; ++i) {
        int x = DC_CHUNK_SIDE - 6 + 6 * (int)i;
        graph.nodes[i] = (fr_structural_gpu_node_t){.id = i + 1u, .generation = 3u,
            .chunk = {origin.x + x / DC_CHUNK_SIDE, origin.y},
            .x_fp = (x % DC_CHUNK_SIDE) * DC_FLUID_FULL, .y_fp = 24 * DC_FLUID_FULL};
    }
    for (unsigned i = 0; i < 2; ++i)
        graph.members[i] = (fr_structural_gpu_member_t){.id = i + 1u, .generation = 3u,
            .a = i + 1u, .b = i + 2u, .material = i ? FR_STRUCTURAL_STONE : FR_STRUCTURAL_WOOD,
            .depth_fp = 2u * DC_FLUID_FULL};
    ASSERT_TRUE(fr_gpu_place_structure(view->gpu, &graph, err, sizeof(err)));
    for (unsigned i = 0; i < 90; ++i) ASSERT_TRUE(dc_gpu_rigid_step(view->gpu, err, sizeof(err)));
    const uint32_t first = FR_STRUCTURAL_BODY_ID_BASE + 1u;
    ASSERT_TRUE(camera_has_live_bond(view->gpu, first, first + 1u));
    dc_gpu_body_motion_t motion;
    for (unsigned i = 0; i < 2; ++i) {
        ASSERT_TRUE(dc_gpu_read_body_motion(view->gpu, first + i, &motion, err, sizeof(err)));
        ASSERT_TRUE(!(motion.flags & DC_GPU_BODY_KINEMATIC));
    }
    ASSERT_TRUE(dc_gpu_read_body_motion(view->gpu, first, &motion, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(view->gpu, first, motion.angle, .05f, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 2 * VIEW_WIDTH, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(view->camera_offscreen.workspace);
    dc_gpu_t *remote = view->camera_offscreen.workspace->gpu;
    ASSERT_TRUE(camera_has_live_bond(remote, first, first + 1u));
    ASSERT_TRUE(!camera_has_live_bond(view->gpu, first, first + 1u));
    for (unsigned i = 0; i < 2; ++i) {
        ASSERT_TRUE(!dc_gpu_read_body_motion(view->gpu, first + i, &motion, NULL, 0));
        ASSERT_TRUE(dc_gpu_read_body_motion(remote, first + i, &motion, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_step_timed(view, .03125f, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(remote, first + 1u, &motion, err, sizeof(err)));
    ASSERT_TRUE(fabsf(motion.angular_velocity) > .00001f);
    dc_gpu_world_body_t confirmed[2], restored;
    for (unsigned i = 0; i < 2; ++i)
        ASSERT_TRUE(dc_gpu_read_world_body(remote, first + i, &confirmed[i], err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -2 * VIEW_WIDTH, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(camera_has_live_bond(view->gpu, first, first + 1u));
    for (unsigned i = 0; i < 2; ++i) {
        ASSERT_TRUE(dc_gpu_read_world_body(view->gpu, first + i, &restored, err, sizeof(err)));
        ASSERT_EQ(memcmp(&restored, &confirmed[i], sizeof(restored)), 0);
    }
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    view = camera_test_view(directory, true, err, sizeof(err));
    ASSERT_TRUE(view && dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(camera_has_live_bond(view->gpu, first, first + 1u));
    for (unsigned i = 0; i < 2; ++i) {
        ASSERT_TRUE(dc_gpu_read_world_body(view->gpu, first + i, &restored, err, sizeof(err)));
        ASSERT_EQ(memcmp(&restored, &confirmed[i], sizeof(restored)), 0);
    }
    ASSERT_EQ(view->world_seconds, 2.03125);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}
#endif
