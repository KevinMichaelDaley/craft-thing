#ifndef DC_SOLVER_LAYOUT_GLSL
#define DC_SOLVER_LAYOUT_GLSL
#ifndef DC_SOLVER_CAPACITY_CACHE
#define DC_SOLVER_CAPACITY_CACHE 1
#endif
#ifdef DC_SOLVER_CAPACITY_PROFILE
layout(set=0,binding=31,std430) restrict buffer CapacityProfile {uint capacity_profile[];};
#endif
#ifdef DC_RUNTIME_BODY_STORAGE
uint solver_capacity_read(uint word) {
#ifdef DC_SOLVER_CAPACITY_PROFILE
    atomicAdd(capacity_profile[0],1u);
#endif
    return DC_RUNTIME_WORK(word);
}
#undef DC_LAYOUT_PAIR_CAPACITY
#undef DC_LAYOUT_CONTACT_CAPACITY
#if DC_SOLVER_CAPACITY_CACHE
uint solver_pair_capacity,solver_contact_capacity;
#define DC_LAYOUT_PAIR_CAPACITY solver_pair_capacity
#define DC_LAYOUT_CONTACT_CAPACITY solver_contact_capacity
#else
#define DC_LAYOUT_PAIR_CAPACITY solver_capacity_read(36u)
#define DC_LAYOUT_CONTACT_CAPACITY solver_capacity_read(37u)
#endif
#endif
#endif
