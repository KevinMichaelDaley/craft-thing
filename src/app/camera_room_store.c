#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dungeoncraft/color.h"
#include "level_internal.h"
#include "level_offscreen.h"

typedef struct {
    char magic[4];
    uint32_t version;
    uint64_t count;
    double seconds;
} catalog_header_t;

typedef struct {
    dc_chunk_coord_t coord;
    dc_chunk_owner_t owner;
    double seconds;
} catalog_entry_t;

static bool error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

static bool sealed(const dc_camera_offscreen_t *room) {
    return room->phase != DC_CAMERA_OFFSCREEN_IDLE && !room->water_face && !room->rigid_group && !room->bond;
}

bool dc_camera_catalog_active(const dc_level_view_t *view) {
    if (view->room_catalog_loaded || sealed(&view->camera_offscreen)) return true;
    for (size_t i = 0; i < view->sealed_capacity; ++i)
        if (sealed(view->sealed_rooms + i)) return true;
    return false;
}

static bool path_for(const dc_level_view_t *view, bool staging, char *path, size_t cap) {
    const char *dir = dc_world_save_directory(view->save, staging);
    if (!dir) return false;
    int n = snprintf(path, cap, "%s/" DC_WORLD_ROOM_RECORD, dir);
    return n > 0 && (size_t)n < cap;
}

static int compare(const void *first, const void *second) {
    const catalog_entry_t *a = first, *b = second;
    if (a->coord.x != b->coord.x) return a->coord.x < b->coord.x ? -1 : 1;
    return (a->coord.y > b->coord.y) - (a->coord.y < b->coord.y);
}

static bool valid(const catalog_entry_t *e, double world_seconds) {
    return dc_chunk_owner_valid(e->owner) && isfinite(e->seconds) &&
        e->seconds >= 0. && e->seconds <= world_seconds;
}

static uint64_t checksum(const catalog_header_t *h, const catalog_entry_t *entries) {
    uint64_t hash = dc_color_hash(UINT64_C(14695981039346656037), h, sizeof(*h));
    return dc_color_hash(hash, entries, h->count * sizeof(*entries));
}

bool dc_camera_catalog_stage(dc_level_view_t *view, char *err, uint32_t cap) {
    char path[1024];
    if (!path_for(view, true, path, sizeof(path))) return error(err, cap, "Missing staged room catalog");
    catalog_header_t h = {.magic = {'D','C','R','1'}, .version = 1u, .seconds = view->world_seconds};
    catalog_entry_t *entries = calloc(view->sealed_capacity + 1u, sizeof(*entries));
    if (!entries) return error(err, cap, "Cannot allocate room catalog");
    bool okay = true;
    for (size_t i = 0; i <= view->sealed_capacity && okay; ++i) {
        const dc_camera_offscreen_t *r = i ? view->sealed_rooms + i - 1u : &view->camera_offscreen;
        if (!sealed(r) || dc_chunk_table_find(&view->table, r->coord, NULL)) continue;
        entries[h.count] = (catalog_entry_t){r->coord, r->owner, r->time.seconds};
        okay = r->phase != DC_CAMERA_OFFSCREEN_SAVING && r->time.known && valid(entries + h.count, h.seconds);
        ++h.count;
    }
    qsort(entries, h.count, sizeof(*entries), compare);
    for (size_t i = 1; i < h.count; ++i) okay &= compare(entries+i-1, entries+i) != 0;
    uint64_t hash = checksum(&h, entries);
    int fd = okay ? open(path, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600) : -1;
    FILE *f = fd < 0 ? NULL : fdopen(fd, "wb");
    if (f) {
        okay = fwrite(&h, sizeof(h), 1, f) == 1 && fwrite(entries, sizeof(*entries), h.count, f) == h.count &&
            fwrite(&hash, sizeof(hash), 1, f) == 1;
        okay = fclose(f) == 0 && okay;
    } else {
        if (fd >= 0) close(fd);
        okay = false;
    }
    free(entries);
    return okay || error(err, cap, "Cannot stage confirmed room catalog");
}

bool dc_camera_catalog_load(dc_level_view_t *view, char *err, uint32_t cap) {
    char path[1024];
    if (!path_for(view, false, path, sizeof(path))) return error(err, cap, "Invalid room catalog path");
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return errno == ENOENT || error(err, cap, "Cannot open room catalog");
    struct stat st;
    FILE *f = fstat(fd, &st) == 0 && S_ISREG(st.st_mode) ? fdopen(fd, "rb") : NULL;
    if (!f) { close(fd); return error(err, cap, "Cannot read room catalog"); }
    catalog_header_t h;
    uint64_t hash;
    bool okay = fread(&h, sizeof(h), 1, f) == 1 && !memcmp(h.magic, "DCR1", 4) && h.version == 1u &&
        (!h.count || h.seconds == view->world_seconds) &&
        h.count <= (SIZE_MAX-sizeof(h)-sizeof(hash))/sizeof(catalog_entry_t) &&
        h.count <= SIZE_MAX/sizeof(dc_camera_offscreen_t) &&
        (uint64_t)st.st_size == sizeof(h) + h.count*sizeof(catalog_entry_t) + sizeof(hash);
    catalog_entry_t *entries = okay && h.count ? calloc(h.count, sizeof(*entries)) : NULL;
    okay = okay && (!h.count || (entries && fread(entries, sizeof(*entries), h.count, f) == h.count)) &&
        fread(&hash, sizeof(hash), 1, f) == 1 && hash == checksum(&h, entries) && fgetc(f) == EOF && !ferror(f);
    okay = fclose(f) == 0 && okay;
    for (size_t i = 0; okay && i < h.count; ++i)
        okay = valid(entries + i, h.seconds) && (!i || compare(entries+i-1, entries+i) < 0) &&
            !dc_camera_find_room(view, entries[i].coord);
    if (okay && h.count) {
        view->sealed_rooms = calloc(h.count, sizeof(*view->sealed_rooms));
        okay = view->sealed_rooms != NULL;
        if (okay) {
            view->sealed_capacity = h.count;
            for (size_t i = 0; i < h.count; ++i)
                view->sealed_rooms[i] = (dc_camera_offscreen_t){.phase = DC_CAMERA_OFFSCREEN_STORED,
                    .coord = entries[i].coord, .owner = entries[i].owner,
                    .time = {entries[i].seconds, true}, .restored_record = true};
            view->room_catalog_loaded = true;
        }
    }
    free(entries);
    if (!okay) return error(err, cap, "Invalid saved room catalog");
    return !view->room_catalog_loaded || dc_level_view_stage_world_save(view, err, cap);
}
