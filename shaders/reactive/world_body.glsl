#ifndef FR_REACTIVE_WORLD_BODY_GLSL
#define FR_REACTIVE_WORLD_BODY_GLSL
#define DC_RUNTIME_BODY_STORAGE 1
#define DC_RUNTIME_RESTRICT_BODY_STORAGE 1
#ifndef DC_RUNTIME_BODY_BINDING
#define DC_RUNTIME_BODY_BINDING 8
#endif
#ifndef FR_THERMAL_OCCUPANCY_BINDING
#define FR_THERMAL_OCCUPANCY_BINDING 9
#endif
#ifndef FR_BODY_POOL_WORD
#define FR_BODY_POOL_WORD params.pool_word
#endif
#include "../sim/rigid_body.glsl"
#include "../sim/rigid_shape.glsl"
#include "../sim/rigid_clip.glsl"
layout(set=0,binding=FR_THERMAL_OCCUPANCY_BINDING,std430) readonly buffer ThermalOccupancy {uint thermal_occupancy[];};
uint pixel_word_at(uint index) {return DC_PIXEL_WORD(index);}
uvec2 fr_body_reactive_handle(Body body,uint slot,uint piece) {
    uint word=body.compound_reserved+pixel_tile_offset(body,piece/256u)+1u+(piece%256u)/8u;
    if(word<FR_BODY_POOL_WORD) return uvec2(0);
    uint relative=word-FR_BODY_POOL_WORD,capacity=DC_RUNTIME_WORK(29u),bank=relative<capacity?8u:10u;
    if(capacity==0u||relative>=2u*capacity) return uvec2(0);
    uvec2 handle=Handles(uvec2(root[bank],root[bank+1u])).values[(relative%capacity)*8u+piece%8u];
    if(handle.y==0u||handle.x>=root[2]) return uvec2(0);
    Words entry=Words(reactive_entry_address(uvec2(root[4],root[5]),handle.x));ivec2 rest=pixel_piece_cell(body,piece);
    if(entry.words[FR_ENTRY_GENERATION]!=handle.y||entry.words[FR_ENTRY_OWNER]!=2u||
       entry.words[FR_ENTRY_OWNER+1u]!=body.id||entry.words[FR_ENTRY_OWNER+2u]!=DC_BODY_OWNER(runtime_body_root,slot).generation||
       entry.words[FR_ENTRY_OWNER+3u]!=uint(rest.x)||entry.words[FR_ENTRY_OWNER+4u]!=uint(rest.y)) return uvec2(0);
    return handle;
}
#endif
