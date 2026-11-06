#ifndef FERRUM_PLAYER_HOTBAR_H
#define FERRUM_PLAYER_HOTBAR_H

#include <stdbool.h>
#include <stdint.h>

enum { FR_PLAYER_HOTBAR_SLOTS = 8u };

typedef enum {
    FR_SLOT_EMPTY = 0u,
    FR_SLOT_SPELL,
    FR_SLOT_MATERIAL
} fr_slot_kind_t;

typedef struct {
    fr_slot_kind_t kind;
    union {
        uint32_t ability_id; /**< FR_SLOT_SPELL: registry ability to cast. */
        uint32_t material;   /**< FR_SLOT_MATERIAL: body material to place. */
    };
} fr_player_hotbar_item_t;

typedef struct {
    fr_player_hotbar_item_t slots[FR_PLAYER_HOTBAR_SLOTS];
    uint32_t selected; /**< Zero-based index of the active slot. */
} fr_player_hotbar_t;

void fr_player_hotbar_init(fr_player_hotbar_t *bar);

/** Select by number key 1..FR_PLAYER_HOTBAR_SLOTS; false when out of range. */
bool fr_player_hotbar_select(fr_player_hotbar_t *bar, uint32_t number_key);

/** Put a castable ability in a slot (1-based number key). */
bool fr_player_hotbar_put_spell(fr_player_hotbar_t *bar, uint32_t number_key,
                                uint32_t ability_id);

/** Put a placeable building material in a slot (1-based number key). */
bool fr_player_hotbar_put_material(fr_player_hotbar_t *bar,
                                   uint32_t number_key, uint32_t material);

/** Empty a slot (1-based number key). */
bool fr_player_hotbar_clear(fr_player_hotbar_t *bar, uint32_t number_key);

/** Copy the active slot's item; false when the slot is empty or out of range. */
bool fr_player_hotbar_selected_item(const fr_player_hotbar_t *bar,
                                    fr_player_hotbar_item_t *item);

#endif
