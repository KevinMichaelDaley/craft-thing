#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dungeoncraft/color.h"
#include "dungeoncraft/world_save.h"

enum { WORLD_DIRECTORY_LIMIT = 900, WORLD_PATH_CAP = 1024 };
enum { CHUNK_PREFIX_LENGTH = sizeof("chunk_") - 1, CHUNK_COORD_LENGTH = 16,
       CHUNK_SEPARATOR = CHUNK_PREFIX_LENGTH + CHUNK_COORD_LENGTH,
       CHUNK_SUFFIX = CHUNK_SEPARATOR + 1 + CHUNK_COORD_LENGTH,
       CHUNK_NAME_LENGTH = CHUNK_SUFFIX + sizeof(".bin") - 1 };
static const char draft_name[] = ".level-save-draft";
static const char committed_name[] = ".level-save-committed";
static const char publish_name[] = ".level-save-publish";
static const char clock_temp_name[] = DC_WORLD_CLOCK_RECORD ".tmp";

struct dc_world_save {
    int root;
    bool staging;
    bool clock_cached;
    double stored_clock;
    char directory[WORLD_DIRECTORY_LIMIT + 1];
    char draft[WORLD_PATH_CAP];
};

typedef enum { SYNC_FILES, PUBLISH_FILES, DISCARD_FILES } file_action_t;

typedef struct {
    char magic[4];
    uint32_t version;
    double seconds;
    uint64_t checksum;
} world_clock_record_t;

enum { WORLD_CLOCK_VERSION = 1 };
_Static_assert(sizeof(world_clock_record_t) == 24, "World clock record layout changed");

static bool error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

static bool valid_file(const char *name, bool discard) {
    size_t length = strlen(name);
    if (discard && length > 4 && strcmp(name + length - 4, ".tmp") == 0)
        length -= 4;
    if ((length == sizeof(DC_WORLD_BODY_RECORD) - 1 && strncmp(name, DC_WORLD_BODY_RECORD, length) == 0) ||
        (length == sizeof(DC_WORLD_CLOCK_RECORD) - 1 && strncmp(name, DC_WORLD_CLOCK_RECORD, length) == 0) ||
        (length == sizeof(DC_WORLD_PLAYER_RECORD) - 1 && strncmp(name, DC_WORLD_PLAYER_RECORD, length) == 0))
        return true;
    if (length != CHUNK_NAME_LENGTH || strncmp(name, "chunk_", CHUNK_PREFIX_LENGTH) != 0 ||
        name[CHUNK_SEPARATOR] != '_' || strncmp(name + CHUNK_SUFFIX, ".bin", sizeof(".bin") - 1) != 0)
        return false;
    const char *hex = "0123456789abcdef";
    return strspn(name + CHUNK_PREFIX_LENGTH, hex) == CHUNK_COORD_LENGTH &&
           strspn(name + CHUNK_SEPARATOR + 1, hex) == CHUNK_COORD_LENGTH;
}

static bool sync_file(int directory, const char *name) {
    int file = openat(directory, name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (file < 0) return false;
    struct stat status;
    bool okay = fstat(file, &status) == 0 && S_ISREG(status.st_mode) && fsync(file) == 0;
    return close(file) == 0 && okay;
}

static bool publish_file(int source, int root, const char *name) {
    struct stat status;
    if (fstatat(source, name, &status, AT_SYMLINK_NOFOLLOW) != 0 || !S_ISREG(status.st_mode)) return false;
    if (unlinkat(root, publish_name, 0) != 0 && errno != ENOENT) return false;
    return linkat(source, name, root, publish_name, 0) == 0 &&
           renameat(root, publish_name, root, name) == 0 &&
           (unlinkat(root, publish_name, 0) == 0 || errno == ENOENT);
}

static bool visit_files(int root, const char *subdirectory, file_action_t action) {
    int source = openat(root, subdirectory, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (source < 0) return errno == ENOENT && action == DISCARD_FILES;
    DIR *entries = fdopendir(source);
    if (!entries) { close(source); return false; }
    bool okay = true;
    bool bodies = false, inventory = false;
    while (okay) {
        errno = 0;
        const struct dirent *entry = readdir(entries);
        if (!entry) { okay = errno == 0; break; }
        const char *name = entry->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        okay = valid_file(name, action == DISCARD_FILES);
        if (!okay) break;
        switch (action) {
        case SYNC_FILES:
            okay = sync_file(source, name);
            bodies |= strcmp(name, DC_WORLD_BODY_RECORD) == 0;
            inventory |= strcmp(name, DC_WORLD_PLAYER_RECORD) == 0;
            break;
        case PUBLISH_FILES: okay = publish_file(source, root, name); break;
        case DISCARD_FILES: okay = unlinkat(source, name, 0) == 0; break;
        }
    }
    if (okay && action == SYNC_FILES) okay = bodies && inventory && fsync(source) == 0;
    return closedir(entries) == 0 && okay;
}

static bool discard_directory(int root, const char *subdirectory) {
    return visit_files(root, subdirectory, DISCARD_FILES) &&
        (unlinkat(root, subdirectory, AT_REMOVEDIR) == 0 || errno == ENOENT);
}

static bool recover(int root) {
    struct stat status;
    if (fstatat(root, committed_name, &status, AT_SYMLINK_NOFOLLOW) != 0) return errno == ENOENT;
    if (!S_ISDIR(status.st_mode)) return false;
    /* Keep the committed links until every canonical name is durable. */
    return visit_files(root, committed_name, PUBLISH_FILES) && fsync(root) == 0 &&
           discard_directory(root, committed_name) && fsync(root) == 0;
}

dc_world_save_t *dc_world_save_open(const char *directory, char *err, uint32_t cap) {
    if (!directory || !directory[0] || strlen(directory) > WORLD_DIRECTORY_LIMIT) {
        error(err, cap, "Invalid world save directory");
        return NULL;
    }
    if (mkdir(directory, 0755) != 0 && errno != EEXIST) {
        error(err, cap, "Cannot create world save directory");
        return NULL;
    }
    int root = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (root < 0 || flock(root, LOCK_EX | LOCK_NB) != 0) {
        if (root >= 0) close(root);
        error(err, cap, "Cannot acquire the world save writer");
        return NULL;
    }
    if (!recover(root) || !discard_directory(root, draft_name)) {
        close(root);
        error(err, cap, "Cannot recover the world save directory");
        return NULL;
    }
    dc_world_save_t *save = calloc(1, sizeof(*save));
    if (!save) { close(root); error(err, cap, "Cannot allocate a world save"); return NULL; }
    save->root = root;
    memcpy(save->directory, directory, strlen(directory) + 1);
    snprintf(save->draft, sizeof(save->draft), "%s/%s", directory, draft_name);
    return save;
}

bool dc_world_save_begin(dc_world_save_t *save, char *err, uint32_t cap) {
    if (!save) return error(err, cap, "Missing world save");
    if (save->staging) return true;
    if (!recover(save->root) || mkdirat(save->root, draft_name, 0700) != 0)
        return error(err, cap, "Cannot create world save staging");
    save->staging = true;
    save->clock_cached = false;
    return true;
}

const char *dc_world_save_directory(const dc_world_save_t *save, bool staging) {
    if (!save) return NULL;
    return staging ? (save->staging ? save->draft : NULL) : save->directory;
}

static bool valid_clock(double seconds) {
    return isfinite(seconds) && seconds >= 0.;
}

static uint64_t clock_checksum(const world_clock_record_t *record) {
    return dc_color_hash(UINT64_C(14695981039346656037), record,
                         offsetof(world_clock_record_t, checksum));
}

bool dc_world_save_load_clock(const dc_world_save_t *save, double *seconds, char *err, uint32_t cap) {
    if (!save || !seconds) return error(err, cap, "Missing world clock destination");
    int fd = openat(save->root, DC_WORLD_CLOCK_RECORD, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        if (errno != ENOENT) return error(err, cap, "Cannot open world clock");
        *seconds = 0.;
        return true;
    }
    struct stat status;
    FILE *file = fstat(fd, &status) == 0 && S_ISREG(status.st_mode) ? fdopen(fd, "rb") : NULL;
    if (!file) {
        close(fd);
        return error(err, cap, "Cannot read world clock");
    }
    world_clock_record_t record;
    bool okay = fread(&record, sizeof(record), 1, file) == 1 && fgetc(file) == EOF && !ferror(file);
    if (fclose(file) != 0 || !okay || memcmp(record.magic, "DWC1", 4) != 0 ||
        record.version != WORLD_CLOCK_VERSION || !valid_clock(record.seconds) ||
        record.checksum != clock_checksum(&record))
        return error(err, cap, "Invalid world clock snapshot");
    *seconds = record.seconds;
    return true;
}

static bool write_clock(int directory, double seconds, bool durable) {
    if (unlinkat(directory, clock_temp_name, 0) != 0 && errno != ENOENT) return false;
    int fd = openat(directory, clock_temp_name, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (fd < 0) return false;
    world_clock_record_t record = {
        .magic = {'D', 'W', 'C', '1'}, .version = WORLD_CLOCK_VERSION, .seconds = seconds
    };
    record.checksum = clock_checksum(&record);
    FILE *file = fdopen(fd, "wb");
    bool okay = file && fwrite(&record, sizeof(record), 1, file) == 1 &&
                fflush(file) == 0 && (!durable || fsync(fd) == 0);
    if ((file ? fclose(file) : close(fd)) != 0) okay = false;
    if (!okay || renameat(directory, clock_temp_name, directory, DC_WORLD_CLOCK_RECORD) != 0) {
        unlinkat(directory, clock_temp_name, 0);
        return false;
    }
    return !durable || fsync(directory) == 0;
}

bool dc_world_save_store_clock(dc_world_save_t *save, double seconds, char *err, uint32_t cap) {
    if (!save || !valid_clock(seconds)) return error(err, cap, "Invalid world clock time");
    if (save->clock_cached && save->stored_clock == seconds) return true;
    save->clock_cached = false;
    int flags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC;
    int directory = save->staging ? openat(save->root, draft_name, flags) : save->root;
    if (directory < 0) return error(err, cap, "Cannot open world clock staging");
    bool okay = write_clock(directory, seconds, !save->staging);
    if (save->staging && close(directory) != 0) okay = false;
    if (!okay) return error(err, cap, "Cannot save world clock");
    save->stored_clock = seconds;
    save->clock_cached = true;
    return true;
}

bool dc_world_save_commit(dc_world_save_t *save, char *err, uint32_t cap) {
    if (!save || !save->staging) return error(err, cap, "Missing staged world save");
    if (!visit_files(save->root, draft_name, SYNC_FILES) ||
        renameat(save->root, draft_name, save->root, committed_name) != 0)
        return error(err, cap, "Cannot commit staged world save");
    save->staging = false;
    save->clock_cached = false;
    return (fsync(save->root) == 0 && recover(save->root)) ||
           error(err, cap, "World save committed and needs recovery");
}

void dc_world_save_close(dc_world_save_t *save) {
    if (!save) return;
    if (save->staging) discard_directory(save->root, draft_name);
    close(save->root);
    free(save);
}
