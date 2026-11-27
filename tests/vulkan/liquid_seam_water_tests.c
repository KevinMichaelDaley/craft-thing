#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/liquid_seam.h"
#include "ferrum/fire/bulk_phase_gpu.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/renderer/gpu.h"
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

enum { FORWARD = 12, REVERSE = 16, SHALLOW = 21, BLOCKED = 26, BACKPRESSURE = 31, DONORS = 4 };
static const uint32_t donor_stock = 49152u;

static uint32_t index_on_line(bool y_axis, uint32_t normal, uint32_t lane, uint32_t width) {
    return y_axis ? normal * width + lane : lane * width + normal;
}

static void seed_basin(dc_chunk_t *first, dc_chunk_t *second, fr_bulk_phase_chunk_t *bulk,
                       bool y_axis) {
    dc_chunk_t *chunks[2] = {first, second};
    const uint32_t lanes[] = {FORWARD, REVERSE, SHALLOW, BLOCKED, BACKPRESSURE};
    for (uint32_t tile = 0; tile < 2; ++tile)
        for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
            chunks[tile]->cells[i].material = DC_MATERIAL_STONE;
    for (uint32_t l = 0; l < sizeof(lanes) / sizeof(lanes[0]); ++l) {
        uint32_t lane = lanes[l];
        for (uint32_t normal = 0; normal < 2u * DC_CHUNK_SIDE; ++normal) {
            uint32_t tile = normal / DC_CHUNK_SIDE, local = normal % DC_CHUNK_SIDE;
            uint32_t i = index_on_line(y_axis, local, lane, DC_CHUNK_SIDE);
            chunks[tile]->cells[i].material = DC_MATERIAL_AIR;
            if (lane == BACKPRESSURE && tile == 1u) bulk->oil_fp[i] = DC_FLUID_FULL;
            float speed = lane == REVERSE ? -9.f : lane == SHALLOW ? 0.65f : 9.f;
            if (y_axis) chunks[tile]->face_velocity[i].y = speed;
            else chunks[tile]->face_velocity[i].x = speed;
        }
        const uint32_t donor = lane == REVERSE ? 65u :
                               (lane == FORWARD || lane == BLOCKED ? 62u : 63u);
        const uint32_t count = lane == FORWARD || lane == REVERSE ? DONORS : 1u;
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t source = lane == REVERSE ? donor + i : donor - i;
            chunks[source / DC_CHUNK_SIDE]->cells[
                index_on_line(y_axis, source % DC_CHUNK_SIDE, lane, DC_CHUNK_SIDE)].fluid_mass = donor_stock;
        }
    }
}

static bool upload(dc_gpu_t *gpu, uint32_t slot, uint32_t x, uint32_t y, const dc_chunk_t *chunk,
                   const fr_bulk_phase_chunk_t *bulk) {
    return dc_gpu_upload_chunk(gpu, slot, chunk, g_err, sizeof(g_err)) &&
        (!bulk || fr_bulk_phase_gpu_import_chunk(gpu, slot, chunk, bulk, g_err, sizeof(g_err))) &&
        dc_gpu_set_page(gpu, x, y, slot, g_err, sizeof(g_err));
}

static void seed_payload(const dc_chunk_t *chunk, unsigned owner,
                         dc_chunk_colors_t *colors, float heat[DC_CHUNK_CELLS]) {
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        if (!chunk->cells[i].fluid_mass) continue;
        colors->water[i][0] = owner ? 0.2f : 0.9f;
        colors->water[i][1] = (float)(i % 7u) / 7.f;
        colors->water[i][2] = owner ? 0.8f : 0.1f;
        colors->water[i][3] = 1.f;
        heat[i] = owner ? -1.f - (float)(i % 4u) : 3.f + (float)(i % 5u);
    }
}

static bool upload_payload(dc_gpu_t *gpu, uint32_t slot, const dc_chunk_t *chunk,
    const dc_chunk_colors_t *colors, const float *heat, bool thermal) {
    return fr_renderer_upload_chunk_colors(gpu, slot, colors, g_err, sizeof(g_err)) &&
        (!thermal || (fr_thermal_gpu_enable(gpu, g_err, sizeof(g_err)) &&
            fr_thermal_gpu_import_water_chunk(gpu, slot, chunk, heat, g_err, sizeof(g_err))));
}

static void seed_apertures(dc_gpu_t *gpu, bool y_axis, uint32_t x, uint32_t y) {
    uint32_t *plane = (uint32_t *)gpu->occupancy_mapped + (y_axis ? 4u : 3u) * gpu->width * gpu->height;
    const uint32_t base = y * gpu->width + x;
    plane[base + index_on_line(y_axis, 63, SHALLOW, gpu->width)] = DC_FLUID_FULL * 3u / 4u;
    plane[base + index_on_line(y_axis, 63, BLOCKED, gpu->width)] = DC_FLUID_FULL;
}

static bool native_water_passes(dc_gpu_t *gpu, float step_seconds, VkBuffer pressure_readback) {
    gpu->fluid_step_scale = step_seconds * 60.f;
    gpu->timed_fluid = true;
    if (!fr_renderer_begin(gpu)) return false;
    fr_renderer_barrier(gpu->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_HOST_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    dc_gpu_record_projection_begin(gpu);
    dc_gpu_record_pressure_passes(gpu, 0, DC_PRESSURE_PASS_COUNT);
    gpu->rigid_occupancy_present = true;
    dc_gpu_record_fluid_transport(gpu, 0u, 4u);
    fr_renderer_barrier(gpu->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    const VkBufferCopy copy = {.size = 2u * DC_CHUNK_CELLS * sizeof(float)};
    vkCmdCopyBuffer(gpu->command, gpu->pressure_a_buffer, pressure_readback, 1, &copy);
    fr_renderer_barrier(gpu->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_COPY_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    return fr_renderer_submit(gpu);
}

static void compare_plan(dc_liquid_seam_axis_t axis) {
    const bool y_axis = axis == DC_LIQUID_SEAM_Y;
    const float step_seconds = y_axis ? 1.f / 30.f : 1.f / 60.f;
    const uint32_t width = y_axis ? 64u : 128u, height = y_axis ? 128u : 64u;
    const char shader[] = "build/shaders/pattern.comp.spv";
    dc_chunk_t *chunks[2] = {calloc(1, sizeof(*chunks[0])), calloc(1, sizeof(*chunks[1]))};
    dc_chunk_t *download = calloc(1, sizeof(*download));
    fr_bulk_phase_chunk_t *bulk = calloc(1, sizeof(*bulk));
    dc_water_seam_plan_t *plan = calloc(1, sizeof(*plan));
    dc_water_seam_plan_t *saved = malloc(sizeof(*saved));
    dc_chunk_colors_t *colors = calloc(2, sizeof(*colors));
    dc_chunk_colors_t *read_colors = calloc(1, sizeof(*read_colors));
    float (*heat)[DC_CHUNK_CELLS] = calloc(2, sizeof(*heat));
    float *read_heat = calloc(DC_CHUNK_CELLS, sizeof(*read_heat));
    dc_gpu_t device = {0}, *mono = NULL;
    dc_liquid_seam_endpoint_t owners[2] = {
        {.coord = {-2, 3}, .tile_x = 1, .slot = 1},
        {.coord = {-2 + !y_axis, 3 + y_axis}, .tile_y = 2, .slot = 0}
    };
    VkBuffer readback = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void *mapped = NULL;
    g_err[0] = '\0';

    ASSERT_TRUE(chunks[0] && chunks[1] && download && bulk && plan && saved && colors && read_colors && heat && read_heat);
    seed_basin(chunks[0], chunks[1], bulk, y_axis);
    ASSERT_TRUE(dc_gpu_initialize_device(&device, 3u * DC_CHUNK_CELLS * sizeof(dc_cell_t),
                                        0, 0, 0, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_create_shared(&owners[0].gpu, &device, 128, 64, shader, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_create_shared(&owners[1].gpu, &device, 64, 192, shader, g_err, sizeof(g_err)));
    for (unsigned owner = 0; owner < 2; ++owner) {
        dc_liquid_seam_endpoint_t ep = owners[owner];
        chunks[owner]->coord = ep.coord;
        seed_payload(chunks[owner], owner, &colors[owner], heat[owner]);
        const dc_chunk_coord_t origin = {ep.coord.x - ep.tile_x, ep.coord.y - ep.tile_y};
        ASSERT_TRUE(dc_gpu_set_body_origin(ep.gpu, origin, g_err, sizeof(g_err)));
        ASSERT_TRUE(upload(ep.gpu, ep.slot, ep.tile_x, ep.tile_y, chunks[owner], owner ? bulk : NULL));
        ASSERT_TRUE(upload_payload(ep.gpu, ep.slot, chunks[owner], &colors[owner], heat[owner], y_axis));
    }
    seed_apertures(owners[0].gpu, y_axis, DC_CHUNK_SIDE, 0);
    owners[0].gpu->rigid_occupancy_present = true;
    ASSERT_TRUE(dc_gpu_plan_water_seam(owners[0], owners[1], axis, step_seconds, NULL,
                                      plan, g_err, sizeof(g_err)));

    for (uint32_t i = 0; i < DONORS; ++i) {
        uint32_t first_donor = index_on_line(y_axis, 62u - i, FORWARD, DC_CHUNK_SIDE);
        uint32_t second_donor = index_on_line(y_axis, 1u + i, REVERSE, DC_CHUNK_SIDE);
        ASSERT_TRUE(plan->cells[0][first_donor].crossing_out_fp > 0);
        ASSERT_TRUE(plan->cells[1][second_donor].crossing_out_fp > 0);
    }
    uint32_t far_forward = 0, far_reverse = 0;
    for (uint32_t i = 1; i < DC_CHUNK_SIDE; ++i) {
        far_forward += plan->cells[1][index_on_line(y_axis, i, FORWARD, DC_CHUNK_SIDE)].crossing_in_fp;
        far_reverse += plan->cells[0][index_on_line(y_axis, i - 1u, REVERSE, DC_CHUNK_SIDE)].crossing_in_fp;
    }
    ASSERT_TRUE(far_forward > 0 && far_reverse > 0);
    ASSERT_TRUE(plan->crossing_fp[FORWARD] > 0 && plan->crossing_fp[REVERSE] < 0);
    ASSERT_TRUE(plan->crossing_fp[SHALLOW] > 0);
    ASSERT_INT_EQ(plan->projection[SHALLOW].aperture_fp, DC_FLUID_FULL / 4u);
    for (uint32_t i = 0; i < DC_CHUNK_SIDE; ++i) {
        uint32_t recipient = index_on_line(y_axis, i, SHALLOW, DC_CHUNK_SIDE);
        ASSERT_TRUE(plan->cells[1][recipient].crossing_in_fp <= DC_FLUID_FULL / 4u);
    }
    ASSERT_INT_EQ(plan->crossing_fp[BLOCKED], 0);
    ASSERT_INT_EQ(plan->crossing_fp[BACKPRESSURE], 0);

    ASSERT_EQ(plan->thermal_enabled, y_axis);
    float far_pigment = 0.f;
    for (uint32_t i = 1; i < DC_CHUNK_SIDE; ++i) {
        uint32_t index = index_on_line(y_axis, i, FORWARD, DC_CHUNK_SIDE);
        if (plan->cells[1][index].crossing_in_fp) far_pigment += plan->payload[1][index].final_color[0];
    }
    ASSERT_TRUE(far_pigment > 0.f);
    for (unsigned owner = 0; owner < 2; ++owner)
        for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
            const dc_water_seam_payload_t *payload = &plan->payload[owner][i];
            ASSERT_INT_EQ(memcmp(payload->initial_color, colors[owner].water[i],
                                  sizeof(payload->initial_color)), 0);
            ASSERT_EQ(payload->initial_excess_heat, y_axis ? heat[owner][i] : 0.f);
        }

    memcpy(saved, plan, sizeof(*saved));
    ASSERT_TRUE(dc_gpu_plan_water_seam(owners[0], owners[1], axis, step_seconds, NULL,
                                      plan, g_err, sizeof(g_err)));
    ASSERT_INT_EQ(memcmp(saved, plan, sizeof(*saved)), 0);
    dc_liquid_seam_endpoint_t stale = owners[1];
    stale.slot = stale.slot + 1u;
    ASSERT_TRUE(!dc_gpu_plan_water_seam(owners[0], stale, axis, step_seconds, NULL,
                                       plan, g_err, sizeof(g_err)));
    ASSERT_INT_EQ(memcmp(saved, plan, sizeof(*saved)), 0);
    for (unsigned owner = 0; owner < 2; ++owner) {
        ASSERT_TRUE(dc_gpu_download_chunk(owners[owner].gpu, owners[owner].slot, download,
                                          g_err, sizeof(g_err)));
        ASSERT_INT_EQ(memcmp(download->cells, chunks[owner]->cells, sizeof(download->cells)), 0);
        ASSERT_INT_EQ(memcmp(download->face_velocity, chunks[owner]->face_velocity,
                             sizeof(download->face_velocity)), 0);
        ASSERT_TRUE(fr_renderer_download_chunk_colors(owners[owner].gpu, owners[owner].slot,
                                                       read_colors, g_err, sizeof(g_err)));
        ASSERT_INT_EQ(memcmp(read_colors->water, colors[owner].water, sizeof(read_colors->water)), 0);
        if (y_axis) {
            ASSERT_TRUE(fr_thermal_gpu_read_water_chunk(owners[owner].gpu, owners[owner].slot,
                read_heat, DC_CHUNK_CELLS, g_err, sizeof(g_err)));
            ASSERT_INT_EQ(memcmp(read_heat, heat[owner], sizeof(*heat)), 0);
        }
        dc_gpu_destroy(owners[owner].gpu);
        owners[owner].gpu = NULL;
    }

    ASSERT_TRUE(dc_gpu_create_shared(&mono, &device, width, height, shader, g_err, sizeof(g_err)));
    ASSERT_TRUE(upload(mono, 0, 0, 0, chunks[0], NULL));
    ASSERT_TRUE(upload(mono, 1, !y_axis, y_axis, chunks[1], bulk));
    for (unsigned owner = 0; owner < 2; ++owner)
        ASSERT_TRUE(upload_payload(mono, owner, chunks[owner], &colors[owner], heat[owner], y_axis));
    seed_apertures(mono, y_axis, 0, 0);
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(mono, 2u * DC_CHUNK_CELLS * sizeof(float),
                                         &readback, &memory, &mapped, g_err, sizeof(g_err)));
    ASSERT_TRUE(native_water_passes(mono, step_seconds, readback));
    double total_heat_initial = 0., total_heat_final = 0.;
    int32_t stock_delta[2][DC_CHUNK_SIDE] = {{0}};
    int32_t crossing[2][DC_CHUNK_SIDE] = {{0}};
    for (unsigned owner = 0; owner < 2; ++owner) {
        ASSERT_TRUE(dc_gpu_download_chunk(mono, owner, download, g_err, sizeof(g_err)));
        ASSERT_TRUE(fr_renderer_download_chunk_colors(mono, owner, read_colors, g_err, sizeof(g_err)));
        if (y_axis)
            ASSERT_TRUE(fr_thermal_gpu_read_water_chunk(mono, owner, read_heat, DC_CHUNK_CELLS,
                                                        g_err, sizeof(g_err)));
        for (uint32_t lane = 0; lane < DC_CHUNK_SIDE; ++lane)
            for (uint32_t i = 0; i < DC_CHUNK_SIDE; ++i) {
                uint32_t index = index_on_line(y_axis, i, lane, DC_CHUNK_SIDE);
                dc_water_seam_cell_t cell = plan->cells[owner][index];
                ASSERT_INT_EQ(cell.initial_fp, chunks[owner]->cells[index].fluid_mass);
                ASSERT_INT_EQ(cell.final_fp, download->cells[index].fluid_mass);
                const dc_water_seam_payload_t *payload = &plan->payload[owner][index];
                for (unsigned component = 0; component < 4; ++component)
                    ASSERT_TRUE(fabsf(payload->final_color[component] -
                                      read_colors->water[index][component]) < 1e-5f);
                ASSERT_TRUE(fabsf(payload->final_excess_heat - (y_axis ? read_heat[index] : 0.f)) < 1e-5f);
                total_heat_initial += payload->initial_excess_heat;
                total_heat_final += payload->final_excess_heat;
                stock_delta[owner][lane] += (int32_t)cell.final_fp - (int32_t)cell.initial_fp;
                crossing[owner][lane] += (int32_t)cell.crossing_in_fp - (int32_t)cell.crossing_out_fp;
            }
    }

    ASSERT_TRUE(fabs(total_heat_initial - total_heat_final) < 1e-4);
    const float *pressure = mapped;
    for (uint32_t lane = 0; lane < DC_CHUNK_SIDE; ++lane) {
        ASSERT_INT_EQ(stock_delta[0][lane], crossing[0][lane]);
        ASSERT_INT_EQ(stock_delta[1][lane], crossing[1][lane]);
        ASSERT_INT_EQ(plan->crossing_fp[lane], -stock_delta[0][lane]);
        ASSERT_INT_EQ(plan->crossing_fp[lane], stock_delta[1][lane]);
        const uint32_t a = index_on_line(y_axis, 63, lane, width);
        const uint32_t b = index_on_line(y_axis, 64, lane, width);
        const float work = (pressure[a] - pressure[b]) * ((float)stock_delta[1][lane] / DC_FLUID_FULL);
        ASSERT_TRUE(fabsf(plan->pressure_work[lane] - work) < 1e-5f);
    }
    ASSERT_TRUE(fabsf(plan->pressure_work[SHALLOW]) > 1e-5f);
    ASSERT_INT_EQ(dc_gpu_validation_errors(&device), 0u);
    PASS();

cleanup:
    if (mapped) vkUnmapMemory(mono->device, memory);
    if (readback) vkDestroyBuffer(mono->device, readback, NULL);
    if (memory) vkFreeMemory(mono->device, memory, NULL);
    dc_gpu_destroy(mono);
    dc_gpu_destroy(owners[1].gpu);
    dc_gpu_destroy(owners[0].gpu);
    dc_gpu_release_device(&device);
    free(read_heat);
    free(heat);
    free(read_colors);
    free(colors);
    free(saved);
    free(plan);
    free(bulk);
    free(download);
    free(chunks[1]);
    free(chunks[0]);
}

static void test_horizontal_water_crossings_match_native_sweeps(void) {
    compare_plan(DC_LIQUID_SEAM_X);
}

static void test_vertical_water_crossings_match_native_sweeps(void) {
    compare_plan(DC_LIQUID_SEAM_Y);
}

int main(void) {
    RUN(test_horizontal_water_crossings_match_native_sweeps);
    RUN(test_vertical_water_crossings_match_native_sweeps);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
