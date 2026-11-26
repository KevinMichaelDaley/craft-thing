#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dungeoncraft/world_save.h"

enum { WORLD_DIRECTORY_LIMIT = 900, WORLD_PATH_CAP = 1024 };
static const char draft_name[] = ".level-save-draft";
static const char committed_name[] = ".level-save-committed";
static const char publish_name[] = ".level-save-publish";

struct dc_world_save {
    int root;
    bool staging;
    char directory[WORLD_DIRECTORY_LIMIT + 1];
    char draft[WORLD_PATH_CAP];
};

typedef enum { SYNC_FILES, PUBLISH_FILES, DISCARD_FILES } file_action_t;

static bool error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

static bool valid_file(const char *name, bool discard) {
    size_t length = strlen(name);
    if (discard && length > 4 && strcmp(name + length - 4, ".tmp") == 0)
        length -= 4;
    if ((length == sizeof("rigid_bodies.bin") - 1 && strncmp(name, "rigid_bodies.bin", length) == 0) ||
        (length == sizeof("player_inventory.bin") - 1 && strncmp(name, "player_inventory.bin", length) == 0))
        return true;
    if (length != 43 || strncmp(name, "chunk_", 6) != 0 || name[22] != '_' ||
        strncmp(name + 39, ".bin", 4) != 0) return false;
    const char *hex = "0123456789abcdef";
    return strspn(name + 6, hex) == 16 && strspn(name + 23, hex) == 16;
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
            bodies |= strcmp(name, "rigid_bodies.bin") == 0;
            inventory |= strcmp(name, "player_inventory.bin") == 0;
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
    return true;
}

const char *dc_world_save_directory(const dc_world_save_t *save, bool staging) {
    if (!save) return NULL;
    return staging ? (save->staging ? save->draft : NULL) : save->directory;
}

bool dc_world_save_commit(dc_world_save_t *save, char *err, uint32_t cap) {
    if (!save || !save->staging) return error(err, cap, "Missing staged world save");
    if (!visit_files(save->root, draft_name, SYNC_FILES) ||
        renameat(save->root, draft_name, save->root, committed_name) != 0)
        return error(err, cap, "Cannot commit staged world save");
    save->staging = false;
    return (fsync(save->root) == 0 && recover(save->root)) ||
           error(err, cap, "World save committed and needs recovery");
}

void dc_world_save_close(dc_world_save_t *save) {
    if (!save) return;
    if (save->staging) discard_directory(save->root, draft_name);
    close(save->root);
    free(save);
}
