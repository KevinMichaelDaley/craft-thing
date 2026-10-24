#ifndef DC_SOLVER_BODY_STATE_GLSL
#define DC_SOLVER_BODY_STATE_GLSL
#ifndef DC_SOLVER_DIRECT_BODY_STATE
#define DC_SOLVER_DIRECT_BODY_STATE 1
#endif
#if defined(DC_RUNTIME_BODY_STORAGE) && DC_SOLVER_DIRECT_BODY_STATE
#define SOLVER_BODY_WORD(slot,word) BodyStorageWords(DC_BODY_PAGE(runtime_body_root,slot).solver).words[((slot)&63u)*64u+(word)]
#else
uint solver_body_offset(uint slot,uint word) {
#ifdef DC_SOLVER_CAPACITY_PROFILE
    atomicAdd(capacity_profile[3],1u);
#endif
    return bs(slot)+word;
}
#define SOLVER_BODY_WORD(slot,word) DC_SOLVER_DATA(solver_body_offset(slot,word))
#endif
float body_getf(uint slot,uint word) {return uintBitsToFloat(SOLVER_BODY_WORD(slot,word));}
void body_putf(uint slot,uint word,float value) {SOLVER_BODY_WORD(slot,word)=floatBitsToUint(value);}
vec2 body_getv(uint slot,uint word) {return vec2(body_getf(slot,word),body_getf(slot,word+1u));}
void body_putv(uint slot,uint word,vec2 value) {body_putf(slot,word,value.x);body_putf(slot,word+1u,value.y);}
#ifdef DC_RUNTIME_BODY_STORAGE
#undef DC_BODY_CLIPPED
#define DC_BODY_CLIPPED(slot) SOLVER_BODY_WORD(slot,48u)
#undef DC_BODY_DEPENDENCIES
#define DC_BODY_DEPENDENCIES(slot) SOLVER_BODY_WORD(slot,49u)
#endif
#endif
