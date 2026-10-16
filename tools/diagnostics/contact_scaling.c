/* Contacts-only diagnostic: no lighting, fluid, raster, or solver submissions. */
#define main pixel_fixture_main
#include "../../tests/vulkan/pixel_component_tests.c"
#undef main

static bool occupied(unsigned kind, unsigned x, unsigned y, unsigned side) {
    if (kind == 0) return true;
    if (kind == 1) return x == 0 || y == 0 || x + 1 == side || y + 1 == side;
    return x == 0 || y % 4 < 2;
}

static void measure(unsigned side, unsigned kind, unsigned pair_kind) {
    char err[256] = {0};
    dc_gpu_t *g = NULL;
    ASSERT_TRUE(side >= 8 && side <= 96 && kind <= 2 && pair_kind <= 2);
    ASSERT_TRUE(dc_gpu_create(&g, 256, 256, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    unsigned area = 0, boundary = 0, perimeter = 0;
    for (unsigned cy = 0; cy < 4; cy++)
        for (unsigned cx = 0; cx < 4; cx++) {
            *chunk = (dc_chunk_t){.coord = {cx, cy}};
            for (unsigned y = 0; y < 64; y++)
                for (unsigned x = 0; x < 64; x++) {
                    unsigned px = cx * 64 + x, py = cy * 64 + y;
                    if (px < 32 || py < 32 || px >= 32 + side || py >= 32 + side)
                        continue;
                    unsigned sx = px - 32, sy = py - 32;
                    if (!occupied(kind, sx, sy, side)) continue;
                    chunk->cells[y * 64 + x].material = DC_MATERIAL_WOOD;
                    area++;
                    boundary += sx == 0 || sy == 0 || sx + 1 == side || sy + 1 == side ||
                                !occupied(kind, sx - 1, sy, side) || !occupied(kind, sx + 1, sy, side) ||
                                !occupied(kind, sx, sy - 1, side) || !occupied(kind, sx, sy + 1, side);
                    perimeter += (sx == 0 || !occupied(kind, sx - 1, sy, side)) +
                                 (sx + 1 == side || !occupied(kind, sx + 1, sy, side)) +
                                 (sy == 0 || !occupied(kind, sx, sy - 1, side)) +
                                 (sy + 1 == side || !occupied(kind, sx, sy + 1, side));
                }
            unsigned slot = cy * 4 + cx;
            ASSERT_TRUE(dc_gpu_upload_chunk(g, slot, chunk, err, sizeof(err)));
            ASSERT_TRUE(dc_gpu_set_page(g, cx, cy, slot, err, sizeof(err)));
        }
    free(chunk);
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_gpu_body_record_t *bodies = g->body_mapped;
    ASSERT_TRUE(bodies[0].piece_count & DC_GPU_PIXEL_BODY_FLAG);
    bodies[1] = bodies[0];
    bodies[1].body.id = 0x70000001u;
    if (pair_kind < 2) {
        unsigned legacy = pair_kind == 0 ? 0 : 1;
        const uint32_t *words = g->body_mapped;
        unsigned low_x = words[bodies[legacy].compound_reserved + 4];
        unsigned low_y = words[bodies[legacy].compound_reserved + 5];
        bodies[legacy].piece_count = 0;
        bodies[legacy].shape.count = 0;
        bodies[legacy].body.width = side;
        bodies[legacy].body.height = side;
        bodies[legacy].body.x_fp += (int32_t)(low_x + side / 2) * 65536 + 16384;
        bodies[legacy].local_x_fp += (int32_t)(low_x + side / 2) * 65536 + 16384;
        bodies[legacy].body.y_fp += (int32_t)low_y * 65536;
        bodies[legacy].local_y_fp += (int32_t)low_y * 65536;
    } else {
        bodies[1].body.x_fp += (int32_t)side * 65536 - 16384;
        bodies[1].local_x_fp += (int32_t)side * 65536 - 16384;
    }
    g->body_count = 2;
    g->component_profile = true;
    uint32_t *pairs = dc_gpu_broadphase_data(g);
    unsigned pair_base = DC_GPU_BROADPHASE_HEADER_WORDS +
                         2 * dc_gpu_broadphase_buckets(g);
    pairs[0] = 1; pairs[2] = 0;
    pairs[pair_base] = bodies[0].body.id;
    pairs[pair_base + 1] = bodies[1].body.id;
    for (unsigned i = 2; i < 8; i++) pairs[pair_base + i] = 0;
    double total_ms = 0;
    for (unsigned sample = 0; sample < 9; sample++) {
        ASSERT_EQ(vkResetCommandBuffer(g->command, 0), VK_SUCCESS);
        VkCommandBufferBeginInfo begin = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        ASSERT_EQ(vkBeginCommandBuffer(g->command, &begin), VK_SUCCESS);
        vkCmdResetQueryPool(g->command, g->timestamp_pool, 0, 2);
        vkCmdWriteTimestamp2(g->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, g->timestamp_pool, 0);
        dc_gpu_record_contacts(g);
        vkCmdWriteTimestamp2(g->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, g->timestamp_pool, 1);
        ASSERT_EQ(vkEndCommandBuffer(g->command), VK_SUCCESS);
        VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                               .commandBufferCount = 1, .pCommandBuffers = &g->command};
        ASSERT_EQ(vkQueueSubmit(g->queue, 1, &submit, VK_NULL_HANDLE), VK_SUCCESS);
        ASSERT_EQ(vkQueueWaitIdle(g->queue), VK_SUCCESS);
        uint64_t times[2];
        ASSERT_EQ(vkGetQueryPoolResults(g->device, g->timestamp_pool, 0, 2, sizeof(times), times,
                                       sizeof(uint64_t), VK_QUERY_RESULT_64_BIT), VK_SUCCESS);
        if (sample) total_ms += (times[1] - times[0]) * g->timestamp_period / 1e6;
    }
    const uint32_t *contacts = dc_gpu_contact_data(g);
    printf("contact scaling side%u shape%u pair%u area%u boundary%u perimeter%u "
           "visited%u boundaryA%u boundaryB%u emitted%u overflow%u mean%.6fms\n",
           side, kind, pair_kind, area, boundary, perimeter, contacts[9], contacts[10], contacts[11],
           contacts[0], contacts[2], total_ms / 8);
    ASSERT_EQ(contacts[2], 0u);
    dc_gpu_destroy(g);
    PASS();
}

int main(int argc, char **argv) {
    if (argc != 4) return 2;
    measure((unsigned)strtoul(argv[1], NULL, 10), (unsigned)strtoul(argv[2], NULL, 10),
            (unsigned)strtoul(argv[3], NULL, 10));
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
