#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glob.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int g_pass = 0;
static int g_fail = 0;

#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_half_native_bonfire_flushes_queued_saves(void) {
    char directory[] = "build/bonfire_shutdown_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    char command[1024];
    int budget = getenv("DC_ASSERT_HALF_NATIVE_FRAME_BUDGET") != NULL;
    int single_save = getenv("DC_ASSERT_SINGLE_STRUCTURE_SAVE") != NULL;
    int fast_colors = getenv("DC_ASSERT_FAST_COLOR_SAVE") != NULL;
    int cache_persist = getenv("DC_ASSERT_PIPELINE_CACHE_PERSIST") != NULL;
    int cache_home = getenv("DC_ASSERT_CACHE_OUTSIDE_BUILD") != NULL;
    char home_directory[] = "build/pipeline_cache_home_XXXXXX";
    if (cache_home) {
        ASSERT_TRUE(mkdtemp(home_directory) != NULL);
        char current_directory[512], absolute_home[sizeof(current_directory) + sizeof(home_directory)];
        ASSERT_TRUE(getcwd(current_directory, sizeof(current_directory)) != NULL);
        ASSERT_TRUE(snprintf(absolute_home, sizeof(absolute_home), "%s/%s",
                             current_directory, home_directory) < (int)sizeof(absolute_home));
        ASSERT_INT_EQ(setenv("XDG_CACHE_HOME", absolute_home, 1), 0);
    }
    struct stat cache_before = {0};
    char cache_path[512] = {0};
    if (cache_persist) {
        glob_t matches = {0};
        ASSERT_INT_EQ(glob("build/pipeline_cache_*.bin", 0, NULL, &matches), 0);
        ASSERT_TRUE(matches.gl_pathc == 1u);
        ASSERT_TRUE(snprintf(cache_path, sizeof(cache_path), "%s", matches.gl_pathv[0]) < (int)sizeof(cache_path));
        globfree(&matches);
        ASSERT_INT_EQ(stat(cache_path, &cache_before), 0);
    }
    ASSERT_TRUE(snprintf(command, sizeof(command),
        "bash -c 'source tools/smoke_profile; %sexec ./build/dungeoncraft_half_native --bonfire --world-dir %s --benchmark-frames %d' 2>&1",
        single_save || fast_colors ? "export DC_SAVE_TRACE=1; " : "", directory, budget ? 32 : 8) < (int)sizeof(command));
    FILE *process = popen(command, "r");
    ASSERT_TRUE(process != NULL);
    char line[1024];
    int saw_frame = 0, saw_cleanup_error = 0;
    int repeated_structure_saves = 0, late_loads = 0;
    unsigned long long color_save_ms = 0;
    double mean_ms = 0.0, worst_ms = 0.0;
    while (fgets(line, sizeof(line), process)) {
        if (strstr(line, "Displayed full frame 960x540")) {
            saw_frame = 1;
            char *time = strstr(line, ": mean ");
            if (time) (void)sscanf(time, ": mean %lfms worst %lfms", &mean_ms, &worst_ms);
        }
        if (strstr(line, "Dungeoncraft cleanup:")) saw_cleanup_error = 1;
        unsigned long long structure_ms = 0;
        if (sscanf(line, "Save schedule structure%llums", &structure_ms) == 1 && structure_ms > 0)
            repeated_structure_saves++;
        if (strstr(line, "Save late load slot")) late_loads++;
        char *color_stage = strstr(line, " colors");
        if (strstr(line, "Save chunk stages ") && color_stage)
            (void)sscanf(color_stage, " colors%llums", &color_save_ms);
    }
    int status = pclose(process);
    ASSERT_TRUE(status != -1 && WIFEXITED(status));
    ASSERT_INT_EQ(saw_frame, 1);
    ASSERT_INT_EQ(saw_cleanup_error, 0);
    if (single_save) ASSERT_TRUE(repeated_structure_saves <= late_loads);
    if (fast_colors) ASSERT_TRUE(color_save_ms > 0 && color_save_ms < 500);
    if (budget) ASSERT_TRUE(mean_ms > 0.0 && mean_ms < 30.0 && worst_ms < 30.0);
    if (cache_persist) {
        struct stat cache_after = {0};
        ASSERT_INT_EQ(stat(cache_path, &cache_after), 0);
        ASSERT_TRUE(cache_after.st_mtim.tv_sec > cache_before.st_mtim.tv_sec ||
            (cache_after.st_mtim.tv_sec == cache_before.st_mtim.tv_sec &&
             cache_after.st_mtim.tv_nsec > cache_before.st_mtim.tv_nsec));
    }
    if (cache_home) {
        char pattern[512];
        ASSERT_TRUE(snprintf(pattern, sizeof(pattern), "%s/dungeoncraft/pipeline_cache_*.bin",
                             home_directory) < (int)sizeof(pattern));
        glob_t matches = {0};
        ASSERT_INT_EQ(glob(pattern, 0, NULL, &matches), 0);
        ASSERT_TRUE(matches.gl_pathc == 1u);
        globfree(&matches);
    }
    PASS();
}

int main(void) {
    printf("=== Bonfire shutdown tests ===\n\n");
    RUN(test_half_native_bonfire_flushes_queued_saves);
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
