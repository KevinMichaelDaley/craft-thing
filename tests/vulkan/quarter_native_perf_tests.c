#define _POSIX_C_SOURCE 200809L
#include <stdio.h>

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_dense_quarter_native_physics_meets_sixty_hz_budget(void) {
    FILE *run = popen("./build/quarter_native_bench 640 448 60", "r");
    ASSERT_TRUE(run != NULL);
    char line[512];
    double tick_ms = 0.0;
    while (fgets(line, sizeof(line), run)) {
        fputs(line, stdout);
        double value;
        if (sscanf(line, "tick_wall_ms=%lf", &value) == 1) tick_ms = value;
    }
    int status = pclose(run);
    ASSERT_INT_EQ(status, 0);
    ASSERT_TRUE(tick_ms > 0.0 && tick_ms <= 1000.0 / 60.0);
    PASS();
}

int main(void) {
    RUN(test_dense_quarter_native_physics_meets_sixty_hz_budget);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
