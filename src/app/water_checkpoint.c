#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dungeoncraft/color.h"
#include "../vulkan/liquid_water_domain_gpu.h"
#include "level_internal.h"
#include "offscreen_internal.h"
#include "water_checkpoint.h"

enum { WATER_RECORD_VERSION = 1, WATER_RECORD_MAX_TILES = 2u * DC_GPU_CHUNK_SLOTS };
static const unsigned grid_fields[] = {PREVIOUS_VERTICAL, HEAD, PRESSURE, PREVIOUS_STOCK};

typedef struct {
    uint32_t tick, marker_correction;
    double rigid_budget;
} owner_clock_t;

typedef struct {
    char magic[4];
    uint32_t version, count, interval, phase, reserved;
    float budget, scale;
    double seconds;
    owner_clock_t owners[2];
    dc_chunk_coord_t camera_origin, remote_coord;
    dc_chunk_owner_t remote_owner;
    uint32_t water_face, offset_x, offset_y, camera_reserved;
} cycle_header_t;

typedef struct {
    dc_chunk_coord_t coord;
    uint32_t context, reserved;
} cycle_tile_t;

static bool error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

static bool transfer(FILE *file, void *data, size_t size, bool writing, uint64_t *hash) {
    if ((writing ? fwrite(data, 1, size, file) : fread(data, 1, size, file)) != size) return false;
    *hash = dc_color_hash(*hash, data, size);
    return true;
}

static bool transfer_tile(FILE *file, struct dc_gpu_seam_projection *scratch,
    const dc_gpu_water_tile_t *location, uint32_t tile, bool writing, uint64_t *hash) {
    for (size_t i = 0; i < sizeof(grid_fields) / sizeof(grid_fields[0]); ++i) {
        uint32_t *grid = projection_field(scratch, grid_fields[i]);
        for (uint32_t row = 0; row < DC_CHUNK_SIDE; ++row) {
            size_t start = (size_t)(location->scratch_y + row) * scratch->width + location->scratch_x;
            if (!transfer(file, grid + start, DC_CHUNK_SIDE * sizeof(*grid), writing, hash)) return false;
        }
    }
    uint32_t *markers = projection_field(scratch, MARKER_GRID);
    return transfer(file, markers + tile * DC_CHUNK_CELLS, DC_CHUNK_CELLS * sizeof(*markers), writing, hash);
}

static owner_clock_t owner_clock(const dc_gpu_t *gpu) {
    return (owner_clock_t){gpu->fluid_tick, gpu->marker_correction, gpu->rigid_step_budget};
}

static bool write_cycle(FILE *file, dc_level_view_t *view) {
    dc_gpu_t *gpu = view->gpu, *peer = gpu->water_cycle_peer;
    cycle_header_t header = {.magic = {'D','W','P','1'}, .version = WATER_RECORD_VERSION};
    struct dc_gpu_seam_projection *scratch = gpu->seam_projection;
    if (peer) {
        if (!scratch || scratch->phase_peer != peer || !gpu->fluid_phase ||
            dc_gpu_liquid_seam_pending(gpu) || dc_gpu_liquid_seam_pending(peer)) return false;
        header.count = scratch->tiles;
        header.phase = gpu->fluid_phase;
        header.interval = gpu->fluid_interval;
        header.budget = gpu->fluid_phase_budget;
        header.scale = gpu->fluid_step_scale;
        header.seconds = view->world_seconds;
        header.owners[0] = owner_clock(gpu);
        header.owners[1] = owner_clock(peer);
        const dc_camera_offscreen_t *camera = &view->camera_offscreen;
        if (camera->water_face && camera->workspace && camera->workspace->gpu == peer) {
            header.camera_origin = view->mapped_origin;
            header.remote_coord = camera->coord;
            header.remote_owner = camera->owner;
            header.water_face = camera->water_face;
            header.offset_x = view->camera_offset_x;
            header.offset_y = view->camera_offset_y;
        }
    }
    uint64_t hash = UINT64_C(14695981039346656037);
    if (!transfer(file, &header, sizeof(header), true, &hash)) return false;
    for (uint32_t i = 0; i < header.count; ++i) {
        const dc_gpu_water_tile_t *loc = (const dc_gpu_water_tile_t *)projection_field(scratch, PUBLICATION_TILES) + i;
        dc_gpu_t *owner = loc->context ? peer : gpu;
        cycle_tile_t tile = {.coord = {owner->body_origin.x + loc->x / DC_CHUNK_SIDE,
                                        owner->body_origin.y + loc->y / DC_CHUNK_SIDE}, .context = loc->context};
        if (!transfer(file, &tile, sizeof(tile), true, &hash) || !transfer_tile(file, scratch, loc, i, true, &hash))
            return false;
    }
    return fwrite(&hash, sizeof(hash), 1, file) == 1;
}

bool dc_level_stage_water_checkpoint(dc_level_view_t *view, char *err, uint32_t cap) {
    dc_gpu_t *gpu = view->gpu;
    if (!gpu || (!gpu->water_cycle_peer && !view->water_checkpoint_resumed)) return true;
    const char *draft = dc_world_save_directory(view->save, true);
    char name[1024];
    if (!draft || snprintf(name, sizeof(name), "%s/" DC_WORLD_WATER_CYCLE_RECORD, draft) >= (int)sizeof(name))
        return error(err, cap, "Missing staged water continuation destination");
    int fd = open(name, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600);
    FILE *file = fd < 0 ? NULL : fdopen(fd, "wb");
    if (!file) {
        if (fd >= 0) close(fd);
        return error(err, cap, "Cannot open staged water continuation");
    }
    bool okay = write_cycle(file, view);
    if (fclose(file) != 0 || !okay) return error(err, cap, "Cannot stage water continuation");
    if (gpu->water_cycle_peer) {
        gpu->water_cycle_peer->water_cycle_peer = NULL;
        gpu->water_cycle_peer = NULL;
        gpu->seam_projection->phase_peer = NULL;
        for (uint32_t i = 0; i < view->table.capacity; ++i) {
            if (view->water_generations[i]) view->table.slots[i].pinned = false;
            view->water_generations[i] = 0;
        }
    }
    return true;
}

static bool valid_header(const cycle_header_t *h) {
    if (memcmp(h->magic, "DWP1", 4) || h->version != WATER_RECORD_VERSION || h->reserved) return false;
    if (!h->count) return !h->water_face;
    if (h->water_face && ((h->water_face & (h->water_face-1u)) || h->water_face > 8u ||
        !dc_chunk_owner_valid(h->remote_owner) || h->offset_x >= DC_CHUNK_SIDE || h->offset_y >= DC_CHUNK_SIDE ||
        h->camera_origin.x < INT64_MIN + HALO_CHUNKS || h->camera_origin.y < INT64_MIN + HALO_CHUNKS ||
        h->camera_origin.x > INT64_MAX - VIEW_CHUNKS_X - HALO_CHUNKS ||
        h->camera_origin.y > INT64_MAX - VIEW_CHUNKS_Y - HALO_CHUNKS)) return false;
    if (h->count < 2 || h->count > WATER_RECORD_MAX_TILES ||
        (h->interval != 1 && h->interval != 2 && h->interval != 3 && h->interval != 6) ||
        !h->phase || h->phase >= DC_FLUID_PHASES || !isfinite(h->budget) || h->budget < 0 ||
        !isfinite(h->scale) || h->scale <= 0 || !isfinite(h->seconds) || h->seconds < 0) return false;
    for (unsigned i = 0; i < 2; ++i)
        if (h->owners[i].marker_correction > 1 || !isfinite(h->owners[i].rigid_budget) ||
            h->owners[i].rigid_budget < 0 || h->owners[i].rigid_budget >= 1.) return false;
    return true;
}

static bool read_cycle(FILE *file, dc_level_view_t *view, const dc_liquid_seam_endpoint_t *tiles,
    uint32_t count, char *err, uint32_t cap) {
    cycle_header_t header;
    uint64_t hash = UINT64_C(14695981039346656037), stored_hash;
    if (!transfer(file, &header, sizeof(header), false, &hash) || !valid_header(&header))
        return error(err, cap, "Invalid water continuation header");
    dc_gpu_t *gpu = view->gpu, *peer = NULL;
    dc_gpu_water_tile_t locations[WATER_RECORD_MAX_TILES];
    bool seen[WATER_RECORD_MAX_TILES] = {false};
    if (header.count) {
        uint32_t width, height;
        if (header.seconds != view->world_seconds || count != header.count || !tiles || tiles[0].gpu != gpu ||
            !dc_gpu_water_checkpoint_layout(tiles, count, locations, &width, &height))
            return error(err, cap, "Saved water continuation requires its mapped owners and clock");
        if (!dc_gpu_prepare_water_projection(gpu, width, height, count, err, cap)) return false;
    }
    for (uint32_t n = 0; n < header.count; ++n) {
        cycle_tile_t saved;
        if (!transfer(file, &saved, sizeof(saved), false, &hash)) return error(err, cap, "Incomplete water continuation");
        uint32_t i = 0;
        while (i < count && (tiles[i].coord.x != saved.coord.x || tiles[i].coord.y != saved.coord.y)) ++i;
        if (i == count || seen[i] || locations[i].context != saved.context || saved.reserved)
            return error(err, cap, "Water continuation tile ownership changed");
        seen[i] = true;
        if (saved.context) peer = tiles[i].gpu;
        if (!transfer_tile(file, gpu->seam_projection, locations + i, i, false, &hash))
            return error(err, cap, "Truncated water phase scratch");
    }
    if (fread(&stored_hash, sizeof(stored_hash), 1, file) != 1 || stored_hash != hash ||
        fgetc(file) != EOF || ferror(file)) return error(err, cap, "Invalid water continuation checksum or length");
    if (header.count) {
        dc_gpu_t *owners[] = {gpu, peer};
        for (unsigned i = 0; i < 2; ++i) {
            owners[i]->fluid_interval = header.interval;
            owners[i]->fluid_phase = header.phase;
            owners[i]->fluid_phase_budget = header.budget;
            owners[i]->fluid_step_scale = header.scale;
            owners[i]->fluid_tick = header.owners[i].tick;
            owners[i]->rigid_step_budget = header.owners[i].rigid_budget;
            owners[i]->marker_correction = header.owners[i].marker_correction;
            owners[i]->water_cycle_peer = owners[1u-i];
        }
        struct dc_gpu_seam_projection *scratch = gpu->seam_projection;
        scratch->phase_peer = peer;
        scratch->phase_first_coord = tiles[0].coord;
        scratch->restore_phase = false;
        memcpy(projection_field(scratch, PUBLICATION_TILES), locations, count * sizeof(*locations));
        view->water_checkpoint_resumed = true;
    }
    view->water_checkpoint_checked = true;
    return true;
}

static bool open_checkpoint(dc_level_view_t *view, FILE **input, char *err, uint32_t cap) {
    const char *root = dc_world_save_directory(view->save, false);
    char path[1024];
    if (!root || snprintf(path, sizeof(path), "%s/" DC_WORLD_WATER_CYCLE_RECORD, root) >= (int)sizeof(path))
        return error(err, cap, "Invalid water continuation path");
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0 && errno == ENOENT) { view->water_checkpoint_checked = true; *input = NULL; return true; }
    FILE *file = fd < 0 ? NULL : fdopen(fd, "rb");
    if (!file) {
        if (fd >= 0) close(fd);
        return error(err, cap, "Cannot open water continuation");
    }
    *input = file;
    return true;
}

bool dc_level_restore_water_camera(dc_level_view_t *view, char *err, uint32_t cap) {
    FILE *file;
    if (!open_checkpoint(view, &file, err, cap)) return false;
    if (!file) return true;
    uint64_t hash = UINT64_C(14695981039346656037), stored;
    cycle_header_t h;
    bool okay = transfer(file, &h, sizeof(h), false, &hash) && valid_header(&h);
    if (okay) {
        const size_t tile_bytes = sizeof(cycle_tile_t) + DC_CHUNK_CELLS * (sizeof(grid_fields) + sizeof(uint32_t));
        unsigned char block[8192];
        size_t left = tile_bytes * h.count;
        while (okay && left) {
            size_t bytes = left < sizeof(block) ? left : sizeof(block);
            okay = transfer(file, block, bytes, false, &hash);
            left -= bytes;
        }
        okay = okay && fread(&stored, sizeof(stored), 1, file) == 1 && hash == stored &&
            fgetc(file) == EOF && !ferror(file) && (!h.count || h.seconds == view->world_seconds);
    }
    fclose(file);
    if (!okay) return error(err, cap, "Invalid saved water continuation");
    if (!h.count) view->water_checkpoint_checked = true;
    if (h.water_face) {
        view->water_camera_pending = true;
        view->origin = view->mapped_origin = h.camera_origin;
        view->camera_offset_x = h.offset_x;
        view->camera_offset_y = h.offset_y;
        view->camera_offscreen = (dc_camera_offscreen_t){.phase = DC_CAMERA_OFFSCREEN_STORED,
            .coord = h.remote_coord, .owner = h.remote_owner, .water_face = h.water_face,
            .time = {h.seconds, true}};
    }
    return true;
}

bool dc_level_resume_water_checkpoint(dc_level_view_t *view, const dc_liquid_seam_endpoint_t *tiles,
    uint32_t count, char *err, uint32_t cap) {
    if (view->water_checkpoint_checked || !view->save) return true;
    FILE *file;
    if (!open_checkpoint(view, &file, err, cap)) return false;
    if (!file) return true;
    bool okay = read_cycle(file, view, tiles, count, err, cap);
    fclose(file);
    return okay;
}
