#include <limits.h>
#include <stdio.h>

#include "dungeoncraft/chunk.h"

static int g_pass = 0;
static int g_fail = 0;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_negative_coordinates_and_chunk_edges(void) {
    dc_chunk_coord_t coord;
    uint32_t x, y;
    dc_cell_address(-1, -65, &coord, &x, &y);
    ASSERT_EQ(coord.x, -1);
    ASSERT_EQ(coord.y, -2);
    ASSERT_EQ(x, 63u);
    ASSERT_EQ(y, 63u);
    dc_cell_address(64, 0, &coord, &x, &y);
    ASSERT_EQ(coord.x, 1);
    ASSERT_EQ(coord.y, 0);
    ASSERT_EQ(x, 0u);
    ASSERT_EQ(y, 0u);
    dc_cell_address(INT64_MIN, INT64_MAX, &coord, &x, &y);
    ASSERT_TRUE(x < DC_CHUNK_SIDE && y < DC_CHUNK_SIDE);
    PASS();
}

static void test_slot_reuse_waits_for_save_and_gpu(void) {
    dc_chunk_table_t table = {0};
    ASSERT_TRUE(dc_chunk_table_init(&table, 1));
    dc_chunk_coord_t a = { -1, 2 }, b = { 3, 4 };
    uint32_t index = UINT32_MAX;
    uint64_t token = 0;
    ASSERT_TRUE(dc_chunk_table_begin_load(&table, a, &index, &token));
    ASSERT_EQ(index, 0u);
    ASSERT_TRUE(dc_chunk_table_finish_load(&table, index, token));
    ASSERT_TRUE(dc_chunk_table_mark_dirty(&table, index));
    ASSERT_TRUE(dc_chunk_table_set_active(&table, index, false));
    table.slots[index].last_gpu_use = 10;
    ASSERT_TRUE(!dc_chunk_table_evict(&table, index, 10));
    ASSERT_TRUE(dc_chunk_table_begin_save(&table, index, &token));
    ASSERT_TRUE(!dc_chunk_table_set_active(&table, index, true));
    ASSERT_TRUE(dc_chunk_table_finish_save(&table, index, token));
    ASSERT_TRUE(!dc_chunk_table_evict(&table, index, 9));
    table.slots[index].pinned = true;
    ASSERT_TRUE(!dc_chunk_table_evict(&table, index, 10));
    table.slots[index].pinned = false;
    ASSERT_TRUE(dc_chunk_table_evict(&table, index, 10));
    ASSERT_TRUE(dc_chunk_table_begin_load(&table, b, &index, &token));
    ASSERT_TRUE(!dc_chunk_table_finish_load(&table, index, token - 1));
    ASSERT_TRUE(dc_chunk_table_finish_load(&table, index, token));
    ASSERT_TRUE(dc_chunk_table_find(&table, b, &index));
    ASSERT_TRUE(!dc_chunk_table_find(&table, a, &index));
    dc_chunk_table_destroy(&table);
    PASS();
}

int main(void) {
    RUN(test_negative_coordinates_and_chunk_edges);
    RUN(test_slot_reuse_waits_for_save_and_gpu);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
