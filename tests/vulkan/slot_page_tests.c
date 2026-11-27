#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/vulkan/gpu_internal.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { printf("FAIL %d: %s\n", __LINE__, #expr); ++g_fail; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() (++g_pass)

/* Full backing storage lets the test observe the logical copy boundary. */
static uint32_t storage[DC_GPU_CHUNK_SLOTS + 1u];
static VkDeviceSize requested;

bool __wrap_dc_gpu_make_mapped_buffer(dc_gpu_t *g, VkDeviceSize bytes, VkBuffer *buffer,
    VkDeviceMemory *memory, void **mapped, char *err, uint32_t cap) {
    (void)buffer; (void)memory; (void)err; (void)cap;
    if (mapped == &g->slot_page_mapped) {
        requested = bytes;
        *mapped = storage;
    }
    return true;
}

static void check_initial_table(uint32_t workspace_slots) {
    dc_gpu_t gpu = {.workspace_slots = workspace_slots};
    uint32_t slots = dc_gpu_slot_capacity(&gpu);
    memset(storage, 0, sizeof(storage));
    memset(gpu.slot_page, 0xff, sizeof(gpu.slot_page));
    requested = 0;
    ASSERT_TRUE(dc_gpu_marker_buffers_init(&gpu, NULL, 0));
    ASSERT_EQ(requested, slots * sizeof(*storage));
    for (uint32_t i = 0; i < slots; ++i) ASSERT_EQ(storage[i], UINT32_MAX);
    for (uint32_t i = slots; i <= DC_GPU_CHUNK_SLOTS; ++i) ASSERT_EQ(storage[i], 0u);
    PASS();
}

static void test_compact_table_initialization(void) { check_initial_table(2); }
static void test_resident_table_initialization(void) { check_initial_table(0); }

static void test_replacing_and_unmapping_compact_slots(void) {
    uint32_t pages[] = {1, 2, 3};
    dc_gpu_t gpu = {.workspace_slots = 3, .page_width = 3, .page_height = 1,
                    .page_mapped = pages, .slot_page_mapped = storage};
    memset(storage, 0, sizeof(storage));
    memset(gpu.slot_page, 0xff, sizeof(gpu.slot_page));
    for (uint32_t i = 0; i < gpu.workspace_slots; ++i) {
        gpu.slot_page[i] = i;
        storage[i] = i;
    }
    gpu.preserve_shifted_slot[1] = true;
    gpu.body_mapped = calloc(1, dc_gpu_legacy_body_storage_bytes(&gpu));
    ASSERT_TRUE(gpu.body_mapped);
    bool replaced = dc_gpu_set_page(&gpu, 0, 0, 1, NULL, 0);
    bool replacement_visible = pages[0] == 2 && pages[1] == 0 &&
                               storage[0] == UINT32_MAX && storage[1] == 0;
    bool unmapped = dc_gpu_set_page(&gpu, 0, 0, UINT32_MAX, NULL, 0);
    free(gpu.body_mapped);
    ASSERT_TRUE(replaced && replacement_visible && unmapped);
    ASSERT_INT_EQ(pages[0], 0);
    ASSERT_EQ(storage[1], UINT32_MAX);
    ASSERT_EQ(storage[2], 2u);
    ASSERT_EQ(pages[2], 3u);
    for (uint32_t i = gpu.workspace_slots; i <= DC_GPU_CHUNK_SLOTS; ++i)
        ASSERT_EQ(storage[i], 0u);
    PASS();
}

int main(void) {
    RUN(test_compact_table_initialization);
    RUN(test_resident_table_initialization);
    RUN(test_replacing_and_unmapping_compact_slots);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
