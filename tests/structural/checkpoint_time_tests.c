#include <math.h>
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
    ASSERT_TRUE(dc_checkpoint_channels_decode_times(&channels, 2u));
    ASSERT_TRUE(channels.chunk_times[0].known && channels.chunk_times[0].seconds == 0.0);
    ASSERT_TRUE(channels.chunk_times[1].known && channels.chunk_times[1].seconds == 1.5);
    free(channels.time_bytes);
    free(channels.chunk_times);
    PASS();
}

static void test_round_trips_and_hashes_a_mixed_clock_table(void) {
    const dc_chunk_time_t times[] = {
        {.seconds = 17.25, .known = true}, {.known = false}, {.known = true}};
    dc_checkpoint_channels_t original = {.version = 9}, loaded = {0};
    ASSERT_TRUE(dc_checkpoint_channels_set_times(&original, times, 3u));
    ASSERT_EQ(original.version, 10u);
    FILE *file = tmpfile();
    ASSERT_TRUE(file && dc_checkpoint_channels_write(file, &original));
    rewind(file);
    ASSERT_TRUE(dc_checkpoint_channels_read(file, original.version, &loaded));
    fclose(file);
    const uint64_t starting_hash = UINT64_C(14695981039346656037);
    ASSERT_EQ(dc_checkpoint_channels_hash(starting_hash, &original),
              dc_checkpoint_channels_hash(starting_hash, &loaded));
    loaded.time_bytes[0] ^= 1;
    ASSERT_TRUE(dc_checkpoint_channels_hash(starting_hash, &original) !=
                dc_checkpoint_channels_hash(starting_hash, &loaded));
    loaded.time_bytes[0] ^= 1;
    ASSERT_TRUE(!dc_checkpoint_channels_decode_times(&loaded, 2u));
    ASSERT_TRUE(dc_checkpoint_channels_decode_times(&loaded, 3u));
    ASSERT_TRUE(loaded.chunk_times[0].known && loaded.chunk_times[0].seconds == 17.25);
    ASSERT_TRUE(!loaded.chunk_times[1].known);
    ASSERT_TRUE(loaded.chunk_times[2].known && loaded.chunk_times[2].seconds == 0.0);
    free(original.time_bytes);
    free(loaded.time_bytes);
    free(loaded.chunk_times);
    PASS();
}

static void test_old_layouts_stay_untimed_and_invalid_clocks_are_rejected(void) {
    const dc_chunk_time_t bad[] = {{.seconds = NAN, .known = true},
        {.seconds = INFINITY, .known = true}, {.seconds = -1.0, .known = true}};
    dc_checkpoint_channels_t channels = {.version = 8};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
        ASSERT_TRUE(!dc_checkpoint_channels_set_times(&channels, &bad[i], 1u));
    ASSERT_EQ(channels.version, 8u);
    ASSERT_TRUE(!channels.time_bytes);

    for (uint32_t version = 8u; version <= 9u; ++version) {
        dc_checkpoint_channels_t old = {.version = version}, loaded = {0};
        const dc_chunk_time_t absent = {0};
        ASSERT_TRUE(dc_checkpoint_channels_set_times(&old, &absent, 1u));
        FILE *file = tmpfile();
        ASSERT_TRUE(file && dc_checkpoint_channels_write(file, &old));
        ASSERT_INT_EQ(ftell(file), version == 8u ? 16 : 24);
        rewind(file);
        ASSERT_TRUE(dc_checkpoint_channels_read(file, version, &loaded));
        fclose(file);
        ASSERT_TRUE(dc_checkpoint_channels_decode_times(&loaded, 1u));
        ASSERT_TRUE(!loaded.chunk_times);
    }
    uint8_t negative_infinity[8] = {[6] = 0xf0, [7] = 0xff};
    dc_checkpoint_channels_t corrupt = {.version = 10u, .time_bytes = negative_infinity,
                                        .time_length = sizeof(negative_infinity)};
    ASSERT_TRUE(!dc_checkpoint_channels_decode_times(&corrupt, 1u));
    ASSERT_TRUE(!corrupt.chunk_times);
    PASS();
}

int main(void) {
    RUN(test_reads_timed_checkpoint_channel);
    RUN(test_round_trips_and_hashes_a_mixed_clock_table);
    RUN(test_old_layouts_stay_untimed_and_invalid_clocks_are_rejected);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
