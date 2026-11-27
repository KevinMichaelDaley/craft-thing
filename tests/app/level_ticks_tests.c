#define _POSIX_C_SOURCE 200809L

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <SDL.h>

#include "ferrum/fire/bulk_phase_gpu.h"
#include "ferrum/fire/gpu.h"
#include "../../src/app/level_internal.h"
#include "../../src/app/player_session_internal.h"
#include "../../src/app/view_config.h"
#include "../../src/vulkan/gpu_internal.h"

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
    unsigned steps, player_steps, presents, presented_steps, reject_step, shifts;
    bool reject_present, restore_first_from_checkpoint;
    bool reject_next_load, reject_next_save, hold_replies, inject_reply;
    unsigned loads_before_reject;
    dc_chunk_coord_t requested_load;
    dc_stream_result_t injected_reply;
    uint64_t requested_save;
    uint16_t material[DC_GPU_CHUNK_SLOTS];
    unsigned uploads[DC_GPU_CHUNK_SLOTS];
} g_frame;

bool __real_dc_stream_request_load(dc_streamer_t *stream, dc_chunk_coord_t coord, uint64_t generation);
bool __wrap_dc_stream_request_load(dc_streamer_t *stream, dc_chunk_coord_t coord, uint64_t generation) {
    if (!g_frame.reject_next_load) return __real_dc_stream_request_load(stream, coord, generation);
    if (g_frame.loads_before_reject) {
        --g_frame.loads_before_reject;
        return __real_dc_stream_request_load(stream, coord, generation);
    }
    g_frame.reject_next_load = false;
    g_frame.requested_load = coord;
    return false;
}

bool __real_dc_stream_request_save_snapshot(dc_streamer_t *, const dc_chunk_t *,
    const dc_chunk_colors_t *, const fr_reactive_chunk_t *, const float *,
    const fr_gas_checkpoint_chunk_t *, const fr_bulk_phase_chunk_t *, dc_stream_chunk_time_t, uint64_t);
bool __wrap_dc_stream_request_save_snapshot(dc_streamer_t *stream, const dc_chunk_t *chunk,
    const dc_chunk_colors_t *colors, const fr_reactive_chunk_t *reactive, const float *water,
    const fr_gas_checkpoint_chunk_t *gas, const fr_bulk_phase_chunk_t *bulk,
    dc_stream_chunk_time_t time, uint64_t generation) {
    g_frame.requested_save = generation;
    if (g_frame.reject_next_save) {
        g_frame.reject_next_save = false;
        return false;
    }
    return __real_dc_stream_request_save_snapshot(stream, chunk, colors, reactive, water, gas, bulk,
                                                  time, generation);
}

bool __real_dc_stream_poll(dc_streamer_t *stream, dc_stream_result_t *out);
bool __wrap_dc_stream_poll(dc_streamer_t *stream, dc_stream_result_t *out) {
    if (g_frame.inject_reply) {
        *out = g_frame.injected_reply;
        g_frame.inject_reply = false;
        return true;
    }
    return !g_frame.hold_replies && __real_dc_stream_poll(stream, out);
}

bool __wrap_dc_gpu_upload_streamed_chunk(dc_gpu_t *gpu, uint32_t slot, const dc_chunk_t *chunk,
    const dc_gpu_stream_fluids_t *stream, bool *restored, dc_chunk_time_t *time,
    bool *gas_admitted, char *err, uint32_t cap) {
    (void)gpu; (void)err; (void)cap;
    *restored = g_frame.restore_first_from_checkpoint && slot == 0;
    *time = (dc_chunk_time_t){0};
    *gas_admitted = stream->gas != NULL || stream->ambient_gas;
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
    ++g_frame.shifts;
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

static bool load_saved_chunk(const char *directory, dc_chunk_coord_t coord, dc_stream_result_t *out) {
    dc_streamer_t *stream = dc_stream_create(directory, 42, 2);
    if (!stream) return false;
    bool received = false;
    if (dc_stream_request_load(stream, coord, 1u)) {
        uint64_t deadline = SDL_GetTicks64() + 3000u;
        do {
            received = dc_stream_poll(stream, out);
            if (!received) SDL_Delay(1);
        } while (!received && SDL_GetTicks64() < deadline);
    }
    dc_stream_destroy(stream);
    return received && out->kind == DC_STREAM_LOADED;
}

static bool wait_worker_error(dc_level_view_t *view, char *err, uint32_t cap) {
    uint64_t deadline = SDL_GetTicks64() + 3000u;
    do {
        if (!dc_level_view_tick(view, err, cap)) return true;
        SDL_Delay(1);
    } while (SDL_GetTicks64() < deadline);
    return false;
}

static bool chunk_record_path(const char *directory, dc_chunk_coord_t coord, bool temporary,
                              char *path, size_t capacity) {
    int length = snprintf(path, capacity, "%s/chunk_%016" PRIx64 "_%016" PRIx64 ".bin%s",
        directory, (uint64_t)coord.x, (uint64_t)coord.y, temporary ? ".tmp" : "");
    return length > 0 && (size_t)length < capacity;
}

static void test_camera_retries_a_rejected_worker_load(void) {
    char directory[] = "build/retry_load_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = ready_view(false);
    ASSERT_TRUE(view && (view->stream = dc_stream_create(directory, 42, 64)));
    g_frame.reject_next_load = true;
    g_frame.loads_before_reject = 1u;
    ASSERT_TRUE(dc_level_view_move(view, 1, 0));
    ASSERT_TRUE(!dc_level_view_tick(view, NULL, 0));
    ASSERT_TRUE(view->mapped_origin.x == view->origin.x && view->pending_chunk_dx == 0);
    const dc_chunk_coord_t coord = g_frame.requested_load;
    ASSERT_TRUE(!dc_chunk_table_find(&view->table, coord, NULL));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 3000, NULL, 0));
    ASSERT_EQ(g_frame.shifts, 1u);
    uint32_t index;
    ASSERT_TRUE(dc_chunk_table_find(&view->table, coord, &index));
    ASSERT_EQ(g_frame.uploads[index], 1u);
    dc_level_view_discard(view);
    PASS();
}

static void test_camera_retries_a_rejected_save_and_ignores_its_old_reply(void) {
    char directory[] = "build/retry_save_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = ready_view(false);
    ASSERT_TRUE(view && (view->stream = dc_stream_create(directory, 42, 64)));
    dc_chunk_slot_t *slot = &view->table.slots[0];
    const dc_chunk_coord_t coord = slot->coord;
    const uint64_t owner = slot->generation;
    slot->time = (dc_chunk_time_t){.seconds = 7.5, .known = true};
    g_frame.material[0] = DC_MATERIAL_STONE;
    ASSERT_TRUE(dc_chunk_table_mark_dirty(&view->table, 0u));
    ASSERT_TRUE(dc_level_view_move(view, 1, 0));
    g_frame.reject_next_save = true;
    ASSERT_TRUE(!dc_level_view_tick(view, NULL, 0));
    const uint64_t rejected = g_frame.requested_save;
    ASSERT_TRUE(slot->state == DC_SLOT_SLEEPING && slot->dirty && slot->generation == owner);

    g_frame.hold_replies = true;
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    ASSERT_TRUE(slot->state == DC_SLOT_SAVING && slot->dirty && slot->generation == owner);
    ASSERT_TRUE(g_frame.requested_save != owner && g_frame.requested_save != rejected);
    g_frame.injected_reply = (dc_stream_result_t){.kind = DC_STREAM_SAVED, .coord = coord, .generation = rejected};
    g_frame.inject_reply = true;
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    ASSERT_TRUE(slot->state == DC_SLOT_SAVING && slot->dirty);
    g_frame.hold_replies = false;
    ASSERT_TRUE(dc_level_view_wait_visible(view, 3000, NULL, 0));
    dc_level_view_discard(view);

    dc_stream_result_t saved = {0};
    ASSERT_TRUE(load_saved_chunk(directory, coord, &saved));
    ASSERT_TRUE(saved.time.known && saved.time.seconds == 7.5 && saved.chunk->cells[0].material == DC_MATERIAL_STONE);
    dc_stream_result_release(&saved);
    PASS();
}

static void test_failed_disk_write_preserves_the_previous_file_and_dirty_owner(void) {
    char directory[] = "build/retry_disk_save_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = ready_view(false);
    ASSERT_TRUE(view);
    dc_chunk_slot_t *slot = &view->table.slots[0];
    const uint64_t owner = slot->generation;
    const dc_chunk_coord_t coord = slot->coord;
    ASSERT_TRUE(write_timed_chunk(directory, coord));
    ASSERT_TRUE(view->stream = dc_stream_create(directory, 42, 64));
    char path[128], error[96];
    ASSERT_TRUE(chunk_record_path(directory, coord, true, path, sizeof(path)) && mkdir(path, 0700) == 0);
    slot->time = (dc_chunk_time_t){.seconds = 9., .known = true};
    g_frame.material[0] = DC_MATERIAL_STONE;
    ASSERT_TRUE(dc_chunk_table_mark_dirty(&view->table, 0u) && dc_level_view_move(view, 1, 0));
    ASSERT_TRUE(wait_worker_error(view, error, sizeof(error)));
    ASSERT_EQ(strcmp(error, "Chunk worker load or save failed"), 0);
    ASSERT_TRUE(slot->state == DC_SLOT_SLEEPING && slot->generation == owner && slot->dirty);

    dc_stream_result_t saved = {0};
    ASSERT_TRUE(load_saved_chunk(directory, coord, &saved));
    ASSERT_TRUE(saved.time.known && saved.time.seconds == 8. && saved.chunk->cells[0].material == DC_MATERIAL_WATER);
    dc_stream_result_release(&saved);
    ASSERT_EQ(rmdir(path), 0);
    ASSERT_TRUE(dc_level_view_wait_visible(view, 3000, NULL, 0));
    dc_level_view_discard(view);
    ASSERT_TRUE(load_saved_chunk(directory, coord, &saved));
    ASSERT_TRUE(saved.time.known && saved.time.seconds == 9. && saved.chunk->cells[0].material == DC_MATERIAL_STONE);
    dc_stream_result_release(&saved);
    PASS();
}

static void test_corrupt_disk_load_allows_camera_retry_after_repair(void) {
    char directory[] = "build/retry_disk_load_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = ready_view(false);
    ASSERT_TRUE(view && (view->stream = dc_stream_create(directory, 42, 64)));
    const dc_chunk_coord_t coord = {VIEW_CHUNKS_X + HALO_CHUNKS, -HALO_CHUNKS};
    char path[128], error[96];
    ASSERT_TRUE(chunk_record_path(directory, coord, false, path, sizeof(path)));
    FILE *file = fopen(path, "wb");
    ASSERT_TRUE(file && fputs("broken", file) >= 0 && fclose(file) == 0);
    ASSERT_TRUE(dc_level_view_move(view, 1, 0) && wait_worker_error(view, error, sizeof(error)));
    ASSERT_EQ(strcmp(error, "Chunk worker load or save failed"), 0);
    ASSERT_TRUE(!dc_chunk_table_find(&view->table, coord, NULL));
    ASSERT_EQ(unlink(path), 0);
    ASSERT_TRUE(dc_level_view_wait_visible(view, 3000, NULL, 0));
    ASSERT_TRUE(dc_chunk_table_find(&view->table, coord, NULL));
    dc_level_view_discard(view);
    PASS();
}

static void test_duplicate_load_reply_cannot_overwrite_a_live_slot(void) {
    dc_level_view_t *view = ready_view(false);
    ASSERT_TRUE(view);
    dc_chunk_slot_t *slot = &view->table.slots[0];
    slot->time = (dc_chunk_time_t){.seconds = 18., .known = true};
    dc_chunk_t *duplicate = calloc(1, sizeof(*duplicate));
    ASSERT_TRUE(duplicate);
    duplicate->coord = slot->coord;
    g_frame.injected_reply = (dc_stream_result_t){.kind = DC_STREAM_LOADED,
        .coord = slot->coord, .generation = slot->generation, .chunk = duplicate};
    g_frame.inject_reply = true;
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    ASSERT_EQ(g_frame.uploads[0], 0u);
    ASSERT_TRUE(slot->time.known && slot->time.seconds == 18.);
    dc_level_view_discard(view);
    PASS();
}

static void test_transfer_releases_rejected_request_and_rebinds_failed_worker_destination(void) {
    char directory[] = "build/retry_transfer_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = ready_view(false);
    ASSERT_TRUE(view && (view->stream = dc_stream_create(directory, 42, 64)));
    ASSERT_TRUE(dc_chunk_table_set_active(&view->table, 0u, false));
    ASSERT_TRUE(dc_chunk_table_evict(&view->table, 0u, 0u));
    const uint32_t source_slot = SIM_CHUNKS_X - 1u;
    const dc_chunk_coord_t source = view->table.slots[source_slot].coord;
    const dc_chunk_coord_t target = {source.x + 1, source.y};
    const int64_t x = target.x * DC_CHUNK_SIDE, y = target.y * DC_CHUNK_SIDE;
    char path[128];
    ASSERT_TRUE(chunk_record_path(directory, target, false, path, sizeof(path)));
    FILE *file = fopen(path, "wb");
    ASSERT_TRUE(file && fputs("broken", file) >= 0 && fclose(file) == 0);

    g_frame.reject_next_load = true;
    ASSERT_TRUE(!dc_level_view_queue_transfer(view, x - 1, y, x, y, 1u, DC_GPU_TRANSFER_SCALAR, NULL, 0));
    ASSERT_TRUE(!view->transfer_pending && !view->table.slots[source_slot].pinned);
    ASSERT_TRUE(!dc_chunk_table_find(&view->table, target, NULL));
    ASSERT_TRUE(dc_level_view_queue_transfer(view, x - 1, y, x, y, 1u, DC_GPU_TRANSFER_SCALAR, NULL, 0));
    ASSERT_TRUE(view->destination_bound);
    const uint64_t rejected_destination = view->to_generation;
    ASSERT_TRUE(wait_worker_error(view, NULL, 0));
    ASSERT_TRUE(view->transfer_pending && !view->destination_bound && view->table.slots[source_slot].pinned);
    ASSERT_TRUE(!dc_chunk_table_find(&view->table, target, NULL));
    ASSERT_EQ(unlink(path), 0);
    g_frame.hold_replies = true;
    ASSERT_TRUE(dc_level_view_tick(view, NULL, 0));
    ASSERT_TRUE(view->destination_bound && view->to_generation != rejected_destination);
    ASSERT_TRUE(view->table.slots[view->to_slot].pinned && view->table.slots[view->to_slot].state == DC_SLOT_LOADING);
    g_frame.hold_replies = false;
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
    RUN(test_camera_retries_a_rejected_worker_load);
    RUN(test_camera_retries_a_rejected_save_and_ignores_its_old_reply);
    RUN(test_failed_disk_write_preserves_the_previous_file_and_dirty_owner);
    RUN(test_corrupt_disk_load_allows_camera_retry_after_repair);
    RUN(test_duplicate_load_reply_cannot_overwrite_a_live_slot);
    RUN(test_transfer_releases_rejected_request_and_rebinds_failed_worker_destination);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
