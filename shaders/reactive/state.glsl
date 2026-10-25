#ifndef FR_REACTIVE_STATE_GLSL
#define FR_REACTIVE_STATE_GLSL
#include "../../include/dungeoncraft/reactive_state_layout.h"

/* The caller supplies storage accessors. A zero generation is the only null
 * handle; full-width index/generation avoid a packed global parcel limit. */
bool reactive_handle_valid(uvec2 handle) {
    return handle.y!=0u&&handle.x<reactive_entry_capacity()&&
        reactive_entry_read(handle.x,FR_ENTRY_GENERATION)==handle.y&&
        reactive_entry_read(handle.x,FR_ENTRY_OWNER)!=0u;
}
/* Mechanical passes own disjoint destination locations. Their publication
 * barrier precedes any consumer of the new handles; no CPU parcel list exists. */
bool reactive_move(uint source,uint destination,uint kind,uvec4 key,uint last) {
    uvec2 handle=reactive_handle_read(source);
    if(source==destination||!reactive_handle_valid(handle)||
        reactive_handle_valid(reactive_handle_read(destination))||kind<1u||kind>3u)
        return false;
    if(kind==1u&&last>=4096u) return false;
    if(kind==2u&&(key.x==0u||key.y==0u||key.z>0x7fffffffu||key.w>0x7fffffffu||last!=0u)) return false;
    if(kind==3u&&((key.x|key.y)==0u||key.w!=0u||last!=0u)) return false;
    reactive_entry_write(handle.x,FR_ENTRY_OWNER,kind);
    reactive_entry_write(handle.x,FR_ENTRY_OWNER+1u,key.x);
    reactive_entry_write(handle.x,FR_ENTRY_OWNER+2u,key.y);
    reactive_entry_write(handle.x,FR_ENTRY_OWNER+3u,key.z);
    reactive_entry_write(handle.x,FR_ENTRY_OWNER+4u,key.w);
    reactive_entry_write(handle.x,FR_ENTRY_OWNER+5u,last);
    reactive_handle_write(destination,handle);
    reactive_handle_write(source,uvec2(0));
    return true;
}
bool reactive_erase(uint location) {
    uvec2 handle=reactive_handle_read(location);
    if(!reactive_handle_valid(handle)) return false;
    reactive_entry_write(handle.x,FR_ENTRY_OWNER,0u);
    /* Saturated generations are retired permanently by the allocator. */
    reactive_entry_write(handle.x,FR_ENTRY_GENERATION,min(handle.y,0xfffffffeu)+1u);
    reactive_handle_write(location,uvec2(0));
    return true;
}

#endif
