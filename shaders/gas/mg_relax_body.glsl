#include "multigrid.glsl"
void main() {
    uint level=mg_level(),at=mg_index();if(at>=mg_count(level)) return;
    bool temporary=(params.bank&(1u<<24u))!=0u;float own=mg_value(level,at,temporary),next=own;
    if(mg_valid(level,at)) {
        float rhs=mg_rhs(level,at);vec4 coefficients=mg_coeff(level,at);
        for(uint d=0u;d<4u;d++) if(coefficients[d]>0.) rhs+=coefficients[d]*mg_value(level,mg_neighbor(level,at,d),temporary);
        next=mix(own,rhs/mg_diag(level,at),.75);
    }
    mg_write(level,at,!temporary,next);
}
