/* Every lane participates; only lane zero consumes the workgroup result. */
shared uint gas_group_masks[64];
uint gas_group_mask(uint value) {
    value=subgroupOr(value);
    if(subgroupElect()) gas_group_masks[gl_SubgroupID]=value;
    barrier();uint result=0u;
    if(gl_LocalInvocationIndex==0u)
        for(uint group=0u;group<gl_NumSubgroups;group++) result|=gas_group_masks[group];
    return result;
}
