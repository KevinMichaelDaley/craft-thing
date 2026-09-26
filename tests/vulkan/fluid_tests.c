#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "dungeoncraft/gpu.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static dc_gpu_t *make_grid(dc_chunk_t *left, dc_chunk_t *right,
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

static void test_water_falls_and_crosses_resident_chunk_edge(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved_left = {0}, saved_right = {0};
    left.cells[2 * 64 + 63].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved_left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &saved_right, err, sizeof(err)));
    uint64_t below = 0;
    for (uint32_t y = 3; y < 64; ++y)
        below += saved_left.cells[y * 64 + 63].fluid_mass;
    ASSERT_TRUE(below > 0);
    for (uint32_t step = 0; step < 100; ++step)
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved_left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &saved_right, err, sizeof(err)));
    uint64_t total = 0, right_mass = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        total += saved_left.cells[i].fluid_mass + saved_right.cells[i].fluid_mass;
        right_mass += saved_right.cells[i].fluid_mass;
    }
    ASSERT_EQ(total, (uint64_t)DC_FLUID_FULL);
    ASSERT_TRUE(right_mass > 0);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_closed_basin_conserves_mass_for_long_run(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved_left = {0}, saved_right = {0};
    for (uint32_t y = 10; y <= 20; ++y) {
        left.cells[y * 64 + 60].material = DC_MATERIAL_STONE;
        right.cells[y * 64 + 4].material = DC_MATERIAL_STONE;
    }
    for (uint32_t x = 60; x < 64; ++x) {
        left.cells[20 * 64 + x].material = DC_MATERIAL_STONE;
    }
    for (uint32_t x = 0; x <= 4; ++x) {
        right.cells[20 * 64 + x].material = DC_MATERIAL_STONE;
    }
    left.cells[11 * 64 + 63].fluid_mass = DC_FLUID_FULL;
    left.cells[11 * 64 + 62].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    for (uint32_t i = 0; i < 100; ++i)
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved_left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &saved_right, err, sizeof(err)));
    uint64_t total = 0, right_mass = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        total += saved_left.cells[i].fluid_mass + saved_right.cells[i].fluid_mass;
        right_mass += saved_right.cells[i].fluid_mass;
        if (saved_left.cells[i].material == DC_MATERIAL_STONE)
            ASSERT_EQ(saved_left.cells[i].fluid_mass, 0u);
        if (saved_right.cells[i].material == DC_MATERIAL_STONE)
            ASSERT_EQ(saved_right.cells[i].fluid_mass, 0u);
    }
    ASSERT_EQ(total, (uint64_t)2 * DC_FLUID_FULL);
    ASSERT_TRUE(right_mass > 0);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_unloaded_neighbor_keeps_mass_in_source(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved_left = {0};
    left.cells[63 * 64 + 63].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, UINT32_MAX, err, sizeof(err)));
    for (uint32_t i = 0; i < 8; ++i)
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved_left, err, sizeof(err)));
    uint64_t total = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) total += saved_left.cells[i].fluid_mass;
    ASSERT_EQ(total, (uint64_t)DC_FLUID_FULL);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_closed_liquid_velocity_is_projected(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0};
    for (uint32_t y = 10; y < 20; ++y)
        for (uint32_t x = 10; x < 20; ++x)
            left.cells[y * 64 + x].fluid_mass = DC_FLUID_FULL;
    for (uint32_t y = 9; y <= 20; ++y) {
        left.cells[y * 64 + 9].material = DC_MATERIAL_STONE;
        left.cells[y * 64 + 20].material = DC_MATERIAL_STONE;
    }
    for (uint32_t x = 9; x <= 20; ++x) {
        left.cells[9 * 64 + x].material = DC_MATERIAL_STONE;
        left.cells[20 * 64 + x].material = DC_MATERIAL_STONE;
    }
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    float divergence = -1.0f;
    ASSERT_TRUE(dc_gpu_fluid_max_divergence(gpu, &divergence, err, sizeof(err)));
    printf("projected maximum divergence: %.6f\n", divergence);
    ASSERT_TRUE(divergence >= 0.0f && divergence < 0.03f);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_chunk_seam_matches_interior_flow(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved_left = {0}, saved_right = {0};
    left.cells[5 * 64 + 31].fluid_mass = DC_FLUID_FULL;
    left.cells[5 * 64 + 63].fluid_mass = DC_FLUID_FULL;
    for (uint32_t x = 20; x < 64; ++x)
        left.cells[12 * 64 + x].material = DC_MATERIAL_STONE;
    for (uint32_t x = 0; x <= 12; ++x)
        right.cells[12 * 64 + x].material = DC_MATERIAL_STONE;
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_set_marker_correction(gpu, false));
    for (uint32_t i = 0; i < 20; ++i)
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved_left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &saved_right, err, sizeof(err)));
    uint32_t crossed = 0;
    for (uint32_t y = 5; y < 12; ++y) {
        for (int32_t offset = -8; offset <= 8; ++offset) {
            uint32_t interior_x = (uint32_t)(31 + offset);
            uint32_t seam_x = (uint32_t)(63 + offset);
            uint32_t interior = saved_left.cells[y * 64 + interior_x].fluid_mass;
            uint32_t seam = seam_x < 64 ?
                saved_left.cells[y * 64 + seam_x].fluid_mass :
                saved_right.cells[y * 64 + seam_x - 64].fluid_mass;
            ASSERT_EQ(interior, seam);
            if (seam_x >= 64) crossed += seam;
        }
    }
    ASSERT_TRUE(crossed > 0);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_sparse_markers_seed_and_survive_chunk_round_trip(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved = {0};
    for (uint32_t x = 20; x < 30; ++x)
        left.cells[10 * 64 + x].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    uint32_t count = 0;
    ASSERT_TRUE(dc_gpu_marker_count(gpu, 0, &count));
    ASSERT_TRUE(count > 0 && count <= DC_MARKERS_PER_CHUNK);
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved, err, sizeof(err)));
    ASSERT_EQ(saved.marker_count, count);
    ASSERT_TRUE(saved.markers[0].kind == DC_MARKER_INSIDE ||
                saved.markers[0].kind == DC_MARKER_OUTSIDE);
    dc_gpu_destroy(gpu);
    gpu = make_grid(&saved, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    uint32_t restored = 0;
    ASSERT_TRUE(dc_gpu_marker_count(gpu, 0, &restored));
    ASSERT_EQ(restored, count);
    dc_gpu_destroy(gpu);
    PASS();
}

static int marker_compare(const void *left, const void *right) {
    const dc_marker_t *a = left, *b = right;
    return a->id < b->id ? -1 : a->id > b->id ? 1 : 0;
}

static bool save_water_capture(dc_gpu_t *gpu, const char *path,
                               char *err, uint32_t cap) {
    uint32_t pixels[128 * 64];
    if (!dc_gpu_render_chunks(gpu, err, cap) ||
        !dc_gpu_readback(gpu, pixels, 128 * 64, err, cap)) return false;
    FILE *file = fopen(path, "wb");
    if (!file) return false;
    bool okay = fprintf(file, "P6\n128 64\n255\n") > 0;
    for (uint32_t i = 0; i < 128 * 64 && okay; ++i) {
        uint8_t rgb[3] = { (uint8_t)pixels[i],
            (uint8_t)(pixels[i] >> 8), (uint8_t)(pixels[i] >> 16) };
        okay = fwrite(rgb, sizeof(rgb), 1, file) == 1;
    }
    if (fclose(file) != 0) okay = false;
    return okay;
}

static void test_marker_ownership_crosses_chunk_and_replays(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved_a = {0}, saved_b = {0};
    left.cells[10 * 64 + 62].fluid_mass = DC_FLUID_FULL;
    left.cells[10 * 64 + 63].fluid_mass = DC_FLUID_FULL;
    for (uint32_t x = 60; x < 64; ++x)
        left.cells[11 * 64 + x].material = DC_MATERIAL_STONE;
    for (uint32_t x = 0; x < 4; ++x)
        right.cells[11 * 64 + x].material = DC_MATERIAL_STONE;
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &saved_a, err, sizeof(err)));
    ASSERT_TRUE(saved_a.marker_count > 0);
    bool outside = false;
    for (uint32_t i = 0; i < saved_a.marker_count; ++i)
        if (saved_a.markers[i].kind == DC_MARKER_OUTSIDE) outside = true;
    ASSERT_TRUE(outside);
    dc_gpu_destroy(gpu);
    gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &saved_b, err, sizeof(err)));
    ASSERT_EQ(saved_a.marker_count, saved_b.marker_count);
    qsort(saved_a.markers, saved_a.marker_count, sizeof(dc_marker_t), marker_compare);
    qsort(saved_b.markers, saved_b.marker_count, sizeof(dc_marker_t), marker_compare);
    for (uint32_t i = 0; i < saved_a.marker_count; ++i) {
        ASSERT_EQ(saved_a.markers[i].id, saved_b.markers[i].id);
        ASSERT_EQ(saved_a.markers[i].x_fp, saved_b.markers[i].x_fp);
        ASSERT_EQ(saved_a.markers[i].y_fp, saved_b.markers[i].y_fp);
    }
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_markers_sharpen_thin_sheet_without_changing_volume(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, warmed = {0}, result = {0};
    for (uint32_t x = 25; x < 45; ++x)
        left.cells[17 * 64 + x].fluid_mass = 5u * DC_FLUID_FULL / 8u;
    left.cells[17 * 64 + 24].fluid_mass = 3u * DC_FLUID_FULL / 8u;
    left.cells[17 * 64 + 45].fluid_mass = 3u * DC_FLUID_FULL / 8u;
    for (uint32_t x = 10; x < 60; ++x)
        left.cells[18 * 64 + x].material = DC_MATERIAL_STONE;
    dc_gpu_t *seed_gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(seed_gpu != NULL);
    ASSERT_TRUE(dc_gpu_set_marker_correction(seed_gpu, false));
    ASSERT_TRUE(dc_gpu_fluid_step(seed_gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(seed_gpu, 0, &warmed, err, sizeof(err)));
    ASSERT_TRUE(warmed.marker_count > 0);
    mkdir("build/screenshots", 0777);
    ASSERT_TRUE(dc_gpu_set_marker_overlay(seed_gpu, true));
    ASSERT_TRUE(save_water_capture(seed_gpu, "build/screenshots/thin_guides.ppm",
                                   err, sizeof(err)));
    dc_gpu_destroy(seed_gpu);
    uint64_t total[2] = {0}, squared[2] = {0};
    mkdir("build/screenshots", 0777);
    for (uint32_t variant = 0; variant < 2; ++variant) {
        dc_gpu_t *gpu = make_grid(&warmed, &right, err, sizeof(err));
        ASSERT_TRUE(gpu != NULL);
        ASSERT_TRUE(dc_gpu_set_marker_correction(gpu, variant == 1));
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &result, err, sizeof(err)));
        for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
            uint64_t mass = result.cells[i].fluid_mass;
            total[variant] += mass;
            squared[variant] += mass * mass;
        }
        ASSERT_TRUE(save_water_capture(gpu, variant == 0 ?
            "build/screenshots/thin_sheet_off.ppm" :
            "build/screenshots/thin_sheet_on.ppm", err, sizeof(err)));
        dc_gpu_destroy(gpu);
    }
    printf("thin-sheet concentration off=%llu on=%llu\n",
           (unsigned long long)squared[0], (unsigned long long)squared[1]);
    ASSERT_EQ(total[0], total[1]);
    ASSERT_TRUE(squared[1] > squared[0]);
    PASS();
}

static void test_markers_sharpen_splash_lobes_without_changing_volume(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, warmed = {0}, result = {0};
    for (uint32_t x = 10; x < 60; ++x)
        left.cells[18 * 64 + x].material = DC_MATERIAL_STONE;
    for (uint32_t x = 20; x < 29; ++x)
        left.cells[17 * 64 + x].fluid_mass = 5u * DC_FLUID_FULL / 8u;
    for (uint32_t x = 40; x < 49; ++x)
        left.cells[17 * 64 + x].fluid_mass = 5u * DC_FLUID_FULL / 8u;
    const uint32_t fringe_x[4] = {19, 29, 39, 49};
    for (uint32_t i = 0; i < 4; ++i)
        left.cells[17 * 64 + fringe_x[i]].fluid_mass = 3u * DC_FLUID_FULL / 8u;
    for (uint32_t x = 24; x <= 26; ++x)
        left.cells[13 * 64 + x].fluid_mass = DC_FLUID_FULL / 2u;
    for (uint32_t x = 37; x <= 39; ++x)
        left.cells[14 * 64 + x].fluid_mass = DC_FLUID_FULL / 2u;
    left.cells[15 * 64 + 33].fluid_mass = DC_FLUID_FULL / 2u;
    dc_gpu_t *seed_gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(seed_gpu != NULL);
    ASSERT_TRUE(dc_gpu_set_marker_correction(seed_gpu, false));
    ASSERT_TRUE(dc_gpu_fluid_step(seed_gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(seed_gpu, 0, &warmed, err, sizeof(err)));
    ASSERT_TRUE(warmed.marker_count > 0);
    dc_gpu_destroy(seed_gpu);
    uint64_t total[2] = {0}, concentration[2] = {0};
    for (uint32_t variant = 0; variant < 2; ++variant) {
        dc_gpu_t *gpu = make_grid(&warmed, &right, err, sizeof(err));
        ASSERT_TRUE(gpu != NULL);
        ASSERT_TRUE(dc_gpu_set_marker_correction(gpu, variant == 1));
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &result, err, sizeof(err)));
        for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
            uint64_t mass = result.cells[i].fluid_mass;
            total[variant] += mass;
            concentration[variant] += mass * mass;
        }
        ASSERT_TRUE(save_water_capture(gpu, variant == 0 ?
            "build/screenshots/splash_off.ppm" :
            "build/screenshots/splash_on.ppm", err, sizeof(err)));
        dc_gpu_destroy(gpu);
    }
    printf("splash concentration off=%llu on=%llu\n",
           (unsigned long long)concentration[0],
           (unsigned long long)concentration[1]);
    ASSERT_EQ(total[0], total[1]);
    ASSERT_TRUE(concentration[1] > concentration[0]);
    PASS();
}

static void test_marker_overlay_is_opt_in(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0};
    left.cells[10 * 64 + 30].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    uint32_t off[128 * 64], on[128 * 64];
    ASSERT_TRUE(dc_gpu_render_chunks(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(gpu, off, 128 * 64, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_marker_overlay(gpu, true));
    ASSERT_TRUE(dc_gpu_render_chunks(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(gpu, on, 128 * 64, err, sizeof(err)));
    uint32_t changed = 0;
    for (uint32_t i = 0; i < 128 * 64; ++i)
        if (off[i] != on[i]) ++changed;
    ASSERT_TRUE(changed > 0);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_marker_pool_stays_bounded_on_dense_interface(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved_left = {0}, saved_right = {0};
    for (uint32_t y = 0; y < 64; ++y)
        for (uint32_t x = 0; x < 64; ++x)
            if (((x + y) & 1u) == 0u)
                left.cells[y * 64 + x].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved_left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &saved_right, err, sizeof(err)));
    ASSERT_TRUE(saved_left.marker_count <= DC_MARKERS_PER_CHUNK);
    ASSERT_TRUE(saved_right.marker_count <= DC_MARKERS_PER_CHUNK);
    uint64_t mass = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
        mass += saved_left.cells[i].fluid_mass + saved_right.cells[i].fluid_mass;
    ASSERT_EQ(mass, (uint64_t)2048 * DC_FLUID_FULL);
    dc_gpu_destroy(gpu);
    PASS();
}

int main(void) {
    RUN(test_water_falls_and_crosses_resident_chunk_edge);
    RUN(test_closed_basin_conserves_mass_for_long_run);
    RUN(test_unloaded_neighbor_keeps_mass_in_source);
    RUN(test_closed_liquid_velocity_is_projected);
    RUN(test_chunk_seam_matches_interior_flow);
    RUN(test_sparse_markers_seed_and_survive_chunk_round_trip);
    RUN(test_marker_ownership_crosses_chunk_and_replays);
    RUN(test_markers_sharpen_thin_sheet_without_changing_volume);
    RUN(test_markers_sharpen_splash_lobes_without_changing_volume);
    RUN(test_marker_overlay_is_opt_in);
    RUN(test_marker_pool_stays_bounded_on_dense_interface);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
