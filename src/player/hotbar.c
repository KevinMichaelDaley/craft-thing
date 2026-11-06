#include "ferrum/player/hotbar.h"

void fr_player_hotbar_init(fr_player_hotbar_t *bar) {
    if (!bar) return;
    for (uint32_t i = 0u; i < FR_PLAYER_HOTBAR_SLOTS; i++)
        bar->slots[i] = (fr_player_hotbar_item_t){.kind = FR_SLOT_EMPTY};
    bar->selected = 0u;
}

bool fr_player_hotbar_select(fr_player_hotbar_t *bar, uint32_t number_key) {
    if (!bar || number_key < 1u || number_key > FR_PLAYER_HOTBAR_SLOTS)
        return false;
    bar->selected = number_key - 1u;
    return true;
}

bool fr_player_hotbar_put_spell(fr_player_hotbar_t *bar, uint32_t number_key,
                                uint32_t ability_id) {
    if (!bar || number_key < 1u || number_key > FR_PLAYER_HOTBAR_SLOTS)
        return false;
    bar->slots[number_key - 1u] =
        (fr_player_hotbar_item_t){.kind = FR_SLOT_SPELL,
                                  .ability_id = ability_id};
    return true;
}

bool fr_player_hotbar_put_material(fr_player_hotbar_t *bar,
                                   uint32_t number_key, uint32_t material) {
    if (!bar || number_key < 1u || number_key > FR_PLAYER_HOTBAR_SLOTS)
        return false;
    bar->slots[number_key - 1u] =
        (fr_player_hotbar_item_t){.kind = FR_SLOT_MATERIAL,
                                  .material = material};
    return true;
}

bool fr_player_hotbar_clear(fr_player_hotbar_t *bar, uint32_t number_key) {
    if (!bar || number_key < 1u || number_key > FR_PLAYER_HOTBAR_SLOTS)
        return false;
    bar->slots[number_key - 1u] = (fr_player_hotbar_item_t){
        .kind = FR_SLOT_EMPTY};
    return true;
}

bool fr_player_hotbar_selected_item(const fr_player_hotbar_t *bar,
                                    fr_player_hotbar_item_t *item) {
    if (!bar || !item || bar->selected >= FR_PLAYER_HOTBAR_SLOTS) return false;
    *item = bar->slots[bar->selected];
    return item->kind != FR_SLOT_EMPTY;
}
