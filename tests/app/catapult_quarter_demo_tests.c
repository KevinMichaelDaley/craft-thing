#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;

#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    g_fail++; return; } } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_quarter_native_full_lit_playback(void) {
    FILE *player = popen("DISPLAY=:1 SDL_VIDEODRIVER=x11 "
                         "./build/catapult_demo_quarter_native 2>&1", "r");
    ASSERT_TRUE(player != NULL);
    char line[512];
    int ready = 0, complete = 0, impact = 0;
    while (fgets(line, sizeof(line), player)) {
        fputs(line, stdout);
        if (strstr(line, "480x270 game view ready")) ready = 1;
        if (strstr(line, "playback complete")) complete = 1;
        if (strstr(line, "wall cleared 1, battlement ignited 1")) impact = 1;
    }
    int result = pclose(player);
    ASSERT_INT_EQ(result, 0);
    ASSERT_TRUE(ready && complete && impact);
    PASS();
}

int main(void) {
    printf("=== Quarter-native catapult demo tests ===\n\n");
    RUN(test_quarter_native_full_lit_playback);
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
