#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "dungeoncraft/stream.h"

static int g_pass = 0;
static int g_fail = 0;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static bool wait_result(dc_streamer_t *stream, dc_stream_result_t *result) {
    struct timespec pause_time = {0, 1000000};
    for (int i = 0; i < 3000; ++i) {
        if (dc_stream_poll(stream, result)) return true;
        nanosleep(&pause_time, NULL);
    }
    return false;
}

static void test_worker_generates_saves_and_reloads_chunk(void) {
    char directory[] = "build/stream_test_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_streamer_t *stream = dc_stream_create(directory, 1234, 2);
    ASSERT_TRUE(stream != NULL);
    dc_chunk_coord_t coord = {-1, 0};
    dc_stream_result_t result = {0};
    ASSERT_TRUE(dc_stream_request_load(stream, coord, 7));
    ASSERT_TRUE(wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_LOADED);
    ASSERT_EQ(result.generation, 7u);
    ASSERT_TRUE(result.chunk != NULL);
    ASSERT_EQ(result.chunk->coord.x, -1);
    result.chunk->cells[10 * DC_CHUNK_SIDE + 63].material = 42;
    ASSERT_TRUE(dc_stream_request_save(stream, result.chunk, 7));
    dc_stream_result_release(&result);
    ASSERT_TRUE(wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_SAVED);
    dc_stream_result_release(&result);
    ASSERT_TRUE(dc_stream_request_load(stream, coord, 9));
    ASSERT_TRUE(wait_result(stream, &result));
    ASSERT_EQ(result.kind, DC_STREAM_LOADED);
    ASSERT_EQ(result.generation, 9u);
    ASSERT_EQ(result.chunk->cells[10 * DC_CHUNK_SIDE + 63].material, 42);
    dc_stream_result_release(&result);
    dc_stream_destroy(stream);
    PASS();
}

int main(void) {
    RUN(test_worker_generates_saves_and_reloads_chunk);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
