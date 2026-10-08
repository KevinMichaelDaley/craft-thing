#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gpu_internal.h"

typedef struct {
    char magic[4];
    uint32_t version, count, reserved;
} body_header_t;

_Static_assert(sizeof(body_header_t) == 16, "Body snapshot header layout changed");
_Static_assert(sizeof(dc_gpu_world_body_t) == 48, "Body snapshot record layout changed");
_Static_assert(sizeof(dc_gpu_body_shape_t) == 72, "Body shape snapshot layout changed");

typedef struct {
    dc_gpu_world_body_t world;
    dc_gpu_body_shape_t shape;
} shaped_body_t;

typedef struct { float angle, angular_velocity; uint32_t flags, reserved; } saved_motion_t;
typedef struct {
    dc_gpu_world_body_t world;
    dc_gpu_body_shape_t shape;
    saved_motion_t motion;
} moving_body_t;

_Static_assert(sizeof(moving_body_t) == 136, "Moving body snapshot layout changed");

_Static_assert(sizeof(shaped_body_t) == 120, "Shaped body snapshot layout changed");

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

bool dc_gpu_save_bodies(dc_gpu_t *gpu, const char *path, char *err, uint32_t cap) {
    if (!gpu || !path || !*path) return error(err, cap, "Invalid body snapshot path");
    char temporary[1024];
    int length = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (length < 0 || (size_t)length >= sizeof(temporary))
        return error(err, cap, "Body snapshot path is too long");
    moving_body_t bodies[DC_GPU_BODY_CAPACITY] = {0};
    body_header_t header = { .magic = {'D', 'C', 'B', '1'}, .version = 3 };
    for (uint32_t i = 0; i < gpu->body_count; ++i) {
        if (!gpu->body_ids[i]) continue;
        if (!dc_gpu_read_world_body(gpu, gpu->body_ids[i], &bodies[header.count].world, err, cap) ||
            !dc_gpu_read_body_shape(gpu, gpu->body_ids[i], &bodies[header.count].shape, err, cap))
            return false;
        dc_gpu_body_motion_t motion;
        if (!dc_gpu_read_body_motion(gpu,gpu->body_ids[i],&motion,err,cap)) return false;
        bodies[header.count].motion = (saved_motion_t){motion.angle,motion.angular_velocity,motion.flags,0};
        ++header.count;
    }
    FILE *file = fopen(temporary, "wb");
    if (!file) return error(err, cap, "Cannot open body snapshot for writing");
    bool okay = fwrite(&header, sizeof(header), 1, file) == 1 &&
        fwrite(bodies, sizeof(*bodies), header.count, file) == header.count;
    if (fclose(file) != 0) okay = false;
    if (okay) okay = rename(temporary, path) == 0;
    if (!okay) { remove(temporary); return error(err, cap, "Cannot save body snapshot"); }
    return true;
}

bool dc_gpu_load_bodies(dc_gpu_t *gpu, const char *path, char *err, uint32_t cap) {
    if (!gpu || !path || !*path) return error(err, cap, "Invalid body snapshot path");
    FILE *file = fopen(path, "rb");
    if (!file) return errno == ENOENT ? true : error(err, cap, "Cannot open body snapshot");
    body_header_t header = {0};
    moving_body_t bodies[DC_GPU_BODY_CAPACITY] = {0};
    bool okay = fread(&header, sizeof(header), 1, file) == 1 &&
        memcmp(header.magic, "DCB1", 4) == 0 && header.version >= 1 && header.version <= 3 && !header.reserved &&
        header.count <= DC_GPU_BODY_CAPACITY;
    for (uint32_t i = 0; okay && i < header.count; ++i) {
        okay = fread(&bodies[i].world, sizeof(bodies[i].world), 1, file) == 1;
        if (okay && header.version >= 2)
            okay = fread(&bodies[i].shape, sizeof(bodies[i].shape), 1, file) == 1;
        if (okay && header.version == 3)
            okay = fread(&bodies[i].motion, sizeof(bodies[i].motion), 1, file) == 1;
    }
    if (okay) okay = fgetc(file) == EOF && !ferror(file);
    if (fclose(file) != 0) okay = false;
    for (uint32_t i = 0; okay && i < header.count; ++i) {
        dc_gpu_body_t body = bodies[i].world.body;
        okay = body.id && body.active && body.width && body.height &&
            body.width <= DC_GPU_BODY_MAX_SIDE && body.height <= DC_GPU_BODY_MAX_SIDE &&
            body.x_fp >= 0 && body.y_fp >= 0 &&
            body.x_fp < (int32_t)(DC_CHUNK_SIDE * DC_FLUID_FULL) &&
            body.y_fp < (int32_t)(DC_CHUNK_SIDE * DC_FLUID_FULL);
        if (okay && bodies[i].shape.count)
            okay = dc_gpu_valid_body_shape(&body, &bodies[i].shape);
        else if (okay) {
            dc_gpu_body_shape_t empty = {0};
            okay = memcmp(&bodies[i].shape, &empty, sizeof(empty)) == 0;
        }
        for (uint32_t j = 0; okay && j < i; ++j) okay = bodies[j].world.body.id != body.id;
        saved_motion_t motion=bodies[i].motion;
        okay = okay && isfinite(motion.angle) && fabsf(motion.angle)<=3.141593f &&
            isfinite(motion.angular_velocity) && fabsf(motion.angular_velocity)<=.5f &&
            !(motion.flags & ~3u) && !motion.reserved;
    }
    if (!okay) return error(err, cap, "Invalid or truncated body snapshot");
    memset(gpu->body_mapped, 0, sizeof(dc_gpu_body_record_t) * DC_GPU_BODY_CAPACITY);
    memset(gpu->body_ids, 0, sizeof(gpu->body_ids));
    gpu->body_count = 0;
    gpu->body_refresh_pending = true;
    for (uint32_t i = 0; i < header.count; ++i) {
        bool spawned = bodies[i].shape.count ?
            dc_gpu_spawn_convex_body(gpu, bodies[i].world, &bodies[i].shape, err, cap) :
            dc_gpu_spawn_world_body(gpu, bodies[i].world, err, cap);
        if (!spawned) return false;
        saved_motion_t motion=bodies[i].motion;
        if (!dc_gpu_set_body_motion(gpu,bodies[i].world.body.id,motion.angle,
                                    motion.angular_velocity,motion.flags,err,cap)) return false;
    }
    return dc_gpu_set_body_origin(gpu, gpu->body_origin, err, cap);
}
