#include <math.h>
#include <string.h>

#include "ferrum/player/manipulation.h"
#include "../vulkan/runtime_body_gpu.h"

enum { CARRY_HOLD_HEIGHT = 8, MAX_THROW_SPEED = 4,
       CHUNK_FP = DC_CHUNK_SIDE * DC_FLUID_FULL };

static bool valid_actor(const dc_gpu_world_body_t *player) {
    return player && player->body.x_fp >= 0 && player->body.x_fp < CHUNK_FP &&
                     player->body.y_fp >= 0 && player->body.y_fp < CHUNK_FP;
}

static bool valid_aim(const fr_player_point_t *aim) {
    return aim && aim->x >= 0.f && aim->x < DC_CHUNK_SIDE &&
                  aim->y >= 0.f && aim->y < DC_CHUNK_SIDE;
}

static double chunk_cells(int64_t target, int64_t origin) {
    double chunks = target >= origin
        ? (double)((uint64_t)target - (uint64_t)origin)
        : -(double)((uint64_t)origin - (uint64_t)target);
    return chunks * DC_CHUNK_SIDE;
}

static dc_chunk_coord_t body_chunk(const dc_gpu_body_record_t *body) {
    uint64_t x = ((uint64_t)body->chunk_x[1] << 32u) | body->chunk_x[0];
    uint64_t y = ((uint64_t)body->chunk_y[1] << 32u) | body->chunk_y[0];
    dc_chunk_coord_t chunk;
    memcpy(&chunk.x, &x, sizeof(x));
    memcpy(&chunk.y, &y, sizeof(y));
    return chunk;
}

static uint32_t aimed_body(dc_gpu_t *gpu, const fr_player_point_t *aim,
                            char *err, uint32_t cap) {
    uint32_t slot = dc_gpu_find_active_chunk(gpu, aim->chunk);
    if (slot == UINT32_MAX || !dc_gpu_set_body_origin(gpu, gpu->body_origin, err, cap))
        return 0u;
    uint32_t page = gpu->slot_page[slot];
    uint32_t x = page % gpu->page_width * DC_CHUNK_SIDE + (uint32_t)aim->x;
    uint32_t y = page / gpu->page_width * DC_CHUNK_SIDE + (uint32_t)aim->y;
    uint32_t id;
    return dc_gpu_read_occupancy(gpu, x, y, &id, NULL, err, cap) ? id : 0u;
}

static uint32_t live_generation(const dc_gpu_t *gpu, uint32_t slot, uint32_t id) {
    const dc_body_storage_owner_t *owner = dc_gpu_body_owner(gpu, slot);
    return owner && owner->state == DC_BODY_OWNER_LIVE && owner->runtime_id == id
        ? owner->generation : 0u;
}

static dc_gpu_body_record_t *held_body(dc_gpu_t *gpu, fr_player_carry_t *carry) {
    if (!gpu || !carry || !carry->body_id) return NULL;
    dc_gpu_body_record_t *body = carry->slot < dc_gpu_body_capacity(gpu)
        ? dc_gpu_body_record(gpu, carry->slot) : NULL;
    if (body && body->body.active && body->body.id == carry->body_id &&
        (body->motion_flags & DC_GPU_BODY_KINEMATIC) &&
        (!gpu->runtime_body_consumers ||
         live_generation(gpu, carry->slot, carry->body_id) == carry->generation))
        return body;
    *carry = (fr_player_carry_t){0};
    return NULL;
}

bool fr_player_manipulation_grab(dc_gpu_t *gpu, fr_player_carry_t *carry,
    const fr_player_manipulation_t *settings, const dc_gpu_world_body_t *player,
    const fr_player_point_t *aim, char *err, uint32_t cap) {
    if (!gpu || !carry || !settings || !valid_actor(player) || !valid_aim(aim) ||
        !(settings->reach >= 0.) || !(settings->carry_weight >= 0.) ||
        held_body(gpu, carry)) return false;
    uint32_t id = aimed_body(gpu, aim, err, cap);
    if (!id || id == player->body.id) return false;
    for (uint32_t slot = 0; slot < dc_gpu_body_capacity(gpu); ++slot) {
        dc_gpu_body_record_t *body = dc_gpu_body_record(gpu, slot);
        if (!body->body.active || body->body.id != id) continue;
        if ((body->motion_flags & DC_GPU_BODY_KINEMATIC) ||
            body->reserved[0] == DC_GPU_STRUCTURAL_TAG ||
            !(body->mass > 0.f && body->mass <= settings->carry_weight)) return false;
        dc_chunk_coord_t chunk = body_chunk(body);
        double dx = chunk_cells(chunk.x, player->chunk.x) +
            (body->local_x_fp - player->body.x_fp) / (double)DC_FLUID_FULL;
        double dy = chunk_cells(chunk.y, player->chunk.y) +
            (body->local_y_fp - player->body.y_fp) / (double)DC_FLUID_FULL;
        if (hypot(dx, dy) > settings->reach) return false;
        uint32_t generation = live_generation(gpu, slot, id);
        if (gpu->runtime_body_consumers && !generation) return false;
        uint32_t restore = body->motion_flags & DC_GPU_BODY_LOCK_ROTATION;
        if (!dc_gpu_set_body_motion(gpu, id, body->angle, 0.f,
                DC_GPU_BODY_KINEMATIC | DC_GPU_BODY_LOCK_ROTATION, err, cap))
            return false;
        body->body.vx_fp = 0;
        body->body.vy_fp = 0;
        *carry = (fr_player_carry_t){id, slot, generation, restore};
        return true;
    }
    return false;
}

bool fr_player_manipulation_carrying(const fr_player_carry_t *carry, uint32_t id) {
    return carry && id && carry->body_id == id;
}

bool fr_player_manipulation_tick(dc_gpu_t *gpu, fr_player_carry_t *carry,
    const dc_gpu_world_body_t *player) {
    dc_gpu_body_record_t *body = held_body(gpu, carry);
    if (!body) return true;
    if (!valid_actor(player)) return false;
    dc_chunk_coord_t anchor = player->chunk;
    int32_t y = player->body.y_fp - (int32_t)(CARRY_HOLD_HEIGHT * DC_FLUID_FULL);
    if (y < 0) {
        if (anchor.y == INT64_MIN) return true;
        --anchor.y;
        y += CHUNK_FP;
    }
    if (dc_gpu_find_active_chunk(gpu, anchor) == UINT32_MAX) return true;
    uint64_t chunk_x = (uint64_t)anchor.x, chunk_y = (uint64_t)anchor.y;
    body->chunk_x[0] = (uint32_t)chunk_x;
    body->chunk_x[1] = (uint32_t)(chunk_x >> 32u);
    body->chunk_y[0] = (uint32_t)chunk_y;
    body->chunk_y[1] = (uint32_t)(chunk_y >> 32u);
    body->local_x_fp = player->body.x_fp;
    body->local_y_fp = y;
    body->body.vx_fp = 0;
    body->body.vy_fp = 0;
    body->rest_ticks = 0;
    gpu->body_refresh_pending = true;
    return true;
}

static bool release_body(dc_gpu_t *gpu, fr_player_carry_t *carry,
                          const dc_gpu_body_record_t *body, char *err, uint32_t cap) {
    if (!dc_gpu_set_body_motion(gpu, carry->body_id, body->angle,
                                 body->angular_velocity, carry->restore_flags, err, cap))
        return false;
    *carry = (fr_player_carry_t){0};
    return true;
}

bool fr_player_manipulation_drop(dc_gpu_t *gpu, fr_player_carry_t *carry,
    char *err, uint32_t cap) {
    dc_gpu_body_record_t *body = held_body(gpu, carry);
    return body && release_body(gpu, carry, body, err, cap);
}

bool fr_player_manipulation_throw(dc_gpu_t *gpu, fr_player_carry_t *carry,
    const fr_player_manipulation_t *settings, const fr_player_point_t *aim,
    char *err, uint32_t cap) {
    if (!settings || !valid_aim(aim) || !isfinite(settings->throw_force) ||
        settings->throw_force < 0.) return false;
    dc_gpu_body_record_t *body = held_body(gpu, carry);
    if (!body) return false;
    dc_chunk_coord_t anchor = body_chunk(body);
    double dx = chunk_cells(aim->chunk.x, anchor.x) + aim->x -
                body->local_x_fp / (double)DC_FLUID_FULL;
    double dy = chunk_cells(aim->chunk.y, anchor.y) + aim->y -
                body->local_y_fp / (double)DC_FLUID_FULL;
    double length = hypot(dx, dy);
    double speed = fmin(settings->throw_force, MAX_THROW_SPEED) * DC_FLUID_FULL;
    if (!release_body(gpu, carry, body, err, cap)) return false;
    body->body.vx_fp = (int32_t)lround(length ? speed * (dx / length) : speed);
    body->body.vy_fp = (int32_t)lround(length ? speed * (dy / length) : 0.);
    return true;
}
