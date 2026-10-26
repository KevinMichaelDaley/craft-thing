#include "acoustic.glsl"
layout(set=0,binding=24,std430) restrict buffer MultigridWork {uint mg_words[];};
uint mg_level() {return (params.bank>>20u)&15u;}
uvec2 mg_size(uint level) {uint scale=1u<<level;return (uvec2(params.width,params.height)+scale-1u)/scale;}
uint mg_count(uint level) {uvec2 size=mg_size(level);return size.x*size.y;}
uint mg_base(uint level) {return level==3u?0u:(level==2u?mg_count(3u)*8u:(mg_count(3u)+mg_count(2u))*8u);}
uint mg_scratch() {return (mg_count(1u)+mg_count(2u)+mg_count(3u))*8u;}
float mg_load(uint word) {return uintBitsToFloat(mg_words[word]);}
void mg_store(uint word,float value) {mg_words[word]=floatBitsToUint(value);}
vec4 mg_coeff(uint level,uint at) {
    if(level==0u) return stencil[at];
    uint b=mg_base(level)+at*8u;return vec4(mg_load(b),mg_load(b+1u),mg_load(b+2u),mg_load(b+3u));
}
float mg_diag(uint level,uint at) {return level==0u?pressure[at].z:mg_load(mg_base(level)+at*8u+5u);}
float mg_rhs(uint level,uint at) {return level==0u?correction_data[at].x:mg_load(mg_base(level)+at*8u+4u);}
bool mg_valid(uint level,uint at) {
    if(mg_diag(level,at)<=0.) return false;
    if(level!=0u) return true;
    uint tile=(at/params.width/8u)*((params.width+7u)/8u)+(at%params.width/8u);
    return (activity[gas_tile_count()+tile]&FR_GAS_SOLVE_TILE_BIT)!=0u;
}
float mg_value(uint level,uint at,bool temporary) {
    if(level==0u) return temporary?mg_load(mg_scratch()+at):correction[at];
    return mg_load(mg_base(level)+at*8u+(temporary?7u:6u));
}
void mg_write(uint level,uint at,bool temporary,float value) {
    if(level==0u&&!temporary) correction[at]=value;
    else mg_store(level==0u?mg_scratch()+at:mg_base(level)+at*8u+(temporary?7u:6u),value);
}
uint mg_neighbor(uint level,uint at,uint direction) {
    uint width=mg_size(level).x;
    return direction==0u?at+1u:(direction==1u?at-1u:(direction==2u?at+width:at-width));
}
float mg_residual(uint level,uint at) {
    float residual=mg_rhs(level,at)-mg_diag(level,at)*mg_value(level,at,false);vec4 coefficients=mg_coeff(level,at);
    for(uint d=0u;d<4u;d++) if(coefficients[d]>0.) residual+=coefficients[d]*mg_value(level,mg_neighbor(level,at,d),false);
    return residual;
}
uint mg_index() {return (gl_WorkGroupID.y*gl_NumWorkGroups.x+gl_WorkGroupID.x)*64u+gl_LocalInvocationIndex;}
