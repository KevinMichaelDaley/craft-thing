#include <stdio.h>
#include <stdlib.h>
static int g_pass, g_fail;
#define RUN(fn)                                                                \
    do {                                                                       \
        printf("RUN %s\n", #fn);                                               \
        fn();                                                                  \
    } while (0)
#define ASSERT_TRUE(e)                                                         \
    do {                                                                       \
        if (!(e)) {                                                            \
            printf("FAIL %d: %s\n", __LINE__, #e);                             \
            g_fail++;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++
static void
test_native_placed_marked_and_brace_controls_drive_actual_vulkan_scene(void) {
    int result = system("timeout -k 5s 60s ./build/dungeoncraft_quarter_native "
                        "--smoke-structural-demo");
    ASSERT_INT_EQ(result, 0);
    PASS();
}
int main(void) {
    RUN(test_native_placed_marked_and_brace_controls_drive_actual_vulkan_scene);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
