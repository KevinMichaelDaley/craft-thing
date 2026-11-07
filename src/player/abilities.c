#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ferrum/player/abilities.h"
#include "ferrum/fire/gpu.h"
#include "dungeoncraft/material_density.h"
#include "../../src/vulkan/gpu_internal.h"

enum { BOLT_SPEED_CELLS = 4u, BOLT_SIDE = 2u,
       STORM_CHILL_KEEP_NUM = 3u, STORM_CHILL_KEEP_DEN = 10u };
#define BOLT_TEMPERATURE_K 1300.f

/* Charge parcel identities climb above every scene-authored id space. */
static uint64_t bolt_parcel_counter(void) {
    static uint64_t counter = 0x8000000000000000ULL;
    return counter++;
}
static bool error(char *err_buf,uint32_t cap,const char *text) {
    if(err_buf&&cap) snprintf(err_buf,cap,"%s",text);
    return false;
}
static bool launch_velocity(const fr_player_point_t *origin,
                            const fr_player_point_t *target, float *vx,
                            float *vy) {
    float dx = (float)(target->chunk.x - origin->chunk.x) * 64.f +
               target->x - origin->x;
    float dy = (float)(target->chunk.y - origin->chunk.y) * 64.f +
               target->y - origin->y;
    float length = sqrtf(dx * dx + dy * dy);
    if (length < 0.001f) {
        *vx = (float)BOLT_SPEED_CELLS;
        *vy = 0.f;
        return true;
    }
    float scale = (float)BOLT_SPEED_CELLS / length;
    *vx = dx * scale;
    *vy = dy * scale;
    return true;
}

bool fr_player_cast_fire_bolt(dc_gpu_t *gpu, uint64_t owner_id,
                              const fr_player_point_t *origin,
                              const fr_player_point_t *target,
                              uint32_t *out_body, char *err_buf,
                              uint32_t err_cap) {
    if (out_body) *out_body = 0u;
    if (!gpu || !owner_id || !origin || !target) return false;
    float vx, vy;
    launch_velocity(origin, target, &vx, &vy);
    float base_x = (float)origin->chunk.x * 64.f + origin->x;
    float base_y = (float)origin->chunk.y * 64.f + origin->y;
    float px = base_x + vx * 2.f, py = base_y + vy * 2.f;
    if (px < 2.f || py < 2.f || px + BOLT_SIDE + 1.f > gpu->width ||
        py + BOLT_SIDE + 1.f > gpu->height)
        return false;
    /* Author a burning coal charge the same way the siege loads are
     * built: paint the cells, seed matching hot reactive parcels, then
     * admit the component as a rigid body that carries its own fire. */
    if (!dc_gpu_paint_material(gpu, (uint32_t)px, (uint32_t)py, 1u,
                               DC_MATERIAL_COAL, err_buf, err_cap))
        return false;
    const fr_reactive_profile_t *profile =
        fr_reactive_profile(DC_MATERIAL_COAL);
    if (!profile) return false;
    uint32_t mass = (uint32_t)lroundf(profile->density * 65536.f);
    uint32_t lx = (uint32_t)px % 64u, ly = (uint32_t)py % 64u;
    uint32_t slot = UINT32_MAX;
    dc_chunk_coord_t coord = {(int64_t)px / 64, (int64_t)py / 64};
    for (uint32_t candidate = 0u; candidate < DC_GPU_CHUNK_SLOTS; candidate++) {
        const uint32_t *registry = dc_gpu_structural_data(gpu) +
            DC_STRUCTURAL_HEADER_WORDS + candidate * 8u;
        if (registry[5] && memcmp(registry, &coord, sizeof(coord)) == 0) {
            slot = candidate;
            break;
        }
    }
    if (slot == UINT32_MAX) return false;
    for (int32_t dy = -1; dy <= 1; dy++)
        for (int32_t dx = -1; dx <= 1; dx++) {
            if (dx * dx + dy * dy > 1) continue;
            int32_t cx = (int32_t)lx + dx, cy = (int32_t)ly + dy;
            if (cx < 0 || cx > 63 || cy < 0 || cy > 63) continue;
            fr_reactive_state_t state;
            if (!fr_reactive_state_default(DC_MATERIAL_COAL, mass, 0,
                    BOLT_TEMPERATURE_K, bolt_parcel_counter(),
                    &state, err_buf, err_cap))
                return false;
            state.flags |= FR_REACTIVE_FLAMING;
            state.ignition_integral = 1.f;
            state.ignition_threshold = .5f;
            if (!fr_gpu_reactive_seed_cell(gpu, slot,
                                           (uint32_t)cy * 64u + (uint32_t)cx,
                                           &state, err_buf, err_cap))
                return false;
        }
    if (!dc_gpu_admit_authored_rigid_cells(gpu, err_buf, err_cap))
        return false;
    /* Find the admitted charge nearest the launch point and launch it. */
    uint32_t bolt = 0u;
    float best = 1e30f;
    for (uint32_t k = 0u; k < 64u; k++) {
        dc_gpu_world_body_t body;
        if (!dc_gpu_read_world_body(gpu, 0x80000001u + k, &body, err_buf,
                                    err_cap))
            continue;
        if (!body.body.active) continue;
        float bx = (float)body.chunk.x * 64.f + body.body.x_fp / 65536.f;
        float by = (float)body.chunk.y * 64.f + body.body.y_fp / 65536.f;
        float distance = (bx - px) * (bx - px) + (by - py) * (by - py);
        if (distance < best) {
            best = distance;
            bolt = 0x80000001u + k;
        }
    }
    if (!bolt || best > 64.f)
        return error(err_buf, err_cap, "Fire bolt charge was not admitted");
    dc_gpu_body_record_t *record = dc_gpu_find_body_record(gpu, bolt);
    if (!record) return false;
    /* A launched charge is live world geometry: visible and awake from now. */
    record->visible = 1u;
    record->body.vx_fp = (int32_t)(vx * 65536.f);
    record->body.vy_fp = (int32_t)(vy * 65536.f);
    record->rest_ticks = 0;
    record->motion_ready = 0;
    /* Force the solver to re-read the record motion so the launch velocity
     * survives the next boundary instead of the stale admitted velocity. */
    if (!dc_gpu_set_body_motion(gpu, bolt, record->angle,
                                record->angular_velocity,
                                record->motion_flags & 3u, err_buf, err_cap))
        return false;
    if (out_body) *out_body = bolt;
    return true;
}

bool fr_player_cast_ice_storm(dc_gpu_t *gpu, const fr_player_point_t *target,
                              uint32_t radius, char *err_buf, uint32_t err_cap) {
    if (!gpu || !target || radius == 0u || radius > 32u) return false;
    float wx = (float)target->chunk.x * 64.f + target->x;
    float wy = (float)target->chunk.y * 64.f + target->y;
    if (wx < radius || wy < radius || wx + radius >= gpu->width ||
        wy + radius >= gpu->height)
        return false;
    /* Soak the bounded disc through the material atlas. */
    if (!dc_gpu_paint_material(gpu, (uint32_t)wx, (uint32_t)wy, radius,
                               DC_MATERIAL_WATER, err_buf, err_cap))
        return false;
    /* Chill every reactive parcel inside the radius on the resident slot. */
    dc_chunk_coord_t coord;
    uint32_t lx = (uint32_t)wx % 64u, ly = (uint32_t)wy % 64u;
    coord = target->chunk;
    uint32_t slot = UINT32_MAX;
    for (uint32_t candidate = 0u; candidate < DC_GPU_CHUNK_SLOTS; candidate++) {
        const uint32_t *registry =
            dc_gpu_structural_data(gpu) + DC_STRUCTURAL_HEADER_WORDS +
            candidate * 8u;
        if (registry[5] && memcmp(registry, &coord, sizeof(coord)) == 0) {
            slot = candidate;
            break;
        }
    }
    if (slot == UINT32_MAX) return true;
    for (uint32_t dy = 0u; dy <= radius * 2u; dy++)
        for (uint32_t dx = 0u; dx <= radius * 2u; dx++) {
            int32_t offset_x = (int32_t)dx - (int32_t)radius;
            int32_t offset_y = (int32_t)dy - (int32_t)radius;
            if (offset_x * offset_x + offset_y * offset_y >
                (int32_t)(radius * radius))
                continue;
            int32_t cell_x = (int32_t)lx + offset_x;
            int32_t cell_y = (int32_t)ly + offset_y;
            if (cell_x < 0 || cell_x > 63 || cell_y < 0 || cell_y > 63)
                continue;
            fr_reactive_record_t parcel;
            bool found = false;
            if (!fr_gpu_reactive_read_cell(gpu, slot,
                                           (uint32_t)cell_y * 64u +
                                               (uint32_t)cell_x,
                                           &parcel, &found, err_buf, err_cap))
                return false;
            if (!found) continue;
            double chilled = fr_reactive_state_energy(&parcel.state) *
                             (double)STORM_CHILL_KEEP_NUM /
                             (double)STORM_CHILL_KEEP_DEN;
            parcel.state.thermal_energy = (float)chilled;
            parcel.state.thermal_energy_residual = 0.f;
            parcel.state.thermal_energy_correction = 0.f;
            if (!fr_gpu_reactive_seed_cell(gpu, slot,
                                           (uint32_t)cell_y * 64u +
                                               (uint32_t)cell_x,
                                           &parcel.state, err_buf, err_cap))
                return false;
        }
    return true;
}
