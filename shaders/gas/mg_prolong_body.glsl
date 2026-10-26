#include "multigrid.glsl"
void main() {
    uint level=mg_level(),at=mg_index();if(at>=mg_count(level)||!mg_valid(level,at)) return;
    uvec2 size=mg_size(level),parent_size=mg_size(level+1u),p=uvec2(at%size.x,at/size.x),parent=p/2u;
    uint source=parent.y*parent_size.x+parent.x;if(!mg_valid(level+1u,source)) return;
    float delta=mg_value(level+1u,source,false);
    mg_write(level,at,false,mg_value(level,at,false)+delta);
}
