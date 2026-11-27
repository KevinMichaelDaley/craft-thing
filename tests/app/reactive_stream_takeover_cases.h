#ifndef DC_REACTIVE_STREAM_TAKEOVER_CASES_H
#define DC_REACTIVE_STREAM_TAKEOVER_CASES_H

static bool reject_duplicate_parcel_takeover(dc_level_view_t *view, dc_stream_result_t *update,
    dc_chunk_owner_t owner, char *err, uint32_t cap) {
    dc_chunk_t *other = calloc(1, sizeof(*other));
    if (!other) return false;
    other->coord = (dc_chunk_coord_t){89, -14};
    other->cells[0].material = DC_MATERIAL_WOOD;
    fr_reactive_state_t parcel;
    fr_reactive_chunk_t unrelated = {0};
    bool seeded = dc_gpu_upload_chunk(view->gpu, 1u, other, err, cap) &&
        fr_reactive_state_default(DC_MATERIAL_WOOD, 39322, 1200, 330, 31, &parcel, err, cap) &&
        fr_gpu_reactive_seed_cell(view->gpu, 1u, 0u, &parcel, err, cap) &&
        fr_gpu_reactive_export_chunk(view->gpu, 1u, other, &unrelated, err, cap);
    free(other);
    if (!seeded || unrelated.count != 1) {
        fr_reactive_chunk_release(&unrelated);
        return false;
    }
    fr_reactive_state_t *incoming = &update->reactive->attachments[0].record.state;
    const fr_reactive_state_t original = *incoming;
    const uint64_t next_id = update->reactive->next_id;
    const fr_reactive_state_t *existing = &unrelated.attachments[0].record.state;
    incoming->parcel_id_low = existing->parcel_id_low;
    incoming->parcel_id_high = existing->parcel_id_high;
    update->reactive->next_id = unrelated.next_id;
    bool queued = dc_stream_request_save_takeover(view->stream, update, owner, 1u);
    *incoming = original;
    update->reactive->next_id = next_id;
    fr_reactive_chunk_release(&unrelated);
    dc_stream_result_t result = {0};
    bool saved = queued && receive(view->stream, &result) && result.kind == DC_STREAM_SAVED;
    dc_stream_result_release(&result);
    uint32_t slot = 0;
    if (!saved || publish_timed_chunk(view, update->coord, &slot, err, cap) ||
        !strstr(err, "duplicates a resident parcel identity")) return false;
    return !dc_chunk_table_find(&view->table, update->coord, NULL) &&
        view->gpu->checkpoint_chunk_count == 1 && !view->gpu->checkpoint_reactive_handles[0] &&
        dc_chunk_owner_equal(view->gpu->checkpoint_owners[0], owner) &&
        memcmp(&view->gpu->checkpoint_reactive[0].attachments[0].record.state,
               &original, sizeof(original)) == 0;
}

static void test_workspace_takeover_retries_and_commits_exactly_once(void) {
    char directory[] = "build/workspace_takeover_XXXXXX", temporary[256], err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    const dc_chunk_coord_t coord = {-18, 24};
    const unsigned grain = 12u * DC_CHUNK_SIDE + 26u, wet = 27u * DC_CHUNK_SIDE + 28u, wood = 80u;
    dc_level_view_t view = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    fr_bulk_phase_chunk_t *bulk = calloc(1, sizeof(*bulk));
    fr_gas_checkpoint_chunk_t *gas = calloc(1, sizeof(*gas));
    ASSERT_TRUE(chunk && bulk && gas);
    chunk->coord = coord;
    chunk->cells[grain].material = DC_MATERIAL_SAND;
    chunk->cells[wet].fluid_mass = 14000;
    chunk->cells[wood].material = DC_MATERIAL_WOOD;
    bulk->oil_fp[wet] = 8000;
    gas->cells[wet][0] = .001f;
    gas->cells[wet][5] = .197f;
    gas->cells[wet][12] = .5f;
    ASSERT_TRUE(structure_time_gpu(&view.gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_configure_atmosphere(&view, err, sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_init(&view.table, DC_GPU_CHUNK_SLOTS));
    uint32_t slot;
    uint64_t generation;
    ASSERT_TRUE(dc_chunk_table_begin_load(&view.table, coord, &slot, &generation));
    ASSERT_TRUE(dc_gpu_upload_chunk(view.gpu, slot, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_finish_load(&view.table, slot, generation));
    ASSERT_TRUE(fr_bulk_phase_gpu_import_chunk(view.gpu, slot, chunk, bulk, err, sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_import_checkpoint(view.gpu, slot, gas, err, sizeof(err)));
    fr_reactive_state_t parcel;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD, 39322, 1200, 330, 30,
                                         &parcel, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(view.gpu, slot, wood, &parcel, err, sizeof(err)));
    view.table.slots[slot].time = (dc_chunk_time_t){1., true};
    view.gas_admitted[slot] = true;
    snprintf(view.body_path, sizeof(view.body_path), "%s/%s", directory, DC_WORLD_BODY_RECORD);
    ASSERT_TRUE(save_level_structure(&view, err, sizeof(err)));
    dc_gpu_destroy(view.gpu);
    dc_chunk_table_destroy(&view.table);

    view.gpu = NULL;
    view.stream = dc_stream_create(directory, 42, 4);
    view.world_seconds = 1.03125;
    memset(view.gas_admitted, 0, sizeof(view.gas_admitted));
    ASSERT_TRUE(view.stream && structure_time_gpu(&view.gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_configure_atmosphere(&view, err, sizeof(err)));
    ASSERT_TRUE(dc_chunk_table_init(&view.table, DC_GPU_CHUNK_SLOTS));
    ASSERT_TRUE(fr_gpu_load_structure(view.gpu, view.body_path, err, sizeof(err)));
    dc_offscreen_t *workspace = dc_offscreen_create(view.gpu, coord, err, sizeof(err));
    ASSERT_TRUE(workspace && dc_level_offscreen_begin(&view, workspace, coord, &generation, err, sizeof(err)));
    dc_stream_result_t result = {0}, before = {0}, after = {0};
    ASSERT_TRUE(receive(view.stream, &result));
    ASSERT_EQ(dc_level_offscreen_result(&view, &result, err, sizeof(err)), DC_LEVEL_OFFSCREEN_HANDLED);
    dc_stream_result_release(&result);
    ASSERT_TRUE(!dc_chunk_table_find(&view.table, coord, NULL));
    ASSERT_TRUE(dc_level_offscreen_owns(&view, coord));
    ASSERT_TRUE(dc_offscreen_snapshot(workspace, coord, generation, &before, err, sizeof(err)));
    ASSERT_TRUE(dc_offscreen_advance(workspace, view.world_seconds, 4, err, sizeof(err)));
    ASSERT_TRUE(dc_offscreen_snapshot(workspace, coord, generation, &after, err, sizeof(err)));
    ASSERT_EQ(after.time.seconds, view.world_seconds);
    ASSERT_TRUE(memcmp(before.chunk->face_velocity, after.chunk->face_velocity,
                       sizeof(chunk->face_velocity)) != 0 ||
                memcmp(before.chunk->particles, after.chunk->particles, sizeof(chunk->particles)) != 0);
    dc_stream_result_release(&before);

    snprintf(temporary, sizeof(temporary), "%s/chunk_%016" PRIx64 "_%016" PRIx64 ".bin.tmp",
             directory, (uint64_t)coord.x, (uint64_t)coord.y);
    ASSERT_EQ(mkdir(temporary, 0700), 0);
    ASSERT_TRUE(dc_level_offscreen_publish(&view, coord, generation, err, sizeof(err)));
    ASSERT_TRUE(dc_offscreen_advance(workspace, view.world_seconds + .03125, 4, err, sizeof(err)));
    ASSERT_TRUE(dc_offscreen_snapshot(workspace, coord, generation, &before, err, sizeof(err)));
    ASSERT_EQ(before.time.seconds, view.world_seconds);
    dc_stream_result_release(&before);
    ASSERT_TRUE(receive(view.stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_FAILED);
    uint64_t failed_write = result.generation;
    ASSERT_EQ(dc_level_offscreen_result(&view, &result, err, sizeof(err)), DC_LEVEL_OFFSCREEN_FAILED);
    dc_stream_result_release(&result);
    ASSERT_TRUE(dc_level_offscreen_owns(&view, coord) && view.gpu->checkpoint_chunk_count == 1);
    ASSERT_TRUE(!dc_level_offscreen_writing(&view));
    ASSERT_EQ(rmdir(temporary), 0);
    ASSERT_TRUE(dc_level_offscreen_publish(&view, coord, generation, err, sizeof(err)));
    ASSERT_TRUE(receive(view.stream, &result) && result.kind == DC_STREAM_SAVED);
    ASSERT_TRUE(result.generation != failed_write);
    dc_stream_result_t stale = result;
    stale.generation = failed_write;
    ASSERT_EQ(dc_level_offscreen_result(&view, &stale, err, sizeof(err)), DC_LEVEL_OFFSCREEN_UNMATCHED);
    stale = result;
    ++stale.coord.y;
    ASSERT_EQ(dc_level_offscreen_result(&view, &stale, err, sizeof(err)), DC_LEVEL_OFFSCREEN_UNMATCHED);
    stale = result;
    ++stale.replaces_owner.serial;
    ASSERT_EQ(dc_level_offscreen_result(&view, &stale, err, sizeof(err)), DC_LEVEL_OFFSCREEN_UNMATCHED);
    ASSERT_TRUE(dc_level_offscreen_writing(&view));
    ASSERT_EQ(dc_level_offscreen_result(&view, &result, err, sizeof(err)), DC_LEVEL_OFFSCREEN_HANDLED);
    dc_stream_result_release(&result);
    ASSERT_TRUE(!dc_level_offscreen_owns(&view, coord) && view.gpu->checkpoint_chunk_count == 1);
    ASSERT_TRUE(!dc_offscreen_snapshot(workspace, coord, generation, &before, err, sizeof(err)));
    uint64_t recovery;
    ASSERT_TRUE(dc_level_offscreen_begin(&view, workspace, coord, &recovery, err, sizeof(err)));
    ASSERT_TRUE(recovery != generation);
    ASSERT_TRUE(receive(view.stream, &result));
    ASSERT_EQ(dc_level_offscreen_result(&view, &result, err, sizeof(err)), DC_LEVEL_OFFSCREEN_HANDLED);
    dc_stream_result_release(&result);
    ASSERT_TRUE(dc_offscreen_snapshot(workspace, coord, recovery, &before, err, sizeof(err)));
    ASSERT_EQ(before.time.seconds, view.world_seconds);
    dc_stream_result_release(&before);
    ASSERT_TRUE(dc_level_offscreen_cancel(&view, coord, recovery, err, sizeof(err)));
    dc_offscreen_destroy(workspace);
    dc_stream_destroy(view.stream);
    dc_gpu_destroy(view.gpu);

    view.stream = dc_stream_create(directory, 42, 4);
    view.gpu = NULL;
    ASSERT_TRUE(view.stream && structure_time_gpu(&view.gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_configure_atmosphere(&view, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_load_structure(view.gpu, view.body_path, err, sizeof(err)));
    ASSERT_TRUE(publish_timed_chunk(&view, coord, &slot, err, sizeof(err)));
    ASSERT_TRUE(view.table.slots[slot].time.known && view.table.slots[slot].time.seconds == view.world_seconds);
    ASSERT_TRUE(view.gpu->checkpoint_chunk_count == 0);
    ASSERT_TRUE(dc_gpu_download_chunk(view.gpu, slot, chunk, err, sizeof(err)));
    ASSERT_TRUE(fr_bulk_phase_gpu_export_chunk(view.gpu, slot, bulk, err, sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_export_checkpoint(view.gpu, slot, gas, err, sizeof(err)));
    ASSERT_EQ(memcmp(chunk->face_velocity, after.chunk->face_velocity, sizeof(chunk->face_velocity)), 0);
    ASSERT_EQ(memcmp(bulk, after.bulk, sizeof(*bulk)), 0);
    ASSERT_EQ(memcmp(gas, after.gas, sizeof(*gas)), 0);
    fr_reactive_chunk_t parcels = {0};
    ASSERT_TRUE(fr_gpu_reactive_export_chunk(view.gpu, slot, chunk, &parcels, err, sizeof(err)));
    ASSERT_TRUE(after.reactive && parcels.count == 1 && after.reactive->count == 1);
    ASSERT_EQ(memcmp(parcels.attachments, after.reactive->attachments, sizeof(*parcels.attachments)), 0);
    ASSERT_EQ(dc_gpu_validation_errors(view.gpu), 0u);
    fr_reactive_chunk_release(&parcels);
    dc_stream_result_release(&after);
    dc_stream_destroy(view.stream);
    dc_gpu_destroy(view.gpu);
    dc_chunk_table_destroy(&view.table);
    free(gas);
    free(bulk);
    free(chunk);
    PASS();
}

#endif
