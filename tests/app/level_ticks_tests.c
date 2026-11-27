#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "ferrum/fire/bulk_phase_gpu.h"
#include "ferrum/fire/gpu.h"
#include "../../src/app/level_internal.h"
#include "../../src/app/player_session_internal.h"
#include "../../src/app/view_config.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++g_fail; return; \
    } \
} while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() ++g_pass

static struct {
    float elapsed[24], player_elapsed[24], seconds;
    unsigned steps, player_steps, presents, presented_steps, reject_step;
    bool reject_present, restore_first_from_checkpoint;
    uint16_t material[DC_GPU_CHUNK_SLOTS];
    unsigned uploads[DC_GPU_CHUNK_SLOTS];
} g_frame;

bool __wrap_fr_gpu_upload_structure_chunk_with_time(dc_gpu_t *gpu, uint32_t slot,
    const dc_chunk_t *chunk, bool *restored, dc_chunk_time_t *time, char *err, uint32_t cap) {
    (void)gpu; (void)err; (void)cap;
    *restored = g_frame.restore_first_from_checkpoint && slot == 0;
    *time = (dc_chunk_time_t){0};
    g_frame.material[slot] = *restored ? DC_MATERIAL_STONE : chunk->cells[0].material;
    ++g_frame.uploads[slot];
    return true;
}

bool __wrap_dc_gpu_download_chunk(dc_gpu_t *gpu, uint32_t slot, dc_chunk_t *chunk,
                                  char *err, uint32_t cap) {
    (void)gpu; (void)err; (void)cap;
    chunk->cells[0].material = g_frame.material[slot];
    return true;
}

bool __wrap_dc_gpu_shift_velocity(dc_gpu_t *gpu, int32_t dx, int32_t dy,
                                  char *err, uint32_t cap) {
    (void)gpu; (void)dx; (void)dy; (void)err; (void)cap;
    return true;
}

bool __wrap_fr_renderer_upload_chunk_colors(dc_gpu_t *gpu, uint32_t slot,
    const dc_chunk_colors_t *colors, char *err, uint32_t cap) {
    (void)gpu; (void)slot; (void)colors; (void)err; (void)cap;
    return true;
}

bool __wrap_fr_renderer_download_chunk_colors(dc_gpu_t *gpu, uint32_t slot,
    dc_chunk_colors_t *colors, char *err, uint32_t cap) {
    (void)gpu; (void)slot; (void)err; (void)cap;
    memset(colors, 0, sizeof(*colors));
    return true;
}

bool __wrap_fr_gpu_reactive_import_chunk(dc_gpu_t *gpu, uint32_t slot,
    const dc_chunk_t *chunk, const fr_reactive_chunk_t *channel, char *err, uint32_t cap) {
    (void)gpu; (void)slot; (void)chunk; (void)channel; (void)err; (void)cap;
    return true;
}

bool __wrap_fr_gpu_reactive_export_chunk(dc_gpu_t *gpu, uint32_t slot,
    const dc_chunk_t *chunk, fr_reactive_chunk_t *out, char *err, uint32_t cap) {
    (void)gpu; (void)slot; (void)chunk; (void)err; (void)cap;
    *out = (fr_reactive_chunk_t){.next_id = 1};
    return true;
}

bool __wrap_fr_bulk_phase_gpu_import_chunk(dc_gpu_t *gpu, uint32_t slot,
    const dc_chunk_t *chunk, const fr_bulk_phase_chunk_t *bulk, char *err, uint32_t cap) {
    (void)gpu; (void)slot; (void)chunk; (void)bulk; (void)err; (void)cap;
    return true;
}

bool __wrap_fr_bulk_phase_gpu_export_chunk(dc_gpu_t *gpu, uint32_t slot,
    fr_bulk_phase_chunk_t *bulk, char *err, uint32_t cap) {
    (void)gpu; (void)slot; (void)err; (void)cap;
    memset(bulk, 0, sizeof(*bulk));
    return true;
}

bool __wrap_dc_gpu_set_page(dc_gpu_t *gpu, uint32_t x, uint32_t y, uint32_t slot,
                            char *err, uint32_t cap) {
    (void)gpu; (void)x; (void)y; (void)slot; (void)err; (void)cap;
    return true;
}

bool __wrap_dc_gpu_set_body_origin(dc_gpu_t *gpu, dc_chunk_coord_t origin,
                                   char *err, uint32_t cap) {
    (void)gpu; (void)origin; (void)err; (void)cap;
    return true;
}

bool __wrap_dc_gpu_prepare_body_origin(dc_gpu_t *gpu, dc_chunk_coord_t origin,
                                       char *err, uint32_t cap) {
    return __wrap_dc_gpu_set_body_origin(gpu, origin, err, cap);
}

bool __wrap_dc_gpu_set_tick_water_source(dc_gpu_t *gpu, bool enabled,
                                         uint32_t x, uint32_t y) {
    (void)gpu; (void)enabled; (void)x; (void)y;
    return true;
}

bool __wrap_dc_gpu_set_tick_seconds(dc_gpu_t *gpu, float seconds) {
    (void)gpu;
    g_frame.seconds = seconds;
    return seconds > 0.f && seconds <= .25f;
}

bool __wrap_dc_gpu_tick_step(dc_gpu_t *gpu, char *err, uint32_t cap) {
    (void)gpu; (void)err; (void)cap;
    if (g_frame.steps >= sizeof(g_frame.elapsed) / sizeof(g_frame.elapsed[0]))
        return false;
    if (g_frame.steps + 1u == g_frame.reject_step) return false;
    g_frame.elapsed[g_frame.steps++] = g_frame.seconds;
    return true;
}

void __wrap_dc_players_prepare_view(dc_level_view_t *view) {
    (void)view;
}

bool __wrap_dc_players_present(dc_level_view_t *view, char *err, uint32_t cap) {
    (void)view; (void)err; (void)cap;
    return true;
}

bool __wrap_dc_players_step(dc_level_view_t *view, float seconds, char *err, uint32_t cap) {
    (void)view; (void)err; (void)cap;
    if (g_frame.player_steps >= sizeof(g_frame.player_elapsed) / sizeof(g_frame.player_elapsed[0]))
        return false;
    g_frame.player_elapsed[g_frame.player_steps++] = seconds;
    return true;
}

bool __wrap_dc_gpu_present_chunks_steps_progress(dc_gpu_t *gpu, uint32_t steps,
                                                 uint32_t *completed,
                                                 char *err, uint32_t cap) {
    *completed = 0;
    ++g_frame.presents;
    g_frame.presented_steps += steps;
    for (uint32_t i = 0; i < steps; ++i) {
        if (!__wrap_dc_gpu_tick_step(gpu, err, cap)) return false;
        ++*completed;
    }
    return !g_frame.reject_present;
}

static dc_level_view_t *ready_view(bool leave_first_loading) {
    memset(&g_frame, 0, sizeof(g_frame));
    dc_level_view_t *view = calloc(1, sizeof(*view));
    if (!view) return NULL;
    if (!dc_chunk_table_init(&view->table, SIM_CHUNKS_X * SIM_CHUNKS_Y)) goto fail;
    for (uint32_t y = 0; y < SIM_CHUNKS_Y; ++y) {
        for (uint32_t x = 0; x < SIM_CHUNKS_X; ++x) {
            dc_chunk_coord_t coord = {(int64_t)x - HALO_CHUNKS, (int64_t)y - HALO_CHUNKS};
            uint32_t index;
            uint64_t generation;
            if (!dc_chunk_table_begin_load(&view->table, coord, &index, &generation)) goto fail;
            if ((x || y || !leave_first_loading) &&
                !dc_chunk_table_finish_load(&view->table, index, generation)) goto fail;
        }
    }
    return view;
fail:
    dc_level_view_discard(view);
    return NULL;
}

static void test_unequal_ticks_reach_solver_in_requested_order(void) {
    dc_level_view_t *view = ready_view(false);
    ASSERT_TRUE(view);
    const float steps[] = {.01f, .03f, .03f, .20f, .18f, .16f, .14f, .12f, .10f};
    for (unsigned i = 0; i < sizeof(steps) / sizeof(steps[0]); ++i)
        ASSERT_TRUE(dc_level_view_step_timed(view, steps[i], NULL, 0));
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    dc_level_view_discard(view);
    ASSERT_EQ(g_frame.steps, sizeof(steps) / sizeof(steps[0]));
    ASSERT_TRUE(memcmp(g_frame.elapsed, steps, sizeof(steps)) == 0);
    ASSERT_INT_EQ(g_frame.presents, 1);
    PASS();
}

static void test_equal_ticks_share_one_presentation_batch(void) {
    dc_level_view_t *view = ready_view(false);
    ASSERT_TRUE(view);
    for (unsigned i = 0; i < 4; ++i)
        ASSERT_TRUE(dc_level_view_step_timed(view, .015625f, NULL, 0));
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    dc_level_view_discard(view);
    ASSERT_INT_EQ(g_frame.presents, 1);
    ASSERT_INT_EQ(g_frame.presented_steps, 4);
    ASSERT_INT_EQ(g_frame.steps, 4);
    PASS();
}

static void test_equal_ticks_completed_before_present_failure_are_not_replayed(void) {
    dc_level_view_t *view = ready_view(false);
    ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_step_timed(view, .02f, NULL, 0));
    ASSERT_TRUE(dc_level_view_step_timed(view, .02f, NULL, 0));
    g_frame.reject_present = true;
    ASSERT_TRUE(!dc_level_view_tick(view, NULL, 0));
    ASSERT_EQ(g_frame.steps, 2u);
    g_frame.reject_present = false;
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    dc_level_view_discard(view);
    printf("Equal batch requested %u ticks across two presents and completed %u\n",
           g_frame.presented_steps, g_frame.steps);
    ASSERT_EQ(g_frame.presents, 2u);
    ASSERT_EQ(g_frame.presented_steps, 2u);
    ASSERT_EQ(g_frame.steps, 2u);
    PASS();
}

static void test_partial_equal_batch_retries_only_its_unfinished_suffix(void) {
    dc_level_view_t *view = ready_view(false);
    ASSERT_TRUE(view);
    for (unsigned i = 0; i < 3; ++i)
        ASSERT_TRUE(dc_level_view_step_timed(view, .02f, NULL, 0));
    g_frame.reject_step = 2;
    ASSERT_TRUE(!dc_level_view_tick(view, NULL, 0));
    ASSERT_EQ(g_frame.steps, 1u);
    g_frame.reject_step = 0;
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    dc_level_view_discard(view);
    printf("Partial equal batch completed %u ticks across two attempts\n", g_frame.steps);
    ASSERT_EQ(g_frame.presents, 2u);
    ASSERT_EQ(g_frame.presented_steps, 5u);
    ASSERT_EQ(g_frame.steps, 3u);
    PASS();
}

static void test_failed_present_before_simulation_keeps_the_equal_batch(void) {
    dc_level_view_t *view = ready_view(false);
    ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_step_timed(view, .02f, NULL, 0));
    ASSERT_TRUE(dc_level_view_step_timed(view, .02f, NULL, 0));
    g_frame.reject_step = 1;
    ASSERT_TRUE(!dc_level_view_tick(view, NULL, 0));
    ASSERT_EQ(g_frame.steps, 0u);
    g_frame.reject_step = 0;
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    dc_level_view_discard(view);
    ASSERT_EQ(g_frame.presents, 2u);
    ASSERT_EQ(g_frame.presented_steps, 4u);
    ASSERT_EQ(g_frame.steps, 2u);
    PASS();
}

static void test_invalid_time_is_rejected_without_changing_valid_requests(void) {
    dc_level_view_t *view = ready_view(false);
    ASSERT_TRUE(view);
    bool zero = dc_level_view_step_timed(view, 0.f, NULL, 0);
    bool negative = dc_level_view_step_timed(view, -.01f, NULL, 0);
    bool nan = dc_level_view_step_timed(view, NAN, NULL, 0);
    bool too_long = dc_level_view_step_timed(view, .251f, NULL, 0);
    bool valid = dc_level_view_step_timed(view, .25f, NULL, 0);
    bool tick = dc_level_view_tick(view, NULL, 0);
    dc_level_view_discard(view);
    ASSERT_TRUE(!zero && !negative && !nan && !too_long);
    ASSERT_TRUE(valid && tick);
    ASSERT_INT_EQ(g_frame.steps, 1);
    ASSERT_EQ(g_frame.elapsed[0], .25f);
    PASS();
}

static void test_mixed_ticks_are_consumed_before_retrying_failed_present(void) {
    dc_level_view_t *view = ready_view(false);
    ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_step_timed(view, .01f, NULL, 0));
    ASSERT_TRUE(dc_level_view_step_timed(view, .02f, NULL, 0));
    g_frame.reject_present = true;
    ASSERT_TRUE(!dc_level_view_tick(view, NULL, 0));
    g_frame.reject_present = false;
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    dc_level_view_discard(view);
    ASSERT_INT_EQ(g_frame.presents, 2);
    ASSERT_INT_EQ(g_frame.steps, 2);
    ASSERT_EQ(g_frame.elapsed[0], .01f);
    ASSERT_EQ(g_frame.elapsed[1], .02f);
    PASS();
}

static void test_loading_frame_does_not_build_catch_up_debt(void) {
    dc_level_view_t *view = ready_view(true);
    ASSERT_TRUE(view);
    dc_chunk_slot_t *timed = &view->table.slots[1];
    timed->time = (dc_chunk_time_t){.seconds = 4.5, .known = true};
    ASSERT_TRUE(dc_level_view_step_timed(view, .01f, NULL, 0));
    ASSERT_TRUE(dc_level_view_step_timed(view, .02f, NULL, 0));
    ASSERT_TRUE(!timed->dirty);
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    ASSERT_TRUE(timed->time.seconds == 4.5 && !timed->dirty);
    ASSERT_EQ(view->world_seconds, 0.);
    ASSERT_TRUE(dc_chunk_table_finish_load(&view->table, 0, view->table.slots[0].generation));
    ASSERT_TRUE(dc_level_view_step_timed(view, .03f, NULL, 0));
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    ASSERT_TRUE(timed->time.seconds == 4.5 + (double).03f && timed->dirty);
    ASSERT_EQ(view->world_seconds, (double).03f);
    ASSERT_TRUE(!view->table.slots[0].time.known);
    dc_level_view_discard(view);
    ASSERT_INT_EQ(g_frame.presents, 2);
    ASSERT_INT_EQ(g_frame.steps, 1);
    ASSERT_EQ(g_frame.elapsed[0], .03f);
    PASS();
}

static void test_partial_tick_failure_retains_only_unfinished_steps(void) {
    dc_level_view_t *view = ready_view(false);
    ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_step_timed(view, .01f, NULL, 0));
    ASSERT_TRUE(dc_level_view_step_timed(view, .02f, NULL, 0));
    g_frame.reject_step = 2;
    ASSERT_TRUE(!dc_level_view_tick(view, NULL, 0));
    g_frame.reject_step = 0;
    ASSERT_TRUE(dc_level_view_step_timed(view, .03f, NULL, 0));
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    dc_level_view_discard(view);
    const float steps[] = {.01f, .02f, .03f};
    ASSERT_INT_EQ(g_frame.presents, 1);
    ASSERT_INT_EQ(g_frame.steps, 3);
    ASSERT_TRUE(memcmp(g_frame.elapsed, steps, sizeof(steps)) == 0);
    PASS();
}

static void test_player_actions_receive_the_same_ordered_time_as_physics(void) {
    dc_level_view_t *view = ready_view(false);
    ASSERT_TRUE(view);
    struct dc_player_session session = {0};
    bool okay = true;
    const float steps[] = {.02f, .01f, .02f};
    view->players = &session;
    for (unsigned i = 0; okay && i < sizeof(steps) / sizeof(steps[0]); ++i)
        okay = dc_level_view_step_timed(view, steps[i], NULL, 0);
    okay = okay && dc_level_view_tick(view, NULL, 0);
    view->players = NULL;
    dc_level_view_discard(view);
    ASSERT_TRUE(okay);
    ASSERT_INT_EQ(g_frame.steps, 3);
    ASSERT_INT_EQ(g_frame.player_steps, 3);
    ASSERT_TRUE(memcmp(g_frame.elapsed, steps, sizeof(steps)) == 0);
    ASSERT_TRUE(memcmp(g_frame.player_elapsed, steps, sizeof(steps)) == 0);
    PASS();
}

static bool pan_until_ready(dc_level_view_t *view, int32_t dx) {
    return dc_level_view_move(view, dx, 0) && dc_level_view_wait_visible(view, 3000, NULL, 0);
}

static bool load_first_clock_chunk(dc_level_view_t *view, const char *directory) {
    view->stream = dc_stream_create(directory, 42, 64);
    const dc_chunk_slot_t *slot = &view->table.slots[0];
    return view->stream && dc_stream_request_load(view->stream, slot->coord, slot->generation + 100u) &&
           dc_stream_request_load(view->stream, slot->coord, slot->generation) &&
           dc_level_view_wait_visible(view, 3000, NULL, 0);
}

static bool write_timed_chunk(const char *directory, dc_chunk_coord_t coord) {
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return false;
    dc_streamer_t *writer = dc_stream_create(directory, 42, 4);
    chunk->coord = coord;
    chunk->cells[0].material = DC_MATERIAL_WATER;
    bool saved = writer && dc_stream_request_save_snapshot(writer, chunk, NULL, NULL, NULL,
        NULL, NULL, (dc_stream_chunk_time_t){.seconds = 8., .known = true}, 999u);
    dc_stream_destroy(writer);
    free(chunk);
    return saved;
}

static void test_timed_chunk_retains_confirmed_ticks_across_pan_and_worker_reopen(void) {
    char directory[] = "build/level_clock_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = ready_view(true);
    ASSERT_TRUE(view);
    const dc_chunk_coord_t coord = view->table.slots[0].coord;
    ASSERT_TRUE(write_timed_chunk(directory, coord));
    ASSERT_TRUE(load_first_clock_chunk(view, directory));
    ASSERT_EQ(view->world_seconds, 0.);
    ASSERT_EQ(g_frame.uploads[0], 1u);
    ASSERT_TRUE(dc_level_view_step_timed(view, .125f, NULL, 0));
    ASSERT_TRUE(dc_level_view_step_timed(view, .125f, NULL, 0));
    g_frame.reject_present = true;
    ASSERT_TRUE(!dc_level_view_tick(view, NULL, 0));
    g_frame.reject_present = false;
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    ASSERT_TRUE(dc_level_view_step_timed(view, .0625f, NULL, 0));
    ASSERT_TRUE(dc_level_view_step_timed(view, .125f, NULL, 0));
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    ASSERT_TRUE(pan_until_ready(view, 1));
    ASSERT_TRUE(pan_until_ready(view, -1));
    ASSERT_TRUE(dc_level_view_step_timed(view, .03125f, NULL, 0));
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    ASSERT_TRUE(pan_until_ready(view, 1));
    dc_level_view_discard(view);

    view = ready_view(true);
    ASSERT_TRUE(view && load_first_clock_chunk(view, directory));
    ASSERT_TRUE(dc_level_view_step_timed(view, .015625f, NULL, 0));
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    ASSERT_TRUE(pan_until_ready(view, 1));
    dc_level_view_discard(view);

    dc_streamer_t *reader = dc_stream_create(directory, 42, 2);
    ASSERT_TRUE(reader && dc_stream_request_load(reader, coord, 1u));
    dc_stream_result_t result = {0};
    uint64_t deadline = SDL_GetTicks64() + 3000u;
    bool received = dc_stream_poll(reader, &result);
    while (!received && SDL_GetTicks64() < deadline) {
        SDL_Delay(1);
        received = dc_stream_poll(reader, &result);
    }
    dc_stream_destroy(reader);
    const dc_stream_chunk_time_t time = result.time;
    const bool loaded = received && result.kind == DC_STREAM_LOADED && result.chunk &&
                        result.chunk->cells[0].material == DC_MATERIAL_WATER;
    dc_stream_result_release(&result);
    printf("Chunk time known %d, %.9f seconds after confirmed ticks and cold returns\n",
           time.known, time.seconds);
    ASSERT_TRUE(loaded);
    ASSERT_TRUE(time.known && time.seconds == 8.484375);
    PASS();
}

static void test_untimed_structure_checkpoint_does_not_inherit_stream_time(void) {
    char directory[] = "build/level_clock_structure_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = ready_view(true);
    ASSERT_TRUE(view && write_timed_chunk(directory, view->table.slots[0].coord));
    g_frame.restore_first_from_checkpoint = true;
    ASSERT_TRUE(load_first_clock_chunk(view, directory));
    ASSERT_TRUE(!view->table.slots[0].time.known);
    ASSERT_EQ(g_frame.material[0], DC_MATERIAL_STONE);
    dc_level_view_discard(view);
    PASS();
}

static void test_generated_chunks_start_at_confirmed_world_time(void) {
    char directory[] = "build/level_world_clock_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = ready_view(false);
    ASSERT_TRUE(view);
    const dc_chunk_coord_t legacy = view->table.slots[0].coord;
    view->save = dc_world_save_open(directory, NULL, 0);
    view->stream = dc_stream_create(directory, 42, 64);
    ASSERT_TRUE(view->save && view->stream);
    ASSERT_TRUE(dc_level_view_step_timed(view, .125f, NULL, 0));
    ASSERT_TRUE(dc_level_view_step_timed(view, .125f, NULL, 0));
    g_frame.reject_present = true;
    ASSERT_TRUE(!dc_level_view_tick(view, NULL, 0));
    ASSERT_EQ(view->world_seconds, .25);
    g_frame.reject_present = false;
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    ASSERT_EQ(view->world_seconds, .25);
    ASSERT_TRUE(dc_level_view_step_timed(view, .0625f, NULL, 0));
    ASSERT_TRUE(dc_level_view_step_timed(view, .125f, NULL, 0));
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    const dc_chunk_coord_t generated = {VIEW_CHUNKS_X + HALO_CHUNKS, 0};
    ASSERT_TRUE(pan_until_ready(view, 1));
    uint32_t index;
    ASSERT_TRUE(!dc_chunk_table_find(&view->table, legacy, &index));
    ASSERT_TRUE(dc_chunk_table_find(&view->table, generated, &index));
    dc_chunk_time_t time = view->table.slots[index].time;
    dc_level_view_discard(view);
    printf("Generated chunk time known %d, %.9f seconds at admission\n", time.known, time.seconds);
    ASSERT_TRUE(time.known && time.seconds == .4375);

    view = ready_view(true);
    ASSERT_TRUE(view);
    view->save = dc_world_save_open(directory, NULL, 0);
    ASSERT_TRUE(view->save && dc_world_save_load_clock(view->save, &view->world_seconds, NULL, 0));
    ASSERT_EQ(view->world_seconds, .4375);
    ASSERT_TRUE(load_first_clock_chunk(view, directory));
    ASSERT_EQ(view->world_seconds, .4375);
    ASSERT_TRUE(!view->table.slots[0].time.known);
    dc_level_view_discard(view);
    PASS();
}

int main(void) {
    RUN(test_unequal_ticks_reach_solver_in_requested_order);
    RUN(test_equal_ticks_share_one_presentation_batch);
    RUN(test_equal_ticks_completed_before_present_failure_are_not_replayed);
    RUN(test_partial_equal_batch_retries_only_its_unfinished_suffix);
    RUN(test_failed_present_before_simulation_keeps_the_equal_batch);
    RUN(test_invalid_time_is_rejected_without_changing_valid_requests);
    RUN(test_mixed_ticks_are_consumed_before_retrying_failed_present);
    RUN(test_loading_frame_does_not_build_catch_up_debt);
    RUN(test_partial_tick_failure_retains_only_unfinished_steps);
    RUN(test_player_actions_receive_the_same_ordered_time_as_physics);
    RUN(test_timed_chunk_retains_confirmed_ticks_across_pan_and_worker_reopen);
    RUN(test_untimed_structure_checkpoint_does_not_inherit_stream_time);
    RUN(test_generated_chunks_start_at_confirmed_world_time);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
