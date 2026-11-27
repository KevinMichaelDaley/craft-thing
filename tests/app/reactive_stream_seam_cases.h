#ifndef DC_REACTIVE_STREAM_SEAM_CASES_H
#define DC_REACTIVE_STREAM_SEAM_CASES_H

#include "../../src/app/offscreen_internal.h"

static bool lose_seam_receipt, lose_seam_recovery, reject_next_queue_wait;

VkResult __real_vkQueueWaitIdle(VkQueue queue);
VkResult __wrap_vkQueueWaitIdle(VkQueue queue) {
    if (!reject_next_queue_wait) return __real_vkQueueWaitIdle(queue);
    reject_next_queue_wait = false;
    return VK_ERROR_DEVICE_LOST;
}

bool __real_fr_renderer_submit(dc_gpu_t *gpu);
bool __wrap_fr_renderer_submit(dc_gpu_t *gpu) {
    bool completed = __real_fr_renderer_submit(gpu);
    if (!lose_seam_receipt) return completed;
    lose_seam_receipt = false;
    reject_next_queue_wait = lose_seam_recovery;
    lose_seam_recovery = false;
    return false;
}

static dc_level_view_t *seam_test_view(const char *directory, char *err, uint32_t cap) {
    dc_level_view_t *view = calloc(1, sizeof(*view));
    if (!view) return NULL;
    view->save = dc_world_save_open(directory, err, cap);
    view->stream = dc_stream_create(directory, 42, 8);
    snprintf(view->body_path, sizeof(view->body_path), "%s/%s", directory, DC_WORLD_BODY_RECORD);
    if (view->save && view->stream && structure_time_gpu(&view->gpu, err, cap) &&
        dc_chunk_table_init(&view->table, DC_GPU_CHUNK_SLOTS)) return view;
    dc_level_view_discard(view);
    return NULL;
}

static void seam_pocket(dc_chunk_t *chunk, dc_chunk_coord_t coord, uint32_t cell, uint32_t water) {
    chunk->coord = coord;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
        chunk->cells[i] = (dc_cell_t){.material = DC_MATERIAL_STONE, .flags = DC_CELL_LOADED_TERRAIN};
    chunk->cells[cell] = (dc_cell_t){.fluid_mass = water};
}

static bool save_seam_pockets(const char *directory, const dc_level_liquid_exchange_t *exchange,
    uint32_t lane, char *err, uint32_t cap) {
    dc_level_view_t *view = seam_test_view(directory, err, cap);
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    bool okay = view && chunk;
    if (okay) view->world_seconds = exchange->begin_seconds;
    for (uint32_t i = 0; okay && i < 2u; ++i) {
        seam_pocket(chunk, i ? exchange->offscreen : exchange->resident,
            i ? lane * DC_CHUNK_SIDE : (lane + 1u) * DC_CHUNK_SIDE - 1u, i ? 6000u : 16000u);
        uint32_t slot;
        uint64_t generation;
        okay = dc_chunk_table_begin_load(&view->table, chunk->coord, &slot, &generation) &&
            dc_gpu_upload_chunk(view->gpu, slot, chunk, err, cap) &&
            dc_chunk_table_finish_load(&view->table, slot, generation);
        if (okay) view->table.slots[slot].time = (dc_chunk_time_t){exchange->begin_seconds, true};
    }
    if (okay) okay = save_level_structure(view, err, cap);
    dc_level_view_discard(view);
    free(chunk);
    return okay;
}

static bool step_seam_resident(dc_level_view_t *view, float seconds, char *err, uint32_t cap) {
    if (!dc_gpu_set_tick_seconds(view->gpu, seconds) || !dc_gpu_tick_step(view->gpu, err, cap)) return false;
    advance_resident_chunks(view, seconds);
    return true;
}

static bool admit_seam_owners(dc_level_view_t *view, dc_level_liquid_exchange_t *exchange,
    uint32_t *resident_slot, dc_offscreen_t **workspace, char *err, uint32_t cap) {
    if (!dc_world_save_load_clock(view->save, &view->world_seconds, err, cap) ||
        !fr_gpu_load_structure(view->gpu, view->body_path, err, cap) ||
        !dc_gpu_set_body_origin(view->gpu, exchange->resident, err, cap) ||
        !publish_timed_chunk(view, exchange->resident, resident_slot, err, cap) ||
        !dc_gpu_set_page(view->gpu, 0, 0, *resident_slot, err, cap)) return false;
    exchange->resident_generation = view->table.slots[*resident_slot].generation;
    *workspace = dc_offscreen_create(view->gpu, exchange->offscreen, err, cap);
    if (!*workspace || !dc_level_offscreen_begin(view, *workspace, exchange->offscreen,
        &exchange->offscreen_generation, err, cap)) return false;
    const uint64_t deadline = SDL_GetTicks64() + 5000u;
    do {
        dc_chunk_time_t time;
        bool writing;
        if (!process_results(view, err, cap) || !dc_level_offscreen_progress(view,
            exchange->offscreen, exchange->offscreen_generation, &time, &writing)) return false;
        if (time.known) return !writing;
        SDL_Delay(1);
    } while (SDL_GetTicks64() < deadline);
    return false;
}

static bool seam_water_matches(dc_level_view_t *view, uint32_t resident_slot,
    const dc_level_liquid_exchange_t *exchange, dc_offscreen_t *workspace, uint32_t lane,
    uint32_t resident_stock, uint32_t offscreen_stock, char *err, uint32_t cap) {
    dc_chunk_t *resident = calloc(1, sizeof(*resident));
    dc_stream_result_t remote = {0};
    bool okay = resident && dc_gpu_download_chunk(view->gpu, resident_slot, resident, err, cap) &&
        dc_offscreen_snapshot(workspace, exchange->offscreen, exchange->offscreen_generation, &remote, err, cap) &&
        resident->cells[(lane + 1u) * DC_CHUNK_SIDE - 1u].fluid_mass == resident_stock &&
        remote.chunk->cells[lane * DC_CHUNK_SIDE].fluid_mass == offscreen_stock;
    dc_stream_result_release(&remote);
    free(resident);
    return okay;
}

static void test_new_exchange_cannot_claim_an_earlier_receipt(void) {
    char directory[] = "build/paired_receipt_XXXXXX", err[256] = {0};
    const uint32_t lane = 24u;
    dc_level_liquid_exchange_t exchange = {.resident = {5, -4}, .offscreen = {6, -4},
        .begin_seconds = 2., .end_seconds = 2.03125};
    exchange.flux[lane].water = 5000;
    ASSERT_TRUE(mkdtemp(directory) && save_seam_pockets(directory, &exchange, lane, err, sizeof(err)));
    dc_level_view_t *view = seam_test_view(directory, err, sizeof(err));
    dc_offscreen_t *workspace = NULL;
    uint32_t slot;
    ASSERT_TRUE(view && admit_seam_owners(view, &exchange, &slot, &workspace, err, sizeof(err)));
    ASSERT_TRUE(step_seam_resident(view, .03125f, err, sizeof(err)));
    ASSERT_TRUE(dc_offscreen_advance(workspace, exchange.end_seconds, 1u, err, sizeof(err)));
    const dc_liquid_seam_endpoint_t local = {.gpu = view->gpu, .coord = exchange.resident, .slot = slot};
    const dc_liquid_seam_endpoint_t remote = {.gpu = workspace->gpu, .coord = exchange.offscreen};
    dc_liquid_seam_flux_t prior[DC_CHUNK_SIDE] = {0};
    prior[lane].water = 1000;
    lose_seam_receipt = true;
    ASSERT_EQ(dc_gpu_commit_liquid_seam(local, remote, DC_LIQUID_SEAM_X, prior, err, sizeof(err)),
              DC_LIQUID_SEAM_UNCONFIRMED);
    ASSERT_EQ(dc_level_offscreen_exchange_liquids(view, &exchange, err, sizeof(err)), DC_LIQUID_SEAM_REJECTED);
    ASSERT_TRUE(dc_gpu_liquid_seam_pending(view->gpu) && !view->world_save_staged);
    ASSERT_EQ(dc_gpu_resolve_liquid_seam(local.gpu, remote.gpu, err, sizeof(err)), DC_LIQUID_SEAM_COMMITTED);
    ASSERT_TRUE(seam_water_matches(view, slot, &exchange, workspace, lane, 15000u, 7000u, err, sizeof(err)));
    ASSERT_EQ(dc_level_offscreen_exchange_liquids(view, &exchange, err, sizeof(err)), DC_LIQUID_SEAM_COMMITTED);
    ASSERT_TRUE(seam_water_matches(view, slot, &exchange, workspace, lane, 10000u, 12000u, err, sizeof(err)));
    dc_level_view_discard(view);
    dc_offscreen_destroy(workspace);
    PASS();
}

static void test_level_seam_orders_owner_clocks_receipts_and_cold_publication(void) {
    char directory[] = "build/paired_liquid_XXXXXX", err[256] = {0}, obstruction[256];
    ASSERT_TRUE(mkdtemp(directory));
    const uint32_t lane = 24u;
    dc_level_liquid_exchange_t exchange = {.resident = {5, -4}, .offscreen = {6, -4},
        .begin_seconds = 2., .end_seconds = 2.03125};
    exchange.flux[lane].water = 5000;
    ASSERT_TRUE(save_seam_pockets(directory, &exchange, lane, err, sizeof(err)));

    dc_level_view_t *view = seam_test_view(directory, err, sizeof(err));
    dc_offscreen_t *workspace = NULL;
    uint32_t slot;
    ASSERT_TRUE(view && admit_seam_owners(view, &exchange, &slot, &workspace, err, sizeof(err)));
    ASSERT_TRUE(step_seam_resident(view, .03125f, err, sizeof(err)));
    ASSERT_EQ(dc_level_offscreen_exchange_liquids(view, &exchange, err, sizeof(err)), DC_LIQUID_SEAM_REJECTED);
    ASSERT_TRUE(dc_offscreen_advance(workspace, exchange.end_seconds, 1u, err, sizeof(err)));
    ASSERT_TRUE(seam_water_matches(view, slot, &exchange, workspace, lane, 16000u, 6000u, err, sizeof(err)));

    dc_level_liquid_exchange_t stale = exchange;
    ++stale.resident_generation;
    ASSERT_EQ(dc_level_offscreen_exchange_liquids(view, &stale, err, sizeof(err)), DC_LIQUID_SEAM_REJECTED);
    stale = exchange;
    ++stale.offscreen_generation;
    ASSERT_EQ(dc_level_offscreen_exchange_liquids(view, &stale, err, sizeof(err)), DC_LIQUID_SEAM_REJECTED);
    stale = exchange;
    ++stale.offscreen.x;
    ASSERT_EQ(dc_level_offscreen_exchange_liquids(view, &stale, err, sizeof(err)), DC_LIQUID_SEAM_REJECTED);
    ASSERT_TRUE(dc_gpu_set_body_origin(view->gpu, (dc_chunk_coord_t){4, -4}, err, sizeof(err)));
    ASSERT_EQ(dc_level_offscreen_exchange_liquids(view, &exchange, err, sizeof(err)), DC_LIQUID_SEAM_REJECTED);
    ASSERT_TRUE(!dc_world_save_directory(view->save, true));
    ASSERT_TRUE(dc_gpu_set_body_origin(view->gpu, exchange.resident, err, sizeof(err)));

    lose_seam_receipt = true;
    ASSERT_EQ(dc_level_offscreen_exchange_liquids(view, &exchange, err, sizeof(err)), DC_LIQUID_SEAM_COMMITTED);
    ASSERT_TRUE(!lose_seam_receipt);
    ASSERT_TRUE(seam_water_matches(view, slot, &exchange, workspace, lane, 11000u, 11000u, err, sizeof(err)));
    ASSERT_EQ(dc_level_offscreen_exchange_liquids(view, &exchange, err, sizeof(err)), DC_LIQUID_SEAM_REJECTED);
    ASSERT_TRUE(!dc_level_offscreen_cancel(view, exchange.offscreen, exchange.offscreen_generation, err, sizeof(err)));

    const char *staging = dc_world_save_directory(view->save, true);
    ASSERT_TRUE(staging);
    ASSERT_TRUE(snprintf(obstruction, sizeof(obstruction), "%s/chunk_%016" PRIx64 "_%016" PRIx64 ".bin.tmp",
        staging, (uint64_t)exchange.offscreen.x, (uint64_t)exchange.offscreen.y) > 0);
    ASSERT_EQ(mkdir(obstruction, 0700), 0);
    ASSERT_TRUE(dc_level_offscreen_publish(view, exchange.offscreen, exchange.offscreen_generation, err, sizeof(err)));
    bool failed = false;
    const uint64_t deadline = SDL_GetTicks64() + 5000u;
    do {
        if (!process_results(view, err, sizeof(err))) { failed = true; break; }
        SDL_Delay(1);
    } while (SDL_GetTicks64() < deadline);
    ASSERT_TRUE(failed && strstr(err, "Offscreen replacement write failed"));
    ASSERT_EQ(rmdir(obstruction), 0);
    double canonical;
    ASSERT_TRUE(dc_world_save_load_clock(view->save, &canonical, err, sizeof(err)));
    ASSERT_EQ(canonical, 2.);
    ASSERT_TRUE(seam_water_matches(view, slot, &exchange, workspace, lane, 11000u, 11000u, err, sizeof(err)));
    ASSERT_EQ(dc_gpu_validation_errors(view->gpu), 0u);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    dc_offscreen_destroy(workspace);

    view = seam_test_view(directory, err, sizeof(err));
    workspace = NULL;
    ASSERT_TRUE(view && admit_seam_owners(view, &exchange, &slot, &workspace, err, sizeof(err)));
    ASSERT_EQ(view->world_seconds, 2.03125);
    ASSERT_TRUE(seam_water_matches(view, slot, &exchange, workspace, lane, 11000u, 11000u, err, sizeof(err)));
    ASSERT_EQ(dc_level_offscreen_exchange_liquids(view, &exchange, err, sizeof(err)), DC_LIQUID_SEAM_REJECTED);
    ASSERT_EQ(dc_gpu_validation_errors(view->gpu), 0u);
    dc_level_view_discard(view);
    dc_offscreen_destroy(workspace);
    PASS();
}

static void test_unreadable_seam_receipt_preserves_both_canonical_owners(void) {
    char directory[] = "build/unconfirmed_liquid_pair_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    const uint32_t lane = 20u;
    dc_level_liquid_exchange_t exchange = {.resident = {7, -6}, .offscreen = {8, -6},
        .begin_seconds = 2., .end_seconds = 2.03125};
    exchange.flux[lane].water = 5000;
    ASSERT_TRUE(save_seam_pockets(directory, &exchange, lane, err, sizeof(err)));
    dc_level_view_t *view = seam_test_view(directory, err, sizeof(err));
    dc_offscreen_t *workspace = NULL;
    uint32_t slot;
    ASSERT_TRUE(view && admit_seam_owners(view, &exchange, &slot, &workspace, err, sizeof(err)));
    ASSERT_TRUE(step_seam_resident(view, .03125f, err, sizeof(err)));
    ASSERT_TRUE(dc_offscreen_advance(workspace, exchange.end_seconds, 1u, err, sizeof(err)));
    lose_seam_receipt = lose_seam_recovery = true;
    ASSERT_EQ(dc_level_offscreen_exchange_liquids(view, &exchange, err, sizeof(err)), DC_LIQUID_SEAM_UNCONFIRMED);
    ASSERT_TRUE(!lose_seam_receipt && !lose_seam_recovery && !reject_next_queue_wait);
    ASSERT_TRUE(!dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_TRUE(!schedule_saves(view, err, sizeof(err)));
    ASSERT_TRUE(!dc_level_offscreen_publish(view, exchange.offscreen, exchange.offscreen_generation, err, sizeof(err)));
    ASSERT_EQ(dc_gpu_validation_errors(view->gpu), 0u);
    ASSERT_TRUE(!dc_level_view_destroy(view, err, sizeof(err)));
    dc_offscreen_destroy(workspace);

    view = seam_test_view(directory, err, sizeof(err));
    workspace = NULL;
    ASSERT_TRUE(view && admit_seam_owners(view, &exchange, &slot, &workspace, err, sizeof(err)));
    ASSERT_EQ(view->world_seconds, 2.);
    ASSERT_TRUE(seam_water_matches(view, slot, &exchange, workspace, lane, 16000u, 6000u, err, sizeof(err)));
    ASSERT_EQ(dc_gpu_validation_errors(view->gpu), 0u);
    dc_level_view_discard(view);
    dc_offscreen_destroy(workspace);
    PASS();
}

#endif
