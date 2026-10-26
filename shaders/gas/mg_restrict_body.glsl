#include "multigrid.glsl"
void main() {
    uint level=mg_level(),at=mg_index();if(at>=mg_count(level)) return;
    uvec2 size=mg_size(level),child_size=mg_size(level-1u),origin=uvec2(at%size.x,at/size.x)*2u;
    bool valid=all(lessThan(origin+1u,child_size));vec4 boundary=vec4(0);float rhs=0.,capacity=0.;
    uint component_mask=0u;
    if((params.bank&FR_GAS_MG_BOUNDARY_BIT)!=0u) {
        uint mask=0u;vec4 edges[4];uint indices[4];
        for(uint child=0u;child<4u;child++) {
            uvec2 p=origin+uvec2(child%2u,child/2u);bool inside=all(lessThan(p,child_size));
            indices[child]=inside?p.y*child_size.x+p.x:0u;edges[child]=vec4(0);
            if(inside&&mg_valid(level-1u,indices[child])) {mask|=1u<<child;edges[child]=mg_coeff(level-1u,indices[child]);}
        }
        uint links[4]=uint[4](0u,0u,0u,0u);
        for(uint child=0u;child<4u;child++) if((mask&(1u<<child))!=0u) {
            uint x=child%2u,y=child/2u;vec4 coefficients=edges[child];
            rhs+=mg_residual(level-1u,indices[child]);capacity+=max(0.,mg_diag(level-1u,indices[child])-dot(coefficients,vec4(1)));
            for(uint d=0u;d<4u;d++) {
                bool outer_face=d==0u?x==1u:(d==1u?x==0u:(d==2u?y==1u:y==0u));
                if(outer_face) boundary[d]+=coefficients[d];
                else {
                    uint neighbor=d==0u?child+1u:(d==1u?child-1u:(d==2u?child+2u:child-2u));
                    if((mask&(1u<<neighbor))!=0u&&coefficients[d]>0.&&edges[neighbor][d^1u]>0.) links[child]|=1u<<neighbor;
                    else capacity+=coefficients[d];
                }
            }
        }
        uint reached=mask&(~mask+1u);
        for(uint pass=0u;pass<4u;pass++) {uint previous=reached;for(uint child=0u;child<4u;child++) if((previous&(1u<<child))!=0u) reached|=links[child];}
        valid=mask!=0u&&reached==mask;
        component_mask=valid?mask:0u;
    } else {
    if(valid) for(uint y=0u;y<2u;y++) for(uint x=0u;x<2u;x++) {
        uint child=(origin.y+y)*child_size.x+origin.x+x;vec4 coefficients=mg_coeff(level-1u,child);
        valid=valid&&mg_valid(level-1u,child)&&(x!=0u||coefficients.x>0.)&&(y!=0u||coefficients.z>0.);
        if(x==1u) boundary.x+=coefficients.x;if(x==0u) boundary.y+=coefficients.y;
        if(y==1u) boundary.z+=coefficients.z;if(y==0u) boundary.w+=coefficients.w;
        rhs+=mg_residual(level-1u,child);capacity+=max(0.,mg_diag(level-1u,child)-dot(coefficients,vec4(1)));
    }
    }
    uint base=mg_base(level)+at*8u;
    for(uint d=0u;d<4u;d++) mg_store(base+d,valid?boundary[d]:0.);
    mg_store(base+4u,valid?rhs:0.);mg_store(base+5u,valid?capacity+dot(boundary,vec4(1)):0.);
    mg_store(base+6u,0.);mg_store(base+7u,float(component_mask));
}
