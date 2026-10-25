#ifndef FR_REACTIVE_ADDRESS_GLSL
#define FR_REACTIVE_ADDRESS_GLSL
/* Handles retain all 32 index bits; multiplying a word index in 32 bits would
 * alias entries above 16 GiB even though their BDA allocation is valid. */
uvec2 reactive_entry_address(uvec2 base,uint index) {
    uint high,low,carry;umulExtended(index,144u,high,low);
    low=uaddCarry(base.x,low,carry);return uvec2(low,base.y+high+carry);
}
uvec2 reactive_request_address(uvec2 base,uint index) {
    return uvec2(base.x+index*168u,base.y);
}
#endif
