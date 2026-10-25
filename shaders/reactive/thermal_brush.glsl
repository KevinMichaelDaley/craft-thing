#ifndef FR_THERMAL_BRUSH_GLSL
#define FR_THERMAL_BRUSH_GLSL
#include "../../include/dungeoncraft/reactive_profile_layout.h"
#include "../../include/dungeoncraft/thermal_parameters.h"
#ifdef DC_RUNTIME_BODY_STORAGE
layout(buffer_reference,std430,buffer_reference_align=4) readonly restrict buffer ThermalProfiles {uint words[];};
ReactiveWords thermal_mailbox() {return ReactiveWords(uvec2(FR_GPU_ROOT.words[20],FR_GPU_ROOT.words[21]));}
ReactiveWords thermal_free_bits() {return ReactiveWords(uvec2(FR_GPU_ROOT.words[18],FR_GPU_ROOT.words[19]));}
bool thermal_quota() {
    uint free=atomicAdd(thermal_mailbox().words[FR_ARENA_MAIL_FREE],0u);
    while(free!=0u) {
        uint previous=atomicCompSwap(thermal_mailbox().words[FR_ARENA_MAIL_FREE],free,free-1u);
        if(previous==free) return true;
        free=previous;
    }
    return false;
}
uint thermal_reserve_slot(uint hint) {
    uint count=(reactive_entry_capacity()-1u)/32u+1u,start=(hint*2654435761u)%count;
    for(uint probe=0u;probe<count;probe++) {
        uint word=(start+probe)%count,mask=atomicAdd(thermal_free_bits().words[word],0u);
        while(mask!=0u) {
            uint bit=uint(findLSB(mask)),prior=atomicCompSwap(thermal_free_bits().words[word],mask,mask&~(1u<<bit));
            if(prior==mask) return word*32u+bit;
            mask=prior;
        }
    }
    return 0xffffffffu;
}
float thermal_brush_coefficient(uint material,uint field) {
    ThermalProfiles profiles=ThermalProfiles(uvec2(thermal_control[DC_THERMAL_PROFILE_ADDRESS_WORD],thermal_control[DC_THERMAL_PROFILE_ADDRESS_WORD+1u]));
    return uintBitsToFloat(profiles.words[material*FR_PROFILE_WORDS+field]);
}
/* Reserve before changing the physical source. Resource preflight bounds the
 * whole brush; existing same-material parcels retain their exact histories. */
bool thermal_brush_begin(uint index,uint old_material,uint material,uvec2 point,out uvec2 prepared) {
    prepared=uvec2(0);
    if(!thermal_water_on()||thermal_control[DC_THERMAL_BRUSH_MATERIAL_WORD]!=material||
       (material!=1u&&material!=7u)||!reactive_enabled()) return true;
    uvec2 existing=reactive_handle_read(index);
    if(old_material==material&&reactive_handle_valid(existing)) return true;
    if(!thermal_quota()) return false;
    uint slot=thermal_reserve_slot(index);
    if(slot==0xffffffffu) {atomicAdd(thermal_mailbox().words[FR_ARENA_MAIL_FREE],1u);return false;}
    ReactiveWords entry=reactive_entry_buffer(slot);uint generation=max(1u,entry.words[FR_ENTRY_GENERATION]);
    for(uint i=0u;i<FR_ENTRY_WORDS;i++) entry.words[i]=0u;
    entry.words[FR_ENTRY_GENERATION]=generation;entry.words[FR_ENTRY_OWNER]=FR_ARENA_OWNER_RESERVED;
    uint relative=(point.y-thermal_control[DC_THERMAL_BRUSH_Y_WORD])*thermal_control[DC_THERMAL_BRUSH_WIDTH_WORD]+
        point.x-thermal_control[DC_THERMAL_BRUSH_X_WORD],carry;
    uint low=uaddCarry(thermal_control[DC_THERMAL_BRUSH_ID_WORD],relative,carry);
    uint high=thermal_control[DC_THERMAL_BRUSH_ID_WORD+1u]+carry;
    uint mass=uint(round(thermal_brush_coefficient(material,FR_PROFILE_DENSITY_WORD)*65536.));
    uint fuel=min(mass,uint(round(float(mass)*thermal_brush_coefficient(material,9u))));
    uint oxidizer=min(mass-fuel,uint(round(float(mass)*thermal_brush_coefficient(material,13u))));
    uint at=FR_ENTRY_STATE;
    entry.words[at]=material;entry.words[at+2u]=low;entry.words[at+3u]=high;
    entry.words[at+4u]=low;entry.words[at+5u]=high;entry.words[at+8u]=mass;
    entry.words[at+9u]=fuel;entry.words[at+11u]=mass-fuel-oxidizer;entry.words[at+15u]=oxidizer;
    entry.words[at+17u]=floatBitsToUint(float(mass)/65536.*thermal_brush_coefficient(material,FR_PROFILE_HEAT_CAPACITY_WORD)*float(FR_THERMAL_AMBIENT_K));
    prepared=uvec2(slot,generation);return true;
}
void thermal_brush_publish(uint index,uint slot,uint local,uvec2 prepared) {
    if(prepared.y==0u) return;
    uint registry=structure_base()+32u+slot*8u;
    reactive_owner_write(prepared.x,1u,uvec4(DC_BUFFER_DATA(registry),DC_BUFFER_DATA(registry+1u),
        DC_BUFFER_DATA(registry+2u),DC_BUFFER_DATA(registry+3u)),local);
    memoryBarrierBuffer();reactive_handle_write(index,prepared);
    atomicAdd(thermal_mailbox().words[FR_ARENA_MAIL_LIVE],1u);
}
#else
bool thermal_brush_begin(uint index,uint old_material,uint material,uvec2 point,out uvec2 prepared) {prepared=uvec2(0);return true;}
void thermal_brush_publish(uint index,uint slot,uint local,uvec2 prepared) {}
#endif
#endif
