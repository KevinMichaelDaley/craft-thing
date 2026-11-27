#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void test_fluid_channel_can_record_empty_stock_without_a_clock(void) {
    const uint8_t packet[42] = {[32] = 2, [40] = 1};
    FILE *file = tmpfile();
    ASSERT_TRUE(file && fwrite(packet, sizeof(packet), 1, file) == 1);
    rewind(file);
    dc_checkpoint_channels_t channels = {0};
    ASSERT_TRUE(dc_checkpoint_channels_read(file, DC_STRUCTURE_FLUID_VERSION, &channels));
    fclose(file);
    ASSERT_TRUE(dc_checkpoint_channels_decode_times(&channels, 2));
    ASSERT_TRUE(!channels.chunk_times);
    dc_chunk_t *chunks = calloc(2, sizeof(*chunks));
    ASSERT_TRUE(chunks);
    dc_checkpoint_fluid_t *stock = NULL;
    ASSERT_TRUE(dc_checkpoint_fluid_decode(chunks, 2, channels.fluid_bytes, channels.fluid_length, &stock));
    ASSERT_TRUE(stock[0].known && !stock[0].gas && !stock[0].bulk);
    ASSERT_TRUE(!stock[1].known);
    dc_checkpoint_fluid_destroy(stock, 2);
    free(chunks);
    free(channels.fluid_bytes);
    PASS();
}

static void test_fluid_channel_rejects_truncation_and_bulk_bound_to_wrong_water(void) {
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    fr_bulk_phase_chunk_t *bulk = calloc(1, sizeof(*bulk));
    const size_t length = 1 + sizeof(*bulk);
    uint8_t *packet = malloc(length);
    ASSERT_TRUE(chunk && bulk && packet);
    chunk->cells[3].fluid_mass = 60000;
    bulk->oil_fp[3] = 6000;
    packet[0] = 5;
    memcpy(packet + 1, bulk, sizeof(*bulk));
    dc_checkpoint_fluid_t *stock = NULL;
    ASSERT_TRUE(!dc_checkpoint_fluid_decode(chunk, 1, packet, length - 1, &stock) && !stock);
    ASSERT_TRUE(!dc_checkpoint_fluid_decode(chunk, 1, packet, length, &stock) && !stock);
    chunk->cells[3].fluid_mass = 50000;
    ASSERT_TRUE(dc_checkpoint_fluid_decode(chunk, 1, packet, length, &stock));
    ASSERT_EQ(stock[0].bulk->oil_fp[3], 6000u);
    dc_checkpoint_fluid_destroy(stock, 1);
    free(packet);
    free(bulk);
    free(chunk);
    PASS();
}

static void test_structure_archive_carries_separate_durable_owners(void) {
    /* The clock is absent here. The owner table follows one empty fluid record. */
    const uint8_t packet[65] = {[32] = 1, [40] = 16, [48] = 1, [49] = 9, [57] = 3};
    FILE *file = tmpfile();
    ASSERT_TRUE(file && fwrite(packet, sizeof(packet), 1, file) == 1);
    rewind(file);
    dc_checkpoint_channels_t channels = {0};
    bool read = dc_checkpoint_channels_read(file, 12u, &channels);
    fclose(file);
    ASSERT_TRUE(read);
    ASSERT_TRUE(dc_checkpoint_channels_decode_times(&channels, 1));
    ASSERT_TRUE(!channels.chunk_times);
    free(channels.fluid_bytes);
    PASS();
}

int main(void) {
    RUN(test_reads_timed_checkpoint_channel);
    RUN(test_round_trips_and_hashes_a_mixed_clock_table);
    RUN(test_old_layouts_stay_untimed_and_invalid_clocks_are_rejected);
    RUN(test_fluid_channel_can_record_empty_stock_without_a_clock);
    RUN(test_fluid_channel_rejects_truncation_and_bulk_bound_to_wrong_water);
    RUN(test_structure_archive_carries_separate_durable_owners);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
