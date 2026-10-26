#include "multigrid.glsl"
void main() {
    uint level=mg_level(),at=mg_index();if(at>=mg_count(level)) return;
    uvec2 size=mg_size(level),child_size=mg_size(level-1u),origin=uvec2(at%size.x,at/size.x)*2u;
    bool valid=all(lessThan(origin+1u,child_size));vec4 boundary=vec4(0);float rhs=0.,capacity=0.;
    if(valid) for(uint y=0u;y<2u;y++) for(uint x=0u;x<2u;x++) {
        uint child=(origin.y+y)*child_size.x+origin.x+x;vec4 coefficients=mg_coeff(level-1u,child);
        valid=valid&&mg_valid(level-1u,child)&&(x!=0u||coefficients.x>0.)&&(y!=0u||coefficients.z>0.);
        if(x==1u) boundary.x+=coefficients.x;if(x==0u) boundary.y+=coefficients.y;
        if(y==1u) boundary.z+=coefficients.z;if(y==0u) boundary.w+=coefficients.w;
        rhs+=mg_residual(level-1u,child);capacity+=max(0.,mg_diag(level-1u,child)-dot(coefficients,vec4(1)));
    }
    uint base=mg_base(level)+at*8u;
    for(uint d=0u;d<4u;d++) mg_store(base+d,valid?boundary[d]:0.);
    mg_store(base+4u,valid?rhs:0.);mg_store(base+5u,valid?capacity+dot(boundary,vec4(1)):0.);
    mg_store(base+6u,0.);mg_store(base+7u,0.);
}
