#ifndef FR_REACTIVE_WORLD_BODY_GLSL
#define FR_REACTIVE_WORLD_BODY_GLSL
#define DC_RUNTIME_BODY_STORAGE 1
#define DC_RUNTIME_ROOT_READONLY 1
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
bool fr_body_sample(uint index,out vec4 node_value,out vec4 metadata) {
    uint id=thermal_occupancy[index],slot=runtime_body_find(id);node_value=vec4(0);metadata=vec4(0);
    if(slot>=BODY_CAPACITY) return false;
    Body body=body_thermal_grid(DC_BODY(slot));if(!pixel_body(body)) return false;
    ivec2 cell=ivec2(index%params.width,index/params.width);
    vec2 low=vec2(cell)-vec2(body.x_fp,body.y_fp)/65536.;uvec4 query=body_piece_query(body,low,low+1.);
    float total=0.,dominant=0.;
    for(uint i=0u;i<query.z*query.w;i++) {
        uint piece=query_piece(body,query,i);if(pixel_piece_material(body,piece)==0u) continue;
        vec2 center;float coverage=body_cell_volume(body,piece,cell,1.,center);if(coverage<=0.) continue;
        uvec2 handle=fr_body_reactive_handle(body,slot,piece);if(handle.y==0u) continue;
        Words e=Words(reactive_entry_address(uvec2(root[4],root[5]),handle.x));uint s=FR_ENTRY_STATE,m=e.words[s];
        float capacity=((float(e.words[s+9u])+float(e.words[s+11u])+float(e.words[s+15u]))*coefficient(m,5u)+
            float(e.words[s+10u])*coefficient(12u,5u)+float(e.words[s+12u])*coefficient(3u,5u)+
            float(e.words[s+13u])*coefficient(14u,5u)+(float(e.words[s+14u])+float(e.words[s+16u]))*coefficient(15u,5u))/65536.;
        float energy=uintBitsToFloat(e.words[s+17u])+uintBitsToFloat(e.words[s+25u])+uintBitsToFloat(e.words[s+26u]);
        float wet_capacity=float(e.words[s+8u])*coefficient(m,19u),wet=wet_capacity>0.?clamp(float(e.words[s+12u])/wet_capacity,0.,1.):0.;
        node_value+=vec4(energy,capacity,coefficient(m,6u),wet)*coverage;total+=coverage;
        if(coverage>dominant) {dominant=coverage;metadata=vec4(uintBitsToFloat(4u),uintBitsToFloat(handle.x),uintBitsToFloat(m),uintBitsToFloat(handle.y));}
    }
    if(total>0.) node_value.zw/=total;
    return node_value.y>0.;
}
void fr_body_atomic_heat(Words scratch,uint word,float heat) {
    if(heat==0.) return;
    uint old=atomicAdd(scratch.words[word],0u);
    for(;;) {uint next=floatBitsToUint(uintBitsToFloat(old)+heat);
        uint observed=atomicCompSwap(scratch.words[word],old,next);if(observed==old) break;old=observed;}
}
float fr_body_heat_weight(uvec2 handle,float coverage,float heat) {
    Words entry=Words(reactive_entry_address(uvec2(root[4],root[5]),handle.x));uint state=FR_ENTRY_STATE;
    float energy=uintBitsToFloat(entry.words[state+17u])+uintBitsToFloat(entry.words[state+25u])+uintBitsToFloat(entry.words[state+26u]);
    Words scratch=Words(uvec2(root[FR_ARENA_THERMAL_SCRATCH],root[FR_ARENA_THERMAL_SCRATCH+1u]));
    float capacity=uintBitsToFloat(scratch.words[handle.x*12u+1u]);
    return fr_thermal_heat_weight(coverage,capacity,energy,heat);
}
void fr_body_deposit_radiance(uint index,float heat,float outgoing,float incoming) {
    if(heat==0.&&outgoing==0.&&incoming==0.) return;
    uint slot=runtime_body_find(thermal_occupancy[index]);if(slot>=BODY_CAPACITY) return;
    Body body=body_thermal_grid(DC_BODY(slot));if(!pixel_body(body)) return;
    ivec2 cell=ivec2(index%params.width,index/params.width);
    vec2 low=vec2(cell)-vec2(body.x_fp,body.y_fp)/65536.;uvec4 query=body_piece_query(body,low,low+1.);
    float total=0.,heat_total=0.;
    for(uint i=0u;i<query.z*query.w;i++) {
        uint piece=query_piece(body,query,i);if(pixel_piece_material(body,piece)==0u) continue;
        if(fr_body_reactive_handle(body,slot,piece).y==0u) continue;
        vec2 center;float coverage=body_cell_volume(body,piece,cell,1.,center);total+=coverage;
        heat_total+=fr_body_heat_weight(fr_body_reactive_handle(body,slot,piece),coverage,heat);
    }
    if(total<=0.) return;
    Words scratch=Words(uvec2(root[FR_ARENA_THERMAL_SCRATCH],root[FR_ARENA_THERMAL_SCRATCH+1u]));
    for(uint i=0u;i<query.z*query.w;i++) {
        uint piece=query_piece(body,query,i);if(pixel_piece_material(body,piece)==0u) continue;
        uvec2 h=fr_body_reactive_handle(body,slot,piece);if(h.y==0u) continue;
        vec2 center;float share=body_cell_volume(body,piece,cell,1.,center)/total;if(share<=0.) continue;
        float weight=fr_body_heat_weight(h,share*total,heat);
        if(heat_total>0.) fr_body_atomic_heat(scratch,h.x*12u+4u,heat*weight/heat_total);
        fr_body_atomic_heat(scratch,h.x*12u+10u,outgoing*share);
        fr_body_atomic_heat(scratch,h.x*12u+11u,incoming*share);
    }
}
void fr_body_deposit(uint index,float heat) {fr_body_deposit_radiance(index,heat,0.,0.);}
#endif
