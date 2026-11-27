#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dungeoncraft/world_save.h"
#include "../../src/app/player_inventory_save.h"

static int g_pass, g_fail;
static char err[256];

#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %d: %s [%s]\n", __LINE__, #expr, err); ++g_fail; return; \
} } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() ++g_pass

static bool write_record(const char *directory, const char *name, const char *record) {
    char path[512];
    if (snprintf(path, sizeof(path), "%s/%s", directory, name) < 0) return false;
    FILE *file = fopen(path, "wb");
    if (!file) return false;
    bool okay = fputs(record, file) >= 0;
    return fclose(file) == 0 && okay;
}

static bool record_equals(const char *directory, const char *name, const char *expected) {
    char path[512], actual[64];
    if (snprintf(path, sizeof(path), "%s/%s", directory, name) < 0) return false;
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    size_t count = fread(actual, 1, sizeof(actual), file);
    bool okay = !ferror(file) && count == strlen(expected) && memcmp(actual, expected, count) == 0;
    return fclose(file) == 0 && okay;
}

static void test_commit_replaces_world_and_owned_material_together(void) {
    char directory[] = "build/world_commit_XXXXXX";
    const char chunk[] = "chunk_ffffffffffffffff_0000000000000002.bin";
    ASSERT_TRUE(mkdtemp(directory) && write_record(directory, chunk, "tree"));
    dc_world_save_t *save = dc_world_save_open(directory, err, sizeof(err));
    ASSERT_TRUE(save && dc_world_save_begin(save, err, sizeof(err)));
    const char *staging = dc_world_save_directory(save, true);
    ASSERT_TRUE(write_record(staging, chunk, "air"));
    ASSERT_TRUE(write_record(staging, "rigid_bodies.bin", "beavers"));
    ASSERT_TRUE(write_record(staging, "player_inventory.bin", "39322"));
    ASSERT_TRUE(record_equals(directory, chunk, "tree"));
    ASSERT_TRUE(dc_world_save_commit(save, err, sizeof(err)));
    dc_world_save_close(save);
    ASSERT_TRUE(record_equals(directory, chunk, "air"));
    ASSERT_TRUE(record_equals(directory, "rigid_bodies.bin", "beavers"));
    ASSERT_TRUE(record_equals(directory, "player_inventory.bin", "39322"));
    PASS();
}

static void test_failed_commit_keeps_previous_world_and_excludes_other_writers(void) {
    char directory[] = "build/world_discard_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory) && write_record(directory, "player_inventory.bin", "39322"));
    dc_world_save_t *save = dc_world_save_open(directory, err, sizeof(err));
    ASSERT_TRUE(save && dc_world_save_begin(save, err, sizeof(err)));
    ASSERT_TRUE(dc_world_save_open(directory, err, sizeof(err)) == NULL);
    const char *staging = dc_world_save_directory(save, true);
    ASSERT_TRUE(write_record(staging, "player_inventory.bin", "0"));
    ASSERT_TRUE(write_record(staging, "rigid_bodies.bin.tmp", "incomplete"));
    ASSERT_TRUE(!dc_world_save_commit(save, err, sizeof(err)));
    dc_world_save_close(save);
    ASSERT_TRUE(record_equals(directory, "player_inventory.bin", "39322"));
    save = dc_world_save_open(directory, err, sizeof(err));
    ASSERT_TRUE(save && dc_world_save_begin(save, err, sizeof(err)));
    dc_world_save_close(save);
    PASS();
}

static void test_open_recovers_a_committed_save_after_publication_stops(void) {
    char directory[] = "build/world_recover_XXXXXX", obstacle[128], scratch[128];
    ASSERT_TRUE(mkdtemp(directory));
    ASSERT_TRUE(snprintf(obstacle, sizeof(obstacle), "%s/rigid_bodies.bin", directory) > 0);
    ASSERT_TRUE(snprintf(scratch, sizeof(scratch), "%s/.level-save-publish", directory) > 0);
    ASSERT_INT_EQ(mkdir(obstacle, 0700), 0);
    dc_world_save_t *save = dc_world_save_open(directory, err, sizeof(err));
    ASSERT_TRUE(save && dc_world_save_begin(save, err, sizeof(err)));
    const char *staging = dc_world_save_directory(save, true);
    ASSERT_TRUE(write_record(staging, "player_inventory.bin", "39322"));
    ASSERT_TRUE(write_record(staging, "rigid_bodies.bin", "beavers"));
    ASSERT_TRUE(!dc_world_save_commit(save, err, sizeof(err)));
    dc_world_save_close(save);
    ASSERT_TRUE(dc_world_save_open(directory, err, sizeof(err)) == NULL);
    ASSERT_INT_EQ(rmdir(obstacle), 0);
    save = dc_world_save_open(directory, err, sizeof(err));
    ASSERT_TRUE(save);
    ASSERT_TRUE(record_equals(directory, "player_inventory.bin", "39322"));
    ASSERT_TRUE(record_equals(directory, "rigid_bodies.bin", "beavers"));
    ASSERT_TRUE(access(scratch, F_OK) != 0);
    dc_world_save_close(save);
    PASS();
}

static void test_legacy_and_invalid_player_records_preserve_expected_balances(void) {
    char directory[] = "build/world_owners_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    dc_world_save_t *save = dc_world_save_open(directory, err, sizeof(err));
    ASSERT_TRUE(save);
    fr_player_inventory_t balances[FR_INPUT_PLAYERS], loaded[FR_INPUT_PLAYERS];
    memset(balances, 0, sizeof(balances));
    memset(loaded, 0x5a, sizeof(loaded));
    ASSERT_TRUE(dc_player_inventory_load(save, loaded, err, sizeof(err)));
    ASSERT_EQ(memcmp(loaded, balances, sizeof(loaded)), 0);
    balances[0].mass_fp[DC_ITEM_WOOD] = 39322u;
    balances[1].mass_fp[DC_ITEM_STONE] = UINT64_MAX;
    ASSERT_TRUE(dc_world_save_begin(save, err, sizeof(err)));
    ASSERT_TRUE(write_record(dc_world_save_directory(save, true), "rigid_bodies.bin", "beavers"));
    ASSERT_TRUE(dc_player_inventory_stage(save, balances, err, sizeof(err)));
    ASSERT_TRUE(dc_world_save_commit(save, err, sizeof(err)));
    ASSERT_TRUE(dc_player_inventory_load(save, loaded, err, sizeof(err)));
    ASSERT_EQ(memcmp(loaded, balances, sizeof(loaded)), 0);

    ASSERT_TRUE(write_record(directory, "player_inventory.bin", "DPI1"));
    ASSERT_TRUE(!dc_player_inventory_load(save, loaded, err, sizeof(err)));
    ASSERT_EQ(memcmp(loaded, balances, sizeof(loaded)), 0);
    ASSERT_TRUE(record_equals(directory, "rigid_bodies.bin", "beavers"));
    dc_world_save_close(save);
    PASS();
}

static void test_committed_recovery_publishes_the_clock_with_physical_records(void) {
    char directory[] = "build/world_clock_recover_XXXXXX", obstacle[128];
    ASSERT_TRUE(mkdtemp(directory));
    ASSERT_TRUE(snprintf(obstacle, sizeof(obstacle), "%s/rigid_bodies.bin", directory) > 0);
    ASSERT_INT_EQ(mkdir(obstacle, 0700), 0);
    dc_world_save_t *save = dc_world_save_open(directory, err, sizeof(err));
    ASSERT_TRUE(save && dc_world_save_store_clock(save, 5.25, err, sizeof(err)));
    ASSERT_TRUE(dc_world_save_begin(save, err, sizeof(err)));
    const char *staging = dc_world_save_directory(save, true);
    ASSERT_TRUE(write_record(staging, "rigid_bodies.bin", "bodies"));
    ASSERT_TRUE(write_record(staging, "player_inventory.bin", "empty"));
    ASSERT_TRUE(dc_world_save_store_clock(save, 7.5, err, sizeof(err)));
    double seconds;
    ASSERT_TRUE(dc_world_save_load_clock(save, &seconds, err, sizeof(err)));
    ASSERT_EQ(seconds, 5.25);
    ASSERT_TRUE(!dc_world_save_commit(save, err, sizeof(err)));
    dc_world_save_close(save);
    ASSERT_INT_EQ(rmdir(obstacle), 0);
    save = dc_world_save_open(directory, err, sizeof(err));
    ASSERT_TRUE(save);
    ASSERT_TRUE(record_equals(directory, "rigid_bodies.bin", "bodies"));
    ASSERT_TRUE(record_equals(directory, "player_inventory.bin", "empty"));
    ASSERT_TRUE(dc_world_save_load_clock(save, &seconds, err, sizeof(err)));
    ASSERT_EQ(seconds, 7.5);
    dc_world_save_close(save);
    PASS();
}

static void test_world_clock_preserves_canonical_time_across_discard_and_reopen(void) {
    char directory[] = "build/world_clock_discard_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    dc_world_save_t *save = dc_world_save_open(directory, err, sizeof(err));
    double seconds = -1.;
    ASSERT_TRUE(save && dc_world_save_load_clock(save, &seconds, err, sizeof(err)));
    ASSERT_EQ(seconds, 0.);
    ASSERT_TRUE(dc_world_save_store_clock(save, 9.125, err, sizeof(err)));
    ASSERT_TRUE(dc_world_save_begin(save, err, sizeof(err)));
    ASSERT_TRUE(dc_world_save_store_clock(save, 9.125, err, sizeof(err)));
    char staged_path[512];
    ASSERT_TRUE(snprintf(staged_path, sizeof(staged_path), "%s/%s",
                         dc_world_save_directory(save, true), DC_WORLD_CLOCK_RECORD) > 0);
    ASSERT_INT_EQ(access(staged_path, F_OK), 0);
    ASSERT_TRUE(dc_world_save_store_clock(save, 10., err, sizeof(err)));
    dc_world_save_close(save);
    ASSERT_TRUE(access(staged_path, F_OK) != 0);

    save = dc_world_save_open(directory, err, sizeof(err));
    ASSERT_TRUE(save);
    ASSERT_TRUE(!dc_world_save_store_clock(save, NAN, err, sizeof(err)));
    ASSERT_TRUE(!dc_world_save_store_clock(save, INFINITY, err, sizeof(err)));
    ASSERT_TRUE(!dc_world_save_store_clock(save, -1., err, sizeof(err)));
    ASSERT_TRUE(dc_world_save_load_clock(save, &seconds, err, sizeof(err)));
    ASSERT_EQ(seconds, 9.125);
    ASSERT_TRUE(write_record(directory, DC_WORLD_CLOCK_RECORD, "DWC1"));
    ASSERT_TRUE(!dc_world_save_load_clock(save, &seconds, err, sizeof(err)));
    ASSERT_EQ(seconds, 9.125);
    dc_world_save_close(save);
    PASS();
}

static void test_aborted_draft_allows_restage_on_the_same_world_writer(void) {
    char directory[] = "build/world_restage_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    dc_world_save_t *save = dc_world_save_open(directory, err, sizeof(err));
    ASSERT_TRUE(save && dc_world_save_store_clock(save, 2., err, sizeof(err)));
    ASSERT_TRUE(dc_world_save_begin(save, err, sizeof(err)));
    ASSERT_TRUE(dc_world_save_store_clock(save, 3., err, sizeof(err)));
    ASSERT_TRUE(dc_world_save_abort(save));
    ASSERT_TRUE(!dc_world_save_directory(save, true));
    ASSERT_TRUE(dc_world_save_begin(save, err, sizeof(err)));
    ASSERT_TRUE(dc_world_save_store_clock(save, 4., err, sizeof(err)));
    double canonical;
    ASSERT_TRUE(dc_world_save_load_clock(save, &canonical, err, sizeof(err)));
    ASSERT_EQ(canonical, 2.);
    dc_world_save_close(save);
    PASS();
}

static void test_water_cycle_record_commits_and_discards_with_its_owner_clock(void) {
    char directory[] = "build/world_water_cycle_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    dc_world_save_t *save = dc_world_save_open(directory, err, sizeof(err));
    ASSERT_TRUE(save && dc_world_save_begin(save, err, sizeof(err)));
    const char *draft = dc_world_save_directory(save, true);
    ASSERT_TRUE(write_record(draft, "water_cycle.bin", "partial"));
    ASSERT_TRUE(write_record(draft, DC_WORLD_BODY_RECORD, "owners"));
    ASSERT_TRUE(write_record(draft, DC_WORLD_PLAYER_RECORD, "inventory"));
    ASSERT_TRUE(dc_world_save_store_clock(save, 2., err, sizeof(err)));
    ASSERT_TRUE(dc_world_save_commit(save, err, sizeof(err)));
    ASSERT_TRUE(dc_world_save_begin(save, err, sizeof(err)));
    ASSERT_TRUE(write_record(dc_world_save_directory(save, true), "water_cycle.bin", "next"));
    ASSERT_TRUE(dc_world_save_store_clock(save, 3., err, sizeof(err)));
    dc_world_save_close(save);
    save = dc_world_save_open(directory, err, sizeof(err));
    double seconds;
    ASSERT_TRUE(save && dc_world_save_load_clock(save, &seconds, err, sizeof(err)));
    ASSERT_EQ(seconds, 2.);
    ASSERT_TRUE(record_equals(directory, "water_cycle.bin", "partial"));
    dc_world_save_close(save);
    PASS();
}

int main(void) {
    RUN(test_water_cycle_record_commits_and_discards_with_its_owner_clock);
    RUN(test_commit_replaces_world_and_owned_material_together);
    RUN(test_failed_commit_keeps_previous_world_and_excludes_other_writers);
    RUN(test_open_recovers_a_committed_save_after_publication_stops);
    RUN(test_legacy_and_invalid_player_records_preserve_expected_balances);
    RUN(test_committed_recovery_publishes_the_clock_with_physical_records);
    RUN(test_world_clock_preserves_canonical_time_across_discard_and_reopen);
    RUN(test_aborted_draft_allows_restage_on_the_same_world_writer);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
