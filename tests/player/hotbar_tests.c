#include <stdio.h>
#include <string.h>

#include "ferrum/player/hotbar.h"

static int g_pass, g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while (0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define PASS() g_pass++

static void test_empty_hotbar_selects_nothing(void) {
    fr_player_hotbar_t bar;
    fr_player_hotbar_init(&bar);
    ASSERT_EQ(bar.selected, 0u);
    ASSERT_EQ(bar.slots[0].kind, FR_SLOT_EMPTY);
    ASSERT_TRUE(!fr_player_hotbar_selected_item(&bar, &bar.slots[0]) ||
                bar.slots[0].kind == FR_SLOT_EMPTY);
    PASS();
}
static void test_number_keys_select_slots(void) {
    fr_player_hotbar_t bar;
    fr_player_hotbar_init(&bar);
    ASSERT_TRUE(fr_player_hotbar_select(&bar, 1u));
    ASSERT_EQ(bar.selected, 0u);
    ASSERT_TRUE(fr_player_hotbar_select(&bar, 5u));
    ASSERT_EQ(bar.selected, 4u);
    /* Out-of-range numbers are ignored, selection unchanged. */
    ASSERT_TRUE(!fr_player_hotbar_select(&bar, 0u));
    ASSERT_TRUE(!fr_player_hotbar_select(&bar, FR_PLAYER_HOTBAR_SLOTS + 1u));
    ASSERT_EQ(bar.selected, 4u);
    PASS();
}
static void test_spell_slots_arm_casting(void) {
    fr_player_hotbar_t bar;
    fr_player_hotbar_init(&bar);
    ASSERT_TRUE(fr_player_hotbar_put_spell(&bar, 2u, 7u));
    ASSERT_TRUE(fr_player_hotbar_select(&bar, 2u));
    fr_player_hotbar_item_t item;
    ASSERT_TRUE(fr_player_hotbar_selected_item(&bar, &item));
    ASSERT_EQ(item.kind, FR_SLOT_SPELL);
    ASSERT_EQ(item.ability_id, 7u);
    PASS();
}
static void test_material_slots_place_blocks(void) {
    fr_player_hotbar_t bar;
    fr_player_hotbar_init(&bar);
    ASSERT_TRUE(fr_player_hotbar_put_material(&bar, 3u, 2u));
    ASSERT_TRUE(fr_player_hotbar_select(&bar, 3u));
    fr_player_hotbar_item_t item;
    ASSERT_TRUE(fr_player_hotbar_selected_item(&bar, &item));
    ASSERT_EQ(item.kind, FR_SLOT_MATERIAL);
    ASSERT_EQ(item.material, 2u);
    PASS();
}
static void test_selected_item_tracks_the_selection(void) {
    fr_player_hotbar_t bar;
    fr_player_hotbar_init(&bar);
    ASSERT_TRUE(fr_player_hotbar_put_spell(&bar, 1u, 3u));
    ASSERT_TRUE(fr_player_hotbar_put_material(&bar, 4u, 6u));
    ASSERT_TRUE(fr_player_hotbar_select(&bar, 1u));
    fr_player_hotbar_item_t item;
    ASSERT_TRUE(fr_player_hotbar_selected_item(&bar, &item));
    ASSERT_EQ(item.ability_id, 3u);
    ASSERT_TRUE(fr_player_hotbar_select(&bar, 4u));
    ASSERT_TRUE(fr_player_hotbar_selected_item(&bar, &item));
    ASSERT_EQ(item.material, 6u);
    PASS();
}
static void test_invalid_slots_are_rejected(void) {
    fr_player_hotbar_t bar;
    fr_player_hotbar_init(&bar);
    ASSERT_TRUE(!fr_player_hotbar_put_spell(&bar, 0u, 7u));
    ASSERT_TRUE(!fr_player_hotbar_put_spell(&bar, FR_PLAYER_HOTBAR_SLOTS + 1u,
                                            7u));
    ASSERT_TRUE(!fr_player_hotbar_put_material(&bar, 0u, 2u));
    ASSERT_TRUE(!fr_player_hotbar_put_material(&bar,
                                               FR_PLAYER_HOTBAR_SLOTS + 1u,
                                               2u));
    PASS();
}
static void test_clear_slot_returns_to_empty(void) {
    fr_player_hotbar_t bar;
    fr_player_hotbar_init(&bar);
    ASSERT_TRUE(fr_player_hotbar_put_spell(&bar, 2u, 7u));
    ASSERT_TRUE(fr_player_hotbar_clear(&bar, 2u));
    ASSERT_EQ(bar.slots[1].kind, FR_SLOT_EMPTY);
    ASSERT_TRUE(!fr_player_hotbar_selected_item(&bar, &bar.slots[1]) ||
                bar.slots[1].kind == FR_SLOT_EMPTY);
    PASS();
}
int main(void) {
    printf("=== Player Hotbar Tests ===\n\n");
    RUN(test_empty_hotbar_selects_nothing);
    RUN(test_number_keys_select_slots);
    RUN(test_spell_slots_arm_casting);
    RUN(test_material_slots_place_blocks);
    RUN(test_selected_item_tracks_the_selection);
    RUN(test_invalid_slots_are_rejected);
    RUN(test_clear_slot_returns_to_empty);
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
