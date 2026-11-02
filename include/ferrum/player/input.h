#ifndef FERRUM_PLAYER_INPUT_H
#define FERRUM_PLAYER_INPUT_H

#include "ferrum/player/player.h"

#define FR_INPUT_PLAYERS 4u
#define FR_INPUT_DEVICES 8u
#define FR_INPUT_BINDINGS 64u
#define FR_INPUT_ACTIONS 32u

typedef enum {
    FR_INPUT_MOVE, FR_INPUT_JUMP, FR_INPUT_CHEW, FR_INPUT_FIRE,
    FR_INPUT_ICE, FR_INPUT_AIM_X, FR_INPUT_AIM_Y
} fr_input_action_t;
typedef enum { FR_INPUT_GAMEPLAY, FR_INPUT_EDITOR, FR_INPUT_UI } fr_input_context_t;
typedef struct {
    float values[FR_INPUT_ACTIONS];
    uint32_t pressed, released;
    bool has_target;
    fr_player_point_t target;
} fr_input_state_t;
typedef struct {
    uint64_t identity;
    fr_input_context_t context;
    fr_input_state_t state;
} fr_input_player_t;
typedef struct { int64_t id; uint64_t owner; bool used; } fr_input_device_t;
typedef struct {
    int64_t device;
    uint32_t control, action;
    float scale, value;
    bool used,blocked;
} fr_input_binding_t;
typedef struct {
    fr_input_player_t players[FR_INPUT_PLAYERS];
    fr_input_device_t devices[FR_INPUT_DEVICES];
    fr_input_binding_t bindings[FR_INPUT_BINDINGS];
    bool focused;
} fr_input_t;

/** Initialize an empty router; action IDs 0..31 are caller-defined. */
void fr_input_init(fr_input_t *input);
/** Register one unique nonzero session identity. */
bool fr_input_add_player(fr_input_t *input, uint64_t identity);
/** Exclusively assign a stable device ID. Reassignment requires disconnect. */
bool fr_input_connect(fr_input_t *input, int64_t device, uint64_t identity);
/** Remove device and its bindings, clearing only its owner's contributions. */
bool fr_input_disconnect(fr_input_t *input, int64_t device);
/** Add or replace a device/control/action binding with finite scale [-1,1]. */
bool fr_input_bind(fr_input_t *input, int64_t device, uint32_t control,
    uint32_t action, float scale);
/** Submit normalized device input. Repeats never create edges. Unfocused or
 * non-gameplay input is consumed without accumulating latent held controls. */
bool fr_input_event(fr_input_t *input, int64_t device, uint32_t control,
    float value, bool repeat);
/** Losing focus clears all physical contributions and targets. */
void fr_input_focus(fr_input_t *input, bool focused);
/** Changing context clears held input; UI/editor actions never reach gameplay. */
bool fr_input_context(fr_input_t *input, uint64_t identity, fr_input_context_t context);
/** Publish a canonical target after the owning camera converts viewport input. */
bool fr_input_target(fr_input_t *input, uint64_t identity,
    bool valid, fr_player_point_t target);
/** Sample once per simulation tick, consuming edges but retaining held input. */
bool fr_input_sample(fr_input_t *input, uint64_t identity, fr_input_state_t *out);

typedef struct {
    uint32_t ability;
    bool has_target;
    fr_player_point_t target;
} fr_player_action_event_t;
typedef struct { fr_player_action_event_t cast; } fr_player_control_t;
/** Arbitrate press actions (fire wins ties), held chew and locomotion. Active
 * casts finish before another action; their ability/target latch at windup.
 * Emit only at frame six, independently of rendering. Caller validates wood
 * every tick and supplies explicitly registered test ability IDs. */
bool fr_player_control_tick(fr_player_control_t *control, fr_player_t *player,
    const fr_input_state_t *input, uint32_t fire, uint32_t ice, bool wood_valid,
    uint64_t elapsed_ms, fr_player_action_event_t *event);

#endif
