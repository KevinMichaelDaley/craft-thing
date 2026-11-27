#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/liquid_seam.h"
#include "../../src/vulkan/gpu_internal.h"
#include "../../src/vulkan/renderer_internal.h"

static int g_pass, g_fail;
static char g_err[256];

#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(test) do { if (!(test)) { \
    printf("FAIL %s:%d: %s [%s]\n", __FILE__, __LINE__, #test, g_err); ++g_fail; goto cleanup; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() ++g_pass

enum { FORWARD = 20, REVERSE = 21, BLOCKED = 23, WALL = 25, RECEIVING_BODY = 27 };
static const uint32_t shallow_aperture = DC_FLUID_FULL / 4u;
static const float step_seconds = 1.f / 120.f;
static const float initial_normal = 0.6f;

static uint32_t cell_index(bool y_axis, uint32_t normal, uint32_t lane, uint32_t width) {
    return y_axis ? normal * width + lane : lane * width + normal;
}

static void seed_basin(dc_chunk_t *a, dc_chunk_t *b, bool y_axis) {
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
        a->cells[i].material = b->cells[i].material = DC_MATERIAL_STONE;
    const uint32_t open_lanes[] = {FORWARD, REVERSE, BLOCKED, WALL, RECEIVING_BODY};
    for (size_t l = 0; l < sizeof(open_lanes) / sizeof(open_lanes[0]); ++l) {
        uint32_t lane = open_lanes[l];
        uint32_t i = cell_index(y_axis, DC_CHUNK_SIDE - 1u, lane, DC_CHUNK_SIDE);
        uint32_t j = cell_index(y_axis, 0, lane, DC_CHUNK_SIDE);
        a->cells[i] = (dc_cell_t){.fluid_mass = DC_FLUID_FULL / 2u};
        if (lane != WALL) b->cells[j] = a->cells[i];
        float velocity = lane == REVERSE ? -initial_normal : initial_normal;
        if (y_axis) a->face_velocity[i].y = velocity;
        else a->face_velocity[i].x = velocity;
    }
    uint32_t i = cell_index(y_axis, DC_CHUNK_SIDE - 1u, FORWARD, DC_CHUNK_SIDE);
    uint32_t j = cell_index(y_axis, 0u, FORWARD, DC_CHUNK_SIDE);
    if (y_axis) {
        a->face_velocity[i].x = -initial_normal;
        b->face_velocity[j].x = initial_normal;
    } else {
        a->face_velocity[i].y = -initial_normal;
        b->face_velocity[j].y = initial_normal;
    }
}

static bool upload(dc_gpu_t *gpu, uint32_t slot, uint32_t tile_x, uint32_t tile_y,
                   const dc_chunk_t *chunk) {
    return dc_gpu_upload_chunk(gpu, slot, chunk, g_err, sizeof(g_err)) &&
        dc_gpu_set_page(gpu, tile_x, tile_y, slot, g_err, sizeof(g_err));
}

static void seed_apertures(dc_gpu_t *gpu, bool y_axis, uint32_t base_x, uint32_t base_y) {
    uint32_t *plane = (uint32_t *)gpu->occupancy_mapped +
                      (y_axis ? 4u : 3u) * gpu->width * gpu->height;
    uint32_t offset = base_y * gpu->width + base_x;
    plane[offset + cell_index(y_axis, DC_CHUNK_SIDE - 1u, REVERSE, gpu->width)] =
        DC_FLUID_FULL - shallow_aperture;
    plane[offset + cell_index(y_axis, DC_CHUNK_SIDE - 1u, BLOCKED, gpu->width)] = DC_FLUID_FULL;
}

static uint32_t *receiver_coverage(dc_gpu_t *gpu, bool y_axis, uint32_t base_x, uint32_t base_y) {
    uint32_t *plane = (uint32_t *)gpu->occupancy_mapped + 2u * gpu->width * gpu->height;
    return plane + base_y * gpu->width + base_x + cell_index(y_axis, 0, RECEIVING_BODY, gpu->width);
}

static bool native_projection(dc_gpu_t *gpu, VkBuffer readback) {
    gpu->fluid_step_scale = step_seconds * 60.f;
    if (!fr_renderer_begin(gpu)) return false;
    fr_renderer_barrier(gpu->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    dc_gpu_record_projection_begin(gpu);
    dc_gpu_record_pressure_passes(gpu, 0, DC_PRESSURE_PASS_COUNT);
    fr_renderer_barrier(gpu->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_HOST_READ_BIT);
    const VkBufferCopy copy = {.size = 2u * DC_CHUNK_CELLS * sizeof(float)};
    vkCmdCopyBuffer(gpu->command, gpu->pressure_a_buffer, readback, 1u, &copy);
    fr_renderer_barrier(gpu->command, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    return fr_renderer_submit(gpu);
}

static void compare_projection(dc_liquid_seam_axis_t axis) {
    const bool y_axis = axis == DC_LIQUID_SEAM_Y;
    const char shader[] = "build/shaders/pattern.comp.spv";
    const uint32_t width = y_axis ? 64u : 128u, height = y_axis ? 128u : 64u;
    dc_chunk_t *a = calloc(1, sizeof(*a)), *b = calloc(1, sizeof(*b));
    dc_chunk_t *before = calloc(1, sizeof(*before)), *after = calloc(1, sizeof(*after));
    dc_liquid_seam_endpoint_t first = {.coord = {-2, 3}, .tile_x = 1u, .slot = 1u};
    dc_liquid_seam_endpoint_t second = {
        .coord = {-2 + !y_axis, 3 + y_axis}, .tile_y = 2u, .slot = 0u};
    dc_gpu_t device = {0};
    dc_gpu_t *mono = NULL;
    VkBuffer readback = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void *mapped = NULL;
    dc_liquid_seam_sample_t samples[DC_CHUNK_SIDE] = {0};
    dc_liquid_seam_sample_t saved[DC_CHUNK_SIDE];
    float previous_normal[DC_CHUNK_SIDE] = {0};
    g_err[0] = '\0';

    ASSERT_TRUE(a && b && before && after);
    a->coord = first.coord;
    b->coord = second.coord;
    seed_basin(a, b, y_axis);
    ASSERT_TRUE(dc_gpu_initialize_device(&device, 3u * DC_CHUNK_CELLS * sizeof(dc_cell_t),
                                         0, 0, 0, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_create_shared(&first.gpu, &device, 128u, 64u, shader, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_create_shared(&second.gpu, first.gpu, 64u, 192u, shader, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(first.gpu, (dc_chunk_coord_t){-3, 3}, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(second.gpu, (dc_chunk_coord_t){second.coord.x, second.coord.y - 2},
                                    g_err, sizeof(g_err)));
    ASSERT_TRUE(upload(first.gpu, first.slot, first.tile_x, first.tile_y, a));
    ASSERT_TRUE(upload(second.gpu, second.slot, second.tile_x, second.tile_y, b));
    seed_apertures(first.gpu, y_axis, DC_CHUNK_SIDE, 0);
    *receiver_coverage(second.gpu, y_axis, 0, 2u * DC_CHUNK_SIDE) = DC_FLUID_FULL;
    ASSERT_TRUE(dc_gpu_download_chunk(first.gpu, first.slot, before, g_err, sizeof(g_err)));

    ASSERT_TRUE(dc_gpu_project_liquid_seam(first, second, axis, step_seconds, NULL,
                                          samples, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_download_chunk(first.gpu, first.slot, after, g_err, sizeof(g_err)));
    ASSERT_EQ(memcmp(before->cells, after->cells, sizeof(before->cells)), 0);
    ASSERT_EQ(memcmp(before->face_velocity, after->face_velocity, sizeof(before->face_velocity)), 0);
    ASSERT_TRUE(dc_gpu_download_chunk(second.gpu, second.slot, after, g_err, sizeof(g_err)));
    ASSERT_EQ(memcmp(b->cells, after->cells, sizeof(b->cells)), 0);
    ASSERT_EQ(memcmp(b->face_velocity, after->face_velocity, sizeof(b->face_velocity)), 0);
    ASSERT_INT_EQ(*receiver_coverage(second.gpu, y_axis, 0, 2u * DC_CHUNK_SIDE), DC_FLUID_FULL);

    ASSERT_TRUE(dc_gpu_create_shared(&mono, first.gpu, width, height, shader, g_err, sizeof(g_err)));
    ASSERT_TRUE(upload(mono, 0, 0, 0, a) && upload(mono, 1, !y_axis, y_axis, b));
    seed_apertures(mono, y_axis, 0, 0);
    *receiver_coverage(mono, y_axis, !y_axis * DC_CHUNK_SIDE, y_axis * DC_CHUNK_SIDE) = DC_FLUID_FULL;
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(mono, 2u * DC_CHUNK_CELLS * sizeof(float), &readback,
                                        &memory, &mapped, g_err, sizeof(g_err)));
    ASSERT_TRUE(native_projection(mono, readback));
    const float *pressure = mapped;
    const dc_face_velocity_t *faces = mono->velocity_mapped;
    for (uint32_t lane = 0; lane < DC_CHUNK_SIDE; ++lane) {
        const uint32_t i = cell_index(y_axis, DC_CHUNK_SIDE - 1u, lane, width);
        const uint32_t j = cell_index(y_axis, DC_CHUNK_SIDE, lane, width);
        const float normal = y_axis ? faces[i].y : faces[i].x;
        ASSERT_TRUE(fabsf(samples[lane].normal_velocity - normal) < 1e-5f);
        ASSERT_TRUE(fabsf(samples[lane].first_pressure - pressure[i]) < 1e-5f);
        ASSERT_TRUE(fabsf(samples[lane].second_pressure - pressure[j]) < 1e-5f);
    }
    ASSERT_TRUE(samples[FORWARD].normal_velocity > 0.1f);
    ASSERT_TRUE(samples[REVERSE].normal_velocity < -0.1f);
    ASSERT_INT_EQ(samples[FORWARD].aperture_fp, DC_FLUID_FULL);
    ASSERT_INT_EQ(samples[REVERSE].aperture_fp, shallow_aperture);
    ASSERT_INT_EQ(samples[BLOCKED].aperture_fp, 0u);
    ASSERT_INT_EQ(samples[WALL].aperture_fp, 0u);
    ASSERT_INT_EQ(samples[RECEIVING_BODY].aperture_fp, 0u);
    ASSERT_EQ(samples[BLOCKED].normal_velocity, 0.f);
    ASSERT_EQ(samples[WALL].normal_velocity, 0.f);
    ASSERT_EQ(samples[RECEIVING_BODY].normal_velocity, 0.f);

    previous_normal[FORWARD] = -5.f;
    previous_normal[REVERSE] = 5.f;
    ASSERT_TRUE(dc_gpu_project_liquid_seam(first, second, axis, step_seconds, previous_normal,
                                          samples, g_err, sizeof(g_err)));
    ASSERT_TRUE(samples[FORWARD].normal_velocity < -0.1f);
    ASSERT_TRUE(samples[REVERSE].normal_velocity > 0.1f);
    memcpy(saved, samples, sizeof(saved));
    ASSERT_TRUE(!dc_gpu_project_liquid_seam(first, second, axis, NAN, NULL,
                                           samples, g_err, sizeof(g_err)));
    ASSERT_INT_EQ(memcmp(saved, samples, sizeof(saved)), 0);
    previous_normal[REVERSE] = INFINITY;
    ASSERT_TRUE(!dc_gpu_project_liquid_seam(first, second, axis, step_seconds, previous_normal,
                                           samples, g_err, sizeof(g_err)));
    ASSERT_INT_EQ(memcmp(saved, samples, sizeof(saved)), 0);
    second.slot = 2u;
    ASSERT_TRUE(!dc_gpu_project_liquid_seam(first, second, axis, step_seconds, NULL,
                                           samples, g_err, sizeof(g_err)));
    ASSERT_INT_EQ(memcmp(saved, samples, sizeof(saved)), 0);
    ASSERT_INT_EQ(dc_gpu_validation_errors(first.gpu), 0u);
    PASS();

cleanup:
    if (mapped) vkUnmapMemory(mono->device, memory);
    if (readback) vkDestroyBuffer(mono->device, readback, NULL);
    if (memory) vkFreeMemory(mono->device, memory, NULL);
    dc_gpu_destroy(mono);
    dc_gpu_destroy(second.gpu);
    dc_gpu_destroy(first.gpu);
    dc_gpu_release_device(&device);
    free(after);
    free(before);
    free(b);
    free(a);
}

static void test_horizontal_matches_monolithic_pressure(void) {
    compare_projection(DC_LIQUID_SEAM_X);
}

static void test_vertical_matches_monolithic_pressure(void) {
    compare_projection(DC_LIQUID_SEAM_Y);
}

int main(void) {
    RUN(test_horizontal_matches_monolithic_pressure);
    RUN(test_vertical_matches_monolithic_pressure);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
