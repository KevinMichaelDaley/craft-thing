#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/gpu.h"
#include "../../src/vulkan/gpu_internal.h"

static int g_pass = 0;
static int g_fail = 0;

#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_gpu_pattern_readback(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    uint32_t cells[16] = {0};
    ASSERT_TRUE(dc_gpu_create(&gpu, 4, 4, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_pattern(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(gpu, cells, 16, err, sizeof(err)));
    ASSERT_EQ(cells[0], 0xff202020u);
    ASSERT_EQ(cells[1], 0xff4040c0u);
    ASSERT_EQ(cells[4], 0xff4040c0u);
    ASSERT_EQ(cells[15], 0xff202020u);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_rejects_invalid_dimensions(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(!dc_gpu_create(&gpu, 0, 4, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(gpu == NULL);
    ASSERT_TRUE(strlen(err) > 0);
    PASS();
}

static void test_host_visible_vram_is_preferred_for_mapped_buffers(void) {
    VkPhysicalDeviceMemoryProperties props = {0};
    props.memoryTypeCount = 3;
    props.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    props.memoryTypes[1].propertyFlags = props.memoryTypes[0].propertyFlags |
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    props.memoryTypes[2].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ASSERT_EQ(dc_gpu_host_memory_type(&props, 7u), 1u);
    ASSERT_EQ(dc_gpu_host_memory_type(&props, 5u), 0u);
    ASSERT_EQ(dc_gpu_host_memory_type(&props, 4u), UINT32_MAX);
    PASS();
}

static void test_chunk_and_particle_state_use_separate_stream_staging(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(gpu->chunk_staging_buffer != VK_NULL_HANDLE);
    ASSERT_TRUE(gpu->particle_staging_buffer != VK_NULL_HANDLE);
    ASSERT_TRUE(gpu->chunk_buffer != gpu->chunk_staging_buffer);
    ASSERT_TRUE(gpu->particle_buffer != gpu->particle_staging_buffer);
    ASSERT_TRUE(gpu->mpm_force_staging_buffer != VK_NULL_HANDLE);
    ASSERT_TRUE(gpu->mpm_velocity_staging_buffer != VK_NULL_HANDLE);
    ASSERT_TRUE(gpu->mpm_force_buffer != gpu->mpm_force_staging_buffer);
    ASSERT_TRUE(gpu->mpm_velocity_buffer != gpu->mpm_velocity_staging_buffer);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_gpu_brush_updates_only_covered_cells(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    uint32_t cells[16] = {0};
    ASSERT_TRUE(dc_gpu_create(&gpu, 4, 4, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_pattern(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint(gpu, 1, 1, 0, 0xff00ff00u, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(gpu, cells, 16, err, sizeof(err)));
    ASSERT_EQ(cells[5], 0xff00ff00u);
    ASSERT_EQ(cells[4], 0xff4040c0u);
    ASSERT_EQ(cells[6], 0xff4040c0u);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_chunk_page_mapping_and_gpu_material_edit(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t left = {0}, right = {0}, saved = {0};
    uint32_t pixels[128 * 64] = {0};
    left.coord = (dc_chunk_coord_t){-1, 0};
    right.coord = (dc_chunk_coord_t){0, 0};
    left.cells[5 * DC_CHUNK_SIDE + 63].material = DC_MATERIAL_STONE;
    right.cells[5 * DC_CHUNK_SIDE].fluid_mass = DC_FLUID_FULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, &left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 1, &right, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, 1, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_render_chunks(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(gpu, pixels, 128 * 64, err, sizeof(err)));
    ASSERT_EQ(pixels[5 * 128 + 63], 0xff707070u);
    ASSERT_EQ(pixels[5 * 128 + 64], 0xffd07030u);
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 64, 5, 0, DC_MATERIAL_SAND, err, sizeof(err)));
    saved.coord = right.coord;
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &saved, err, sizeof(err)));
    ASSERT_EQ(saved.cells[5 * DC_CHUNK_SIDE].material, DC_MATERIAL_SAND);
    ASSERT_EQ(saved.cells[5 * DC_CHUNK_SIDE].fluid_mass, DC_FLUID_FULL);
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved, err, sizeof(err)));
    ASSERT_EQ(saved.cells[5 * DC_CHUNK_SIDE + 63].material, DC_MATERIAL_STONE);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_gpu_box_crosses_chunk_edge_and_rests_on_terrain(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t left = {0}, right = {0};
    uint32_t pixels[128 * 64] = {0};
    for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x) {
        left.cells[20 * DC_CHUNK_SIDE + x].material = DC_MATERIAL_STONE;
        right.cells[20 * DC_CHUNK_SIDE + x].material = DC_MATERIAL_STONE;
    }
    ASSERT_TRUE(dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, &left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 1, &right, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, 1, err, sizeof(err)));
    dc_gpu_body_t body = { .x_fp = 63 << 16, .y_fp = 1 << 16,
        .vx_fp = 1 << 16, .width = 2, .height = 2, .id = 1, .active = 1 };
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, body, err, sizeof(err)));
    for (int i = 0; i < 20; ++i)
        ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body(gpu, &body, err, sizeof(err)));
    ASSERT_EQ(body.x_fp, 83 << 16);
    ASSERT_EQ(body.y_fp, 18 << 16);
    ASSERT_EQ(body.vy_fp, 0);
    ASSERT_TRUE(dc_gpu_render_chunks(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(gpu, pixels, 128 * 64, err, sizeof(err)));
    ASSERT_EQ(pixels[18 * 128 + 83], 0xff30c040u);
    ASSERT_EQ(pixels[20 * 128 + 83], 0xff707070u);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_partial_page_velocity_copies_stay_inside_viewport(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t chunk = {0}, saved = {0};
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
        chunk.face_velocity[i] = (dc_face_velocity_t){7.0f, 9.0f};
    ASSERT_TRUE(dc_gpu_create(&gpu, 65, 65, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, &chunk, err, sizeof(err)));
    const uint32_t visible = 65u * 65u;
    const uint32_t padded = 129u * 65u + 64u;
    dc_face_velocity_t *mirror = malloc(padded * sizeof(*mirror));
    ASSERT_TRUE(mirror != NULL);
    for (uint32_t i = 0; i < padded; ++i)
        mirror[i] = (dc_face_velocity_t){-123.0f, -456.0f};
    void *mapped = gpu->velocity_mapped;
    gpu->velocity_mapped = mirror;
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 1, 0, err, sizeof(err)));
    ASSERT_EQ(mirror[visible - 1u].x, 7.0f);
    ASSERT_EQ(mirror[visible - 1u].y, 9.0f);
    for (uint32_t i = 0; i + 1u < visible; ++i)
        ASSERT_EQ(mirror[i].x, -123.0f);
    for (uint32_t i = visible; i < padded; ++i) {
        ASSERT_EQ(mirror[i].x, -123.0f);
        ASSERT_EQ(mirror[i].y, -456.0f);
    }
    mirror[visible - 1u] = (dc_face_velocity_t){11.0f, 13.0f};
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved, err, sizeof(err)));
    ASSERT_EQ(saved.face_velocity[0].x, 11.0f);
    ASSERT_EQ(saved.face_velocity[0].y, 13.0f);
    for (uint32_t i = 1; i < DC_CHUNK_CELLS; ++i) {
        ASSERT_EQ(saved.face_velocity[i].x, 7.0f);
        ASSERT_EQ(saved.face_velocity[i].y, 9.0f);
    }
    gpu->velocity_mapped = mapped;
    free(mirror);
    dc_gpu_destroy(gpu);
    PASS();
}

static uint32_t red_channel(uint32_t pixel) { return pixel & 255u; }
static uint32_t blue_channel(uint32_t pixel) { return (pixel >> 16) & 255u; }

static void test_density_mixes_only_within_each_cell(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t left = {0}, right = {0}, saved = {0};
    uint32_t pixels[128 * 64] = {0};
    const uint16_t kinds[] = {DC_MATERIAL_STONE, DC_MATERIAL_SAND,
                              DC_MATERIAL_DIRT, DC_MATERIAL_GRAVEL};
    for (uint32_t kind = 0; kind < 4; ++kind)
        for (uint32_t y = 8; y <= 12; ++y)
            for (uint32_t x = 8 + 10 * kind; x <= 12 + 10 * kind; ++x)
                left.cells[y * DC_CHUNK_SIDE + x].material = kinds[kind];
    left.cells[10 * DC_CHUNK_SIDE + 50].fluid_mass = DC_FLUID_FULL / 2u;
    left.cells[10 * DC_CHUNK_SIDE + 54].fluid_mass = DC_FLUID_FULL;
    for (uint32_t y = 20; y <= 24; ++y)
        for (uint32_t x = 30; x <= 34; ++x)
            left.cells[y * DC_CHUNK_SIDE + x].fluid_mass = DC_FLUID_FULL;
    for (uint32_t y = 30; y <= 34; ++y) {
        for (uint32_t x = 25; x <= 27; ++x)
            left.cells[y * DC_CHUNK_SIDE + x].material = DC_MATERIAL_STONE;
        for (uint32_t x = 61; x <= 63; ++x)
            left.cells[y * DC_CHUNK_SIDE + x].material = DC_MATERIAL_STONE;
    }
    for (uint32_t y = 24; y <= 26; ++y)
        for (uint32_t x = 6; x <= 8; ++x)
            right.cells[y * DC_CHUNK_SIDE + x].material = DC_MATERIAL_SAND;
    right.coord.x = 1;
    dc_chunk_seed_particles(&left);
    dc_chunk_seed_particles(&right);
    for (uint32_t y = 24; y <= 26; ++y)
        for (uint32_t x = 6; x <= 8; ++x) {
            uint32_t index = y * DC_CHUNK_SIDE + x;
            dc_chunk_particle_init(&right.particles[index + DC_CHUNK_CELLS],
                                   right.coord, index, DC_MATERIAL_GRAVEL);
            right.particles[index + DC_CHUNK_CELLS].id_lo += DC_CHUNK_CELLS;
            ++right.particle_count;
        }
    ASSERT_TRUE(dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, &left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 1, &right, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, 1, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_render_chunks(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(gpu, pixels, 128 * 64, err, sizeof(err)));
    for (uint32_t kind = 0; kind < 4; ++kind) {
        uint32_t center = 10u + 10u * kind;
        ASSERT_EQ(pixels[10 * 128 + center - 3u], 0xff181818u);
        ASSERT_TRUE(pixels[10 * 128 + center] != 0xff181818u);
    }
    ASSERT_EQ(pixels[10 * 128 + 10], 0xff707070u);
    ASSERT_EQ(pixels[10 * 128 + 20], 0xff40c8e0u);
    ASSERT_EQ(pixels[10 * 128 + 30], 0xff326495u);
    ASSERT_EQ(pixels[10 * 128 + 40], 0xff808090u);
    ASSERT_EQ(pixels[22 * 128 + 32], 0xffd07030u);
    ASSERT_EQ(pixels[10 * 128 + 49], 0xff181818u);
    ASSERT_TRUE(blue_channel(pixels[10 * 128 + 50]) <
                blue_channel(pixels[10 * 128 + 54]));
    ASSERT_TRUE(red_channel(pixels[25 * 128 + 71]) > 144u);
    ASSERT_TRUE(red_channel(pixels[25 * 128 + 71]) < 224u);
    ASSERT_EQ(pixels[32 * 128 + 27], pixels[32 * 128 + 63]);
    ASSERT_EQ(pixels[32 * 128 + 28], pixels[32 * 128 + 64]);
    ASSERT_EQ(pixels[32 * 128 + 64], 0xff181818u);
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved, err, sizeof(err)));
    ASSERT_EQ(saved.cells[10 * DC_CHUNK_SIDE + 50].fluid_mass,
              DC_FLUID_FULL / 2u);
    ASSERT_EQ(saved.particle_count, left.particle_count);
    ASSERT_EQ(memcmp(saved.particles, left.particles, sizeof(left.particles)), 0);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_tick_capture_orders_gpu_stages_and_handoffs(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t left = {0}, right = {0};
    ASSERT_TRUE(dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, &left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 1, &right, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, 1, err, sizeof(err)));
    dc_gpu_body_t body = { .x_fp = 63 << 16, .y_fp = 2 << 16,
        .vx_fp = 1 << 16, .width = 2, .height = 2, .id = 7, .active = 1 };
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, body, err, sizeof(err)));
    dc_gpu_tick_capture_t capture = {0};
    ASSERT_TRUE(dc_gpu_tick_capture(gpu, &capture, err, sizeof(err)));
    ASSERT_EQ(capture.stages[0].id, DC_GPU_STAGE_RIGID);
    ASSERT_EQ(capture.stages[1].id, DC_GPU_STAGE_FLUID);
    ASSERT_EQ(capture.stages[2].id, DC_GPU_STAGE_SAND);
    ASSERT_EQ(capture.stages[0].handoff, 7u);
    ASSERT_EQ(capture.stages[1].handoff, 8u);
    ASSERT_EQ(capture.stages[2].handoff, 9u);
    ASSERT_TRUE(capture.stages[0].gpu_ns + capture.stages[1].gpu_ns +
                capture.stages[2].gpu_ns > 0);
    ASSERT_TRUE(dc_gpu_read_body(gpu, &body, err, sizeof(err)));
    ASSERT_EQ(body.x_fp, 64 << 16);
    dc_gpu_destroy(gpu);
    PASS();
}

int main(void) {
    RUN(test_gpu_pattern_readback);
    RUN(test_rejects_invalid_dimensions);
    RUN(test_host_visible_vram_is_preferred_for_mapped_buffers);
    RUN(test_chunk_and_particle_state_use_separate_stream_staging);
    RUN(test_gpu_brush_updates_only_covered_cells);
    RUN(test_chunk_page_mapping_and_gpu_material_edit);
    RUN(test_gpu_box_crosses_chunk_edge_and_rests_on_terrain);
    RUN(test_partial_page_velocity_copies_stay_inside_viewport);
    RUN(test_density_mixes_only_within_each_cell);
    RUN(test_tick_capture_orders_gpu_stages_and_handoffs);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
