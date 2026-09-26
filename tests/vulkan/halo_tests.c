#include <stdint.h>
#include <stdio.h>

#include "dungeoncraft/gpu.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static dc_gpu_t *two_chunk_gpu(dc_chunk_t *left, dc_chunk_t *right,
                                char *err, uint32_t cap) {
    dc_gpu_t *gpu = NULL;
    if (!dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv", err, cap) ||
        !dc_gpu_upload_chunk(gpu, 0, left, err, cap) ||
        !dc_gpu_upload_chunk(gpu, 1, right, err, cap) ||
        !dc_gpu_set_page(gpu, 0, 0, 0, err, cap) ||
        !dc_gpu_set_page(gpu, 1, 0, 1, err, cap)) {
        dc_gpu_destroy(gpu);
        return NULL;
    }
    return gpu;
}

static void test_halo_reads_neighbor_and_marks_missing_chunk(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0};
    right.cells[10 * 64].material = DC_MATERIAL_SAND;
    right.cells[10 * 64].fluid_mass = 42;
    dc_gpu_t *gpu = two_chunk_gpu(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_refresh_halos(gpu, err, sizeof(err)));
    dc_gpu_halo_cell_t cell = {0};
    ASSERT_TRUE(dc_gpu_read_halo(gpu, 0, 0, 64, 10, &cell, err, sizeof(err)));
    ASSERT_EQ(cell.resident, 1u);
    ASSERT_EQ(cell.cell.material, DC_MATERIAL_SAND);
    ASSERT_EQ(cell.cell.fluid_mass, 42u);
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, UINT32_MAX, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_refresh_halos(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_halo(gpu, 0, 0, 64, 10, &cell, err, sizeof(err)));
    ASSERT_EQ(cell.resident, 0u);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_scalar_transfer_waits_for_chunk_and_matches_unsplit_grid(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved_left = {0}, saved_right = {0};
    left.cells[10 * 64 + 63].fluid_mass = 100;
    right.cells[10 * 64].fluid_mass = 25;
    dc_gpu_t *gpu = two_chunk_gpu(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, UINT32_MAX, err, sizeof(err)));
    dc_gpu_transfer_t transfer = { .from_x = 63, .from_y = 10, .to_x = 64,
        .to_y = 10, .amount = 30, .kind = DC_GPU_TRANSFER_SCALAR };
    ASSERT_TRUE(dc_gpu_queue_transfer(gpu, transfer, err, sizeof(err)));
    dc_gpu_transfer_state_t state = DC_GPU_TRANSFER_BLOCKED;
    ASSERT_TRUE(dc_gpu_try_transfer(gpu, &state, err, sizeof(err)));
    ASSERT_EQ(state, DC_GPU_TRANSFER_PENDING);
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved_left, err, sizeof(err)));
    ASSERT_EQ(saved_left.cells[10 * 64 + 63].fluid_mass, 100u);
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, 1, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_try_transfer(gpu, &state, err, sizeof(err)));
    ASSERT_EQ(state, DC_GPU_TRANSFER_APPLIED);
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved_left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &saved_right, err, sizeof(err)));
    ASSERT_EQ(saved_left.cells[10 * 64 + 63].fluid_mass, 70u);
    ASSERT_EQ(saved_right.cells[10 * 64].fluid_mass, 55u);
    ASSERT_EQ(saved_left.cells[10 * 64 + 63].fluid_mass +
              saved_right.cells[10 * 64].fluid_mass, 125u);

    dc_gpu_t *single = NULL;
    dc_chunk_t unsplit = {0}, result = {0};
    unsplit.cells[10 * 64 + 20].fluid_mass = 100;
    unsplit.cells[10 * 64 + 21].fluid_mass = 25;
    ASSERT_TRUE(dc_gpu_create(&single, 64, 64, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(single, 0, &unsplit, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(single, 0, 0, 0, err, sizeof(err)));
    transfer.from_x = 20; transfer.to_x = 21;
    ASSERT_TRUE(dc_gpu_queue_transfer(single, transfer, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_try_transfer(single, &state, err, sizeof(err)));
    ASSERT_EQ(state, DC_GPU_TRANSFER_APPLIED);
    ASSERT_TRUE(dc_gpu_download_chunk(single, 0, &result, err, sizeof(err)));
    ASSERT_EQ(result.cells[10 * 64 + 20].fluid_mass,
              saved_left.cells[10 * 64 + 63].fluid_mass);
    ASSERT_EQ(result.cells[10 * 64 + 21].fluid_mass,
              saved_right.cells[10 * 64].fluid_mass);
    dc_gpu_destroy(single);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_particle_transfer_crosses_chunk_edge_without_duplication(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, result_left = {0}, result_right = {0};
    left.cells[12 * 64 + 63].material = DC_MATERIAL_SAND;
    dc_gpu_t *gpu = two_chunk_gpu(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    dc_gpu_transfer_t transfer = { .from_x = 63, .from_y = 12, .to_x = 64,
        .to_y = 12, .kind = DC_GPU_TRANSFER_PARTICLE };
    ASSERT_TRUE(dc_gpu_queue_transfer(gpu, transfer, err, sizeof(err)));
    dc_gpu_transfer_state_t state = DC_GPU_TRANSFER_PENDING;
    ASSERT_TRUE(dc_gpu_try_transfer(gpu, &state, err, sizeof(err)));
    ASSERT_EQ(state, DC_GPU_TRANSFER_APPLIED);
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &result_left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &result_right, err, sizeof(err)));
    ASSERT_EQ(result_left.cells[12 * 64 + 63].material, DC_MATERIAL_AIR);
    ASSERT_EQ(result_right.cells[12 * 64].material, DC_MATERIAL_SAND);
    dc_gpu_destroy(gpu);
    PASS();
}

int main(void) {
    RUN(test_halo_reads_neighbor_and_marks_missing_chunk);
    RUN(test_scalar_transfer_waits_for_chunk_and_matches_unsplit_grid);
    RUN(test_particle_transfer_crosses_chunk_edge_without_duplication);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
