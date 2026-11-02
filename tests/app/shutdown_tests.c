#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/app/level.h"
#include "../../src/app/shutdown.h"
#include "../../src/app/view_config.h"

static int g_pass, g_fail;
#define RUN(fn)                                                                \
    do {                                                                       \
        printf("RUN %s\n", #fn);                                               \
        fn();                                                                  \
    } while (0)
#define ASSERT_TRUE(e)                                                         \
    do {                                                                       \
        if (!(e)) {                                                            \
            printf("FAIL %d: %s\n", __LINE__, #e);                             \
            g_fail++;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_termination_signals_request_shutdown_once(void) {
    dc_app_install_shutdown_handlers();
    ASSERT_TRUE(!dc_app_shutdown_requested());
    ASSERT_INT_EQ(raise(SIGTERM), 0);
    ASSERT_TRUE(dc_app_shutdown_requested());
    ASSERT_TRUE(dc_app_consume_shutdown_request());
    ASSERT_TRUE(!dc_app_shutdown_requested());
    ASSERT_TRUE(!dc_app_consume_shutdown_request());
    ASSERT_INT_EQ(raise(SIGINT), 0);
    ASSERT_TRUE(dc_app_shutdown_requested());
    ASSERT_TRUE(dc_app_consume_shutdown_request());
    ASSERT_TRUE(!dc_app_consume_shutdown_request());
    PASS();
}
static void test_reinstall_replaces_handlers_safely(void) {
    dc_app_install_shutdown_handlers();
    dc_app_install_shutdown_handlers();
    ASSERT_INT_EQ(raise(SIGTERM), 0);
    ASSERT_TRUE(dc_app_consume_shutdown_request());
    PASS();
}
static bool screen_chunk_cell(dc_level_view_t *view, uint32_t x, uint32_t y,
                              dc_chunk_coord_t *coord, uint32_t *cell) {
    dc_level_view_status_t status;
    if (!dc_level_view_status(view, &status)) return false;
    int64_t local_x = (int64_t)x + status.offset_x;
    int64_t local_y = (int64_t)y + status.offset_y;
    int64_t chunk_x = local_x >= 0 ? local_x / 64 : -((-local_x + 63) / 64);
    int64_t chunk_y = local_y >= 0 ? local_y / 64 : -((-local_y + 63) / 64);
    coord->x = status.origin.x + chunk_x;
    coord->y = status.origin.y + chunk_y;
    *cell = (uint32_t)((local_y % 64) * 64 + (local_x % 64));
    return true;
}
static void test_signal_shutdown_flushes_latest_edits(void) {
    char directory[] = "build/shutdown_edits_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_app_install_shutdown_handlers();
    dc_level_view_t *view =
        dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    dc_chunk_coord_t carved_coord, island_coord, edit_coord;
    uint32_t carved_cell, island_cell, edit_cell;
    ASSERT_TRUE(screen_chunk_cell(view, 32, 28, &carved_coord, &carved_cell));
    ASSERT_TRUE(screen_chunk_cell(view, 32, 32, &island_coord, &island_cell));
    ASSERT_TRUE(screen_chunk_cell(view, 40, 44, &edit_coord, &edit_cell));
    ASSERT_TRUE(dc_level_view_paint(view, 32, 32, 6, DC_MATERIAL_AIR, err,
                                    sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view, 32, 32, 1, DC_MATERIAL_STONE, err,
                                    sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view, 40, 44, 1, DC_MATERIAL_DIRT, err,
                                    sizeof(err)));
    ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    dc_gpu_world_body_t body;
    dc_gpu_body_motion_t motion;
    bool extracted = dc_level_view_body_state(view, 0x80000001u, &body,
                                              &motion, err, sizeof(err));
    uint32_t color = 0;
    ASSERT_TRUE(dc_level_view_pixel(view, 8, 8, &color, err, sizeof(err)));
    ASSERT_INT_EQ(raise(SIGINT), 0);
    ASSERT_TRUE(dc_app_shutdown_requested());
    ASSERT_TRUE(dc_app_consume_shutdown_request());
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_restored(view));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    ASSERT_TRUE(
        dc_level_view_chunk(view, carved_coord, chunk, err, sizeof(err)));
    bool carved = chunk->cells[carved_cell].material == DC_MATERIAL_AIR;
    ASSERT_TRUE(
        dc_level_view_chunk(view, edit_coord, chunk, err, sizeof(err)));
    bool edited = chunk->cells[edit_cell].material == DC_MATERIAL_DIRT;
    dc_gpu_world_body_t restored_body;
    dc_gpu_body_motion_t restored_motion;
    bool rigid = dc_level_view_body_state(view, 0x80000001u, &restored_body,
                                          &restored_motion, err, sizeof(err));
    uint32_t restored_color = 0;
    ASSERT_TRUE(
        dc_level_view_pixel(view, 8, 8, &restored_color, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    free(chunk);
    ASSERT_TRUE(extracted);
    ASSERT_TRUE(carved);
    ASSERT_TRUE(edited);
    ASSERT_TRUE(rigid);
    ASSERT_EQ(restored_color, color);
    PASS();
}
int main(void) {
    printf("=== Shutdown Signal Tests ===\n\n");
    RUN(test_termination_signals_request_shutdown_once);
    RUN(test_reinstall_replaces_handlers_safely);
    RUN(test_signal_shutdown_flushes_latest_edits);
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
