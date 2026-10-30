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
    ASSERT_TRUE(snprintf(command, sizeof(command),
        "./build/dungeoncraft_half_native --bonfire --world-dir %s --benchmark-frames 8 2>&1",
        directory) < (int)sizeof(command));
    FILE *process = popen(command, "r");
    ASSERT_TRUE(process != NULL);
    char line[1024];
    int saw_frame = 0, saw_cleanup_error = 0;
    while (fgets(line, sizeof(line), process)) {
        if (strstr(line, "Displayed full frame 960x540")) saw_frame = 1;
        if (strstr(line, "Dungeoncraft cleanup:")) saw_cleanup_error = 1;
    }
    int status = pclose(process);
    ASSERT_TRUE(status != -1 && WIFEXITED(status));
    ASSERT_INT_EQ(saw_frame, 1);
    ASSERT_INT_EQ(saw_cleanup_error, 0);
    PASS();
}

int main(void) {
    printf("=== Bonfire shutdown tests ===\n\n");
    RUN(test_half_native_bonfire_flushes_queued_saves);
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
