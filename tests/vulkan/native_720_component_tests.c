#define main pixel_component_fixture_main
#include "pixel_component_tests.c"
#undef main

static void test_native_720_warm_seam_above_128_cache_slots_is_exact(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    ASSERT_TRUE(DC_GPU_CHUNK_SLOTS >= 240u);
    ASSERT_TRUE(dc_gpu_create(&gpu, 1280, 720, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    for (unsigned cy = 0; cy < 12; ++cy)
        for (unsigned cx = 0; cx < 20; ++cx) {
            *chunk = (dc_chunk_t){.coord = {cx, cy}};
            if (cy == 10 && (cx == 18 || cx == 19))
                for (unsigned y = 10; y < 30; ++y)
                    for (unsigned x = 0; x < 64; ++x)
                        if (cx == 18 ? x >= 60 : x < 16)
                            chunk->cells[y * 64 + x].material =
                                cx == 18 ? DC_MATERIAL_WOOD : DC_MATERIAL_STONE;
            unsigned slot = cy * 20 + cx;
            ASSERT_TRUE(dc_gpu_upload_chunk(gpu, slot, chunk, err, sizeof(err)));
            ASSERT_TRUE(dc_gpu_set_page(gpu, cx, cy, slot, err, sizeof(err)));
        }
    ASSERT_TRUE(dc_gpu_enable_automatic_components(gpu));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_body_t body;
    bool born = dc_gpu_read_body_id(gpu, 0x80000001u, &body, err, sizeof(err));
    if (!born)
        printf("native720 warm seam missing body: %s\n", err);
    if (!born) {
        free(chunk);
        dc_gpu_destroy(gpu);
        ASSERT_TRUE(born);
    }
    dc_gpu_body_pixel_t pixels[400];
    uint32_t count;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(gpu, body.id, pixels, 400, &count,
                                       err, sizeof(err)));
    ASSERT_EQ(count, 400u);
    bool seen[400] = {false};
    unsigned wood = 0, stone = 0;
    for (unsigned i = 0; i < count; ++i) {
        ASSERT_TRUE(pixels[i].x < 20 && pixels[i].y < 20);
        unsigned index = pixels[i].y * 20 + pixels[i].x;
        ASSERT_TRUE(!seen[index]);
        seen[index] = true;
        ASSERT_EQ(pixels[i].material, pixels[i].x < 4 ? DC_GPU_BODY_WOOD :
                                                       DC_GPU_BODY_STONE);
        wood += pixels[i].material == DC_GPU_BODY_WOOD;
        stone += pixels[i].material == DC_GPU_BODY_STONE;
    }
    ASSERT_EQ(wood, 80u);
    ASSERT_EQ(stone, 320u);
    dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu, body.id, &motion, err, sizeof(err)));
    ASSERT_TRUE(fabs(motion.mass - 912.0) < .002);
    for (unsigned slot = 218; slot <= 219; ++slot) {
        ASSERT_TRUE(dc_gpu_download_chunk(gpu, slot, chunk, err, sizeof(err)));
        for (unsigned i = 0; i < 4096; ++i)
            ASSERT_EQ(chunk->cells[i].material, DC_MATERIAL_AIR);
    }
    printf("native physics grid1280x720, cache slots%u: exact400-pixel seam\n",
           DC_GPU_CHUNK_SLOTS);
    free(chunk);
    dc_gpu_destroy(gpu);
    PASS();
}
int main(void) {
    RUN(test_native_720_warm_seam_above_128_cache_slots_is_exact);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
