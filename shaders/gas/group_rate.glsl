shared uvec3 group_rates[FR_GAS_LOCAL_SIZE];
void gas_group_rate(Primitive p) {
    float rate=abs(p.hydro.y)+abs(p.hydro.z)+(!gas_implicit_policy()?2.*p.metric.x:0.);
    uvec3 bits=uvec3(gas_finite(rate)&&rate>=0.?floatBitsToUint(rate):0u,0u,0u);
    if(gas_implicit_policy()) bits.yz=uvec2(floatBitsToUint(abs(p.hydro.y)),floatBitsToUint(abs(p.hydro.z)));
    uvec3 peak=subgroupMax(bits);
    if(subgroupElect()) group_rates[gl_SubgroupID]=peak;
    barrier();
    if(gl_LocalInvocationIndex==0u) {
        uvec3 total=uvec3(0);for(uint group=0u;group<gl_NumSubgroups;group++) total=max(total,group_rates[group]);
        if(total.x!=0u) atomicMax(stats[FR_GAS_RATE_WORD],total.x);
        if(total.y!=0u) atomicMax(stats[FR_GAS_RATE_X_WORD],total.y);
        if(total.z!=0u) atomicMax(stats[FR_GAS_RATE_Y_WORD],total.z);
        if(gl_WorkGroupID.x==0u) stats[FR_GAS_ROW_NORM_BEFORE_WORD]=FR_GAS_LOCAL_SIZE;
    }
}
