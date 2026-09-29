#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "dungeoncraft/gpu.h"
#include "../../src/vulkan/gpu_internal.h"

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
        ASSERT_TRUE(saved_left.cells[i].fluid_mass <= DC_FLUID_FULL);
        ASSERT_TRUE(saved_right.cells[i].fluid_mass <= DC_FLUID_FULL);
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
    ASSERT_TRUE(divergence >= 0.0f && divergence < 0.005f);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_chunk_seam_matches_interior_flow(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, interior_left = {0},
               interior_right = {0}, seam_left = {0}, seam_right = {0};
    for (uint32_t x = 0; x < 64; ++x) {
        left.cells[12 * 64 + x].material = DC_MATERIAL_STONE;
        right.cells[12 * 64 + x].material = DC_MATERIAL_STONE;
    }
    left.cells[5 * 64 + 31].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_set_marker_correction(gpu, false));
    for (uint32_t i = 0; i < 20; ++i)
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &interior_left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &interior_right, err, sizeof(err)));
    dc_gpu_destroy(gpu);
    left.cells[5 * 64 + 31].fluid_mass = 0;
    left.cells[5 * 64 + 63].fluid_mass = DC_FLUID_FULL;
    gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_set_marker_correction(gpu, false));
    for (uint32_t i = 0; i < 20; ++i)
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &seam_left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &seam_right, err, sizeof(err)));
    uint32_t crossed = 0, max_delta = 0;
    for (uint32_t y = 5; y < 12; ++y) {
        for (int32_t offset = -8; offset <= 8; ++offset) {
            uint32_t interior_x = (uint32_t)(31 + offset);
            uint32_t seam_x = (uint32_t)(63 + offset);
            uint32_t interior = interior_left.cells[y * 64 + interior_x].fluid_mass;
            uint32_t seam = seam_x < 64 ?
                seam_left.cells[y * 64 + seam_x].fluid_mass :
                seam_right.cells[y * 64 + seam_x - 64].fluid_mass;
            uint32_t delta = interior > seam ? interior - seam : seam - interior;
            if (delta > max_delta) max_delta = delta;
            if (seam_x >= 64) crossed += seam;
        }
    }
    printf("chunk-seam maximum fixed-point delta=%u\n", max_delta);
    ASSERT_TRUE(max_delta <= 128);
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

static bool save_tall_water_capture(dc_gpu_t *gpu, const char *path,
                                    char *err, uint32_t cap) {
    uint32_t pixels[64u * 128u];
    if (!dc_gpu_render_chunks(gpu, err, cap) ||
        !dc_gpu_readback(gpu, pixels, 64u * 128u, err, cap)) return false;
    FILE *file = fopen(path, "wb");
    if (!file) return false;
    bool okay = fprintf(file, "P6\n64 128\n255\n") > 0;
    for (uint32_t i = 0; i < 64u * 128u && okay; ++i) {
        uint8_t rgb[3] = { (uint8_t)pixels[i],
            (uint8_t)(pixels[i] >> 8), (uint8_t)(pixels[i] >> 16) };
        okay = fwrite(rgb, sizeof(rgb), 1, file) == 1;
    }
    if (fclose(file) != 0) okay = false;
    return okay;
}

static void test_high_painted_water_falls_as_continuous_column(void) {
    char err[256] = {0};
    dc_chunk_t *top = calloc(1, sizeof(*top));
    dc_chunk_t *bottom = calloc(1, sizeof(*bottom));
    dc_chunk_t *saved_top = calloc(1, sizeof(*saved_top));
    dc_chunk_t *saved_bottom = calloc(1, sizeof(*saved_bottom));
    ASSERT_TRUE(top && bottom && saved_top && saved_bottom);
    bottom->coord.y = 1;
    for (uint32_t x = 0; x < 64u; ++x)
        bottom->cells[46u * 64u + x].material = DC_MATERIAL_STONE;
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 128,
                              "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, top, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 1, bottom, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 1, 1, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_fluid_interval(gpu, 3u));
    ASSERT_TRUE(dc_gpu_set_tick_seconds(gpu, 1.0f / 60.0f));
    mkdir("build/screenshots", 0777);
    ASSERT_TRUE(save_tall_water_capture(gpu,
        "build/screenshots/high_painted_water_before.ppm", err, sizeof(err)));
    for (uint32_t frame = 0; frame < 45u; ++frame) {
        ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_paint_material(gpu, 32u, 8u, 6u,
                                          DC_MATERIAL_WATER, err, sizeof(err)));
    }
    ASSERT_TRUE(save_tall_water_capture(gpu,
        "build/screenshots/high_painted_water_after_45_frames.ppm",
        err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, saved_top, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, saved_bottom, err, sizeof(err)));
    int first_wet = -1, last_wet = -1, gap = 0, max_gap = 0;
    int thin_rows = 0;
    for (uint32_t y = 16u; y < 105u; ++y) {
        uint64_t row_mass = 0u;
        const dc_chunk_t *chunk = y < 64u ? saved_top : saved_bottom;
        for (uint32_t x = 27u; x <= 37u; ++x)
            row_mass += chunk->cells[(y % 64u) * 64u + x].fluid_mass;
        if (y >= 20u && y < 55u && row_mass < 2u * DC_FLUID_FULL)
            ++thin_rows;
        if (row_mass >= DC_FLUID_FULL / 4u) {
            if (first_wet < 0) first_wet = (int)y;
            last_wet = (int)y;
            if (gap > max_gap) max_gap = gap;
            gap = 0;
        } else if (first_wet >= 0) ++gap;
    }
    printf("high painted water stream rows %d..%d, maximum dry band %d, thin rows %d\n",
           first_wet, last_wet, max_gap, thin_rows);
    ASSERT_TRUE(first_wet >= 0 && last_wet >= first_wet + 20);
    ASSERT_TRUE(max_gap <= 1);
    ASSERT_TRUE(thin_rows <= 3);
    bool wet[64u * 64u] = {0};
    bool visited[64u * 64u] = {0};
    uint32_t queue[64u * 64u];
    uint32_t wet_cells = 0u, largest_component = 0u;
    for (uint32_t y = 16u; y < 56u; ++y)
        for (uint32_t x = 24u; x <= 40u; ++x) {
            uint32_t index = y * 64u + x;
            wet[index] = saved_top->cells[index].fluid_mass >= DC_FLUID_FULL / 4u;
            wet_cells += wet[index];
        }
    for (uint32_t y = 16u; y < 56u; ++y)
        for (uint32_t x = 24u; x <= 40u; ++x) {
            uint32_t start = y * 64u + x;
            if (!wet[start] || visited[start]) continue;
            uint32_t head = 0u, tail = 0u;
            queue[tail++] = start;
            visited[start] = true;
            while (head < tail) {
                uint32_t index = queue[head++];
                uint32_t neighbors[4] = {index - 1u, index + 1u,
                                         index - 64u, index + 64u};
                for (uint32_t i = 0u; i < 4u; ++i) {
                    uint32_t next = neighbors[i];
                    uint32_t nx = next % 64u, ny = next / 64u;
                    if (nx < 24u || nx > 40u || ny < 16u || ny >= 56u ||
                        !wet[next] || visited[next]) continue;
                    visited[next] = true;
                    queue[tail++] = next;
                }
            }
            if (tail > largest_component) largest_component = tail;
        }
    printf("falling stream connected %u of %u wet cells\n",
           largest_component, wet_cells);
    ASSERT_TRUE(wet_cells > 100u && largest_component * 4u >= wet_cells * 3u);
    dc_gpu_destroy(gpu);
    free(top); free(bottom); free(saved_top); free(saved_bottom);
    PASS();
}

static void test_32_cell_high_river_settles_after_surface_displacement(void) {
    char err[256] = {0};
    dc_chunk_t *left = calloc(1, sizeof(*left));
    dc_chunk_t *right = calloc(1, sizeof(*right));
    ASSERT_TRUE(left && right);
    for (uint32_t x = 4; x <= 123; ++x) {
        dc_chunk_t *chunk = x < 64 ? left : right;
        uint32_t local = x % 64u;
        chunk->cells[60u * 64u + local].material = DC_MATERIAL_STONE;
    }
    for (uint32_t y = 20; y < 60; ++y) {
        left->cells[y * 64u + 4u].material = DC_MATERIAL_STONE;
        right->cells[y * 64u + 59u].material = DC_MATERIAL_STONE;
    }
    for (uint32_t y = 24; y < 60; ++y)
        for (uint32_t x = 5; x <= 122; ++x) {
            if (y < 28u && x >= 64u) continue;
            dc_chunk_t *chunk = x < 64 ? left : right;
            chunk->cells[y * 64u + x % 64u].fluid_mass = DC_FLUID_FULL;
        }
    dc_gpu_t *gpu = make_grid(left, right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    mkdir("build/screenshots", 0777);
    ASSERT_TRUE(save_water_capture(gpu, "build/screenshots/river_32_before.ppm",
                                   err, sizeof(err)));
    double mean_vertical[2] = {0.0, 0.0};
    uint64_t high_mass[2] = {0u, 0u};
    for (uint32_t tick = 0; tick < 240u; ++tick) {
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
        if (tick != 59u && tick != 239u) continue;
        uint32_t sample = tick == 59u ? 0u : 1u;
        ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, left, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, right, err, sizeof(err)));
        uint64_t mass = 0u, wet = 0u;
        for (uint32_t y = 0; y < 60u; ++y)
            for (uint32_t x = 5; x <= 122u; ++x) {
                const dc_chunk_t *chunk = x < 64u ? left : right;
                uint32_t local = y * 64u + x % 64u;
                uint32_t fill = chunk->cells[local].fluid_mass;
                mass += fill;
                if (y < 22u) high_mass[sample] += fill;
                if (fill >= DC_FLUID_FULL / 2u) {
                    mean_vertical[sample] += fabsf(chunk->face_velocity[local].y);
                    ++wet;
                }
            }
        ASSERT_EQ(mass, (uint64_t)4012u * DC_FLUID_FULL);
        ASSERT_TRUE(wet > 3000u);
        mean_vertical[sample] /= (double)wet;
        ASSERT_TRUE(save_water_capture(gpu,
            sample == 0u ? "build/screenshots/river_32_after_1s.ppm" :
                           "build/screenshots/river_32_after_4s.ppm",
            err, sizeof(err)));
    }
    printf("32-cell river mean |vy|: 1s %.4f, 4s %.4f; high mass %.2f/%.2f cells\n",
           mean_vertical[0], mean_vertical[1],
           (double)high_mass[0] / DC_FLUID_FULL,
           (double)high_mass[1] / DC_FLUID_FULL);
    ASSERT_TRUE(mean_vertical[0] > 0.02);
    ASSERT_TRUE(mean_vertical[1] < 0.025 &&
                mean_vertical[1] < mean_vertical[0] * 0.75);
    ASSERT_TRUE(high_mass[1] < 4u * (uint64_t)DC_FLUID_FULL);
    dc_gpu_destroy(gpu);
    free(left); free(right);
    PASS();
}

static void test_marker_ownership_crosses_chunk_and_replays(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved_a = {0}, saved_b = {0};
    left.cells[10 * 64 + 62].fluid_mass = DC_FLUID_FULL / 4u;
    left.cells[10 * 64 + 63].fluid_mass = DC_FLUID_FULL;
    left.marker_count = 1;
    left.markers[0] = (dc_marker_t){ .x_fp = (64 << 16) + 6554,
        .y_fp = (10 << 16) + 32768, .id = 4242,
        .kind = DC_MARKER_OUTSIDE };
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
        if (saved_a.markers[i].kind == DC_MARKER_OUTSIDE &&
            saved_a.markers[i].id == 4242u) outside = true;
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

static void test_marker_advects_with_gpu_face_velocity(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved = {0};
    left.cells[5 * 64 + 30].fluid_mass = DC_FLUID_FULL;
    left.marker_count = 1;
    left.markers[0] = (dc_marker_t){ .x_fp = (30 << 16) + 32768,
        .y_fp = (5 << 16) + 32768, .id = 42, .kind = DC_MARKER_INSIDE };
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved, err, sizeof(err)));
    bool found = false;
    for (uint32_t i = 0; i < saved.marker_count; ++i) {
        if (saved.markers[i].id != 42u) continue;
        found = true;
        ASSERT_TRUE(saved.markers[i].y_fp > left.markers[0].y_fp);
        break;
    }
    ASSERT_TRUE(found);
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

static void test_markers_correct_a_moving_free_surface(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, warmed = {0}, result = {0};
    for (uint32_t x = 24; x < 40; ++x)
        left.cells[8 * 64 + x].fluid_mass = 5u * DC_FLUID_FULL / 8u;
    left.cells[8 * 64 + 23].fluid_mass = 3u * DC_FLUID_FULL / 8u;
    left.cells[8 * 64 + 40].fluid_mass = 3u * DC_FLUID_FULL / 8u;
    dc_gpu_t *seed_gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(seed_gpu != NULL);
    ASSERT_TRUE(dc_gpu_set_marker_correction(seed_gpu, false));
    ASSERT_TRUE(dc_gpu_fluid_step(seed_gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(seed_gpu, 0, &warmed, err, sizeof(err)));
    ASSERT_TRUE(warmed.marker_count > 0);
    memset(warmed.face_velocity, 0, sizeof(warmed.face_velocity));
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
        dc_gpu_destroy(gpu);
    }
    printf("moving-surface concentration off=%llu on=%llu\n",
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
    dc_chunk_t left = {0}, right = {0}, saved_left = {0},
               saved_right = {0}, replay = {0};
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
    gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &replay, err, sizeof(err)));
    ASSERT_EQ(replay.marker_count, saved_left.marker_count);
    qsort(saved_left.markers, saved_left.marker_count,
          sizeof(dc_marker_t), marker_compare);
    qsort(replay.markers, replay.marker_count,
          sizeof(dc_marker_t), marker_compare);
    for (uint32_t i = 0; i < replay.marker_count; ++i)
        ASSERT_EQ(replay.markers[i].id, saved_left.markers[i].id);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_visible_spring_supplies_fast_flow_in_one_second(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved_left = {0}, saved_right = {0};
    for (uint32_t x = 0; x < 64; ++x) {
        left.cells[48 * 64 + x].material = DC_MATERIAL_STONE;
        right.cells[48 * 64 + x].material = DC_MATERIAL_STONE;
    }
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_set_tick_water_source(gpu, true, 31, 8));
    for (uint32_t tick = 0; tick < 60; ++tick)
        ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved_left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &saved_right, err, sizeof(err)));
    uint64_t mass = 0;
    uint32_t rightmost = 0;
    for (uint32_t y = 0; y < 48; ++y)
        for (uint32_t x = 0; x < 128; ++x) {
            uint32_t cell_mass = x < 64 ?
                saved_left.cells[y * 64 + x].fluid_mass :
                saved_right.cells[y * 64 + x - 64].fluid_mass;
            mass += cell_mass;
            if (cell_mass >= DC_FLUID_FULL / 4u && x > rightmost) rightmost = x;
        }
    printf("one-second spring mass=%llu cells rightmost=%u\n",
           (unsigned long long)(mass / DC_FLUID_FULL), rightmost);
    ASSERT_TRUE(mass >= (uint64_t)150 * DC_FLUID_FULL);
    ASSERT_TRUE(rightmost >= 45);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_deep_spring_pool_does_not_spray_across_surface(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved_left = {0}, saved_right = {0};
    for (uint32_t x = 0; x < 64; ++x) {
        left.cells[48u * 64u + x].material = DC_MATERIAL_STONE;
        right.cells[48u * 64u + x].material = DC_MATERIAL_STONE;
    }
    uint32_t airborne[2] = {0};
    for (uint32_t variant = 0; variant < 2; ++variant) {
        dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
        ASSERT_TRUE(gpu != NULL);
        ASSERT_TRUE(dc_gpu_set_marker_correction(gpu, variant != 0u));
        ASSERT_TRUE(dc_gpu_set_tick_water_source(gpu, true, 31u, 8u));
        for (uint32_t tick = 0; tick < 300u; ++tick)
            ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_tick_water_source(gpu, false, 0u, 0u));
        for (uint32_t tick = 0; tick < 120u; ++tick)
            ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0u, &saved_left, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1u, &saved_right, err, sizeof(err)));
        for (uint32_t y = 0; y < 32u; ++y)
            for (uint32_t x = 0; x < 128u; ++x) {
                if (x >= 26u && x <= 36u) continue;
                const dc_chunk_t *chunk = x < 64u ? &saved_left : &saved_right;
                if (chunk->cells[y * 64u + x % 64u].fluid_mass >=
                    DC_FLUID_FULL / 16u) ++airborne[variant];
            }
        dc_gpu_destroy(gpu);
    }
    printf("deep-pool airborne cells marker off=%u on=%u\n",
           airborne[0], airborne[1]);
    ASSERT_TRUE(airborne[1] <= 8u);
    PASS();
}

static void test_supported_water_spreads_sideways_quickly(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved_left = {0}, saved_right = {0};
    for (uint32_t x = 0; x < 64; ++x) {
        left.cells[48 * 64 + x].material = DC_MATERIAL_STONE;
        right.cells[48 * 64 + x].material = DC_MATERIAL_STONE;
    }
    for (uint32_t y = 40; y < 48; ++y)
        for (uint32_t x = 16; x < 32; ++x)
            left.cells[y * 64 + x].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    for (uint32_t tick = 0; tick < 10; ++tick)
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved_left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &saved_right, err, sizeof(err)));
    uint64_t mass = 0;
    uint32_t rightmost = 0;
    for (uint32_t y = 0; y < 48; ++y)
        for (uint32_t x = 0; x < 128; ++x) {
            uint32_t cell_mass = x < 64 ?
                saved_left.cells[y * 64 + x].fluid_mass :
                saved_right.cells[y * 64 + x - 64].fluid_mass;
            mass += cell_mass;
            if (cell_mass >= DC_FLUID_FULL / 4u && x > rightmost) rightmost = x;
        }
    printf("ten-tick lateral front=%u\n", rightmost);
    ASSERT_EQ(mass, (uint64_t)128 * DC_FLUID_FULL);
    ASSERT_TRUE(rightmost >= 45);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_falling_water_is_not_limited_to_one_cell_per_tick(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved = {0};
    left.cells[2 * 64 + 20].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    for (uint32_t tick = 0; tick < 8; ++tick)
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved, err, sizeof(err)));
    uint64_t mass = 0, deep_mass = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        mass += saved.cells[i].fluid_mass;
        if (i / 64u >= 11u) deep_mass += saved.cells[i].fluid_mass;
        ASSERT_TRUE(saved.cells[i].fluid_mass <= DC_FLUID_FULL);
    }
    ASSERT_EQ(mass, (uint64_t)DC_FLUID_FULL);
    ASSERT_TRUE(deep_mass > 0);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_water_crosses_vertical_chunk_seam(void) {
    char err[256] = {0};
    dc_chunk_t top = {0}, bottom = {0}, saved_top = {0}, saved_bottom = {0};
    top.cells[63 * 64 + 32].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 128, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, &top, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 1, &bottom, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 1, 1, err, sizeof(err)));
    for (uint32_t tick = 0; tick < 5; ++tick)
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved_top, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &saved_bottom, err, sizeof(err)));
    uint64_t top_mass = 0, bottom_mass = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        top_mass += saved_top.cells[i].fluid_mass;
        bottom_mass += saved_bottom.cells[i].fluid_mass;
    }
    ASSERT_EQ(top_mass + bottom_mass, (uint64_t)DC_FLUID_FULL);
    ASSERT_TRUE(bottom_mass > 0);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_erased_floor_drains_into_lower_chunk(void) {
    char err[256] = {0};
    dc_chunk_t top = {0}, bottom = {0}, before = {0}, after_top = {0},
               after_bottom = {0};
    for (uint32_t x = 0; x < 64; ++x)
        top.cells[63 * 64 + x].material = DC_MATERIAL_STONE;
    for (uint32_t y = 56; y < 63; ++y)
        for (uint32_t x = 24; x < 40; ++x)
            top.cells[y * 64 + x].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 128, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, &top, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 1, &bottom, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 1, 1, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 32, 63, 2, DC_MATERIAL_AIR,
                                      err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &before, err, sizeof(err)));
    uint64_t initial = 0, remaining = 0, drained = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
        initial += before.cells[i].fluid_mass;
    for (uint32_t tick = 0; tick < 20; ++tick)
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &after_top, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &after_bottom, err, sizeof(err)));
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        remaining += after_top.cells[i].fluid_mass;
        drained += after_bottom.cells[i].fluid_mass;
    }
    ASSERT_EQ(remaining + drained, initial);
    ASSERT_TRUE(drained > (uint64_t)8 * DC_FLUID_FULL);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_still_pool_does_not_spray_above_surface(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved = {0};
    for (uint32_t x = 15; x <= 48; ++x)
        left.cells[48 * 64 + x].material = DC_MATERIAL_STONE;
    for (uint32_t y = 36; y < 48; ++y) {
        left.cells[y * 64 + 15].material = DC_MATERIAL_STONE;
        left.cells[y * 64 + 48].material = DC_MATERIAL_STONE;
    }
    for (uint32_t y = 40; y < 48; ++y)
        for (uint32_t x = 16; x < 48; ++x)
            left.cells[y * 64 + x].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    for (uint32_t tick = 0; tick < 100; ++tick)
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved, err, sizeof(err)));
    uint64_t spray = 0, mass = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        mass += saved.cells[i].fluid_mass;
        if (i / 64u < 38u) spray += saved.cells[i].fluid_mass;
    }
    printf("still-pool high spray=%llu\n", (unsigned long long)spray);
    ASSERT_EQ(mass, (uint64_t)256 * DC_FLUID_FULL);
    ASSERT_EQ(spray, 0u);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_camera_shift_rebases_velocity_on_gpu(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 128, 128, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    float *faces = gpu->velocity_mapped;
    uint32_t old_index = 70u * 128u + 70u;
    faces[2u * old_index] = 3.25f;
    faces[2u * old_index + 1u] = -1.5f;
    ASSERT_TRUE(dc_gpu_shift_velocity(gpu, 1, 0, err, sizeof(err)));
    uint32_t shifted_index = 70u * 128u + 6u;
    ASSERT_TRUE(faces[2u * shifted_index] == 3.25f);
    ASSERT_TRUE(faces[2u * shifted_index + 1u] == -1.5f);
    ASSERT_TRUE(faces[2u * old_index] == 0.0f);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_chunk_velocity_survives_gpu_round_trip(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved = {0};
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    float *faces = gpu->velocity_mapped;
    uint32_t face = 10u * 128u + 20u;
    faces[2u * face] = 2.25f;
    faces[2u * face + 1u] = -0.75f;
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved, err, sizeof(err)));
    ASSERT_TRUE(saved.face_velocity[10u * 64u + 20u].x == 2.25f);
    ASSERT_TRUE(saved.face_velocity[10u * 64u + 20u].y == -0.75f);
    dc_gpu_destroy(gpu);
    gpu = make_grid(&saved, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    faces = gpu->velocity_mapped;
    ASSERT_TRUE(faces[2u * face] == 2.25f);
    ASSERT_TRUE(faces[2u * face + 1u] == -0.75f);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_cropped_viewport_edges_are_internal_fluid_faces(void) {
    char err[256] = {0};
    dc_chunk_t *chunks = calloc(24, sizeof(*chunks));
    ASSERT_TRUE(chunks != NULL);
    for (uint32_t x = 58; x <= 82; ++x)
        chunks[(146u / 64u) * 6u + x / 64u]
            .cells[(146u % 64u) * 64u + x % 64u].material = DC_MATERIAL_STONE;
    for (uint32_t x = 300; x <= 326; ++x)
        chunks[(146u / 64u) * 6u + x / 64u]
            .cells[(146u % 64u) * 64u + x % 64u].material = DC_MATERIAL_STONE;
    for (uint32_t y = 140; y < 146; ++y)
        for (uint32_t x = 64; x < 80; ++x)
            chunks[(y / 64u) * 6u + x / 64u]
                .cells[(y % 64u) * 64u + x % 64u].fluid_mass = DC_FLUID_FULL;
    for (uint32_t y = 140; y < 146; ++y)
        for (uint32_t x = 304; x < 320; ++x)
            chunks[(y / 64u) * 6u + x / 64u]
                .cells[(y % 64u) * 64u + x % 64u].fluid_mass = DC_FLUID_FULL;
    chunks[(63u / 64u) * 6u + 128u / 64u]
        .cells[(63u % 64u) * 64u + 128u % 64u].fluid_mass = DC_FLUID_FULL;
    chunks[(191u / 64u) * 6u + 150u / 64u]
        .cells[(191u % 64u) * 64u + 150u % 64u].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 384, 256, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_viewport(gpu, 64, 64, 256, 128));
    for (uint32_t slot = 0; slot < 24; ++slot) {
        ASSERT_TRUE(dc_gpu_upload_chunk(gpu, slot, &chunks[slot], err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(gpu, slot % 6u, slot / 6u, slot,
                                    err, sizeof(err)));
    }
    for (uint32_t tick = 0; tick < 20; ++tick)
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    uint64_t total = 0, left_halo = 0, right_halo = 0,
             visible_top = 0, bottom_halo = 0;
    for (uint32_t slot = 0; slot < 24; ++slot) {
        ASSERT_TRUE(dc_gpu_download_chunk(gpu, slot, &chunks[slot], err, sizeof(err)));
        for (uint32_t local = 0; local < DC_CHUNK_CELLS; ++local) {
            uint32_t x = (slot % 6u) * 64u + local % 64u;
            uint32_t y = (slot / 6u) * 64u + local / 64u;
            uint32_t mass = chunks[slot].cells[local].fluid_mass;
            total += mass;
            if (x < 64u && y >= 135u && y < 146u) left_halo += mass;
            if (x >= 320u && y >= 135u && y < 146u) right_halo += mass;
            if (x >= 120u && x < 140u && y >= 64u && y < 100u)
                visible_top += mass;
            if (x >= 140u && x < 160u && y >= 192u)
                bottom_halo += mass;
        }
    }
    printf("viewport edge transfers: left=%llu right=%llu top=%llu bottom=%llu\n",
           (unsigned long long)left_halo, (unsigned long long)right_halo,
           (unsigned long long)visible_top, (unsigned long long)bottom_halo);
    ASSERT_EQ(total, (uint64_t)194 * DC_FLUID_FULL);
    ASSERT_TRUE(left_halo > 0 && right_halo > 0 &&
                visible_top > 0 && bottom_halo > 0);
    dc_gpu_destroy(gpu);
    free(chunks);
    PASS();
}

static void test_projected_water_velocity_has_tiny_final_decay(void) {
    char err[256] = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        chunk->cells[i].fluid_mass = DC_FLUID_FULL;
        chunk->face_velocity[i].x = 1.0f;
    }
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    float velocity = chunk->face_velocity[30 * DC_CHUNK_SIDE + 30].x;
    printf("projected wet-face x velocity after one step: %.6f\n", velocity);
    ASSERT_TRUE(velocity > 0.986f && velocity < 0.988f);
    dc_gpu_destroy(gpu);
    free(chunk);
    PASS();
}

static void test_no_slip_wall_damps_tangential_water_more_than_interior(void) {
    char err[256] = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    for (uint32_t y = 0; y < DC_CHUNK_SIDE - 1u; ++y)
        for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x) {
            uint32_t i = y * DC_CHUNK_SIDE + x;
            chunk->cells[i].fluid_mass = DC_FLUID_FULL;
            chunk->face_velocity[i].x = 1.0f;
        }
    for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x)
        chunk->cells[(DC_CHUNK_SIDE - 1u) * DC_CHUNK_SIDE + x].material =
            DC_MATERIAL_STONE;
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    float interior = chunk->face_velocity[30 * DC_CHUNK_SIDE + 30].x;
    float wall = chunk->face_velocity[62 * DC_CHUNK_SIDE + 30].x;
    printf("no-slip tangential velocity: interior %.4f, wall %.4f\n",
           interior, wall);
    ASSERT_TRUE(interior > 0.8f);
    ASSERT_TRUE(wall >= 0.0f && wall < interior * 0.9f);
    uint64_t mass = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
        mass += chunk->cells[i].fluid_mass;
    ASSERT_EQ(mass, (uint64_t)63u * 64u * DC_FLUID_FULL);
    for (uint32_t tick = 0; tick < 60u; ++tick)
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    float settled_wall = chunk->face_velocity[62 * DC_CHUNK_SIDE + 30].x;
    printf("no-slip wall velocity after one second: %.4f\n", settled_wall);
    ASSERT_TRUE(settled_wall > -wall * 0.1f &&
                settled_wall < wall * 0.1f);
    mass = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
        mass += chunk->cells[i].fluid_mass;
    ASSERT_EQ(mass, (uint64_t)63u * 64u * DC_FLUID_FULL);
    dc_gpu_destroy(gpu);
    free(chunk);
    PASS();
}

static void test_fluid_interval_counts_only_scheduled_updates(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    chunk->cells[8 * DC_CHUNK_SIDE + 8].fluid_mass = DC_FLUID_FULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_fluid_interval(gpu, 6));
    ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_EQ(gpu->fluid_phase, 1u);
    for (uint32_t i = 1; i < 5; ++i)
        ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_EQ(gpu->fluid_phase, 5u);
    ASSERT_EQ(gpu->fluid_tick, 0u);
    ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_EQ(gpu->fluid_tick, 1u);
    ASSERT_EQ(gpu->fluid_phase, 0u);
    for (uint32_t i = 0; i < 6; ++i)
        ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_EQ(gpu->fluid_tick, 2u);
    ASSERT_TRUE(dc_gpu_set_fluid_interval(gpu, 3u));
    ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_EQ(gpu->fluid_phase, 2u);
    ASSERT_EQ(gpu->fluid_tick, 2u);
    ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_EQ(gpu->fluid_phase, 4u);
    ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_EQ(gpu->fluid_phase, 0u);
    ASSERT_EQ(gpu->fluid_tick, 3u);
    ASSERT_TRUE(!dc_gpu_set_fluid_interval(gpu, 0));
    dc_gpu_destroy(gpu);
    free(chunk);
    PASS();
}

static void test_pressure_budget_can_change_per_gpu_context(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_EQ(gpu->pressure_sweeps, 20u);
    ASSERT_TRUE(!dc_gpu_set_pressure_sweeps(NULL, 8u));
    ASSERT_TRUE(!dc_gpu_set_pressure_sweeps(gpu, 0u));
    ASSERT_TRUE(!dc_gpu_set_pressure_sweeps(gpu, 33u));
    ASSERT_TRUE(dc_gpu_set_pressure_sweeps(gpu, 8u));
    ASSERT_EQ(gpu->pressure_sweeps, 8u);
    ASSERT_TRUE(dc_gpu_set_pressure_sweeps(gpu, 20u));
    ASSERT_EQ(gpu->pressure_sweeps, 20u);
    ASSERT_TRUE(dc_gpu_set_fluid_interval(gpu, 6u));
    ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(!dc_gpu_set_pressure_sweeps(gpu, 8u));
    ASSERT_EQ(gpu->pressure_sweeps, 20u);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_six_fluid_phases_match_one_uncoupled_update(void) {
    char err[256] = {0};
    dc_gpu_t *fast = NULL, *staged = NULL;
    dc_chunk_t *initial = calloc(1, sizeof(*initial));
    dc_chunk_t *one = calloc(1, sizeof(*one));
    dc_chunk_t *six = calloc(1, sizeof(*six));
    ASSERT_TRUE(initial && one && six);
    for (uint32_t y = 12; y < 20; ++y)
        for (uint32_t x = 18; x < 32; ++x)
            initial->cells[y * DC_CHUNK_SIDE + x].fluid_mass = DC_FLUID_FULL;
    ASSERT_TRUE(dc_gpu_create(&fast, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_create(&staged, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(fast, 0, initial, err, sizeof(err)) &&
                dc_gpu_upload_chunk(staged, 0, initial, err, sizeof(err)) &&
                dc_gpu_set_page(fast, 0, 0, 0, err, sizeof(err)) &&
                dc_gpu_set_page(staged, 0, 0, 0, err, sizeof(err)) &&
                dc_gpu_set_fluid_interval(staged, 6));
    ASSERT_TRUE(dc_gpu_tick_step(fast, err, sizeof(err)));
    for (uint32_t i = 0; i < 6u; ++i)
        ASSERT_TRUE(dc_gpu_tick_step(staged, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(fast, 0, one, err, sizeof(err)) &&
                dc_gpu_download_chunk(staged, 0, six, err, sizeof(err)));
    ASSERT_EQ(memcmp(one->cells, six->cells, sizeof(one->cells)), 0);
    ASSERT_EQ(memcmp(one->face_velocity, six->face_velocity,
                     sizeof(one->face_velocity)), 0);
    dc_gpu_destroy(fast);
    dc_gpu_destroy(staged);
    free(initial);
    free(one);
    free(six);
    PASS();
}

static void test_elapsed_time_advances_staged_water_further(void) {
    char err[256] = {0};
    dc_chunk_t initial = {0}, normal = {0}, slow_frame = {0};
    for (uint32_t x = 24; x < 40; ++x)
        initial.cells[8 * DC_CHUNK_SIDE + x].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *a = NULL, *b = NULL;
    ASSERT_TRUE(dc_gpu_create(&a, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)) &&
                dc_gpu_create(&b, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(a, 0, &initial, err, sizeof(err)) &&
                dc_gpu_upload_chunk(b, 0, &initial, err, sizeof(err)) &&
                dc_gpu_set_page(a, 0, 0, 0, err, sizeof(err)) &&
                dc_gpu_set_page(b, 0, 0, 0, err, sizeof(err)) &&
                dc_gpu_set_fluid_interval(a, 6) &&
                dc_gpu_set_fluid_interval(b, 6) &&
                dc_gpu_set_tick_seconds(a, 1.0f / 60.0f) &&
                dc_gpu_set_tick_seconds(b, 2.0f / 60.0f));
    for (uint32_t i = 0; i < 12; ++i)
        ASSERT_TRUE(dc_gpu_tick_step(a, err, sizeof(err)) &&
                    dc_gpu_tick_step(b, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(a, 0, &normal, err, sizeof(err)) &&
                dc_gpu_download_chunk(b, 0, &slow_frame, err, sizeof(err)));
    uint64_t normal_depth = 0, slow_depth = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        normal_depth += (i / DC_CHUNK_SIDE) * (uint64_t)normal.cells[i].fluid_mass;
        slow_depth += (i / DC_CHUNK_SIDE) * (uint64_t)slow_frame.cells[i].fluid_mass;
    }
    ASSERT_TRUE(slow_depth > normal_depth);
    dc_gpu_destroy(a);
    dc_gpu_destroy(b);
    PASS();
}

static void test_staged_water_renders_from_gpu_snapshots(void) {
    char err[256] = {0};
    dc_chunk_t initial = {0};
    uint32_t before[DC_CHUNK_CELLS], snapshot[DC_CHUNK_CELLS];
    uint32_t blended[DC_CHUNK_CELLS];
    for (uint32_t x = 20; x < 36; ++x)
        initial.cells[8 * DC_CHUNK_SIDE + x].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)) &&
                dc_gpu_upload_chunk(gpu, 0, &initial, err, sizeof(err)) &&
                dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)) &&
                dc_gpu_set_fluid_interval(gpu, 6));
    ASSERT_TRUE(dc_gpu_render_chunks(gpu, err, sizeof(err)) &&
                dc_gpu_readback(gpu, before, DC_CHUNK_CELLS, err, sizeof(err)));
    for (uint32_t i = 0; i < 4; ++i)
        ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_render_chunks(gpu, err, sizeof(err)) &&
                dc_gpu_readback(gpu, snapshot, DC_CHUNK_CELLS, err, sizeof(err)));
    ASSERT_EQ(memcmp(before, snapshot, sizeof(before)), 0);
    ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)) &&
                dc_gpu_render_chunks(gpu, err, sizeof(err)) &&
                dc_gpu_readback(gpu, blended, DC_CHUNK_CELLS, err, sizeof(err)));
    ASSERT_TRUE(memcmp(before, blended, sizeof(before)) != 0);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_equal_elapsed_time_tracks_across_frame_cadences(void) {
    char err[256] = {0};
    dc_chunk_t initial = {0}, fast = {0}, slow = {0};
    for (uint32_t x = 20; x < 44; ++x)
        initial.cells[8 * DC_CHUNK_SIDE + x].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *a = NULL, *b = NULL;
    ASSERT_TRUE(dc_gpu_create(&a, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)) &&
                dc_gpu_create(&b, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)) &&
                dc_gpu_upload_chunk(a, 0, &initial, err, sizeof(err)) &&
                dc_gpu_upload_chunk(b, 0, &initial, err, sizeof(err)) &&
                dc_gpu_set_page(a, 0, 0, 0, err, sizeof(err)) &&
                dc_gpu_set_page(b, 0, 0, 0, err, sizeof(err)) &&
                dc_gpu_set_fluid_interval(a, 6) &&
                dc_gpu_set_fluid_interval(b, 6) &&
                dc_gpu_set_tick_seconds(a, 1.0f / 60.0f) &&
                dc_gpu_set_tick_seconds(b, 2.0f / 60.0f));
    for (uint32_t i = 0; i < 12; ++i)
        ASSERT_TRUE(dc_gpu_tick_step(a, err, sizeof(err)));
    for (uint32_t i = 0; i < 6; ++i)
        ASSERT_TRUE(dc_gpu_tick_step(b, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(a, 0, &fast, err, sizeof(err)) &&
                dc_gpu_download_chunk(b, 0, &slow, err, sizeof(err)));
    uint64_t mass_a = 0, mass_b = 0, depth_a = 0, depth_b = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        mass_a += fast.cells[i].fluid_mass;
        mass_b += slow.cells[i].fluid_mass;
        depth_a += (i / DC_CHUNK_SIDE) * (uint64_t)fast.cells[i].fluid_mass;
        depth_b += (i / DC_CHUNK_SIDE) * (uint64_t)slow.cells[i].fluid_mass;
    }
    ASSERT_EQ(mass_a, mass_b);
    uint64_t difference = depth_a > depth_b ? depth_a - depth_b : depth_b - depth_a;
    printf("equal-time mean depth 60Hz=%.2f 30Hz=%.2f\n",
           (double)depth_a / mass_a, (double)depth_b / mass_b);
    ASSERT_TRUE(difference < mass_a * 3u);
    dc_gpu_destroy(a);
    dc_gpu_destroy(b);
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
    RUN(test_marker_advects_with_gpu_face_velocity);
    RUN(test_markers_sharpen_thin_sheet_without_changing_volume);
    RUN(test_markers_sharpen_splash_lobes_without_changing_volume);
    RUN(test_markers_correct_a_moving_free_surface);
    RUN(test_marker_overlay_is_opt_in);
    RUN(test_marker_pool_stays_bounded_on_dense_interface);
    RUN(test_visible_spring_supplies_fast_flow_in_one_second);
    RUN(test_deep_spring_pool_does_not_spray_across_surface);
    RUN(test_supported_water_spreads_sideways_quickly);
    RUN(test_falling_water_is_not_limited_to_one_cell_per_tick);
    RUN(test_high_painted_water_falls_as_continuous_column);
    RUN(test_water_crosses_vertical_chunk_seam);
    RUN(test_erased_floor_drains_into_lower_chunk);
    RUN(test_still_pool_does_not_spray_above_surface);
    RUN(test_32_cell_high_river_settles_after_surface_displacement);
    RUN(test_camera_shift_rebases_velocity_on_gpu);
    RUN(test_chunk_velocity_survives_gpu_round_trip);
    RUN(test_cropped_viewport_edges_are_internal_fluid_faces);
    RUN(test_projected_water_velocity_has_tiny_final_decay);
    RUN(test_no_slip_wall_damps_tangential_water_more_than_interior);
    RUN(test_fluid_interval_counts_only_scheduled_updates);
    RUN(test_pressure_budget_can_change_per_gpu_context);
    RUN(test_six_fluid_phases_match_one_uncoupled_update);
    RUN(test_elapsed_time_advances_staged_water_further);
    RUN(test_staged_water_renders_from_gpu_snapshots);
    RUN(test_equal_elapsed_time_tracks_across_frame_cadences);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
