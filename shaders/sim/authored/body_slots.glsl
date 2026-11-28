/* One availability mask per workgroup, followed by the two inclusive prefix planes. */
const uint SLOT_GROUP_SIZE=128u, SLOT_MASK_WORDS=4u;
shared uint group_masks[SLOT_MASK_WORDS];

void plan_body_slots() {
    uint needed=words[4];
    if(needed==0u) return;
    uint group=gl_WorkGroupID.y*gl_NumWorkGroups.x+gl_WorkGroupID.x;
    uint lane=gl_LocalInvocationIndex;
    uint groups=1u+(BODY_CAPACITY-1u)/SLOT_GROUP_SIZE;
    uint base=output_plan(pixels()), prefix=base+groups*SLOT_MASK_WORDS;
    if(PHASE==10u) {
        uint i=group*SLOT_GROUP_SIZE+lane;
        if(i>=groups) return;
        uint offset=params.padding>>1u, parity=params.padding&1u;
        uint value=words[prefix+groups*parity+i];
        if(i>=offset) value+=words[prefix+groups*parity+i-offset];
        words[prefix+groups*(parity^1u)+i]=value;
        return;
    }
    if(group>=groups) return;
    uint slot=group*SLOT_GROUP_SIZE+lane;
    uint word=lane/32u, bit=1u<<(lane%32u);
    if(PHASE==9u) {
        if(lane<SLOT_MASK_WORDS) group_masks[lane]=0u;
        barrier();
        if(slot<BODY_CAPACITY) {
            BodyStorageOwner owner=DC_BODY_OWNER(runtime_body_root,slot);
            if(DC_BODY_FREE(slot) && owner.state==DC_BODY_OWNER_FREE && owner.generation!=0xffffffffu)
                atomicOr(group_masks[word],bit);
        }
        barrier();
        if(lane<SLOT_MASK_WORDS) words[base+group*SLOT_MASK_WORDS+lane]=group_masks[lane];
        if(lane==0u) {
            uint count=0u;
            for(uint j=0u;j<SLOT_MASK_WORDS;++j) count+=bitCount(group_masks[j]);
            words[prefix+group]=count;
        }
        return;
    }
    prefix+=groups*params.padding;
    if(words[prefix+groups-1u]<needed) {
        if(group==0u && lane==0u) atomicOr(words[2],8u);
        return;
    }
    uint mask_base=base+group*SLOT_MASK_WORDS;
    uint mask=words[mask_base+word];
    if((mask&bit)==0u) return;
    uint rank=group==0u ? 0u : words[prefix+group-1u];
    for(uint j=0u;j<word;++j) rank+=bitCount(words[mask_base+j]);
    rank+=bitCount(mask&(bit-1u));
    if(rank<needed) words[output_plan(rank)+9u]=slot;
}
