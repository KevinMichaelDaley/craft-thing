#ifndef FR_REACTIVE_ADDRESS_GLSL
#define FR_REACTIVE_ADDRESS_GLSL
#include "../../include/dungeoncraft/reactive_arena_layout.h"
#include "../../include/dungeoncraft/reactive_state_layout.h"
/* Handles retain all 32 index bits; multiplying a word index in 32 bits would
 * alias entries above 16 GiB even though their BDA allocation is valid. */
uvec2 reactive_entry_address(uvec2 base,uint index) {
    uint high,low,carry;umulExtended(index,FR_ENTRY_WORDS*4u,high,low);
    low=uaddCarry(base.x,low,carry);return uvec2(low,base.y+high+carry);
}
uvec2 reactive_request_address(uvec2 base,uint index) {
    uint high,low,carry;umulExtended(index,FR_ARENA_REQUEST_WORDS*4u,high,low);
    low=uaddCarry(base.x,low,carry);return uvec2(low,base.y+high+carry);
}
#endif
