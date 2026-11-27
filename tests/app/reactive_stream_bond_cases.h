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

enum bond_check { BOND_CAMERA, BOND_COVERAGE, BOND_HANDOFF, BOND_RETRY, BOND_CONTACT, BOND_SEPARATE_MEMBER };

static bool read_bond_contacts(dc_gpu_t *gpu, uint32_t first, dc_gpu_contact_t *out, uint32_t *count,
    char *err, uint32_t cap) {
    dc_gpu_contact_t contacts[256];
    dc_gpu_contact_stats_t stats;
    if (!dc_gpu_read_contacts(gpu, &stats, contacts, 256u, err, cap) || stats.overflow) return false;
    *count = 0;
    for (unsigned i = 0; i < stats.count; ++i)
        if (contacts[i].kind == DC_GPU_CONTACT_TERRAIN &&
            (contacts[i].body_a == first || contacts[i].body_a == first + 1u)) out[(*count)++] = contacts[i];
    return true;
}
static void check_camera_bonded_assembly(enum bond_check check) {
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
            .x_fp = (x % DC_CHUNK_SIDE) * DC_FLUID_FULL,
            .y_fp = (check == BOND_CONTACT ? 59 : 24) * DC_FLUID_FULL};
    }
    for (unsigned i = 0; i < 2; ++i)
        graph.members[i] = (fr_structural_gpu_member_t){.id = i + 1u, .generation = 3u,
            .a = i + 1u, .b = i + 2u, .material = i ? FR_STRUCTURAL_STONE : FR_STRUCTURAL_WOOD,
            .depth_fp = 2u * DC_FLUID_FULL};
    if (check == BOND_SEPARATE_MEMBER) {
        graph.node_count += 3u;
        graph.member_count += 2u;
        for (unsigned i = 0; i < 3; ++i) {
            graph.nodes[3u + i] = (fr_structural_gpu_node_t){.id = 4u + i, .generation = 5,
                .chunk = {origin.x + 3, origin.y}, .x_fp = (12 + (int)i * 8) * DC_FLUID_FULL,
                .y_fp = 18 * DC_FLUID_FULL};
        }
        graph.members[2] = (fr_structural_gpu_member_t){.id = 3u, .generation = 5u, .a = 4u, .b = 5u,
            .material = FR_STRUCTURAL_WOOD, .depth_fp = DC_FLUID_FULL};
        graph.members[3] = (fr_structural_gpu_member_t){.id = 4u, .generation = 6u, .a = 5u, .b = 6u,
            .material = FR_STRUCTURAL_STONE, .depth_fp = DC_FLUID_FULL};
    }
    ASSERT_TRUE(fr_gpu_place_structure(view->gpu, &graph, err, sizeof(err)));
    for (unsigned i = 0; i < 90; ++i) ASSERT_TRUE(dc_gpu_rigid_step(view->gpu, err, sizeof(err)));
    const uint32_t first = FR_STRUCTURAL_BODY_ID_BASE + 1u;
    ASSERT_TRUE(camera_has_live_bond(view->gpu, first, first + 1u));
    if (check == BOND_COVERAGE) {
        for (unsigned seed = 0; seed < 2; ++seed) {
            dc_offscreen_bond_group_t group = {0};
            ASSERT_TRUE(dc_offscreen_collect_bond_group(view->gpu, first + seed, &group));
            ASSERT_EQ(group.member_count, 2u);
            ASSERT_EQ(group.page_count, 2u);
            unsigned pages = 0, members = 0;
            for (unsigned i = 0; i < 2; ++i) {
                ASSERT_TRUE(group.pages[i].x == origin.x || group.pages[i].x == origin.x + 1);
                ASSERT_EQ(group.pages[i].y, origin.y);
                pages |= 1u << (group.pages[i].x - origin.x);
                ASSERT_TRUE(group.members[i].id == first || group.members[i].id == first + 1u);
                members |= 1u << (group.members[i].id - first);
                ASSERT_EQ(group.members[i].member_generation, graph.members[0].generation);
                ASSERT_EQ(dc_gpu_body_record(view->gpu, group.members[i].slot)->body.id, group.members[i].id);
            }
            ASSERT_EQ(pages, 3u);
            ASSERT_EQ(members, 3u);
        }
        ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
        PASS();
        return;
    }
    dc_gpu_body_motion_t motion;
    for (unsigned i = 0; i < 2; ++i) {
        ASSERT_TRUE(dc_gpu_read_body_motion(view->gpu, first + i, &motion, err, sizeof(err)));
        ASSERT_TRUE(!(motion.flags & DC_GPU_BODY_KINEMATIC));
    }
    ASSERT_TRUE(dc_gpu_read_body_motion(view->gpu, first, &motion, err, sizeof(err)));
    dc_gpu_contact_t previous[256], next[256];
    uint32_t previous_count = 0, next_count = 0;
    if (check == BOND_CONTACT) {
        for (unsigned i = 0; i < 60 && !previous_count; ++i) {
            ASSERT_TRUE(dc_gpu_rigid_step(view->gpu, err, sizeof(err)));
            ASSERT_TRUE(read_bond_contacts(view->gpu, first, previous, &previous_count, err, sizeof(err)));
        }
        ASSERT_TRUE(previous_count > 0);
    } else {
        ASSERT_TRUE(dc_gpu_set_body_motion(view->gpu, first, motion.angle, .05f, 0, err, sizeof(err)));
    }
    if (check == BOND_HANDOFF) {
        dc_offscreen_t *workspace = dc_offscreen_create(view->gpu, origin, err, sizeof(err));
        ASSERT_TRUE(workspace);
        dc_gpu_t *remote = workspace->gpu;
        dc_offscreen_bond_group_t group;
        ASSERT_TRUE(dc_offscreen_collect_bond_group(view->gpu, first, &group));
        for (unsigned i = 0; i < group.page_count; ++i) {
            uint32_t slot;
            dc_chunk_t chunk = {.coord = group.pages[i]};
            ASSERT_TRUE(dc_chunk_table_find(&view->table, group.pages[i], &slot));
            ASSERT_TRUE(dc_gpu_download_chunk(view->gpu, slot, &chunk, err, sizeof(err)));
            dc_stream_result_t source = {.kind = DC_STREAM_LOADED, .coord = chunk.coord, .chunk = &chunk,
                .generation = 9u, .time = {.known = true, .seconds = view->world_seconds}};
            ASSERT_TRUE(dc_offscreen_admit(workspace, &source, 9u, err, sizeof(err)));
        }
        dc_gpu_world_body_t unrelated = {.chunk = origin, .body = {.active = 1u, .id = 77,
            .width = 2, .height = 2, .x_fp = 4 * DC_FLUID_FULL, .y_fp = 5 * DC_FLUID_FULL}};
        ASSERT_TRUE(dc_gpu_spawn_world_body(remote, unrelated, err, sizeof(err)));
        ++group.members[1].member_generation;
        ASSERT_TRUE(!dc_offscreen_move_bond_group(view->gpu, remote, &group, err, sizeof(err)));
        ASSERT_TRUE(camera_has_live_bond(view->gpu, first, first + 1u));
        --group.members[1].member_generation;
        ASSERT_TRUE(dc_offscreen_move_bond_group(view->gpu, remote, &group, err, sizeof(err)));
        ASSERT_TRUE(!camera_has_live_bond(view->gpu, first, first + 1u));
        ASSERT_TRUE(camera_has_live_bond(remote, first, first + 1u));
        fr_structural_gpu_member_state_t state;
        for (unsigned i = 0; i < 2; ++i) {
            ASSERT_TRUE(!fr_gpu_read_structure_member(view->gpu, i + 1u, &state));
            ASSERT_TRUE(fr_gpu_read_structure_member(remote, i + 1u, &state));
            ASSERT_EQ(state.generation, 3u);
            ASSERT_EQ(state.state, FR_STRUCTURAL_CONVERTED);
            ASSERT_TRUE(!dc_gpu_read_body_motion(view->gpu, first + i, &motion, NULL, 0));
        }
        for (unsigned i = 0; i < 6; ++i) ASSERT_TRUE(dc_gpu_rigid_step(remote, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_body_motion(remote, first + 1u, &motion, err, sizeof(err)));
        ASSERT_TRUE(fabsf(motion.angular_velocity) > .00001f);
        ASSERT_TRUE(dc_offscreen_collect_bond_group(remote, first + 1u, &group));
        ASSERT_TRUE(dc_offscreen_move_bond_group(remote, view->gpu, &group, err, sizeof(err)));
        ASSERT_TRUE(camera_has_live_bond(view->gpu, first, first + 1u));
        ASSERT_TRUE(!camera_has_live_bond(remote, first, first + 1u));
        ASSERT_TRUE(dc_gpu_read_world_body(remote, 77, &unrelated, err, sizeof(err)));
        dc_offscreen_destroy(workspace);
        ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
        PASS();
        return;
    }
    dc_gpu_world_body_t uninterrupted[2];
    dc_gpu_body_motion_t uninterrupted_motion[2];
    if (check == BOND_CAMERA) {
        dc_gpu_t *reference = NULL;
        ASSERT_TRUE(dc_gpu_create_shared(&reference, view->gpu, 2 * DC_CHUNK_SIDE, DC_CHUNK_SIDE,
            "build/shaders/pattern.comp.spv", err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_body_origin(reference, origin, err, sizeof(err)));
        for (unsigned i = 0; i < 2; ++i) {
            dc_chunk_t chunk = {.coord = {origin.x + i, origin.y}};
            uint32_t slot;
            ASSERT_TRUE(dc_chunk_table_find(&view->table, chunk.coord, &slot));
            ASSERT_TRUE(dc_gpu_download_chunk(view->gpu, slot, &chunk, err, sizeof(err)));
            ASSERT_TRUE(dc_gpu_upload_chunk(reference, i, &chunk, err, sizeof(err)));
            ASSERT_TRUE(dc_gpu_set_page(reference, i, 0, i, err, sizeof(err)));
        }
        ASSERT_TRUE(fr_gpu_place_structure(reference, &graph, err, sizeof(err)));
        for (unsigned i = 0; i < 90; ++i) ASSERT_TRUE(dc_gpu_rigid_step(reference, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_body_motion(reference, first, &motion, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_body_motion(reference, first, motion.angle, .05f, 0, err, sizeof(err)));
        const float intervals[] = {.0078125f, .03125f};
        for (unsigned i = 0; i < 2; ++i)
            ASSERT_TRUE(dc_gpu_set_tick_seconds(reference, intervals[i]) && dc_gpu_tick_step(reference, err, sizeof(err)));
        for (unsigned i = 0; i < 2; ++i) {
            ASSERT_TRUE(dc_gpu_read_world_body(reference, first + i, &uninterrupted[i], err, sizeof(err)));
            ASSERT_TRUE(dc_gpu_read_body_motion(reference, first + i, &uninterrupted_motion[i], err, sizeof(err)));
        }
        dc_gpu_destroy(reference);
        ASSERT_TRUE(dc_level_view_step_timed(view, intervals[0], err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    double handoff_seconds = view->world_seconds;
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 2 * VIEW_WIDTH, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
    ASSERT_TRUE(view->camera_offscreen.workspace);
    dc_gpu_t *remote = view->camera_offscreen.workspace->gpu;
    ASSERT_TRUE(camera_has_live_bond(remote, first, first + 1u));
    ASSERT_TRUE(!camera_has_live_bond(view->gpu, first, first + 1u));
    if (check == BOND_SEPARATE_MEMBER) {
        fr_structural_gpu_member_state_t state;
        ASSERT_TRUE(camera_has_live_bond(view->gpu, first + 2u, first + 3u));
        ASSERT_TRUE(!camera_has_live_bond(remote, first + 2u, first + 3u));
        ASSERT_TRUE(fr_gpu_read_structure_member(view->gpu, 3u, &state));
        ASSERT_EQ(state.generation, 5u);
        ASSERT_TRUE(!fr_gpu_read_structure_member(remote, 3u, &state));
        ASSERT_TRUE(dc_gpu_read_body_motion(view->gpu, first + 2u, &motion, err, sizeof(err)));
        ASSERT_TRUE(!dc_gpu_read_body_motion(remote, first + 2u, &motion, NULL, 0));
        ASSERT_TRUE(!fr_gpu_read_structure_member(view->gpu, 1u, &state));
        ASSERT_TRUE(fr_gpu_read_structure_member(remote, 1u, &state));
    }
    if (check == BOND_CONTACT) {
        ASSERT_TRUE(read_bond_contacts(view->gpu, first, next, &next_count, err, sizeof(err)));
        ASSERT_EQ(next_count, 0u);
        ASSERT_TRUE(read_bond_contacts(remote, first, next, &next_count, err, sizeof(err)));
        ASSERT_EQ(next_count, previous_count);
        for (unsigned i = 0; i < previous_count; ++i) {
            unsigned j = 0;
            while (j < next_count && memcmp(&previous[i], &next[j], sizeof(next[j]))) ++j;
            ASSERT_TRUE(j < next_count);
        }
    }
    for (unsigned i = 0; i < 2; ++i) {
        ASSERT_TRUE(!dc_gpu_read_body_motion(view->gpu, first + i, &motion, NULL, 0));
        ASSERT_TRUE(dc_gpu_read_body_motion(remote, first + i, &motion, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_step_timed(view, .03125f, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(remote, first + 1u, &motion, err, sizeof(err)));
    if (check != BOND_CONTACT) ASSERT_TRUE(fabsf(motion.angular_velocity) > .00001f);
    dc_gpu_world_body_t confirmed[2], restored;
    for (unsigned i = 0; i < 2; ++i)
        ASSERT_TRUE(dc_gpu_read_world_body(remote, first + i, &confirmed[i], err, sizeof(err)));
    if (check == BOND_CAMERA) {
        for (unsigned i = 0; i < 2; ++i) {
            ASSERT_EQ(memcmp(&uninterrupted[i], &confirmed[i], sizeof(confirmed[i])), 0);
            ASSERT_TRUE(dc_gpu_read_body_motion(remote, first + i, &motion, err, sizeof(err)));
            ASSERT_EQ(motion.mass, uninterrupted_motion[i].mass);
            ASSERT_EQ(motion.inertia, uninterrupted_motion[i].inertia);
            ASSERT_EQ(motion.angle, uninterrupted_motion[i].angle);
            ASSERT_EQ(motion.angular_velocity, uninterrupted_motion[i].angular_velocity);
        }
    }
    char failed_page[512];
    if (check == BOND_RETRY) {
        const char *draft = dc_world_save_directory(view->save, true);
        ASSERT_TRUE(draft);
        snprintf(failed_page, sizeof(failed_page), "%s/chunk_%016" PRIx64 "_%016" PRIx64 ".bin.tmp",
            draft, (uint64_t)(origin.x + 1), (uint64_t)origin.y);
        ASSERT_EQ(mkdir(failed_page, 0700), 0);
    }
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -2 * VIEW_WIDTH, 0));
    if (check == BOND_RETRY) {
        ASSERT_TRUE(!dc_level_view_wait_visible(view, 25000u, err, sizeof(err)));
        ASSERT_TRUE(strstr(err, "replacement write failed"));
        ASSERT_TRUE(camera_has_live_bond(remote, first, first + 1u));
        ASSERT_TRUE(!camera_has_live_bond(view->gpu, first, first + 1u));
        for (unsigned i = 0; i < 2; ++i) {
            ASSERT_TRUE(dc_level_offscreen_owns(view, (dc_chunk_coord_t){origin.x + i, origin.y}));
            ASSERT_TRUE(!dc_gpu_read_world_body(view->gpu, first + i, &restored, NULL, 0));
            ASSERT_TRUE(dc_gpu_read_world_body(remote, first + i, &restored, err, sizeof(err)));
            ASSERT_EQ(memcmp(&restored, &confirmed[i], sizeof(restored)), 0);
        }
        ASSERT_EQ(rmdir(failed_page), 0);
        /* Shutdown must finish the page that failed even if its sibling still has a receipt in flight. */
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
        return;
    }
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
    ASSERT_EQ(view->world_seconds, handoff_seconds + .03125);
    if (check == BOND_SEPARATE_MEMBER) {
        ASSERT_TRUE(camera_has_live_bond(view->gpu, first + 2u, first + 3u));
        for (unsigned i = 0; i < graph.member_count; ++i) {
            fr_structural_gpu_member_state_t state;
            ASSERT_TRUE(fr_gpu_read_structure_member(view->gpu, i + 1u, &state));
            ASSERT_EQ(state.generation, graph.members[i].generation);
        }
        ASSERT_TRUE(dc_gpu_read_body_motion(view->gpu, first + 2u, &motion, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}
static void test_both_bond_endpoints_identify_the_same_members_and_pages(void) {
    check_camera_bonded_assembly(BOND_COVERAGE);
}

static void test_bond_handoff_rebinds_slots_and_preserves_member_generations(void) {
    check_camera_bonded_assembly(BOND_HANDOFF);
}

static void test_camera_transfers_an_assembly_spanning_two_chunk_pages(void) {
    check_camera_bonded_assembly(BOND_CAMERA);
}
static void test_close_retries_failed_bond_page_and_reopens_the_whole_group(void) {
    check_camera_bonded_assembly(BOND_RETRY);
}
static void test_camera_preserves_completed_terrain_contacts_of_bonded_members(void) {
    check_camera_bonded_assembly(BOND_CONTACT);
}
static void test_camera_keeps_other_authored_components_with_their_owner(void) {
    check_camera_bonded_assembly(BOND_SEPARATE_MEMBER);
}
#endif
