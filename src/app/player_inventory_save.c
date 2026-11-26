#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dungeoncraft/color.h"
#include "player_inventory_save.h"

typedef struct {
    uint64_t owner;
    fr_player_inventory_t inventory;
} player_inventory_record_t;

typedef struct {
    char magic[4];
    uint32_t version;
    player_inventory_record_t players[FR_INPUT_PLAYERS];
    uint64_t checksum;
} player_inventory_save_t;

_Static_assert(sizeof(player_inventory_record_t) == 9u * sizeof(uint64_t), "Inventory record layout changed");

static bool inventory_error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

static uint64_t inventory_checksum(const player_inventory_save_t *save) {
    return dc_color_hash(UINT64_C(14695981039346656037), save, offsetof(player_inventory_save_t, checksum));
}

static bool inventory_path(const dc_world_save_t *world, bool staging, char *path, size_t size) {
    const char *directory = dc_world_save_directory(world, staging);
    if (!directory) return false;
    int length = snprintf(path, size, "%s/player_inventory.bin", directory);
    return length > 0 && (size_t)length < size;
}

bool dc_player_inventory_load(const dc_world_save_t *world, fr_player_inventory_t *balances,
                              char *err, uint32_t cap) {
    char path[1024];
    if (!balances || !inventory_path(world, false, path, sizeof(path)))
        return inventory_error(err, cap, "Invalid player inventory destination");
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        if (errno != ENOENT) return inventory_error(err, cap, "Cannot open player inventory");
        memset(balances, 0, FR_INPUT_PLAYERS * sizeof(*balances));
        return true;
    }
    struct stat status;
    FILE *file = fstat(fd, &status) == 0 && S_ISREG(status.st_mode) ? fdopen(fd, "rb") : NULL;
    if (!file) {
        close(fd);
        return inventory_error(err, cap, "Cannot read player inventory record");
    }
    player_inventory_save_t save;
    bool okay = fread(&save, sizeof(save), 1, file) == 1 && fgetc(file) == EOF && !ferror(file);
    okay = fclose(file) == 0 && okay;
    if (!okay || memcmp(save.magic, "DPI1", 4) != 0 || save.version != 1u ||
        save.checksum != inventory_checksum(&save))
        return inventory_error(err, cap, "Invalid player inventory snapshot");
    for (uint32_t n = 0; n < FR_INPUT_PLAYERS; ++n) {
        const player_inventory_record_t *record = save.players + n;
        if (record->owner != DC_PLAYER_TRANSACTION_BASE + n + 1u || record->inventory.mass_fp[0] != 0u)
            return inventory_error(err, cap, "Invalid saved player inventory owner");
        for (size_t item = DC_ITEM_STONE + 1u; item < sizeof(record->inventory.mass_fp) / sizeof(uint64_t); ++item)
            if (record->inventory.mass_fp[item] != 0u)
                return inventory_error(err, cap, "Unknown saved inventory item");
    }
    for (uint32_t n = 0; n < FR_INPUT_PLAYERS; ++n) balances[n] = save.players[n].inventory;
    return true;
}

bool dc_player_inventory_stage(const dc_world_save_t *world, const fr_player_inventory_t *balances,
                               char *err, uint32_t cap) {
    char path[1024];
    if (!balances || !inventory_path(world, true, path, sizeof(path)))
        return inventory_error(err, cap, "Invalid player inventory staging path");
    player_inventory_save_t save = {.magic = {'D','P','I','1'}, .version = 1u};
    for (uint32_t n = 0; n < FR_INPUT_PLAYERS; ++n) {
        save.players[n].owner = DC_PLAYER_TRANSACTION_BASE + n + 1u;
        save.players[n].inventory = balances[n];
    }
    save.checksum = inventory_checksum(&save);
    FILE *file = fopen(path, "wb");
    if (!file) return inventory_error(err, cap, "Cannot open staged player inventory");
    bool okay = fwrite(&save, sizeof(save), 1, file) == 1;
    return (fclose(file) == 0 && okay) || inventory_error(err, cap, "Cannot write staged player inventory");
}
