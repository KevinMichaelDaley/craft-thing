#ifndef DC_SOLVER_CONTACT_METADATA_GLSL
#define DC_SOLVER_CONTACT_METADATA_GLSL
#ifndef DC_SOLVER_DIRECT_CONTACTS
#define DC_SOLVER_DIRECT_CONTACTS 1
#endif
#if DC_SOLVER_DIRECT_CONTACTS
#define DC_CONTACT_DATA(index) DC_DATA(index)
#else
#define DC_CONTACT_DATA(index) DC_SOLVER_DATA(index)
#endif
float contact_getf(uint p) {
#if defined(DC_SOLVER_CAPACITY_PROFILE) && !DC_SOLVER_DIRECT_CONTACTS
    atomicAdd(capacity_profile[2],1u);
#endif
    return uintBitsToFloat(DC_CONTACT_DATA(p));
}
void contact_putf(uint p,float value) {
#if defined(DC_SOLVER_CAPACITY_PROFILE) && !DC_SOLVER_DIRECT_CONTACTS
    atomicAdd(capacity_profile[2],1u);
#endif
    DC_CONTACT_DATA(p)=floatBitsToUint(value);
}
vec2 contact_getv(uint p) {return vec2(contact_getf(p),contact_getf(p+1u));}
void contact_putv(uint p,vec2 value) {contact_putf(p,value.x);contact_putf(p+1u,value.y);}
#endif
