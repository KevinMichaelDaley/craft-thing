#ifdef DC_RUNTIME_BODY_STORAGE
layout(buffer_reference,std430,buffer_reference_align=4) readonly buffer RevoluteCollision {uint words[];};
bool revolute_suppresses_collision(uint first,uint second) {
    if(DC_RUNTIME_WORK(98u)==0u) return false;
    uint ai=runtime_body_find(first),bi=runtime_body_find(second);
    if(ai>=BODY_CAPACITY||bi>=BODY_CAPACITY) return false;
    uint a=min(ai,bi),b=max(ai,bi),count=DC_RUNTIME_WORK(100u),base=DC_RUNTIME_WORK(99u);
    uint hash=a*0x9e3779b9u^b*0x85ebca6bu;hash^=hash>>16u;
    RevoluteCollision joints=RevoluteCollision(uvec2(DC_RUNTIME_WORK(96u),DC_RUNTIME_WORK(97u)));
    for(uint probe=0u;probe<count;probe++) {
        uint entry=base+4u*((hash+probe)&(count-1u)),record=joints.words[entry+2u];
        if(record==0u) return false;
        if(joints.words[entry]!=a||joints.words[entry+1u]!=b) continue;
        uint p=(record-1u)*24u;if(joints.words[p+1u]==0u) continue;
        BodyStorageOwner ao=DC_BODY_OWNER(runtime_body_root,joints.words[p+2u]);
        BodyStorageOwner bo=DC_BODY_OWNER(runtime_body_root,joints.words[p+5u]);
        if(ao.state==DC_BODY_OWNER_LIVE&&bo.state==DC_BODY_OWNER_LIVE&&
           ao.generation==joints.words[p+3u]&&bo.generation==joints.words[p+6u]&&
           ao.runtime_id==joints.words[p+4u]&&bo.runtime_id==joints.words[p+7u]) return true;
    }
    return false;
}
#else
bool revolute_suppresses_collision(uint first,uint second) {return false;}
#endif
