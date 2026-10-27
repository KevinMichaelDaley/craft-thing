#include "bonfire.h"
#include <string.h>
static fr_structural_gpu_node_t node(uint32_t id,uint32_t x,uint32_t y) {
    return (fr_structural_gpu_node_t){.id=id,.generation=1,.chunk={x/64,y/64},.x_fp=(x%64)*65536,.y_fp=(y%64)*65536};
}
static void piece(dc_bonfire_plan_t *out,uint32_t side,uint32_t kind,uint32_t x,uint32_t y,uint32_t width,uint32_t height) {
    uint32_t index=out->count++;
    out->pieces[index]=(dc_bonfire_piece_t){.object=index+1,.kind=kind,.x=x+side*192,.y=y,
        .width=width,.height=height,.wet=side!=0,.temperature_K=kind==DC_BONFIRE_STARTER?1400.f:293.15f};
}
bool dc_bonfire_plan(dc_bonfire_plan_t *out) {
    if(!out) return false;
    memset(out,0,sizeof(*out));out->pit.node_count=12;out->pit.member_count=10;out->pit.anchor_count=4;
    for(uint32_t side=0;side<2;side++) {
        uint32_t x=142+side*192,n=side*6,m=side*5;
        for(uint32_t i=0;i<4;i++) out->pit.nodes[n+i]=node(n+i+1,x+12*i,94);
        out->pit.nodes[n+4]=node(n+5,x,82);out->pit.nodes[n+5]=node(n+6,x+36,82);
        for(uint32_t i=0;i<3;i++) out->pit.members[m+i]=(fr_structural_gpu_member_t){
            .id=m+i+1,.generation=1,.a=n+i+1,.b=n+i+2,.material=FR_STRUCTURAL_STONE,.depth_fp=3*65536};
        out->pit.members[m+3]=(fr_structural_gpu_member_t){.id=m+4,.generation=1,.a=n+5,.b=n+1,.material=FR_STRUCTURAL_STONE,.depth_fp=3*65536};
        out->pit.members[m+4]=(fr_structural_gpu_member_t){.id=m+5,.generation=1,.a=n+6,.b=n+4,.material=FR_STRUCTURAL_STONE,.depth_fp=3*65536};
        for(uint32_t end=0;end<2;end++) {
            uint32_t ax=x+36*end;
            out->pit.anchors[side*2+end]=(fr_structural_gpu_anchor_t){.id=side*2+end+1,.generation=1,.node=n+1+3*end,
                .kind=FR_JOINT_MOMENT,.chunk={(ax-1)/64,1},.x=(ax-1)%64,.y=32,.width=3,.height=1,
                .terrain_material=DC_MATERIAL_STONE,.normal_stiffness=1e8,.shear_stiffness=1e8,.rotational_stiffness=1e8,
                .bearing_capacity=1e7,.pullout_capacity=1e7,.shear_capacity=1e7,.moment_capacity=1e7,.friction=.6f,.normal_angle=-1.57079632679f};
        }
        piece(out,side,DC_BONFIRE_KINDLING,148,90,3,1);piece(out,side,DC_BONFIRE_KINDLING,154,90,3,1);
        piece(out,side,DC_BONFIRE_STARTER,158,90,3,1);
        piece(out,side,DC_BONFIRE_KINDLING,162,90,3,1);piece(out,side,DC_BONFIRE_KINDLING,168,90,3,1);
        piece(out,side,DC_BONFIRE_LOG,146,85,10,3);piece(out,side,DC_BONFIRE_LOG,164,85,10,3);
        piece(out,side,DC_BONFIRE_LOG,152,80,16,3);
    }
    return true;
}
