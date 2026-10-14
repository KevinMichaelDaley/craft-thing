#include <stdio.h>
#include <string.h>

#include "ferrum/assets/prop.h"

static int g_pass, g_fail;
static unsigned char original[152];
#define RUN(fn) do { printf("RUN %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(e) do { if (!(e)) { printf("FAIL line%d: %s\n", __LINE__, #e); g_fail++; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++

static void word(unsigned char *p, unsigned value) {
    for (unsigned i = 0; i < 4; i++) p[i] = (unsigned char)(value >> (i * 8));
}
static void checksum(unsigned char *p) {
    unsigned crc = ~0u;
    for (unsigned i = 0; i < sizeof(original); i++) {
        if (i >= 44 && i < 48) continue;
        crc ^= p[i];
        for (unsigned b = 0; b < 8; b++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    word(p + 44, ~crc);
}
static void test_valid_canonical_package(void) {
    fr_prop_asset_t view = {0};
    ASSERT_TRUE(fr_prop_asset_open(&view, original, sizeof(original), NULL, 0));
    ASSERT_INT_EQ(view.width, 1);
    PASS();
}
static void test_reserved_positive_word_rejected_atomically(void) {
    unsigned char bad[sizeof(original)];
    memcpy(bad, original, sizeof(bad));
    word(bad + 88 + 44, 0x3f800000u); checksum(bad);
    fr_prop_asset_t view, before;
    ASSERT_TRUE(fr_prop_asset_open(&view, original, sizeof(original), NULL, 0));
    before = view;
    ASSERT_TRUE(!fr_prop_asset_open(&view, bad, sizeof(bad), NULL, 0));
    ASSERT_EQ(memcmp(&before, &view, sizeof(view)), 0);
    PASS();
}
static void test_reserved_negative_zero_rejected_atomically(void) {
    unsigned char bad[sizeof(original)];
    memcpy(bad, original, sizeof(bad));
    word(bad + 88 + 52, 0x80000000u); checksum(bad);
    fr_prop_asset_t view, before;
    ASSERT_TRUE(fr_prop_asset_open(&view, original, sizeof(original), NULL, 0));
    before = view;
    ASSERT_TRUE(!fr_prop_asset_open(&view, bad, sizeof(bad), NULL, 0));
    ASSERT_EQ(memcmp(&before, &view, sizeof(view)), 0);
    PASS();
}
int main(void) {
    memcpy(original, "CPRP", 4);
    word(original + 4, 1); word(original + 8, sizeof(original));
    word(original + 12, 1); word(original + 16, 1); word(original + 20, 1);
    word(original + 28, 64); word(original + 32, 80);
    word(original + 36, 8); word(original + 40, 64); original[48] = 1;
    word(original + 64, 100); word(original + 72, 8); word(original + 76, 64);
    original[82] = 4;
    word(original + 88 + 24, 0x3f800000u);
    word(original + 88 + 48, 0x3f800000u);
    checksum(original);
    RUN(test_valid_canonical_package);
    RUN(test_reserved_positive_word_rejected_atomically);
    RUN(test_reserved_negative_zero_rejected_atomically);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
