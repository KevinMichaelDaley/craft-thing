#include "storage.glsl"
#include "bulk_volume.glsl"
#include "../../include/dungeoncraft/terrain_flags.h"
#include "../../include/ferrum/fire/gas_constants.h"
layout(set=0,binding=40,std430) buffer GeometryPaths {vec2 geometry_paths[];};
layout(set=0,binding=41,std430) buffer GeometryDemands {float geometry_demands[];};
uint geometry_index(ivec2 p) {return uint(p.y)*params.width+uint(p.x);}
float geometry_route_cached(ivec2 p) {return geometry_paths[geometry_index(p)].x;}
float geometry_capacity_cached(ivec2 p) {return geometry_paths[geometry_index(p)].y;}
float geometry_demand_cached(ivec2 p) {return geometry_demands[geometry_index(p)];}
float geometry_void(uint index,uint atlas) {
    uint packed=cells[atlas].x,material=packed&65535u;
    bool solid=material!=0u&&material!=3u&&((packed&(DC_CELL_LOADED_TERRAIN<<DC_CELL_FLAGS_SHIFT))!=0u||
        material==1u||material==6u||material==7u||material==8u||material==9u||
        material==11u||material==12u);
    if(solid) {
        bool burning_wood=DC_CELL_SMOKE_PASS_WOOD(material)&&
            (packed&(DC_CELL_BURNING_SMOKE_PASS<<DC_CELL_FLAGS_SHIFT))!=0u;
        return burning_wood?float(DC_CELL_BURNING_SMOKE_VOID):0.;
    }
    uint covered=min(65536u,occupancy[2u*params.width*params.height+index]);
    float available=max(0.,1.-float(covered)/65536.-
        min(max(mpm[index].w,0.),float(FR_GAS_GRANULAR_PACKING_FRACTION))-
        float(cells[atlas].y)/65536.-gas_bulk_volume(atlas));
    if(material==0u&&covered>0u&&(packed&(DC_CELL_BURNING_SMOKE_PASS<<DC_CELL_FLAGS_SHIFT))!=0u)
        available=max(available,float(DC_CELL_BURNING_SMOKE_VOID));
    return covered>0u&&available<=1./65536.?0.:available;
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
bool geometry_sweep_transit(ivec2 p) {
    vec4 metadata=geometry_metadata_at(p);
    if(metadata.x>0.||metadata.y<=0.) return false;
    uint at=gas_atlas(p);
    return at!=0xffffffffu&&gas_world(at).geometry.y>0.;
}
float geometry_route(ivec2 p) {
    float total=0.;
    for(uint d=0u;d<4u;d++) {
        ivec2 previous=p,q=p+geometry_direction(d);
        while(gas_atlas(q)!=0xffffffffu&&geometry_open(previous,d)) {
            float available=geometry_available(q);
            if(available>0.) {total+=available;break;}
            if(!geometry_sweep_transit(q)) break;
            previous=q;q+=geometry_direction(d);
        }
    }
    return total;
}
float geometry_growth(ivec2 p) {
    vec4 metadata=geometry_metadata_at(p);
    if(metadata.y<=0.||metadata.x<=0.) return 0.;
    uint at=gas_atlas(p);if(at==0xffffffffu) return 0.;
    return max(0.,metadata.x-gas_world(at).geometry.x);
}
bool geometry_growth_transit(ivec2 p) {return geometry_growth(p)>0.;}
float geometry_stable_donor_volume(ivec2 p) {
    vec4 metadata=geometry_metadata_at(p);
    if(metadata.x<=0.||metadata.y>0.) return 0.;
    uint at=gas_atlas(p);if(at==0xffffffffu) return 0.;
    Gas donor=gas_world(at);
    return donor.geometry.x==metadata.x&&gas_mass(donor)>0.?donor.geometry.x:0.;
}
float geometry_stable_capacity(ivec2 p) {
    float total=0.;
    for(uint d=0u;d<4u;d++) {
        ivec2 previous=p,q=p+geometry_direction(d);
        while(gas_atlas(q)!=0xffffffffu&&geometry_open(previous,d)) {
            float donor=geometry_stable_donor_volume(q);
            if(donor>0.) {total+=donor;break;}
            if(!geometry_growth_transit(q)) break;
            previous=q;q+=geometry_direction(d);
        }
    }
    return total;
}
float geometry_stable_demand(ivec2 p,float donor_volume) {
    /* Divide each new void among its open, unchanged neighbors. A donor's
     * total demand determines one retained fraction shared by all recipients. */
    float demand=0.;
    for(uint d=0u;d<4u;d++) {
        ivec2 previous=p,q=p+geometry_direction(d);
        while(gas_atlas(q)!=0xffffffffu&&geometry_open(previous,d)) {
            float growth=geometry_growth(q);if(growth<=0.) break;
            float capacity=geometry_capacity_cached(q);
            if(capacity>0.) demand+=growth*donor_volume/capacity;
            previous=q;q+=geometry_direction(d);
        }
    }
    return demand;
}
/* Retain density during boundary remapping; sealed pockets retain all stock. */
vec2 geometry_retention(ivec2 p,Gas original,float available) {
    if(gas_mass(original)<=0.||available>=original.geometry.x) return vec2(1,0);
    uint at=gas_atlas(p);
    if(available<=0.&&at!=0xffffffffu&&
        (cells[at].x&65535u)==3u&&cells[at].y>0u) return vec2(1,0);
    float route=geometry_route_cached(p);if(route<=0.) return vec2(1,0);
    return vec2(clamp(available/original.geometry.x,0.,1.),route);
}
Gas geometry_scale_packet(Gas original,float fraction) {
    original.species*=fraction;original.dynamics*=fraction;original.visual*=fraction;
    original.geometry.zw*=fraction;return original;
}
void geometry_mark(uint index,bool changed) {
    if(gas_cache_owner(index)==0u) cache1[index].metric.z=changed?1.:0.;else cache0[index].metric.z=changed?1.:0.;
}
