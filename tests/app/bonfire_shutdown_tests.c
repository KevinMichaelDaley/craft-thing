#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

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
    ASSERT_TRUE(snprintf(command, sizeof(command),
        "bash -c 'source tools/smoke_profile; exec ./build/dungeoncraft_half_native --bonfire --world-dir %s --benchmark-frames %d' 2>&1",
        directory, budget ? 32 : 8) < (int)sizeof(command));
    FILE *process = popen(command, "r");
    ASSERT_TRUE(process != NULL);
    char line[1024];
    int saw_frame = 0, saw_cleanup_error = 0;
    double mean_ms = 0.0, worst_ms = 0.0;
    while (fgets(line, sizeof(line), process)) {
        if (strstr(line, "Displayed full frame 960x540")) {
            saw_frame = 1;
            char *time = strstr(line, ": mean ");
            if (time) (void)sscanf(time, ": mean %lfms worst %lfms", &mean_ms, &worst_ms);
        }
        if (strstr(line, "Dungeoncraft cleanup:")) saw_cleanup_error = 1;
    }
    int status = pclose(process);
    ASSERT_TRUE(status != -1 && WIFEXITED(status));
    ASSERT_INT_EQ(saw_frame, 1);
    ASSERT_INT_EQ(saw_cleanup_error, 0);
    if (budget) ASSERT_TRUE(mean_ms > 0.0 && mean_ms < 30.0 && worst_ms < 30.0);
    PASS();
}

int main(void) {
    printf("=== Bonfire shutdown tests ===\n\n");
    RUN(test_half_native_bonfire_flushes_queued_saves);
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
