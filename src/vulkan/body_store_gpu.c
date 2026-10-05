#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "gpu_internal.h"

typedef struct {
    char magic[4];
    uint32_t version, count, reserved;
} body_header_t;

_Static_assert(sizeof(body_header_t) == 16, "Body snapshot header layout changed");
_Static_assert(sizeof(dc_gpu_world_body_t) == 48, "Body snapshot record layout changed");

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
    dc_gpu_world_body_t bodies[DC_GPU_BODY_CAPACITY];
    body_header_t header = { .magic = {'D', 'C', 'B', '1'}, .version = 1 };
    for (uint32_t i = 0; i < gpu->body_count; ++i) {
        if (!gpu->body_ids[i]) continue;
        if (!dc_gpu_read_world_body(gpu, gpu->body_ids[i], &bodies[header.count], err, cap))
            return false;
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
    body_header_t header;
    dc_gpu_world_body_t bodies[DC_GPU_BODY_CAPACITY];
    bool okay = fread(&header, sizeof(header), 1, file) == 1 &&
        memcmp(header.magic, "DCB1", 4) == 0 && header.version == 1 && !header.reserved &&
        header.count <= DC_GPU_BODY_CAPACITY;
    if (okay) okay = fread(bodies, sizeof(*bodies), header.count, file) == header.count &&
                     fgetc(file) == EOF && !ferror(file);
    if (fclose(file) != 0) okay = false;
    for (uint32_t i = 0; okay && i < header.count; ++i) {
        dc_gpu_body_t body = bodies[i].body;
        okay = body.id && body.active && body.width && body.height &&
            body.width <= 16 && body.height <= 16 && body.x_fp >= 0 && body.y_fp >= 0 &&
            body.x_fp < (int32_t)(DC_CHUNK_SIDE * DC_FLUID_FULL) &&
            body.y_fp < (int32_t)(DC_CHUNK_SIDE * DC_FLUID_FULL);
        for (uint32_t j = 0; okay && j < i; ++j) okay = bodies[j].body.id != body.id;
    }
    if (!okay) return error(err, cap, "Invalid or truncated body snapshot");
    memset(gpu->body_mapped, 0, sizeof(dc_gpu_body_record_t) * DC_GPU_BODY_CAPACITY);
    memset(gpu->body_ids, 0, sizeof(gpu->body_ids));
    gpu->body_count = 0;
    gpu->body_refresh_pending = true;
    for (uint32_t i = 0; i < header.count; ++i)
        if (!dc_gpu_spawn_world_body(gpu, bodies[i], err, cap)) return false;
    return dc_gpu_set_body_origin(gpu, gpu->body_origin, err, cap);
}
