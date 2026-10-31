#include "storage.glsl"
#include "../../include/dungeoncraft/terrain_flags.h"
float geometry_void(uint index,uint atlas) {
    uint packed=cells[atlas].x,material=packed&65535u;
    bool solid=material!=0u&&material!=3u&&((packed&(DC_CELL_LOADED_TERRAIN<<DC_CELL_FLAGS_SHIFT))!=0u||
        material==1u||material==6u||material==7u||material==8u||material==9u||
        material==11u||material==12u);
    if(solid) return 0.;
    return max(0.,1.-float(min(65536u,occupancy[2u*params.width*params.height+index]))/65536.-
        clamp(mpm[index].w,0.,1.)-float(cells[atlas].y)/65536.);
}
vec4 geometry_metadata(uint index) {return gas_cache_candidate(index).hydro;}
Gas geometry_candidate(uint atlas) {return gas_cell_owners()?gas_provisional(atlas):bank1[atlas];}
void geometry_write_candidate(uint atlas,Gas value) {if(gas_cell_owners()) gas_write_provisional(atlas,value);else bank1[atlas]=value;}
vec4 geometry_metadata_at(ivec2 p) {
    if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,ivec2(params.width,params.height)))) return vec4(0);
    return geometry_metadata(uint(p.y)*params.width+uint(p.x));
}
float geometry_available(ivec2 p) {return geometry_metadata_at(p).x;}
ivec2 geometry_direction(uint d) {return d==0u?ivec2(1,0):(d==1u?ivec2(-1,0):(d==2u?ivec2(0,1):ivec2(0,-1)));}
bool geometry_open(ivec2 p,uint d) {return gas_gate_open((d&1u)==0u?p:p+geometry_direction(d),d/2u);}
float geometry_route(ivec2 p) {
    float total=0.;for(uint d=0u;d<4u;d++) if(geometry_open(p,d)) total+=geometry_available(p+geometry_direction(d));return total;
}
float geometry_growth(ivec2 p) {
    vec4 metadata=geometry_metadata_at(p);
    if(metadata.y<=0.||metadata.x<=0.) return 0.;
    uint at=gas_atlas(p);if(at==0xffffffffu) return 0.;
    return max(0.,metadata.x-gas_world(at).geometry.x);
}
float geometry_stable_donor_volume(ivec2 p) {
    vec4 metadata=geometry_metadata_at(p);
    if(metadata.x<=0.||metadata.y>0.) return 0.;
    uint at=gas_atlas(p);if(at==0xffffffffu) return 0.;
    Gas donor=gas_world(at);
    return donor.geometry.x==metadata.x&&gas_mass(donor)>0.?donor.geometry.x:0.;
}
float geometry_stable_capacity(ivec2 p) {
    float total=0.;
    for(uint d=0u;d<4u;d++)
        if(geometry_open(p,d)) total+=geometry_stable_donor_volume(p+geometry_direction(d));
    return total;
}
float geometry_stable_demand(ivec2 p,float donor_volume) {
    float demand=0.;
    for(uint d=0u;d<4u;d++) {
        if(!geometry_open(p,d)) continue;
        ivec2 q=p+geometry_direction(d);
        float growth=geometry_growth(q);if(growth<=0.) continue;
        float capacity=geometry_stable_capacity(q);
        if(capacity>0.) demand+=growth*donor_volume/capacity;
    }
    return demand;
}
/* Retain density during boundary remapping; sealed pockets retain all stock. */
vec2 geometry_retention(ivec2 p,Gas original,float available) {
    if(gas_mass(original)<=0.||available>=original.geometry.x) return vec2(1,0);
    float route=geometry_route(p);if(route<=0.) return vec2(1,0);
    return vec2(clamp(available/original.geometry.x,0.,1.),route);
}
Gas geometry_scale_packet(Gas original,float fraction) {
    original.species*=fraction;original.dynamics*=fraction;original.visual*=fraction;
    original.geometry.zw*=fraction;return original;
}
void geometry_mark(uint index,bool changed) {
    if(gas_cache_owner(index)==0u) cache1[index].metric.z=changed?1.:0.;else cache0[index].metric.z=changed?1.:0.;
}
