#include <math.h>
#include <stdio.h>

#include "ferrum/fire/gpu.h"
#include "ferrum/player/abilities.h"
#include "../vulkan/gpu_internal.h"

enum { BOLT_SPEED_CELLS = 4u, BOLT_SIDE = 3u, BOLT_CLEARANCE_TICKS = 2u,
       STORM_MAX_RADIUS = 32u,
       STORM_CHILL_KEEP_NUM = 3u, STORM_CHILL_KEEP_DEN = 10u };
#define BOLT_TEMPERATURE_K 1300.f

static bool error(char *err_buf,uint32_t cap,const char *text) {
    if(err_buf&&cap) snprintf(err_buf,cap,"%s",text);
    return false;
}

static bool bolt_point_valid(const fr_player_point_t *point) {
    return point && point->x >= 0.f && point->x < DC_CHUNK_SIDE &&
                    point->y >= 0.f && point->y < DC_CHUNK_SIDE;
}

static double bolt_chunk_difference(int64_t target, int64_t origin) {
    return target >= origin
        ? (double)((uint64_t)target - (uint64_t)origin)
        : -(double)((uint64_t)origin - (uint64_t)target);
}

static void bolt_launch_velocity(const fr_player_point_t *origin,
                                 const fr_player_point_t *target,
                                 double *vx, double *vy) {
    double dx = bolt_chunk_difference(target->chunk.x, origin->chunk.x) *
                DC_CHUNK_SIDE + target->x - origin->x;
    double dy = bolt_chunk_difference(target->chunk.y, origin->chunk.y) *
                DC_CHUNK_SIDE + target->y - origin->y;
    double length = hypot(dx, dy);
    if (length < .001) {
        *vx = BOLT_SPEED_CELLS;
        *vy = 0.;
    } else {
        *vx = dx / length * BOLT_SPEED_CELLS;
        *vy = dy / length * BOLT_SPEED_CELLS;
    }
}

static bool bolt_offset_point(const fr_player_point_t *origin, double dx, double dy,
                              fr_player_point_t *out) {
    double x = origin->x + dx, y = origin->y + dy;
    int64_t floor_x = (int64_t)floor(x), floor_y = (int64_t)floor(y);
    dc_chunk_coord_t chunk_offset;
    uint32_t lx, ly;
    dc_cell_address(floor_x, floor_y, &chunk_offset, &lx, &ly);
    if ((chunk_offset.x > 0 && origin->chunk.x > INT64_MAX - chunk_offset.x) ||
        (chunk_offset.x < 0 && origin->chunk.x < INT64_MIN - chunk_offset.x) ||
        (chunk_offset.y > 0 && origin->chunk.y > INT64_MAX - chunk_offset.y) ||
        (chunk_offset.y < 0 && origin->chunk.y < INT64_MIN - chunk_offset.y))
        return false;
    /* Rounding a fractional local coordinate must not move it to the next chunk. */
    float max_local = nextafterf((float)DC_CHUNK_SIDE, 0.f);
    *out = (fr_player_point_t){{origin->chunk.x + chunk_offset.x,
                                origin->chunk.y + chunk_offset.y},
        fminf((float)(lx + (x - floor_x)), max_local),
        fminf((float)(ly + (y - floor_y)), max_local)};
    return true;
}

static bool bolt_emit(dc_gpu_t *gpu, const fr_player_point_t *origin,
                      double vx, double vy, double travel_ticks, uint32_t *out_body,
                      char *err, uint32_t cap) {
    fr_player_point_t anchor;
    if (!bolt_offset_point(origin, vx * travel_ticks - 1., vy * travel_ticks - 1.,
                            &anchor))
        return error(err, cap, "Fire bolt leaves the world coordinate range");

    static const uint16_t cross[BOLT_SIDE] = {2u, 7u, 2u};
    dc_gpu_world_body_t bolt = {.chunk = anchor.chunk, .body = {
        .x_fp = (int32_t)(uint32_t)anchor.x * (int32_t)DC_FLUID_FULL,
        .y_fp = (int32_t)(uint32_t)anchor.y * (int32_t)DC_FLUID_FULL,
        .vx_fp = (int32_t)lround(vx * DC_FLUID_FULL),
        .vy_fp = (int32_t)lround(vy * DC_FLUID_FULL),
        .width = BOLT_SIDE, .height = BOLT_SIDE}};
    uint32_t id;
    if (!fr_gpu_spawn_burning_pixels(gpu, bolt, cross, DC_GPU_BODY_COAL,
                                      BOLT_TEMPERATURE_K, &id, err, cap))
        return false;
    if (out_body) *out_body = id;
    return true;
}

bool fr_player_cast_fire_bolt(dc_gpu_t *gpu, uint64_t owner_id,
                              const fr_player_point_t *origin,
                              const fr_player_point_t *target,
                              uint32_t *out_body, char *err_buf,
                              uint32_t err_cap) {
    if (out_body) *out_body = 0u;
    if (!gpu || !owner_id || !bolt_point_valid(origin) || !bolt_point_valid(target))
        return error(err_buf, err_cap, "Invalid fire bolt aim or caster");
    double vx, vy;
    bolt_launch_velocity(origin, target, &vx, &vy);
    return bolt_emit(gpu, origin, vx, vy, BOLT_CLEARANCE_TICKS,
                     out_body, err_buf, err_cap);
}

bool fr_player_cast_body_fire_bolt(dc_gpu_t *gpu, uint64_t owner_id,
                                   const dc_gpu_world_body_t *caster,
                                   const fr_player_point_t *target,
                                   uint32_t *out_body, char *err_buf, uint32_t err_cap) {
    if (out_body) *out_body = 0u;
    if (!gpu || !owner_id || !caster || !caster->body.active ||
        !caster->body.width || !caster->body.height || !bolt_point_valid(target))
        return error(err_buf, err_cap, "Invalid fire bolt aim or body caster");
    fr_player_point_t anchor = {caster->chunk,
        caster->body.x_fp / (float)DC_FLUID_FULL, caster->body.y_fp / (float)DC_FLUID_FULL};
    double half_width = caster->body.width * .5, half_height = caster->body.height * .5;
    fr_player_point_t center;
    if (!bolt_point_valid(&anchor) ||
        !bolt_offset_point(&anchor, half_width, half_height, &center))
        return error(err_buf, err_cap, "Invalid fire bolt caster position");
    double vx, vy;
    bolt_launch_velocity(&center, target, &vx, &vy);
    double exit_ticks = fmin(vx == 0. ? INFINITY : half_width / fabs(vx),
                             vy == 0. ? INFINITY : half_height / fabs(vy));
    return bolt_emit(gpu, &center, vx, vy, exit_ticks + BOLT_CLEARANCE_TICKS,
                     out_body, err_buf, err_cap);
}

static bool storm_target_cell(dc_gpu_t *gpu, const fr_player_point_t *target,
                              uint32_t *x, uint32_t *y) {
    uint32_t slot = dc_gpu_find_active_chunk(gpu, target->chunk);
    if (slot == UINT32_MAX) return false;
    uint32_t tile = gpu->slot_page[slot];
    *x = tile % gpu->page_width * DC_CHUNK_SIDE + (uint32_t)target->x;
    *y = tile / gpu->page_width * DC_CHUNK_SIDE + (uint32_t)target->y;
    return true;
}

bool fr_player_cast_ice_storm(dc_gpu_t *gpu, const fr_player_point_t *target,
                              uint32_t radius, char *err_buf, uint32_t err_cap) {
    if (!gpu || !target || radius == 0u || radius > STORM_MAX_RADIUS ||
        !(target->x >= 0.f && target->x < DC_CHUNK_SIDE &&
          target->y >= 0.f && target->y < DC_CHUNK_SIDE))
        return error(err_buf, err_cap, "Invalid ice storm target or radius");
    uint32_t x, y;
    if (!storm_target_cell(gpu, target, &x, &y))
        return error(err_buf, err_cap, "Ice storm target is outside the active pages");
    if (x < radius || y < radius || x + radius >= gpu->width ||
        y + radius >= gpu->height)
        return error(err_buf, err_cap, "Ice storm extends outside the active grid");
    if (!dc_gpu_paint_material(gpu, x, y, radius, DC_MATERIAL_WATER,
                               err_buf, err_cap))
        return false;

    const uint32_t *pages = gpu->page_mapped;
    int32_t reach = (int32_t)radius;
    for (int32_t dy = -reach; dy <= reach; dy++)
        for (int32_t dx = -reach; dx <= reach; dx++) {
            if (dx * dx + dy * dy > reach * reach) continue;
            uint32_t cell_x = (uint32_t)((int32_t)x + dx);
            uint32_t cell_y = (uint32_t)((int32_t)y + dy);
            uint32_t page = pages[cell_y / DC_CHUNK_SIDE * gpu->page_width +
                                  cell_x / DC_CHUNK_SIDE];
            if (!page) continue;
            uint32_t cell = cell_y % DC_CHUNK_SIDE * DC_CHUNK_SIDE +
                            cell_x % DC_CHUNK_SIDE;
            fr_reactive_record_t parcel;
            bool found = false;
            if (!fr_gpu_reactive_read_cell(gpu, page - 1u, cell,
                                           &parcel, &found, err_buf, err_cap))
                return false;
            if (!found) continue;
            double chilled = fr_reactive_state_energy(&parcel.state) *
                             (double)STORM_CHILL_KEEP_NUM /
                             (double)STORM_CHILL_KEEP_DEN;
            parcel.state.thermal_energy = (float)chilled;
            parcel.state.thermal_energy_residual = 0.f;
            parcel.state.thermal_energy_correction = 0.f;
            if (!fr_gpu_reactive_seed_cell(gpu, page - 1u, cell,
                                           &parcel.state, err_buf, err_cap))
                return false;
        }
    return true;
}
