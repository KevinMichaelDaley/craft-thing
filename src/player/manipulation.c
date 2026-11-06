#include <math.h>
#include <string.h>

#include "ferrum/player/manipulation.h"
#include "../../src/vulkan/gpu_internal.h"

enum { GRAB_SCAN_FIRST = 0x80000000u, GRAB_SCAN_COUNT = 64u,
       CARRY_HOLD_HEIGHT = 8u };

static bool body_contains(const dc_gpu_world_body_t *b, float x, float y) {
    float bx = (float)b->chunk.x * 64.f + b->body.x_fp / 65536.f;
    float by = (float)b->chunk.y * 64.f + b->body.y_fp / 65536.f;
    float half_w = b->body.width * 0.5f, half_h = b->body.height * 0.5f;
    return x >= bx - half_w && x <= bx + half_w && y >= by - half_h &&
           y <= by + half_h;
}

bool fr_player_manipulation_grab(dc_gpu_t *gpu,fr_player_carry_t *carry,
    const fr_player_manipulation_t *m,float player_x,float player_y,
    const fr_player_point_t *reticle,char *err_buf,uint32_t err_cap) {
    (void)err_buf;(void)err_cap;
    if (!gpu || !carry || !m || !reticle || carry->body_id) return false;
    float rx = (float)reticle->chunk.x * 64.f + reticle->x;
    float ry = (float)reticle->chunk.y * 64.f + reticle->y;
    float best = 1e30f;uint32_t best_id = 0u;
    for (uint32_t k = 0u;k < GRAB_SCAN_COUNT;k++) {
        uint32_t id = GRAB_SCAN_FIRST + k;
        dc_gpu_world_body_t b;
        if (!dc_gpu_read_world_body(gpu,id,&b,err_buf,err_cap)) continue;
        if (!b.body.active) continue;
        if (!body_contains(&b,rx,ry)) continue;
        float bx = (float)b.chunk.x * 64.f + b.body.x_fp / 65536.f;
        float by = (float)b.chunk.y * 64.f + b.body.y_fp / 65536.f;
        float distance = sqrtf((bx-player_x)*(bx-player_x)+(by-player_y)*(by-player_y));
        if (distance > m->reach) continue;
        if (distance < best) {best = distance;best_id = id;}
    }
    if (!best_id) return false;
    dc_gpu_body_motion_t motion;
    if (!dc_gpu_read_body_motion(gpu,best_id,&motion,err_buf,err_cap)) return false;
    if ((double)motion.mass > m->carry_weight) return false;
    carry->body_id = best_id;
    return dc_gpu_set_body_motion(gpu,best_id,motion.angle,motion.angular_velocity,
        DC_GPU_BODY_KINEMATIC|DC_GPU_BODY_LOCK_ROTATION,err_buf,err_cap);
}

bool fr_player_manipulation_carrying(const fr_player_carry_t *carry,
    uint32_t body_id) {
    return carry && carry->body_id && carry->body_id == body_id;
}

bool fr_player_manipulation_tick(dc_gpu_t *gpu,fr_player_carry_t *carry,
    float player_x,float player_y,char *err_buf,uint32_t err_cap) {
    (void)err_buf;(void)err_cap;
    if (!carry || !carry->body_id || !gpu) return true;
    uint32_t carried_id = carry->body_id;
    /* The carry point rides over the player's head toward the reticle side. */
    float carry_x = player_x,carry_y = player_y - (float)CARRY_HOLD_HEIGHT;
    dc_gpu_body_record_t *b = dc_gpu_find_body_record(gpu,carried_id);
    if (!b) return true;
    /* Position within the body's own chunk anchor, wrapped to the chunk side. */
    float base_x = (float)b->chunk_x[0] * 64.f;
    float base_y = (float)b->chunk_y[0] * 64.f;
    b->body.x_fp = b->local_x_fp = (int32_t)((carry_x - base_x) * 65536.f);
    b->body.y_fp = b->local_y_fp = (int32_t)((carry_y - base_y) * 65536.f);
    b->body.vx_fp = 0;b->body.vy_fp = 0;
    b->rest_ticks = 0;
    return true;
}

bool fr_player_manipulation_drop(dc_gpu_t *gpu,fr_player_carry_t *carry,
    char *err_buf,uint32_t err_cap) {
    (void)err_buf;(void)err_cap;
    if (!carry || !carry->body_id) return false;
    uint32_t carried_id = carry->body_id;
    return dc_gpu_set_body_motion(gpu,carried_id,0.f,0.f,0u,err_buf,err_cap) &&
        (carry->body_id = 0u,true);
}

bool fr_player_manipulation_throw(dc_gpu_t *gpu,fr_player_carry_t *carry,
    const fr_player_manipulation_t *m,float player_x,float player_y,
    const fr_player_point_t *reticle,char *err_buf,uint32_t err_cap) {
    (void)player_x;(void)player_y;
    if (!carry || !carry->body_id || !m || !reticle) return false;
    uint32_t carried_id = carry->body_id;
    float rx = (float)reticle->chunk.x * 64.f + reticle->x;
    float ry = (float)reticle->chunk.y * 64.f + reticle->y;
    dc_gpu_world_body_t carried;
    if (!dc_gpu_read_world_body(gpu,carried_id,&carried,err_buf,err_cap))
        return false;
    float bx = (float)carried.chunk.x * 64.f + carried.body.x_fp / 65536.f;
    float by = (float)carried.chunk.y * 64.f + carried.body.y_fp / 65536.f;
    float dx = rx - bx,dy = ry - by;
    float length = sqrtf(dx*dx+dy*dy);
    float speed = (float)m->throw_force;
    if (length > 0.001f) {
        dx /= length;dy /= length;
    } else {dx = 1.f;dy = 0.f;}
    if (speed > 4.f) speed = 4.f;
    bool okay = dc_gpu_set_body_motion(gpu,carried_id,0.f,0.f,0u,err_buf,err_cap);
    /* Linear launch through the body record's velocity words. */
    dc_gpu_body_record_t *b = dc_gpu_find_body_record(gpu,carried_id);
    if (b) {
        b->body.vx_fp = (int32_t)(dx * speed * 65536.f);
        b->body.vy_fp = (int32_t)(dy * speed * 65536.f);
    }
    carry->body_id = 0u;
    return okay;
}

void fr_player_manipulation_reset(fr_player_carry_t *carry) {
    if (carry) carry->body_id = 0u;
}
