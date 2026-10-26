#include "multigrid.glsl"
void main() {
    uint level=mg_level(),at=mg_index();if(at>=mg_count(level)) return;
    uvec2 size=mg_size(level),child_size=mg_size(level-1u),parent=uvec2(at%size.x,at/size.x);
    uint base=mg_base(level)+at*8u,mask=uint(mg_load(base+7u));vec4 linked=vec4(0);
    if(mask!=0u) for(uint d=0u;d<4u;d++) {
        ivec2 neighbor=ivec2(parent)+gas_direction(d);
        if(any(lessThan(neighbor,ivec2(0)))||any(greaterThanEqual(neighbor,ivec2(size)))) continue;
        uint other=uint(neighbor.y)*size.x+uint(neighbor.x),other_mask=uint(mg_load(mg_base(level)+other*8u+7u));
        for(uint side=0u;side<2u;side++) {
            uint child=d==0u?1u+side*2u:(d==1u?side*2u:(d==2u?2u+side:side));
            uint opposite=d==0u?side*2u:(d==1u?1u+side*2u:(d==2u?side:2u+side));
            if((mask&(1u<<child))==0u||(other_mask&(1u<<opposite))==0u) continue;
            uvec2 p=parent*2u+uvec2(child%2u,child/2u);
            linked[d]+=mg_coeff(level-1u,p.y*child_size.x+p.x)[d];
        }
    }
    for(uint d=0u;d<4u;d++) mg_store(base+d,linked[d]);
}
