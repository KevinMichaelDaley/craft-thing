#ifndef FR_REACTIVE_STATE_GLSL
#define FR_REACTIVE_STATE_GLSL
#include "../../include/dungeoncraft/reactive_state_layout.h"

/* The caller supplies storage accessors. A zero generation is the only null
 * handle; full-width index/generation avoid a packed global parcel limit. */
bool reactive_handle_valid(uvec2 handle) {
    return false;
}
bool reactive_move(uint source,uint destination,uint kind,uvec4 key,uint last) {
    return false;
}
bool reactive_erase(uint location) {return false;}

#endif
