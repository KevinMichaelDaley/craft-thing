#ifndef FERRUM_TESTS_GREEK_DEPLETION_PROBE_H
#define FERRUM_TESTS_GREEK_DEPLETION_PROBE_H

#include <inttypes.h>

#include "dungeoncraft/reactive_state_layout.h"
#include "ferrum/fire/bulk_phase_gpu.h"

typedef struct {
    uint64_t fuel, water, outside_fuel, mobile;
    uint32_t wet_faces, faces;
} greek_pool_sample_t;

static bool greek_pool_sample(dc_gpu_t *gpu, uint32_t body,
                              greek_pool_sample_t *out, char *err, uint32_t cap) {
    greek_pool_sample_t sample = {0};
    fr_gpu_reactive_stats_t stats;
    if (!greek_ball_water_contact(gpu, body, &sample.wet_faces, &sample.faces,
                                  err, cap) ||
        !fr_gpu_reactive_read_stats(gpu, &stats, err, cap) || stats.live == 0u)
        return false;

    /* Scan the completed authoritative owners so a transferred film never
     * counts as burned merely because its original body lost it. */
    const uint32_t *entries = gpu->reactive->arena.entries.mapped;
    uint32_t attached = 0u;
    for (uint32_t index = 0; index < stats.capacity; ++index) {
        const uint32_t *entry = entries + (size_t)index * FR_ENTRY_WORDS;
        uint32_t owner = entry[FR_ENTRY_OWNER];
        if (owner == 0u) continue;
        if (owner > FR_REACTIVE_OWNER_PARTICLE) return false;
        const uint32_t *state = entry + FR_ENTRY_STATE;
        uint64_t fuel = (uint64_t)state[FR_STATE_COATING_OIL] +
                        state[FR_STATE_COATING_JELLY];
        if (owner == FR_REACTIVE_OWNER_BODY && entry[FR_ENTRY_OWNER + 1u] == body) {
            sample.fuel += fuel;
            sample.water += state[FR_STATE_BOUND_WATER];
            ++attached;
        } else {
            sample.outside_fuel += fuel;
        }
    }
    if (attached == 0u) return false;
    fr_bulk_phase_chunk_t phase;
    for (uint32_t slot = 0; slot < DC_GPU_CHUNK_SLOTS; ++slot) {
        if (!fr_bulk_phase_gpu_export_chunk(gpu, slot, &phase, err, cap)) return false;
        for (uint32_t cell = 0; cell < DC_CHUNK_CELLS; ++cell)
            sample.mobile += (uint64_t)phase.oil_fp[cell] + phase.slime_fp[cell];
    }
    *out = sample;
    return true;
}

static int greek_depletion_record(int argc, char **argv) {
    uint32_t soak, limit;
    if (argc != 5 || !greek_parse_u32(argv[3], &soak) || !soak ||
        !greek_parse_u32(argv[4], &limit) || !limit) {
        fprintf(stderr, "Expected load, soak seconds, and a positive observation limit\n");
        return 2;
    }
    dc_siege_load_t load;
    if (strcmp(argv[2], "greek") == 0) load = DC_SIEGE_LOAD_GREEK;
    else if (strcmp(argv[2], "oil") == 0) load = DC_SIEGE_LOAD_OIL;
    else {
        fprintf(stderr, "Expected a Greek or ordinary-oil load\n");
        return 2;
    }

    char err[256] = {0};
    dc_gpu_t *gpu = greek_grid();
    if (!gpu) return 1;
    dc_siege_scene_t scene;
    greek_pool_sample_t wet, struck;
    fr_gas_gpu_clock_t before_soak, after_soak;
    bool okay = dc_siege_scene_build_douse_pool(gpu, load, &scene, err, sizeof(err)) &&
        fr_gas_gpu_clock(gpu, &before_soak) &&
        greek_ticks(gpu, soak, err, sizeof(err)) &&
        fr_gas_gpu_clock(gpu, &after_soak) &&
        after_soak.physical_steps - before_soak.physical_steps == 60u * (uint64_t)soak &&
        fabs(after_soak.advanced_seconds - before_soak.advanced_seconds - soak) <=
            1e-6 * soak &&
        greek_pool_sample(gpu, scene.coal_body, &wet, err, sizeof(err)) &&
        dc_siege_scene_ignite_load(gpu, &scene, 0.f, err, sizeof(err)) &&
        greek_pool_sample(gpu, scene.coal_body, &struck, err, sizeof(err)) &&
        struck.fuel == wet.fuel && struck.water == wet.water;
    if (!okay) {
        fprintf(stderr, "Incomplete pool soak or strike changed stock. %s\n", err);
        dc_gpu_destroy(gpu);
        return 1;
    }

    for (uint32_t second = 0; ; ++second) {
        fr_gas_gpu_clock_t now;
        if (!fr_gas_gpu_clock(gpu, &now)) break;
        printf("POOL_SAMPLE %u %" PRIu64 " %.9f %" PRIu64 " %" PRIu64
               " %u %u %" PRIu64 " %" PRIu64 "\n",
               second, now.physical_steps - after_soak.physical_steps,
               now.advanced_seconds - after_soak.advanced_seconds,
               struck.fuel, struck.water, struck.wet_faces, struck.faces,
               struck.outside_fuel, struck.mobile);
        fflush(stdout);
        if (struck.fuel == 0u || second == limit) {
            printf("POOL_END %u\n", second);
            dc_gpu_destroy(gpu);
            return 0;
        }
        if (!greek_ticks(gpu, 1u, err, sizeof(err)) ||
            !greek_pool_sample(gpu, scene.coal_body, &struck, err, sizeof(err)))
            break;
    }
    fprintf(stderr, "Incomplete pool observation. %s\n", err);
    dc_gpu_destroy(gpu);
    return 1;
}

#endif
