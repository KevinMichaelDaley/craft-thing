#include <stdio.h>
#include <stdlib.h>

#include "../../src/vulkan/structural_store_channels.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++g_fail; return; \
    } \
} while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() ++g_pass

static void test_reads_timed_checkpoint_channel(void) {
    /* Four lengths, then two little-endian clock records for 0 and 1.5 seconds. */
    const uint8_t packet[48] = {[24] = 16, [46] = 0xf8, [47] = 0x3f};
    FILE *file = tmpfile();
    ASSERT_TRUE(file);
    ASSERT_EQ(fwrite(packet, sizeof(packet), 1, file), 1u);
    rewind(file);
    dc_checkpoint_channels_t channels = {0};
    bool read = dc_checkpoint_channels_read(file, 10u, &channels);
    fclose(file);
    ASSERT_TRUE(read);
    ASSERT_INT_EQ(channels.version, 10u);
    PASS();
}

int main(void) {
    RUN(test_reads_timed_checkpoint_channel);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
