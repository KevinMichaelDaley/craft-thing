#include "acoustic.glsl"
#ifdef FR_GAS_GROUP_RATE
#include "group_rate.glsl"
#endif
void main() {
    bool practical=gas_practical_policy();
    if(!practical&&!(uintBitsToFloat(stats[FR_GAS_RESIDUAL_WORD])<=1e-7)) return;
#ifdef FR_GAS_GROUP_RATE
    uint tiles_per_group=FR_GAS_LOCAL_SIZE/64u,rank=gl_WorkGroupID.x*tiles_per_group+gl_LocalInvocationIndex/64u;
    if(gl_WorkGroupID.x*tiles_per_group>=indirect[12]) return;
    uint tile=rank<indirect[12]?active_tiles[3u*gas_tile_count()+rank]:0u,lane=gl_LocalInvocationIndex%64u,width=(params.width+7u)/8u;
    uvec2 point=uvec2(tile%width,tile/width)*8u+uvec2(lane%8u,lane/8u);
    bool inside=rank<indirect[12]&&point.x<params.width&&point.y<params.height;
    uint index=inside?point.y*params.width+point.x:0u;
#else
    uint index=gas_sparse_index(3u);if(index>=params.width*params.height) return;
    bool inside=true;
#endif
    ivec2 p=ivec2(int(index%params.width),int(index/params.width));uint atlas=inside?gas_atlas(p):0xffffffffu;
    bool rate_accepted=false;
    Primitive primitive=gas_empty_primitive();
    if(atlas!=0xffffffffu) {
        bool advected=gas_advected(index);Primitive center=gas_cached(p);
        Gas value=advected?gas_provisional(atlas):gas_source(p);
        vec4 own_provisional=advected?provisional[index]:vec4(center.hydro.yz*center.metric.z,0.,center.metric.w);
        vec2 own_force=advected?force[index]:vec2(0);
        if(advected&&(params.bank&FR_GAS_FUSED_METADATA_BIT)!=0u) {
            value=gas_practical_state(value);GasFlux gravity=gas_empty_flux();
            gravity.dynamics.y=own_provisional.z;gravity.dynamics.w=own_force.x;value=gas_add_flux(value,gravity,1.);
        }
        float stage_kinetic=practical?gas_kinetic_energy(value):0.,thermal=practical?gas_thermal_energy(value):0.;
        float transported_kinetic=0.;
        if(practical&&gas_mass(value)>0.) {
            float mass=gas_mass(value),vx=value.dynamics.z/mass,vy=advected?own_force.y:center.hydro.z;
            transported_kinetic=.5*gas_parameter(FR_GAS_KINETIC_WORD)*mass*(vx*vx+vy*vy);
        }
        float dt=uintBitsToFloat(params.seconds),volume=value.geometry.x;
        GasFlux change=gas_empty_flux();uint vertical_walls=0u;
        if(volume>0.) for(uint d=0u;d<4u;d++) {
            uint j=0u;if(!gas_neighbor(p,d,j)) {if(d>=2u) vertical_walls++;continue;}
            Primitive other=gas_cached(p+gas_direction(d));
            vec4 neighbor=gas_advected(j)?provisional[j]:vec4(other.hydro.yz*other.metric.z,0.,other.metric.w);
            float area=min(volume,neighbor.w);
            float difference=((other.hydro.w-center.hydro.w)+(pressure[j].x-pressure[index].x))+(acoustic[j].z-acoustic[index].z);
            float sign=(d&1u)==0u?-1.:1.;
            if(!practical) {
                float enthalpy=(params.bank&FR_GAS_LARGE_ENERGY_BIT)!=0u?matrix[index][d]/area:.5*(center.metric.x+other.metric.x);
                float q=.5*(own_provisional[d/2u]/volume+neighbor[d/2u]/neighbor.w)*area+sign*dt*area*difference;
                change.dynamics.y+=sign*enthalpy*q;
            }
            change.dynamics[2u+d/2u]+=sign*.5*area*difference;
        }
        value=gas_add_flux(value,change,dt);
        value.dynamics.w-=.5*float(vertical_walls)*own_force.x;
        if(practical) {
            value=gas_practical_momentum(value,thermal);float kinetic=gas_kinetic_energy(value);GasFlux correction=gas_empty_flux();
            correction.dynamics.y=-stage_kinetic;value=gas_add_flux(value,correction,1.);
            correction.dynamics.y=kinetic;value=gas_add_flux(value,correction,1.);value.geometry.w+=kinetic-transported_kinetic;
        } else {
            float mass=gas_mass(value);
            float work=mass>0.?gas_parameter(FR_GAS_KINETIC_WORD)*own_force.x*.5*(own_force.y+value.dynamics.w/mass):0.;
            GasFlux correction=gas_empty_flux();correction.dynamics.y=-own_provisional.z;value=gas_add_flux(value,correction,1.);
            correction.dynamics.y=work;value=gas_add_flux(value,correction,1.);value.geometry.w+=work;
        }
        if(practical) value=gas_practical_state(value);
        bool okay=false;primitive=gas_primitive(value,okay);
        if(!okay) {gas_error(index);atomicOr(stats[FR_GAS_REJECT_STAGE_WORD],FR_GAS_REJECT_FINISH);}
        else {
#ifdef FR_GAS_GROUP_RATE
            rate_accepted=true;
#else
            gas_rate(primitive);
#endif
        }
        gas_write_provisional(atlas,value);
    }
    if(inside) {if((params.bank&1u)==0u) cache1[index]=primitive;else cache0[index]=primitive;}
#ifdef FR_GAS_GROUP_RATE
    gas_group_rate(rate_accepted?primitive:gas_empty_primitive());
#endif
}
